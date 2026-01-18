#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 
#endif

const char* USERS_URL = "https://sinusos.ovh/api.php?device=1";

int activeUsersCount = 0;
unsigned long lastUsersUpdate = 0;
const unsigned long USERS_UPDATE_INTERVAL = 60000;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int BOOT_BTN_PIN = 0; 
const int BUTTON_PIN = 13; 
ESP8266WebServer server(80);
DNSServer dnsServer;
struct Config {
  char ssid[32];
  char password[64];
  char cityName[32];
  float latitude;
  float longitude;
  bool showTemperature, showHumidity, showPressure, showDescription, showDateTime, powerSavingMode, configured, initialized;
  int language, displayFps;
};
Config config;
String weatherData = "Brak danych";
unsigned long lastWeatherUpdate = 0, lastDisplayUpdate = 0, lastUserInteraction = 0, lastTimeUpdate = 0;
bool displayNeedsRefresh = true, powerSavingActive = false, apMode = false, showInfoBox = false, bootBtnActive = false;
const unsigned long POWER_SAVING_TIMEOUT = 60000, WEATHER_REFRESH_INTERVAL = 60000, TIME_UPDATE_INTERVAL = 60000, NTP_SYNC_INTERVAL = 3600000, FACTORY_RESET_HOLD_TIME = 10000;
unsigned long bootBtnTimer = 0;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
unsigned long lastNtpSync = 0;
bool timeNeedsSync = true;
const unsigned char PROGMEM factory_reset_icon[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x04, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00,
  0x00, 0x00, 0x08, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x04, 0x00, 0x08, 0x00, 0xF1, 0xF1, 0xC1, 0xC4, 0x2F, 0xBC, 0x44, 0x00, 0x00,
  0x00, 0x00, 0x20, 0x02, 0x00, 0x10, 0x00, 0x89, 0x04, 0x24, 0x12, 0x28, 0x22, 0x28, 0x00, 0x00,
  0x04, 0x00, 0x40, 0x02, 0x00, 0x10, 0x00, 0xF1, 0xE4, 0x04, 0x11, 0x4F, 0x3C, 0x10, 0x00, 0x00,
  0x02, 0x00, 0x80, 0x01, 0x00, 0x20, 0x00, 0x89, 0xF3, 0xC1, 0xC0, 0x8F, 0xA2, 0x10, 0x00, 0x00,
  0x01, 0x00, 0x80, 0x00, 0x80, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00,
  0x01, 0x01, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x42, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x38, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const char* texts[][60] = {
  {"Inicjalizacja...", "Laczenie z WiFi", "Polaczono z WiFi", "Tryb konfiguracji", "Polacz z WiFi:", "Adres strony:", "lub", "Skonfiguruj", "urzadzenie", "Wejdz na:", "Brak polaczenia", "Tryb AP aktywny err1", "Brak", "polaczenia WiFi", "err1", "konfiguracje", "lokalizacji", "Wejdz na strone", "Slonecznie", "Glownie slonecznie", "Czesciowe zachmurzenie", "Pochmurnie", "Mgla", "Osad sadzi", "Lekka mzawka", "Umiarkowana mzawka", "Gesta mzawka", "Lekki deszcz", "Umiarkowany deszcz", "Gesty deszcz", "Lekki snieg", "Umiarkowany snieg", "Gesty snieg", "Przelotna mzawka", "Umiark. przel. mzawka", "Gesta przel. mzawka", "Przelotny snieg", "Gesty przel. snieg", "Burza", "Burza z gradem", "Silna burza z gradem", "Nieznana pogoda", "Wilg", "Cisn", "Brak danych", "pogodowych", "Reset", "ustawien", "Restartowanie...", "Czekaj...", "Zapisano", "Zmiana jezyka...", "Nowa wersja!", "Pob...", "Wersja:", "Aktualna:"},
  {"Initialization...", "Connecting to WiFi", "Connected to WiFi", "Configuration mode", "Connect to WiFi:", "Address:", "or", "Configure", "device", "Go to:", "No connection", "AP mode active err1", "No", "WiFi connection", "err1", "configuration", "location", "Go to page", "Sunny", "Mainly sunny", "Partly cloudy", "Cloudy", "Fog", "Rime fog", "Light drizzle", "Moderate drizzle", "Dense drizzle", "Light rain", "Moderate rain", "Heavy rain", "Light snow", "Moderate snow", "Heavy snow", "Light showers", "Moderate showers", "Heavy showers", "Light snow showers", "Heavy snow showers", "Thunderstorm", "Thunderstorm with hail", "Heavy thunderstorm with hail", "Unknown weather", "Hum", "Press", "No data", "weather data", "Reset", "settings", "Restarting...", "Wait...", "Saved", "Changing lang...", "New version!", "DL...", "Version:", "Current:"}
};
String getText(int index) {
  if (config.language < 0 || config.language > 1) config.language = 0;
  return String(texts[config.language][index]);
}
void startAccessPoint(), setupWiFi(), updateDisplay(), handleRoot(), handleSaveWifi(), handleSaveLocation(), handleSaveSettings(), handleScan(), handleReboot(), handleFactoryReset(), handleConfirmReset(), handleWeather();
void displayMessage(String l1, String l2 = "", String l3 = "", String l4 = "");
void updateWeather(), displayWeatherWithColors(), displayPowerSaving(), checkButton(), checkPowerSaving(), checkTimeSync(), updateTimeDisplay();
bool syncNTPTime();
void loadConfig(), saveConfig(), drawInfoBox(), checkBootButton(), displayResetWarning(int secondsLeft), bootAnimationSinuOS();
bool getCoordinatesFromCity(String city);
String getClassicHTML(), limitText(String text, int maxLength), getWeatherDescriptionFromCode(int c), getDateTime(), getDate();
float extractValue(String data, String key);
String extractDescription(String data), urlEncode(String str);

void updateActiveUsers() {
  if (WiFi.status() == WL_CONNECTED && !apMode) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    if (http.begin(client, USERS_URL) && http.GET() == 200) {
      activeUsersCount = http.getString().toInt();
    }
    http.end();
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  EEPROM.begin(sizeof(Config) + 50);
  Wire.begin(); 
  Wire.setClock(400000);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  bootAnimationSinuOS();
  loadConfig();
  setupWiFi();
  
  if (!apMode && WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin("sinusoslite")) {
        MDNS.addService("http", "tcp", 80);
    }
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    syncNTPTime();
  }
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/savewifi", handleSaveWifi);
  server.on("/savelocation", handleSaveLocation);
  server.on("/savesettings", handleSaveSettings);
  server.on("/weather", handleWeather);
  server.on("/reboot", handleReboot);
  server.on("/factoryreset", handleFactoryReset);
  server.on("/confirmreset", handleConfirmReset);
  server.onNotFound([]() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.begin();
  if (!apMode && config.configured && WiFi.status() == WL_CONNECTED && config.latitude != 0) {
    updateWeather();
    lastWeatherUpdate = millis();
  }
  lastUserInteraction = lastTimeUpdate = millis();
}
void loop() {
  unsigned long currentTime = millis();
  checkBootButton();
  if (bootBtnActive) { delay(10); return; }
  if (apMode) dnsServer.processNextRequest();
  server.handleClient();
  MDNS.update(); 
  checkButton();
  checkPowerSaving();
  if (!apMode) checkTimeSync();
  updateTimeDisplay();
  if (!apMode && WiFi.status() == WL_CONNECTED) {
    if (currentTime - lastUsersUpdate > USERS_UPDATE_INTERVAL) {
      updateActiveUsers();
      lastUsersUpdate = currentTime;
    }
  }
  if (!apMode && config.configured && WiFi.status() == WL_CONNECTED && config.latitude != 0) {
    if (currentTime - lastWeatherUpdate > WEATHER_REFRESH_INTERVAL || currentTime < lastWeatherUpdate) {
      if (!powerSavingActive) {
        updateWeather();
        lastWeatherUpdate = currentTime;
      }
    }
  }
  if (currentTime - lastDisplayUpdate >= 16) {
    updateDisplay();
    lastDisplayUpdate = currentTime;
  }
}
void checkBootButton() {
  if (digitalRead(BOOT_BTN_PIN) == LOW) {
    if (!bootBtnActive) {
      bootBtnActive = true;
      bootBtnTimer = millis();
    } else {
      unsigned long heldTime = millis() - bootBtnTimer;
      if (heldTime >= FACTORY_RESET_HOLD_TIME) {
        handleFactoryReset();
      } else {
        displayResetWarning((FACTORY_RESET_HOLD_TIME - heldTime) / 1000 + 1);
      }
    }
  } else if (bootBtnActive) {
    bootBtnActive = false;
    displayNeedsRefresh = true;
    lastDisplayUpdate = 0;
  }
}
void displayResetWarning(int secondsLeft) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  String warningText = (config.language == 0) ? "UWAGA!" : "WARNING!";
  display.setCursor((128 - warningText.length() * 12) / 2, 0);
  display.println(warningText);
  display.setTextSize(1);
  String countText = (config.language == 0) ?
    "Ust. fabr za: " + String(secondsLeft) + "s" : "Wipe data in: " + String(secondsLeft) + "s";
  display.setCursor((128 - countText.length() * 6) / 2, 20);
  display.println(countText);
  display.drawBitmap(0, 32, factory_reset_icon, 128, 32, WHITE);
  display.display();
}
void setupWiFi() {
  if (config.configured && strlen(config.ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    displayMessage(getText(1), config.ssid);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(1000);
      attempts++;
      if (attempts % 2 == 0) displayMessage(getText(1), config.ssid, String(attempts) + "/20s");
    }
    if (WiFi.status() == WL_CONNECTED) {
      apMode = false;
      displayMessage(getText(2), "sinusoslite.local", "v: 1.5 ESP8266");
    } else {
      displayMessage(getText(10), getText(14));
      delay(20000);
      WiFi.disconnect(true);
      startAccessPoint();
    }
  } else {
    startAccessPoint();
  }
}
void startAccessPoint() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("SinusOS Lite", "");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());
  displayMessage(getText(3), "SinusOS Lite", "Connect with device by wifi");
}
void factoryReset() {
  config.initialized = false;
  config.configured = false;
  memset(config.ssid, 0, 32);
  memset(config.password, 0, 64);
  memset(config.cityName, 0, 32);
  EEPROM.put(0, config);
  EEPROM.commit();
  WiFi.disconnect(true);
  delay(500);
  displayMessage(getText(46), getText(47), getText(48));
  delay(2000);
  ESP.restart();
}
void handleFactoryReset() { 
  server.send(200, "text/html", "<h1>Resetting...</h1><p>Wait 10s...</p>"); 
  delay(1000);
  factoryReset();
}
void handleConfirmReset() { factoryReset(); }
void displayMessage(String l1, String l2, String l3, String l4) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  if(l1.length()) display.println(limitText(l1,21));
  if(l2.length()) { display.setCursor(0,10); display.println(limitText(l2,21)); }
  if(l3.length()) { display.setCursor(0,20); display.println(limitText(l3,21)); }
  if(l4.length()) { display.setCursor(0,30); display.println(limitText(l4,21)); }
  display.display();
}
void updateTimeDisplay() {
  unsigned long currentTime = millis();
  if (currentTime - lastTimeUpdate > TIME_UPDATE_INTERVAL) {
    lastTimeUpdate = currentTime;
  }
}
bool syncNTPTime() {
  if (WiFi.status() != WL_CONNECTED) return false;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, 5000)) return false;
  lastNtpSync = millis();
  timeNeedsSync = false;
  return true;
}
void checkTimeSync() {
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (timeNeedsSync || currentTime - lastNtpSync > NTP_SYNC_INTERVAL) {
      syncNTPTime();
    }
  }
}
void checkButton() {
  static unsigned long lastButtonPress = 0;
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastButtonPress > 300) {
      lastButtonPress = millis();
      if (powerSavingActive) {
        powerSavingActive = false;
        lastUserInteraction = millis();
        if (!apMode && WiFi.status() == WL_CONNECTED) timeNeedsSync = true;
      } else {
        showInfoBox = !showInfoBox;
        lastUserInteraction = millis();
      }
      displayNeedsRefresh = true;
      updateDisplay();
      lastDisplayUpdate = millis();
    }
  }
}
void checkPowerSaving() {
  if (config.powerSavingMode && !powerSavingActive) {
    if (millis() - lastUserInteraction > POWER_SAVING_TIMEOUT) {
      powerSavingActive = true;
      showInfoBox = false;
    }
  }
}
void loadConfig() {
  EEPROM.get(0, config);
  if (config.initialized != true || isnan(config.latitude)) {
    config.latitude = 50.0024;
    config.longitude = 18.4630;
    config.showTemperature = true;
    config.showHumidity = false;
    config.showPressure = false;
    config.showDescription = true;
    config.showDateTime = true;
    config.powerSavingMode = false;
    config.configured = false;
    config.initialized = true;
    config.language = 0;
    config.displayFps = 60;
    memset(config.ssid, 0, 32);
    memset(config.password, 0, 64);
    String("Wodzislaw Slaski").toCharArray(config.cityName, 32);
    saveConfig();
  }
  if (config.displayFps < 1 || config.displayFps > 60) config.displayFps = 20;
}
void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}
String limitText(String text, int maxLength) {
  return text.length() > maxLength ? text.substring(0, maxLength) : text;
}
String getDateTime() {
  if (apMode) return "AP Mode err1";
  struct tm t;
  if(!getLocalTime(&t)) return "--:--";
  char s[10];
  strftime(s, 10, "%H:%M", &t);
  return String(s);
}
String getDate() {
  if (apMode) return WiFi.softAPIP().toString();
  struct tm t;
  if(!getLocalTime(&t)) return "--.--.--";
  char s[15];
  strftime(s, 15, "%d.%m.%Y", &t);
  return String(s);
}
void drawInfoBox() {
  int boxW = 122, boxH = 60;
  int x = (128 - boxW) / 2, y = (64 - boxH) / 2;
  display.fillRect(x, y, boxW, boxH, BLACK);
  display.drawRect(x, y, boxW, boxH, WHITE);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(x + 6, y + 4);
  display.print("SinusOS");
  display.setTextSize(1);
  display.setCursor(x + 92, y + 12); 
  display.print("lite");
  display.setCursor(x + 6, y + 22);
  display.print("Ver: 1.5");
  display.setCursor(x + 6, y + 31);
  display.println("Dev: Blazesztos");
  display.setCursor(x + 6, y + 41);
  display.println("Thx: croxtyl");
  display.setCursor(x + 6, y + 50);
  display.println("Thx: Mikrosat");
}

void updateDisplay() {
  if (apMode) {
    displayMessage(getText(7), getText(9), "Connect with device", "by wifi(AP Mode err1)");
    return;
  }
  if (powerSavingActive) {
    displayPowerSaving();
  } else {
    displayWeatherWithColors();
  }
  if (!powerSavingActive && showInfoBox) drawInfoBox();
  display.display();
}
void displayPowerSaving() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  String t = getDateTime(), d = getDate();
  int yb = 32;
  if (config.showDateTime) {
    display.setTextSize(2);
    display.setCursor((128 - d.length()*12)/2, 5);
    display.println(d);
    display.drawLine(0, yb, 128, yb, WHITE);
  }
  if (config.showDescription) {
    display.setTextSize(2);
    display.setCursor((128 - t.length()*12)/2, yb+2);
    display.println(t);
    String desc = extractDescription(weatherData);
    display.setTextSize(1);
    display.setCursor((128 - (desc.length()>21?21:desc.length())*6)/2, yb+20);
    display.println(limitText(desc, 21));
  } else {
    display.setTextSize(3);
    display.setCursor((128 - t.length()*18)/2, yb+5);
    display.println(t);
  }
}
void displayWeatherWithColors() {
  display.clearDisplay();
  int yb = config.showDateTime ? 25 : 0;
  if (config.showDateTime) {
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0,0);
    display.println(getDateTime());
    display.setTextSize(1);
    display.setCursor(0,18);
    display.println(getDate());
    display.drawLine(0, yb, 128, yb, WHITE);
  }
  int y = yb + 3;
  float temp = extractValue(weatherData, "temp");
  float hum = extractValue(weatherData, "humidity");
  float press = extractValue(weatherData, "pressure");
  bool any = false;
  if (config.showTemperature && temp > -50 && temp < 60) {
    display.setCursor(5, y);
    display.setTextSize(2);
    display.print(temp, 1);
    display.setTextSize(1);
    display.print("C");
    any = true;
  }
  if (config.showPressure && press > 800) {
    display.setCursor(5, y + 18);
    display.setTextSize(1);
    display.print(getText(43) + ": ");
    display.print(press, 0);
    display.print(" hPa");
    any = true;
  }
  if (config.showHumidity && hum >= 0) {
    display.setCursor(5, y + 28);
    display.setTextSize(1);
    display.print(getText(42) + ": ");
    display.print(hum, 0);
    display.print("%");
    any = true;
  }
  if (!any) {
    display.setCursor(0, y);
    display.println(getText(44));
  }
}
float extractValue(String data, String key) {
  int s = data.indexOf(key + ":");
  if (s == -1) return -999;
  s += key.length() + 1;
  int e = data.indexOf(",", s);
  if (e == -1) e = data.indexOf("}", s);
  if (e == -1) e = data.length();
  return data.substring(s, e).toFloat();
}
String extractDescription(String data) {
  int s = data.indexOf("description:");
  if (s == -1) return "";
  s += 12;
  int e = data.indexOf(",", s);
  if (e == -1) e = data.length();
  String d = data.substring(s, e);
  d.replace("\"", "");
  return d;
}
void bootAnimationSinuOS() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  int centerY = 32;
  float amp = 12, freq = 0.25;
  int startX = 5, endX = 45;
  int prevX = startX, prevY = centerY + sin(0) * amp;
  for (int x = startX + 1; x <= endX; x++) {
    float y = centerY + sin((x - startX) * freq) * amp;
    display.drawLine(prevX, prevY, x, (int)y, WHITE);
    prevX = x; prevY = (int)y;
    if (x % 2 == 0) { display.display(); delay(15); }
  }
  display.display(); delay(300);
  String rest = "inusOS";
  display.setTextSize(2);
  int textX = endX + 5, textY = centerY - 8;
  for (int i = 0; i < rest.length(); i++) {
    display.setCursor(textX + i * 12, textY);
    display.print(rest[i]);
    display.display(); delay(120);
  }
  display.setTextSize(1);
  String liteText = "lite";
  display.setCursor(textX + ((rest.length() * 12 - liteText.length() * 6) / 2), textY + 20);
  display.print(liteText);
  display.display(); delay(700);
}
void updateWeather() {
  if (WiFi.status() == WL_CONNECTED && !apMode) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(config.latitude,6) + 
                 "&longitude=" + String(config.longitude,6) + 
                 "&current=temperature_2m,relative_humidity_2m,surface_pressure,weather_code&timezone=Europe/Warsaw";
    if (http.begin(client, url)) {
      if (http.GET() == 200) {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, http.getString());
        JsonObject c = doc["current"];
        float t = c["temperature_2m"];
        float h = c["relative_humidity_2m"];
        float p = c["surface_pressure"];
        int wc = c["weather_code"];
        weatherData = "temp:" + String(t) + ",humidity:" + String(h) + ",pressure:" + String(p) + ",description:" + getWeatherDescriptionFromCode(wc);
      }
      http.end();
    }
  }
}
String getWeatherDescriptionFromCode(int c) {
  if(c==0) return getText(18);
  if(c==1) return getText(19); if(c==2) return getText(20);
  if(c==3) return getText(21); if(c>=45&&c<=48) return getText(22); if(c>=51&&c<=55) return getText(24);
  if(c>=61&&c<=65) return getText(27);
  if(c>=71&&c<=77) return getText(30); if(c>=80&&c<=82) return getText(33);
  if(c>=95) return getText(38); return getText(41);
}
String urlEncode(String str) {
  String encodedString = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) encodedString += c;
    else if (c == ' ') encodedString += '+';
    else { encodedString += String('%');
    if ((unsigned char)c < 16) encodedString += "0"; encodedString += String((unsigned char)c, HEX); }
  }
  return encodedString;
}
bool getCoordinatesFromCity(String city) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(city) + "&count=1&language=pl&format=json";
  if (!http.begin(client, url)) return false;
  bool success = false;
  if (http.GET() == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    if (doc.containsKey("results") && doc["results"].size() > 0) {
      config.latitude = doc["results"][0]["latitude"];
      config.longitude = doc["results"][0]["longitude"];
      String(doc["results"][0]["name"]).toCharArray(config.cityName, 32);
      success = true;
    }
  }
  http.end();
  return success;
}
String getClassicHTML() {
  String lang = config.language == 0 ? "pl" : "en";
  String title = "SinusOS lite (ESP8266)";
  String html = "<!DOCTYPE html><html lang='" + lang + "'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>" + title + "</title>";
  html += "<style>:root { --bg: #f4f6f9; --card-bg: #ffffff; --text: #333333; --accent: #4361ee; --border: #e0e0e0; --success: #2ec4b6; --danger: #ef476f; }";
  html += "[data-theme='dark'] { --bg: #121212; --card-bg: #1e1e1e; --text: #e0e0e0; --accent: #4cc9f0; --border: #333333; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: var(--bg); color: var(--text); margin: 0; padding: 20px; transition: 0.3s; }";
  html += ".container { max-width: 500px; margin: 0 auto; }";
  html += ".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }";
  html += ".card { background: var(--card-bg); border-radius: 12px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); border: 1px solid var(--border); }";
  html += "h1 { margin: 0; font-size: 1.5rem; color: var(--accent); }";
  html += "h2 { font-size: 1.1rem; margin-top: 0; margin-bottom: 15px; border-bottom: 1px solid var(--border); padding-bottom: 10px; }";
  html += "input, select { width: 100%; padding: 10px; margin: 5px 0 15px; border-radius: 8px; border: 1px solid var(--border); background: var(--bg); color: var(--text); box-sizing: border-box; }";
  html += "button { width: 100%; padding: 12px; background: var(--accent); color: white; border: none; border-radius: 8px; cursor: pointer; font-weight: bold; transition: 0.2s; }";
  html += "button:hover { opacity: 0.9; transform: translateY(-1px); }";
  html += ".btn-danger { background: var(--danger); }";
  html += ".checkbox-group { display: flex; flex-direction: column; gap: 10px; }";
  html += ".checkbox-item { display: flex; align-items: center; gap: 10px; }";
  html += ".checkbox-item input { width: auto; margin: 0; }";
  html += ".theme-toggle { background: none; border: none; color: var(--text); font-size: 1.5rem; width: auto; padding: 5px; }</style></head><body>";
  html += "<div class='container'><div class='header'><h1>" + title + "</h1><button class='theme-toggle' onclick='toggleTheme()'>&#9728;&#65039;</button></div>";
  if (apMode) {
    html += "<div class='card'><h2>WiFi Setup</h2><p>" + String(config.language==0?"Wybierz sieć WiFi:":"Choose WiFi Network:") + "</p><button onclick='scanWifi()'>Skanuj / Scan</button><div id='wifiList'></div></div>";
  } else {
    html += "<div class='card'><h2>" + String(config.language == 0 ? "Ustawienia" : "Settings") + "</h2><label>Język / Language</label><select id='language'>";
    html += "<option value='0'" + String(config.language == 0 ? " selected" : "") + ">Polski</option>";
    html += "<option value='1'" + String(config.language == 1 ? " selected" : "") + ">English</option></select>";
    html += "<div class='checkbox-group'><label class='checkbox-item'><input type='checkbox' id='showTemp' " + String(config.showTemperature?"checked":"") + "> " + (config.language==0?"Temperatura":"Temperature") + "</label>";
    html += "<label class='checkbox-item'><input type='checkbox' id='showHumidity' " + String(config.showHumidity?"checked":"") + "> " + (config.language==0?"Wilgotność":"Humidity") + "</label>";
    html += "<label class='checkbox-item'><input type='checkbox' id='showPressure' " + String(config.showPressure?"checked":"") + "> " + (config.language==0?"Ciśnienie":"Pressure") + "</label>";
    html += "<label class='checkbox-item'><input type='checkbox' id='powerSaving' " + String(config.powerSavingMode?"checked":"") + "> Power Saving Mode</label></div><br>";
    html += "<button onclick='saveSettings()'>" + String(config.language==0?"Zapisz":"Save Settings") + "</button></div>";
    html += "<div class='card'><h2>" + String(config.language==0?"Lokalizacja (Miasto)":"Location (City)") + "</h2>";
    html += "<input type='text' id='city' value='" + String(config.cityName) + "' placeholder='" + String(config.language==0?"Wpisz nazwę miasta":"Enter city name") + "'>";
    html += "<p style='font-size:0.8rem; color:#666; margin-bottom:10px;'>Aktualne wsp: " + String(config.latitude, 4) + ", " + String(config.longitude, 4) + "</p>";
    html += "<button onclick='saveLoc()'>" + String(config.language==0?"Szukaj i Zapisz":"Search & Save") + "</button></div>";
    html += "<div class='card'><button class='btn-danger' onclick='factoryReset()'>Factory Reset</button></div>";
  }
  html += "<script>const oldLang = " + String(config.language) + ";";
  html += "function toggleTheme() { let b=document.body; let t=b.getAttribute('data-theme'); b.setAttribute('data-theme',t==='dark'?'light':'dark'); localStorage.setItem('theme',t==='dark'?'light':'dark'); }";
  html += "(function(){ let t=localStorage.getItem('theme'); document.body.setAttribute('data-theme', t ? t : 'dark'); })();";
  html += "function scanWifi(){document.getElementById('wifiList').innerHTML='Scanning...';fetch('/scan').then(r=>r.json()).then(d=>{let h='';d.forEach(w=>{h+='<div style=\"margin:5px 0\"><b>'+w.ssid+'</b> <button style=\"width:auto;padding:5px\" onclick=\"conn(\\''+w.ssid+'\\')\">Connect</button></div>'});document.getElementById('wifiList').innerHTML=h;})}";
  html += "function conn(s){let p=prompt('Password for '+s); if(p){fetch('/savewifi?ssid='+encodeURIComponent(s)+'&password='+encodeURIComponent(p)).then(r=>r.text()).then(t=>{alert(t);location.reload()})}}";
  html += "function saveLoc(){let c=document.getElementById('city').value; if(!c) return; fetch('/savelocation?city='+encodeURIComponent(c)).then(r=>r.text()).then(t=>{alert(t);location.reload()})}";
  html += "function factoryReset(){if(confirm('Factory Reset?'))location.href='/factoryreset'}";
  html += "function saveSettings(){let newLang=parseInt(document.getElementById('language').value);let s={language:newLang,showTemperature:document.getElementById('showTemp').checked,showHumidity:document.getElementById('showHumidity').checked,showPressure:document.getElementById('showPressure').checked,powerSavingMode:document.getElementById('powerSaving').checked};";
  html += "fetch('/savesettings',{method:'POST',body:JSON.stringify(s)}).then(r=>r.text()).then(t=>{if(newLang!==oldLang){alert('Language changed. Device rebooting...');}else{alert('Settings saved.');location.reload();}});}</script></div></body></html>";
  return html;
}
void handleRoot() { server.send(200, "text/html", getClassicHTML()); }
void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for(int i = 0; i < n; ++i) {
    if(i) j += ",";
    j += "{\"ssid\":\"" + WiFi.SSID(i) + "\"}";
  }
  j += "]";
  server.send(200, "application/json", j);
}
void handleSaveWifi() {
  if(server.hasArg("ssid")) {
    server.arg("ssid").toCharArray(config.ssid, 32);
    server.arg("password").toCharArray(config.password, 64);
    config.configured = true;
    saveConfig();
    server.send(200, "text/plain", "OK. Rebooting...");
    delay(500);
    ESP.restart();
  }
}
void handleSaveLocation() {
  if(server.hasArg("city")) {
    if(getCoordinatesFromCity(server.arg("city"))) {
      saveConfig();
      updateWeather();
      String msg = (config.language == 0 ? "Znaleziono: " : "Found: ") + String(config.cityName);
      server.send(200, "text/plain", msg);
    } else {
      server.send(200, "text/plain", config.language == 0 ? "Błąd! Nie znaleziono miasta." : "Error! City not found.");
    }
  } else if (server.hasArg("lat") && server.hasArg("lon")) {
    config.latitude = server.arg("lat").toFloat();
    config.longitude = server.arg("lon").toFloat();
    saveConfig();
    updateWeather();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing params");
  }
}
void handleSaveSettings() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, server.arg("plain"));
    int newLang = doc["language"];
    bool langChanged = (newLang != config.language);
    config.language = newLang;
    config.displayFps = 60;
    config.showTemperature = doc["showTemperature"];
    config.showHumidity = doc["showHumidity"];
    config.showPressure = doc["showPressure"];
    config.powerSavingMode = doc["powerSavingMode"];
    saveConfig();
    if (langChanged) {
      server.send(200, "text/plain", "REBOOT");
      delay(500);
      ESP.restart();
    } else {
      server.send(200, "text/plain", "SAVED");
      displayNeedsRefresh = true;
      lastDisplayUpdate = 0;
      updateWeather();
    }
  }
}
void handleReboot() { server.send(200, "text/plain", "OK"); delay(500); ESP.restart(); }
void handleWeather() { server.send(200, "text/plain", weatherData); }