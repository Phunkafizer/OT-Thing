#pragma once
#include "ArduinoJson.h"

struct FlameStats {
    void loop();
    uint8_t getDuty() const;
    double getFreq() const;
    double getOnTime() const;
    double getOffTime() const;
    int getCurrentOnTime() const;
    void writeJson(JsonObject &obj) const;
private:
    static const uint8_t BUFSIZE_MINUTES = 180; // 3 h
    static const uint8_t BUFSIZE_CYCLES = 10;
    void update();
    void set(const bool flame);
    bool currentFlame {false};
    uint32_t lastEdge {0};
    uint32_t lastLoop {0};
    uint8_t idxMinutes {0};
    uint8_t idxCycles {0};
    bool onTimesInit {false};
    bool offTimesInit {false};
    template <typename T1, size_t T2>
    struct Ringbuf {
        void update(const uint8_t idx);
        T1 current {0};
        T1 buf[T2];
        uint32_t sum {0};
    };
    Ringbuf<uint8_t, BUFSIZE_MINUTES> on; // number of on minutes per minute
    Ringbuf<uint8_t, BUFSIZE_MINUTES> cycles; // number of cycles per minute
    Ringbuf<uint32_t, BUFSIZE_CYCLES> onTimes; // on time per cycle in seconds
    Ringbuf<uint32_t, BUFSIZE_CYCLES> offTimes; // on time per cycle in seconds
};

extern FlameStats flameStats;