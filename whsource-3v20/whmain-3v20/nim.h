/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: nim.h                                                                       */
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

#ifndef NIM_H
	#define NIM_H

	#include "stvvglna.h"
	#include <stdint.h>

	#define NIM_A					1
	#define NIM_B					2
	#define NIM_C					3
	#define NIM_D					4

	#define NIM_TUNER_XTAL 			30000 /* in KHz */
	#define NIM_DEMOD_MCLK 			135000000 /* in Hz */

	#define NIM_INPUT_TOP    		1
	#define NIM_INPUT_BOTTOM 		2

	#define NIM_ANT_TOP	    		1
	#define NIM_ANT_BOT		 		2

	#define REPEATER_OFF			0x38
	#define REPEATER_ON				0xb8

	int32_t		nim_init					(uint8_t*, uint8_t*, uint8_t*, uint8_t*, uint8_t*) ;
	uint8_t 	nim_read_tuner 				(uint8_t,  uint8_t*);
	uint8_t 	nim_write_tuner				(uint8_t,  uint8_t );
	uint8_t 	nim_read_demod 				(uint16_t, uint8_t*);
	uint8_t 	nim_write_demod				(uint16_t, uint8_t );
	uint8_t 	nim_read_xlna  				(uint8_t,  uint8_t, uint8_t*);
	uint8_t 	nim_write_xlna 				(uint8_t,  uint8_t, uint8_t );
	uint8_t 	nim_set_stv0910_repeaters 	(bool) ;
#endif

