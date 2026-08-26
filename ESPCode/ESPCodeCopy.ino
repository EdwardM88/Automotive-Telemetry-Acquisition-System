/*
 *  Hardware:
 *    - ESP32 dev board (main controller)
 *    - ADXL345 accelerometer (I2C) -> shock/motion detection
 *    - NEO-6M GPS module (UART2)   -> location + speed
 *    - SSD1306 OLED display (I2C)  -> live status
 *    - MCP2515 CAN Bus module (SPI)-> vehicle bus interface
 *    - Arduino Uno (separate board) acting as an engine/RPM simulator,
 *      sending pulses on ArduinoPin to emulate crank/tach signal
 *
 *  NOTE on CAN mode:
 *    The MCP2515 is currently in MCP_LOOPBACK mode. This is intended for
 *    bench-testing the CAN peripheral itself: the ESP32 sends its own RPM
 *    frame and immediately reads it back, so what you see on the OLED is
 *    a loopback echo, NOT real data from a vehicle bus.
 *    Before installing on a real car, switch to:
 *        CAN0.setMode(MCP_NORMAL);      // read/write on real bus
 *        // or
 *        CAN0.setMode(MCP_LISTENONLY);  // read-only, never transmits
 */

#include <SPI.h>
#include <mcp_can.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <Adafruit_ADXL345_U.h>
#include <TinyGPS++.h>

//for json file
unsigned long ultimulJSON = 0;
const unsigned long intervalJSON = 1000;

// pin used to comunicate with Arduino
const int ArduinoPin = 33; 

// Pins used for CAN Bus MCP2515 module
#define CAN0_CS 15
MCP_CAN CAN0(CAN0_CS);

// Pini Serial Hardware pentru GPS (UART2)
#define RXD2 17  //  TX GPS
#define TXD2 16  //  RX GPS
HardwareSerial serialGPS(2);
TinyGPSPlus gps;

// OLED screen I2C (SDA=21, SCL=22)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//  ADXL345 accelerometer I2C (SDA=21, SCL=22)
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
const int pinIntrareADXL = 14; // Connected to Pin 14 on ESP32

// variable used for interrupts (volatile to avoid loop statement)
volatile unsigned long currentPeriod = 0;
volatile unsigned long lastPeriod = 0;
unsigned long sleepTimeout = 0;
const unsigned long deepSleepPeriod = 8000;
unsigned long lastRPM = 0;
volatile bool newEdge = false;


//interrupt function to put ESP in sleep
void IRAM_ATTR impulseRead()
{
    unsigned long now = micros();
    unsigned long diff = now - lastPeriod;

    if(diff > 000)
    {
      currentPeriod = diff;
      lastPeriod = now;
      newEdge = true;
    }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(21,22);

  // Confuguration for OLED screen
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[ERR] Ecranul OLED nu a fost detectat!"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(15, 15);
  display.println("=== SYSTEM ON ===");
  display.setCursor(15, 35);
  display.println("Connecting CAN...");
  display.display();
  delay(1000);

  // Setup ADXL345 
  if(!accel.begin()) {
    Serial.println("[ERR] Senzorul ADXL345 nu a fost detectat!");
  } else {
    accel.setRange(ADXL345_RANGE_16_G); // Set for big shocks
  }

  // Setup GPS module
  serialGPS.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Setup CAN Bus MCP2515 module
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("CAN: MCP2515 pregătit în mod LOOPBACK.");
    CAN0.setMode(MCP_LOOPBACK);
  } else {
    Serial.println("[ERR] Eroare inițializare modul CAN!");
  }

  // Setup comunication pin with Arduino using an intern resistance
  pinMode(ArduinoPin,INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(ArduinoPin),impulseRead,RISING);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)ArduinoPin, 1);

  sleepTimeout = millis();
}

void loop() {
  unsigned long timeC = millis();
  static int rpmbun = 0; 

  // test for gps module
  while (serialGPS.available() > 0) {
    gps.encode(serialGPS.read());
  }

  // Read ADXL
  sensors_event_t event;
  accel.getEvent(&event);

  // Verify for safery period
  unsigned long period1 = 0;
  bool newPulse = false;

  noInterrupts();
  if(newEdge)
  {
    period1 = currentPeriod;
    newPulse = true;
    newEdge = false;
  }
  interrupts();

  //motor stopped condition
  if(micros() - lastPeriod > 300000)
  {
    rpmbun = 0;
  }else if(newPulse && currentPeriod > 0)
  {
    unsigned long rawRPM = 30000000UL/ currentPeriod;

    if(rawRPM >= 800 && rawRPM < 6000)
    {
      rpmbun = (rpmbun * 3 + rawRPM) / 4;
      sleepTimeout = timeC;
    }
  }



// CAN Bus
  byte CAN[8] = {0};
  CAN[0] = (rpmbun >> 8) & 0xFF;
  CAN[1] = rpmbun & 0xFF;

  if (gps.speed.isValid()) {
    CAN[2] = (byte)gps.speed.kmph(); 
  } else {
    CAN[2] = 0; 
  }

  CAN0.sendMsgBuf(0x1F0, 0, 8, CAN);

  // CAN bus c (Loopback)
  long unsigned int rxId;
  unsigned char len = 0;
  unsigned char rxBuf[8];

  int rpmCAN = 0;
  int speedCAN = 0;

  if (CAN0.checkReceive() == CAN_MSGAVAIL) {
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
    if (rxId == 0x1F0) {
      rpmCAN = (rxBuf[0] << 8) | rxBuf[1];
      speedCAN = rxBuf[2];
    }
  }

  // Update OLED screen
  static unsigned long lastOLED = 0;

  if(timeC - lastOLED >= 100)
  {
    lastOLED = timeC;
    display.clearDisplay(); 

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("CAN ID: 0x1F0");

    display.setTextSize(2);
    display.setCursor(0, 15);
    display.print(rpmCAN);
    display.print(" RPM");

    display.setTextSize(1);
    display.setCursor(0, 37);
    display.print("Viteza GPS: ");
    display.print(speedCAN);
    display.print(" km/h");

    display.setCursor(0, 47);
    display.print("G-Force X: ");
    display.print(event.acceleration.x, 1);
    display.print(" m/s2");

    display.setCursor(0, 57);
    if (rpmbun == 0) {
      long timpRamas = (long)deepSleepPeriod - (long)(timeC - sleepTimeout);
      if (timpRamas < 0) timpRamas = 0;
      int sec = timpRamas / 1000;
      
      display.print("Sleep in: ");
      display.print(sec);
      display.print("s");
    } else {
      display.print("Sateliti GPS: ");
      display.print(gps.satellites.value());
    }
    display.display();
  }
// Telemetry (JSON over Serial)
  if (timeC - ultimulJSON >= intervalJSON) {
    ultimulJSON = timeC;

    sensors_event_t eventS;
    accel.getEvent(&eventS);

    JsonDocument doc;
    doc["timestamp"] = "LIVE"; 
    
    JsonObject slave = doc["nodes"]["slave_simulator"].to<JsonObject>();
    slave["mcu"] = "Arduino Uno";
    slave["data"]["engine_rpm"] = rpmbun;

    JsonObject master = doc["nodes"]["master_telemetry"].to<JsonObject>();
    master["mcu"] = "ESP32";
    master["data"]["gps_speed_kmh"] = gps.speed.isValid() ? (int)gps.speed.kmph() : 0;
    master["data"]["satellites_locked"] = gps.satellites.isValid() ? gps.satellites.value() : 0;
    master["data"]["g_force_x"] = event.acceleration.x;

    if (gps.location.isValid()) {
      master["data"]["latitude"] = gps.location.lat();
      master["data"]["longitude"] = gps.location.lng();
    } else {
      master["data"]["latitude"] = 0.0;
      master["data"]["longitude"] = 0.0;
    }

    // Sent JSON text to pc through UART 
    serializeJson(doc, Serial);
    Serial.println();
  }
  

// Deep Sleep statement
  if (rpmbun == 0 && (timeC - sleepTimeout >= deepSleepPeriod)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 25);
    display.print("SYSTEM GOING TO SLEEP");
    display.display();
    delay(1500);
    
    display.ssd1306_command(SSD1306_DISPLAYOFF); 
    Serial.println("Sistemul intră în Deep Sleep. Aștept impuls de trezire...");
    
    esp_deep_sleep_start(); 
  }
}
