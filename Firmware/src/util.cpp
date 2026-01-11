#include "util.h"

SemHelper::SemHelper(SemaphoreHandle_t &mtx, const uint16_t timeout):
        mtx(mtx) {

    result = xSemaphoreTake(mtx, (TickType_t) timeout / portTICK_PERIOD_MS);
}

SemHelper::~SemHelper() {
    if (*this) {
        xSemaphoreGive(mtx);
    }
}

SemHelper::operator bool() {
    return result == pdTRUE;
}