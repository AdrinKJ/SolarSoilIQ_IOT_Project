#include "DHT.h"
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "LoRaWan_APP.h"
#include "HT_SSD1306Wire.h"

#define DHTPIN 4
#define DHTTYPE DHT22
#define SOILPIN 7
#define BATTPIN 6
#define SOLAR_THRESHOLD_V 4.0
#define BATT_CALIBRATION 1.204

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
bool lora_idle = true;

float lastTemp = 0, lastHumidity = 0, lastVoltage = 0, lastCurrent = 0, lastBattV = 0;
int lastSoil = 0;
char lastPowerSource = 'B';

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void OnTxDone(void) {
  Serial.println("TX done");
  lora_idle = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("TX Timeout");
  lora_idle = true;
}

float readBatteryVoltage() {
  long total = 0;
  const int samples = 50;
  for (int i = 0; i < samples; i++) {
    total += analogReadMilliVolts(BATTPIN);
    delay(10);
  }
  float avgMv = total / (float)samples;
  float dividerVoltage = avgMv / 1000.0;
  return dividerVoltage * 2 * BATT_CALIBRATION;
}

void showReadings(int countdown) {
  display.clear();
  display.drawString(0, 0, "Node A - Next TX: " + String(countdown) + "s");
  display.drawString(0, 13, "T:" + String(lastTemp) + " H:" + String(lastHumidity));
  display.drawString(0, 26, "Soil:" + String(lastSoil) + "% P:" + String(lastPowerSource));
  display.drawString(0, 39, "V:" + String(lastVoltage) + " C:" + String(lastCurrent));
  display.drawString(0, 52, "Batt:" + String(lastBattV) + "V");
  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.println("SolarSoil Node A - LoRa Transmitter");

  VextON();
  delay(100);

  dht.begin();
  I2C_INA.begin(33, 34);
  ina219.begin(&I2C_INA);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

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
}

void loop() {
  if (lora_idle) {
    // Countdown from 10 to 1, updating display each second with last known readings
    for (int i = 10; i >= 1; i--) {
      showReadings(i);
      delay(1000);
    }

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    int soilRaw = analogRead(SOILPIN);
    int soilPercent = map(soilRaw, 2514, 1464, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100);
    float busVoltage = ina219.getBusVoltage_V();
    float current_mA = ina219.getCurrent_mA();
    char powerSource = (busVoltage >= SOLAR_THRESHOLD_V) ? 'S' : 'B';
    float battVoltage = readBatteryVoltage();

    // Save for next countdown cycle's display
    lastTemp = temperature;
    lastHumidity = humidity;
    lastSoil = soilPercent;
    lastVoltage = busVoltage;
    lastCurrent = current_mA;
    lastPowerSource = powerSource;
    lastBattV = battVoltage;

    sprintf(txpacket, "T:%.1f,H:%.1f,Soil:%d,V:%.2f,C:%.2f,P:%c,B:%.2f",
            temperature, humidity, soilPercent, busVoltage, current_mA, powerSource, battVoltage);

    Serial.printf("Sending: %s\n", txpacket);

    display.clear();
    display.drawString(0, 0, "Sending...");
    display.drawString(0, 15, txpacket);
    display.display();

    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    lora_idle = false;
  }
  Radio.IrqProcess();
}