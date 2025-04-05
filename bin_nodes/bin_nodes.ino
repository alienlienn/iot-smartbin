#include <SPI.h>
#include <RH_RF95.h>
#include <RHMesh.h>

// Configuration
#define NODE_ID 1                        // <<< CHANGE THIS FOR EACH NODE
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 433.0

// Sensor Pins
#define TRIG_PIN 3                       // Ultrasonic trigger pin
#define ECHO_PIN 4                       // Ultrasonic echo pin

// Network Settings
#define MESH_KEY 0xAB                    // 1-byte network ID
#define BROADCAST_INTERVAL 10000         // Main status broadcast interval
#define BEACON_BROADCAST_INTERVAL 60000  // Beacon broadcast every 60 seconds
#define MAX_NODES 4                      // Maximum nodes in network
#define NODE_TIMEOUT 120000              // 2 minutes timeout for node removal

// Create both a mesh manager and a direct RF95 instance
RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHMesh manager(rf95, NODE_ID);

#pragma pack(push, 1)
struct SecureMessage {
  uint8_t key;
  uint8_t sender;
  bool status;
  uint32_t timestamp;
  uint16_t crc;
};
#pragma pack(pop)

struct NodeInfo {
  uint8_t id;
  bool status;
  uint32_t lastSeen;
};
NodeInfo knownNodes[MAX_NODES];
uint8_t nodeCount = 0;

// Timing variables
uint32_t nextMeshBroadcast = 0;
uint32_t nextBeaconBroadcast = 0;

// ======= FUNCTION DECLARATIONS =======
bool checkBinStatus();                   // Check bin fill level using ultrasonic
void sendMeshStatus();                   // Send status to mesh network
void sendBeaconMessage();                // Send simple ID for beacons
void listenForMessages();                // Listen for incoming messages
void printNetworkStatus();               // Print current network status
void cleanupNodes();                     // Remove timed-out nodes
uint16_t computeCRC16(const uint8_t* data, size_t length);  // CRC calculation
// ====================================

/**
 * Initialize the Arduino and radio module
 * - Sets up ultrasonic sensor pins
 * - Initializes serial communication
 * - Configures and initializes the RFM95 radio
 * - Sets initial timing for broadcasts with random offsets to avoid collisions
 */
void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.begin(9600);
  while (!Serial && millis() < 5000);    // Wait up to 5 seconds for serial
  
  randomSeed(analogRead(0));             // Seed for random delays
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  
  // Initialize radio
  if (!manager.init()) while(1);         // Hang if initialization fails
  if (!rf95.setFrequency(RF95_FREQ)) while(1);

  // Configure radio settings
  rf95.setTxPower(10, false);
  rf95.setSignalBandwidth(125000);
  rf95.setSpreadingFactor(7);
  
  // Initialize timers with some randomness to avoid collisions
  nextMeshBroadcast = millis() + random(0, 5000);
  nextBeaconBroadcast = millis() + random(5000, 10000);
  
  Serial.print(F("Node ")); 
  Serial.print(NODE_ID); 
  Serial.println(F(" ready!"));
}

/**
 * Check if the bin is full using ultrasonic sensor
 * - Sends trigger pulse to ultrasonic sensor
 * - Measures distance based on echo pulse duration
 * - Returns true if distance ≤ 5cm (bin is full)
 * - Logs distance measurement to serial monitor
 * 
 * @return bool - true if bin is full, false otherwise
 */
bool checkBinStatus() {
  // Ultrasonic measurement with debug print
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;
  
  Serial.print(F("[Sensor] Distance: ")); 
  Serial.print(distance);
  Serial.println(F("cm"));
  
  return (distance > 0 && distance <= 5); // FULL if <=5cm
}

/**
 * Send current bin status to the mesh network
 * - Checks bin status using ultrasonic sensor
 * - Creates a secure message with network key, node ID, bin status, timestamp
 * - Calculates CRC16 checksum for message integrity
 * - Implements exponential backoff for retries on failed transmissions
 * - Broadcasts message to all nodes in the mesh network
 */
void sendMeshStatus() {
  static uint16_t retryDelay = 50;       // Initial retry delay
  static uint32_t lastBroadcastTime = 0; // Track last broadcast time
  bool isFull = checkBinStatus();        // Determine if the bin is full
  
  // Create the message
  SecureMessage msg = {
    MESH_KEY,
    NODE_ID,
    isFull,
    millis(),
    0
  };

  // Calculate CRC16
  msg.crc = computeCRC16((uint8_t*)&msg, sizeof(msg) - sizeof(msg.crc));
  
  // Log the broadcasting status
  Serial.print(F("Broadcasting: "));
  Serial.println(isFull ? "FULL" : "OK");

  // Check if enough time has passed since last broadcast
  if (millis() - lastBroadcastTime >= retryDelay) {
    // Attempt to send the message
    if (manager.sendtoWait((uint8_t*)&msg, sizeof(msg), RH_BROADCAST_ADDRESS) == RH_ROUTER_ERROR_NONE) {
        retryDelay = 50;                // Reset delay if successful
    } else {
        // Exponential backoff with maximum limit
        retryDelay = min(retryDelay * 2, 2000);
    }
    
    lastBroadcastTime = millis();       // Update last broadcast time
  }
}

/**
 * Send a beacon message for network discovery
 * - Creates a simple message containing only the node ID
 * - Uses direct RF95 instance instead of mesh manager for basic communication
 * - Sends beacon and listens for responses for 2 seconds
 * - Used during startup phase and periodic network discovery
 */
void sendBeaconMessage() {
  // Create a simple ID message for beacons
  String idStr = String(NODE_ID);
  
  // Use underlying rf95 directly without reinitializing
  Serial.print(F("Beacon message: "));
  Serial.println(idStr);
  
  // Send directly using the rf95 instance
  rf95.send((uint8_t*)idStr.c_str(), idStr.length() + 1);
  rf95.waitPacketSent();
  
  // Listen for beacon responses (limited time)
  unsigned long listenStart = millis();
  while (millis() - listenStart < 2000) {  // Listen for 2 seconds max
    if (rf95.available()) {
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      
      if (rf95.recv(buf, &len)) {
        buf[len] = '\0';
        Serial.print(F("Beacon response: "));
        Serial.println((char*)buf);
      }
    }
  }
}

/**
 * Listen for incoming messages from other nodes
 * - Receives message packets through the mesh network
 * - Verifies message integrity using CRC16 checksum
 * - Updates known node information with received status
 * - Tracks last seen time for each node
 * - Adds new nodes to the network if space is available
 * - Prints updated network status after processing messages
 */
void listenForMessages() {
  uint8_t buf[sizeof(SecureMessage)];
  uint8_t len = sizeof(buf);
  uint8_t from;

  if (manager.recvfromAck(buf, &len, &from)) {
    SecureMessage* msg = (SecureMessage*)buf;

    // Verify CRC before trusting content
    uint16_t receivedCRC = msg->crc;
    uint16_t computedCRC = computeCRC16((uint8_t*)msg, sizeof(SecureMessage) - sizeof(msg->crc));

    if (receivedCRC != computedCRC) {
      Serial.println(F("CRC mismatch! Discarding message."));
      return;
    }

    if (msg->key == MESH_KEY) {
      Serial.print(F("Received from ")); 
      Serial.print(msg->sender);
      Serial.print(F(": ")); 
      Serial.println(msg->status ? "FULL" : "OK");

      // Update or add node information
      bool exists = false;
      for (uint8_t i = 0; i < nodeCount; i++) {
        if (knownNodes[i].id == msg->sender) {
          knownNodes[i].status = msg->status;
          knownNodes[i].lastSeen = millis();
          exists = true;
          break;
        }
      }

      if (!exists && nodeCount < MAX_NODES) {
        knownNodes[nodeCount++] = {
          msg->sender,
          msg->status,
          millis()
        };
        Serial.print(F("New node added: "));
        Serial.println(msg->sender);
      }

      printNetworkStatus();
    }
  }
}

/**
 * Remove nodes that haven't been seen for too long
 * - Checks last seen timestamp for each known node
 * - Removes nodes that exceed NODE_TIMEOUT threshold (2 minutes)
 * - Reorganizes node array by shifting remaining nodes
 * - Updates node count after removing timed-out nodes
 * - Prints updated network status after cleanup
 */
void cleanupNodes() {
  uint32_t currentTime = millis();
  for (uint8_t i = 0; i < nodeCount; ) {
    if ((currentTime - knownNodes[i].lastSeen) > NODE_TIMEOUT) {
      Serial.print(F("Node ")); 
      Serial.print(knownNodes[i].id);
      Serial.println(F(" timed out!"));
      
      // Remove the node by shifting array
      for (uint8_t j = i; j < nodeCount - 1; j++) {
        knownNodes[j] = knownNodes[j + 1];
      }
      nodeCount--;
      printNetworkStatus();
    } else {
      i++;
    }
  }
}

/**
 * Print the current network status to serial monitor
 * - Displays this node's ID
 * - Lists all known nodes with their:
 *   - ID number
 *   - Status (FULL or OK)
 *   - Time since last message in seconds
 * - Provides visibility into the mesh network topology
 */
void printNetworkStatus() {
  Serial.println(F("\n=== Network Status ==="));
  Serial.print(F("My ID: ")); 
  Serial.println(NODE_ID);
  
  for (uint8_t i = 0; i < nodeCount; i++) {
    uint32_t secondsAgo = (millis() - knownNodes[i].lastSeen) / 1000;
    
    Serial.print(F("Node ")); 
    Serial.print(knownNodes[i].id);
    Serial.print(F(": ")); 
    Serial.print(knownNodes[i].status ? "FULL" : "OK");
    Serial.print(F(" | Last seen ")); 
    Serial.print(secondsAgo); 
    Serial.println(F("s ago"));
  }
  Serial.println(F("====================\n"));
}

/**
 * Calculate CRC16-CCITT checksum for message integrity
 * - Takes data buffer and length as input
 * - Implements standard CRC-16-CCITT polynomial (0x1021)
 * - Returns 16-bit checksum for verification
 * - Helps ensure messages haven't been corrupted during transmission
 * 
 * @param data - Pointer to data buffer
 * @param length - Size of data in bytes
 * @return uint16_t - Calculated CRC16 checksum
 */
uint16_t computeCRC16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;      // Polynomial for CRC-16-CCITT
      else
        crc <<= 1;
    }
  }
  return crc;
}

/**
 * Main program loop
 * - Has two operational modes:
 *   1. First 20 seconds: Beacon mode for network discovery
 *   2. Normal operation: Full mesh network participation
 * - During normal operation:
 *   - Broadcasts status at regular intervals
 *   - Listens for incoming messages
 *   - Periodically cleans up inactive nodes
 * - Includes a small delay to prevent CPU overutilization
 */
void loop() {
  static uint32_t startupTime = millis(); // Track node startup time
  uint32_t currentTime = millis();
  
  // First 20 seconds: beacon mode only
  if (currentTime - startupTime < 20000) {
    sendBeaconMessage();
  } 
  else {
    // Normal operation: mesh network participation
    if (currentTime >= nextMeshBroadcast) {
      sendMeshStatus();
      nextMeshBroadcast = currentTime + BROADCAST_INTERVAL + (NODE_ID * 500);
      cleanupNodes();
    }
    listenForMessages();
    delay(10);                          // Small delay to avoid busy waiting
  }
}