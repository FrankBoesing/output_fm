/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/* modified to output FM, Frank B., 2015  */

#include "output_fm.h"
#include "utility/pdb.h"

#if defined(__MK20DX256__)

DMAMEM static uint8_t fm_buffer[AUDIO_BLOCK_SAMPLES*2];
audio_block_t * AudioOutputFM::block_left_1st = NULL;
audio_block_t * AudioOutputFM::block_left_2nd = NULL;
bool AudioOutputFM::update_responsibility = false;
DMAChannel AudioOutputFM::dma(false);

void AudioOutputFM::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first
	// set the programmable delay block to trigger DMA requests
	if (!(SIM_SCGC6 & SIM_SCGC6_PDB)
	  || (PDB0_SC & PDB_CONFIG) != PDB_CONFIG
	  || PDB0_MOD != PDB_PERIOD
	  || PDB0_IDLY != 1
	  || PDB0_CH0C1 != 0x0101) {
		SIM_SCGC6 |= SIM_SCGC6_PDB;
		PDB0_IDLY = 1;
		PDB0_MOD = PDB_PERIOD;
		PDB0_SC = PDB_CONFIG | PDB_SC_LDOK;
		PDB0_SC = PDB_CONFIG | PDB_SC_SWTRIG;
		PDB0_CH0C1 = 0x0101;
	}

	dma.TCD->SADDR = fm_buffer;
	dma.TCD->SOFF = 1;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
	dma.TCD->NBYTES_MLNO = 1;
	dma.TCD->SLAST = -sizeof(fm_buffer);
	dma.TCD->DADDR = &OSC0_CR;
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = sizeof(fm_buffer) ;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = sizeof(fm_buffer);
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_PDB);
	update_responsibility = update_setup();
	dma.enable();
	dma.attachInterrupt(isr);
}


void AudioOutputFM::update(void)
{ 
	audio_block_t *block;
	block = receiveReadOnly(0); // input 0
	if (block) {
		__disable_irq();
		if (block_left_1st == NULL) {
			block_left_1st = block;
			__enable_irq();
		} else if (block_left_2nd == NULL) {
			block_left_2nd = block;
			__enable_irq();
		} else {
			audio_block_t *tmp = block_left_1st;
			block_left_1st = block_left_2nd;
			block_left_2nd = block;
			__enable_irq();
			release(tmp);
		}
	}
}

// TODO: the DAC has much higher bandwidth than the datasheet says
// can we output a 2X oversampled output, for easier filtering?

void AudioOutputFM::isr(void)
{
	const int16_t *src;
	const int8_t *end;
	int8_t *dest;
	audio_block_t *block;
	uint32_t saddr;
//Serial.print("!");
	saddr = (uint32_t)(dma.TCD->SADDR);
	dma.clearInterrupt();
	if (saddr < (uint32_t)fm_buffer + sizeof(fm_buffer) / 2) {
		static int z=0;	
		// DMA is transmitting the first half of the buffer
		// so we must fill the second half
		dest = (int8_t *)&fm_buffer[AUDIO_BLOCK_SAMPLES];
		end = (int8_t *)&fm_buffer[AUDIO_BLOCK_SAMPLES*2];
	} else {
		static int z=0;
		// DMA is transmitting the second half of the buffer
		// so we must fill the first half
		dest = (int8_t *)fm_buffer;
		end = (int8_t *)&fm_buffer[AUDIO_BLOCK_SAMPLES];
	}
	block = AudioOutputFM::block_left_1st;
	if (block) {
		src = block->data;		
		do {
			*dest++ =((((*src++) + 32767) >> 15));
			//Try these(?):
			//*dest++ =4+((((*src++) + 32767) >> 14));		
			//*dest++ =4+((((*src++) + 32767) >> 13));
			//*dest++ =4+((((*src++) + 32767) >> 12));
		} while (dest < end);
		
		AudioStream::release(block);
		AudioOutputFM::block_left_1st = AudioOutputFM::block_left_2nd;
		AudioOutputFM::block_left_2nd = NULL;
	} else {
		do {
			*dest++ = 8;
		} while (dest < end);
	}
	if (AudioOutputFM::update_responsibility) AudioStream::update_all();
}


#if 0 // not implemented 
#elif defined (__MKL26Z64__)

DMAMEM static uint16_t fm_buffer1[AUDIO_BLOCK_SAMPLES];
DMAMEM static uint16_t fm_buffer2[AUDIO_BLOCK_SAMPLES];
audio_block_t * AudioOutputFM::block_left_1st = NULL;
bool AudioOutputFM::update_responsibility = false;
DMAChannel AudioOutputFM::dma1(false);
DMAChannel AudioOutputFM::dma2(false);

void AudioOutputFM::begin(void)
{
	dma1.begin(true); // Allocate the DMA channels first
	dma2.begin(true); // Allocate the DMA channels first

	delay(2500);
	Serial.println("AudioOutputAnalog begin");
	delay(10);

	SIM_SCGC6 |= SIM_SCGC6_DAC0;
	DAC0_C0 = DAC_C0_DACEN | DAC_C0_DACRFS;		// VDDA (3.3V) ref
	// slowly ramp up to DC voltage, approx 1/4 second
	for (int16_t i=0; i<2048; i+=8) {
		*(int16_t *)&(DAC0_DAT0L) = i;
		delay(1);
	}

	// commandeer FTM1 for timing (PWM on pin 3 & 4 will become 22 kHz)
	FTM1_SC = 0;
	FTM1_CNT = 0;
	FTM1_MOD = (uint32_t)((F_PLL/2) / AUDIO_SAMPLE_RATE_EXACT + 0.5);
	FTM1_SC = FTM_SC_CLKS(1);

	dma1.sourceBuffer(fm_buffer1, sizeof(fm_buffer1));
	dma1.destination(*(int16_t *)&DAC0_DAT0L);
	dma1.interruptAtCompletion();
	dma1.disableOnCompletion();
	dma1.triggerAtCompletionOf(dma2);
	dma1.triggerAtHardwareEvent(DMAMUX_SOURCE_FTM1_OV);
	dma1.attachInterrupt(isr1);

	dma2.sourceBuffer(fm_buffer2, sizeof(fm_buffer2));
	dma2.destination(*(int16_t *)&DAC0_DAT0L);
	dma2.interruptAtCompletion();
	dma2.disableOnCompletion();
	dma2.triggerAtCompletionOf(dma1);
	dma2.triggerAtHardwareEvent(DMAMUX_SOURCE_FTM1_OV);
	dma2.attachInterrupt(isr2);

	update_responsibility = update_setup();
/*
	dma.TCD->SADDR = fm_buffer;
	dma.TCD->SOFF = 2;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->SLAST = -sizeof(fm_buffer);
	dma.TCD->DADDR = &DAC0_DAT0L;
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = sizeof(fm_buffer) / 2;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = sizeof(fm_buffer) / 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_PDB);
	update_responsibility = update_setup();
	dma.enable();
	dma.attachInterrupt(isr);
*/
}

void AudioOutputFM::isr1(void)
{
	dma1.clearInterrupt();

}

void AudioOutputFM::isr2(void)
{
	dma2.clearInterrupt();


}


void AudioOutputFM::update(void)
{
	audio_block_t *block;
	block = receiveReadOnly();
	if (block) {
		__disable_irq();
		if (block_left_1st == NULL) {
			block_left_1st = block;
			__enable_irq();
		} else {
			audio_block_t *tmp = block_left_1st;
			block_left_1st = block;
			__enable_irq();
			release(tmp);
		}
	}
}



#endif
#else

void AudioOutputFM::begin(void)
{
}

void AudioOutputFM::update(void)
{
	audio_block_t *block;
	block = receiveReadOnly(0); // input 0
	if (block) release(block);
}

#endif // defined(__MK20DX256__)



