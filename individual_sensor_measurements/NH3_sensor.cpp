// RS232 Bridge TX ---> ESP32 Pin 17 (RX)
// RS232 Bridge RX ---> ESP32 Pin 18 (TX)
#define SENSOR_RX_PIN 17 
#define SENSOR_TX_PIN 18 

float bytesToFloat(const uint8_t* b) {
  float f;
  memcpy(&f, b, sizeof(float));
  return f;
}

void parseAxetrisFrame(const uint8_t* buf, size_t len) {
  // Silently ignore boot/version frames
  if (buf[1] == 'V') return;

  // Reject incomplete or unknown frames
  if (len < 22 || buf[1] != 'M') return;

  uint16_t status     = (buf[4] << 8) | buf[5];
  float concentration = bytesToFloat(&buf[6]);
  float rawSignal     = bytesToFloat(&buf[10]); 
  float temperature   = bytesToFloat(&buf[14]); 
  float pressure      = (len >= 26) ? bytesToFloat(&buf[18]) : 0.0f;

  // Print data in a clean, single-line format
  Serial.print("NH3: ");
  Serial.print(concentration, 3);
  Serial.print(" ppm | Temp: ");
  Serial.print(temperature, 2);
  Serial.print(" °C | Signal: ");
  Serial.print(rawSignal, 1);

  if (pressure > 0) {
    Serial.print(" | Press: ");
    Serial.print(pressure, 1);
    Serial.print(" hPa");
  }

  Serial.print(" | Status: 0x");
  if (status < 0x10) Serial.print("0");
  Serial.println(status, HEX);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, SENSOR_RX_PIN, SENSOR_TX_PIN);

  delay(2000);
  Serial.println("--- Axetris NH3 Sensor Active ---");
}

void loop() {
  static uint8_t frame[128];
  static size_t idx = 0;
  static bool collecting = false;

  while (Serial1.available() > 0) {
    uint8_t b = Serial1.read();

    if (b == '{') {
      idx = 0;
      collecting = true;
    }

    if (collecting) {
      if (idx < sizeof(frame)) {
        frame[idx++] = b;
      }

      if (b == '}') {
        collecting = false;
        parseAxetrisFrame(frame, idx);
      }
    }
  }
}