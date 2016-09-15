/*
 * sdcard.h
 *
 *  Created on: 15/09/2016
 *  Author: Javier Martínez Arrieta
 *  Version: 1.0
 *  This is part of the sdcard library, with functions that will allow you to read and (in the future) write in an SD card formatted using FAT32 (single partition).
 */

 /*  Copyright (C) 2016 Javier Martínez Arrieta
 *
 *  This project is licensed under Creative Commons Attribution-Non Commercial-Share Alike 4.0 International (CC BY-NC-SA 4.0). According to this license you are free to:
 *  Share — copy and redistribute the material in any medium or format.
 *  Adapt — remix, transform, and build upon the material.
 *  The licensor cannot revoke these freedoms as long as you follow the license terms.
 *	Complete information about this license can be found at: https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode
 *
 */

#include "inc/tm4c123gh6pm.h"
#include <stdint.h>


/* Definitions for MMC/SDC commands */
#define CMD0     0x40    	/* GO_IDLE_STATE */
#define CMD1     0x41    	/* SEND_OP_COND */
#define CMD8     0x48    	/* SEND_IF_COND */
#define CMD9     0x49    	/* SEND_CSD */
#define CMD10    0x4A   	/* SEND_CID */
#define CMD12    0x4C    	/* STOP_TRANSMISSION */
#define CMD16    0x50    	/* SET_BLOCKLEN */
#define CMD17    0x51    	/* READ_SINGLE_BLOCK */
#define CMD18    0x52    	/* READ_MULTIPLE_BLOCK */
#define CMD23    0x57    	/* SET_BLOCK_COUNT */
#define CMD24    0x58    	/* WRITE_BLOCK */
#define CMD25    0x59    	/* WRITE_MULTIPLE_BLOCK */
#define CMD41    0x69    	/* SEND_OP_COND (ACMD) */
#define CMD55    0x77    	/* APP_CMD */
#define CMD58    0x7A    	/* READ_OCR */

enum typeOfWrite{
  COMMAND,                              // the transmission is an LCD command
  DATA                                  // the transmission is data
};

enum SSI{
	SSI0,
	SSI1,
	SSI2,
	SSI3
};

enum name_type{
	SHORT_NAME,
	LONG_NAME
};

enum get_subdirs{
	GET_SUBDIRS,
	NO_SUBDIRS
};

void startSSI0(void);
void startSSI1(void);
void startSSI2(void);
void startSSI3(void);
void sd_write(char message,enum SSI);
unsigned char sd_read(enum SSI);
void initialize_sd(enum SSI);
void cs_high(enum SSI);
void cs_low(enum SSI);
void dummy_clock(enum SSI);
void tx_high(enum SSI);
void Timer5_Init(void);
void change_speed(enum SSI);
long open_file(long next_cluster,enum SSI);
long get_root_dir_first_cluster(void);
long get_first_cluster(int pos);
long list_dirs_and_files(long next_cluster,enum name_type name, enum get_subdirs subdirs, enum SSI SSI_number);
void tx_SSI(enum SSI);
void clean_name(void);
void disk_timerproc(void);

