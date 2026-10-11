

#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* gpio.c：初始化板级 GPIO，包括 PC2 状态输出和 PD4/PD5 按键输入。 */
/* 初始化板级 GPIO 和按键输入。 */
void GPIO_Init(void);

#ifdef __cplusplus
}
#endif
#endif
