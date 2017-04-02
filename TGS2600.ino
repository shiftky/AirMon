#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include "consts.h"

#define SPI_CS  4
#define BZ      5
#define LED     16

void setupOTA() {
  Serial.begin(115200);

  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(otaHostname);
  //ArduinoOTA.setPassword(otaPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

unsigned long readSPICh0() {
  byte byteMSB, byteLSB;

  digitalWrite(SPI_CS, LOW);
  byteMSB = SPI.transfer(0b01101000);
  byteLSB = SPI.transfer(0b00000000);
  digitalWrite(SPI_CS, HIGH);

  return ((byteMSB << 8) + byteLSB) & 0x3FF;
}

void setup() {
  setupOTA();

  pinMode(BZ, OUTPUT);
  pinMode(LED, OUTPUT);

  pinMode(SPI_CS, OUTPUT);
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
}

void loop() {
  ArduinoOTA.handle();

  unsigned long sensor, val;
  val = readSPICh0();
  sensor = map(val, 0, 1023, 0, 5000);
  Serial.println(sensor);

  digitalWrite(LED, HIGH);
  delay(500);
  digitalWrite(LED, LOW);
  delay(500);
}
