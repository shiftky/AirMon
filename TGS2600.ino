#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <Ticker.h>
#include <pt.h>
#include "consts.h"

#define SPI_CS  4
#define BZ      5
#define LED     16
#define SEND_INTERVAL_SEC   10
#define BZ_INTERVAL_MSEC    500

ESP8266WebServer server(80);
Ticker ticker;
static struct pt pt;
bool sendTickerFlag = false;
unsigned long sensorValue;
int bzCount = 0;

unsigned long readSPICh0() {
  byte byteMSB, byteLSB;

  digitalWrite(SPI_CS, LOW);
  byteMSB = SPI.transfer(0b01101000);
  byteLSB = SPI.transfer(0b00000000);
  digitalWrite(SPI_CS, HIGH);

  return ((byteMSB << 8) + byteLSB) & 0x3FF;
}

unsigned long getSensorVal() {
  unsigned long sensor, val;
  val = readSPICh0();
  sensor = map(val, 0, 1023, 0, 5000);
  return sensor;
}

static int protothread(struct pt *pt) {
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(true) {
    PT_WAIT_UNTIL(pt, millis() - timestamp > BZ_INTERVAL_MSEC);
    timestamp = millis();
    if (bzCount > 0) {
      digitalWrite(LED, !digitalRead(LED));
      digitalWrite(BZ, !digitalRead(BZ));
      bzCount--;
    }
  }
  PT_END(pt);
}

void setSendTickerFlag() {
  sendTickerFlag = true;
}

void sendToInfluxDB() {
  digitalWrite(LED, HIGH);

  String postData = String(seriesKey) + "," + String(seriesTag) + " value=" + String(sensorValue);
  String url = "/write?db=" + String(dbName);

  HTTPClient http;
  http.begin(influxdbAddress, influxdbPort, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Content-Length", String(postData.length()));

  int httpCode = http.POST(postData);
  if (httpCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
  } else {
    Serial.println("[HTTP] no connection or no HTTP server.");
  }

  http.end();
  digitalWrite(LED, LOW);
}

void handleRoot() {
  char ipBuff[15];
  IPAddress ip = WiFi.localIP();
  sprintf(ipBuff, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  String str = "<h2>TGS2600</h2>";
  str += String(ipBuff) + "<br><br>";
  str += "Sensor val: " + String(sensorValue) + "<br><br>";
  str += "<a href=\"/call_bz?count=5\"><button>Buzzer Test</button></a>";
  server.send(200, "text/html", str);
}

void handleCallBz() {
  String countStr = server.arg("count");
  bzCount = countStr.toInt() * 2;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void setupWifi() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  server.begin();
  Serial.println("Server started");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);

  setupWifi();
  MDNS.begin("airmon");

  pinMode(BZ, OUTPUT);
  pinMode(LED, OUTPUT);

  pinMode(SPI_CS, OUTPUT);
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);

  server.on("/", handleRoot);
  server.on("/call_bz", handleCallBz);

  ticker.attach(SEND_INTERVAL_SEC, setSendTickerFlag);
  PT_INIT(&pt);
}

void loop() {
  sensorValue = getSensorVal();

  if (sendTickerFlag) {
    sendToInfluxDB();
    sendTickerFlag = false;
  }

  server.handleClient();
  protothread(&pt);
}
