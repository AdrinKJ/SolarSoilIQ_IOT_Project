#include <SPI.h>
#include "LoRaWan_APP.h"
#include "HT_SSD1306Wire.h"
#include <WiFi.h>
#include <PubSubClient.h>

#define RF_FREQUENCY 868000000
#define TX_OUTPUT_POWER 14
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1

const char* ssid = "OnePlus 12R";
const char* password = "12345678";
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "solarsoil/nodeB";

WiFiClient espClient;
PubSubClient client(espClient);

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
static RadioEvents_t RadioEvents;
char rxpacket[256];
bool firstPacketReceived = false;

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    String clientId = "NodeB-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT connected!");
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// Parses "T:25.1,H:46.0,Soil:65,V:5.2,C:0.15,P:S,B:5.78" into JSON with RSSI added
void parseAndPublish(char* packet, int16_t rssi) {
  float temp = 0, humidity = 0, voltage = 0, current = 0, battVoltage = 0;
  int soil = 0;
  char powerSource = 'U'; // Unknown if not found

  char* tPos = strstr(packet, "T:");
  char* hPos = strstr(packet, "H:");
  char* soilPos = strstr(packet, "Soil:");
  char* vPos = strstr(packet, "V:");
  char* cPos = strstr(packet, "C:");
  char* pPos = strstr(packet, "P:");
  char* bPos = strstr(packet, "B:");

  if (tPos) temp = atof(tPos + 2);
  if (hPos) humidity = atof(hPos + 2);
  if (soilPos) soil = atoi(soilPos + 5);
  if (vPos) voltage = atof(vPos + 2);
  if (cPos) current = atof(cPos + 2);
  if (pPos) powerSource = *(pPos + 2);
  if (bPos) battVoltage = atof(bPos + 2);

  const char* powerStr = (powerSource == 'S') ? "solar" : (powerSource == 'B') ? "battery" : "unknown";

  char jsonPayload[300];
  snprintf(jsonPayload, sizeof(jsonPayload),
           "{\"temp\":%.1f,\"humidity\":%.1f,\"soil\":%d,\"v\":%.2f,\"current\":%.2f,\"rssi\":%d,\"power_source\":\"%s\",\"battery_v\":%.2f}",
           temp, humidity, soil, voltage, current, rssi, powerStr, battVoltage);

  Serial.println("Publishing: " + String(jsonPayload));
  client.publish(mqtt_topic, jsonPayload);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Serial.printf("Received: %s, RSSI: %d\n", rxpacket, rssi);

  display.clear();

  if (!firstPacketReceived) {
    display.drawString(0, 0, "Handshake Positive!");
    display.drawString(0, 15, "Connection Established");
    display.display();
    firstPacketReceived = true;
    delay(2000);
    display.clear();
  }

  // Split the packet across two lines so nothing overflows the OLED width
  String packetStr = String(rxpacket);
  int splitPoint = packetStr.length() / 2;
  while (splitPoint < packetStr.length() && packetStr.charAt(splitPoint) != ',') {
    splitPoint++;
  }
  String line1 = packetStr.substring(0, splitPoint + 1);
  String line2 = packetStr.substring(splitPoint + 1);

  display.drawString(0, 0, "SolarSoil Gateway");
  display.drawString(0, 15, line1);
  display.drawString(0, 28, line2);
  display.drawString(0, 42, "RSSI: " + String(rssi));
  display.display();

  if (!client.connected()) reconnectMQTT();
  parseAndPublish(rxpacket, rssi);

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
  display.drawString(0, 0, "Connecting WiFi...");
  display.display();

  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);

  display.clear();
  display.drawString(0, 0, "Waiting for Node A...");
  display.display();

  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                     LORA_CODINGRATE, 0, 8, 0, false, 0, true, 0, 0, false, true);
  Radio.Rx(0);
}

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();
  Radio.IrqProcess();
}