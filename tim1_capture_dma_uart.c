// Example using Timer 1 capture to look up- and down- going events
// on PD2.  Using DMA to capture that event and write it to a circular
// queue that can be read out later.

#include "ch32fun.h"
#include <stdio.h>

#define BIT_W (0x137)
#define	HEAD1  ((samples_in_buffer - DMA_IN1->CNTR) & (samples_in_buffer-1))
#define	HEAD2  ((samples_in_buffer - DMA_IN2->CNTR) & (samples_in_buffer-1))

#define DEBUG 0
#define debug_printf(fmt, ...) \
            do { if (DEBUG) printf(fmt, __VA_ARGS__); } while (0)
uint16_t buffer1[256];
uint16_t buffer2[256];
uint16_t out_word = 0;
int out_bits = 0;

enum State_t { 	Idle,
								Data,
							};
enum State_t State = Idle;

void add_ones(int bits)
{
  debug_printf("Adding %d ones to 0x%04X with %d bits\n", bits, out_word, out_bits);
	if((bits + out_bits) > 8) 
	{
		if(out_bits != 0){
			// Last bits plus some idle
			for(int i = out_bits; i < 8; i++)
				out_word = (out_word >> 1) | 0x80;
			debug_printf("1 Received %04X\n", out_word);
			printf("%c", out_word);
			out_bits = 0;
			out_word = 0;
			State = Idle;
		}
	}else{
		out_bits += bits;
		for(int i = bits; i > 0; i--)
			out_word = (out_word >> 1) | 0x80;
	}
	if(out_bits == 8)
	{
		debug_printf("2 Received %04X\n", out_word);
		printf("%c", out_word);
		out_bits = 0;
		out_word = 0;
		State = Idle;
	}
}

void add_zeroes(int bits)
{
  debug_printf("Adding %d zeroes to 0x%04X with %d bits\n", bits, out_word, out_bits);
	if((bits + out_bits) > 8)
	{
		out_bits = 0;
		out_word = 0;
		State = Idle;
	}else{
		out_bits += bits;
		for(int i = bits; i > 0; i--)
			out_word = (out_word >> 1);
	}
	if(out_bits == 8)
	{
		debug_printf("3 Received %04X\n", out_word);
		printf("%c", out_word);
		out_bits = 0;
		out_word = 0;
		State = Idle;
	}
}

int main()
{
	SystemInit();
	funGpioInitAll();

	// Enable GPIOs
	RCC->APB2PCENR |= RCC_APB2Periph_TIM1 | RCC_APB2Periph_AFIO;
	RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;


//	for(int cnt=0; cnt < samples_in_buffer; cnt++){
//		buffer1[cnt]=0xdead;
//		buffer2[cnt]=0xbeef;
//	}

	#define DMA_IN1  DMA1_Channel2 // TIM1_CH1
	#define DMA_IN2  DMA1_Channel3 // TIM1_CH2

	int samples_in_buffer = sizeof(buffer1) / sizeof(buffer1[0]);

	// TIM1_CH1
	DMA_IN1->MADDR = (uint32_t)buffer1;
	DMA_IN1->PADDR = (uint32_t)&TIM1->CH2CVR; // Input
	DMA_IN1->CFGR = 
		0                 |                  // PERIPHERAL to MEMORY
		0                 |                  // Low priority.
		DMA_CFGR1_MSIZE_0 |                  // 16-bit memory
		DMA_CFGR1_PSIZE_0 |                  // 16-bit peripheral
		DMA_CFGR1_MINC    |                  // Increase memory.
		DMA_CFGR1_CIRC    |                  // Circular mode.
		0                 |                  // NO Half-trigger
		0                 |                  // NO Whole-trigger
		DMA_CFGR1_EN;                        // Enable
	DMA_IN1->CNTR = samples_in_buffer;

	// TIM1_UP
	DMA_IN2->MADDR = (uint32_t)buffer2;
	DMA_IN2->PADDR = (uint32_t)&TIM1->CH1CVR; // Input
	DMA_IN2->CFGR = 
		0                 |                  // PERIPHERAL to MEMORY
		0                 |                  // Low priority.
		DMA_CFGR1_MSIZE_0 |                  // 16-bit memory
		DMA_CFGR1_PSIZE_0 |                  // 16-bit peripheral
		DMA_CFGR1_MINC    |                  // Increase memory.
		DMA_CFGR1_CIRC    |                  // Circular mode.
		0                 |                  // NO Half-trigger
		0                 |                  // NO Whole-trigger
		DMA_CFGR1_EN;                        // Enable
	DMA_IN2->CNTR = samples_in_buffer;


	// Set the prescaler
	TIM1->PSC = 0x000f;

	// Set maximum cycle width
	TIM1->ATRLR = 0xffff;

	// TIM1 CC2 selector to 0b10: CC1 is input and IC1 is mapped to TI2
	//      CC1 selector to 0b01: CC1 is input and IC1 is mapped to TI1
//	TIM1->CHCTLR1 = TIM_CC1S_0 | TIM_CC2S_1; 
	TIM1->CHCTLR1 = TIM_CC1S_1 | TIM_CC2S_0;

	// Enable TIM1 CC1 and CC2 and set CC1 to trigger on falling edge
	TIM1->CCER = TIM_CC2P;
//	TIM1->CCER = TIM_CC1P | TIM_CC2P;
	
	// initialize counter
	TIM1->SWEVGR = TIM_UG;

	// Setup slave mode for tim1 input to go to tim2.
//	TIM1->SMCFGR = TIM_TS_0 | TIM_TS_2 | TIM_SMS_2;
	TIM1->SMCFGR = TIM_TS_1 | TIM_TS_2 | TIM_SMS_2;

	// Enable TIM1 and autoreload
	TIM1->CTLR1 = TIM_ARPE | TIM_CEN ;
	
	// Enable TIM1 DMA for CC1 and CC2. Enable trigger and update DMA events
	TIM1->DMAINTENR = TIM_TDE | TIM_CC1DE | TIM_CC2DE | TIM_UDE;
	//
	// Enable T1C1 and T1C2
	TIM1->CCER |= TIM_CC1E | TIM_CC2E;

	int tail1 = 0, tail2 = 1;
	int last_low=0;
	int bits;

	while(1)
	{
//		if( DMA_IN1->CNTR <= 245 ) {
//			for(int cnt=0; cnt < 20; cnt++){
//				debug_printf( "%02d: %04X %04X %02d %02d\n", cnt, buffer1[cnt], buffer2[cnt], ((buffer1[cnt] + (BIT_W / 2)) / BIT_W), ((buffer2[cnt] + (BIT_W / 2)) / BIT_W) );
//			}
//			while(1) {}
//		}

		while( HEAD1 == tail1) { }
		if ( HEAD1 != tail1 )
		{
//			debug_printf( "tail1: %05d, CNTR1: %03ld, CNTR2: %03ld, ", tail1, DMA_IN1->CNTR, DMA_IN2->CNTR );
			debug_printf( "tail1: %05d, HEAD1: %03ld, HEAD2: %03ld, ", tail1, HEAD1, HEAD2 );
			debug_printf( "HIGH %08X\n", buffer1[tail1] - last_low );
					
			bits = (((buffer1[tail1] - last_low) + (BIT_W / 2)) / BIT_W);
			debug_printf( "Ones: %08X\n", bits);
			if(State != Idle){
				add_ones(bits);
			}else{
				if(DEBUG)printf("Ignoring because Idle\n");
			}
			
			tail1 = (tail1+1)&(samples_in_buffer-1);
		}

		while (HEAD2 == tail2) { }
		if( HEAD2 != tail2 )
		{
//			debug_printf( "tail2: %05d, CNTR1: %03ld, CNTR2: %03ld, ", tail2, DMA_IN1->CNTR, DMA_IN2->CNTR );
			debug_printf( "tail2: %05d, HEAD1: %03ld, HEAD2: %03ld, ", tail2, HEAD1, HEAD2 );
			debug_printf( "LOW  %08X\n", buffer2[tail2] );
			last_low = buffer2[tail2];

			bits = ((buffer2[tail2] + (BIT_W / 2)) / BIT_W);
			debug_printf( "Zeroes: %08X\n", bits);
			if(State == Idle){
			  if(DEBUG)printf("Throw away a start bit.\n");
				State = Data;
				if(bits > 1){
					add_zeroes(bits - 1);
				}
			}else{
				add_zeroes(bits);
			}

			tail2 = (tail2+1)&(samples_in_buffer-1);
		}
	}
}
