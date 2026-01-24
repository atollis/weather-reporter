/*
 * Weather Display - Horizontal Landscape Design
 * Using OpenWeatherMap One Call API 3.0
 *
 * Location: Aroona, QLD
 *
 * Wiring:
 * SCL  -> GPIO18
 * SDA  -> GPIO23
 * DC   -> GPIO27
 * RST  -> GPIO4
 * CS   -> GPIO5
 * VCC  -> 3.3V
 * GND  -> GND
 *
 * Buttons:
 * LEFT   -> GPIO12
 * RIGHT  -> GPIO13
 * SELECT -> GPIO14
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "credentials.h"


// OpenWeatherMap settings (API key in credentials.h)
const char* LATITUDE = "-26.7984";
const char* LONGITUDE = "153.1394";

// Button pins
#define BTN_LEFT   13
#define BTN_RIGHT  12
#define BTN_SELECT 14

// Update interval - 5 minutes
const unsigned long UPDATE_INTERVAL = 300000;
unsigned long lastUpdate = 0;

// Button debounce
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 200;

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Screen states
enum Screen { SCREEN_HOURLY, SCREEN_HOURLY2, SCREEN_CONDITIONS, SCREEN_DAILY, SCREEN_SETTINGS, SCREEN_ABOUT, SCREEN_DEMO, SCREEN_DEMO2, SCREEN_DEMO3 };
Screen currentScreen = SCREEN_HOURLY;  // Start on hourly forecast

// Auto page switch
const unsigned long PAGE_SWITCH_INTERVAL = 3000;  // 3 seconds
unsigned long lastPageSwitch = 0;
bool autoSwitch = false;  // Enable auto switching

// Colon flash timer
unsigned long lastColonUpdate = 0;
const unsigned long COLON_FLASH_INTERVAL = 500;
bool colonVisible = true;

// Display power state
bool displayOn = true;

// Screen modes: weather screens vs settings screens
bool settingsMode = false;  // false = weather (Current, Hourly, Daily), true = settings (Settings, Demo)
const unsigned long LONG_PRESS_TIME = 800;  // Long press threshold in ms
unsigned long selectPressStart = 0;
bool selectHeld = false;
bool longPressHandled = false;  // Prevents short-click firing after long-press

// Color palette - elegant dark mode
#define COLOR_BG       0x0000  // Pure black
#define COLOR_TEXT     0xD69A  // Soft white (toned down)
#define COLOR_SUBTLE   0x8410  // Grey for secondary text
#define COLOR_RAIN_BG  0x0A1F  // Very dark blue for rain bars
#define COLOR_SUN      0xFE60  // Warm yellow
#define COLOR_CLOUD    0x8C71  // Light grey (highlight) - toned down
#define COLOR_CLOUD_MID 0x6B6D // Medium grey
#define COLOR_CLOUD_DARK 0x4228 // Dark grey (shadow)
#define COLOR_RAIN     TFT_BLUE  // Pure blue for rain/humidity
#define COLOR_BOLT     0xFFE0  // Yellow for lightning
#define COLOR_SNOW     0xBDF7  // Light blue-white for snow
#define COLOR_SUCCESS  0x3666  // Muted green
#define COLOR_ACCENT   0xFD20  // Orange accent
#define COLOR_DAYTIME  0xFFDB  // Cornsilk #FFF8DC
#define COLOR_MOON     0x9CD3  // Grey moon, slightly lighter than clouds
#define COLOR_OVERCAST 0x4208  // Darker grey for overcast

// Weather data structure
struct HourlyData {
  float temperature;
  int weatherCode;
  int hour;
};

struct DailyData {
  float tempMin;
  float tempMax;
  int weatherCode;
  String dayName;
  int pop;  // Probability of precipitation (0-100%)
};

struct WeatherData {
  float temperature;
  float apparent_temp;
  int humidity;
  float windSpeed;
  int windDeg;
  String windDir;
  int weatherCode;
  String condition;
  float minutelyRain[60];
  bool hasMinutelyData;
  bool dataValid;
  HourlyData hourly[24];
  int hourlyCount;
  DailyData daily[8];
  int dailyCount;
  time_t sunrise;
  time_t sunset;
  // Additional current conditions
  float uvi;
  int visibility;  // meters
  int pressure;    // hPa
  float dewPoint;
  int clouds;      // percentage
  // Moon data (from daily[0])
  time_t moonrise;
  time_t moonset;
  float moonPhase; // 0-1
} weather;

// Last updated time
String lastUpdateTime = "";

// Function declarations
void connectToWiFi();
void fetchOneCallData();
void displayHourlyForecast();
void displayHourlyForecast2();
void displayConditions();
void displayDailyForecast();
void displaySettings();
void displayAbout();
void displayDemo();
void displayDemo2();
void displayDemo3();
void displayConnecting();
void displayError(String msg);
void bootAnimation();
String degToCompass(int deg);
bool isDaytime();
void drawWeatherIcon(int code, int x, int y, int size, bool isNight = false);
void drawWindArrow(int x, int y, int deg, int size);
void handleButtons();
void drawScreenIndicator();
void drawHeader();
void drawFooter();
uint16_t getTempColor(float temp);
void swipeTransition(Screen from, Screen to);
void displayScreen(Screen screen);

void setup() {
  Serial.begin(115200);
  delay(100);

  // Setup buttons with internal pullup
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);  // Horizontal landscape mode (320x240)
  tft.invertDisplay(true);
  tft.fillScreen(COLOR_BG);

  connectToWiFi();

  // Show fetch status on screen
  tft.drawString("Fetching weather data...", 10, 175);

  fetchOneCallData();

  tft.drawString("Done", 10, 203);
  delay(500);

  displayHourlyForecast();
  lastUpdate = millis();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectToWiFi();
    Serial.println("WiFi reconnected successfully.");
  }

  handleButtons();

  // Auto switch pages every 3 seconds
  if (autoSwitch && millis() - lastPageSwitch >= PAGE_SWITCH_INTERVAL) {
    Screen nextScreen;
    switch (currentScreen) {
      case SCREEN_HOURLY: nextScreen = SCREEN_HOURLY2; break;
      case SCREEN_HOURLY2: nextScreen = SCREEN_CONDITIONS; break;
      case SCREEN_CONDITIONS: nextScreen = SCREEN_DAILY; break;
      case SCREEN_DAILY: nextScreen = SCREEN_SETTINGS; break;
      case SCREEN_SETTINGS: nextScreen = SCREEN_ABOUT; break;
      case SCREEN_ABOUT: nextScreen = SCREEN_DEMO; break;
      case SCREEN_DEMO: nextScreen = SCREEN_DEMO2; break;
      case SCREEN_DEMO2: nextScreen = SCREEN_DEMO3; break;
      case SCREEN_DEMO3: nextScreen = SCREEN_HOURLY; break;
      default: nextScreen = SCREEN_HOURLY; break;
    }
    swipeTransition(currentScreen, nextScreen);
    currentScreen = nextScreen;
    lastPageSwitch = millis();
  }

  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    Serial.println("Updating weather data...");
    fetchOneCallData();
    if (displayOn) {
      displayScreen(currentScreen);
    }
    lastUpdate = millis();
    Serial.print("Update complete. Next update in ");
    Serial.print(UPDATE_INTERVAL / 60000);
    Serial.println(" minutes.");
  }

  // Flash colon in time display
  if (displayOn && millis() - lastColonUpdate >= COLON_FLASH_INTERVAL) {
    colonVisible = !colonVisible;
    lastColonUpdate = millis();
    // Only update header on screens that show it
    if (currentScreen == SCREEN_HOURLY || currentScreen == SCREEN_CONDITIONS || currentScreen == SCREEN_DAILY) {
      drawHeader();
    }
  }

  delay(50);
}

void handleButtons() {
  bool leftPressed = digitalRead(BTN_LEFT) == LOW;
  bool rightPressed = digitalRead(BTN_RIGHT) == LOW;
  bool selectPressed = digitalRead(BTN_SELECT) == LOW;

  // Handle SELECT long press detection
  if (selectPressed) {
    if (!selectHeld && !longPressHandled) {
      selectPressStart = millis();
      selectHeld = true;
    } else if (selectHeld && millis() - selectPressStart >= LONG_PRESS_TIME) {
      // Long press detected - toggle settings mode
      selectHeld = false;
      longPressHandled = true;  // Prevent short-click on release
      lastButtonPress = millis();
      settingsMode = !settingsMode;
      if (settingsMode) {
        currentScreen = SCREEN_SETTINGS;
      } else {
        currentScreen = SCREEN_HOURLY;
      }
      displayScreen(currentScreen);
      return;
    }
  } else {
    // SELECT released
    if (selectHeld && millis() - selectPressStart < LONG_PRESS_TIME) {
      // Short press - toggle display
      selectHeld = false;
      lastButtonPress = millis();
      if (!displayOn) {
        displayOn = true;
        tft.writecommand(0x29);
        // Return to default screen when turning on
        settingsMode = false;
        currentScreen = SCREEN_HOURLY;
        displayScreen(currentScreen);
      } else {
        displayOn = false;
        tft.writecommand(0x28);
      }
      return;
    }
    selectHeld = false;
    longPressHandled = false;  // Reset for next press
  }

  // Debounce for left/right
  if (millis() - lastButtonPress < DEBOUNCE_DELAY) return;

  // If display is off, any button turns it on
  if (!displayOn && (leftPressed || rightPressed)) {
    lastButtonPress = millis();
    displayOn = true;
    tft.writecommand(0x29);
    // Return to default screen when turning on
    settingsMode = false;
    currentScreen = SCREEN_HOURLY;
    displayScreen(currentScreen);
    return;
  }

  if (leftPressed || rightPressed) {
    lastButtonPress = millis();
    Screen previousScreen = currentScreen;

    if (settingsMode) {
      // Settings mode: Settings <-> About <-> Demo <-> Demo2 <-> Demo3
      if (leftPressed) {
        switch (currentScreen) {
          case SCREEN_SETTINGS: currentScreen = SCREEN_ABOUT; break;
          case SCREEN_ABOUT: currentScreen = SCREEN_DEMO; break;
          case SCREEN_DEMO: currentScreen = SCREEN_DEMO2; break;
          case SCREEN_DEMO2: currentScreen = SCREEN_DEMO3; break;
          case SCREEN_DEMO3: currentScreen = SCREEN_SETTINGS; break;
          default: currentScreen = SCREEN_SETTINGS; break;
        }
      } else {
        switch (currentScreen) {
          case SCREEN_SETTINGS: currentScreen = SCREEN_DEMO3; break;
          case SCREEN_ABOUT: currentScreen = SCREEN_SETTINGS; break;
          case SCREEN_DEMO: currentScreen = SCREEN_ABOUT; break;
          case SCREEN_DEMO2: currentScreen = SCREEN_DEMO; break;
          case SCREEN_DEMO3: currentScreen = SCREEN_DEMO2; break;
          default: currentScreen = SCREEN_SETTINGS; break;
        }
      }
    } else {
      // Weather mode: Hourly <-> Hourly2 <-> Conditions <-> Daily
      if (leftPressed) {
        switch (currentScreen) {
          case SCREEN_HOURLY: currentScreen = SCREEN_HOURLY2; break;
          case SCREEN_HOURLY2: currentScreen = SCREEN_CONDITIONS; break;
          case SCREEN_CONDITIONS: currentScreen = SCREEN_DAILY; break;
          case SCREEN_DAILY: currentScreen = SCREEN_HOURLY; break;
          default: currentScreen = SCREEN_HOURLY; break;
        }
      } else {
        switch (currentScreen) {
          case SCREEN_HOURLY: currentScreen = SCREEN_DAILY; break;
          case SCREEN_HOURLY2: currentScreen = SCREEN_HOURLY; break;
          case SCREEN_CONDITIONS: currentScreen = SCREEN_HOURLY2; break;
          case SCREEN_DAILY: currentScreen = SCREEN_CONDITIONS; break;
          default: currentScreen = SCREEN_HOURLY; break;
        }
      }
    }

    if (currentScreen != previousScreen) {
      swipeTransition(previousScreen, currentScreen);
      lastPageSwitch = millis();
    }
  }
}

void connectToWiFi() {
  int lineY = 35;
  int lineHeight = 24;

  tft.fillScreen(COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);

  // Product name (larger font)
  tft.setTextFont(4);
  tft.drawString("Weather Reporter", 10, lineY);
  lineY += 35;

  // Status messages

  // Show connecting status
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  tft.drawString("Connecting to WiFi...", 10, lineY);
  lineY += lineHeight;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    tft.drawString("WiFi Connected!", 10, lineY);
    lineY += lineHeight;

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    tft.drawString("IP: " + WiFi.localIP().toString(), 10, lineY);
    lineY += lineHeight;

    configTime(10 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Syncing time...");
    tft.drawString("Syncing time...", 10, lineY);
  } else {
    Serial.println("\nWiFi Connection Failed!");
    tft.drawString("WiFi Connection Failed!", 10, lineY);
  }
}

void displayConnecting() {
  tft.fillScreen(COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Connecting", 160, 100);
  tft.setTextFont(2);
  tft.drawString(WIFI_SSID, 160, 130);
}

void displayError(String msg) {
  tft.fillScreen(COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_RED, COLOR_BG);
  tft.drawString(msg, 160, 120);
}

String degToCompass(int deg) {
  const char* dirs[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int index = ((deg + 11) / 22) % 16;
  return String(dirs[index]);
}

bool isDaytime() {
  time_t now = time(NULL);
  if (now < 1000000000) return true;  // Default to day if time not synced
  return (now >= weather.sunrise && now < weather.sunset);
}

// Get color based on temperature: blue (cold) -> white (neutral) -> orange (hot)
uint16_t getTempColor(float temp) {
  // Clamp temperature to range
  if (temp <= 15) {
    return TFT_BLUE;  // Cold - blue
  }
  else if (temp >= 40) {
    return COLOR_ACCENT;  // Hot - orange
  }
  else if (temp >= 24 && temp <= 26) {
    return COLOR_TEXT;  // Neutral - white
  }
  else if (temp < 24) {
    // Gradient from blue (15) to white (24)
    float ratio = (temp - 15.0) / 9.0;  // 0 at 15, 1 at 24
    int r = ratio * 255;
    int g = ratio * 255;
    int b = 255;
    return tft.color565(r, g, b);
  }
  else {
    // Gradient from white (26) to orange (40)
    float ratio = (temp - 26.0) / 14.0;  // 0 at 26, 1 at 40
    int r = 255;
    int g = 255 - (ratio * 155);  // 255 to 100
    int b = 255 - (ratio * 255);  // 255 to 0
    return tft.color565(r, g, b);
  }
}

void fetchOneCallData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection!");
    weather.dataValid = false;
    return;
  }

  Serial.println("\nFetching One Call API 3.0 data...");

  // Build One Call API 3.0 URL - include hourly and daily data
  String url = "https://api.openweathermap.org/data/3.0/onecall?lat=";
  url += LATITUDE;
  url += "&lon=";
  url += LONGITUDE;
  url += "&units=metric&exclude=alerts&appid=";
  url += OWM_API_KEY;

  Serial.print("URL: ");
  Serial.println(url);

  HTTPClient http;
  http.setTimeout(10000);
  http.begin(url);
  int httpCode = http.GET();

  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Response received");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      // Current weather data
      JsonObject current = doc["current"];
      weather.temperature = current["temp"];
      weather.apparent_temp = current["feels_like"];
      weather.humidity = current["humidity"];
      weather.windSpeed = current["wind_speed"];
      weather.windDeg = current["wind_deg"];
      weather.windDir = degToCompass(weather.windDeg);
      weather.weatherCode = current["weather"][0]["id"];
      weather.condition = current["weather"][0]["description"].as<String>();

      // Capitalize first letter
      if (weather.condition.length() > 0) {
        weather.condition[0] = toupper(weather.condition[0]);
      }

      // Sunrise/sunset data
      weather.sunrise = current["sunrise"].as<time_t>();
      weather.sunset = current["sunset"].as<time_t>();

      // Additional current conditions
      weather.uvi = current["uvi"];
      weather.visibility = current["visibility"];
      weather.pressure = current["pressure"];
      weather.dewPoint = current["dew_point"];
      weather.clouds = current["clouds"];

      // Minutely precipitation data (60 minutes)
      weather.hasMinutelyData = false;
      for (int i = 0; i < 60; i++) {
        weather.minutelyRain[i] = 0;
      }

      if (doc.containsKey("minutely")) {
        JsonArray minutely = doc["minutely"];
        weather.hasMinutelyData = true;
        int count = min((int)minutely.size(), 60);
        for (int i = 0; i < count; i++) {
          weather.minutelyRain[i] = minutely[i]["precipitation"].as<float>();
        }
      }

      // Hourly forecast data
      weather.hourlyCount = 0;
      if (doc.containsKey("hourly")) {
        JsonArray hourly = doc["hourly"];
        weather.hourlyCount = min((int)hourly.size(), 24);
        for (int i = 0; i < weather.hourlyCount; i++) {
          weather.hourly[i].temperature = hourly[i]["temp"];
          weather.hourly[i].weatherCode = hourly[i]["weather"][0]["id"];
          // Get hour from timestamp
          time_t ts = hourly[i]["dt"];
          struct tm* timeinfo = localtime(&ts);
          weather.hourly[i].hour = timeinfo->tm_hour;
        }
      }

      // Daily forecast data
      weather.dailyCount = 0;
      if (doc.containsKey("daily")) {
        JsonArray daily = doc["daily"];
        weather.dailyCount = min((int)daily.size(), 8);
        const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        for (int i = 0; i < weather.dailyCount; i++) {
          weather.daily[i].tempMin = daily[i]["temp"]["min"];
          weather.daily[i].tempMax = daily[i]["temp"]["max"];
          weather.daily[i].weatherCode = daily[i]["weather"][0]["id"];
          weather.daily[i].pop = (int)(daily[i]["pop"].as<float>() * 100);  // Convert 0-1 to 0-100%
          // Get day name from timestamp
          time_t ts = daily[i]["dt"];
          struct tm* timeinfo = localtime(&ts);
          weather.daily[i].dayName = dayNames[timeinfo->tm_wday];
        }
        // Moon data from today (daily[0])
        weather.moonrise = daily[0]["moonrise"].as<time_t>();
        weather.moonset = daily[0]["moonset"].as<time_t>();
        weather.moonPhase = daily[0]["moon_phase"];
      }

      weather.dataValid = true;

      Serial.println("\n=== Weather Data ===");
      Serial.print("Temperature: ");
      Serial.print(weather.temperature);
      Serial.println(" C");
      Serial.print("Hourly points: ");
      Serial.println(weather.hourlyCount);
      Serial.print("Daily points: ");
      Serial.println(weather.dailyCount);
      Serial.println("====================\n");

    } else {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      weather.dataValid = false;
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    weather.dataValid = false;
  }

  http.end();

  // Update time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    lastUpdateTime = String(timeStr);
  }
}

// Draw weather icon using TFT primitives
void drawWeatherIcon(int code, int x, int y, int size, bool isNight) {
  int r = size / 2;

  if (code == 800) {
    if (isNight) {
      // Clear sky at night - moon with craters
      tft.fillCircle(x, y, r * 0.5, COLOR_MOON);
      // Subtle darker craters
      uint16_t craterColor = 0x8410;  // Darker grey
      tft.fillCircle(x - r*0.15, y - r*0.1, r * 0.12, craterColor);
      tft.fillCircle(x + r*0.2, y + r*0.15, r * 0.08, craterColor);
      tft.fillCircle(x - r*0.05, y + r*0.25, r * 0.06, craterColor);
    } else {
      // Clear sky - sun with 12 short pointed rays and orange center
      int numRays = 12;
      float outerR = r * 0.75;   // Shorter rays
      float innerR = r * 0.55;
      for (int i = 0; i < numRays; i++) {
        float angle = i * (360.0 / numRays) * 0.0174533;
        float nextAngle = (i + 1) * (360.0 / numRays) * 0.0174533;
        float midAngle = (angle + nextAngle) / 2;
        int tipX = x + cos(midAngle) * outerR;
        int tipY = y + sin(midAngle) * outerR;
        int base1X = x + cos(angle) * innerR;
        int base1Y = y + sin(angle) * innerR;
        int base2X = x + cos(nextAngle) * innerR;
        int base2Y = y + sin(nextAngle) * innerR;
        tft.fillTriangle(tipX, tipY, base1X, base1Y, base2X, base2Y, COLOR_SUN);
      }
      tft.fillCircle(x, y, r * 0.56, COLOR_ACCENT);  // Slightly larger to cover ray bases
    }
  }
  else if (code == 801) {
    if (isNight) {
      // Few clouds at night - moon with craters behind cloud
      int moonX = x - r*0.3;
      int moonY = y - r*0.2;
      tft.fillCircle(moonX, moonY, r * 0.3, COLOR_MOON);
      uint16_t craterColor = 0x8410;
      tft.fillCircle(moonX - r*0.1, moonY - r*0.05, r * 0.07, craterColor);
      tft.fillCircle(moonX + r*0.1, moonY + r*0.08, r * 0.05, craterColor);
    } else {
      // Few clouds - sun with 12 short rays behind cloud
      int sunX = x - r*0.3;
      int sunY = y - r*0.2;
      int numRays = 12;
      float outerR = r * 0.48;  // Shorter rays
      float innerR = r * 0.35;
      for (int i = 0; i < numRays; i++) {
        float angle = i * (360.0 / numRays) * 0.0174533;
        float nextAngle = (i + 1) * (360.0 / numRays) * 0.0174533;
        float midAngle = (angle + nextAngle) / 2;
        int tipX = sunX + cos(midAngle) * outerR;
        int tipY = sunY + sin(midAngle) * outerR;
        int base1X = sunX + cos(angle) * innerR;
        int base1Y = sunY + sin(angle) * innerR;
        int base2X = sunX + cos(nextAngle) * innerR;
        int base2Y = sunY + sin(nextAngle) * innerR;
        tft.fillTriangle(tipX, tipY, base1X, base1Y, base2X, base2Y, COLOR_SUN);
      }
      tft.fillCircle(sunX, sunY, r * 0.36, COLOR_ACCENT);  // Slightly larger to cover ray bases
    }
    // Cloud with depth - dark base, mid layer, light highlights
    tft.fillCircle(x + r*0.15, y + r*0.35, r * 0.3, COLOR_CLOUD_DARK);  // Shadow
    tft.fillCircle(x + r*0.1, y + r*0.2, r * 0.35, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.4, y + r*0.3, r * 0.3, COLOR_CLOUD_MID);
    tft.fillCircle(x - r*0.2, y + r*0.3, r * 0.25, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.05, y + r*0.15, r * 0.2, COLOR_CLOUD);  // Highlight
  }
  else if (code >= 802 && code <= 803) {
    // Cloudy - layered for depth
    // Dark shadow layer
    tft.fillCircle(x - r*0.25, y + r*0.3, r * 0.4, COLOR_CLOUD_DARK);
    tft.fillCircle(x + r*0.25, y + r*0.25, r * 0.35, COLOR_CLOUD_DARK);
    // Mid layer
    tft.fillCircle(x - r*0.3, y - r*0.1, r * 0.45, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.2, y - r*0.05, r * 0.5, COLOR_CLOUD_MID);
    tft.fillCircle(x - r*0.1, y + r*0.2, r * 0.4, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.35, y + r*0.15, r * 0.35, COLOR_CLOUD_MID);
    // Light highlights on top
    tft.fillCircle(x - r*0.35, y - r*0.2, r * 0.25, COLOR_CLOUD);
    tft.fillCircle(x + r*0.1, y - r*0.15, r * 0.3, COLOR_CLOUD);
  }
  else if (code == 804) {
    // Overcast - darker clouds, no highlights
    tft.fillCircle(x - r*0.25, y + r*0.3, r * 0.4, COLOR_OVERCAST);
    tft.fillCircle(x + r*0.25, y + r*0.25, r * 0.35, COLOR_OVERCAST);
    tft.fillCircle(x - r*0.3, y - r*0.1, r * 0.45, COLOR_CLOUD_DARK);
    tft.fillCircle(x + r*0.2, y - r*0.05, r * 0.5, COLOR_CLOUD_DARK);
    tft.fillCircle(x - r*0.1, y + r*0.2, r * 0.4, COLOR_CLOUD_DARK);
    tft.fillCircle(x + r*0.35, y + r*0.15, r * 0.35, COLOR_CLOUD_DARK);
    // Subtle mid-tone on top
    tft.fillCircle(x - r*0.35, y - r*0.2, r * 0.2, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.1, y - r*0.15, r * 0.25, COLOR_CLOUD_MID);
  }
  else if (code >= 500 && code <= 531) {
    // Rain - cloud with depth and vertical rain lines
    // Dark shadow
    tft.fillCircle(x - r*0.1, y + r*0.05, r * 0.35, COLOR_CLOUD_DARK);
    // Mid layer
    tft.fillCircle(x - r*0.25, y - r*0.3, r * 0.35, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.15, y - r*0.25, r * 0.4, COLOR_CLOUD_MID);
    tft.fillCircle(x - r*0.05, y - r*0.1, r * 0.35, COLOR_CLOUD_MID);
    // Highlights
    tft.fillCircle(x - r*0.3, y - r*0.35, r * 0.2, COLOR_CLOUD);
    tft.fillCircle(x + r*0.1, y - r*0.3, r * 0.22, COLOR_CLOUD);
    // Vertical rain lines
    for (int i = 0; i < 5; i++) {
      int dx = x - r*0.35 + i * r * 0.18;
      int dy1 = y + r*0.15;
      int dy2 = y + r*0.5 + (i % 2) * r * 0.15;  // Staggered lengths
      tft.drawLine(dx, dy1, dx, dy2, COLOR_RAIN);
    }
  }
  else if (code >= 200 && code <= 232) {
    // Thunderstorm - darker clouds for stormy look
    tft.fillCircle(x - r*0.1, y + r*0.05, r * 0.35, COLOR_CLOUD_DARK);
    tft.fillCircle(x - r*0.25, y - r*0.3, r * 0.35, COLOR_CLOUD_DARK);
    tft.fillCircle(x + r*0.15, y - r*0.25, r * 0.4, COLOR_CLOUD_MID);
    tft.fillCircle(x - r*0.05, y - r*0.1, r * 0.35, COLOR_CLOUD_MID);
    // Small highlight
    tft.fillCircle(x + r*0.1, y - r*0.3, r * 0.18, COLOR_CLOUD);
    // Lightning bolt
    int bx = x;
    int by = y + r*0.1;
    tft.fillTriangle(bx, by, bx + r*0.25, by + r*0.3, bx - r*0.1, by + r*0.35, COLOR_BOLT);
    tft.fillTriangle(bx - r*0.05, by + r*0.3, bx + r*0.15, by + r*0.35, bx - r*0.15, by + r*0.7, COLOR_BOLT);
  }
  else if (code >= 600 && code <= 622) {
    // Snow - cloud with depth
    tft.fillCircle(x - r*0.1, y + r*0.05, r * 0.35, COLOR_CLOUD_DARK);
    tft.fillCircle(x - r*0.25, y - r*0.3, r * 0.35, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.15, y - r*0.25, r * 0.4, COLOR_CLOUD_MID);
    tft.fillCircle(x - r*0.05, y - r*0.1, r * 0.35, COLOR_CLOUD_MID);
    tft.fillCircle(x - r*0.3, y - r*0.35, r * 0.2, COLOR_CLOUD);
    tft.fillCircle(x + r*0.1, y - r*0.3, r * 0.22, COLOR_CLOUD);
    // Snowflakes
    tft.fillCircle(x - r*0.3, y + r*0.35, 3, COLOR_SNOW);
    tft.fillCircle(x, y + r*0.45, 3, COLOR_SNOW);
    tft.fillCircle(x + r*0.3, y + r*0.35, 3, COLOR_SNOW);
  }
  else if (code >= 701 && code <= 781) {
    // Atmosphere (mist, fog) - layered with varying opacity
    uint16_t mistDark = tft.color565(70, 70, 70);
    uint16_t mistMid = tft.color565(100, 100, 100);
    uint16_t mistLight = tft.color565(140, 140, 140);
    tft.fillCircle(x - r*0.1, y + r*0.1, r * 0.3, mistDark);
    tft.fillCircle(x - r*0.3, y - r*0.2, r * 0.3, mistMid);
    tft.fillCircle(x + r*0.1, y - r*0.15, r * 0.35, mistMid);
    tft.fillCircle(x - r*0.35, y - r*0.25, r * 0.18, mistLight);
    // Mist lines with varying shades
    for (int i = 0; i < 3; i++) {
      int ly = y + r*0.3 + i * 8;
      uint16_t lineColor = (i == 1) ? mistMid : mistDark;
      tft.drawLine(x - r*0.5, ly, x + r*0.5, ly, lineColor);
    }
  }
  else {
    // Default cloud with depth
    tft.fillCircle(x, y + r*0.1, r * 0.35, COLOR_CLOUD_DARK);
    tft.fillCircle(x - r*0.2, y, r * 0.4, COLOR_CLOUD_MID);
    tft.fillCircle(x + r*0.2, y - r*0.05, r * 0.45, COLOR_CLOUD_MID);
    tft.fillCircle(x - r*0.25, y - r*0.1, r * 0.2, COLOR_CLOUD);
  }
}

// Draw compass arrow pointing in wind direction
void drawWindArrow(int x, int y, int deg, int size) {
  // Convert wind direction to radians (0 = North = up)
  // Wind direction is where wind comes FROM, so arrow points opposite
  float rad = (deg + 180) * 0.0174533;

  // Arrow tip
  int tipX = x + sin(rad) * size;
  int tipY = y - cos(rad) * size;

  // Arrow base points - wider spread for better visibility
  float baseRad1 = rad + 0.5;  // ~30 degrees spread
  float baseRad2 = rad - 0.5;
  int base1X = x + sin(baseRad1) * (size * 0.3);
  int base1Y = y - cos(baseRad1) * (size * 0.3);
  int base2X = x + sin(baseRad2) * (size * 0.3);
  int base2Y = y - cos(baseRad2) * (size * 0.3);

  // Draw thick arrow with multiple triangles for thickness
  tft.fillTriangle(tipX, tipY, base1X, base1Y, base2X, base2Y, COLOR_ACCENT);
  // Draw slightly offset for thickness
  tft.fillTriangle(tipX+1, tipY, base1X+1, base1Y, base2X+1, base2Y, COLOR_ACCENT);
  tft.fillTriangle(tipX, tipY+1, base1X, base1Y+1, base2X, base2Y+1, COLOR_ACCENT);

  // Draw circle at center
  tft.fillCircle(x, y, 4, COLOR_SUBTLE);
  tft.drawCircle(x, y, 6, COLOR_ACCENT);
}

// Draw screen indicator dots at bottom right
void drawScreenIndicator() {
  int y = 222;
  int numDots = settingsMode ? 5 : 4;
  int spacing = 15;
  int startX = 310 - (numDots - 1) * spacing;  // Right aligned

  int screenIndex;
  if (settingsMode) {
    switch (currentScreen) {
      case SCREEN_SETTINGS: screenIndex = 0; break;
      case SCREEN_ABOUT: screenIndex = 1; break;
      case SCREEN_DEMO: screenIndex = 2; break;
      case SCREEN_DEMO2: screenIndex = 3; break;
      case SCREEN_DEMO3: screenIndex = 4; break;
      default: screenIndex = 0; break;
    }
  } else {
    switch (currentScreen) {
      case SCREEN_HOURLY: screenIndex = 0; break;
      case SCREEN_HOURLY2: screenIndex = 1; break;
      case SCREEN_CONDITIONS: screenIndex = 2; break;
      case SCREEN_DAILY: screenIndex = 3; break;
      default: screenIndex = 0; break;
    }
  }

  for (int i = 0; i < numDots; i++) {
    int x = startX + i * spacing;
    if (i == screenIndex) {
      tft.fillCircle(x, y, 4, COLOR_ACCENT);
    } else {
      tft.drawCircle(x, y, 4, COLOR_SUBTLE);
    }
  }
}

// Draw footer with day/date on left (for weather screens)
// Get ordinal suffix for day number
const char* getOrdinalSuffix(int day) {
  if (day >= 11 && day <= 13) return "th";
  switch (day % 10) {
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    default: return "th";
  }
}

void drawFooter() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  char dateStr[32];
  sprintf(dateStr, "%s, %d%s %s", days[timeinfo.tm_wday], timeinfo.tm_mday, getOrdinalSuffix(timeinfo.tm_mday), months[timeinfo.tm_mon]);

  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString(dateStr, 10, 222);
}

// Draw header with location and time
void drawHeader() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  // Location on the left
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Caloundra, QLD", 10, 15);

  // Time on the right with flashing colon
  int hour = timeinfo.tm_hour;
  String ampm = (hour < 12) ? "am" : "pm";
  if (hour == 0) hour = 12;
  else if (hour > 12) hour -= 12;

  // Clear time area first (to handle width changes)
  tft.fillRect(200, 0, 120, 30, COLOR_BG);

  tft.setTextDatum(MR_DATUM);
  uint16_t timeColor = isDaytime() ? COLOR_DAYTIME : COLOR_SUBTLE;

  // Draw hour
  char hourStr[4];
  sprintf(hourStr, "%d", hour);
  tft.setTextColor(timeColor, COLOR_BG);
  int hourWidth = tft.textWidth(hourStr);

  // Calculate positions
  char minStr[8];
  sprintf(minStr, "%02d%s", timeinfo.tm_min, ampm.c_str());
  int minWidth = tft.textWidth(minStr);
  int colonWidth = tft.textWidth(":");

  int totalWidth = hourWidth + colonWidth + minWidth;
  int startX = 310 - totalWidth;

  // Draw components
  tft.setTextDatum(ML_DATUM);
  tft.drawString(hourStr, startX, 15);

  // Colon - dimmed when visible, hidden when not
  if (colonVisible) {
    tft.setTextColor(COLOR_SUBTLE, COLOR_BG);  // Dimmed colon
    tft.drawString(":", startX + hourWidth, 15);
  }

  // Minutes and am/pm
  tft.setTextColor(timeColor, COLOR_BG);
  tft.drawString(minStr, startX + hourWidth + colonWidth, 15);
}

// Get UV Index description
String getUVDescription(float uvi) {
  if (uvi < 3) return "Low";
  if (uvi < 6) return "Moderate";
  if (uvi < 8) return "High";
  if (uvi < 11) return "Very High";
  return "Extreme";
}

// Get UV Index color
uint16_t getUVColor(float uvi) {
  if (uvi < 3) return COLOR_SUCCESS;      // Green - Low
  if (uvi < 6) return COLOR_SUN;          // Yellow - Moderate
  if (uvi < 8) return COLOR_ACCENT;       // Orange - High
  if (uvi < 11) return TFT_RED;           // Red - Very High
  return TFT_MAGENTA;                     // Purple - Extreme
}

void displayConditions() {
  tft.fillScreen(COLOR_BG);

  if (!weather.dataValid) {
    displayError("No Data!");
    return;
  }

  drawHeader();

  int y = 50;
  int lineHeight = 32;
  int labelX = 20;

  // UV Index
  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("UV Index", labelX, y);
  tft.setTextColor(getUVColor(weather.uvi), COLOR_BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(weather.uvi, 1) + " " + getUVDescription(weather.uvi), 310, y);

  // Visibility
  y += lineHeight;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Visibility", labelX, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MR_DATUM);
  float visKm = weather.visibility / 1000.0;
  tft.drawString(String(visKm, 1) + " km", 310, y);

  // Pressure
  y += lineHeight;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Pressure", labelX, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(weather.pressure) + " hPa", 310, y);

  // Dew Point
  y += lineHeight;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Dew Point", labelX, y);
  tft.setTextColor(getTempColor(weather.dewPoint), COLOR_BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(weather.dewPoint, 0) + "'", 310, y);

  // Cloud Cover
  y += lineHeight;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Cloud Cover", labelX, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(weather.clouds) + "%", 310, y);

  // Footer and screen indicator
  drawFooter();
  drawScreenIndicator();
}

void displayHourlyForecast() {
  tft.fillScreen(COLOR_BG);

  if (!weather.dataValid || weather.hourlyCount == 0) {
    displayError("No Hourly Data!");
    return;
  }

  drawHeader();

  // === NOW ROW ===
  // Weather icon on left
  drawWeatherIcon(weather.weatherCode, 45, 70, 55, !isDaytime());

  // Large temperature
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(7);
  tft.setTextColor(getTempColor(weather.temperature), COLOR_BG);
  tft.drawString(String(weather.temperature, 0), 85, 70);

  // Condition text (smaller font if too long)
  tft.setTextFont(weather.condition.length() > 12 ? 2 : 4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString(weather.condition, 160, 55);

  // Feels like
  tft.drawString("Feels " + String(weather.apparent_temp, 0), 160, 85);

  // === HOURLY COLUMNS (skip hour 0, show hours 1-7) ===
  int displayCount = min(7, weather.hourlyCount - 1);
  int margin = 8;
  int availableWidth = 320 - (margin * 2);
  int spacing = availableWidth / displayCount;
  int startY = 130;

  for (int i = 0; i < displayCount; i++) {
    int hourIndex = i + 1;  // Skip hour 0 (now)
    int x = margin + spacing / 2 + i * spacing;

    // Hour label - format as "2am" or "12pm"
    // Color based on whether that hour is during daylight
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    int h = weather.hourly[hourIndex].hour;

    // Check if this hour is during daylight
    struct tm sunriseTm, sunsetTm;
    localtime_r(&weather.sunrise, &sunriseTm);
    localtime_r(&weather.sunset, &sunsetTm);
    bool hourIsDaytime = (h >= sunriseTm.tm_hour && h <= sunsetTm.tm_hour);
    tft.setTextColor(hourIsDaytime ? COLOR_DAYTIME : COLOR_SUBTLE, COLOR_BG);

    String ampm = (h < 12) ? "am" : "pm";
    int displayH = h;
    if (displayH == 0) displayH = 12;
    else if (displayH > 12) displayH -= 12;
    String hourStr = String(displayH) + ampm;
    tft.drawString(hourStr, x, startY);

    // Weather icon (smaller) - show moon at night
    drawWeatherIcon(weather.hourly[hourIndex].weatherCode, x, startY + 27, 35, !hourIsDaytime);

    // Temperature - color coded
    tft.setTextFont(2);
    tft.setTextColor(getTempColor(weather.hourly[hourIndex].temperature), COLOR_BG);
    tft.drawString(String(weather.hourly[hourIndex].temperature, 0), x, startY + 55);
  }

  // Footer and screen indicator
  drawFooter();
  drawScreenIndicator();
}

void displayHourlyForecast2() {
  tft.fillScreen(COLOR_BG);

  if (!weather.dataValid || weather.hourlyCount < 14) {
    displayError("No Hourly Data!");
    return;
  }

  drawHeader();

  // === TOP SECTION: Rain chance for today ===
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Rain Today", 10, 55);

  // Rain percentage with droplet icon
  int rainX = 200;
  int rainY = 65;
  tft.fillCircle(rainX, rainY + 5, 12, COLOR_RAIN);
  tft.fillTriangle(rainX - 12, rainY + 5, rainX + 12, rainY + 5, rainX, rainY - 15, COLOR_RAIN);

  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(7);
  int rainChance = weather.daily[0].pop;
  tft.setTextColor(rainChance > 50 ? COLOR_RAIN : COLOR_TEXT, COLOR_BG);
  tft.drawString(String(rainChance) + "%", rainX + 25, 70);

  // === HOURLY COLUMNS (show hours 8-14) ===
  int displayCount = min(7, weather.hourlyCount - 8);
  int margin = 8;
  int availableWidth = 320 - (margin * 2);
  int spacing = availableWidth / displayCount;
  int startY = 130;

  for (int i = 0; i < displayCount; i++) {
    int hourIndex = i + 8;  // Start from hour 8
    int x = margin + spacing / 2 + i * spacing;

    // Hour label
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    int h = weather.hourly[hourIndex].hour;

    // Check if this hour is during daylight
    struct tm sunriseTm, sunsetTm;
    localtime_r(&weather.sunrise, &sunriseTm);
    localtime_r(&weather.sunset, &sunsetTm);
    bool hourIsDaytime = (h >= sunriseTm.tm_hour && h <= sunsetTm.tm_hour);
    tft.setTextColor(hourIsDaytime ? COLOR_DAYTIME : COLOR_SUBTLE, COLOR_BG);

    String ampm = (h < 12) ? "am" : "pm";
    int displayH = h;
    if (displayH == 0) displayH = 12;
    else if (displayH > 12) displayH -= 12;
    String hourStr = String(displayH) + ampm;
    tft.drawString(hourStr, x, startY);

    // Weather icon (smaller) - show moon at night
    drawWeatherIcon(weather.hourly[hourIndex].weatherCode, x, startY + 27, 35, !hourIsDaytime);

    // Temperature - color coded
    tft.setTextFont(2);
    tft.setTextColor(getTempColor(weather.hourly[hourIndex].temperature), COLOR_BG);
    tft.drawString(String(weather.hourly[hourIndex].temperature, 0), x, startY + 55);
  }

  // Footer and screen indicator
  drawFooter();
  drawScreenIndicator();
}

void displayDailyForecast() {
  tft.fillScreen(COLOR_BG);

  if (!weather.dataValid || weather.dailyCount == 0) {
    displayError("No Daily Data!");
    return;
  }

  drawHeader();

  // 4-day grid layout (2x2)
  int displayCount = min(4, weather.dailyCount);
  int cellW = 160;  // Half screen width
  int cellH = 80;   // Cell height (reduced to fit footer)
  int startY = 38;  // Below header

  for (int i = 0; i < displayCount; i++) {
    int col = i % 2;
    int row = i / 2;
    int cellX = col * cellW + cellW / 2;
    int cellY = startY + row * cellH;

    // Day name
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
    String dayLabel = (i == 0) ? "Today" : weather.daily[i].dayName;
    tft.drawString(dayLabel, cellX, cellY);

    // Weather icon
    drawWeatherIcon(weather.daily[i].weatherCode, cellX, cellY + 30, 35);

    // High / Low temps
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    String tempStr = String((int)weather.daily[i].tempMax) + " / " + String((int)weather.daily[i].tempMin);

    // Use color of the high temp
    tft.setTextColor(getTempColor(weather.daily[i].tempMax), COLOR_BG);
    tft.drawString(tempStr, cellX, cellY + 58);
  }

  // Footer and screen indicator
  drawFooter();
  drawScreenIndicator();
}

void displaySettings() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Settings", 10, 25);

  int y = 70;
  int lineHeight = 28;

  tft.setTextFont(2);

  // Location
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Location:", 20, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Aroona, QLD", 120, y);

  // Coordinates
  y += lineHeight;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Lat/Lon:", 20, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString(String(LATITUDE) + ", " + String(LONGITUDE), 120, y);

  // Update interval
  y += lineHeight;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Update:", 20, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Every 5 minutes", 120, y);

  // WiFi status
  y += lineHeight;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("WiFi:", 20, y);
  tft.setTextColor(WiFi.status() == WL_CONNECTED ? COLOR_SUCCESS : TFT_RED, COLOR_BG);
  tft.drawString(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected", 120, y);

  // IP Address
  y += lineHeight;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("IP:", 20, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString(WiFi.localIP().toString(), 120, y);

  // Last update
  y += lineHeight;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Updated:", 20, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString(lastUpdateTime, 120, y);

  // Screen indicator
  drawScreenIndicator();
}

// Display a specific screen
void displayScreen(Screen screen) {
  switch (screen) {
    case SCREEN_HOURLY: displayHourlyForecast(); break;
    case SCREEN_HOURLY2: displayHourlyForecast2(); break;
    case SCREEN_CONDITIONS: displayConditions(); break;
    case SCREEN_DAILY: displayDailyForecast(); break;
    case SCREEN_SETTINGS: displaySettings(); break;
    case SCREEN_ABOUT: displayAbout(); break;
    case SCREEN_DEMO: displayDemo(); break;
    case SCREEN_DEMO2: displayDemo2(); break;
    case SCREEN_DEMO3: displayDemo3(); break;
  }
}

// Screen transition (no animation)
void swipeTransition(Screen from, Screen to) {
  tft.fillScreen(COLOR_BG);
  displayScreen(to);
}

// About page
void displayAbout() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("About", 10, 18);

  int y = 55;
  int lineHeight = 22;

  tft.setTextFont(4);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Weather Reporter", 10, y);

  y += lineHeight + 15;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Created by", 10, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Adrian", 140, y);

  y += lineHeight + 5;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("with help from", 10, y);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Claude AI", 165, y);

  y += lineHeight + 15;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Powered by OpenWeatherMap", 10, y);

  drawScreenIndicator();
}

// Demo page showing all weather icons
void displayDemo() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Weather Icons", 160, 12);

  tft.setTextFont(1);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);

  // Row 1: Day icons - Sun, Few Clouds, Cloudy, Overcast
  int y1 = 50;
  drawWeatherIcon(800, 40, y1, 40);   // Clear/Sun
  tft.drawString("Clear", 40, y1 + 26);

  drawWeatherIcon(801, 120, y1, 40);  // Few clouds day
  tft.drawString("Few Cld", 120, y1 + 26);

  drawWeatherIcon(802, 200, y1, 40);  // Cloudy
  tft.drawString("Cloudy", 200, y1 + 26);

  drawWeatherIcon(804, 280, y1, 40);  // Overcast
  tft.drawString("Overcast", 280, y1 + 26);

  // Row 2: Night + weather - Moon, Few Clouds Night, Rain, Storm
  int y2 = 105;
  drawWeatherIcon(800, 40, y2, 40, true);   // Clear/Moon
  tft.drawString("Night", 40, y2 + 26);

  drawWeatherIcon(801, 120, y2, 40, true);  // Few clouds night
  tft.drawString("Night Cld", 120, y2 + 26);

  drawWeatherIcon(500, 200, y2, 40);   // Rain
  tft.drawString("Rain", 200, y2 + 26);

  drawWeatherIcon(200, 280, y2, 40);  // Thunderstorm
  tft.drawString("Storm", 280, y2 + 26);

  // Row 3: More weather - Snow, Mist, Wind arrows
  int y3 = 160;
  drawWeatherIcon(600, 40, y3, 40);  // Snow
  tft.drawString("Snow", 40, y3 + 26);

  drawWeatherIcon(701, 120, y3, 40);  // Mist
  tft.drawString("Mist", 120, y3 + 26);

  // Wind arrows
  tft.setTextFont(2);
  tft.drawString("Wind:", 185, y3 - 10);
  drawWindArrow(230, y3, 0, 15);    // N
  drawWindArrow(260, y3, 90, 15);   // E
  drawWindArrow(290, y3, 180, 15);  // S

  // Row 4: Temp color samples
  int y4 = 210;
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Temp:", 40, y4);
  tft.setTextColor(getTempColor(15), COLOR_BG);
  tft.drawString("15", 90, y4);
  tft.setTextColor(getTempColor(25), COLOR_BG);
  tft.drawString("25", 120, y4);
  tft.setTextColor(getTempColor(38), COLOR_BG);
  tft.drawString("38", 150, y4);

  // Time color samples
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Time:", 190, y4);
  tft.setTextColor(COLOR_DAYTIME, COLOR_BG);
  tft.drawString("Day", 240, y4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Night", 280, y4);

  drawScreenIndicator();
}

// Demo page 2 - Design elements showcase
void displayDemo2() {
  tft.fillScreen(COLOR_BG);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);

  // === SHAPES SECTION ===
  tft.drawString("Shapes", 10, 5);

  // Rectangles
  tft.drawRect(10, 18, 30, 20, COLOR_ACCENT);           // Outline
  tft.fillRect(45, 18, 30, 20, COLOR_ACCENT);           // Filled

  // Rounded rectangles
  tft.drawRoundRect(80, 18, 30, 20, 5, COLOR_SUCCESS);  // Outline
  tft.fillRoundRect(115, 18, 30, 20, 5, COLOR_SUCCESS); // Filled

  // Circles
  tft.drawCircle(160, 28, 10, COLOR_RAIN);              // Outline
  tft.fillCircle(185, 28, 10, COLOR_RAIN);              // Filled

  // Triangles
  tft.drawTriangle(210, 38, 220, 18, 230, 38, COLOR_SUN);         // Outline
  tft.fillTriangle(240, 38, 250, 18, 260, 38, COLOR_SUN);         // Filled

  // === LINES SECTION ===
  tft.drawString("Lines", 10, 48);
  for (int i = 1; i <= 5; i++) {
    int x = 10 + (i - 1) * 30;
    // Draw thick line by drawing multiple parallel lines
    for (int t = 0; t < i; t++) {
      tft.drawLine(x, 60 + t, x + 20, 70 + t, COLOR_TEXT);
    }
  }

  // Dotted/dashed effect
  for (int x = 170; x < 250; x += 6) {
    tft.drawLine(x, 65, x + 3, 65, COLOR_SUBTLE);
  }

  // === GRADIENT BARS ===
  tft.drawString("Gradients", 10, 82);

  // Temperature gradient (blue -> white -> orange)
  for (int i = 0; i < 140; i++) {
    float temp = 10 + (i * 30.0 / 140);  // 10 to 40 degrees
    tft.drawLine(10 + i, 95, 10 + i, 110, getTempColor(temp));
  }
  tft.setTextFont(1);
  tft.drawString("10C", 10, 113);
  tft.drawString("40C", 130, 113);

  // Custom RGB gradient
  for (int i = 0; i < 140; i++) {
    uint8_t r = (i < 70) ? 0 : (i - 70) * 255 / 70;
    uint8_t g = (i < 70) ? i * 255 / 70 : 255 - (i - 70) * 255 / 70;
    uint8_t b = (i < 70) ? 255 - i * 255 / 70 : 0;
    tft.drawLine(170 + i, 95, 170 + i, 110, tft.color565(r, g, b));
  }

  // === PROGRESS BARS ===
  tft.drawString("Progress", 10, 128);

  // Simple progress bar
  int progress = 70;  // 70%
  tft.drawRect(10, 140, 100, 12, COLOR_SUBTLE);
  tft.fillRect(11, 141, progress - 2, 10, COLOR_SUCCESS);

  // Segmented bar
  for (int i = 0; i < 10; i++) {
    uint16_t col = (i < 7) ? COLOR_ACCENT : COLOR_CLOUD_DARK;
    tft.fillRect(120 + i * 12, 140, 10, 12, col);
  }

  // === ARCS / GAUGE ===
  tft.drawString("Gauge", 10, 160);

  // Simple arc gauge
  int cx = 60, cy = 200;
  int radius = 30;
  // Draw arc segments
  for (int angle = 180; angle <= 360; angle += 5) {
    float rad = angle * 0.0174533;
    int x1 = cx + cos(rad) * (radius - 5);
    int y1 = cy + sin(rad) * (radius - 5);
    int x2 = cx + cos(rad) * radius;
    int y2 = cy + sin(rad) * radius;
    uint16_t col = (angle < 270) ? COLOR_SUCCESS : ((angle < 330) ? COLOR_SUN : COLOR_ACCENT);
    tft.drawLine(x1, y1, x2, y2, col);
  }
  // Needle
  float needleAngle = 290 * 0.0174533;
  tft.drawLine(cx, cy, cx + cos(needleAngle) * 22, cy + sin(needleAngle) * 22, COLOR_TEXT);
  tft.fillCircle(cx, cy, 4, COLOR_SUBTLE);

  // === COLOR PALETTE ===
  tft.drawString("Palette", 160, 160);
  int px = 160, py = 175;
  int boxSize = 18;
  uint16_t colors[] = {COLOR_TEXT, COLOR_SUBTLE, COLOR_ACCENT, COLOR_SUN, COLOR_SUCCESS, COLOR_RAIN, COLOR_MOON, COLOR_CLOUD};
  for (int i = 0; i < 8; i++) {
    int col = i % 4;
    int row = i / 4;
    tft.fillRect(px + col * (boxSize + 2), py + row * (boxSize + 2), boxSize, boxSize, colors[i]);
  }

  drawScreenIndicator();
}

// Demo page 3 - Typography showcase
void displayDemo3() {
  tft.fillScreen(COLOR_BG);

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(TL_DATUM);

  // === BUILT-IN FONTS ===
  int y = 5;

  tft.setTextFont(1);
  tft.drawString("Font 1: The quick brown fox (8px)", 5, y);
  y += 12;

  tft.setTextFont(2);
  tft.drawString("Font 2: Quick brown fox (16px)", 5, y);
  y += 20;

  tft.setTextFont(4);
  tft.drawString("Font 4: Brown fox (26px)", 5, y);
  y += 30;

  // === NUMERIC FONTS ===
  tft.setTextFont(1);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Numeric fonts:", 5, y);
  y += 12;

  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  // Font 6 - large numbers
  tft.setTextFont(6);
  tft.drawString("6:", 5, y);
  tft.drawString("123", 30, y);

  // Font 7 - 7-segment style
  tft.setTextFont(7);
  tft.drawString("7:", 110, y);
  tft.drawString("45", 135, y);

  // Font 8 - very large numbers
  tft.setTextFont(8);
  tft.drawString("89", 220, y);

  y += 55;

  // === TEXT SCALING ===
  tft.setTextFont(1);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Font 2 scaled:", 5, y);
  y += 12;

  tft.setTextFont(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  tft.setTextSize(1);
  tft.drawString("1x", 5, y);

  tft.setTextSize(2);
  tft.drawString("2x", 40, y);

  tft.setTextSize(3);
  tft.drawString("3x", 100, y);

  tft.setTextSize(1);  // Reset

  y += 50;

  // === TEXT DATUMS ===
  tft.setTextFont(1);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Alignment (datum):", 5, y);
  y += 15;

  tft.setTextFont(2);
  int lineY = y + 10;

  // Draw reference line
  tft.drawLine(5, lineY, 315, lineY, COLOR_CLOUD_DARK);

  // Show different datums
  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("TL", 10, lineY);
  tft.fillCircle(10, lineY, 2, COLOR_SUCCESS);

  tft.setTextDatum(TC_DATUM);
  tft.drawString("TC", 80, lineY);
  tft.fillCircle(80, lineY, 2, COLOR_SUCCESS);

  tft.setTextDatum(ML_DATUM);
  tft.drawString("ML", 140, lineY);
  tft.fillCircle(140, lineY, 2, COLOR_SUCCESS);

  tft.setTextDatum(MC_DATUM);
  tft.drawString("MC", 200, lineY);
  tft.fillCircle(200, lineY, 2, COLOR_SUCCESS);

  tft.setTextDatum(MR_DATUM);
  tft.drawString("MR", 260, lineY);
  tft.fillCircle(260, lineY, 2, COLOR_SUCCESS);

  tft.setTextDatum(BL_DATUM);
  tft.drawString("BL", 300, lineY);
  tft.fillCircle(300, lineY, 2, COLOR_SUCCESS);

  tft.setTextDatum(TL_DATUM);  // Reset

  drawScreenIndicator();
}

void bootAnimation() {
  int centerX = 160;
  int centerY = 100;

  // Sun rising animation
  for (int r = 0; r < 50; r += 4) {
    tft.fillCircle(centerX, centerY, r, COLOR_SUN);
    if (r > 15) {
      for (int angle = 0; angle < 360; angle += 45) {
        float rad = angle * 0.0174533;
        int x1 = centerX + cos(rad) * (r + 5);
        int y1 = centerY + sin(rad) * (r + 5);
        int x2 = centerX + cos(rad) * (r + 15);
        int y2 = centerY + sin(rad) * (r + 15);
        tft.drawLine(x1, y1, x2, y2, COLOR_SUN);
      }
    }
    delay(20);
  }

  delay(300);

  // Fade out
  for (int r = 0; r < 160; r += 6) {
    tft.drawCircle(centerX, centerY, r, COLOR_BG);
    tft.drawCircle(centerX, centerY, r + 1, COLOR_BG);
    tft.drawCircle(centerX, centerY, r + 2, COLOR_BG);
    delay(8);
  }

  tft.fillScreen(COLOR_BG);

  // Title reveal
  String title = "Weather Reporter";
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(6);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);

  for (int i = 1; i <= (int)title.length(); i++) {
    tft.fillRect(0, 75, 320, 55, COLOR_BG);
    tft.drawString(title.substring(0, i), centerX, 100);
    delay(40);
  }

  delay(200);

  tft.setTextFont(4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  for (int x = 320; x >= centerX; x -= 10) {
    tft.fillRect(0, 130, 320, 30, COLOR_BG);
    tft.drawString("Aroona, QLD", x, 145);
    delay(8);
  }

  delay(400);

  // Loading dots
  tft.setTextFont(4);
  tft.setTextColor(COLOR_SUBTLE, COLOR_BG);
  tft.drawString("Loading", centerX, 190);

  for (int i = 0; i < 3; i++) {
    for (int dot = 0; dot < 3; dot++) {
      tft.fillCircle(130 + (dot * 20), 215, 5, (dot <= i) ? COLOR_SUBTLE : COLOR_BG);
    }
    delay(250);
  }

  delay(200);
}
