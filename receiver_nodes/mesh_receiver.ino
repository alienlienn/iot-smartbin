#include <SPI.h>
#include <RH_RF95.h>
#include <RHMesh.h>

// Configuration
#define NODE_ID 0
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 433.0

// Network Settings
#define MESH_KEY 0xAB          // 1-byte network ID
#define BROADCAST_INTERVAL 30000 
#define JITTER_WINDOW 3000     
#define MAX_NODES 4            
#define NODE_TIMEOUT 120000    // 2 minutes timeout

const unsigned long listenInterval = 200; // Time between listen operations (ms)
unsigned long lastListenTime = 0;

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHMesh manager(rf95, NODE_ID);

#pragma pack(push, 1)
struct SecureMessage {
  uint8_t key;
  uint8_t sender;
  bool status;
  uint32_t timestamp;
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
unsigned long nextStatusPrintTime = 0;
unsigned long nextCleanupTime = 0;

// ======= FUNCTION DECLARATIONS =======
void listenForMessages();
void printNetworkStatus();
void cleanupNodes();
// ====================================

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 5000);  // Wait up to 5 seconds for serial
  
  randomSeed(analogRead(0));
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  
  if (!manager.init()) {
    Serial.println(F("Manager initialization failed"));
    while(1);
  }
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println(F("Frequency setting failed"));
    while(1);
  }

  rf95.setTxPower(10, false);
  rf95.setSignalBandwidth(125000);
  rf95.setSpreadingFactor(7);
  
  // Initialize timing variables with some randomness
  nextStatusPrintTime = millis() + 15000;  // Print status every 15 seconds
  nextCleanupTime = millis() + 10000;      // Check for timeouts every 10 seconds
  
  Serial.print(F("Mesh Network Node ")); 
  Serial.print(NODE_ID);
  Serial.println(F(" ready!"));
}

void listenForMessages() {
  uint8_t buf[sizeof(SecureMessage)];
  uint8_t len = sizeof(buf);
  uint8_t from;

  if (manager.available()) {
    if (manager.recvfromAck(buf, &len, &from)) {
      SecureMessage* msg = (SecureMessage*)buf;

      if (msg->key == MESH_KEY) {
        Serial.print(F("Received from ")); 
        Serial.print(msg->sender);
        Serial.print(F(": ")); 
        Serial.println(msg->status ? "FULL" : "OK");

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
          knownNodes[nodeCount].id = msg->sender;
          knownNodes[nodeCount].status = msg->status;
          knownNodes[nodeCount].lastSeen = millis();
          nodeCount++;
          Serial.print(F("New node added: "));
          Serial.println(msg->sender);
        }
      }
    }
  }
}

void cleanupNodes() {
  uint32_t currentTime = millis();
  for (uint8_t i = 0; i < nodeCount; ) {
    if ((currentTime - knownNodes[i].lastSeen) > NODE_TIMEOUT) {
      Serial.print(F("Node ")); 
      Serial.print(knownNodes[i].id);
      Serial.println(F(" timed out!"));
      
      for (uint8_t j = i; j < nodeCount - 1; j++) {
        knownNodes[j] = knownNodes[j + 1];
      }
      nodeCount--;
    } else {
      i++;
    }
  }
}

void printNetworkStatus() {
  Serial.println(F("Routing Table:")); 
  for (uint8_t i = 0; i < nodeCount; i++) {
    uint32_t secondsAgo = (millis() - knownNodes[i].lastSeen) / 1000;
    
    Serial.print(F("Node ")); 
    Serial.print(knownNodes[i].id);
    Serial.print(F(" - Last Seen "));
    Serial.print(secondsAgo); 
    Serial.print(F("s ago - Status "));
    Serial.println(knownNodes[i].status ? "FULL" : "OK");
  }
  Serial.println(F("====================\n"));
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check if it's time to listen for mesh messages
  if (currentTime - lastListenTime >= listenInterval) {
    listenForMessages();
    lastListenTime = currentTime;
  }
  
  // Check if it's time to print network status
  if (currentTime >= nextStatusPrintTime) {
    printNetworkStatus();
    nextStatusPrintTime = currentTime + 15000; // Every 15 seconds
  }
  
  // Check if it's time to clean up nodes
  if (currentTime >= nextCleanupTime) {
    cleanupNodes();
    nextCleanupTime = currentTime + 10000; // Every 10 seconds
  }
  
  // Small delay to avoid busy waiting
  delay(10);
}