#include "heatingcurve.h"
#include <algorithm>
#include <math.h>

void HeatingCurve::setConfig(JsonObject &hpObj) {
    flowMax = hpObj[F("flowMax")] | 40;
    exponent = hpObj[F("exponent")] | 1.0;
    gradient = hpObj[F("gradient")] | 1.0;
    offset = hpObj[F("offset")] | 0.0;
    smooth = hpObj[F("curveSmooth")] | false;

    // Use ArduinoJson default operator to ensure a valid curve type when missing.
    curveType = static_cast<CurveType>(hpObj[F("type")] | static_cast<int>(CURVE_SIMPLE));

    points.clear();
    JsonArray pointsArr = hpObj[F("points")].as<JsonArray>();
    for (JsonObject pointObj: pointsArr) {
        CurvePoint pt;
        pt.outside = pointObj[F("outside")];
        pt.flow = pointObj[F("flow")];
        points.push_back(pt);
    }

    // Keep points sorted to ensure deterministic interpolation behavior.
    std::sort(points.begin(), points.end(), [](const CurvePoint &a, const CurvePoint &b) {
        return a.outside > b.outside;
    });

    // Clamp/normalize points from external config to avoid invalid curves.
    for (auto it=points.begin(); it<points.end(); it++) {
        if (it->flow > flowMax)
            it->flow = flowMax;

        if (it > points.begin()) {
            auto prev = it - 1;
            if (it->flow < prev->flow)
                it->flow = prev->flow;
        }
    }
}

double HeatingCurve::getFlowMax() const {
    return flowMax;
}

double HeatingCurve::getFlowTemp(const double outsideTemp, const double roomSet) const {
    switch (curveType) {
    case CURVE_SIMPLE:
        return getFlowTempSimple(outsideTemp, roomSet);
    case CURVE_MULTIPOINT:
        return getFlowTempMultipoint(outsideTemp, roomSet);
    }
    return 0;
}

double HeatingCurve::getFlowTempSimple(const double outsideTemp, const double roomSet) const {
	double minOutside = roomSet - (flowMax - roomSet) / gradient;
    double c1 = (flowMax - roomSet) / pow(roomSet - minOutside, 1.0 / exponent);
    return roomSet + c1 * pow(roomSet - outsideTemp, 1.0 / exponent) + offset;
}

double HeatingCurve::getFlowTempMultipoint(const double outsideTemp, const double roomSet) const {
    if (points.empty())
        return 0.0;

    if ((points.size() == 1) || (outsideTemp >= points[0].outside))
        return points[0].flow;

    auto it1 = points.begin();
    auto it2 = points.begin() + 1;

    // find points where given outsideTemp is between
    while (true) {
        if (outsideTemp >= it2->outside)
            break;

        it1++;
        it2++;

        if (it2 == points.end())
            return it1->flow;
    }

    if (smooth) {
        auto it0 = it1;
        if (it0 > points.begin())
            it0--;
        auto it3 = it2;
        if (it3 < points.end() - 1)
            it3++;

        // Catmull-Rom spline
        // Guard against duplicate outside values to avoid division by zero.
        const double denom = (it2->outside - it1->outside);
        if (fabs(denom) < 0.001)
            return it1->flow;

        const double t = (outsideTemp - it1->outside) / denom;
        const double t2 = t * t;
        const double t3 = t2 * t;

        const double flow = 0.5 * (
            (2.0 * it1->flow) +
            (-it0->flow + it2->flow) * t +
            (2.0 * it0->flow - 5.0 * it1->flow + 4.0 * it2->flow - it3->flow) * t2 +
            (-it0->flow + 3.0 * it1->flow - 3.0 * it2->flow + it3->flow) * t3
        );

        const double lo = fmin(it1->flow, it2->flow);
        const double hi = fmax(it1->flow, it2->flow);
        return fmin(fmax(flow, lo), hi);
    }
    else {
        // interpolate
        // Guard against duplicate outside values to avoid division by zero.
        const double denom = (it2->outside - it1->outside);
        if (fabs(denom) < 0.001)
            return it1->flow;

        const double ratio = (outsideTemp - it1->outside) / denom;
        return it1->flow + ratio * (it2->flow - it1->flow);
    }    
}