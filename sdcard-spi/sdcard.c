/*
 * sdcard.c
 *
 *  Created on: 15/09/2016
 *  Author: Javier Martínez Arrieta
 *  Version: 1.0
 *  This is part of the sdcard library, with functions that will allow you to read and (in the future) write in an SD card formatted using FAT32 (single partition).
 */

 /*  Copyright (C) 2016 Javier Martínez Arrieta
 *
 *  This project is licensed under Creative Commons Attribution-Non Commercial-Share Alike 4.0 International (CC BY-NC-SA 4.0). According to this license you are free to:
 *  Share & copy and redistribute the material in any medium or format.
 *  Adapt & remix, transform, and build upon the material.
 *  The licensor cannot revoke these freedoms as long as you follow the license terms.
 *	Complete information about this license can be found at: https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode
 *
 */


/* Part of this example (partially modified functions rcvr_datablock, rcvr_spi_m, disk_timerproc, Timer5A_Handler, Timer5_Init, is_ready, send_command and part of initialize_sd) accompanies the books
   Embedded Systems: Real-Time Operating Systems for ARM Cortex-M Microcontrollers, Volume 3,
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2013

   Volume 3, Program 6.3, section 6.6   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013

 Copyright 2013 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file (concretely the functions mentioned)
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

#include "sdcard.h"
#include "/Users/javiermartinez/TivaWare/inc/tm4c123gh6pm.h"
#include <stdio.h>


unsigned char Timer1,Timer2;
unsigned long lba_begin_address,number_of_sectors,lba_addr,cluster_start,file_size,cluster_begin_lba,fat_begin_lba,sectors_per_fat,root_dir_first_cluster;
unsigned long previous_cluster=0,cluster_dir=0;
unsigned char sectors_per_cluster;
char fd_count=0,current_count=0;
char finish=0;
int row=0,column=0,number=0;

typedef struct
{
	char hour;
	char minute;
	unsigned int year;
	char month;
	char day;
	long size;
	long first_cluster;
}tfile_info;

typedef struct
{
	tfile_info info;
	unsigned char file_dir_name[255];
}tfile_name;

typedef struct
{
	tfile_name name;
	enum type_of_file
	{
		IS_NONE,
		IS_DIR,
		IS_FILE
	}type;
}tfile_dir;

tfile_dir file_dir[40];

/**
 * Writes to the SD card
 */
void sd_write(char message,enum SSI SSI_number)
{
	unsigned short volatile rcvdata;
	switch(SSI_number)
	{
		case SSI0:
		{
			// wait until transmit FIFO not full
			while((SSI0_SR_R&SSI_SR_TNF)==0){};
			//DC = DC_DATA;
			SSI0_DR_R = message;
			while((SSI0_SR_R&SSI_SR_RNE)==0){};
			rcvdata = SSI0_DR_R;
			break;
		}
		case SSI1:
		{
			while((SSI1_SR_R&SSI_SR_TNF)==0){};
			//DC = DC_DATA;
			SSI1_DR_R = message;
			while((SSI1_SR_R&SSI_SR_RNE)==0){};
			rcvdata = SSI1_DR_R;
			break;
		}
		case SSI2:
		{
			while((SSI2_SR_R&SSI_SR_TNF)==0){};
			//DC = DC_DATA;
			SSI2_DR_R = message;
			while((SSI2_SR_R&SSI_SR_RNE)==0){};
			rcvdata = SSI2_DR_R;
			break;
		}
		case SSI3:
		{
			while((SSI3_SR_R&SSI_SR_TNF)==0){};
			//DC = DC_DATA;
			SSI3_DR_R = message;
			while((SSI3_SR_R&SSI_SR_RNE)==0){};
			rcvdata = SSI3_DR_R;
			break;
		}
	}
}

/*
 * Removes null or other non-printable characters from the file or directory name string
 * Also 'translate' accented characters so they can be printed
 */
void clean_name()
{
    char temp_name[255] = "";
    char j=0, k=0;
    //Remove all non-rintable characters
    for(j=0;j<255;j++)
    {
        if(file_dir[fd_count].name.file_dir_name[j]>=0x20 && file_dir[fd_count].name.file_dir_name[j]<=0xFC)
        {
            temp_name[k] = file_dir[fd_count].name.file_dir_name[j];
            k++;
        }
    }
    for(j=0;j<255;j++)
    {
        file_dir[fd_count].name.file_dir_name[j] = temp_name[j];
    }


    //Uncomment the following line if using my Nokia5110 library
    //Translate Unicode extended characters like accented characters
    /*char original_char[] = {'\xe1','\xe9', '\xed', '\xf3', '\xfa', '\xfc', '\xf1', '\xd1'};
    char translated_char[] = {'\xa1', '\xa9', '\xad', '\xb3', '\xba', '\xbc', '\xb1', '\x91'};
    for(j=0;j<255;j++)
    {
        uint8_t found = 0;
        k=0;
        while(found == 0 && k < 8)
        {
            if(file_dir[fd_count].name.file_dir_name[j] == original_char[k])
            {
                found = 1;
                file_dir[fd_count].name.file_dir_name[j] = translated_char[k];
            }
            k++;
        }
    }*/
}

/*
 * Reads from the SD card
 */
unsigned char sd_read(enum SSI SSI_number)
{
	switch(SSI_number)
	{
		case SSI0:
		{
			while((SSI0_SR_R&SSI_SR_TNF)==0){};    // wait until room in FIFO
			SSI0_DR_R = 0xFF;                      // data out, garbage
			while((SSI0_SR_R&SSI_SR_RNE)==0){};    // wait until response
			return (unsigned char) SSI0_DR_R;      // read received data
			break;
		}
		case SSI1:
		{
			while((SSI1_SR_R&SSI_SR_TNF)==0){};    // wait until room in FIFO
			SSI1_DR_R = 0xFF;                      // data out, garbage
			while((SSI1_SR_R&SSI_SR_RNE)==0){};    // wait until response
			return (unsigned char) SSI1_DR_R;      // read received data
			break;
		}
		case SSI2:
		{
			while((SSI2_SR_R&SSI_SR_TNF)==0){};    // wait until room in FIFO
			SSI2_DR_R = 0xFF;                      // data out, garbage
			while((SSI2_SR_R&SSI_SR_RNE)==0){};    // wait until response
			return (unsigned char) SSI2_DR_R;      // read received data
			break;
		}
		case SSI3:
		{
			while((SSI3_SR_R&SSI_SR_TNF)==0){};    // wait until room in FIFO
			SSI3_DR_R = 0xFF;                      // data out, garbage
			while((SSI3_SR_R&SSI_SR_RNE)==0){};    // wait until response
			return (unsigned char) SSI3_DR_R;      // read received data
			break;
		}
	}
}

/*
 * Wait until sd card is ready
 */
unsigned char is_ready(enum SSI SSI_number){
  unsigned char response;
  Timer2 = 50;    /* Wait for ready in timeout of 500ms */
  //sd_write(DATA,0xFF,SSI0);
  do
  {
    response = sd_read(SSI_number);
  }while ((response != 0xFF) && Timer2);
  return response;
}


/*
 * Sends the command, preparing the packet to be sent
 */
unsigned char send_command(unsigned char command, unsigned long argument, enum SSI SSI_number)
{
	/* Argument */
	unsigned char crc, response,n;
	if (is_ready(SSI_number) != 0xFF) return 0xFF;

    /* Send command packet */
	sd_write(command,SSI_number);                        /* Command */
	sd_write((unsigned char)(argument >> 24),SSI_number);        /* Argument[31..24] */
	sd_write((unsigned char)(argument >> 16),SSI_number);        /* Argument[23..16] */
	sd_write((unsigned char)(argument >> 8),SSI_number);            /* Argument[15..8] */
	sd_write((unsigned char)argument,SSI_number);                /* Argument[7..0] */
	crc = 0;
	if (command == CMD0)
	{
		crc = 0x95;            /* CRC for CMD0(0) */
	}
	if (command == CMD8)
	{
		crc = 0x87;            /* CRC for CMD8(0x1AA) */
	}
	sd_write(crc,SSI_number);

    /* Receive command response */
	if (command == CMD12) sd_write(0xFF,SSI_number);        /* Skip a stuff byte when stop reading */
	n = 10;                                /* Wait for a valid response in timeout of 10 attempts */
	do
	{
		response = sd_read(SSI_number);
	}while ((response & 0x80) && --n);
	return response;
}

/*
 * Initialises the SD card
 */
void initialise_sd(enum SSI SSI_number)
{
	unsigned char i;
	unsigned char ocr[4];
	unsigned char sd_type;
	//Sends a 1 through CS and MOSI lines for at least 74 clock cycles
	cs_high(SSI_number);
	dummy_clock(SSI_number);
	cs_low(SSI_number);
	i=0;
	/*Checks if SD card is in IDLE mode. If so, response will be 1*/
	if(send_command(CMD0, 0,SSI_number) == 1)
	{
		Timer1 = 100; /* Initialization timeout of 1000 msec */
		if(send_command(CMD8, 0x1AA,SSI_number) == 1)
		{
			/* SDC Ver2+ */
			for(i=0;i<4;i++)
			{
				ocr[i]=sd_read(SSI_number);
			}
			if(ocr[2]==0x01&&ocr[3]==0xAA)
			{
				//sends ACMD41, which is a command sequence of CMD55 and CMD41
				do
				{
					if(send_command(CMD55, 0, SSI_number) <= 1 && send_command(CMD41, 1UL << 30,SSI_number) == 0)
					{
						break; //R1 response is 0x00
					}
				}while(Timer1);
				if(Timer1 && send_command(CMD58, 0,SSI_number) == 0)
				{
					for(i=0;i<4;i++)
					{
						ocr[i]=sd_read(SSI_number);
						sd_type = (ocr[0] & 0x40) ? 6 : 2;
					}
				}
			}
		}
		else
		{
			/*It is not SD version 2 or upper*/
			sd_type=(send_command(CMD55, 0,SSI_number)<=1 &&send_command(CMD41, 0,SSI_number) <= 1) ? 2 : 1;    /*Check if SD or MMC*/
			do
			{
				if(sd_type==2)
				{
					if(send_command(CMD55, 0,SSI_number)<=1&&send_command(CMD41, 0,SSI_number)==0) /*ACMD41*/
			        {
						break;
			        }
				}
				else
				{
					if (send_command(CMD1, 0,SSI_number) == 0) /*CMD1*/
					{
						break;
					}
			    }
			}while(Timer1);
			if(!Timer1 || send_command(CMD16, 512,SSI_number) != 0)    /*Select R/W block length if timeput not reached*/
			{
				sd_type=0;
			}
		}
	}
	else
	{
		printf("Failure in CMD0");
	}
}


void startSSI0()
{
	volatile unsigned long delay;
	SYSCTL_RCGC2_R |= 0x00000001;   	  //  activate clock for Port A
	SYSCTL_RCGCSSI_R|=SYSCTL_RCGCSSI_R0;  //  activate clock for SSI0
	delay = SYSCTL_RCGC2_R;         	  //  allow time for clock to stabilize
	GPIO_PORTA_DIR_R |= 0x08;             // make PA6,7 out
	GPIO_PORTA_DR4R_R |= 0xEC;            // 4mA output on outputs
	GPIO_PORTA_AFSEL_R |= 0x34;           // enable alt funct on PA2,4,5
	GPIO_PORTA_AFSEL_R &= ~0xC8;          // disable alt funct on PA3,6,7
	GPIO_PORTA_DEN_R |= 0xFC;             // enable digital I/O on PA2,3,4,5,6,7
										  // configure PA2,4,5 as SSI
	GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFF00F0FF)+0x00220200;
	                                      // configure PA6,7 as GPIO
	GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0x00FFFFFF)+0x00000000;
	GPIO_PORTA_AMSEL_R &= ~0xFC;          // disable analog functionality on PA2,3,4,5
	SSI0_CR1_R&=~SSI_CR1_SSE;		  	  // Disable SSI while configuring it
	SSI0_CR1_R&=~SSI_CR1_MS;		      // Set as Master
	SSI0_CC_R|=SSI_CC_CS_M; 			  // Configure clock source
	SSI0_CC_R|=SSI_CC_CS_SYSPLL; 		  // Configure clock source
	SSI0_CC_R|=SSI_CPSR_CPSDVSR_M;		  // Configure prescale divisor
	SSI0_CPSR_R = (SSI0_CPSR_R&~SSI_CPSR_CPSDVSR_M)+10; // must be even number
	SSI0_CR0_R |=0x0300;
	SSI0_CR0_R &= ~(SSI_CR0_SPH | SSI_CR0_SPO);
	SSI0_CR0_R = (SSI0_CR0_R&~SSI_CR0_FRF_M)+SSI_CR0_FRF_MOTO;
	                                        // DSS = 8-bit data
	SSI0_CR0_R = (SSI0_CR0_R&~SSI_CR0_DSS_M)+SSI_CR0_DSS_8;
	SSI0_CR1_R|=SSI_CR1_SSE;		  // 3)Enable SSI
}

void startSSI1()
{
	volatile unsigned long delay;
	SYSCTL_RCGC2_R |= 0x30;   		// activate clock for Port E and Port F
	//SYSCTL_RCGC1_R |= SYSCTL_RCGC1_SSI1;
	SYSCTL_RCGCSSI_R|=SYSCTL_RCGCSSI_R1;	// activate clock for SSI1
	delay = SYSCTL_RCGC2_R;         		// allow time for clock to stabilize
	GPIO_PORTF_LOCK_R=0x4C4F434B;
	GPIO_PORTF_CR_R=0x01;
	GPIO_PORTF_DIR_R |= 0x08;             // make PF3 out
	GPIO_PORTF_AFSEL_R |= 0x07;           // enable alt funct on PF0, PF1 and PF2
	GPIO_PORTF_AFSEL_R &= ~0xF8;          // disable alt funct on PF3
	GPIO_PORTF_DEN_R |= 0x0F;             // enable digital I/O on PF0,PF1,PF2,PF3
                                          // configure PF0, PF1 and PF2 as SSI
	GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0x00000FFF)+0x00000222;
	                                      // configure PF3 as GPIO
	GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFF0FFF)+0x00000000;
	GPIO_PORTF_AMSEL_R &= ~0x0F;          // disable analog functionality on PF0,PF1,PF2,PF3
	SSI1_CR1_R&=~SSI_CR1_SSE;		  		// Disable SSI while configuring it
	SSI1_CR1_R&=~SSI_CR1_MS;		  		// Set as Master
	SSI1_CC_R|=SSI_CC_CS_M; 				// Configure clock source
	SSI1_CC_R|=SSI_CC_CS_SYSPLL; 			// Configure clock source
	SSI1_CC_R|=SSI_CPSR_CPSDVSR_M;		// Configure prescale divisor
	SSI1_CPSR_R = (SSI1_CPSR_R&~SSI_CPSR_CPSDVSR_M)+10; // must be even number
	SSI1_CR0_R |=0x0300;
	SSI1_CR0_R &= ~(SSI_CR0_SPH | SSI_CR0_SPO);
	SSI1_CR0_R = (SSI1_CR0_R&~SSI_CR0_FRF_M)+SSI_CR0_FRF_MOTO;
	                                        // DSS = 8-bit data
	SSI1_CR0_R = (SSI1_CR0_R&~SSI_CR0_DSS_M)+SSI_CR0_DSS_8;
	SSI1_CR1_R|=SSI_CR1_SSE;		  		// Enable SSI
}

void startSSI2()
{
	volatile unsigned long delay;
	SYSCTL_RCGC2_R |= 0x00000002;   //  activate clock for Port B
	SYSCTL_RCGCSSI_R|=SYSCTL_RCGCSSI_R2;	// activate clock for SSI2
	delay = SYSCTL_RCGC2_R;         //    allow time for clock to stabilize
	GPIO_PORTB_LOCK_R=0x4C4F434B;
	GPIO_PORTB_CR_R=0x0C;
	GPIO_PORTB_DIR_R |= 0x20;             // make PB5 out
	GPIO_PORTB_AFSEL_R |= 0xD0;           // enable alt funct on PB4,PB6 and PB7
	GPIO_PORTB_DEN_R |= 0xF0;             // enable digital I/O on PB4,PB5,PB6 and PB7
                                          // configure PB4,PB6 and PB7 as SSI
	GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0x00F0FFFF)+0x22020000;
	                                      // configure PB5 as GPIO
	GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFF0FFFFF)+0x00000000;
	GPIO_PORTB_AMSEL_R &= ~0xFC;          // disable analog functionality from PB2 to PB7
	SSI2_CR1_R&=~SSI_CR1_SSE;		  // Disable SSI while configuring it
	SSI2_CR1_R&=~SSI_CR1_MS;		  //  Set as Master
	SSI2_CC_R|=SSI_CC_CS_M; //  Configure clock source
	SSI2_CC_R|=SSI_CC_CS_SYSPLL; //  Configure clock source
	SSI2_CC_R|=SSI_CPSR_CPSDVSR_M;//  Configure prescale divisor
	SSI2_CPSR_R = (SSI2_CPSR_R&~SSI_CPSR_CPSDVSR_M)+10; // must be even number
	SSI2_CR0_R |=0x0300;
	SSI2_CR0_R &= ~(SSI_CR0_SPH | SSI_CR0_SPO);
	SSI2_CR0_R = (SSI2_CR0_R&~SSI_CR0_FRF_M)+SSI_CR0_FRF_MOTO;
	                                        // DSS = 8-bit data
	SSI2_CR0_R = (SSI2_CR0_R&~SSI_CR0_DSS_M)+SSI_CR0_DSS_8;
	SSI2_CR1_R|=SSI_CR1_SSE;		  // 3)Enable SSI
}

void startSSI3()
{
	volatile unsigned long delay;
	SYSCTL_RCGC2_R |= 0x00000008;   		// activate clock for Port D
	//SYSCTL_RCGC1_R|=SYSCTL_RCGC1_SSI1;	// activate clock for SSI3
	SYSCTL_RCGCSSI_R|=SYSCTL_RCGCSSI_R3;	// activate clock for SSI3
	delay = SYSCTL_RCGC2_R;         		// allow time for clock to stabilize
	GPIO_PORTD_LOCK_R=0x4C4F434B;
	GPIO_PORTD_CR_R=0x80;
	GPIO_PORTD_DIR_R |= 0x08;             // make PD3 out
	GPIO_PORTD_AFSEL_R |= 0x0D;           // enable alt funct on PD0, PD2 and PD3
	GPIO_PORTD_AFSEL_R &= ~0xF2;          // disable alt funct on PD1
	GPIO_PORTD_DEN_R |= 0x0F;             // enable digital I/O on PD0, PD1, PD2 and PD3
	// configure PD0, PD2 and PD3 as SSI
	GPIO_PORTD_PCTL_R = (GPIO_PORTD_PCTL_R&0xFFFF00F0)+0x00001101;
	// configure PD1 as GPIO
	GPIO_PORTD_PCTL_R = (GPIO_PORTD_PCTL_R&0xFFFFFFF0F)+0x00000000;
	                                        // configure PD0, PD1 and PD3 as SSI
	GPIO_PORTD_AMSEL_R &= ~0xCF;          // disable analog functionality on PD0, PD1, PD2, PD3, PD6 and PD7
	SSI3_CR1_R&=~SSI_CR1_SSE;		  		// Disable SSI while configuring it
	SSI3_CR1_R&=~SSI_CR1_MS;		  		// Set as Master
	SSI3_CC_R|=SSI_CC_CS_M; 				// Configure clock source
	SSI3_CC_R|=SSI_CC_CS_SYSPLL; 			// Configure clock source
	SSI3_CC_R|=SSI_CPSR_CPSDVSR_M;		// Configure prescale divisor
	SSI3_CPSR_R = (SSI3_CPSR_R&~SSI_CPSR_CPSDVSR_M)+10; // must be even number
	SSI3_CR0_R |=0x0300;
	SSI3_CR0_R &= ~(SSI_CR0_SPH | SSI_CR0_SPO);
	SSI3_CR0_R = (SSI3_CR0_R&~SSI_CR0_FRF_M)+SSI_CR0_FRF_MOTO;
	                                        // DSS = 8-bit data
	SSI3_CR0_R = (SSI3_CR0_R&~SSI_CR0_DSS_M)+SSI_CR0_DSS_8;
	SSI3_CR1_R|=SSI_CR1_SSE;		  // 3)Enable SSI
}

/*
 * Makes use of the clock along with CS and MOSI to make the SD card readable using SPI
 */
void dummy_clock(enum SSI SSI_number)
{
	unsigned int i;
	//In order to initialize and set SPI mode, there should be at least 74 clock cycles with MOSI and CS set to 1
	for ( i = 0; i < 2; i++);
	//CS set high
	cs_high(SSI_number);
	//Disables SSI on TX/MOSI pin to send a 1
	tx_high(SSI_number);
	for ( i = 0; i < 10; i++)
	{
		sd_write(0xFF,SSI_number);
	}
	tx_SSI(SSI_number);
}

/*
 * Gets the first cluster of a file or directory
 */
long get_first_cluster(int pos)
{
	return file_dir[pos].name.info.first_cluster;
}

/*
 * Gets the first cluster of the root directory
 */
long get_root_dir_first_cluster(void)
{
	return root_dir_first_cluster;
}

/*
 * Makes chip select line high
 */
void cs_high(enum SSI SSI_number)
{
	switch(SSI_number)
	{
		case SSI0:
		{
			GPIO_PORTA_DATA_R|=0x08;
			break;
		}
		case SSI1:
		{
			GPIO_PORTF_DATA_R|=0x08;
			break;
		}
		case SSI2:
		{
			GPIO_PORTB_DATA_R|=0x20;
			break;
		}
		case SSI3:
		{
			GPIO_PORTD_DATA_R|=0x02;
			break;
		}
	}
}

/*
 * Makes chip select line low
 */
void cs_low(enum SSI SSI_number)
{
	switch(SSI_number)
	{
		case SSI0:
		{
			GPIO_PORTA_DATA_R&=~0x08;
			break;
		}
		case SSI1:
		{
			GPIO_PORTF_DATA_R&=~0x08;
			break;
		}
		case SSI2:
		{
			GPIO_PORTB_DATA_R&=~0x20;
			break;
		}
		case SSI3:
		{
			GPIO_PORTD_DATA_R&= ~0x02;
			break;
		}
	}
}

/*
 * Writes a '1' in the transmission line of the SSI that is being used
 */
void tx_high(enum SSI SSI_number)
{
	switch(SSI_number)
	{
		case SSI0:
		{
			GPIO_PORTA_AFSEL_R &= ~0x20;           // disable alt funct on PA5
			GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFF0FFFFF);
			GPIO_PORTA_DATA_R |= 0x20;            // PA5 high
			break;
		}
		case SSI1:
		{
			GPIO_PORTF_AFSEL_R &= ~0x02;           // disable alt funct on PF1
			GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFFF0F);
			GPIO_PORTF_DATA_R |= 0x02;            // PF1 high
			break;
		}
		case SSI2:
		{
			GPIO_PORTB_AFSEL_R &= ~0x80;           // disable alt funct on PB7
			GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0x0FFFFFFF);
			GPIO_PORTB_DATA_R |= 0x80;            // PB7 high
			break;
		}
		case SSI3:
		{
			GPIO_PORTD_AFSEL_R &= ~0x08;           // disable alt funct on PD3
			GPIO_PORTD_PCTL_R = (GPIO_PORTD_PCTL_R&0xFFFF0FFF);
			GPIO_PORTD_DATA_R |= 0x08;            // PD3 high
			break;
		}
	}
}

/*
 * Configure again the transmission line of the SSI that is being used
 */
void tx_SSI(enum SSI SSI_number)
{
	switch(SSI_number)
	{
		case SSI0:
		{
			GPIO_PORTA_AFSEL_R |= 0x20;           // enable alt funct on PA5
			GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFF0FFFFF)+0x00200000;
			break;
		}
		case SSI1:
		{
			GPIO_PORTF_AFSEL_R |= 0x02;           // enable alt funct on PF1
			GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFFF0F)+0x00000020;
			break;
		}
		case SSI2:
		{
			GPIO_PORTB_AFSEL_R |= 0x80;           // enable alt funct on PB7
			GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0x0FFFFFFF)+0x20000000;
			break;
		}
		case SSI3:
		{
			GPIO_PORTD_AFSEL_R |= 0x08;           // enable alt funct on PD3
			GPIO_PORTD_PCTL_R = (GPIO_PORTD_PCTL_R&0xFFFF0FFF)+0x00001000;
			break;
		}
	}
}

/*Change speed to (?) 8 MHz*/
void change_speed(enum SSI SSI_number)
{
	switch(SSI_number)
	{
		case SSI0:
		{
			SSI0_CC_R|=SSI_CPSR_CPSDVSR_M;// Configure prescale divisor
			SSI0_CPSR_R = (SSI0_CPSR_R&~SSI_CPSR_CPSDVSR_M)+2; // must be even number
			SSI0_CR0_R |=0x0000;
			break;
		}
		case SSI1:
		{
			SSI1_CC_R|=SSI_CPSR_CPSDVSR_M;// Configure prescale divisor
			SSI1_CPSR_R = (SSI1_CPSR_R&~SSI_CPSR_CPSDVSR_M)+2; // must be even number
			SSI1_CR0_R |=0x0000;
			break;
		}
		case SSI2:
		{
			SSI2_CC_R|=SSI_CPSR_CPSDVSR_M;// Configure prescale divisor
			SSI2_CPSR_R = (SSI2_CPSR_R&~SSI_CPSR_CPSDVSR_M)+2; // must be even number
			SSI2_CR0_R |=0x0000;
			break;
		}
		case SSI3:
		{
			SSI3_CC_R|=SSI_CPSR_CPSDVSR_M;// Configure prescale divisor
			SSI3_CPSR_R = (SSI3_CPSR_R&~SSI_CPSR_CPSDVSR_M)+2; // must be even number
			SSI3_CR0_R |=0x0000;
			break;
		}
	}
}

void read_csd(enum SSI SSI_number)
{
	unsigned char csd[16];
	send_command(CMD9,0,SSI_number);
	rcvr_datablock(csd,16,SSI_number);
}

/*
 * Verify if file system is FAT32
 */
void read_first_sector(enum SSI SSI_number)
{
	unsigned char buffer[512];
	if (send_command(CMD17, 0x00000000,SSI_number) == 0)
	{
		rcvr_datablock(buffer, 512,SSI_number);
	}
	if((buffer[450] == 0x0B || buffer[450] == 0x0C) && buffer[510] == 0x55 && buffer[511] == 0xAA)
	{
		printf("FS is FAT32\n");
	}
	else{
		printf("\nError FAT32");
		exit(1);
	}
	lba_begin_address=(unsigned long)buffer[454]+(((unsigned long)buffer[455])<<8)+(((unsigned long)buffer[456])<<16)+(((unsigned long)buffer[457])<<24);
	number_of_sectors=(unsigned long)buffer[458]+(((unsigned long)buffer[459])<<8)+(((unsigned long)buffer[460])<<16)+(((unsigned long)buffer[461])<<24);
}

/*
 * Reads the necessary data so as to be able to access the files and directories
 */
void read_disk_data(enum SSI SSI_number)
{
	unsigned char buffer[512];
	if (send_command(CMD17, lba_begin_address,SSI_number) == 0)
	{
		rcvr_datablock(buffer, 512,SSI_number);
	}
	fat_begin_lba = lba_begin_address + (unsigned long)buffer[14] + (((unsigned long)buffer[15])<<8); //Partition_LBA_BEGIN + Number of reserved sectors
	sectors_per_fat=((unsigned long)buffer[36]+(((unsigned long)buffer[37])<<8)+(((unsigned long)buffer[38])<<16)+(((unsigned long)buffer[39])<<24));
	cluster_begin_lba = fat_begin_lba + ((unsigned long)buffer[16] * ((unsigned long)buffer[36]+(((unsigned long)buffer[37])<<8)+(((unsigned long)buffer[38])<<16)+(((unsigned long)buffer[39])<<24)));//Partition_LBA_Begin + Number_of_Reserved_Sectors + (Number_of_FATs * Sectors_Per_FAT);
	sectors_per_cluster = (unsigned char) buffer[13];//BPB_SecPerClus;
	root_dir_first_cluster = (unsigned long)buffer[44]+(((unsigned long)buffer[45])<<8)+(((unsigned long)buffer[46])<<16)+(((unsigned long)buffer[47])<<24);//BPB_RootClus;
	lba_addr = cluster_begin_lba + ((root_dir_first_cluster/*cluster_number*/ - 2) * (unsigned long)sectors_per_cluster);
}

/*
 * List directories and files using the long name (if it has) or the short name, listing subdirectories as well if asked by the user
 */
long get_files_and_dirs(long next_cluster,enum name_type name, enum get_subdirs subdirs, enum SSI SSI_number)
{
	unsigned char buffer[512];
    char filename[255] = "";
	int position=0,filename_position=0;
	int n=0;
	unsigned long count=10,sectors_to_be_read=sectors_per_cluster;//Calculate this
	long address=cluster_begin_lba + ((next_cluster - 2) * (unsigned long)sectors_per_cluster);
	if(cluster_dir == next_cluster)
	{
		cluster_dir=0;
	}
	if(send_command(CMD18,address,SSI_number)==0)
	{
		do
		{
			rcvr_datablock(buffer, 512,SSI_number);
			do
			{
				if(position<512 && filename_position<255)
				{//Long filename text - 11th byte is 0x0F
					if(position%32==0)
					{//Check if file has a long filename text, normal record with short filename, unused or end of a directory
						if(buffer[position]==0x00 || buffer[position]==0x2E)
						{//End of directory
							position=position+32;
						}
						else
						{
							if(buffer[position]==0xE5)
							{//Deleted file or directory that is maintained until overriden
								position=position+32;
							}
							else
							{
								if(name==LONG_NAME)//Review this
								{//Review this as there are files which long name are not read, probably because they only have one sequence for the name
									short keep_counting=1,do_not_continue=0,is_dir=0, keep_reading = 1;
									uint8_t seq_num = 0, filename_read_finished = 0;
									if(buffer[position+11]==0x0F)
									{
									    do
                                        {
                                            //Check if it is last record group of the filename
                                            if(buffer[position+11]!=0x0F)
                                            {
                                                keep_reading = 0;
                                                if(position == 512)
                                                {
                                                    filename_read_finished = 0;
                                                }
                                                else
                                                {
                                                    filename_read_finished = 1;
                                                }
                                            }
                                            //Get the sequence number
                                            seq_num=buffer[position]&0x1F;

                                            uint8_t k=0,l=0;
                                            while(k<32 && keep_reading == 1)
                                            {
                                                if((k>0 && k<11) || (k>13 && k<26) || (k>27 && k<32))
                                                {
                                                    filename[(32*(seq_num-1))+l] = buffer[position];
                                                    l++;
                                                }
                                                k++;
                                                position++;
                                            }
                                        }while(keep_reading == 1);
									}
									else
									{
									    //Filename exactly has a length of 8 bytes and either the base name or the extension have all its characters in capital letters, so we need to check
									    if((buffer[position+12]&0x18)>0)
                                        {
									        if((buffer[position+12]&0x10)>0 && (buffer[position+12]&0x08)>0)
									        {
									            uint8_t k=0;
									            while(k<11)
									            {
									                if(k < 8)
									                {
									                    filename[k] = buffer[position];
									                }
									                else
									                {
									                    if(k >= 8)
									                    {
									                        filename[k+1] = buffer[position];
									                    }
									                }
									                k++;
									                position++;
									            }
									            filename[8] = '.';
									        }
									        else
									        {
									            //Extension is in lowercase, basename is in uppercase
									            if((buffer[position+12]&0x10)>0)
									            {
									                uint8_t k=0;
									                while(k<11)
									                {
									                    if(k<8)
									                    {
									                        filename[k] = buffer[position];
									                    }
									                    else
									                    {
									                        filename[k+1] = buffer[position] + 32;
									                    }
									                    k++;
									                    position++;
									                }
									                filename[8] = '.';
									            }
									            else
									            {
									                //Extension in uppercase, basename in lowercase
									                if((buffer[position+12]&0x08)>0)
									                {
									                    uint8_t k=0;
									                    while(k<11)
									                    {
									                        if(k<8)
									                        {
									                            filename[k] = buffer[position] + 32;
									                        }
									                        else
									                        {
									                            filename[k+1] = buffer[position];
									                        }
									                        k++;
									                        position++;
									                    }
									                    filename[8] = '.';
									                }
									            }
									        }
                                            filename_read_finished = 1;
                                            position = position - 11;
                                        }
									    else
									    {
									        //Both basename and extension are lowercase
									        if((buffer[position+12]&0x18)==0)
									        {
									            uint8_t k=0;
									            while(k<11)
									            {
									                if(k < 8)
									                {
									                    filename[k] = buffer[position] + 32;
									                }
									                else
									                {
									                    if(k >= 8)
									                    {
									                        filename[k+1] = buffer[position] + 32;
									                    }
									                }
									                k++;
									                position++;
									            }
									            filename[8] = '.';
	                                            filename_read_finished = 1;
	                                            position = position - 11;
									        }
									    }
									}
									//Check if filename is a System file and the filename reading was completed
									if((buffer[position+11]&0x0E)==0x00 && filename_read_finished == 1)
									{
									    if((buffer[position+11]&0x30)==0x10)
                                        {
                                            file_dir[fd_count].type=IS_DIR;
                                        }
                                        else
                                        {
                                            if((buffer[position+11]&0x30)==0x20)
                                            {
                                                file_dir[fd_count].type=IS_FILE;
                                            }
                                        }
									    uint8_t k = 0;
									    while(k<255)
									    {
									        file_dir[fd_count].name.file_dir_name[k]=filename[k];
									        k++;
									    }
									    //Reset filename
									    k = 0;
									    while(k<255)
									    {
									        filename[k]=0x00;
									        k++;
									    }
                                        int time=(((int)(buffer[position+23]))<<8) + ((int)buffer[position+22]);
                                        int date=(((int)(buffer[position+25]))<<8) + ((int)buffer[position+24]);
                                        file_dir[fd_count].name.info.minute=(time&0x07E0)>>5;
                                        file_dir[fd_count].name.info.hour=(time&0xF800)>>11;
                                        file_dir[fd_count].name.info.month=((date&0x01E0)>>5);
                                        file_dir[fd_count].name.info.year=((date&0xFE00)>>9)+1980;
                                        file_dir[fd_count].name.info.day=date&0x001F;
                                        file_dir[fd_count].name.info.size=(long)((buffer[position+31])<<24)+(long)((buffer[position+30])<<16)+(long)((buffer[position+29])<<8)+(long)(buffer[position+28]);
                                        file_dir[fd_count].name.info.first_cluster=(long)((buffer[position+21])<<24)+(long)((buffer[position+20])<<16)+(long)((buffer[position+27])<<8)+(long)(buffer[position+26]);
                                        position = position + 32;
									}
									else
									{
									    if(filename_read_finished == 1)
									    {
                                            //Reset filename
                                            uint8_t k = 0;
                                            while(k<255)
                                            {
                                                filename[k]=0x00;
                                                k++;
                                            }
                                            position++;
									    }
									}
								}
								else
								{//Normal record with short filename
									//Check it is not a system file or directory and names to be read are short names
								    if((buffer[position+11]&0x0E)==0x00 && name==SHORT_NAME)
								    {
								        if((buffer[position+11]&0x30)==0x10)
								        {
								            file_dir[fd_count].type=IS_DIR;
								        }
								        else
								        {
								            if((buffer[position+11]&0x30)==0x10)
								            {
								                file_dir[fd_count].type=IS_FILE;
								            }
								        }
                                        for(n=0;n<11;n++)
                                        {
                                            file_dir[fd_count].name.file_dir_name[n]=buffer[position];
                                            position++;
                                        }
                                        int time=(((int)(buffer[position-11+23]))<<8) + ((int)buffer[position-11+22]);
                                        int date=(((int)(buffer[position-11+25]))<<8) + ((int)buffer[position-11+24]);
                                        file_dir[fd_count].name.info.minute=(time&0x07E0)>>5;
                                        file_dir[fd_count].name.info.hour=(time&0xF800)>>11;
                                        file_dir[fd_count].name.info.month=((date&0x01E0)>>5);
                                        file_dir[fd_count].name.info.year=((date&0xFE00)>>9)+1980;
                                        file_dir[fd_count].name.info.day=date&0x001F;
                                        file_dir[fd_count].name.info.size=(long)((buffer[position-11+31])<<24)+(long)((buffer[position-11+30])<<16)+(long)((buffer[position-11+29])<<8)+(long)(buffer[position-11+28]);
                                        file_dir[fd_count].name.info.first_cluster=(long)((buffer[position-11+21])<<24)+(long)((buffer[position-11+20])<<16)+(long)((buffer[position-11+27])<<8)+(long)(buffer[position-11+26]);
								    }
								    else
								    {
								        if(position==512)
								        {
								            //position=0;
								        }
								        else
								        {
								            position++;
								        }
								    }
								}
							}
						}
						clean_name();
						if(file_dir[fd_count].name.file_dir_name[0]!=0xFF && file_dir[fd_count].name.file_dir_name[0]!=0x00)
						{
							if(file_dir[fd_count].type==IS_DIR)
							{
								printf("%d. (DIR)\t", number);
							}
							else
							{
								printf("%d. (FILE)\t", number);
							}
							char i;
							for(i=0;i<255;i++)
							{
								if(file_dir[fd_count].name.file_dir_name[i]!=0x00)
								{
									printf("%c",file_dir[fd_count].name.file_dir_name[i]);
								}
							}
							printf("\t\t");
							printf("%d/%d/%d	%d:%d\n",file_dir[fd_count].name.info.day,file_dir[fd_count].name.info.month,file_dir[fd_count].name.info.year,file_dir[fd_count].name.info.hour,file_dir[fd_count].name.info.minute);
							fd_count++;
							number++;
						}
					}
					else
					{
						if(position==512)
						{
							//position=0;
						}
						else
						{
							position++;
						}
					}
				}
				else
				{
					if(position==512)
					{
						count--;
					}
					else
					{
						position++;
					}
				}
			} while (position<512);
			position=0;
			sectors_to_be_read--;
		}while(sectors_to_be_read>0);
	}
	send_command(CMD12,0,SSI_number);
	sectors_to_be_read=(next_cluster*4)/512;
	long sector=0;
	if(send_command(CMD18,fat_begin_lba,SSI_number)==0)
	{
		do
		{
			sector++;
			rcvr_datablock(buffer, 512,SSI_number);
		}while(sectors_to_be_read>0);
		sector--;
	}
	send_command(CMD12,0,SSI_number);
	next_cluster=(((long)(buffer[((next_cluster*4)-(sector*512))+3]))<<24)+(((long)(buffer[((next_cluster*4)-(sector*512))+2]))<<16)+(((long)(buffer[((next_cluster*4)-(sector*512))+1]))<<8)+(long)(buffer[((next_cluster*4)-(sector*512))]);
	if((next_cluster==0x0FFFFFFF || next_cluster==0xFFFFFFFF) && current_count<40 && subdirs==GET_SUBDIRS)
	{
		while(current_count<40&&file_dir[current_count].type!=IS_DIR)
		{
			current_count++;
		}
		if(current_count<40 && file_dir[current_count].type==IS_DIR)
		{
			printf("Content of ");
			char i;
			for(i=0;i<255;i++)
			{
				if(file_dir[current_count].name.file_dir_name[i]!=0x00)
				{
					printf("%c",file_dir[current_count].name.file_dir_name[i]);
				}
			}
			next_cluster=file_dir[current_count].name.info.first_cluster;
			current_count++;
			printf("\n\t");
		}
	}
	if(current_count==40)
	{
		number=0;
		next_cluster=0x0FFFFFFF;
	}
	return next_cluster;
}

/*
 *Reads file content.
 *Please note that this method should be modified if the file to be opened is not a txt file (concretely the content inside the for loop)
 */
long open_file(long next_cluster,enum SSI SSI_number)
{

	unsigned char buffer[512];
	long sector=0;
	long sectors_to_be_read=sectors_per_cluster;
	long address=cluster_begin_lba + ((next_cluster - 2) * (unsigned long)sectors_per_cluster);
	if(send_command(CMD18,address,SSI_number)==0)
	{
		do
		{
			rcvr_datablock(buffer, 512,SSI_number);
			int c=0;
			for(c=0;c<512;c++)
			{
				if(buffer[c]!=0x00)
				{
					printf("%c", buffer[c]);
				}
				else
				{
					c=512;
					finish=1;
				}
			}
			sectors_to_be_read--;
		}while(sectors_to_be_read>0 && finish!=1);
	}
	send_command(CMD12,0,SSI_number);
	sectors_to_be_read=(next_cluster*4)/512;
	if(send_command(CMD18,fat_begin_lba,SSI_number)==0)
	{
		do
		{
			sector++;
			rcvr_datablock(buffer, 512,SSI_number);
		}while(sectors_to_be_read>0);
		sector--;
	}
	send_command(CMD12,0,SSI_number);
	next_cluster=(((long)(buffer[((next_cluster*4)-(sector*512))+3]))<<24)+(((long)(buffer[((next_cluster*4)-(sector*512))+2]))<<16)+(((long)(buffer[((next_cluster*4)-(sector*512))+1]))<<8)+(long)(buffer[((next_cluster*4)-(sector*512))]);
	if(next_cluster==0x0FFFFFFF || next_cluster==0x0FFFFFFF)
	{
		finish=0;
	}
	return next_cluster;
}

/*
 * Initialises Timer 5
 */
void Timer5_Init(void)
{

	volatile unsigned short delay;
	SYSCTL_RCGCTIMER_R |= 0x20;
	delay = SYSCTL_SCGCTIMER_R;
	delay = SYSCTL_SCGCTIMER_R;
	TIMER5_CTL_R = 0x00000000;       // 1) disable timer5A during setup
	TIMER5_CFG_R = 0x00000000;       // 2) configure for 32-bit mode
	TIMER5_TAMR_R = 0x00000002;      // 3) configure for periodic mode, default down-count settings
	TIMER5_TAILR_R = 159999;         // 4) reload value, 10 ms, 16 MHz clock
	TIMER5_TAPR_R = 0;               // 5) bus clock resolution
	TIMER5_ICR_R |= 0x00000001;       // 6) clear timer5A timeout flag
	TIMER5_IMR_R |= 0x00000001;       // 7) arm timeout interrupt
	NVIC_PRI23_R = (NVIC_PRI23_R&0xFFFFFF00)|0x00000040; // 8) priority 2
	// interrupts enabled in the main program after all devices initialized
	// vector number 108, interrupt number 92
	NVIC_EN2_R = 0x10000000;         // 9) enable interrupt 92 in NVIC
	TIMER5_CTL_R = 0x00000001;       // 10) enable timer5A
}

// Executed every 10 ms
void Timer5A_Handler(void){
  TIMER5_ICR_R = 0x00000001;       // acknowledge timer5A timeout
  disk_timerproc();
}

void disk_timerproc(void){   /* Decrements timer */
  if(Timer1) Timer1--;
  if(Timer2) Timer2--;
}

/*
 * Receives a block from a read of an SD card
 */
unsigned int rcvr_datablock (
    unsigned char *buff,         /* Data buffer to store received data */
    unsigned int btr, enum SSI SSI_number){          /* Byte count (must be even number) */
	unsigned char token;
  Timer1 = 10;
  do {                            /* Wait for data packet in timeout of 100ms */
    token = sd_read(SSI_number);
  } while ((token == 0xFF) && Timer1);
  if(token != 0xFE) return 0;    /* If not valid data token, retutn with error */

  do {                            /* Receive the data block into buffer */
    rcvr_spi_m(buff++,SSI_number);
    rcvr_spi_m(buff++,SSI_number);
  } while (btr -= 2);
  sd_write(0xFF,SSI_number);                        /* Discard CRC */
  sd_write(0xFF,SSI_number);

  return 1;                    /* Return with success */
}

void rcvr_spi_m(unsigned char *dst,enum SSI SSI_number){
  *dst = sd_read(SSI_number);
}
