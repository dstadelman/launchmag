#include <msp430g2231.h>
#include <stdbool.h>

#include "DCO_Library.h"

#include "LM_PacketFlags.h"

// ********************************************************************************
// START UART
// ********************************************************************************

// START Half Duplex Software UART on the LaunchPad CODE

// This code based on:
/******************************************************************************
 *                 Half Duplex Software UART on the LaunchPad
 * 
 * Description: This code provides a simple Bi-Directional Half Duplex
 * 		Software UART. The timing is dependant on SMCLK, which
 * 		is set to 1MHz. The transmit function is based off of
 * 		the example code provided by TI with the LaunchPad.
 * 		This code was originally created for "NJC's MSP430
 * 		LaunchPad Blog".
 * 
 * Author: Nicholas J. Conn - http://msp430launchpad.com
 * Email: webmaster at msp430launchpad.com
 * Date: 08-17-10
 ******************************************************************************/
  
#define		UART_TXD             	BIT1    // UART_TXD on P1.1
#define		UART_RXD				BIT2	// UART_RXD on P1.2

#define     Bit_time    		    1664//104		// 9600 Baud, SMCLK=1MHz (1MHz/9600)=104
#define		Bit_time_5			    832//52			// Time for half a bit.

unsigned char UART_BitCnt;					// Bit count, used when transmitting byte
unsigned int  UART_TXByte;					// Value recieved once hasRecieved is set

// Function Transmits Character from UART_TXByte 
void UART_TransmitByte()
{ 
  	CCTL0 = OUT;						// UART_TXD Idle as Mark
  	TACTL = TASSEL_2 + MC_2;			// SMCLK, continuous mode

  	UART_BitCnt = 0xA;					// Load Bit counter, 8 bits + ST/SP
  	CCR0 = TAR;							// Initialize compare register
  
  	CCR0 += Bit_time;					// Set time till first bit
  	UART_TXByte |= 0x100;				// Add stop bit to UART_TXByte (which is logical 1)
  	UART_TXByte = UART_TXByte << 1;		// Add start bit (which is logical 0)
  
  	CCTL0 =  CCIS0 + OUTMOD0 + CCIE;	// Set signal, intial value, enable interrupts
  	while ( CCTL0 & CCIE );				// Wait for previous TX completion
}

// Timer A0 interrupt service routine
#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A (void)
{
	CCR0 += Bit_time;			// Add Offset to CCR0  
	if ( UART_BitCnt == 0)		// If all bits TXed
	{
		TACTL = TASSEL_2;		// SMCLK, timer off (for power consumption)
		CCTL0 &= ~CCIE ;		// Disable interrupt
	}
	else
	{
		CCTL0 |=  OUTMOD2;				// Set TX bit to 0
		if (UART_TXByte & 0x01)
			CCTL0 &= ~OUTMOD2;			// If it should be 1, set it to 1
		UART_TXByte = UART_TXByte >> 1;
		UART_BitCnt--;
	}
}

void UART_Initialize()
{
	volatile unsigned int i;

	P1SEL |= UART_TXD;
	P1DIR |= UART_TXD;
  
	__bis_SR_register(GIE);			// interrupts enabled
	
	// Garbage gets sent out on the UART if we do not wait for a bit
	i = 50000;
	do (i--);
	while (i != 0);
}
// END Half Duplex Software UART on the LaunchPad CODE

void UART_TransmitString(const char * string)
{
	while (*string != 0)
	{
		UART_TXByte = *string++;
		UART_TransmitByte();
	}	
}

// ********************************************************************************
// START MAG STRIPE READER (MSR)
// ********************************************************************************

#define LM_STATUSLED			BIT0

#define LM_T2_CARD_LOADED		BIT2 // track 2 card loaded 
#define LM_T2_CLOCK			    BIT4 // track 2 clock
#define LM_T2_DATA				BIT5 // track 2 data

#define LM_T1_CARD_LOADED		BIT3 // track 1 card loaded
#define LM_T1_CLOCK			    BIT6 // track 1 clock
#define LM_T1_DATA				BIT7 // track 1 data

#define LM_T2DATABUFFER_SIZE				32
#define LM_T1DATABUFFER_SIZE				16

unsigned char LM_t2DataBuffer[LM_T2DATABUFFER_SIZE];
unsigned char LM_t1DataBuffer[LM_T1DATABUFFER_SIZE];

volatile unsigned char LM_t2DataReadLocation  = 0;
volatile unsigned char LM_t2DataWriteLocation = 0;
volatile unsigned char LM_t1DataReadLocation  = 0;
volatile unsigned char LM_t1DataWriteLocation = 0;

volatile unsigned char LM_t2DataCurrentByte   = 0;
volatile unsigned char LM_t2DataCurrentBit    = 0;
volatile unsigned char LM_t1DataCurrentByte   = 0;
volatile unsigned char LM_t1DataCurrentBit    = 0;

void LM_Initialize()
{
	P1DIR |= LM_STATUSLED;         	    // Set LM_STATUSLED to output direction
	
	P1IES |= LM_T2_CLOCK;				// Hi/lo edge interrupt
	P1IFG &= ~LM_T2_CLOCK;				// Clear (flag) before enabling interrupt
	P1IE  |= LM_T2_CLOCK;				// Enable interrupt		
	
	P1IES |= LM_T2_CARD_LOADED;		    // Hi/lo edge interrupt
	P1IFG &= ~LM_T2_CARD_LOADED;		// Clear (flag) before enabling interrupt 
	P1IE  |= LM_T2_CARD_LOADED;		    // Enable interrupt
	
	P1IES |= LM_T1_CLOCK;				// Hi/lo edge interrupt
	P1IFG &= ~LM_T1_CLOCK;				// Clear (flag) before enabling interrupt
	P1IE  |= LM_T1_CLOCK;				// Enable interrupt	
	
	P1IES |= LM_T1_CARD_LOADED;		    // Hi/lo edge interrupt
	P1IFG &= ~LM_T1_CARD_LOADED;		// Clear (flag) before enabling interrupt
	P1IE  |= LM_T1_CARD_LOADED;		    // Enable interrupt
	
	__bis_SR_register(GIE);			    // interrupts enabled
	
	P1OUT |= LM_STATUSLED;              // Card reader ready to rumble
}

void LM_QueueByte(unsigned char *dataBuffer, volatile unsigned char *writeLocation, unsigned char dataBufferSize, unsigned char byte)
{
	dataBuffer[(*writeLocation)++] = byte;
	if ((*writeLocation) >= dataBufferSize)
		(*writeLocation) = 0;
}

void LM_SendQueuedBytes()
{
	while (LM_t2DataReadLocation != LM_t2DataWriteLocation)
	{
		UART_TXByte = LM_t2DataBuffer[LM_t2DataReadLocation++];
		UART_TransmitByte();
		
		if (LM_t2DataReadLocation >= LM_T2DATABUFFER_SIZE)
			LM_t2DataReadLocation = 0;
	}
	
	while (LM_t1DataReadLocation != LM_t1DataWriteLocation)
	{
		UART_TXByte = LM_t1DataBuffer[LM_t1DataReadLocation++];
		UART_TransmitByte();
		
		if (LM_t1DataReadLocation >= LM_T1DATABUFFER_SIZE)
			LM_t1DataReadLocation = 0;
	}	
}

void LM_FlushT2Byte()
{
	LM_t2DataCurrentBit  |= LM_PACKET_FLAG_TRACK2;
	LM_t2DataCurrentByte |= LM_PACKET_FLAG_TRACK2;
	
	LM_QueueByte(LM_t2DataBuffer, &LM_t2DataWriteLocation, LM_T2DATABUFFER_SIZE, LM_t2DataCurrentBit);
	LM_QueueByte(LM_t2DataBuffer, &LM_t2DataWriteLocation, LM_T2DATABUFFER_SIZE, LM_t2DataCurrentByte);
	LM_t2DataCurrentByte = 0;
	LM_t2DataCurrentBit = 0;	
}

void LM_ReadT2Bit(bool bit)
{
	if (bit)
		LM_t2DataCurrentByte |= (0x01 << (4 - LM_t2DataCurrentBit));
	LM_t2DataCurrentBit++;
	if (LM_t2DataCurrentBit >= 5)
		LM_FlushT2Byte();
}

void LM_FlushT1Byte()
{
	// commented out to save space
	//LM_t2DataCurrentBit  |= LM_PACKET_FLAG_TRACK1;
	//LM_t2DataCurrentByte |= LM_PACKET_FLAG_TRACK1; 
	
	LM_QueueByte(LM_t1DataBuffer, &LM_t1DataWriteLocation, LM_T1DATABUFFER_SIZE, LM_t1DataCurrentBit);
	LM_QueueByte(LM_t1DataBuffer, &LM_t1DataWriteLocation, LM_T1DATABUFFER_SIZE, LM_t1DataCurrentByte);
	LM_t1DataCurrentByte = 0;
	LM_t1DataCurrentBit = 0;	
}

void LM_ReadT1Bit(bool bit)
{
	if (bit)
		LM_t1DataCurrentByte |= (0x01 << (4 - LM_t1DataCurrentBit));
	LM_t1DataCurrentBit++;
	if (LM_t1DataCurrentBit >= 5)
		LM_FlushT1Byte();
}

// ********************************************************************************
// START PORT1 ISR
// ********************************************************************************

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{  	
	if (P1IFG & LM_T2_CLOCK)
	{		
		P1IE  &= ~LM_T2_CLOCK;			// Disable LM_T2_CLOCK interrupt
		P1IFG &= ~LM_T2_CLOCK;			// Clear LM_T2_CLOCK IFG (interrupt flag)
		
		LM_ReadT2Bit(!(P1IN & LM_T2_DATA));
		
		P1IE  |= LM_T2_CLOCK;			// Enable interrupt
	}
	else if (P1IFG & LM_T1_CLOCK)
	{		
		P1IE  &= ~LM_T1_CLOCK;			// Disable LM_T1_CLOCK interrupt
		P1IFG &= ~LM_T1_CLOCK;			// Clear LM_T1_CLOCK IFG (interrupt flag)
		
		LM_ReadT1Bit(!(P1IN & LM_T1_DATA));
		
		P1IE  |= LM_T1_CLOCK;			// Enable interrupt
	}
	if (P1IFG & LM_T2_CARD_LOADED)
	{		
		P1IE  &= ~LM_T2_CARD_LOADED;			// Disable LM_T2_CARD_LOADED interrupt
		P1IFG &= ~LM_T2_CARD_LOADED;			// Clear LM_T2_CARD_LOADED IFG (interrupt flag)
		
		if (!(P1IN & LM_T2_CARD_LOADED))
		{ 
			// start read of track 2
			P1IES &= ~LM_T2_CARD_LOADED;		// lo/hi edge interrupt
			P1IE  |= LM_T2_CARD_LOADED;			// Enable interrupt
			
			LM_QueueByte(LM_t2DataBuffer, &LM_t2DataWriteLocation, LM_T2DATABUFFER_SIZE, LM_PACKET_FLAG_TRACK2 | LM_PACKET_FLAG_STARTSTOPCONTROL | LM_PACKET_FLAG_START);
			 
			P1OUT &= ~LM_STATUSLED;				// Read started	
		}
		else
		{
			// end read of track 2
			P1IES |= LM_T2_CARD_LOADED;		    // hi/lo edge interrupt
			P1IE  |= LM_T2_CARD_LOADED;			// Enable interrupt
			
			LM_FlushT2Byte();
			LM_QueueByte(LM_t2DataBuffer, &LM_t2DataWriteLocation, LM_T2DATABUFFER_SIZE, LM_PACKET_FLAG_TRACK2 | LM_PACKET_FLAG_STARTSTOPCONTROL | LM_PACKET_FLAG_STOP);						
			
			P1OUT |= LM_STATUSLED;              // Read complete
		}
	}
	else if (P1IFG & LM_T1_CARD_LOADED)
	{		
		P1IE  &= ~LM_T1_CARD_LOADED;			// Disable LM_T1_CARD_LOADED interrupt
		P1IFG &= ~LM_T1_CARD_LOADED;			// Clear LM_T1_CARD_LOADED IFG (interrupt flag)
		
		if (!(P1IN & LM_T1_CARD_LOADED))
		{
			// start read of track 3
			P1IES &= ~LM_T1_CARD_LOADED;		// lo/hi edge interrupt
			P1IE  |= LM_T1_CARD_LOADED;		// Enable interrupt
			
			LM_QueueByte(LM_t1DataBuffer, &LM_t1DataWriteLocation, LM_T1DATABUFFER_SIZE, LM_PACKET_FLAG_TRACK1 | LM_PACKET_FLAG_STARTSTOPCONTROL | LM_PACKET_FLAG_START);
			 
			P1OUT &= ~LM_STATUSLED;				// Read started			
		}
		else
		{
			// end read of track 3
			P1IES |= LM_T1_CARD_LOADED;		    // hi/lo edge interrupt
			P1IE  |= LM_T1_CARD_LOADED;			// Enable interrupt
			
			LM_FlushT1Byte();
			LM_QueueByte(LM_t1DataBuffer, &LM_t1DataWriteLocation, LM_T1DATABUFFER_SIZE, LM_PACKET_FLAG_TRACK1 | LM_PACKET_FLAG_STARTSTOPCONTROL | LM_PACKET_FLAG_STOP);						
			
			P1OUT |= LM_STATUSLED;              // Read complete
		}
	}
}

#define		DCO_SETTING			TI_DCO_16MHZ		// see DCO_Library.h for more settings

void sDCO()
{
	volatile unsigned int I;
	P1DIR |= BIT0; 									// P1.0 output
	BCSCTL1 &= ~XTS;								// external source is LF;
	BCSCTL3 &= ~(LFXT1S0 + LFXT1S1);  				// watch crystal mode
	BCSCTL3 |= XCAP0 + XCAP1; 						// ~12.5 pf cap on the watch crystal as recommended

	for( I = 0; I < 0xFFFF; I++){} 					// delay for ACLK startup
	if(TI_SetDCO(DCO_SETTING) == TI_DCO_NO_ERROR)	// if setting the clock was successful,
		P1OUT |= BIT0; 								// bring P1.0 high (Launchpad red LED)
	else
		while(1);									// trap if setting the clock isn't successful
}

void main(void)
{
	WDTCTL = WDTPW + WDTHOLD;
	
	sDCO();		
	
	UART_Initialize();
	
	LM_Initialize();
	
	while(1)
		LM_SendQueuedBytes();
}
