# Weather Reporter

An ESP32-based weather display featuring a color TFT screen showing current conditions, hourly and daily forecasts from OpenWeatherMap.

## Features

- Current weather with large temperature display and conditions
- 7-hour forecast with icons
- 4-day forecast grid
- Detailed conditions (UV index, visibility, pressure, dew point, cloud cover)
- Custom weather icons (sun, moon, clouds, rain, storms, snow, mist)
- Temperature color-coding (blue to orange gradient)
- Day/night aware display
- Multiple demo screens showcasing design elements and typography

## Hardware

- **MCU:** ESP32 DOIT DevKit V1
- **Display:** ST7789 TFT 320x240
- **Buttons:** 3x momentary switches

### Wiring

| TFT Pin | ESP32 GPIO |
|---------|------------|
| SCL | 18 |
| SDA | 23 |
| DC | 27 |
| RST | 4 |
| CS | 5 |
| VCC | 3.3V |
| GND | GND |

| Button | ESP32 GPIO |
|--------|------------|
| LEFT | 13 |
| RIGHT | 12 |
| SELECT | 14 |

Buttons connect to GND (using internal pull-ups).

## Setup

1. **Install PlatformIO**

2. **Configure WiFi credentials**

   Create `include/credentials.h`:
   ```cpp
   #define WIFI_SSID "YourNetworkName"
   #define WIFI_PASSWORD "YourPassword"
   ```

3. **Get OpenWeatherMap API key**

   Sign up at [openweathermap.org](https://openweathermap.org/api) and subscribe to the One Call API 3.0.

   Update the API key in `src/main.cpp`:
   ```cpp
   const char* OWM_API_KEY = "your_api_key_here";
   ```

4. **Set your location**

   Update coordinates in `src/main.cpp`:
   ```cpp
   const char* LATITUDE = "-26.7984";
   const char* LONGITUDE = "153.1394";
   ```

## Build & Upload

```bash
pio run -t upload
```

## Controls

| Action | Function |
|--------|----------|
| LEFT/RIGHT | Navigate between screens |
| SELECT (short press) | Toggle display on/off |
| SELECT (long press) | Switch between Weather and Settings modes |

## Screens

### Weather Mode (3 screens)
1. **Hourly** - Current weather + 7-hour forecast
2. **Conditions** - UV, visibility, pressure, dew point, clouds
3. **Daily** - 4-day forecast

### Settings Mode (5 screens)
1. **Settings** - Location, WiFi status, IP address
2. **About** - Project credits
3. **Demo** - Weather icons showcase
4. **Demo 2** - Shapes, gradients, progress bars
5. **Demo 3** - Typography and fonts

## Dependencies

- TFT_eSPI
- ArduinoJson

## Configuration

Weather updates every 5 minutes. Time is synced via NTP (configured for UTC+10 Brisbane).

## Credits

Created by Adrian with assistance from Claude AI.
Weather data from OpenWeatherMap.
