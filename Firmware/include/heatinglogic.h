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
  float p1_out, p1_flow; 
  float p2_out, p2_flow; 
  float p3_out, p3_flow; 
  float p4_out, p4_flow; 
  
  float hysteresis;      
};

class HeatingLogic {
  private:
    bool _isSummerMode = false;
    float _smoothedTemp = -999.0; 
    unsigned long _lastUpdate = 0;

  public:
    HeatingConfig config;

    HeatingLogic() {
      // Defaults
      config.active = false; 
      config.linearSlope = 1.2;
      config.baseTemp = 20.0;
      config.linearOffset = 0.0; // Default: no offset
      config.exponent = 1.0;
      
      config.tMin = 25.0; 
      config.tMax = 75.0;
      config.hysteresis = 2.0; 
      
      // Viessmann-like curve
      config.p1_out = 18.0; config.p1_flow = 20.0; 
      config.p2_out = 10.0; config.p2_flow = 38.0; 
      config.p3_out = 0.0;  config.p3_flow = 50.0; 
      config.p4_out = -15.0; config.p4_flow = 65.0; 
    }

    float interpolate(float x, float x1, float y1, float x2, float y2) {
      if (abs(x2 - x1) < 0.001) return y1;
      return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
    }

    void updateOutdoorTemp(float currentRaw) {
      if (currentRaw < -50 || currentRaw > 60) return;
      
      // Rate limit: update only every 10 seconds for inertia
      if (millis() - _lastUpdate < 10000 && _smoothedTemp > -900) return;
      _lastUpdate = millis();

      if (_smoothedTemp <= -900.0) {
        _smoothedTemp = currentRaw; 
      } else {
        float alpha = 0.1; // Inertia
        _smoothedTemp = (alpha * currentRaw) + ((1.0 - alpha) * _smoothedTemp);
      }
    }

    float getCalculatedSetpoint() {
      if (_smoothedTemp <= -900.0) return config.tMin;

      float calcTemp = _smoothedTemp;
      float target = config.tMin;

      // 1. Summer/winter (global, also for linear if desired,
      // but usually linear already uses tMin. Keep it active for 4-point mode.)
      if (config.active) {
        float limitOff = config.p1_out + (config.hysteresis / 2.0);
        float limitOn = config.p1_out - (config.hysteresis / 2.0);
        
        if (_isSummerMode && calcTemp < limitOn) _isSummerMode = false;
        else if (!_isSummerMode && calcTemp > limitOff) _isSummerMode = true;
        
        if (_isSummerMode) return 0.0; 
      }

      // 2. Curve calculation
      if (!config.active) {
        // --- LINEAR ---
        // Formula: base + slope*(base - outdoor)^(1/exponent) + offset
        float delta = config.baseTemp - calcTemp;
        float shaped = delta;
        if (delta > 0.0f && config.exponent > 0.0f) {
          shaped = powf(delta, 1.0f / config.exponent);
        }
        target = config.baseTemp + config.linearSlope * shaped + config.linearOffset;
      } else {
        // --- 4-PUNKT ---
        if (calcTemp >= config.p1_out) target = config.p1_flow; 
        else if (calcTemp <= config.p4_out) target = config.p4_flow;
        else if (calcTemp > config.p2_out) target = interpolate(calcTemp, config.p1_out, config.p1_flow, config.p2_out, config.p2_flow);
        else if (calcTemp > config.p3_out) target = interpolate(calcTemp, config.p2_out, config.p2_flow, config.p3_out, config.p3_flow);
        else target = interpolate(calcTemp, config.p3_out, config.p3_flow, config.p4_out, config.p4_flow);
      }

      // 3. Limits
      if (target < config.tMin) target = config.tMin;
      if (target > config.tMax) target = config.tMax;
      
      return target;
    }
    
    float getSmoothedTemp() { return _smoothedTemp; }
    bool isSummer() { return _isSummerMode; }
};

#endif