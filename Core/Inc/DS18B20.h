/*
 * DS18B20.h
 *
 *  Created on: Jan 26, 2025
 *      Author: Lenovo310
 */

#ifndef INC_DS18B20_H_
#define INC_DS18B20_H_

#include "tim.h"

#define _0              (70U)
#define _1              (10U)
#define _IDLE           (0U)

// LSB first
static uint16_t Cmd_ConvertT_44h[9] = {_0, _0, _1, _0, _0, _0, _1, _0, _IDLE};
static uint16_t Cmd_ReadSCr_BEh[9]  = {_0, _1, _1, _1, _1, _1, _0, _1, _IDLE};
static uint16_t Cmd_SkipRom_CCh[9]  = {_0, _0, _1, _1, _0, _0, _1, _1, _IDLE};


void DS18B20_Init (void);
void DS18B20_Generate_Reset (void);
void DS18B20_Send_Cmd (uint16_t *cmd, uint16_t len);


#endif /* INC_DS18B20_H_ */
