#include "scheduler.h"

void Scheduler::setConfig(JsonObject obj) {
    entries.clear();
    
    JsonArray schArr = obj[F("entries")].as<JsonArray>();
    for (JsonObject schObj : schArr) {
        SchedulerEntry entry;
        
        entry.days = 0;
        String daysStr = schObj[F("days")].as<String>();
        while (!daysStr.isEmpty()) {
            char d = daysStr.charAt(0);
            entry.days |= 1<<(d - '0'); // bitmask for days, bit 0 = Sunday, bit 6 = Saturday
            daysStr.remove(0, 1);
        }
        
        entry.temp = schObj[F("temp")] | 20.0;

        String timeStr = schObj[F("time")].as<String>(); // format HH:MM
        int hours = atoi(timeStr.substring(0, 2).c_str()); // get hours
        int minutes = atoi(timeStr.substring(3, 5).c_str()); // get minutes
        entry.time = hours * 60 + minutes;

        entries.push_back(entry);
    }
    active = obj[F("enabled")] | false;
    lastIdx = -1;
}

int8_t Scheduler::getCurrentIdx() const {
    if (!active || entries.empty())
        return -1;

    struct tm timeinfo;
    int8_t result = -1;
    if (!getLocalTime(&timeinfo, 0)) {
        return lastIdx;
    }

    for (int8_t back=0; back < 7; back++) {
        uint8_t day = (timeinfo.tm_wday - back + 7) % 7;
        uint16_t thresh = (back == 0) ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : 24*60; // for current day use current time as threshold, for previous days use 24:00
        int16_t bestMins = -1;
        for (auto &entry: entries) {
            if ( (entry.days & (1 << day)) == 0)
                continue; // entry not active on this day
            if ( (entry.time <= thresh) && (entry.time > bestMins)) {
                bestMins = entry.time;
                result = &entry - &entries[0]; // index of entry
            }
        }
        if (result > -1)
            break;
    }

    return result;
}

bool Scheduler::getSetpoint(double &temp) {
    int8_t idx = getCurrentIdx();

    if (idx != lastIdx) {
        lastIdx = idx;
        
        if (idx > -1) {
            temp = entries[idx].temp;
            return true;
        }
    }
    return false;
}