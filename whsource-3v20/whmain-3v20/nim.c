/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: nim.c                                                                       */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - handlers for the nim module itself                                                            */
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

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- INCLUDES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

#include "globals.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS and constants ---------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/*
	GLOBALNIM is a globlal variable which the I2C routines use to access the correct NIM (1/2)
*/

/* 
	The tuner and external LNAs are accessed by turning on the I2C bus repeater in the demodulator.
	This reduces the noise on the tuner and external lna I2C lines as they are inactive when the 
	repeater is turned off. We need to keep track of this when we access the NIM.

	For WinterHill:
	
	Although the demodulators in the two NIMs are on different I2C addresses (D0,D2),
	the tuners and external LNAs have similar addresses.
	Only one NIM should have its repeater turned on when tuners and external LNAs are accessed.
	
	Each demodulator in the NIM has a repeater, but only the P1 repeater is used (P1_I2CRPT). 
	
	The I2C peripheral (/dev/i2c-1) on the Raspberry Pi controls the NIMs. The I2C routines 
	have been moved here and modified, so ftdi.c is no longer required.
	
*/
   
extern	uint32_t	GLOBALNIM ;
extern  uint8_t 	NIMI2CADDRESS [5] ;

		bool 		repeater_on   [5]		= {false,false,false,false,false} ;	// ..... NIM_A NIM_B NIM_C NIM_D 
		bool	    nimspresent   [5] 		= {false,false,false,false,false} ;    
     	bool      	xlnaspresent  [5]       = {false,false,false,false,false} ; 
                                                   
    
/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------------------------------- */
/* software initialises one nim (at the highest level)                                                */
/* GLOBALNIM is set by the calling routine to select the NIM                                          */
/* the NIMs are hardware reset by the PICs                                                            */
/* check for the presence of the NIMs by writing and reading a register                               */
/* check for presence of external (to the STV6120) LNAs                                               */
/*                                                                                                    */
/* return: 	bits15:0 	  = NIM type; 4334, 4335                                                      */
/*			bits31:16   1 = presence of external LNAs                                                 */
/*                                                                                                    */
/* -------------------------------------------------------------------------------------------------- */

int32_t nim_init (uint8_t* nimexists, uint8_t* xlnaexists, uint8_t* chipid0910, uint8_t* devid0910, uint8_t* id6120) 
{
    uint8_t 	err		= ERROR_NONE ;
    uint8_t 	val ;
    uint8_t 	tempc ;
	bool		tempo ;
	bool		tempo2 ;


///    printf("Flow: NIM init\r\n") ;

    printf ("============================================================================================\r\n") ;
   	printf ("Checking for presence of NIM_%c    (errors may occur during checking)\r\n",'@'+GLOBALNIM) ;
    printf ("============================================================================================\r\n") ;

/* check we can read and write a register */
/* check to see if we can write and readback to a demod register */

    if (err == ERROR_NONE) err = nim_write_demod (RSTV0910_P1_VTH34, 0xaa); 	/* random reg with alternating bits */
    if (err == ERROR_NONE) err = nim_read_demod  (RSTV0910_P1_VTH34, &val);
    if (err == ERROR_NONE) 
	{
        if (val != 0xaa) 													/* check alternating bits ok */
		{ 
            printf ("ERROR: nim_init read/write test failed\r\n") ;
            err = ERROR_NIM_INIT ;
        }
        else
		{
			*nimexists = 1 ;
			repeater_on 		[GLOBALNIM] = true ;			// set it on logically
			nim_set_stv0910_repeaters 	(false) ;						// . . so that it will be turned off physically
			
			if (err == ERROR_NONE)	err	= nim_read_demod (RSTV0910_MID, 	chipid0910) ;
			if (err == ERROR_NONE)	err = nim_read_demod (RSTV0910_DID, 	devid0910) ;
			if (err == ERROR_NONE)	err = nim_read_tuner (STV6120_CTRL10, 	&tempc) ;
			*id6120 	= tempc >> 6 ;
		
			stvvglna_init (NIM_INPUT_TOP, 1, &tempo) ;			// check for external LNA			
			stvvglna_init (NIM_INPUT_BOTTOM, 1, &tempo2) ;		// check for external LNA		
			*xlnaexists = (tempo << 1) | tempo2 ;
		
			stv0910_init() ;									// set up the demodulators			
		}
	}
	
    if (err != ERROR_NONE) printf ("ERROR: nim_init\r\n") ;
    return (err) ;
}


/* -------------------------------------------------------------------------------------------------- */
/* reads a demodulator register and takes care of the i2c bus repeater                                */
/*    reg: which demod register to read                                                               */
/*    val: where to put the result                                                                    */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */

uint8_t nim_read_demod (uint16_t reg, uint8_t *val) {

    uint8_t 	err	= ERROR_NONE ;


	nim_set_stv0910_repeaters (false) ;													// turn off both repeaters
	
    if (err == ERROR_NONE) err = i2c_read_reg16 (NIMI2CADDRESS[GLOBALNIM],reg,val) ;
    if (err != ERROR_NONE) printf ("ERROR: demod read 0x%.4x\r\n",reg) ;

    return (err) ;
}

/* -------------------------------------------------------------------------------------------------- */
/* writes to a demodulator register and takes care of the i2c bus repeater                            */
/*    reg: which demod register to write to                                                           */
/*    val: what to write to it                                                                        */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */

uint8_t nim_write_demod (uint16_t reg, uint8_t val) 
{
    uint8_t 	err = ERROR_NONE ;


	nim_set_stv0910_repeaters (false) ;													// turn off both repeaters
	
    if (err == ERROR_NONE) err = i2c_write_reg16 (NIMI2CADDRESS[GLOBALNIM],reg,val) ;
    if (err != ERROR_NONE) printf ("ERROR: demod write 0x%.4x, 0x%.2x\n",reg,val) ;

    return (err) ;
}


/* -------------------------------------------------------------------------------------------------- */
/* reads from the specified external lna taking care of the i2c bus repeater                          */
/*  xlna_addr: i2c address of the external lna to access                                              */
/*        reg: which lna register to read                                                             */
/*        val: where to put the result                                                                */
/*     return: error code                                                                             */
/* -------------------------------------------------------------------------------------------------- */

uint8_t nim_read_xlna (uint8_t xlna_addr, uint8_t reg, uint8_t *val) 
{
    uint8_t err = ERROR_NONE ;


	nim_set_stv0910_repeaters (true) ;									

    if (err == ERROR_NONE) err = i2c_read_reg8 (xlna_addr,reg,val) ;
    if (err != ERROR_NONE) printf ("ERROR: xlna read 0x%.2x, 0x%.2x\r\n",xlna_addr,reg) ;

    return (err) ;
}

/* -------------------------------------------------------------------------------------------------- */
/* writes to the specified external lna taking care of the i2c bus repeater                           */
/*  xlna_addr: i2c address of the lna to access                                                       */
/*        reg: which xlna register to write to                                                        */
/*        val: what to write to it                                                                    */
/*     return: error code                                                                             */
/* -------------------------------------------------------------------------------------------------- */

uint8_t nim_write_xlna (uint8_t xlna_addr, uint8_t reg, uint8_t val) 
{
    uint8_t err = ERROR_NONE ;


	nim_set_stv0910_repeaters (true) ;									

    if (err == ERROR_NONE) err = i2c_write_reg8 (xlna_addr,reg,val) ;
    if (err != ERROR_NONE) printf ("ERROR: lna write 0x%.2x, 0x%.2x,0x%.2x\r\n",xlna_addr,reg,val) ;

    return (err) ;
}

/* -------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */
/* reads from the stv6120 (tuner) taking care of the i2c bus repeater                                 */
/*    reg: which tuner register to read from                                                          */
/*    val: where to put the result                                                                    */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */

uint8_t nim_read_tuner (uint8_t reg, uint8_t *val) 
{
    uint8_t err = ERROR_NONE ;


	nim_set_stv0910_repeaters (true) ;									

    if (err == ERROR_NONE) err = i2c_read_reg8 (NIM_TUNER_ADDR,reg,val) ;
    if (err != ERROR_NONE) printf ("ERROR: tuner read 0x%.2x\r\n",reg) ;

    return (err) ;
}


uint8_t nim_write_tuner (uint8_t reg, uint8_t val) 
{
    uint8_t err = ERROR_NONE ;


	nim_set_stv0910_repeaters (true) ;									

    if (err==ERROR_NONE) err = i2c_write_reg8(NIM_TUNER_ADDR,reg,val) ;
    if (err!=ERROR_NONE) printf ("ERROR: tuner write %i,%i\r\n",reg,val) ;

    return (err) ;
}


// only the repeater for the required NIM should be active when writing to tuners or xlnas
// only the P1 repeater is used in each NIM

uint8_t nim_set_stv0910_repeaters (bool state)
{
    uint8_t 	err = ERROR_NONE ;
	uint8_t		nimsave ;


	nimsave = GLOBALNIM ;								// save the current NIM
	
	if (state == false)									// turn off both repeaters
	{
		GLOBALNIM = NIM_A ;
		if (nimspresent[GLOBALNIM] == true)
		{
			if (repeater_on [GLOBALNIM] == true)
			{
    		    repeater_on [GLOBALNIM] = false ;
	    		err = i2c_write_reg16 (NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_OFF) ;
		    	if (err != ERROR_NONE) 
		    	{
		    		printf 
		    		(
		    			"ERROR: demod write 0x%.2x, 0x%.4x, 0x%.2x\r\n", 
		    			NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_OFF 
		    		) ;
				}
			}
		}
			
		GLOBALNIM = NIM_B ;
		if (nimspresent[GLOBALNIM] == true)
		{
			if (repeater_on [GLOBALNIM] == true)
			{
    		    repeater_on [GLOBALNIM] = false ;
		    	err = i2c_write_reg16 (NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_OFF) ;
		    	if (err != ERROR_NONE) 
		    	{
		    		printf 
		    		(
		    			"ERROR: demod write 0x%.2x, 0x%.4x, 0x%.2x\r\n", 
		    			NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_OFF 
		    		) ;
				}
			}
		}
	}
	else												// turn on the required repeater and turn off the other
	{
		if (repeater_on [GLOBALNIM] == false)
		{
    	    repeater_on [GLOBALNIM] = true ;
	    	err = i2c_write_reg16 (NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_ON) ;
	    	if (err != ERROR_NONE) 
	    	{
	    		printf 
	    		(
	    			"ERROR: demod write 0x%.2x, 0x%.4x, 0x%.2x\r\n", 
	    			NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_ON 
	    		) ;
			}
		}
		
		if (GLOBALNIM == NIM_A)
		{
			GLOBALNIM = NIM_B ;
		}    	    
		else if (GLOBALNIM == NIM_B)
		{
			GLOBALNIM = NIM_A ;
		}    	    

		if (nimspresent[GLOBALNIM] == true)
		{
			if (repeater_on [GLOBALNIM] == true)
			{
    	    	repeater_on [GLOBALNIM] = false ;
	    		err = i2c_write_reg16 (NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_OFF) ;
		    	if (err != ERROR_NONE) 
		    	{
		    		printf 
		    		(
		    			"ERROR: demod write 0x%.2x, 0x%.4x, 0x%.2x\r\n", 
		    			NIMI2CADDRESS[GLOBALNIM], RSTV0910_P1_I2CRPT, REPEATER_OFF 
		    		) ;
				}
			}
		}
	}
	GLOBALNIM = nimsave ;								// restore the current NIM
	return (err) ;
}
	





