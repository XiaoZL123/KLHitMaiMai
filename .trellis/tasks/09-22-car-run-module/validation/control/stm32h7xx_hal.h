#ifndef CONTROL_HOST_HAL_H
#define CONTROL_HOST_HAL_H

#include <stddef.h>
#include <stdint.h>

typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint8_t unused; } FDCAN_HandleTypeDef;
typedef struct { uint8_t unused; } FDCAN_RxHeaderTypeDef;

#endif
