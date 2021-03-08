/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stv6120.c                                                                   */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - the stv6120 (nimtuner) specific routines                                                      */
/* Copyright 2019 Heather Lomond                                                                      */
/* 													                                                  */
/* Modified July 2020, Brian Jordan, G4EWJ, for BATC Ryde / WinterHill project	                      */
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

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- INCLUDES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

#include "math.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "nim.h"
#include "stv6120.h"
#include "stv6120_regs.h"
#include "stv6120_utils.h"
#include "errors.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

/* some globals to make it easier to keep track of register contents across function calls */
uint8_t rdiv;
uint8_t ctrl7;
uint8_t ctrl8;
uint8_t ctrl16;
uint8_t ctrl17;

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- CONSTANTS ---------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* from the datasheet: charge pump data */
const uint32_t stv6120_icp_lookup[7][3]={
     /* low     high  icp */
    {2380000, 2472000, 0},
    {2473000, 2700000, 1},  
    {2701000, 3021000, 2},  
    {3022000, 3387000, 3},
    {3388000, 3845000, 5},
    {3846000, 4394000, 6},
    {4395000, 4760000, 7} 
};

/* a lookup table for the cutuff freq of the HF filter in MHz */
const uint16_t stv6120_cfhf[32]={6796, 5828, 4778, 4118, 3513, 3136, 2794, 2562,
                            2331, 2169, 2006, 1890, 1771, 1680, 1586, 1514,
                            1433, 1374, 1310, 1262, 1208, 1167, 1122, 1087,
                            1049, 1018,  983,  956,  926,  902,  875,  854};
 
/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */
/* Initialises one tuner at a time. The other tuner is untouched.                                     */
/*               																					  */
/*   Calling:                          					                                              */
/*   nimreceiver:  		1-4 for dual NIM WinterHill system										  	  */
/*   freq_tuner:  		kHz			0 = disable tuner                                                 */
/*   antenna:  			1 = top, 	2 = bottom				NIM_INPUT_TOP	NIM_INPUT_BOTTOM          */
/*   symbolrate:		to set IQ filter bandwidth (future expansion)                                 */
/*                                  					                                              */
/* return: error code   			                                                                  */
/* -------------------------------------------------------------------------------------------------- */
 
uint8_t stv6120_init (uint32_t nimtuner, uint32_t freq_tuner, uint32_t antenna, uint32_t symbolrate) 
{
    uint8_t     err     = ERROR_NONE ;
    uint8_t     k ;
	uint8_t		temp ;
	uint8_t		temp9 ;
	uint8_t		temp10 ;

    printf ("Flow: Tuner%d init %d antenna%d \r\n",nimtuner,freq_tuner,antenna) ;

// Note: the tuners are set up separately as required, so the other one must not be disturbed

	err = stv6120_read_reg (STV6120_CTRL10,&temp) ;
	printf ("<%02X %02X>\r\n",temp,STV6120_CTRL10_RESET) ; /////////////////
///	if (temp == STV6120_CTRL10_RESET)					// chip has been reset, so set up the common clock
	if (1) //////////////////
	{													// LNA-A and LNA-D will be turned off lower down		

// we calculate K from: F_xtal / (K+16) = 1MHz, as specified in the datasheet

	    k = NIM_TUNER_XTAL / 1000 - 16 ;

// set up the common clock for both tuners (note rdiv is a global) 

	    if (NIM_TUNER_XTAL >= STV6120_RDIV_THRESHOLD) 
    	{
        	rdiv = 1 ;       // from the data sheet
    	}
	    else 
    	{
        	rdiv = 0 ;
    	}	
    
	    if (err == ERROR_NONE) 
    	{
        	err = stv6120_write_reg
	        (
    	        STV6120_CTRL1,
        	    (k                         << STV6120_CTRL1_K_SHIFT)       |
            	(rdiv                      << STV6120_CTRL1_RDIV_SHIFT)    |
	            (STV6120_CTRL1_OSHAPE_SINE << STV6120_CTRL1_OSHAPE_SHIFT)  |
    	        (STV6120_CTRL1_MCLKDIV_4   << STV6120_CTRL1_MCLKDIV_SHIFT) 
        	) ;
        	err = stv6120_write_reg
	        (
    	        STV6120_CTRL1,
        	    (k                         << STV6120_CTRL1_K_SHIFT)       |
            	(rdiv                      << STV6120_CTRL1_RDIV_SHIFT)    |
	            (STV6120_CTRL1_OSHAPE_SINE << STV6120_CTRL1_OSHAPE_SHIFT)  |
    	        (STV6120_CTRL1_MCLKDIV_4   << STV6120_CTRL1_MCLKDIV_SHIFT) 
        	) ;
    	}
    	
// set up the CAL registers
   	
	    if (err == ERROR_NONE) 
    	{
    		err 	= stv6120_read_reg (STV6120_CTRL8,&temp) ;
    		temp   &= ~(STV6120_CTRL8_TCAL_MASK | STV6120_CTRL8_CALTIME_MASK) ;
        	temp   |= (STV6120_CTRL8_TCAL_DIV_2    << STV6120_CTRL8_TCAL_SHIFT) | 
            	      (STV6120_CTRL8_CALTIME_500US << STV6120_CTRL8_CALTIME_SHIFT) ; 	// changed from TCAL_SHIFT
    
	        err 	= stv6120_write_reg (STV6120_CTRL8, temp) ;    	
    	}
    	    	
		if (err == ERROR_NONE) 
		{
			err 	= stv6120_read_reg (STV6120_CTRL17,&temp) ;
    		temp   &= ~STV6120_CTRL8_CALTIME_MASK ;
        	temp   |=  STV6120_CTRL8_CALTIME_500US << STV6120_CTRL8_CALTIME_SHIFT ; 	// changed from TCAL_SHIFT   
	        err 	= stv6120_write_reg (STV6120_CTRL17, temp) ;    	
		}		
    }

 
//	setup the LNAs and path registers
//	LNA-A and LNA-D are not used 
//	LNA-B is fed from the TOP NIM input, LNB-C from the BOTTOM 

	if (nimtuner == TUNER_1)
	{
    	
        if (err == ERROR_NONE) 
        {
            err = stv6120_write_reg
            (
                STV6120_CTRL2,
                (STV6120_CTRL2_DCLOOPOFF_ENABLE  << STV6120_CTRL2_DCLOOPOFF_SHIFT) |
                (STV6120_CTRL2_SDOFF_OFF        << STV6120_CTRL2_SDOFF_SHIFT)      |
                (STV6120_CTRL2_SYN_ON           << STV6120_CTRL2_SYN_SHIFT)        |
                (STV6120_CTRL2_REFOUTSEL_1_25V  << STV6120_CTRL2_REFOUTSEL_SHIFT)  |
                (STV6120_CTRL2_BBGAIN_6DB       << STV6120_CTRL2_BBGAIN_SHIFT)    
            ) ; 
        }    

    	if (symbolrate == 27500 || symbolrate == 22000)
    	{
			temp = STV6120_CTRL7_CF_23MHZ ;
    	}
    	else
    	{
			temp = STV6120_CTRL7_CF_5MHZ ;
    	}
        if (err == ERROR_NONE) 
        {
            ctrl7 	= (STV6120_CTRL7_RCCLKOFF_DISABLE << STV6120_CTRL7_RCCLKOFF_SHIFT) |
                      (temp							  << STV6120_CTRL7_CF_SHIFT) ; 	     		// needs changing for higher SR

           	err 	= stv6120_write_reg (STV6120_CTRL7, ctrl7) ;
        }
        
		err    |= stv6120_read_reg (STV6120_CTRL10,&temp10) ;		// get LNA / path selection register

		if (freq_tuner)
		{
			temp10 |= 1 << STV6120_CTRL10_PATHON_1_SHIFT ;			// enable path 1 (nimtuner 1)
		}
		else
		{
			temp10 &= ~(1 << STV6120_CTRL10_PATHON_1_SHIFT) ;		// disable path 1 (nimtuner 1)
		}
		
		err    |= stv6120_read_reg (STV6120_CTRL9,&temp9) ;			// get the antenna selection register
		temp9  &= ~(3 << STV6120_CTRL9_RFSEL_1_SHIFT) ;				// clear the LNA entry for tuner 1
		temp10 &= ~(1 << STV6120_CTRL10_LNAAON_SHIFT) ;				// LNA-A is not connected, so turn it off
		temp10 &= ~(1 << STV6120_CTRL10_LNADON_SHIFT) ;				// LNA-D is not connected

		if (antenna == NIM_INPUT_TOP)
		{
        	temp9  |= STV6120_CTRL9_RFSEL_RFB_IN << STV6120_CTRL9_RFSEL_1_SHIFT ;
       		temp10 |= 1 << STV6120_CTRL10_LNABON_SHIFT ;			// LNA-B is on
        } 
        else if (antenna == NIM_INPUT_BOTTOM)
        {
        	temp9  |= STV6120_CTRL9_RFSEL_RFC_IN << STV6120_CTRL9_RFSEL_1_SHIFT ;
       		temp10 |= 1 << STV6120_CTRL10_LNACON_SHIFT ;			// LNA-C is on
        }
		err |= stv6120_write_reg (STV6120_CTRL9,temp9) ;			// update the registers
		err |= stv6120_write_reg (STV6120_CTRL10,temp10) ;					
	}
	else if (nimtuner == TUNER_2)
	{
        if (err == ERROR_NONE) 
        {
            err = stv6120_write_reg
            (
                STV6120_CTRL11, 
                (STV6120_CTRL2_DCLOOPOFF_ENABLE << STV6120_CTRL2_DCLOOPOFF_SHIFT) |
                (STV6120_CTRL2_SDOFF_OFF        << STV6120_CTRL2_SDOFF_SHIFT)     |
                (STV6120_CTRL2_SYN_ON           << STV6120_CTRL2_SYN_SHIFT)       |
                (STV6120_CTRL2_REFOUTSEL_1_25V  << STV6120_CTRL2_REFOUTSEL_SHIFT) |
                (STV6120_CTRL2_BBGAIN_6DB       << STV6120_CTRL2_BBGAIN_SHIFT)    
            ) ;
		}

    	if (symbolrate == 27500 || symbolrate == 22000)
    	{
			temp = STV6120_CTRL7_CF_23MHZ ;
    	}
    	else
    	{
			temp = STV6120_CTRL7_CF_5MHZ ;
    	}
        if (err == ERROR_NONE) 
        {
            ctrl16 	= (STV6120_CTRL7_RCCLKOFF_DISABLE << STV6120_CTRL7_RCCLKOFF_SHIFT) |
                      (temp				              << STV6120_CTRL7_CF_SHIFT) ;				// needs changing for higher SR

            err 	= stv6120_write_reg (STV6120_CTRL16, ctrl16) ;
        }

		err    |= stv6120_read_reg (STV6120_CTRL10,&temp10) ; 		// get LNA / path selection register

		if (freq_tuner)
		{
			temp10 |= 1 << STV6120_CTRL10_PATHON_2_SHIFT ;			// enable path 1 (nimtuner 1)
		}
		else
		{
			temp10 &= ~(1 << STV6120_CTRL10_PATHON_2_SHIFT) ;		// disable path 1 (nimtuner 1)
		}

		err	   |= stv6120_read_reg (STV6120_CTRL9,&temp9) ;			// get the antenna selection register
		temp9  &= ~(3 << STV6120_CTRL9_RFSEL_2_SHIFT) ;				// clear the LNA entry for tuner 2
		temp10 &= ~(1 << STV6120_CTRL10_LNAAON_SHIFT) ;				// LNA-A is not connected, so turn it off
		temp10 &= ~(1 << STV6120_CTRL10_LNADON_SHIFT) ;				// LNA-D is not connected

		if (antenna == NIM_INPUT_TOP)
		{
        	temp9  |= STV6120_CTRL9_RFSEL_RFB_IN << STV6120_CTRL9_RFSEL_2_SHIFT ;
       		temp10 |= 1 << STV6120_CTRL10_LNABON_SHIFT ;			// LNA-B is on
        }  
        else if (antenna == NIM_INPUT_BOTTOM)
        {
        	temp9  |= STV6120_CTRL9_RFSEL_RFC_IN << STV6120_CTRL9_RFSEL_2_SHIFT ;
       		temp10 |= 1 << STV6120_CTRL10_LNACON_SHIFT ;			// LNA-C is on
        }
		err |= stv6120_write_reg (STV6120_CTRL9,temp9) ;			// update the registers
		err |= stv6120_write_reg (STV6120_CTRL10,temp10) ;					
	}
	
///	err |= stvvglna_init (antenna,STVVGLNA_ON,0) ;					// turn on the top or bottom LNA	



/* no need to touch the STAT2 status register for now */

/* CTRL18, CTRL19 are test regs so just write in the default */

    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg (STV6120_CTRL18,STV6120_CTRL18_DEFAULT) ;
    }
    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg (STV6120_CTRL19,STV6120_CTRL19_DEFAULT) ;
	}
    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg 
        (
        	STV6120_CTRL20,	(STV6120_CTRL20_VCOAMP_NORMAL << STV6120_CTRL20_VCOAMP_SHIFT) | STV6120_CTRL20_RESERVED 
        ) ; 
    }
    
/* CTRL21, CTRL22 are test regs so leave alone */

    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg (STV6120_CTRL21,STV6120_CTRL21_DEFAULT) ;
    }
    
    if (err == ERROR_NONE) 
    {
        err=stv6120_write_reg (STV6120_CTRL22,STV6120_CTRL22_DEFAULT) ;
    }
    
    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg
        (
            STV6120_CTRL23, (STV6120_CTRL20_VCOAMP_NORMAL << STV6120_CTRL20_VCOAMP_SHIFT) | STV6120_CTRL20_RESERVED 
        ) ;
	}
	        

/* now we can calibrate the lowpass filters and setup the PLLs for each tuner required */

    if ((err == ERROR_NONE) && (freq_tuner > 0)) 
    {
        err = stv6120_cal_lowpass (nimtuner) ;
        if (err == ERROR_NONE) 
        {
            err = stv6120_set_freq (nimtuner, freq_tuner) ;
        }
    }
    
    if (err != ERROR_NONE) 
    {
        printf ("ERROR: Failed to init Tuner%i, %i\r\n",nimtuner, freq_tuner) ;
    }
    
    return (err) ;
}


/* -------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

uint8_t stv6120_cal_lowpass (uint8_t nimtuner) 
{
    uint8_t     err         = ERROR_NONE ;
    uint8_t     val ;
    uint16_t    timeout ;

    printf ("Flow: Tuner cal lowpass\n") ;

/* turn on the clock for the low pass filter. This is in ctrl7/16 so we have a shadow for it */

    if (nimtuner == TUNER_1) 
    {
        err = stv6120_write_reg (STV6120_CTRL7, ctrl7 & ~(1 << STV6120_CTRL7_RCCLKOFF_SHIFT)) ;
    }
    else                
    {
        err = stv6120_write_reg (STV6120_CTRL16, ctrl16 & ~(1 << STV6120_CTRL7_RCCLKOFF_SHIFT)) ;
    }
    
/* now we can do a low pass filter calibration, by setting the CALRCSTRT bit. NOte it is safe to just write to it */

    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg 
        (
            nimtuner == TUNER_1 ? STV6120_STAT1 : STV6120_STAT2,(STV6120_STAT1_CALRCSTRT_START << STV6120_STAT1_CALRCSTRT_SHIFT)
        ) ;
	}
	        
/* wait for the bit to be cleared  to say cal has finished */

    if (err == ERROR_NONE) 
    {
        timeout = 0 ;
        do 
        {
            err = stv6120_read_reg (STV6120_STAT1, &val) ; 
            timeout++ ;
            if (timeout == STV6120_CAL_TIMEOUT) 
            {
                err = ERROR_TUNER_CAL_LOWPASS_TIMEOUT ;
                printf ("ERROR: tuner wait on CAL_lowpass timed out\n") ;
            }
        } while ((err == ERROR_NONE) && ((val & (1<<STV6120_STAT1_CALRCSTRT_SHIFT)) == (1<<STV6120_STAT1_CALRCSTRT_SHIFT))) ;
    }

/* turn off the low pass filter clock (=1) */

    if (err == ERROR_NONE) 
    {
        err=stv6120_write_reg (nimtuner == TUNER_1 ? STV6120_CTRL7 : STV6120_CTRL16,(nimtuner==TUNER_1 ? ctrl7 : ctrl16)) ;
    }
    
    if (err != ERROR_NONE) 
    {
        printf ("ERROR: Failed to cal lowpass filter\n") ;
    }
    
    return (err) ;
}

/* -------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */
/* Sets one of the tuners to the given frequency                                                      */
/*                                                                                                    */
/*   nimtuner: 	TUNER_1  |  TUNER_2 : which tuner we are going to work on                             */
/*    	 freq: 	the frequency to set the tuner to in KHz                                              */
/*     return: 	error code                                                                            */
/*                                                                                                    */
/* when locked,  F.lo = F.vco/P = (F.xtal/R) * (N+F/2^18)/P                                           */
/* -------------------------------------------------------------------------------------------------- */

uint8_t stv6120_set_freq (uint8_t nimtuner, uint32_t freq) 
{
    uint8_t     err = ERROR_NONE ;
    uint8_t     val ;
    uint8_t     pos ;
    uint32_t    f ;
    uint16_t    n ;
    uint8_t     p ;
    uint8_t     icp ;
    uint32_t    f_vco ;
    uint16_t    timeout ;
    uint8_t     cfhf ;

    printf ("Flow: Tuner set freq\n");

/* the global rdiv has already been set up in the init routines */

/* p is defined from the datasheet (note, this is reg value, not P) */
    
    if (freq <= STV6120_P_THRESHOLD_1) 
    {
        p = 3 ;                             /* P = 16 */
    }
    else if (freq <= STV6120_P_THRESHOLD_2) 
    {
        p = 2 ;                             /* P = 8 */
    }
    else if (freq <= STV6120_P_THRESHOLD_3) 
    {
        p = 1 ;                             /* P = 4 */
    }
    else
    {
        p = 0 ;                             /* P = 2 */
    }
    
/* we have to be careful of the size of the typesi in the following  */
/* F.vco=F.rf*P where F.rf=F.lo    all in KHz */
/* f_vco is uint32_t, so p_max is 3 (i.e P_max is 16), freq_max is 2500000KHz, results is 0x02625a00 ... OK */

    f_vco = freq << (p + 1) ;

/* n=integer(f_vco/f_xtal*R)  note: f_xtal and f_vco both in KHz */
/* we do the *R first (a shift by rdiv), and max is 0x04c4b400, then the divide and we are OK */

    n = (uint16_t)((f_vco << rdiv) / NIM_TUNER_XTAL) ;

/* f = fraction(f_vco/f_xtal*R).2^18 */
/* as for n, we do the shift first (which we know is safe), then modulus to get the fraction */
/* then we have to go to 64 bits to do the shift and divide, and then back to uint32_t for the result */

    f = (uint32_t)(( ((uint64_t)((f_vco << rdiv) % NIM_TUNER_XTAL)) << 18) / NIM_TUNER_XTAL);
    
/* lookup the ICP value in the lookup table as per datasheet */

    pos = 0 ;
    while (f_vco > stv6120_icp_lookup[pos++][1]) ;
    icp = stv6120_icp_lookup [pos-1] [2] ;

/* lookup the high freq filter cutoff setting as per datasheet */

    cfhf = 0 ;
    while ((3 * freq / 1000) <= stv6120_cfhf[cfhf]) 
    {
        cfhf++ ;
    }
    cfhf-- ;    /* we are sure it isn't greater then the first array element so this is safe */

    printf ("      Status: tuner:%i, f_vco=0x%x, icp=0x%x, f=0x%x, n=0x%x,\n",nimtuner, f_vco, icp, f, n) ;
    printf ("              rdiv=0x%x, p=0x%x, freq=%i, cfhf=%i\n", rdiv, p, freq, stv6120_cfhf[cfhf]) ;

/* now we fill in the PLL and ICP values */
    
    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg 
        (
            nimtuner == TUNER_1 ? STV6120_CTRL3 : STV6120_CTRL12, (n & 0x00ff)       /* set N[7:0] */
        ) ;
    }
    if (err == ERROR_NONE) /* set F[6:0] */
    {
        err = stv6120_write_reg 
        (
            nimtuner == TUNER_1 ? STV6120_CTRL4 : STV6120_CTRL13, ((f & 0x0000007f) << 1) | ((n & 0x0100) >> 8)     /* N[8] */
        ) ;
    }
    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg 
        (
            nimtuner == TUNER_1 ? STV6120_CTRL5 : STV6120_CTRL14, ((f & 0x00007f80) >> 7)            /* set F[14:7] */
        ) ;
    }
    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg
        (
            nimtuner == TUNER_1 ? STV6120_CTRL6 : STV6120_CTRL15, 
            (
            	((f & 0x00038000) >> 15)   |       									/* set f[17:15] */
                (icp << STV6120_CTRL6_ICP_SHIFT) |                 					/* ICP[2:0] */
               	STV6120_CTRL6_RESERVED                             					/* reserved bit */
            )
        ) ;
    }
    if (nimtuner == TUNER_1) 
    {
        if (err == ERROR_NONE) 
        {
            err = stv6120_write_reg (STV6120_CTRL7, (p<<STV6120_CTRL7_PDIV_SHIFT) | ctrl7) ; /* put back in RCCLKOFF_1 as well */
        }
        
        if (err == ERROR_NONE) 
        {
            err=stv6120_write_reg (STV6120_CTRL8, (cfhf << STV6120_CTRL8_CFHF_SHIFT) | ctrl8) ;
        }
        
    } 
    else    /* tuner = TUNER_2 */ 
    {
        if (err == ERROR_NONE) 
        {
            err = stv6120_write_reg (STV6120_CTRL16, (p<<STV6120_CTRL7_PDIV_SHIFT) | ctrl16) ; /* put back in RCCLKOFF_2 as well */
        }
        if (err == ERROR_NONE) 
        {
            err = stv6120_write_reg (STV6120_CTRL17, (cfhf << STV6120_CTRL8_CFHF_SHIFT) | ctrl17) ;
        }
    }

/* if we change the filter re-cal it, and if we change VCO we have to re-cal it, so here goes */

    if (err == ERROR_NONE) 
    {
        err = stv6120_write_reg
        (
            nimtuner == TUNER_1 ? STV6120_STAT1 : STV6120_STAT2,                	/* start CALVCOSTRT */      
            (STV6120_STAT1_CALVCOSTRT_START << STV6120_STAT1_CALVCOSTRT_SHIFT) | 
            STV6120_STAT1_RESERVED           
        ) ;
    }
        
/* wait for CALVCOSTRT bit to go low to say VCO cal is finished */

    if (err == ERROR_NONE) 
    {
        timeout = 0 ;
        do 
        {
            err = stv6120_read_reg (nimtuner==TUNER_1 ? STV6120_STAT1 : STV6120_STAT2, &val) ;
            timeout++;
        } 
        while 
        (
        	(err == ERROR_NONE) &&
            (timeout< STV6120_CAL_TIMEOUT) && 
            ((val & (1 << STV6120_STAT1_CALVCOSTRT_SHIFT)) != (STV6120_STAT1_CALVCOSTRT_FINISHED << STV6120_STAT1_CALVCOSTRT_SHIFT))
		) ;
		  	
        if ((err == ERROR_NONE) && (timeout == STV6120_CAL_TIMEOUT)) 
        {
            printf ("ERROR: tuner wait on CAL timed out\n") ;
            err = ERROR_TUNER_CAL_TIMEOUT ;
        }
    }

/* wait for LOCK bit to go high to say PLL is locked */

    if (err == ERROR_NONE) 
    {
        timeout=0 ;
        do 
        {
            err = stv6120_read_reg (nimtuner==TUNER_1 ? STV6120_STAT1 : STV6120_STAT2, &val) ;
            timeout++;
        } 
        while 
        (
        	(err == ERROR_NONE) &&
            (timeout<STV6120_CAL_TIMEOUT) &&
            ((val & (1<<STV6120_STAT1_LOCK_SHIFT)) != (STV6120_STAT1_LOCK_LOCKED << STV6120_STAT1_LOCK_SHIFT)) 
		) ;
		                 
        if ((err == ERROR_NONE) && (timeout == STV6120_CAL_TIMEOUT)) 
        {
            printf ("ERROR: tuner wait on lock timed out\n");
            err = ERROR_TUNER_LOCK_TIMEOUT;
        }
    }

    if (err != ERROR_NONE) 
    {
        printf ("ERROR: Tuner set freq %i\n",freq) ;
    }
    
    return (err) ;
}


/* -------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */
/* debug routine to print out all the regsister values in the tuner                                    */
/* -------------------------------------------------------------------------------------------------- */

void stv6120_print_settings() 
{
    uint8_t     val ;
    uint8_t     i ;

/*    
    printf ("Tuner regs are:\n") ;
    for (i = 0 ; i < 0x19 ; i++) 
    {
        stv6120_read_reg (i, &val) ;
        printf (" 0x%.2x = 0x%.2x\n", i, val) ;
    }
*/

    printf ("Tuner regs are: ") ;
    for (i = 0 ; i < 0x19 ; i++) 
    {
        printf ("%02X ", i) ;
    }
    printf ("\r\n") ;
    
    printf ("                ") ;
    for (i = 0 ; i < 0x19 ; i++) 
    {
        stv6120_read_reg (i, &val) ;
        printf ("%02X ", val) ;
    }
    printf ("\r\n") ;
    
}

