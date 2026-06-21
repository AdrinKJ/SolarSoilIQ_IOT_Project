#include "DHT.h"
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "LoRaWan_APP.h"
#include "HT_SSD1306Wire.h"

#define DHTPIN 4
#define DHTTYPE DHT22
#define SOILPIN 7
#define SOLAR_THRESHOLD_V 4.0
#define SLEEP_SECONDS 10

#define RF_FREQUENCY 868000000
#define TX_OUTPUT_POWER 14
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
TwoWire I2C_INA = TwoWire(1);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_INA219 ina219;
static RadioEvents_t RadioEvents;
char txpacket[256];
volatile bool txDone = false;

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void OnTxDone(void) {
  Serial.println("TX done");
  txDone = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("TX Timeout");
  txDone = true; // proceed to sleep anyway, don't get stuck forever
}

void goToSleep() {
  Serial.println("Going to deep sleep for " + String(SLEEP_SECONDS) + " seconds...");
  display.clear();
  display.drawString(0, 0, "Sleeping...");
  display.display();
  delay(100);
  esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Serial.println("SolarSoil Node A - Waking up");

  VextON();
  delay(100);

  dht.begin();
  I2C_INA.begin(33, 34);
  ina219.begin(&I2C_INA);

  display.init();
  display.clear();
  display.drawString(0, 0, "Node A Starting...");
  display.display();

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE, 8, false,
                     true, 0, 0, false, 3000);

  delay(500); // let DHT22 stabilize after power-up

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int soilRaw = analogRead(SOILPIN);
  int soilPercent = map(soilRaw, 2514, 1464, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);
  float busVoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  char powerSource = (busVoltage >= SOLAR_THRESHOLD_V) ? 'S' : 'B';

  sprintf(txpacket, "T:%.1f,H:%.1f,Soil:%d,V:%.2f,C:%.2f,P:%c",
          temperature, humidity, soilPercent, busVoltage, current_mA, powerSource);

  Serial.printf("Sending: %s\n", txpacket);

  display.clear();
  display.drawString(0, 0, "Node A - Sensor Data");
  display.drawString(0, 15, "T:" + String(temperature) + " H:" + String(humidity));
  display.drawString(0, 30, "Soil:" + String(soilPercent) + "%");
  display.drawString(0, 45, "V:" + String(busVoltage) + " C:" + String(current_mA));
  display.drawString(0, 60, powerSource == 'S' ? "Power: SOLAR" : "Power: BATTERY");
  display.display();

  Radio.Send((uint8_t *)txpacket, strlen(txpacket));

  // Wait until transmission confirms complete, with a safety timeout
  unsigned long waitStart = millis();
  while (!txDone && millis() - waitStart < 5000) {
    Radio.IrqProcess();
  }

  goToSleep();
}

void loop() {
  // Not used - everything happens in setup() after each wake cycle
}