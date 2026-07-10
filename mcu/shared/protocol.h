#pragma once
#include "esp_log.h"

typedef struct __attribute__((packed))
{
	uint32_t timestamp;
    uint16_t x;
    uint16_t y;
} data_t;