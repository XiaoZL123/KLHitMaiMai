

#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

void I2C1_Init(void);

#ifdef __cplusplus
}
#endif

#endif
