#include "heatinglogic.h"
#include <math.h>

HeatingLogic::HeatingLogic() {
  // Defaults
  config.chOn = false;
  config.active = false;
  config.curveSmooth = false;
  config.linearSlope = 1.2;
  config.baseTemp = 20.0;
  config.linearOffset = 0.0; // Default: no offset
  config.exponent = 1.0;

  config.tMin = 10.0;
  config.tMax = 60.0;
  config.minSuspend = false;
  config.suspOffset = 0.0;
  config.flow = 45.0;
  config.enableHyst = false;
  config.hysteresis = 0.5;
  config.roomComp.enabled = false;
  config.roomComp.p = 1.5;
  config.roomComp.i = 0.5;
  config.roomComp.boost = 0.0;

  // Viessmann-like curve (6 points)
  config.points[0] = {20.0, 20.0};
  config.points[1] = {10.0, 38.0};
  config.points[2] = {0.0, 50.0};
  config.points[3] = {-10.0, 58.0};
  config.points[4] = {-20.0, 65.0};
  config.points[5] = {-30.0, 70.0};
}

double HeatingLogic::interpolate(double x, double x1, double y1, double x2, double y2) {
  if (fabs(x2 - x1) < 0.001) return y1;
  return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

double HeatingLogic::catmullRom(double t, double p0, double p1, double p2, double p3) {
  const double t2 = t * t;
  const double t3 = t2 * t;
  return 0.5 * (
    (2.0 * p1) +
    (-p0 + p2) * t +
    (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
    (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3
  );
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
    // --- 6-POINT ---
    const int numPoints = sizeof(config.points) / sizeof(config.points[0]);
    if (calcTemp >= config.points[0].out) {
      target = config.points[0].flow;
    } else if (calcTemp <= config.points[numPoints - 1].out) {
      target = config.points[numPoints - 1].flow;
    } else {
      for (int i = 0; i < numPoints - 1; i++) {
        if (calcTemp <= config.points[i].out && calcTemp > config.points[i + 1].out) {
          if (config.curveSmooth) {
            const double x1 = config.points[i].out;
            const double x2 = config.points[i + 1].out;
            const double t = (calcTemp - x1) / (x2 - x1);
            const double p0 = (i == 0) ? config.points[0].flow : config.points[i - 1].flow;
            const double p1 = config.points[i].flow;
            const double p2 = config.points[i + 1].flow;
            const double p3 = (i + 2 < numPoints) ? config.points[i + 2].flow : config.points[numPoints - 1].flow;
            const double smooth = catmullRom(t, p0, p1, p2, p3);
            const double lo = fmin(p1, p2);
            const double hi = fmax(p1, p2);
            target = fmin(fmax(smooth, lo), hi);
          } else {
            target = interpolate(
              calcTemp,
              config.points[i].out,
              config.points[i].flow,
              config.points[i + 1].out,
              config.points[i + 1].flow
            );
          }
          break;
        }
      }
    }
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
