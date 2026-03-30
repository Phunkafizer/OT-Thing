#include "flamestats.h"
#include "otcontrol.h"

FlameStats flameStats;

void FlameStats::set(const bool flame) {
    if (currentFlame != flame) {
        if (lastLoop == 0) {
            // this is first flame on within 1st minute
            memset(on.buf, 60, sizeof(on.buf));
            on.sum = BUFSIZE_MINUTES * 60UL;
        }

        if (flame) {
            cycles.current++;
            offTimes.current = (millis() - lastEdge) / 1000;
            if ((lastEdge > 0) && !offTimesInit) {
                // this is first flame on after 1st flame off, initialize off times buffer
                for (int i=0; i<BUFSIZE_CYCLES; i++)
                    offTimes.buf[i] = offTimes.current;
                offTimes.sum = offTimes.current * BUFSIZE_CYCLES;
                offTimesInit = true;
            }
            offTimes.update(idxCycles);
        }
        else {
            onTimes.current = (millis() - lastEdge) / 1000;
            if ((lastEdge > 0) && !onTimesInit) {
                // this is first flame off after 1st flame on, initialize on times buffer
                for (int i=0; i<BUFSIZE_CYCLES; i++)
                    onTimes.buf[i] = onTimes.current;
                onTimes.sum = onTimes.current * BUFSIZE_CYCLES;
                onTimesInit = true;
            }
            onTimes.update(idxCycles);
            idxCycles = (idxCycles + 1) % BUFSIZE_CYCLES;
        }
        
        update();
        lastEdge = millis();
        currentFlame = flame;
    }
}

void FlameStats::update() {
    if (currentFlame) {
        uint8_t diff = 0;
        if (lastEdge > lastLoop)
            diff = (millis() - lastEdge) / 1000;
        else
            diff = (millis() - lastLoop) / 1000;
        on.current += diff;
    }
}

uint8_t FlameStats::getDuty() const {
    return 100 * on.sum / BUFSIZE_MINUTES / 60;
}

double FlameStats::getFreq() const {
    return round(cycles.sum / (BUFSIZE_MINUTES / 60.0) * 10) / 10.0;
}

/*
 * @brief return current on-time / seconds, 0 if flame is off
 */
int FlameStats::getCurrentOnTime() const {
    if (!currentFlame)
        return 0;
    return (millis() - lastEdge) / 1000;
}

/* 
 * @brief return average on-time over last x on/off cycles / minutes
 */

double FlameStats::getOnTime() const {
    return round(onTimes.sum / BUFSIZE_CYCLES / 60.0 * 10) / 10.0;
}

/* 
 * @brief return average off-time over last x on/off cycles / minutes
 */

double FlameStats::getOffTime() const {
    return round(offTimes.sum / BUFSIZE_CYCLES / 60.0 * 10) / 10.0;
}

void FlameStats::loop() {
    set(otcontrol.getFlame());

    if (millis() >= lastLoop + 60000) {
        update();

        on.update(idxMinutes);
        cycles.update(idxMinutes);
        
        idxMinutes = (idxMinutes + 1) % BUFSIZE_MINUTES;
        lastLoop = millis();
    }
}

template <typename T1, size_t T2>
void FlameStats::Ringbuf<T1, T2>::update(const uint8_t idx) {
    sum -= buf[idx]; // remove old value
    buf[idx] = current;
    sum += current;
    current = 0;
}

void FlameStats::writeJson(JsonObject &obj) const {
    JsonObject fs = obj[F("flameStats")].to<JsonObject>();
    fs["duty"] = getDuty();
    fs["freq"] = getFreq();
    if (onTimesInit)
        fs["onTime"] = getOnTime();
    if (offTimesInit)
        fs["offTime"] = getOffTime();
}
