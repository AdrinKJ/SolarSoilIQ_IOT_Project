#include <SPI.h>
#include "LoRaWan_APP.h"
#include "HT_SSD1306Wire.h"

#define RF_FREQUENCY 868000000
#define TX_OUTPUT_POWER 14
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
static RadioEvents_t RadioEvents;
char rxpacket[256];

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Serial.printf("Received: %s, RSSI: %d\n", rxpacket, rssi);

  display.clear();
  display.drawString(0, 0, "SolarSoil IQ Gateway");
  display.drawString(0, 15, rxpacket);
  display.drawString(0, 35, "RSSI: " + String(rssi));
  display.display();

  Radio.Sleep();
  Radio.Rx(0);
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  VextON();
  delay(100);
  display.init();
  display.clear();
  display.drawString(0, 0, "Waiting for data...");
  display.display();

  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                     LORA_CODINGRATE, 0, 8, 0, false, 0, true, 0, 0, false, true);
  Radio.Rx(0);
}

void loop() {
  Radio.IrqProcess();
}