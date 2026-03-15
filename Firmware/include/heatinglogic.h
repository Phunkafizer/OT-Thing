#ifndef HEATING_LOGIC_H
#define HEATING_LOGIC_H

#include <Arduino.h>

struct HeatingConfig {
  bool chOn;
  bool active;        // True = 6-point, false = linear
  bool curveSmooth;   // Smooth 6-point curve (Catmull-Rom)
  double linearSlope;  // Slope
  double baseTemp;     // Base room temp (pivot)
  double linearOffset; // NEW: vertical shift (level)
  double exponent;     // Radiator exponent for curve shape
  
  double tMin;         // Global minimum temperature
  double tMax;         // Global maximum temperature
  bool minSuspend;     // Suspend if flow below tMin
  double suspOffset;   // Offset for room-setpoint suspend

  // 6-point definition
  struct CurvePoint {
    double out;
    double flow;
  } points[6];

  double flow; // Default flow temperature
  bool enableHyst;
  double hysteresis;
  struct {
    bool enabled;
    double p;
    double i;
    double boost;
  } roomComp;
};

class HeatingLogic {
  private:
    bool _isSummerMode = false;
    double _smoothedTemp = -999.0; 
    unsigned long _lastUpdate = 0;

    double catmullRom(double t, double p0, double p1, double p2, double p3);

  public:
    HeatingConfig config;

    HeatingLogic();

    double interpolate(double x, double x1, double y1, double x2, double y2);
    void updateOutdoorTemp(double currentRaw);
    double getCalculatedSetpoint();
    double getSmoothedTemp();
    bool isSummer();
};

#endif