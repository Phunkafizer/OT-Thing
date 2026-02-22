#include "heatinglogic.h"
#include <math.h>

HeatingLogic::HeatingLogic() {
  // Defaults
  config.chOn = false;
  config.active = false;
  config.linearSlope = 1.2;
  config.baseTemp = 20.0;
  config.linearOffset = 0.0; // Default: no offset
  config.exponent = 1.0;

  config.tMin = 25.0;
  config.tMax = 75.0;
  config.flow = 35.0;
  config.enableHyst = false;
  config.hysteresis = 2.0;
  config.roomComp.enabled = false;
  config.roomComp.p = 0.0;
  config.roomComp.i = 0.0;
  config.roomComp.boost = 3.0;

  // Viessmann-like curve
  config.points[0] = {18.0, 20.0};
  config.points[1] = {10.0, 38.0};
  config.points[2] = {0.0, 50.0};
  config.points[3] = {-15.0, 65.0};
}

double HeatingLogic::interpolate(double x, double x1, double y1, double x2, double y2) {
  if (fabs(x2 - x1) < 0.001) return y1;
  return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

void HeatingLogic::updateOutdoorTemp(double currentRaw) {
  if (currentRaw < -50 || currentRaw > 60) return;

  // Rate limit: update only every 10 seconds for inertia
  if (millis() - _lastUpdate < 10000 && _smoothedTemp > -900) return;
  _lastUpdate = millis();

  if (_smoothedTemp <= -900.0) {
    _smoothedTemp = currentRaw;
  } else {
    double alpha = 0.1; // Inertia
    _smoothedTemp = (alpha * currentRaw) + ((1.0 - alpha) * _smoothedTemp);
  }
}

double HeatingLogic::getCalculatedSetpoint() {
  if (_smoothedTemp <= -900.0) return config.tMin;

  double calcTemp = _smoothedTemp;
  double target = config.tMin;

  // 1. Summer/winter (global, also for linear if desired,
  // but usually linear already uses tMin. Keep it active for 4-point mode.)
  if (config.active) {
    double limitOff = config.points[0].out + (config.hysteresis / 2.0);
    double limitOn = config.points[0].out - (config.hysteresis / 2.0);

    if (_isSummerMode && calcTemp < limitOn) _isSummerMode = false;
    else if (!_isSummerMode && calcTemp > limitOff) _isSummerMode = true;

    if (_isSummerMode) return 0.0;
  }

  // 2. Curve calculation
  if (!config.active) {
    // --- LINEAR ---
    // Formula: base + slope*(base - outdoor)^(1/exponent) + offset
    double delta = config.baseTemp - calcTemp;
    double shaped = delta;
    if (delta > 0.0 && config.exponent > 0.0) {
      shaped = pow(delta, 1.0 / config.exponent);
    }
    target = config.baseTemp + config.linearSlope * shaped + config.linearOffset;
  } else {
    // --- 4-POINT ---
    const auto &p1 = config.points[0];
    const auto &p2 = config.points[1];
    const auto &p3 = config.points[2];
    const auto &p4 = config.points[3];

    if (calcTemp >= p1.out) target = p1.flow;
    else if (calcTemp <= p4.out) target = p4.flow;
    else if (calcTemp > p2.out) target = interpolate(calcTemp, p1.out, p1.flow, p2.out, p2.flow);
    else if (calcTemp > p3.out) target = interpolate(calcTemp, p2.out, p2.flow, p3.out, p3.flow);
    else target = interpolate(calcTemp, p3.out, p3.flow, p4.out, p4.flow);
  }

  // 3. Limits
  if (target < config.tMin) target = config.tMin;
  if (target > config.tMax) target = config.tMax;

  return target;
}

double HeatingLogic::getSmoothedTemp() {
  return _smoothedTemp;
}

bool HeatingLogic::isSummer() {
  return _isSummerMode;
}
