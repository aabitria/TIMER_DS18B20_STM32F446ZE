/*
 * DS18B20.c
 *
 *  Created on: Jan 26, 2025
 *      Author: Lenovo310
 */
#include "DS18B20.h"
#include <string.h>

#define _0                              (70U)
#define _1                              (10U)
#define _IDLE                           (0U)

/* STM32 ARR and CCRx standard values */
#define DS18B20_ARR_RESET               (999U)
#define DS18B20_CCR_RESET               (499U)
#define DS18B20_ARR_RW                  (79U)
#define DS18B20_CCR_READ_TIME           (10U)


// LSB first
uint16_t Cmd_ConvertT_44h[9] = {_0, _0, _1, _0, _0, _0, _1, _0, _IDLE};
uint16_t Cmd_ReadSCr_BEh[9]  = {_0, _1, _1, _1, _1, _1, _0, _1, _IDLE};
uint16_t Cmd_SkipRom_CCh[9]  = {_0, _0, _1, _1, _0, _0, _1, _1, _IDLE};


uint16_t Input[72] = {0};
float TempC;

union DS18B20_Union
{
	uint64_t bits;
	struct Bytes
	{
		uint8_t Temp_Lsb;
		uint8_t Temp_Msb;
		uint8_t Th_Reg;
		uint8_t Tl_Reg;
		uint8_t Cfg;
		uint8_t Rsvd1;
		uint8_t Rsvd2;
		uint8_t Rsvd3;
	} DS18B20_Bytes;
};

union DS18B20_Union DS18B20_Data;
uint8_t DS18B20_Crc = {0};

static void DS18B20_Update_Data (void)
{
	uint16_t i;

	memset(&DS18B20_Data, 0, sizeof(DS18B20_Data));

	for (i = 0; i < 64; i++)
	{
		if (Input[i] < 15)
		{
			DS18B20_Data.bits |= (1 << i);
		}
	}

	DS18B20_Crc = 0;
	for (i = 64; i < 72; i++)
	{
		if (Input[i] < 15)
		{
			DS18B20_Crc |= (1 << (i - 64));
		}
	}
}


static void DS18B20_TIM_Init_Override (void)
{
    // Set ARR Preload Enable
	SET_BIT(TIM1->CR1, TIM_CR1_ARPE);

	// Make this active HIGH so at the 5V line it's inverted (by the Q)
	TIM1->CCER |= TIM_CCER_CC3P;

	TIM1->CCMR1 |= TIM_CCMR1_CC2S_0;

	TIM1->CR1 &= ~TIM_CR1_CEN;
	TIM1->ARR = 999;
	TIM1->CCR3 = 0;

	// Enable PWM channels; input capture enable only on reset and read.
	TIM1->CCER |= TIM_CCER_CC3E;
	TIM1->EGR |= TIM_EGR_UG;
	TIM1->BDTR |= TIM_BDTR_MOE;
	TIM1->CR1 |= TIM_CR1_CEN;
}


static void DS18B20_DMA_Init (void)
{
	DMA2_Stream6->CR &= ~(1 << 0);

	// removal of the above has something to do with no interrupt after dma receive.
	// We forgot to set TC interrupt
	SET_BIT(DMA2_Stream6->CR, DMA_SxCR_TCIE);

	SET_BIT(DMA2_Stream2->CR, DMA_SxCR_TCIE);

	// Update DMA enabled; CC3 DMA enabled
	TIM1->DIER |= TIM_DIER_CC3DE | TIM_DIER_CC2DE;

	DMA2_Stream6->NDTR = 0;
	DMA2_Stream6->PAR = (uint32_t)&TIM1->CCR3;
	DMA2_Stream6->M0AR = 0;

	DMA2_Stream2->NDTR = 0;
	DMA2_Stream2->PAR = (uint32_t)&TIM1->CCR2;
	DMA2_Stream2->M0AR = 0;
}

void DS18B20_Init (void)
{
	DS18B20_TIM_Init_Override();
	DS18B20_DMA_Init();
}

uint8_t Flag = 0;
uint16_t ccr2 = 0;

void DS18B20_Generate_Reset (void)
{
	// clear flag in SR.
	TIM1->SR &= ~(TIM_SR_CC2IF | TIM_SR_CC2OF);
	TIM1->DIER &= ~TIM_DIER_CC2IE;
	TIM1->DIER |= TIM_DIER_CC2IE;

	TIM1->CR1 &= ~TIM_CR1_CEN;
	TIM1->ARR = DS18B20_ARR_RESET;
	TIM1->CCR3 = DS18B20_CCR_RESET;

	// Enable capture
	TIM1->CCER |= TIM_CCER_CC2E;

	Flag = 1;

	TIM1->EGR |= TIM_EGR_UG;
    // Fire!
	TIM1->CR1 |= TIM_CR1_CEN;

	// This will take effect after the next update event.
	TIM1->CCR3 = _IDLE;
	TIM1->ARR = 79;

	// We must read the CCR2 reg to see if DS18 responded.
	// if value is 500 then no response; should be higher.
	// This is to be done by input capture TIM1 CH2.  No
	// need for DMA.  Just 1 interrupt should be enough.

	// Blocking portion - handle in RTOS'es
	while (Flag == 1);
	ccr2 = TIM1->CCR2;

}

void DS18B20_Send_Cmd (uint16_t *cmd, uint16_t len)
{
	// disable timer1
	TIM1->CR1 &= ~TIM_CR1_CEN;

	// set arr, ccr
	TIM1->ARR = DS18B20_ARR_RW;

	// Disable capture
	TIM1->CCER &= ~TIM_CCER_CC2E;
	TIM1->EGR |= TIM_EGR_UG;

	// set dma regs
	DMA2_Stream6->CR &= ~DMA_SxCR_EN;

	DMA2_Stream6->NDTR = len;
	DMA2_Stream6->M0AR = (uint32_t)cmd;

	// Update DMA enabled; CC3 DMA enabled
	TIM1->DIER &= ~(TIM_DIER_UDE | TIM_DIER_CC3DE);
	TIM1->DIER |= TIM_DIER_CC3DE;

	// Capture/Compare DMA select - when CCR event occurs
	TIM1->CR2 &= ~TIM_CR2_CCDS;

	Flag = 1;

	// Enable
	DMA2_Stream6->CR |= DMA_SxCR_EN;
	TIM1->CR1 |= TIM_CR1_CEN;

	while (Flag == 1);

	// Idle
	TIM1->CCR3 = _IDLE;
}

void DS18B20_Receive (uint16_t *buffer, uint16_t len)
{
	// disable timer1
	TIM1->CR1 &= ~TIM_CR1_CEN;

	// Disable timer interrupt; only DMA interrupt allowed.
	TIM1->DIER &= ~TIM_DIER_CC2IE;

	// Enable capture
	TIM1->CCER |= TIM_CCER_CC2E;

	// set dma regs
	DMA2_Stream2->CR &= ~DMA_SxCR_EN;

	// set arr, ccr
	TIM1->ARR = DS18B20_ARR_RW;
	TIM1->CCR3 = DS18B20_CCR_READ_TIME;			// PWM still need to send 10 us pulse, then wait
	TIM1->EGR |= TIM_EGR_UG;


	DMA2_Stream2->NDTR = len;			// no idle bit/8 bits per byte, so we're reading 9B
	DMA2_Stream2->M0AR = (uint32_t)buffer;

	// Update DMA disabled; CC2 DMA enabled
	TIM1->DIER &= ~(TIM_DIER_UDE | TIM_DIER_CC2DE);
	TIM1->DIER |= TIM_DIER_CC2DE;

	// Capture/Compare DMA select - when CCR event occurs
	TIM1->CR2 &= ~TIM_CR2_CCDS;

	Flag = 1;

	// Enable
	DMA2_Stream2->CR |= DMA_SxCR_EN;
	TIM1->CR1 |= TIM_CR1_CEN;

	while (Flag == 1);

	TIM1->CCR3 = _IDLE;

	return;
}

void DS18B20_PWM_TC_Interrupt_Handler (void)
{
	uint32_t Hisr = READ_REG(DMA2->HISR);

	if ((Hisr & DMA_HISR_TCIF6) != 0)
	{
	    DMA2->HIFCR |= DMA_HIFCR_CTCIF6;
	    Flag = 0;

	}
}


void DS18B20_IC_TC_Interrupt_Handler (void)
{
	uint32_t Lisr = READ_REG(DMA2->LISR);

	if ((Lisr & DMA_LISR_TCIF2) != 0)
	{
	    DMA2->LIFCR |= DMA_LIFCR_CTCIF2;
	    Flag = 0;

	    if (TIM1->SR & TIM_SR_CC2OF)
	    {
	    	TIM1->SR &= ~(TIM_SR_CC2IF | TIM_SR_CC2OF);
	    }

	}
}

void DS18B20_IC_Interrupt_Handler (void)
{
	// 1st capture event is still present, we just ignore it...
    if (TIM1->SR & TIM_SR_CC2OF)
    {
    	TIM1->SR &= ~(TIM_SR_CC2IF | TIM_SR_CC2OF);
    	Flag = 0;
    	//ccr2 = TIM1->CCR2;
    	return;
    }

    return;
}


void DS18B20_Read_Temp (void)
{
	//Reset, skiprom, converT, reset, skiprom, readT and receive incoming bits
	DS18B20_Generate_Reset();
	HAL_Delay(1);
	DS18B20_Send_Cmd(Cmd_SkipRom_CCh, 9);
	HAL_Delay(1);
	DS18B20_Send_Cmd(Cmd_ConvertT_44h, 9);
	HAL_Delay(1);

	DS18B20_Generate_Reset();
	HAL_Delay(1);
	DS18B20_Send_Cmd(Cmd_SkipRom_CCh, 9);
	HAL_Delay(1);
	DS18B20_Send_Cmd(Cmd_ReadSCr_BEh, 9);
	HAL_Delay(1);
	DS18B20_Receive(Input, 72);
	HAL_Delay(1);

	DS18B20_Update_Data();

	uint16_t Temp = (DS18B20_Data.DS18B20_Bytes.Temp_Msb << 8) | (DS18B20_Data.DS18B20_Bytes.Temp_Lsb);
	TempC = (float)Temp / 16.0f;

	return;
}
