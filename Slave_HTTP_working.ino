#include <Wire.h>
#include <MKRNB.h>
#include <ArduinoHttpClient.h>
#include <math.h>
#include <string.h>

// =====================================================
// 1. I2C CONFIGURATION
// =====================================================

#define I2C_SLAVE_ADDR 0x55

#define PACKET_SIZE 4
#define QUEUE_SIZE 128

// I2C receive queue
float measurementQueue[QUEUE_SIZE];

volatile uint8_t queueHead = 0;
volatile uint8_t queueTail = 0;

volatile uint32_t receivedPackets = 0;
volatile uint32_t droppedPackets = 0;

// =====================================================
// 2. AVERAGING CONFIGURATION
// =====================================================

#define SAMPLES_PER_AVERAGE 60

float measurementSum = 0.0f;
uint16_t measurementCount = 0;

// =====================================================
// 3. NB-IOT CONFIGURATION
// =====================================================

// 1NCE SIM
const char PINNUMBER[] = "";
const char APN[] = "sensor.net";

// ThingSpeak server
const char SERVER[] = "api.thingspeak.com";
const int SERVER_PORT = 443;
const char PATH[] = "/update";

// IMPORTANT: Replace with your ThingSpeak Write API Key
const char WRITE_API_KEY[] = "C5H1BG2UF43SFE2I";

// NB-IoT objects
NB nbAccess;
GPRS gprs;
NBClient nbClient;

// =====================================================
// 4. I2C RECEIVE CALLBACK
// =====================================================

void onReceive(int numBytes)
{
  // Expect exactly 4 bytes (one float)
  if (numBytes != PACKET_SIZE)
  {
    while (Wire.available())
    {
      Wire.read();
    }

    droppedPackets++;
    return;
  }

  uint8_t data[PACKET_SIZE];

  for (int j = 0; j < PACKET_SIZE; j++)
  {
    data[j] = Wire.read();
  }

  // Reconstruct float from received bytes
  float receivedValue;

  memcpy(&receivedValue, data, sizeof(float));

  // Validate measurement
  if (!isfinite(receivedValue) ||
      receivedValue < 0.0f ||
      receivedValue > 100.0f)
  {
    droppedPackets++;
    return;
  }

  // Add measurement to queue
  uint8_t nextHead = (queueHead + 1) % QUEUE_SIZE;

  if (nextHead != queueTail)
  {
    measurementQueue[queueHead] = receivedValue;
    queueHead = nextHead;

    receivedPackets++;
  }
  else
  {
    // Queue full
    droppedPackets++;
  }
}

// =====================================================
// 5. NB-IOT CONNECTION
// =====================================================

bool connectNB()
{
  Serial.println();
  Serial.println("Connecting to NB-IoT...");

  // Register on cellular network
  while (nbAccess.begin(PINNUMBER, APN) != NB_READY)
  {
    Serial.println("Waiting for network registration...");
    delay(5000);
  }

  Serial.println("NB-IoT registered!");

  // Establish packet-data connection
  while (gprs.attachGPRS() != GPRS_READY)
  {
    Serial.println("Waiting for packet-data connection...");
    delay(5000);
  }

  Serial.println("GPRS data connection established!");

  return true;
}

// =====================================================
// 6. UPLOAD AVERAGE TO THINGSPEAK
// =====================================================

bool uploadAverage(float average, uint16_t samples)
{
  Serial.println();
  Serial.println("Testing HTTP connection to ThingSpeak...");

  HttpClient http(nbClient, SERVER, 80);

  String payload = "api_key=";
  payload += WRITE_API_KEY;
  payload += "&field1=";
  payload += String(average, 2);
  payload += "&field2=";
  payload += String(samples);

  http.post(
    "/update",
    "application/x-www-form-urlencoded",
    payload
  );

  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("HTTP status: ");
  Serial.println(statusCode);

  Serial.print("ThingSpeak response: ");
  Serial.println(response);

  bool success = (statusCode == 200 && response.toInt() > 0);

  if (success)
  {
    Serial.println("Upload successful!");
  }
  else
  {
    Serial.println("Upload failed!");
  }

  http.stop();

  return success;
}

// =====================================================
// 7. SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // Start I2C slave
  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onReceive(onReceive);

  Serial.println();
  Serial.println("================================");
  Serial.println("MKR NB 1500 - I2C SLAVE");
  Serial.println("================================");

  Serial.print("I2C address: 0x");
  Serial.println(I2C_SLAVE_ADDR, HEX);

  // Connect to NB-IoT
  connectNB();

  Serial.println();
  Serial.println("System ready!");
  Serial.println("Waiting for ESP32 measurements...");
}

// =====================================================
// 8. MAIN LOOP
// =====================================================

void loop()
{
  float value;
  bool hasMeasurement = false;

  // -----------------------------------------------
  // Retrieve measurements from I2C queue
  // -----------------------------------------------

  while (true)
  {
    hasMeasurement = false;

    // Protect queue access against I2C interrupts
    noInterrupts();

    if (queueTail != queueHead)
    {
      value = measurementQueue[queueTail];

      queueTail = (queueTail + 1) % QUEUE_SIZE;

      hasMeasurement = true;
    }

    interrupts();

    if (!hasMeasurement)
    {
      break;
    }

    // -----------------------------------------------
    // Add measurement to running average
    // -----------------------------------------------

    measurementSum += value;
    measurementCount++;

    Serial.print("Received: ");
    Serial.println(value, 2);

    // -----------------------------------------------
    // Calculate average after 60 samples
    // -----------------------------------------------

    if (measurementCount >= SAMPLES_PER_AVERAGE)
    {
      float average = measurementSum / measurementCount;

      Serial.println();
      Serial.println("================================");
      Serial.println("60 MEASUREMENTS RECEIVED");
      Serial.print("Average: ");
      Serial.println(average, 2);
      Serial.println("================================");

      // Reset averaging variables before upload
      measurementSum = 0.0f;
      measurementCount = 0;

      // Upload to ThingSpeak
      uploadAverage(average, SAMPLES_PER_AVERAGE);
    }
  }

  // -----------------------------------------------
  // Periodic communication statistics
  // -----------------------------------------------

  static unsigned long previousStats = 0;

  if (millis() - previousStats >= 10000)
  {
    previousStats = millis();

    uint32_t received;
    uint32_t dropped;

    noInterrupts();
    received = receivedPackets;
    dropped = droppedPackets;
    interrupts();

    Serial.println();
    Serial.println("--- Communication statistics ---");

    Serial.print("Received packets: ");
    Serial.println(received);

    Serial.print("Dropped packets: ");
    Serial.println(dropped);
  }
}