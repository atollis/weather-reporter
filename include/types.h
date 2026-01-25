/*
 * Type Definitions for Weather Display
 *
 * Shared struct definitions used across the project.
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <time.h>

// Hourly forecast data
struct HourlyData
{
  float temperature;
  int weatherCode;
  int hour;
};

// Daily forecast data
struct DailyData
{
  float tempMin;
  float tempMax;
  int weatherCode;
  String dayName;
  int pop;        // Probability of precipitation (0-100%)
  String summary; // Human-readable weather summary
};

// Main weather data structure
struct WeatherData
{
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
  int visibility; // meters
  int pressure;   // hPa
  float dewPoint;
  int clouds; // percentage
  // Moon data (from daily[0])
  time_t moonrise;
  time_t moonset;
  float moonPhase; // 0-1
};

#endif // TYPES_H
