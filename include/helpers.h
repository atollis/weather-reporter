/*
 * Helper Functions for Weather Display
 *
 * Utility functions for temperature colors, compass directions,
 * UV index handling, time formatting, and text wrapping.
 */

#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "types.h"

// External references to globals defined in main.cpp
extern TFT_eSPI tft;
extern TFT_eSprite sprite;
extern WeatherData weather;

// Color constants (must match main.cpp)
#define COLOR_BG 0x0000
#define COLOR_TEXT 0xD69A
#define COLOR_SUBTLE 0x8410
#define COLOR_SUN 0xFE60
#define COLOR_SUCCESS 0x3666
#define COLOR_ACCENT 0xFD20

// Convert wind degrees to compass direction
inline String degToCompass(int deg)
{
  const char *dirs[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int index = ((deg + 11) / 22) % 16;
  return String(dirs[index]);
}

// Check if current time is between sunrise and sunset
inline bool isDaytime()
{
  time_t now = time(NULL);
  if (now < 1000000000)
    return true; // Default to day if time not synced
  return (now >= weather.sunrise && now < weather.sunset);
}

// Format time_t to readable string (e.g., "06:45 am")
inline String formatWeatherTime(time_t rawTime)
{
  struct tm *timeinfo = localtime(&rawTime);
  char buffer[10];
  strftime(buffer, sizeof(buffer), "%I:%M %p", timeinfo);
  return String(buffer);
}

// Draw text with word wrapping
inline void drawWrappedString(String text, int x, int y, int maxWidth)
{
  int currentY = y;
  String word = "";
  String line = "";

  for (unsigned int i = 0; i < text.length(); i++)
  {
    char c = text[i];
    if (c == ' ')
    {
      // Check if adding the word exceeds maxWidth
      if (sprite.textWidth(line + word) > maxWidth)
      {
        sprite.drawString(line, x, currentY);
        line = word + " ";
        currentY += sprite.fontHeight();
      }
      else
      {
        line += word + " ";
      }
      word = "";
    }
    else
    {
      word += c;
    }
  }
  // Draw the final leftover text
  sprite.drawString(line + word, x, currentY);
}

// Get color based on temperature: blue (cold) -> white (neutral) -> orange (hot)
inline uint16_t getTempColor(float temp)
{
  if (temp <= 15)
  {
    return TFT_BLUE;
  }
  else if (temp >= 40)
  {
    return COLOR_ACCENT;
  }
  else if (temp >= 24 && temp <= 26)
  {
    return COLOR_TEXT;
  }
  else if (temp < 24)
  {
    // Gradient from blue (15) to white (24)
    float ratio = (temp - 15.0) / 9.0;
    int r = ratio * 255;
    int g = ratio * 255;
    int b = 255;
    return sprite.color565(r, g, b);
  }
  else
  {
    // Gradient from white (26) to orange (40)
    float ratio = (temp - 26.0) / 14.0;
    int r = 255;
    int g = 255 - (ratio * 155);
    int b = 255 - (ratio * 255);
    return sprite.color565(r, g, b);
  }
}

// Get UV Index description
inline String getUVDescription(float uvi)
{
  if (uvi < 3)
    return "Low";
  if (uvi < 6)
    return "Moderate";
  if (uvi < 8)
    return "High";
  if (uvi < 11)
    return "Very High";
  return "Extreme";
}

// Get UV Index color
inline uint16_t getUVColor(float uvi)
{
  if (uvi < 3)
    return COLOR_SUCCESS;
  if (uvi < 6)
    return COLOR_SUN;
  if (uvi < 8)
    return COLOR_ACCENT;
  if (uvi < 11)
    return TFT_RED;
  return TFT_MAGENTA;
}

// Get ordinal suffix for day number (1st, 2nd, 3rd, etc.)
inline const char *getOrdinalSuffix(int day)
{
  if (day >= 11 && day <= 13)
    return "th";
  switch (day % 10)
  {
  case 1:
    return "st";
  case 2:
    return "nd";
  case 3:
    return "rd";
  default:
    return "th";
  }
}

#endif // HELPERS_H
