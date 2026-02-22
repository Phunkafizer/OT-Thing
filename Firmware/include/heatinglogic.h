#ifndef HEATING_LOGIC_H
#define HEATING_LOGIC_H

#include <Arduino.h>

struct HeatingConfig {
  bool active;        // True = 4-point, false = linear
  float linearSlope;  // Slope
  float baseTemp;     // Base room temp (pivot)
  float linearOffset; // NEW: vertical shift (level)
  float exponent;     // Radiator exponent for curve shape
  
  float tMin;         // Global minimum temperature
  float tMax;         // Global maximum temperature

  // 4-point definition
  struct CurvePoint {
    float out;
    float flow;
  } points[4];
  
  float hysteresis;      
};

class HeatingLogic {
  private:
    bool _isSummerMode = false;
    float _smoothedTemp = -999.0; 
    unsigned long _lastUpdate = 0;

  public:
    HeatingConfig config;

    HeatingLogic();

    float interpolate(float x, float x1, float y1, float x2, float y2);
    void updateOutdoorTemp(float currentRaw);
    float getCalculatedSetpoint();
    float getSmoothedTemp();
    bool isSummer();
};

#endif