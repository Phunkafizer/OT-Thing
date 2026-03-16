#pragma once
#include <vector>
#include <ArduinoJson.h>

class HeatingCurve {
public:
    double getFlowTemp(const double outsideTemp, const double roomSet) const;
    void setConfig(JsonObject &config);
    double getFlowMax() const;
private:
    struct CurvePoint {
        double outside;
        double flow;
    };
    enum CurveType {
        CURVE_SIMPLE = 0,
        CURVE_MULTIPOINT = 1
    } curveType;

    double flowMax;
    double exponent;
    double gradient;
    double offset;
    bool smooth;
    std::vector<CurvePoint> points; // must be sorted by outsideTemp, highest outsideTemp first!

    double getFlowTempSimple(const double outsideTemp, const double roomSet) const;
    double getFlowTempMultipoint(const double outsideTemp, const double roomSet) const;
};