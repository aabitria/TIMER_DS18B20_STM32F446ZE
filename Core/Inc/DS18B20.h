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


void DS18B20_Init (void);
void DS18B20_Generate_Reset (void);
void DS18B20_Send_Cmd (uint16_t *cmd, uint16_t len);
void DS18B20_Read_Temp (void);

#endif /* INC_DS18B20_H_ */
