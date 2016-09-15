/*
 * main.c
 */
#include "sdcard.h"
#include <stdio.h>

int main(void)
{
	startSSI3();
	initialize_sd(SSI3);
	cs_high(SSI3);
	change_speed(SSI3);
	cs_low(SSI3);
	read_first_sector(SSI3);
	read_disk_data(SSI3);
	long next_cluster=get_root_dir_first_cluster();
	do
	{
		next_cluster=list_dirs_and_files(next_cluster,LONG_NAME,GET_SUBDIRS,SSI3);
	}while(next_cluster!=0x0FFFFFFF && next_cluster!=0xFFFFFFFF);
	printf("\nDirs and files listed\n\n");
	next_cluster=get_first_cluster(5);
	do
	{
		next_cluster=open_file(next_cluster,SSI3);
	}while(next_cluster!=0x0FFFFFFF && next_cluster!=0xFFFFFFFF);

	return 0;
}
