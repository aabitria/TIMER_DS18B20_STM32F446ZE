/*
 * DS18B20.c
 *
 *  Created on: Jan 26, 2025
 *      Author: Lenovo310
 */
#include "DS18B20.h"

#define READ_TIME_CCR                   (10)

// LSB first
uint16_t Cmd_ConvertT_44h[9] = {_0, _0, _1, _0, _0, _0, _1, _0, _IDLE};
uint16_t Cmd_ReadSCr_BEh[9]  = {_0, _1, _1, _1, _1, _1, _0, _1, _IDLE};
uint16_t Cmd_SkipRom_CCh[9]  = {_0, _0, _1, _1, _0, _0, _1, _1, _IDLE};


uint16_t Input[72] = {0};

static void DS18B20_TIM_Init_Override (void)
{
    // Set ARR Preload Enable
	SET_BIT(TIM1->CR1, TIM_CR1_ARPE);

	// Make this active HIGH so at the 5V line it's inverted (by the Q)
	TIM1->CCER |= 1 << 9;

	TIM1->CCMR1 |= 1 << 8;

	TIM1->CR1 &= ~(1 << 0);
	TIM1->ARR = 999;
	TIM1->CCR3 = 0;

	// Enable PWM channels; input cpature enable will depend on function.
	TIM1->CCER |= (1 << 8);
	TIM1->EGR |= (1 << 0);
	TIM1->BDTR |= (1 << 15);
	TIM1->CR1 |= (1 << 0);
}


static void DS18B20_DMA_Init (void)
{
	uint32_t setmask, clearmask;

	DMA2_Stream6->CR &= ~(1 << 0);

	// Set up DMA2_Stream6 for PWM/Send
	// set minc, pinc, periphaddr, cndtr, normal mode, channel 6, TC interrupt
    setmask =    (6 << 25)          // DMA2 stream6 channel 6
			   | (1 << 16)          // medium priority
			   | (1 << 13)          // mem size 16-bit
			   | (1 << 11)          // periph size 16-bit
			   | (1 << 10)          // mem addr incremented
			   | (1 << 6)           // mem to periph direction
			   | (1 << 4);			// TC int enabled
    clearmask =  (1 << 9)           // periph address not incremented
			   | (1 << 8);          // normal, not circular mode

	MODIFY_REG(DMA2_Stream6->CR, clearmask, setmask);

	// Set up DMA2_Stream2 for IC/Recv
	// set minc, pinc, periphaddr, cndtr, normal mode, channel 6, TC interrupt
//    setmask =    (6 << 25)          // DMA2 stream2 channel 6, TIM1 CH2
//			   | (1 << 16)          // medium priority
//			   | (1 << 13)          // mem size 16-bit
//			   | (1 << 11)          // periph size 16-bit
//			   | (1 << 10)          // mem addr incremented
//			   | (1 << 6)           // mem to periph direction
//			   | (1 << 4);			// TC int enabled
//    clearmask =  (1 << 9)           // periph address not incremented
//			   | (1 << 8)           // normal, not circular mode
//			   | (1 << 6);          // periph to memory direction
//
//	MODIFY_REG(DMA2_Stream2->CR, clearmask, setmask);
	SET_BIT(DMA2_Stream2->CR, 1 << 4);

	// Update DMA enabled; CC3 DMA enabled
//	TIM1->DIER &= ~((1 << 8) | (1 << 11));
	TIM1->DIER |= (1 << 8) | (1 << 11) | (1 << 10);

	// Capture/Compare DMA select - when update event occurs
//	TIM1->CR2 &= ~(1 << 3);
	TIM1->CR2 |= (1 << 3);

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
	TIM1->SR &= ~((1 << 2) | (1 << 10));
	TIM1->DIER &= ~(1 << 2);
	TIM1->DIER |= (1 << 2);

	TIM1->CR1 &= ~(1 << 0);
	TIM1->ARR = 999;
	TIM1->CCR3 = 500;

	// Enable capture
	TIM1->CCER |= (1 << 4);

	Flag = 1;

	TIM1->EGR |= (1 << 0);
    // Fire!
	TIM1->CR1 |= (1 << 0);

	TIM1->CCR3 = 0;
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
	TIM1->CR1 &= ~(1 << 0);

	// set arr, ccr
	TIM1->ARR = 79;

	// Disable capture
	TIM1->CCER &= ~(1 << 4);
	TIM1->EGR |= (1 << 0);

	// set dma regs
	DMA2_Stream6->CR &= ~(1 << 0);

	DMA2_Stream6->NDTR = len;			// should be 9
	DMA2_Stream6->M0AR = (uint32_t)cmd;

	// Update DMA enabled; CC3 DMA enabled
	TIM1->DIER &= ~((1 << 8) | (1 << 11));
	TIM1->DIER |= ((1 << 8) | (1 << 11));

	// Capture/Compare DMA select - when update event occurs
	TIM1->CR2 &= ~(1 << 3);
	TIM1->CR2 |= (1 << 3);

	Flag = 1;

	// Enable
	DMA2_Stream6->CR |= (1 << 0);
	TIM1->CR1 |= (1 << 0);

	TIM1->CCR3 = 0;

	while (Flag == 1);

}

void DS18B20_Receive (uint16_t *buffer, uint16_t len)
{
	// disable timer1
	TIM1->CR1 &= ~(1 << 0);

	// Disable timer interrupt; only DMA interrupt allowed.
	TIM1->DIER &= ~(1 << 2);

	// Enable capture
	TIM1->CCER |= (1 << 4);

	// set dma regs
	DMA2_Stream2->CR &= ~(1 << 0);

	// set arr, ccr
	TIM1->ARR = 79;
	TIM1->CCR3 = READ_TIME_CCR;			// PWM still need to send 10 us pulse, then wait
	TIM1->EGR |= (1 << 0);


	DMA2_Stream2->NDTR = 72;			// no idle bit/8 bits per byte, so we're reading 9B
	DMA2_Stream2->M0AR = (uint32_t)buffer;

	Flag = 1;

	// Enable
	DMA2_Stream2->CR |= (1 << 0);
	TIM1->CR1 |= (1 << 0);

	while (Flag == 1);

	return;
}

void DS18B20_PWM_TC_Interrupt_Handler (void)
{
	uint32_t Hisr = READ_REG(DMA2->HISR);

	if ((Hisr & (1 << 21)) != 0)
	{
	    DMA2->HIFCR |= (1 << 21);
	    Flag = 0;
	}
}


void DS18B20_IC_TC_Interrupt_Handler (void)
{
	uint32_t Lisr = READ_REG(DMA2->LISR);

	if ((Lisr & (1 << 21)) != 0)
	{
	    DMA2->LIFCR |= (1 << 21);
	    Flag = 0;

	    if (TIM1->SR & (1 << 10))
	    {
	    	TIM1->SR &= ~((1 << 2) | (1 << 10));
	    }
	}
}

void DS18B20_IC_Interrupt_Handler (void)
{
//    if (TIM1->SR & (1 << 2))
//    {
//    	TIM1->SR &= ~(1 << 2);
//    	Flag = 0;
//    	//ccr2 = TIM1->CCR2;
//    }

	// 1st capture event is still present, we just ignore it...
    if (TIM1->SR & (1 << 10))
    {
    	TIM1->SR &= ~((1 << 2) | (1 << 10));
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

	return;
}
