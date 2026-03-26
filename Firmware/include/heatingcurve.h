#pragma once
#include <vector>
#include <ArduinoJson.h>

class HeatingCurve {
public:
    double getFlowTemp(const double roomSet) const;
    void setConfig(JsonObject &config);
    double getFlowMax() const;
    double getReturnLimit(const double roomSet) const;
private:
    struct CurvePoint {
        double outside;
        double flow;
    };
    enum CurveMode {
        CURVE_SIMPLE = 0,
        CURVE_MULTIPOINT = 1
    } curveMode;

    double flowMax;
    double exponent;
    double gradient;
    double offset;
    struct {
        double deltaT;
    } retLimit;
    
    std::vector<CurvePoint> points; // must be sorted by outsideTemp, highest outsideTemp first!

    double getFlowTempSimple(const double outsideTemp, const double roomSet) const;
    double getFlowTempMultipoint(const double outsideTemp, const double roomSet) const;
};