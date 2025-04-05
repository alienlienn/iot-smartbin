#include <SPI.h>
#include <RH_RF95.h>

// Configuration
#define NODE_ID 0
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 433.0

// Timing variables
const unsigned long listenInterval = 100;
unsigned long lastListenTime = 0;
unsigned long nextStatusPrintTime = 0;

// LoRa radio instance
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// ======= FUNCTION DECLARATIONS =======
void listenForBeacons();
// ====================================

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 5000);  // Wait up to 5 seconds for serial
  
  randomSeed(analogRead(0));
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  
  // Reset the radio
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  
  if (!rf95.init()) {
    Serial.println(F("LoRa initialization failed"));
    while(1);
  }
  
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println(F("Frequency setting failed"));
    while(1);
  }

  // Configure radio parameters
  rf95.setTxPower(10, false);
  rf95.setSignalBandwidth(125000);
  rf95.setSpreadingFactor(7);
  rf95.setCodingRate4(5);
  
  // Initialize timing
  nextStatusPrintTime = millis() + 15000;  // Print status every 15 seconds
  
  Serial.print(F("LoRa Beacon Receiver Node "));
  Serial.print(NODE_ID);
  Serial.println(F(" ready!"));
  Serial.println(F("Listening for beacons..."));
}

void listenForBeacons() {
  // Check for direct LoRa messages from beacons
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.recv(buf, &len)) {
      // Null-terminate the message
      buf[len] = '\0';
      Serial.println((char*)buf);
    }
  }
}

void loop() {
  unsigned long currentTime = millis();
  
  // Listen for beacons
  if (currentTime - lastListenTime >= listenInterval) {
    listenForBeacons();
    lastListenTime = currentTime;
  }
  
  // Status update
  if (currentTime >= nextStatusPrintTime) {
    Serial.println(F("LoRa Beacon Receiver running..."));
    nextStatusPrintTime = currentTime + 15000; // Every 15 seconds
  }
  
  // Small delay to avoid busy waiting
  delay(10);
}