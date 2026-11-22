#include <Arduino.h>

#define RXD2 4
#define TXD2 5
unsigned long lastTime = 0;
void setup() {
  Serial.begin(115200); 
  // Initialize UART bridge at default GMP343 settings
  Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);

  Serial.println("Apply power to the GMP343 NOW...");

  // // Send 'Z' to enter service mode during power-up
  // unsigned long startTime = millis();
  // while (millis() - startTime < 5000) {
  //   Serial2.print("Z");
  //   delay(20);
  // }

  // Serial.println("\nService mode assumed.");
  // delay(1500);

  // // Send a carriage return to clear leftover 'Z's
  // Serial2.print("\r");
  // delay(500);

  // // 1. Switch communication hardware to RS-232
  // Serial.println("--> Sending RSMODE 232");
  // Serial2.print("RSMODE 232\r");
  // delay(1000);

  // // 2. Save the parameters to EEPROM
  // Serial.println("--> Sending SAVE");
  // Serial2.print("SAVE\r");
  // delay(2500); // 2.5 second delay to ensure EEPROM write completes[cite: 43]

  // // 3. Reset the device to apply hardware changes
  // Serial.println("--> Sending RESET");
  // Serial2.print("RESET\r");
  // delay(4000);
  //Serial2.print("R\r");
  //delay(1000);
  Serial.println("Configuration complete. Pass-through mode active.");
}

void loop() {
  // Pass-through to read sensor responses
  if (Serial2.available()) {
    Serial.write(Serial2.read());
  }
  if (Serial.available()) {
    Serial2.write(Serial.read());
  }
  if (millis() - lastTime > 1000) {
  Serial2.print("SEND\r");
  lastTime=millis();
  }
}
