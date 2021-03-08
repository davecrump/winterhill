/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: ftdi.h                                                                      */
/* Copyright 2019 Heather Lomond                                                                      */
/* -------------------------------------------------------------------------------------------------- */
/*
    This file is part of longmynd.

    Longmynd is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Longmynd is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with longmynd.  If not, see <https://www.gnu.org/licenses/>.

    WinterHill December 2020:

    Parts modified for the BATC WinterHill 4 channel receiver project - terms as above
    Copyright Brian Jordan, G4EWJ
*/

// For WinterHill, i2c is handled by the RPi

#ifndef RPI2C_H
	#define RPI2C_H

	#include <linux/i2c-dev.h>

	#define I2CNAME				"/dev/i2c-1"

// i2c addresses

	#define NIM_DEMOD_ADDR_A    0xd0
	#define NIM_DEMOD_ADDR_B    0xd2
	#define NIM_DEMOD_ADDR_C    0xd4
	#define NIM_DEMOD_ADDR_D    0xd6

	#define NIM_DEMOD_ADDR_0    0xd0
	#define NIM_DEMOD_ADDR_1    0xd2
	#define NIM_DEMOD_ADDR_2    0xd4
	#define NIM_DEMOD_ADDR_3    0xd6
	
	#define PIC_ADDR_A			0xd8
	#define PIC_ADDR_B			0xda
	#define PIC_ADDR_C			0xdc
	#define PIC_ADDR_D			0xde
	
	#define PIC_ADDR_0			0xd8
	#define PIC_ADDR_1			0xda
	#define PIC_ADDR_2			0xdc
	#define PIC_ADDR_3			0xde
	
// the following address are common to all NIMs, so only the required STV0910 repeater should be enabled

	#define NIM_TUNER_ADDR 		0xc0
	
	#define STVVGLNA_I2C_ADDR0 	0xc8
	#define STVVGLNA_I2C_ADDR1 	0xca
	#define STVVGLNA_I2C_ADDR2 	0xcc
	#define STVVGLNA_I2C_ADDR3 	0xce
	
	#define NIM_LNA_0_ADDR 		STVVGLNA_I2C_ADDR3
	#define NIM_LNA_1_ADDR 		STVVGLNA_I2C_ADDR0
	
	uint8_t i2c_read_reg8 		(uint8_t, uint8_t   reg, uint8_t*) ;
	uint8_t i2c_read_reg16 		(uint8_t, uint16_t  reg, uint8_t*) ;
	uint8_t i2c_write_reg8 		(uint8_t, uint8_t   reg, uint8_t)  ;
	uint8_t i2c_write_reg16		(uint8_t, uint16_t  reg, uint8_t)  ;

	uint8_t i2c_write_pic16		(uint8_t, uint8_t   reg, uint8_t, uint8_t)  ;
	uint8_t i2c_write_pic8		(uint8_t, uint8_t,  uint8_t) ;
	uint8_t i2c_read_pic8		(uint8_t, uint8_t, 	uint8_t*) ;

#endif

