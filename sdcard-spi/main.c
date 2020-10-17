/*
 * main.c
 */
#include "sdcard.h"
#include <stdio.h>

int main(void)
{
    SysTick_Init();
	startSSI3();

	//Before initialising the SD card, check the CD (Card Detection) pin if available
	/*GPIO_PORTD_DIR_R &= ~0x40; //Set PD6 as input
	uint8_t continue_prog=0;
	while(continue_prog == 0)
	{
	    char read = GPIO_PORTD_DATA_R&0x40;
	    if(read == 0x00)
	    {
	        continue_prog=1;
	    }
	};*/

	SysTick_Wait50ms(40);
	initialise_sd(SSI3);
	cs_high(SSI3);
	change_speed(SSI3);
	cs_low(SSI3);
	read_first_sector(SSI3);
	read_disk_data(SSI3);
	long next_cluster=get_root_dir_first_cluster();
	do
	{
		next_cluster=get_files_and_dirs(next_cluster,LONG_NAME,GET_SUBDIRS,SSI3);
	}while(next_cluster!=0x0FFFFFFF && next_cluster!=0xFFFFFFFF);
	printf("\nDirs and files listed\n\n");
	next_cluster=get_first_cluster(2);
	do
	{
		next_cluster=open_file(next_cluster,SSI3);
	}while(next_cluster!=0x0FFFFFFF && next_cluster!=0xFFFFFFFF);

	return 0;
}

void SysTick_Wait(unsigned long delay){
  NVIC_ST_RELOAD_R = delay-1;  // number of counts to wait
  NVIC_ST_CURRENT_R = 0;       // any value written to CURRENT clears
  while((NVIC_ST_CTRL_R&0x00010000)==0){ // wait for count flag
  }
}


void SysTick_Wait50ms(unsigned long delay){
  unsigned long i;
  for(i=0; i<delay; i++){
    SysTick_Wait(800000);  // wait 50ms
  }
}


void SysTick_Init(void){
  NVIC_ST_CTRL_R = 0;               // disable SysTick during setup
  NVIC_ST_CTRL_R = 0x00000005;      // enable SysTick with core clock
}
