#ifndef HEATING_LOGIC_H
#define HEATING_LOGIC_H

#include <Arduino.h>

struct HeatingConfig {
  bool active;        // True = 4-Punkt, False = Linear
  float linearSlope;  // Steigung
  float baseTemp;     // Basis Raumtemp (Drehpunkt)
  float linearOffset; // NEU: Parallelverschiebung (Niveau)
  float exponent;     // Heizkoerper-Exponent fuer Kurvenform
  
  float tMin;         // Globale Minimaltemperatur
  float tMax;         // Globale Maximaltemperatur

  // 4-Punkt Definition
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
      config.linearOffset = 0.0; // Standard: Kein Offset
      config.exponent = 1.0;
      
      config.tMin = 25.0; 
      config.tMax = 75.0;
      config.hysteresis = 2.0; 
      
      // Viessmann-ähnliche Kurve
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
      
      // Zeitbremse: Nur alle 10 Sekunden updaten für Trägheit
      if (millis() - _lastUpdate < 10000 && _smoothedTemp > -900) return;
      _lastUpdate = millis();

      if (_smoothedTemp <= -900.0) {
        _smoothedTemp = currentRaw; 
      } else {
        float alpha = 0.1; // Trägheit
        _smoothedTemp = (alpha * currentRaw) + ((1.0 - alpha) * _smoothedTemp);
      }
    }

    float getCalculatedSetpoint() {
      if (_smoothedTemp <= -900.0) return config.tMin;

      float calcTemp = _smoothedTemp;
      float target = config.tMin;

      // 1. Sommer/Winter (gilt global, auch für Linear wenn gewünscht, 
      // aber meistens ist das in Linear einfach durch tMin abgedeckt. 
      // Wir lassen es hier für den 4-Punkt Modus aktiv)
      if (config.active) {
        float limitOff = config.p1_out + (config.hysteresis / 2.0);
        float limitOn = config.p1_out - (config.hysteresis / 2.0);
        
        if (_isSummerMode && calcTemp < limitOn) _isSummerMode = false;
        else if (!_isSummerMode && calcTemp > limitOff) _isSummerMode = true;
        
        if (_isSummerMode) return 0.0; 
      }

      // 2. Kurven-Berechnung
      if (!config.active) {
        // --- LINEAR ---
        // Formel: Basis + Steigung*(Basis - Außen)^(1/exponent) + OFFSET
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