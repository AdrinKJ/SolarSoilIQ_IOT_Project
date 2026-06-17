#include "DHT.h"
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "LoRaWan_APP.h"
#include "HT_SSD1306Wire.h"

#define DHTPIN 4
#define DHTTYPE DHT22
#define SOILPIN 7

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

void setup() {
  Serial.begin(115200);
  Serial.println("SolarSoil Node A - LoRa Transmitter");

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
}

void loop() {
  if (lora_idle) {
    delay(10000);

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    int soilRaw = analogRead(SOILPIN);
    int soilPercent = map(soilRaw, 2514, 1464, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100);
    float busVoltage = ina219.getBusVoltage_V();

    sprintf(txpacket, "T:%.1f,H:%.1f,Soil:%d,V:%.2f",
            temperature, humidity, soilPercent, busVoltage);

    Serial.printf("Sending: %s\n", txpacket);

    display.clear();
    display.drawString(0, 0, "Node A - Sensor Data");
    display.drawString(0, 15, "T:" + String(temperature) + " H:" + String(humidity));
    display.drawString(0, 30, "Soil:" + String(soilPercent) + "%");
    display.drawString(0, 45, "V:" + String(busVoltage));
    display.display();

    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    lora_idle = false;
  }
  Radio.IrqProcess();
}