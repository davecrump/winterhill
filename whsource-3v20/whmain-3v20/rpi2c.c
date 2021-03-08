/* -------------------------------------------------------------------------------------------------- */
/*																									  */
/* BATC WinterHill project																			  */
/* This file, rpi2c.c, was originally ftdi.c, Copyright 2019 Heather Lomond                           */
/*																									  */
/* i2c is now controlled by the Raspberry Pi /dev/i2c-1 											  */
/* References to USB have been removed																  */		
/* The transport streams are conveyed by PICs to SPI ports on a Raspberry Pi     	          		  */
/* The PICs are also on the i2c bus        															  */
/*																									  */
/* -------------------------------------------------------------------------------------------------- */
/*
    This file is part of longmynd

    Longmynd is software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License 
    as published by the Free Software Foundation, either version 3 of the 
    License, or (at your option) any later version.

    Longmynd is distributed in the hope that it will 
    be useful, but WITHOUT ANY WARRANTY; without even the implied 
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along 
    with longmynd.  If not, see <https://www.gnu.org/licenses/>.

    WinterHill December 2020:

    Parts modified for the BATC WinterHill 4 channel receiver project - terms as above
    Copyright Brian Jordan, G4EWJ
*/

/* 
I2C addresses need to be shifted right for /dev/i2c-1
	
#define NIM_DEMOD_ADDR_A 	0xd0
#define NIM_DEMOD_ADDR_B 	0xd2
#define PIC_ADDR_A	 		0xd8						// each NIM interfaces to a PIC, which is on the i2c bus
#define PIC_ADDR_B	 		0xda
#define NIM_TUNER_ADDR 		0xc0						
#define STVVGLNA_I2C_ADDR0 	0xc8						// same for all NIMs . . . 
#define STVVGLNA_I2C_ADDR1 	0xca						// same for all NIMs . . . 
#define STVVGLNA_I2C_ADDR2 	0xcc						// same for all NIMs . . . 
#define STVVGLNA_I2C_ADDR3 	0xce						// same for all NIMs . . . 
#define NIM_LNA_0_ADDR 		STVVGLNA_I2C_ADDR3			// make sure only the required repeater . . . 
#define NIM_LNA_1_ADDR 		STVVGLNA_I2C_ADDR0			// in the STV0910s is turned on		
*/

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- INCLUDES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "rpi2c.h"
#include "errors.h"
#include "nim.h"

#include <string.h>				
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- DEFINES ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */


static	int			i2cfd ;									// file descriptor for i2C
static	char		i2cname 	  [] = {"/dev/i2c-1"} ;		// system file name for I2C		 

		uint8_t     NIMI2CADDRESS [] = {0,NIM_DEMOD_ADDR_A,NIM_DEMOD_ADDR_B} ;



/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------------------------------- */
/* read an i2c 16 bit register from the nim   													      */
/*																									  */	
/*   addr: the 16 bit address of the i2c register to access 										  */
/*    reg: the 8 biti2c register to read                                                              */
/*   *val: the return value for the register we have read                                             */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */

uint8_t i2c_read_reg16 (uint8_t addr, uint16_t reg, uint8_t *val) 
{
    int err ;

	char		buff [8] ;									

///	printf ("i2c: %02X %04X\r\n",addr,reg) ; ///////////////	    

	i2cfd = open (i2cname,O_RDWR) ;							
    if (i2cfd < 0)
    {
        printf ("Error@: Cannot open %s (%s)\r\n",i2cname,strerror(errno)) ;
        err = errno ;
    }
	else													// check for slave presence
	{
		err = ioctl (i2cfd, I2C_SLAVE, addr >> 1) ;			// slave addresses are 7 bit for /dev/i2c-1		
		if (err < 0)
		{
			printf ("Error@: Cannot open I2C slave 0x%02X (%s)\r\n",addr,strerror(errno)) ;
			err = errno ;
		}
		else
		{
			buff [0] = (reg >> 8) & 0xff ;
			buff [1] = (reg >> 0) & 0xff ;
			err = write (i2cfd,buff,2) ;					// first send the register address
			if (err < 0)
			{
				printf ("Error@: Cannot set register address on device 0x%02X register 0x%04X (%s)\r\n",addr,reg,strerror(errno)) ;
				err = errno ;
			}
			else
			{
				err = read (i2cfd,buff,1) ;					// now read the byte
				if (err < 0)
				{
					printf ("Error@: Cannot read byte from device 0x%02X (%s)\r\n",addr,strerror(errno)) ;
					err = errno ;
				}
				else
				{
					err = 0 ;
					*val = (uint8_t) (buff[0]) ;
				}
			}
		}
		close (i2cfd) ;
	}

    return (err) ;
} 

/* -------------------------------------------------------------------------------------------------- */
/* write an 8 bit value into a 16 bit  i2c register                                                   */
/*   addr: the i2c bus address to access                                                              */
/*    reg: the i2c register to write to                                                               */
/*    val: the value to write into the register                                                       */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */

uint8_t i2c_write_reg16 (uint8_t addr, uint16_t reg, uint8_t val) 
{
    int err;
    
	char		buff [8] ;								

///	printf ("i2c: %02X %04X %02X\r\n",addr,reg,val) ; ///////////////	    

	i2cfd = open (i2cname,O_RDWR) ;						
    if (i2cfd < 0)
    {
        printf ("Error@: Cannot open %s (%s)\r\n",i2cname,strerror(errno)) ;
        err = errno ;
    }
	else													// check for slave presence
	{
		err = ioctl (i2cfd, I2C_SLAVE, addr >> 1) ;			// slave addresses are 7 bit for /dev/i2c-1		
		if (err < 0)
		{
			printf ("Error@: Cannot open I2C slave 0x%02X (%s)\r\n",addr,strerror(errno)) ;
			close (i2cfd) ;
			err = errno ;
		}
		else
		{
			buff [0] = (reg >> 8) & 0xff ;
			buff [1] = (reg >> 0) & 0xff ;
			buff [2] = val ;
			err = write (i2cfd,buff,3) ;
			if (err < 0)
			{
				printf ("Error@: Cannot write byte to device 0x%02X register 0x%02X (%s)\r\n",addr,reg,strerror(errno)) ;
				err = errno ;
			}
			else
			{
				err = 0 ;
			}
			close (i2cfd) ;
		}
	}

    return (err) ;
}

/* -------------------------------------------------------------------------------------------------- */
/* read an i2c 8 bit register from the nim                                                            */
/*   addr: the i2c bus address to access                                                              */
/*    reg: the 16 bit address of the i2c register to read                                                                   */
/*   *val: the return value for the register we have read                                             */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */

uint8_t i2c_read_reg8 (uint8_t addr, uint8_t reg, uint8_t *val) 
{
///    uint8_t err;											

	char		buff [8] ;									
	int			err ;											

///	printf ("i2c: %02X %02X\r\n",addr,reg) ; ///////////////	    

	i2cfd = open (i2cname,O_RDWR) ;							
    if (i2cfd < 0)
    {
        printf ("Error@: Cannot open %s (%s)\r\n",i2cname,strerror(errno)) ;
        err = errno ;
    }
	else													// check for slave presence
	{
		err = ioctl (i2cfd, I2C_SLAVE, addr >> 1) ;			// slave addresses are 7 bit for /dev/i2c-1		
		if (err < 0)
		{
			printf ("Error@: Cannot open I2C slave 0x%02X (%s)\r\n",addr,strerror(errno)) ;
			err = errno ;
		}
		else
		{
			buff [0] = reg ;
			err = write (i2cfd,buff,1) ;					// first send the register address
			if (err < 0)
			{
				printf ("Error@: Cannot set register address on device 0x%02X register 0x%04X (%s)\r\n",addr,reg,strerror(errno)) ;
				err = errno ;
			}
			else
			{
				err = read (i2cfd,buff,1) ;					// now read the byte
				if (err < 0)
				{
					printf ("Error@: Cannot read byte from device 0x%02X (%s)\r\n",addr,strerror(errno)) ;
					err = errno ;
				}
				else
				{
					err = 0 ;
					*val = (uint8_t) (buff[0]) ;
				}
			}
		}
		close (i2cfd) ;
	}

    if (err!=ERROR_NONE) printf("ERROR: i2c read reg8 0x%.2x, 0x%.2x\n",addr,reg);

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */
/* write an 8 bit value to an i2c 8 bit register address in the nim                                   */
/*   addr: the i2c bus address to access                                                              */
/*    reg: the i2c register to write to                                                               */
/*    val: the value to write into the register                                                       */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */

uint8_t i2c_write_reg8 (uint8_t i2caddr, uint8_t reg, uint8_t val) 
{
    int err ;

	char		buff [8] ;									

///	printf ("i2c: %02X %02X %02X\r\n",addr,reg,val) ; ///////////////	    

	i2cfd = open (i2cname,O_RDWR) ;							
    if (i2cfd < 0)
    {
        printf ("Error@: Cannot open %s (%s)\r\n",i2cname,strerror(errno)) ;
        err = errno ;
    }
	else													// check for slave presence
	{
		err = ioctl (i2cfd, I2C_SLAVE, i2caddr >> 1) ;			// slave addresses are 7 bit for /dev/i2c-1		
		if (err < 0)
		{
			printf ("Error@: Cannot open I2C slave 0x%02X (%s)\r\n",i2caddr,strerror(errno)) ;
			close (i2cfd) ;
			err = errno ;
		}
		else
		{
			buff [0] = reg ;
			buff [1] = val ;
			err = write (i2cfd,buff,2) ;
			if (err < 0)
			{
				printf ("Error@: Cannot write byte to device 0x%02X register 0x%02X (%s)\r\n",i2caddr,reg,strerror(errno)) ;
				err = errno ;
			}
			else
			{
				err = 0 ;
			}
			close (i2cfd) ;
		}
	}

    return (err) ;

    if (err!=ERROR_NONE) printf("ERROR: i2c_write reg8 0x%.2x, 0x%.2x, 0x%.2x\n",i2caddr,reg,val);

    return err;
}


// send I2C address/write + function address + 2 bytes to a PIC

uint8_t i2c_write_pic16 (uint8_t i2caddr, uint8_t val1, uint8_t val2, uint8_t val3) 
{
	uint8_t		err = 0 ;
	
	err |= nim_set_stv0910_repeaters (false) ;
	err |= i2c_write_reg16 (i2caddr, ((((uint16_t)val1) << 8)) | val2, val3) ;

	return (err) ;
}


// send I2C address/write + function address + 1 byte to a PIC

uint8_t i2c_write_pic8 (uint8_t i2caddr, uint8_t val1, uint8_t val2) 
{
	uint8_t		err = 0 ;
	
	err |= nim_set_stv0910_repeaters (false) ;
	err |= i2c_write_reg8 (i2caddr, val1, val2) ;

	return (err) ;
}


// send I2C address/read and function address, then read 1 byte from a PIC

uint8_t i2c_read_pic8 (uint8_t i2caddr, uint8_t functionaddr, uint8_t* buff) 
{
	uint8_t		err = 0 ;
	uint8_t		val ;

	err |= nim_set_stv0910_repeaters (false) ;
	err |= i2c_read_reg8 (i2caddr, functionaddr, &val) ;
	*buff = val ;

	return (err) ;
}

