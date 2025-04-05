#include <SPI.h>
#include <RH_RF95.h>

// LoRa Module Pins
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 433.0  // Frequency in MHz

// *************** BEACON CONFIGURATION ***************
#define BEACON_ID 1  // Change this for each beacon (1, 2, or 3)
// ***************************************************

// LoRa Module Initialization
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Buffer management variables
unsigned long lastResetTime = 0;
const unsigned long RESET_INTERVAL = 3600000; // Reset radio every hour (in milliseconds)
unsigned long lastActivityTime = 0;
const unsigned long ACTIVITY_TIMEOUT = 60000; // Reset if no activity for 1 minute

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 5000); // Wait for serial port to connect
  
  initializeRadio(); // Separate function for radio initialization
  
  Serial.println(F("Beacon Ready"));
  Serial.print(F("Beacon ID: "));
  Serial.println(BEACON_ID);
}

void initializeRadio() {
  // Reset the radio
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  // Initialize LoRa Module
  if (!rf95.init()) {
    Serial.println(F("FAILED: LoRa init"));
    while (1);
  }

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println(F("FAILED: Set frequency"));
    while (1);
  }

  rf95.setSignalBandwidth(125000);    // 125 kHz
  rf95.setSpreadingFactor(7);         // SF7
  rf95.setCodingRate4(5);             // CR 4/5
  rf95.setPreambleLength(8);          // 8 symbols preamble

  rf95.setTxPower(13, false);  // Set LoRa transmission power
  
  // Clear any pending packets in the buffer
  while (rf95.available()) {
    uint8_t dummy[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(dummy);
    rf95.recv(dummy, &len);
  }
  
  Serial.println(F("Radio initialized"));
}

void loop() {
  // Check if we need to periodically reset the radio
  unsigned long currentTime = millis();
  
  // Reset radio if no activity for too long or periodic reset interval reached
  if ((currentTime - lastActivityTime > ACTIVITY_TIMEOUT && lastActivityTime > 0) || 
      (currentTime - lastResetTime > RESET_INTERVAL)) {
    Serial.println(F("Performing scheduled radio reset"));
    initializeRadio();
    lastResetTime = currentTime;
    lastActivityTime = 0; // Reset activity timer
  }
  
  if (rf95.available()) {
    uint8_t buf[64];  // Buffer for incoming message
    uint8_t len = sizeof(buf);
    
    // Clear buffer before receiving
    memset(buf, 0, sizeof(buf));

    // Check if a message is received
    if (rf95.recv(buf, &len)) {
      // Message received successfully - update activity timestamp
      lastActivityTime = currentTime;
      
      // Null-terminate the buffer
      buf[len] = '\0';
      
      // Display raw received message
      Serial.print(F("Received: "));
      Serial.println((char*)buf);
      
      // Convert the message to a String
      String receivedMsg = String((char*)buf);
      
      // Trim any whitespace
      receivedMsg.trim();
      
      // Check if the received message is a valid bin ID
      if (receivedMsg.length() > 0 && isdigit(receivedMsg[0])) {
        String binIdValue = receivedMsg;
        Serial.print(F("Extracted bin_id: "));
        Serial.println(binIdValue);
        
        // Measure RSSI of the received packet
        float receivedRSSI = rf95.lastRssi();
        Serial.print(F("RSSI: "));
        Serial.println(receivedRSSI);

        // Create a response message
        String responseMsg = "{\"id\":\"B";
        responseMsg += String(BEACON_ID);       // Result: B1, B2 etc.
        responseMsg += "\",\"b\":";             // Bin
        responseMsg += binIdValue;              // Bin_id value
        responseMsg += ",\"r\":";               // RSSI
        responseMsg += String(receivedRSSI, 2); // RSSI value
        responseMsg += "}";

        Serial.print(F("Response: "));
        Serial.println(responseMsg);
        
        // Implement TDMA to prevent collision
        // Each beacon waits a different amount of time based on its ID
        int baseDelay = BEACON_ID * 600;  // 600ms spacing between beacons
        int randomOffset = random(0, 200); // Add some randomness to prevent systematic collisions
        
        Serial.print(F("Waiting for "));
        Serial.print(baseDelay + randomOffset);
        Serial.println(F("ms before transmitting"));
        
        delay(baseDelay + randomOffset);
        
        // Send the response
        rf95.send((uint8_t*)responseMsg.c_str(), responseMsg.length() + 1);
        rf95.waitPacketSent();
        Serial.println(F("Response sent"));
        
        // Clear any pending packets that might have arrived during transmission
        while (rf95.available()) {
          uint8_t dummy[RH_RF95_MAX_MESSAGE_LEN];
          uint8_t len = sizeof(dummy);
          rf95.recv(dummy, &len);
        }
      } else {
        Serial.println(F("Received message is not a valid bin ID"));
      }
    } else {
      Serial.println(F("ERROR: LoRa receive failed"));
    }
  }
  
  // Add a small delay to avoid busy waiting and reduce power consumption
  delay(10);
}