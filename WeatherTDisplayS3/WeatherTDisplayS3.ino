#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager  version 2.0.17
#include <TFT_eSPI.h>
#include <ArduinoJson.h>  // 7.1.0
#include <HTTPClient.h>   // https://github.com/arduino-libraries/ArduinoHttpClient  version 0.6.1
#include <ESP32Time.h>    // https://github.com/fbiego/ESP32Time  version 2.0.6
#include "NotoSansBold15.h"
#include "tinyFont.h"
#include "smallFont.h"
#include "midleFont.h"
#include "bigFont.h"
#include "font18.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite errSprite = TFT_eSprite(&tft);
ESP32Time rtc(0);

//#################### EDIT THIS  ###################
int zone = -4; //Daylight Savings EST -5 for Non Daylight Savings
String town = "town"; ///put %20 in spaces
String myAPI = "openweathermapapikey";
String units = "imperial";  // metric, imperial
//#################### end of edits ###################

const char* ntpServer = "pool.ntp.org";
String server = "https://api.openweathermap.org/data/2.5/weather?q=" + town + "&appid=" + myAPI + "&units=" + units;

// Additional variables
int ani = 100;
float maxT;
float minT;
unsigned long timePased = 0;
int counter = 0;

// Colors
#define bck TFT_BLACK
unsigned short grays[13];

// Static strings of data shown on right side
char* PPlbl[] = { "HUM", "PRESS", "WIND" };
String PPlblU[] = { "%", "hPa", "m/s" };

// Data that changes
float temperature = 22.2;
float wData[3];
float PPpower[24] = {};
float PPpowerT[24] = {};
int PPgraph[24] = { 0 };

// Scrolling message on bottom right side
String Wmsg = "";

void setTime() {
  configTime(3600 * zone, 0, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
    Serial.println("Time updated: " + rtc.getTime());
  } else {
    Serial.println("Failed to sync time");
  }
}

void setup() {
  Serial.begin(115200); // Initialize Serial for debugging
  Serial.println("Starting setup...");

  // Using this board can work on battery
  pinMode(15, OUTPUT);
  digitalWrite(15, 1);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connecting to WIFI!!", 30, 50, 4);
  sprite.createSprite(320, 170);
  errSprite.createSprite(164, 15);

  // Set brightness (original LEDC code, no fix)
  ledcSetup(0, 10000, 8);
  ledcAttachPin(38, 0);
  ledcWrite(0, 130);

  // Connect board to WiFi
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(5000);
  if (!wifiManager.autoConnect("VolosWifiConf", "password")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }
  Serial.println("WiFi connected.");

  setTime();
  getData();

  // Generate 13 levels of gray
  int co = 210;
  for (int i = 0; i < 13; i++) {
    grays[i] = tft.color565(co, co, co);
    co = co - 20;
  }
}

void getData() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(5000);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi reconnect failed");
      return;
    }
  }

  HTTPClient http;
  http.begin(server);
  int httpResponseCode = http.GET();
  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("Payload: ");
    Serial.println(payload);

    StaticJsonDocument<2048> doc; // Increased buffer size
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      temperature = doc["main"]["temp"];
      wData[0] = doc["main"]["humidity"];
      wData[1] = doc["main"]["pressure"];
      wData[2] = doc["wind"]["speed"];
      int visibility = doc["visibility"];
      const char* description = doc["weather"][0]["description"];
      Wmsg = "#Description: " + String(description) + "  #Visibility: " + String(visibility) + " #Updated: " + rtc.getTime();
      Serial.print("Data updated | Temperature: ");
      Serial.println(temperature);
    } else {
      Serial.print("JSON Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void draw() {
  Serial.print("Drawing with temperature: ");
  Serial.println(temperature);

  errSprite.fillSprite(grays[10]);
  errSprite.setTextColor(grays[1], grays[10]);
  errSprite.drawString(Wmsg, ani, 4);

  sprite.fillSprite(TFT_BLACK);
  sprite.drawLine(138, 10, 138, 164, grays[6]);
  sprite.drawLine(100, 108, 134, 108, grays[6]);
  sprite.setTextDatum(0);

  // LEFT SIDE
  sprite.loadFont(midleFont);
  sprite.setTextColor(grays[1], TFT_BLACK);
  sprite.drawString("WEATHER", 6, 10);
  sprite.unloadFont();

  sprite.loadFont(font18);
  sprite.setTextColor(grays[7], TFT_BLACK);
  sprite.drawString("TOWN:", 6, 110);
  sprite.setTextColor(grays[2], TFT_BLACK);
  if (units == "metric")
    sprite.drawString("C", 14, 50);
  if (units == "imperial")
    sprite.drawString("F", 14, 50);

  sprite.setTextColor(grays[3], TFT_BLACK);
  sprite.drawString(town, 46, 110);
  sprite.fillCircle(8, 52, 2, grays[2]);
  sprite.unloadFont();

  // Draw time without seconds
  sprite.loadFont(tinyFont);
  sprite.setTextColor(grays[4], TFT_BLACK);
  sprite.drawString(rtc.getTime().substring(0, 5), 6, 132);
  sprite.unloadFont();

  // Draw some static text
  sprite.setTextColor(grays[5], TFT_BLACK);
  sprite.drawString("INTERNET", 86, 10);
  sprite.drawString("STATION", 86, 20);
  sprite.setTextColor(grays[7], TFT_BLACK);
  sprite.drawString("SECONDS", 92, 157);

  // Draw temperature
  sprite.setTextDatum(4);
  sprite.loadFont(bigFont);
  sprite.setTextColor(grays[0], TFT_BLACK);
  sprite.drawFloat(temperature, 1, 69, 80);
  sprite.unloadFont();

  // Draw seconds rectangle
  sprite.fillRoundRect(90, 132, 42, 22, 2, grays[2]);
  // Draw seconds
  sprite.loadFont(font18);
  sprite.setTextColor(TFT_BLACK, grays[2]);
  sprite.drawString(rtc.getTime().substring(6, 8), 111, 144);
  sprite.unloadFont();

  sprite.setTextDatum(0);
  // RIGHT SIDE
  sprite.loadFont(font18);
  sprite.setTextColor(grays[1], TFT_BLACK);
  sprite.drawString("LAST 12 HOUR", 144, 10);
  sprite.unloadFont();

  sprite.fillRect(144, 28, 84, 2, grays[10]);
  sprite.setTextColor(grays[3], TFT_BLACK);
  sprite.drawString("MIN:" + String(minT), 254, 10);
  sprite.drawString("MAX:" + String(maxT), 254, 20);
  sprite.fillSmoothRoundRect(144, 34, 174, 60, 3, grays[10], bck);
  sprite.drawLine(170, 39, 170, 88, TFT_WHITE);
  sprite.drawLine(170, 88, 314, 88, TFT_WHITE);

  sprite.setTextDatum(4);
  for (int j = 0; j < 24; j++)
    for (int i = 0; i < PPgraph[j]; i++)
      sprite.fillRect(173 + (j * 6), 83 - (i * 4), 4, 3, grays[2]);

  sprite.setTextColor(grays[2], grays[10]);
  sprite.drawString("MAX", 158, 42);
  sprite.drawString("MIN", 158, 86);
  sprite.loadFont(font18);
  sprite.setTextColor(grays[7], grays[10]);
  sprite.drawString("T", 158, 58);
  sprite.unloadFont();

  for (int i = 0; i < 3; i++) {
    sprite.fillSmoothRoundRect(144 + (i * 60), 100, 54, 32, 3, grays[9], bck);
    sprite.setTextColor(grays[3], grays[9]);
    sprite.drawString(PPlbl[i], 144 + (i * 60) + 27, 107);
    sprite.setTextColor(grays[2], grays[9]);
    sprite.loadFont(font18);
    sprite.drawString(String((int)wData[i]) + PPlblU[i], 144 + (i * 60) + 27, 124);
    sprite.unloadFont();

    sprite.fillSmoothRoundRect(144, 148, 174, 16, 2, grays[10], bck);
    errSprite.pushToSprite(&sprite, 148, 150);
  }
  sprite.setTextColor(grays[4], bck);
  sprite.drawString("CURRENT WEATHER", 190, 141);
  sprite.setTextColor(grays[9], bck);
  sprite.drawString(String(counter), 310, 141);

  sprite.pushSprite(0, 0);
}

void updateData() {
  // Scrolling weather message
  ani--;
  if (ani < -420) {
    ani = 100;
  }

  // Periodic update
  Serial.print("millis(): ");
  Serial.print(millis());
  Serial.print(" | timePased: ");
  Serial.println(timePased);
  if (millis() - timePased >= 180000) {
    timePased = millis();
    counter++;
    Serial.print("Update triggered | Counter: ");
    Serial.println(counter);
    getData();

    if (counter == 10) {
      Serial.println("Performing 30-minute update");
      setTime();
      counter = 0;
      maxT = -50;
      minT = 1000;
      PPpower[23] = temperature;
      for (int i = 23; i > 0; i--) {
        PPpower[i - 1] = PPpowerT[i];
      }
      for (int i = 0; i < 24; i++) {
        PPpowerT[i] = PPpower[i];
        if (PPpower[i] < minT) minT = PPpower[i];
        if (PPpower[i] > maxT) maxT = PPpower[i];
      }
      for (int i = 0; i < 24; i++) {
        PPgraph[i] = map(PPpower[i], minT, maxT, 0, 12);
      }
      Serial.print("PPpower[23]: ");
      Serial.print(PPpower[23]);
      Serial.print(" | minT: ");
      Serial.print(minT);
      Serial.print(" | maxT: ");
      Serial.println(maxT);
    }
  }
}

void loop() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    Serial.println("Heartbeat: Code is running");
    lastPrint = millis();
  }
  updateData();
  draw();
}
