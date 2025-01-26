/*
 * DS18B20.c
 *
 *  Created on: Jan 26, 2025
 *      Author: Lenovo310
 */
#include "DS18B20.h"


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

	// Enable both Input and PWM channels
	TIM1->CCER |= (1 << 4) | (1 << 8);
	TIM1->EGR |= (1 << 0);
	TIM1->BDTR |= (1 << 15);
	TIM1->CR1 |= (1 << 0);
}


static void DS18B20_DMA_Init (void)
{
	DMA2_Stream6->CR &= ~(1 << 0);

	// set minc, pinc, periphaddr, cndtr, normal mode, channel 6, TC interrupt
    uint32_t setmask =   (6 << 25)          // DMA2 stream6 channel 6
                       | (1 << 16)          // medium priority
                       | (1 << 13)          // mem size 16-bit
                       | (1 << 11)          // periph size 16-bit
                       | (1 << 10)          // mem addr incremented
                       | (1 << 6)           // mem to periph direction
                       | (1 << 4);			// TC int enabled
    uint32_t clearmask = (1 << 9)           // periph address not incremented
                       | (1 << 8);          // normal, not circular mode

	MODIFY_REG(DMA2_Stream6->CR, clearmask, setmask);

	// Update DMA enabled; CC3 DMA enabled
//	TIM1->DIER &= ~((1 << 8) | (1 << 11));
	TIM1->DIER |= ((1 << 8) | (1 << 11));

	// Capture/Compare DMA select - when update event occurs
//	TIM1->CR2 &= ~(1 << 3);
	TIM1->CR2 |= (1 << 3);

	DMA2_Stream6->NDTR = 0;
	DMA2_Stream6->PAR = (uint32_t)&TIM1->CCR3;
	DMA2_Stream6->M0AR = 0;
}

void DS18B20_Init (void)
{
	DS18B20_TIM_Init_Override();
	DS18B20_DMA_Init();
}

void DS18B20_Generate_Reset (void)
{
	TIM1->CR1 &= ~(1 << 0);
	TIM1->ARR = 999;
	TIM1->CCR3 = 500;

	// Enable both Input and PWM channels
	//TIM1->CCER |= (1 << 4) | (1 << 8);
	TIM1->EGR |= (1 << 0);
	//TIM1->BDTR |= (1 << 15);
	TIM1->CR1 |= (1 << 0);

	TIM1->CCR3 = 0;
}

void DS18B20_Send_Cmd (uint16_t *cmd, uint16_t len)
{
	// disable timer1
	TIM1->CR1 &= ~(1 << 0);

	// set arr, ccr
	TIM1->ARR = 79;
	TIM1->EGR |= (1 << 0);

	// set dma regs
	DMA2_Stream6->CR &= ~(1 << 0);

	DMA2_Stream6->NDTR = len;			// should be 9
	DMA2_Stream6->M0AR = (uint32_t)cmd;

//	// Update DMA enabled; CC3 DMA enabled
//	TIM1->DIER &= ~((1 << 8) | (1 << 11));
//	TIM1->DIER |= ((1 << 8) | (1 << 11));
//
//	// Capture/Compare DMA select - when update event occurs
//	TIM1->CR2 &= ~(1 << 3);
//	TIM1->CR2 |= (1 << 3);

	// Enable
	DMA2_Stream6->CR |= (1 << 0);
	TIM1->CR1 |= (1 << 0);
}

void DS18B20_TC_Interrupt_Handler (void)
{
	uint32_t Hisr = READ_REG(DMA2->HISR);

	if ((Hisr & (1 << 21)) != 0)
	{
	    DMA2->HIFCR |= (1 << 21);
	}
}

