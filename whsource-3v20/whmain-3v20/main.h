/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: main.h                                                                      */
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
*/

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

/* states of the main loop state machine */
/* for WinterHill, change these to match the value in HEADER_MODE stv0910:Px_DMDSTATE */

#define STATE_DEMOD_HUNTING      	0
#define STATE_SEARCH			 	STATE_DEMOD_HUNTING
#define STATE_DEMOD_FOUND_HEADER 	1
#define STATE_HEADER_S2			 	STATE_DEMOD_FOUND_HEADER
#define STATE_DEMOD_S2           	2
#define STATE_DEMOD_S            	3
#define STATE_LOST				 	0x80							
#define STATE_TIMEOUT				0x81							// TS leaving the building has timed out
#define STATE_IDLE					0x82							// not searching for nor receiving a signal

/* define the various status reports */

#define STATUS_STATE               1
#define STATUS_LNA_GAIN            2
#define STATUS_PUNCTURE_RATE       3
#define STATUS_POWER_I             4 
#define STATUS_POWER_Q             5
#define STATUS_CARRIER_FREQUENCY   6
#define STATUS_CONSTELLATION_I     7
#define STATUS_CONSTELLATION_Q     8
#define STATUS_SYMBOL_RATE         9
#define STATUS_VITERBI_ERROR_RATE 10
#define STATUS_BER                11
#define STATUS_MER                12
#define STATUS_SERVICE_NAME       13		// usually callsign
#define STATUS_SERVICE_PROVIDER_NAME  14
#define STATUS_TS_NULL_PERCENTAGE 15
#define STATUS_ES_PID             16
#define STATUS_ES_TYPE            17
#define STATUS_MODCOD             18
#define STATUS_FRAME_TYPE         19
#define STATUS_PILOTS             20
#define STATUS_ERRORS_LDPC_COUNT  21
#define STATUS_ERRORS_BCH_COUNT   22
#define STATUS_ERRORS_BCH_UNCORRECTED   23
#define STATUS_LNB_SUPPLY         24
#define STATUS_LNB_POLARISATION_H 25


// new statuses added for WinterHill

#define STATUS_MULTISTREAM0   	  26		// B3:B2:B1:B0 = PDELCTR1,PDELCTRL0,MATSTR1,MATSTR0  
#define STATUS_MULTISTREAM1   	  27		//       B1:B0 = ISIBITENA,ISIENTRY
#define STATUS_DEBUG0			  28		//		 B1:B0 = TSSTATUS2,TSSTATUS 
#define STATUS_DEBUG1			  29		//		 B1:B0 = TSSTATUS,TSSTATUS
#define STATUS_DNUMBER			  30		// MER value - MER decode threshold (can be negative)		
#define STATUS_VIDEO_TYPE		  31		// 0x02=MPEG2, 0x1b=H264, 0x24=H265	
#define STATUS_ROLLOFF			  32		// 0,1,2,3 = 0.35,0.25,0.20,0.15	
#define STATUS_ANTENNA			  33		// 1:2 = TOP:BOT
#define STATUS_AUDIO_TYPE		  34		// 0x03=MP2, 0x0f=AAC	

#define STATUS_VLCSTOPS		  	  96		// STOP commands sent by the WH main application
#define STATUS_VLCNEXTS			  97		// NEXT commands sent by the WH main application
#define STATUS_MODECHANGES		  98		// increments on a new command, new callsign, new codec
#define STATUS_IPCHANGES		  99		// incremented when a command comes from a different IP address

/* The number of constellation peeks we do for each background loop */

void config_set_frequency(uint32_t frequency);
void config_set_symbolrate(uint32_t symbolrate);
void config_set_frequency_and_symbolrate(uint32_t frequency, uint32_t symbolrate);
void config_set_lnbv(bool enabled, bool horizontal);

#endif

