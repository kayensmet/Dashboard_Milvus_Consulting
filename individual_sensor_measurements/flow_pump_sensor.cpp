// SFM4100 closed-loop flow controller (ESP32, Arduino core 3.x)
// Wiring: VDD(pin1)=5V, VLS(pin3)=3.3V, GND(pins 4,6,8), SCL=pin5, SDA=pin7
// 10k pull-ups from SDA and SCL to 3.3V. PWM pin -> logic-level MOSFET -> pump.
//
// Serial commands (115200 baud, newline):
//   <number>   flow setpoint in slm (0 = off), e.g. 3
//   p<val> i<val> d<val>   PID gains, e.g. p1 i2
//   o<val>     open loop: fixed duty in %, e.g. o30   (o-1 = back to closed loop)

// ---- Pins ----
const int SDA_PIN = 21;
const int SCL_PIN = 22;
const int PWM_PIN = 18;

// ---- PWM ----
const int PWM_FREQ = 20000;    // Hz (raise to ~20000 if the pump whines)
const int PWM_RES  = 10;      // bits

// ---- Sensor ----
const bool REQUIRE_CRC = false;   // true = reject readings with CRC mismatch
const unsigned long AVG_MS = 80;  // averaging window per control cycle
uint8_t sensorAddr = 0;           // found automatically (1..5)
bool useStop = false;             // set automatically

// ---- PID ----
float setpoint = 1.50;             // slm
float manualDuty = -1;            // -1 = closed loop, >= 0 = open loop duty %
float kp = 5.3;                   // % per slm
float ki = 10.5;                   // % per (slm*s)
float kd = 0.0;
float integral = 0.0, lastFlow = 0.0, duty = 0.0, flow = 0.0;
int failCount = 0;

const unsigned long LOOP_MS  = 100;
const unsigned long PRINT_MS = 200;
unsigned long lastLoop = 0, lastPrint = 0;

// ================= Bit-banged I2C =================
#define DLY delayMicroseconds(5)   // ~100 kHz

void sdaHigh() { pinMode(SDA_PIN, INPUT_PULLUP); }
void sdaLow()  { digitalWrite(SDA_PIN, LOW); pinMode(SDA_PIN, OUTPUT); }
void sclLow()  { digitalWrite(SCL_PIN, LOW); pinMode(SCL_PIN, OUTPUT); }

void sclHigh() {
  pinMode(SCL_PIN, INPUT_PULLUP);
  uint32_t t = micros();
  while (!digitalRead(SCL_PIN) && micros() - t < 50000);   // clock stretching
}

void i2cStart() { sdaHigh(); sclHigh(); DLY; sdaLow(); DLY; sclLow(); }
void i2cStop()  { sdaLow(); DLY; sclHigh(); DLY; sdaHigh(); DLY; }

bool i2cWrite(uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    (b >> i) & 1 ? sdaHigh() : sdaLow();
    DLY; sclHigh(); DLY; sclLow();
  }
  sdaHigh(); DLY; sclHigh();
  bool ack = !digitalRead(SDA_PIN);
  DLY; sclLow();
  return ack;
}

uint8_t i2cRead(bool ack) {
  uint8_t b = 0;
  sdaHigh();
  for (int i = 0; i < 8; i++) {
    sclHigh(); DLY;
    b = (b << 1) | digitalRead(SDA_PIN);
    sclLow(); DLY;
  }
  ack ? sdaLow() : sdaHigh();
  DLY; sclHigh(); DLY; sclLow();
  sdaHigh();
  return b;
}

uint8_t crc8(const uint8_t *d, size_t n) {
  uint8_t crc = 0;
  for (size_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  }
  return crc;
}

// 0 = OK, 1 = addr NACK, 2 = cmd NACK, 3 = read-addr NACK, 4 = CRC mismatch
int readFlow(uint8_t addr, float &slm, uint8_t *raw) {
  i2cStart();
  if (!i2cWrite(addr << 1)) { i2cStop(); return 1; }
  if (!i2cWrite(0xF1))      { i2cStop(); return 2; }
  if (useStop) { i2cStop(); delayMicroseconds(100); }
  i2cStart();
  if (!i2cWrite((addr << 1) | 1)) { i2cStop(); return 3; }
  raw[0] = i2cRead(true);
  raw[1] = i2cRead(true);
  raw[2] = i2cRead(false);
  i2cStop();
  slm = (int16_t)((raw[0] << 8) | raw[1]) / 1000.0f;
  return (crc8(raw, 2) == raw[2]) ? 0 : 4;
}

// Average many fast reads to smooth out pump pulsation. Returns 0 if OK.
int readFlowAvg(float &out) {
  float sum = 0; int n = 0; int e = 1;
  unsigned long t0 = millis();
  while (millis() - t0 < AVG_MS) {
    uint8_t raw[3]; float f;
    e = readFlow(sensorAddr, f, raw);
    if (e == 0 || (e == 4 && !REQUIRE_CRC)) { sum += f; n++; }
  }
  if (!n) return e;
  out = sum / n;
  return 0;
}

// ================= Control =================
void setDuty(float pct) {
  pct = constrain(pct, 0.0f, 100.0f);
  ledcWrite(PWM_PIN, (uint32_t)(pct / 100.0f * ((1 << PWM_RES) - 1)));
}

void parseCommand(String s) {
  s.trim();
  s.toLowerCase();
  if (s.length() == 0) return;
  char c = s[0];
  if (c == 'o') {
    manualDuty = s.substring(1).toFloat();
    integral = 0;
    if (manualDuty >= 0) Serial.printf("Open loop duty: %.1f%%\n", manualDuty);
    else { manualDuty = -1; Serial.println("Closed loop"); }
  } else if (c == 'p' || c == 'i' || c == 'd') {
    float v = s.substring(1).toFloat();
    if (c == 'p') kp = v; else if (c == 'i') ki = v; else kd = v;
    integral = 0;
    Serial.printf("Kp=%.2f Ki=%.2f Kd=%.2f\n", kp, ki, kd);
  } else {
    setpoint = constrain(s.toFloat(), 0.0f, 20.0f);
    Serial.printf("Setpoint: %.2f slm\n", setpoint);
  }
}

void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { parseCommand(line); line = ""; }
    else line += c;
  }
}

void setup() {
  Serial.begin(115200);
  ledcAttach(PWM_PIN, PWM_FREQ, PWM_RES);
  setDuty(0);

  sdaHigh(); sclHigh();
  delay(1000);                     // sensor warm-up

  // Auto-detect address and read mode
  float f; uint8_t r[3];
  for (uint8_t a = 1; a <= 5 && !sensorAddr; a++) {
    for (int mode = 0; mode < 2 && !sensorAddr; mode++) {
      useStop = mode;
      int e = readFlow(a, f, r);
      if (e == 0 || e == 4) {
        sensorAddr = a;
        Serial.printf("Sensor found at 0x%02X (%s)\n", a, useStop ? "STOP mode" : "repeated start");
      }
      delay(20);
    }
  }
  if (!sensorAddr) {
    Serial.println("No sensor found - pump disabled. Check wiring/power.");
    return;
  }

  Serial.println("Enter flow setpoint in slm (0 = off)");
  Serial.println("Tune with p<val>, i<val>, d<val> (e.g. p1)");
  Serial.println("Open loop test: o<duty%> (e.g. o30), o-1 = closed loop");
}

void loop() {
  handleSerial();
  if (!sensorAddr) { setDuty(0); delay(100); return; }

  unsigned long now = millis();
  if (now - lastLoop < LOOP_MS) return;
  float dt = (now - lastLoop) / 1000.0f;
  lastLoop = now;

  int e = readFlowAvg(flow);

  if (e == 0) {
    failCount = 0;
  } else if (++failCount >= 5) {   // sensor lost -> pump off
    setDuty(0);
    integral = 0;
    if (now - lastPrint >= PRINT_MS) {
      lastPrint = now;
      Serial.printf("Sensor error %d, pump off\n", e);
    }
    return;
  } else {
    return;                        // skip one bad cycle
  }

  if (manualDuty >= 0) {
    duty = manualDuty;             // open loop
    integral = 0;
  } else if (setpoint <= 0.0f) {
    duty = 0;
    integral = 0;
  } else {
    float err = setpoint - flow;
    integral += err * dt;
    if (ki > 0) integral = constrain(integral, 0.0f, 100.0f / ki);   // anti-windup
    float dTerm = -kd * (flow - lastFlow) / dt;
    duty = constrain(kp * err + ki * integral + dTerm, 0.0f, 100.0f);
  }
  lastFlow = flow;
  setDuty(duty);

  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    Serial.printf("setpoint:%.3f flow:%.3f duty:%.1f\n", manualDuty >= 0 ? 0.0f : setpoint, flow, duty);
  }
}
