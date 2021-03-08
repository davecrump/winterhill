#define VERSION "whpicprog-3v20"

/* -------------------------------------------------------------------------------------------------- */
/* In-circuit PIC programmer for the WinterHill 4 channel DATV receiver                               */
/* This runs on a Raspberry Pi 4																	  */
/* This is specifically for WinterHill and should not be used as a general programmer                 */
/* Copyright 2020 Brian Jordan, G4EWJ                                                                 */
/* -------------------------------------------------------------------------------------------------- */
/*
    This file is part of WinterHill.

    WinterHill is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    WinterHill is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with WinterHill.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
	2021-02-17	2v40
		added PIC check function and status return

	2020-12-24  2v30
		use BOARD_RESET as a common reset signal for programming
		jumpers and user intervention no longer required

	2020-12  	2v22
		use SPI0 pins to in-circuit program PIC_A for BATC WinterHill
		use SPI6 pins to in-circuit program PIC_B for BATC WinterHill
		use SPISS with a board jumper to each PIC reset
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <bcm_host.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

typedef	unsigned char		uint8 ;
typedef	unsigned int		uint32 ;
typedef	signed int			int32 ;

#define CR					13
#define LF					10

#define BASE_VERSION		0x0300
#define ERROR_UNKNOWN		0xffffffff
#define ERROR_NOTSUDO		0xfffffffe
#define ERROR_PARAMS		0xfffffffd
#define ERROR_NOI2CPORT		0xfffffffc
#define ERROR_IOMAP			0xfffffffb
#define ERROR_NOFILE		0xfffffffa
#define ERROR_ADDRESS		0xfffffff9
#define ERROR_NOTHEX		0xfffffff8
#define ERROR_HEXRANGE		0xfffffff7
#define ERROR_NOVERSION		0x0
#define ERROR_NOPIC			0x1
#define ERROR_PICPRESENT	0x2						// internal use

#define PERIPHERALS_BASE    0xfe000000
#define PERIPHERALS_SIZE    0x20000000
#define GPIO_OFFSET         0x00200000
#define GPFSEL0             (0x00 / 4)
#define GPFSEL1             (0x04 / 4)
#define GPFSEL2             (0x08 / 4)
#define GPFSET0             (0x1c / 4)
#define GPFCLR0             (0x28 / 4)
#define GPFLEV0             (0x34 / 4)
#define GPIO_PULL0 			(0xe4 / 4)
#define GPIO_PULL1			(0xe8 / 4)

#define FSEL_ALT0           4
#define FSEL_ALT3           7
#define FSEL_ALT4           3
#define FSEL_ALT5           2
#define FSEL_OUTPUT         1
#define FSEL_INPUT          0

#define BOARD_RESET			7										// program reset on BCMGPIO  7 J:26
 
// SPI0 for PIC_A

///#define PRESET_A			8										// program reset on BCMGPIO  8 J:24
#define PCLOCK_A			11										// program clock on BCMGPIO 11 J:23
#define PDATA_A				10										// program data  on	BCMGPIO 10 J:19		

// SPI6 for PIC_B

///#define PRESET_B			18										// program reset on BCMGPIO 18 J:12
#define PCLOCK_B			21										// program clock on BCMGPIO 21 J:40
#define PDATA_B				20										// program data  on	BCMGPIO 20 J:38		

#define PROGENTRY			0x4D434851								// sequence to enter prog mode
#define PROGENTRY_REV		0x8a12c2b2								// reverse bit order
#define FLASHBYTES          (0x2b000*2)                 			// adddress range is 0x2b000 16 bit words
#define PAGEBYTES           512										// 512 bytes are programmed at a time
#define PAGES               (FLASHBYTES / PAGEBYTES)
#define PROGENTRY			0x4D434851								// sequence to enter prog mode
#define PROGENTRY_REV		0x8a12c2b2								// reverse bit order

#define PIC_ADDR_A				0xd8
#define PIC_ADDR_B				0xda
#define PIC_REG_VERSION_LOW		0x96
#define PIC_REG_VERSION_HIGH	0x97


// commands

#define	SIX					0
#define	REGOUT				1
#define INSERTADRLOW		2
#define INSERTADRHIGH		3
#define READSOURCE			4
#define WRITERPI			5
#define INCRPI				6
#define INCPIC				7
#define VISILOW				8
#define VISIHIGH			9
#define INSERTDATLOW		10
#define INSERTDATHIGH		11
#define	STARTLOOP			0x80
#define ENDLOOP				0x90
#define JUMPTRUE			0xa0
#define JUMPFALSE			0xb0
#define FINISH				0xff


typedef struct
{
		int32				command ;								// SIX or VISI
		int32				codeline ;									
} progline_t ;


typedef struct 
{
		uint32				active ;								// active or not
		uint32				startindex ;							// line index of the start point
		uint32				iterations	;							// loop count (descending)
} progloop_t ;


//
// 	flash 16 bit word addresses 0x000000-0x02bffe
//
// 	erase block 512 instructions, 	1024 words, 		2048 bytes
//		16 bit word addresses: 		0x000000-0x0003fe
//									0x000400-0x0007fe
//
// 	program page 128 instructions, 	256 words, 			512 bytes
//		16 bit word addresses		0x000000-0x0000ff
//
//	boot block 1024 instructions,	2048 words,			4096 bytes


							progline_t			chiperase [] = 
							{
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{SIX,			0x2400e0},			// mov 0x400e,w0
								{SIX,			0x883b00},			// mov w0,NVMCON
								{SIX,			0x200550},			// mov #0x55,w0
								{SIX,			0x883b30},			// mov w0,NVMKEY
								{SIX,			0x200aa0},			// mov #0xaa,w0
								{SIX,			0x883b30},			// mov w0,NVMKEY
								{SIX,			0xa8e761},			// bset NVMCON,#WR
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{STARTLOOP+2,	0},					// loop repeaedly
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{SIX,			0x803b02},			// mov NVMCON,w2
								{SIX,			0},					// nop
								{SIX,			0x883c22},			// mov w2,VISI
								{SIX,			0},					// nop
								{VISILOW,		0},					// read VISI 
								{SIX,			0},					// nop
								{JUMPTRUE+2,	0x008000},			// AND then go to STARTLOOP2 if TRUE		
								{SIX,			0x883b00},			// mov w0,NVMCON
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{FINISH,		0}				
							} ;


							progline_t			pageprogram [] = 
							{
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{SIX,			0x240020},			// mov 0x4002,w0
								{SIX,			0x883b00},			// mov w0,NVMCON
								{SIX,			0x200fac},			// mov #0xfa,w12
								{SIX,			0x8802ac},			// mov w12,TBLPAG
								{SIX,			0xeb0380},			// clr w7	
								{STARTLOOP+1,	-1},				// program COUNT1 instructions
								{INSERTDATLOW,	0x200006},			// mov #LOW data,w6  move from data buffer
								{SIX,			0},					// nop
								{SIX,			0xbb0b86},			// tblwtl w6,[w7]	
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{INSERTDATHIGH, 0x200006},			// mov #HIGH data,w6  move from data buffer
								{SIX,			0},					// nop
								{SIX,			0xbb8b86},			// tblwth w6,[w7]	
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{SIX,			0xe88387},			// inc2 w7,w7  increment latch address
								{INCRPI,		0},					// increment source address
								{SIX,			0},					// nop
								{ENDLOOP+1,		0},					// go around loop 1
								{SIX,			0},					// nop
								{INSERTADRLOW,	0x200003},			// mov #LOW picaddress,w3
								{INSERTADRHIGH,	0x200004},			// mov #HIGH picaddress,w4
								{SIX,			0x883b13},			// mov w3,NVMADR
								{SIX,			0x883b24},			// mov w4,NVMADRU
								{SIX,			0},					// nop
								{SIX,			0x200550},			// mov #0x55,w0
								{SIX,			0x883b30},			// mov w0,NVMKEY
								{SIX,			0x200aa0},			// mov #0xaa,w0
								{SIX,			0x883b30},			// mov w0,NVMKEY
								{SIX,			0xa8e761},			// bset NVMCON,#WR
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{STARTLOOP+2,	0},					// loop repeaedly
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{SIX,			0x803b02},			// mov NVMCON,w2
								{SIX,			0},					// nop
								{SIX,			0x883c22},			// mov w2,VISI
								{SIX,			0},					// nop
								{VISILOW,		0},					// read VISI 
								{SIX,			0},					// nop
								{JUMPTRUE+2,	0x008000},			// AND then go to STARTLOOP2 if TRUE		
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{SIX,			0x200000},			// mov #0,w0
								{SIX,			0x883b00},			// mov w0,NVMCON
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{FINISH,		0}				
							} ;

// PIC ICSP instruction sequence to erase a flash block

							progline_t			blockerase [] = 
							{
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{SIX,			0x240030},			// mov 0x4003,w0
								{SIX,			0x883b00},			// mov w0,NVMCON
								{INSERTADRLOW,	0x200000},			// mov #LOW PAGEADDR,w0
								{SIX,			0x883b10},			// mov w0,NVMADR
								{INSERTADRHIGH,	0x200000},			// mov #HIGH PAGEADDR,w0
								{SIX,			0x883b20},			// mov w0,NVMADRU
								{SIX,			0},					// nop
								{SIX,			0x200550},			// mov #0x55,w0
								{SIX,			0x883b30},			// mov w0,NVMKEY
								{SIX,			0x200aa0},			// mov #0xaa,w0
								{SIX,			0x883b30},			// mov w0,NVMKEY
								{SIX,			0xa8e761},			// bset NVMCON,#WR
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{SIX,			0},					// nop
								{STARTLOOP+1,	0},					// loop repeaedly
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{SIX,			0x803b02},			// mov NVMCON,w2
								{SIX,			0},					// nop
								{SIX,			0x883c22},			// mov w2,VISI
								{SIX,			0},					// nop
								{VISILOW,		0},					// read VISI 
								{SIX,			0},					// nop
								{JUMPTRUE+1,	0x008000},			// AND then go to STARTLOOP+1 if TRUE		
								{SIX,			0},					// nop
								{SIX,			0x200000},			// mov #0,w0
								{SIX,			0x883b00},			// mov w0,NVMCON
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},					// nop
								{FINISH,		0},				
							} ;						


// PIC ICSP instruction sequence to read flash

		progline_t			readwords [] =
							{
								{SIX,			0},					// nop
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},
								{SIX,			0x207847},			// mov #VISI,w7
								{STARTLOOP+1,	-1},				// use supplied count1 for iterations
								{SIX,			0},
								{INSERTADRHIGH,	0x200000},			// mov #HIGH picaddress,w0
								{SIX,			0x8802a0},			// mov w0,TBLPAG
								{INSERTADRLOW,	0x200006},			// mov #LOW picaddress,w6
								{SIX,			0},					// not in data sheet but seems to be required
								{SIX,			0xba0b96},			// tblrdl [w6],[w7]						
								{SIX,			0},
								{SIX,			0},
								{VISILOW,		0},					// read VISI 
								{SIX,			0},
								{SIX,			0xba8b96},			// tblrdh [w6],[w7]						
								{SIX,			0},
								{SIX,			0},
								{VISIHIGH,		0},					// read VISI 
								{SIX,			0},
								{WRITERPI,		0},					// store visi to RPi
								{INCRPI,		0},					// add 4 to RPi address
								{INCPIC,		0},					// add 2 to PIC address
								{ENDLOOP+1,		0},
								{SIX,			0x040200},			// goto 0x200
								{SIX,			0},
								{FINISH,		0}								
							} ;

        uint32      		address ;
        uint8       		flashbytes      [FLASHBYTES] ;
        uint8       		flashbytes2     [FLASHBYTES] ;
		uint32*				gpio ;									// pointer to the GPIO area
		int					i2cfd ;									// I2C handle
		char				i2cname			[64] ;					// I2C device name 
		uint32				loopno ;
		progloop_t			loops [16] ;
		uint32				pclock ;
		uint32				pdata ;
		uint32				peripherals_base ;						// base of all peripherals; 0xfe0000 for RPi4
		uint32				preset ;
        uint        		usedpages       [FLASHBYTES/PAGEBYTES] ;    // indicates whether a page 
		uint32				xbuff [256] ;
        													

// function prototypes

		void				entericspmode		(void) ;
		void				exiticspmode		(void) ;
		int32				check_pic_status	(int32) ;
		int32				i2c_read_reg8		(uint8, uint8, uint8*) ;
		int32				i2c_write_reg8		(uint8, uint8, uint8) ;
		uint32				get_return_value	(uint32) ;
		void				gpioconfig			(uint32,uint32) ;
		uint32				gpioget				(uint32) ;
		void				gpioset				(uint32,uint32) ;
		void				icsp				(progline_t*,uint32*,uint32,uint32,uint32) ;
		void				pdelay				(void) ;
		uint32				receivebits			(uint32) ;
		void				restore_pins		(void) ;
		void				sendbits			(uint32,uint32) ;
		uint32*				setup_io_map		(uint32) ;
		void				setup_pins			(void) ;


// ******* maximum clock 5MHz *******


int32 main (int argc, char* argv[])
{
	int			errors ;
	char		hexline [256] ;
	FILE		*ip ;
	uint32		parsedbytecount ;
    uint8       parsedbytes     	[128] ;
	uint32		picaddress ;
	char*		pos ;	
	uint32		recordtype ;
	uint32*		rpiaddress ;
	uint32*		rpiaddress2 ;
	int			temp1 ;
	int			temp2 ;
	char		temps [16] ;
	uint32		tempu ;
	int32		x ;
	int32		y ;
	uint32		displaycount ;
	uint32		bcm_peri_base ;	
	uint32		pic ;
	uint32		returnstatus ;	
	uint32		returnvalue ;
	int			status ;
	uint8		tempc ;
	int32		picversion ;

	printf ("\r\n") ;
    printf ("----------------------------------------------------------------------------------------\r\n") ;
    printf ("%s: PIC24FJ256GA702 in-circuit programmer for the BATC Advanced Receiver PCB         \r\n",VERSION) ;
	printf ("                Ensure that the WinterHill or Ryde main program is not running\r\n") ;
    printf ("----------------------------------------------------------------------------------------\r\n") ;

// check we are being run as sudo	
	
	if (geteuid() != 0)
	{
		printf ("This program must be run as sudo\r\n") ;
		printf ("Usage: sudo ./%s hexfilename   (program both PICs using the hex file)\r\n",VERSION) ;
		printf ("Usage: sudo ./%s               (check for PIC presence and software version)\r\n",VERSION) ;
	    printf ("----------------------------------------------------------------------------------------\r\n") ;
		printf ("\r\n") ;
		usleep (2 * 1000 * 1000) ;							// 2s
		exit (ERROR_NOTSUDO) ;
	}

// check number of parameters	
	
	if (argc != 2)
	{
		printf ("Usage: sudo ./%s hexfilename   (program both PICs using the hex file)\r\n",VERSION) ;
		printf ("Usage: sudo ./%s --check       (check for PIC presence and software version)\r\n",VERSION) ;
	    printf ("----------------------------------------------------------------------------------------\r\n") ;
		printf ("\r\n") ;
		usleep (2 * 1000 * 1000) ;							// 2s
		exit (ERROR_PARAMS) ;
	}

// map to the GPIO peripheral

    gpio = setup_io_map (GPIO_OFFSET) ;                      // create a view into the peripherals
    if (gpio == 0)
    {
	    printf ("Error: Cannot set peripheral mapping\r\n") ;
	    printf ("----------------------------------------------------------------------------------------\r\n") ;
		printf ("\r\n") ;
		usleep (2 * 1000 * 1000) ;							// 2s
		return (ERROR_IOMAP) ;
    }

// setup IO pins and reset the board

	setup_pins() ;											// set up programming pins for both PICs	
	
	gpioset (BOARD_RESET, 0) ;								// apply board reset
	usleep (100 * 1000) ;									// wait 100ms
	gpioset (BOARD_RESET, 1) ;								// release board reset
	usleep (100 * 1000) ;									// wait 100ms

	preset = BOARD_RESET ;

// look for the I2C port

	sprintf (i2cname, "/dev/i2c-1") ;

	i2cfd = open (i2cname,O_RDWR) ;							
	if (i2cfd < 1)
	{
		printf ("Cannot open /dev/i2c-1; has the I2C port been enabled in RPi Configuration?\r\n") ;
	    printf ("----------------------------------------------------------------------------------------\r\n") ;
		printf ("\r\n") ;
		usleep (2 * 1000 * 1000) ;							// 2s
		restore_pins() ;
		return (ERROR_NOI2CPORT) ;
	}
	close (i2cfd) ;
	
// check PIC status and then exit, if no parameters passed

	returnstatus = 0 ;

	if (strcasecmp(argv[1],"--check") == 0)					// check for PIC presence and software version
	{
		printf ("Checking for PIC presence and version - the PICs will not be programmed:\r\n") ;

		returnstatus = check_pic_status (returnstatus) ;
		returnvalue  = get_return_value (returnstatus) ;

		restore_pins() ;

	    printf ("----------------------------------------------------------------------------------------\r\n") ;
		printf ("\r\n") ;
		usleep (2 * 1000 * 1000) ;							// 2s
		exit (returnvalue) ;
	}

// initialise the buffers for PIC writing and comparing	
	
	rpiaddress  = (uint32*) &flashbytes  ;
	rpiaddress2 = (uint32*) &flashbytes2 ;
	for (x = 0 ; x < FLASHBYTES / 4 ; x++)
	{
		rpiaddress[x]  = 0x00ffffff ;
		rpiaddress2[x] = 0x00ffffff ;
	}

// open the hex file
	
	ip = fopen (argv[1],"rt") ;
	if (ip == 0)
	{
		printf ("Error: Cannot open %s \r\n",argv[1]) ;
	    printf ("----------------------------------------------------------------------------------------\r\n") ;
		printf ("Usage: sudo ./%s hexfilename  (program both PICs using the hex file)\r\n",VERSION) ;
		printf ("Usage: sudo ./%s --check      (check for PIC presence and software version)\r\n",VERSION) ;
	    printf ("----------------------------------------------------------------------------------------\r\n") ;
		printf ("\r\n") ;
		usleep (2 * 1000 * 1000) ;							// 2s
		restore_pins() ;
		exit (ERROR_NOFILE) ;
	}

// parse the hex file into the flashbytes buffer	
	
    address = 0 ;
    memset ((void*)usedpages,0,sizeof(usedpages)) ;  
	recordtype = 0 ;
	while (!feof(ip) && recordtype != 1)
	{
		fgets (hexline,sizeof(hexline),ip) ;
		pos = strchr (hexline,CR) ;
		if (pos)
		{
			*pos = 0 ;
		}
		pos = strchr (hexline,LF) ;
		if (pos)
		{
			*pos = 0 ;
		}

		memset (parsedbytes,0,sizeof(parsedbytes)) ;
		pos = hexline ;
		if (*pos++ != ':')
		{
			printf ("Error: %s is not in hex format\r\n",argv[1]) ;
			fclose (ip) ;
		    printf ("----------------------------------------------------------------------------------------\r\n") ;
			printf ("\r\n") ;
			usleep (2 * 1000 * 1000) ;							// 2s
			exit (ERROR_NOTHEX) ;
		}
		parsedbytecount = 0 ;
		while (*pos)
		{
			temp1 = *pos++ ;
			temp1 = toupper (temp1) ;
			if (temp1 >= 'A')
			{
				temp1 -= 7 ;				
			}
			temp1 -= '0' ;
			temp2 = *pos++ ;
			temp2 = toupper (temp2) ;
			if (temp2 >= 'A')
			{
				temp2 -= 7 ;				
			}
			temp2 -= '0' ;
			parsedbytes [parsedbytecount++] = temp1 * 0x10 + temp2 ;
		}

		recordtype = parsedbytes [3] ;

        if (recordtype == 4)                                        // upper 16 bits ;
        {
            tempu = parsedbytes[4] * 0x100 + parsedbytes[5] ;
            address &= 0xffff ;
            address |= tempu << 16 ;
        }
        else if (recordtype == 0)
        {
            tempu = parsedbytes[1] * 0x100 + parsedbytes[2] ;
			address &= 0xffff0000 ;
			address |= tempu ;
            for (x = 0 ; x < parsedbytes[0] ; x++)
            {
            	if (address+x >= FLASHBYTES)
            	{
					printf ("Error: 0x%06X address limit exceeded - upper areas not supported\r\n",FLASHBYTES/2) ;
					fclose (ip) ;
				    printf ("----------------------------------------------------------------------------------------\r\n") ;
					printf ("\r\n") ;
					usleep (2 * 1000 * 1000) ;						// 2s
					exit (ERROR_ADDRESS) ;
            	}
                flashbytes[address+x]           = parsedbytes[x+4] ;
                usedpages[address/PAGEBYTES]    = 1 ;
            }
        }
    } ;
    fclose (ip) ;

// display hex file page use

	printf ("Reading HEX file '%s' \r\n",argv[1]) ;
	printf ("Pages used: ") ;
	displaycount = 0 ;
	for (x = 0 ; x < PAGES ; x++)
	{
		if (usedpages[x])
		{
			if ((displaycount % 8) == 0)
			{
				printf ("\r\n  ") ;
			}
			printf ("%06X  ", x * PAGEBYTES / 2) ;			// 16 bit word address
			tempu++ ;
			displaycount++ ;
		}
	}
	if ((displaycount % 8) != 0)
	{
		printf ("\r\n") ;
	}

	setup_pins() ;											// set up programming pins for both PICs	
/*
	usleep (100 * 1000) ;
	gpioset (BOARD_RESET, 1) ;								// release board reset
	preset = BOARD_RESET ;
*/

// program both PICs

	for (pic = 0 ; pic <= 1 ; pic++)
	{
		if (pic == 0)
		{
			pclock = PCLOCK_A ;
			pdata  = PDATA_A ;
		}
		else 
		{
			pclock = PCLOCK_B ;
			pdata  = PDATA_B ;
		}

		entericspmode() ;

// we are now in ICSP mode

		printf ("------\r\n") ;
		printf ("PIC_%c:\r\n", pic+'A') ;
		printf ("------\r\n") ;
		printf ("  Reading chip ID: \r\n") ;
		memset ((void*)xbuff, 0, sizeof(xbuff)) ;
		icsp (readwords, xbuff, 0xff0000, 2, 0) ;
		printf ("    DeviceID=%04X\r\n", xbuff[0]) ;
		printf ("    Revision=%04X\r\n", xbuff[1]) ;
		if (xbuff[0] == 0x750e)
		{
			returnstatus |= ERROR_PICPRESENT << (pic * 16) ;
			printf ("    PIC24FJ256GA702 detected\r\n") ;
		}
		else
		{
			exiticspmode() ;
			returnstatus |= ERROR_NOPIC << (pic * 16) ;
			printf ("    PIC24FJ256GA702 not detected\r\n") ;
			continue ;
		}

		printf ("  Erasing chip: \r\n") ;
		icsp (chiperase,0,0,0,0) ;

		exiticspmode() ;

// exit icsp mode and re-enter; programming fails every alternate time if this is not done

		entericspmode() ;
		
		printf ("  Programming: ") ;
		displaycount = 0 ;
		for (x = 0 ; x < PAGES ; x++)    
		{
			if (usedpages[x])
			{
				if ((displaycount % 8) == 0)
				{
					printf ("\r\n    ") ;
				}
				rpiaddress = (uint32*) (x * PAGEBYTES + (uint32)(&flashbytes)) ;
				picaddress = x * PAGEBYTES / 2 ;                       	 	// 16 bit words
				printf ("%06X  ", picaddress) ;
				displaycount++ ;
				icsp (pageprogram,rpiaddress,picaddress,PAGEBYTES/4,0) ;	// 512/4 = 128 instructions
			}
		}
		if ((displaycount % 8) != 0)
		{
			printf ("\r\n") ;
		}

		usleep (100 * 1000) ;

		printf ("  Verifying: ") ;

		errors = 0 ;
		displaycount = 0 ;
		for (x = 0 ; x < PAGES ; x++)    
		{
			if (usedpages[x])
			{
				if ((displaycount % 8) == 0)
				{
					printf ("\r\n    ") ;
				}
				rpiaddress  = (uint32*) (x * PAGEBYTES + (uint32)(&flashbytes)) ;
				rpiaddress2 = (uint32*) (x * PAGEBYTES + (uint32)(&flashbytes2)) ;
				picaddress  = x * PAGEBYTES / 2 ;                      	// 16 bit words
				icsp (readwords,rpiaddress2, picaddress,PAGEBYTES / 4, 0) ; // 128 instructions @ 4 bytes per instruction
				printf ("%06X", picaddress) ;
				if (memcmp((void*)rpiaddress, (void*)rpiaddress2, PAGEBYTES) != 0)
				{
					errors++ ;
					printf ("* ") ;
				}
				else
				{
					printf ("  ") ;
				}
				displaycount++ ;
			}
		}    
		if ((displaycount % 8) != 0)
		{
			printf ("\r\n") ;
		}

		if (errors == 0)
		{
			printf ("  *** PIC_%c SUCCESS ***\r\n", 'A' + pic) ;
		}
		else 
		{
			returnstatus &= ~0xffff << (pic * 16) ;
///			returnstatus |= ERROR_VERIFY << (pic * 16) ;				// verify error
			returnstatus |= ERROR_UNKNOWN << (pic * 16) ;				
			printf ("  Error: %d page comparison fault", errors) ;
			if (errors != 1)
			{
				printf ("s") ;
			}
			printf ("\r\n") ;
			printf ("  FAILED \r\n") ;
		}
		exiticspmode() ;
	}

	usleep (1200 * 1000) ;												// 1.2s

	restore_pins() ;

    printf ("----------------------------------------------------------------------------------------\r\n") ;
	printf ("Checking status:\r\n") ;

///	returnstatus = 0 ;
	returnstatus = check_pic_status (returnstatus) ;
	returnvalue  = get_return_value (returnstatus) ;

    printf ("----------------------------------------------------------------------------------------\r\n") ;
	printf ("\r\n") ;

	exit (returnvalue) ;
}


void setup_pins()
{
	gpioconfig 	(BOARD_RESET, 	FSEL_OUTPUT) ;
	gpioconfig 	(PCLOCK_A, 		FSEL_OUTPUT) ;
	gpioconfig 	(PDATA_A, 		FSEL_INPUT) ;
	gpioconfig 	(PCLOCK_B, 		FSEL_OUTPUT) ;
	gpioconfig 	(PDATA_B, 		FSEL_INPUT) ;
	gpioset		(BOARD_RESET,	0) ;				// reset the board
	gpioset		(PCLOCK_A,		0) ;				// initial state is low					
	gpioset		(PDATA_A, 		0) ;				// initial state is low					
	gpioset		(PCLOCK_B,		0) ;				// initial state is low					
	gpioset		(PDATA_B, 		0) ;				// initial state is low					
}


void restore_pins()
{
	gpioconfig 	(BOARD_RESET, 	FSEL_INPUT) ;
	gpioconfig 	(PCLOCK_A, 		FSEL_INPUT) ;
	gpioconfig 	(PDATA_A, 		FSEL_INPUT) ;
	gpioconfig 	(PCLOCK_B, 		FSEL_INPUT) ;
	gpioconfig 	(PDATA_B, 		FSEL_INPUT) ;
}


void entericspmode()
{
	usleep		(100*1000) ;				// wait for 100ms
	gpioset		(preset, 0) ;				// reset low
	usleep		(1000) ;					// wait 1ms 
	gpioset		(preset, 1) ;				// reset high
	usleep		(1) ;						// pdelay must be < 500us
	gpioset		(preset, 0) ;				// reset low
	usleep		(2*1000) ;					// wait 2ms
	sendbits	(PROGENTRY_REV, 32) ;		// send the 32 bit programming sequence		
	usleep		(1) ;						// P19 25ns min
	gpioset		(preset, 1) ;				// reset high
	usleep		(100*1000) ;				// wait 100ms
	sendbits	(0, 5) ;					// send 5 clocks
	usleep		(100*1000) ;
}


void exiticspmode()
{
    usleep      (1) ;
    gpioset     (pclock,0) ;                // clock low
    gpioset     (pdata,0) ;                 // data low
    gpioset     (preset,0) ;                // reset low
    usleep      (10 * 1000) ;   
    gpioset     (preset,1) ;                // reset high
    usleep      (100 * 1000) ;
}


// send bits, lsb first
// PIC samples data on the +ve edge of the clock

void sendbits (uint32 value, uint32 bitcount)
{
	int			x ;

	gpioconfig 	(pdata,FSEL_OUTPUT) ;	
	pdelay() ;
	for (x = 0 ; x < bitcount ; x++)
	{
		gpioset (pdata,value & 1) ;	
		pdelay() ;
		gpioset (pclock,1) ;
		pdelay() ;
		gpioset (pclock,0) ;
		pdelay() ;
		value 	>>= 1 ;
	}
	gpioconfig 	(pdata,FSEL_INPUT) ;	
	pdelay() ;
	gpioset 	(pdata,0) ;
	pdelay() ;
}
		

// receive bits, lsb first
// data from the PIC changes on the -ve edge of the clock 

uint32 receivebits (uint32 bitcount)
{
	int			x ;
	uint32		temp ;

	gpioconfig 	(pdata,FSEL_INPUT) ;
	pdelay() ;
	temp = 0 ;
	for (x = 0 ; x < bitcount ; x++)
	{
		temp	 >>= 1 ;
		if (gpioget(pdata))
		{
			temp |= 0x80000000 ;
		}
		gpioset (pclock,1) ;
		pdelay() ;
		gpioset (pclock,0) ;
		pdelay() ;
	}
	temp >>= (32 - bitcount) ;
	return (temp) ;
}



// process a list of ICSP commands

void icsp (progline_t* icspsequence, uint32* xrpiaddress, uint32 xpicaddress, uint32 xcount1, uint32 xcount2) 
{
	int					x ;
	uint32				outvalue ;
	uint32				visilow ;
	uint32				visihigh ;
	progline_t*			p ;
	uint32*				p32 ;
	uint32*				rpiaddress ;
	uint32				picaddress ;
	uint32				count1 ;
	uint32				count2 ;
	int32				tempi ;
	
	usleep (1) ;

	memset ((void*)loops,0,sizeof(loops)) ;

	p 				= icspsequence ;
	rpiaddress 		= xrpiaddress ;	
	picaddress 		= xpicaddress ;	
	count1			= xcount1 ;
	count2			= xcount2 ;
	visilow			= 0 ;
	visihigh		= 0 ;

	x = 0 ;	
	while (1)
	{			
		outvalue = p[x].codeline ;
		if (p[x].command == FINISH)							// finished
		{
			break ;
		}
		else if (p[x].command == SIX)						// command
		{
			sendbits	(SIX,4) ;							
			sendbits 	(p[x].codeline,24) ;				// send command
		}
		else if (p[x].command == VISILOW)					// read VISI and store in visilow
		{
			sendbits	(REGOUT,4) ;						// send command
			sendbits 	(0,8) ;								// send 8 clocks
			visilow		= receivebits (16) ;				// get the visi register			
		}
		else if (p[x].command == VISIHIGH)					// read VISI and store in visihigh
		{
			sendbits	(REGOUT,4) ;						// send command
			sendbits 	(0,8) ;								// send 8 clocks
			visihigh	= receivebits (16) ;				// get the visi register			
		}
		else if ((p[x].command & 0xf0) == STARTLOOP)		// start of loop
		{
			loopno = p[x].command & 0xf ;					// loop number
			if (loops[loopno].active == 0)					// check loop not already set up
			{
				tempi = p[x].codeline ;
				if (tempi == -1)
				{
					tempi = count1 ;
				}
				else if (tempi == -2)
				{
					tempi = count2 ;
				}
				else if (tempi < 0)
				{
					tempi = 0 ;
				}				
				loops[loopno].iterations = tempi ;			// set up loop counter				
				loops[loopno].startindex = x ;				// loop start index
				loops[loopno].active 	 = 1 ;				// mark as active
			}
		}
		else if ((p[x].command & 0xf0) == ENDLOOP)			// end of loop
		{
			loopno = p[x].command & 0xf ;					// loop number
			if (loops[loopno].active)
			{
				if (loops[loopno].iterations)
				{
					loops[loopno].iterations-- ;
				}
				if (loops[loopno].iterations)
				{
					x = loops[loopno].startindex ;			// goto loop start; the x++ further down is OK
				}
				else
				{
					loops[loopno].active  = 0 ;				// allow for loop nesting
				}
			}
		}
		else if ((p[x].command & 0xf0) == JUMPTRUE)			// AND codeline with visilow and jump if TRUE
		{
			loopno = p[x].command & 0xf ;					// loop number
			tempi = visilow & p[x].codeline ;
			if (tempi)
			{
				x = loops[loopno].startindex ;				// goto loop start; the x++ further down is OK
			}
		}
		else if ((p[x].command & 0xf0) == JUMPFALSE)		// AND codeline with visilow and jump if FALSE
		{
			loopno = p[x].command & 0xf ;					// loop number
			tempi = visilow & p[x].codeline ;
			if (tempi == 0)
			{
				x = loops[loopno].startindex ;				// goto loop start; the x++ further down is OK
			}
		}
		else if (p[x].command == WRITERPI)					// store visi to RPI 
		{
			*rpiaddress = (visihigh << 16) | visilow ;
		}
		else if (p[x].command == INCRPI)					// increment RPi address 
		{
			rpiaddress++ ;
		}
		else if (p[x].command == INSERTADRLOW)				// insert low PIC address
		{
			outvalue 	&= 	~0x0ffff0 ;
			outvalue 	|= 	(picaddress & 0xffff) << 4 ;
			sendbits 	   	(0,4) ;							// SIX command 
			sendbits	   	(outvalue,24) ;				
		}
		else if (p[x].command == INSERTADRHIGH)				// insert high PIC address
		{
			outvalue 	&= 	~0x0ffff0 ;
			outvalue 	|= 	((picaddress >> 16) & 0xffff) << 4 ;
			sendbits 	   	(0,4) ;							// SIX command 
			sendbits	   	(outvalue,24) ;				
		}
		else if (p[x].command == INSERTDATLOW)				// insert low RPI data
		{
			outvalue 	&= 	~0x0ffff0 ;
			outvalue 	|= 	(*rpiaddress & 0xffff) << 4 ;
			sendbits 	   	(0,4) ;							// SIX command 
			sendbits	   	(outvalue,24) ;				
		}
		else if (p[x].command == INSERTDATHIGH)				// insert high PIC address
		{
			outvalue 	 = 	p[x].codeline ;
			outvalue 	&= 	~0x0ffff0 ;
			outvalue 	|= 	((*rpiaddress >> 16) & 0xffff) << 4 ;
			sendbits 	   	(0,4) ;							// SIX command 
			sendbits	   	(outvalue,24) ;				
		}
		else if (p[x].command == INCPIC)					// increment PIC address 
		{
			picaddress  += 2 ;
		}
///		printf ("%2d %02X %08X %08X\r\n",x,p[x].command,outvalue,visilow) ;
		outvalue = 0 ;
		x++ ;												// process next command line
	}
}


// set the gpio port functions

void gpioconfig (uint32 bcmportno, uint32 altfunction)
{
    uint32                    index ;
    uint32                    pos ;
    uint32                    temp ;
 
    index                   = bcmportno / 10 ;                  // 10 gpio settings per word
    pos                     = (bcmportno - (index * 10)) * 3 ;  // get the bit position
    temp                    = gpio [GPFSEL0 + index] ;          // get the function settings register
    temp                   &= ~(7 << pos) ;                     // make the pin an input
    gpio [GPFSEL0 + index]  = temp ;
    temp                   |= altfunction << pos ;              // set the alternate function
    gpio [GPFSEL0 + index]  = temp ;
}


uint32 gpioget (uint32 bcmbitno)
{
	uint32		temp ;

	temp 	= gpio [GPFLEV0] & (1 << bcmbitno) ;
	temp  >>= bcmbitno ;
	return (temp) ;
}
	

void gpioset (uint32 bcmbitno,uint32 value)
{
	if (value)
	{
		gpio [GPFSET0] = 1 << bcmbitno ;
	}
	else
	{
		gpio [GPFCLR0] = 1 << bcmbitno ;
	}
}


#define BLOCK_SIZE              (0x1000)

uint32* setup_io_map (uint32 offset)									// return a pointer to the required peripheral offset
{
	int			mem_fd ;
	void		*peri_map ;	
	int			io_base ;
	uint32		bcm_peri_base ;

	
    bcm_peri_base 	= bcm_host_get_peripheral_address() ;				// get the PIC peripheral base address
	io_base			= bcm_peri_base + offset ;
	                
/* open /dev/mem */

	mem_fd = open ("/dev/mem", O_RDWR | O_SYNC) ;
   	if (mem_fd  < 0)
   	{
    	printf ("Can't open /dev/mem \r\n") ;
		return (0) ;
   	}

/* mmap peripheral */

    peri_map = mmap
    (
        NULL,                 							// Any adddress in our space will do
 		BLOCK_SIZE,           							// Map length
      	PROT_READ | PROT_WRITE,							// Enable reading & writing to mapped memory
      	MAP_SHARED,           							// Shared with other processes
      	mem_fd,               							// File to map
      	io_base               							// Offset to GPIO peripheral
   	) ;

    close (mem_fd) ; 		  							// No need to keep mem_fd open after mmap

    if ((int)peri_map == -1)
    {
       printf("mmap error %d \r\n", (int)peri_map) ;	// errno also set!
       return (0) ;
    }

	return (uint32*) peri_map ;
}


// pdelay routine for programming outputs

void pdelay()
{
	uint32		x ;
	
	for (x = 0 ; x < 50 ; x++) ;
}


int32 i2c_read_reg8 (uint8 i2caddr, uint8 reg, uint8 *val) 
{
///    uint8_t err;											

	char		buff [8] ;									
	int			err ;											

///	printf ("i2c: %02X %02X\r\n",addr,reg) ; ///////////////	    

	i2cfd = open (i2cname,O_RDWR) ;							
    if (i2cfd < 0)
    {
        printf ("Error@: Cannot open %s (%s)\r\n",i2cname,strerror(errno)) ;
        err = ERROR_NOI2CPORT ;
    }
	else												// check for slave presence
	{
		ioctl (i2cfd, I2C_SLAVE, i2caddr >> 1) ; 		// slave addresses are 7 bit for /dev/i2c-1
		buff [0] = reg ;
		err = write (i2cfd,buff,1) ;					// first send the register address
		if (err >= 0)
		{
			err = read (i2cfd,buff,1) ;					// now read the byte
			if (err >= 0)
			{
				*val = (uint8) buff[0] ;
			}
		}
	}
	close (i2cfd) ;

///    if (err < 0) printf("ERROR: i2c read reg8 0x%.2x, 0x%.2x\n",i2caddr,reg);

    return err;
}

/////////////////////////////////////////////////////////////////////

int32 check_pic_status (int32 returnstatus)
{
	int32		status ;
	uint32		picid [2] ;
	uint32		picversion ;
	int32		temp ;

// check for PIC_A presence

	temp = (returnstatus & 0xffff) ; 				
	if (temp == 0)
	{
		returnstatus &= ~0xffff ;
		pclock = PCLOCK_A ;
		pdata  = PDATA_A ;
		entericspmode() ;
		memset ((void*)picid, 0, sizeof(picid)) ;
		icsp (readwords, picid, 0xff0000, 2, 0) ;
		if (picid[0] != 0x750e)
		{
			returnstatus |= ERROR_NOPIC ;
		}
		exiticspmode() ;
	}
	else if (temp == ERROR_PICPRESENT)
	{
		returnstatus &= ~0xffff ;
	}
	
// check for PIC_B presence

	temp = (returnstatus >> 16) ;
	if (temp == 0)
	{
		returnstatus &= ~0xffff0000 ;
		pclock = PCLOCK_B ;
		pdata  = PDATA_B ;
		entericspmode() ;
		memset ((void*)picid, 0, sizeof(picid)) ;
		icsp (readwords, picid, 0xff0000, 2, 0) ;

		if (picid[0] != 0x750e)
		{
			returnstatus |= ERROR_NOPIC << 16 ;
		}
		exiticspmode() ;
	}	
	else if (temp == ERROR_PICPRESENT)
	{
		returnstatus &= ~0xffff0000 ;
	}

	usleep (1200000) ;

// get PIC_A version

	if ((returnstatus & 0xffff) != ERROR_NOPIC)						// no errors yet
	{
		picversion = 0 ;
		status  = i2c_read_reg8 (PIC_ADDR_A, PIC_REG_VERSION_HIGH, (uint8*)&picversion) ;
		picversion <<= 8 ;
		status |= i2c_read_reg8 (PIC_ADDR_A, PIC_REG_VERSION_LOW,  (uint8*)&picversion) ;
		if (status < 0)
		{
			returnstatus |= ERROR_NOVERSION ;
		}
		else if (picversion == 0xffff || picversion == 0x0000)
		{
			returnstatus |= ERROR_NOVERSION ;
		}
		else
		{
			returnstatus |= picversion ;
		}
	}

// get PIC_B version

	if ((returnstatus >> 16) != ERROR_NOPIC)						// no errors yet
	{
		picversion = 0 ;
		status  = i2c_read_reg8 (PIC_ADDR_B, PIC_REG_VERSION_HIGH, (uint8*)&picversion) ;
		picversion <<= 8 ;
		status |= i2c_read_reg8 (PIC_ADDR_B, PIC_REG_VERSION_LOW,  (uint8*)&picversion) ;
		if (status < 0)
		{
			returnstatus |= ERROR_NOVERSION << 16 ;
		}
		else if (picversion == 0xffff || picversion == 0x0000)
		{
			returnstatus |= ERROR_NOVERSION << 16 ;
		}
		else
		{
			returnstatus |= picversion << 16 ;
		}
	}

	return (returnstatus) ;
}

uint32 get_return_value (uint32 retstat)
{
	int			x ;
	uint32		retval ;
	uint32		tempu ;
	
	retval = 0 ;

	for (x = 0 ; x < 2 ; x++)
	{
		printf ("  PIC_%c:  ", 'A' + x) ;
		tempu = retstat >> (x * 16) ;
		tempu &= 0xffff ;
		if (tempu == ERROR_NOPIC)
		{
			printf ("not present") ;
		}
		else if (tempu == ERROR_NOVERSION)
		{
			printf ("not programmed or version unknown") ;
		}
		else if (tempu < 0)
		{
			printf ("unknown error") ;
		}
		else
		{
			printf ("version %Xv%02X", tempu >> 8, tempu & 0xff) ;
		}
		printf ("\r\n") ;
	}
	
// reformat the version number

	tempu = retstat & 0xffff ;						// get PIC_A status
	if (tempu >= BASE_VERSION)						// valid version number
	{
		tempu -= BASE_VERSION ;						// if version was x.y0, get xy as a decimal
		tempu >>= 4 ;								// "
		tempu = (tempu >> 4) * 10 + (tempu & 0xf) ;
		if (tempu >= 2 && tempu <= 14)	
		{
			retval |= tempu ;
		}
		else
		{
			retval |= ERROR_NOVERSION ;
		}
	}
	else
	{
		retval |= tempu ;							// 0 / 1 for PIC absent / unknown
	}
	
	tempu = retstat >> 16 ;						// get PIC_B status
	if (tempu >= BASE_VERSION) 					// valid version number
	{
		tempu -= BASE_VERSION ;					// if version was x.y0, get xy as a decimal
		tempu >>= 4 ;							// "
		tempu = (tempu >> 4) * 10 + (tempu & 0xf) ;
		if (tempu >= 2 && tempu <= 14)	
		{
			retval |= tempu << 4 ;
		}
		else 
		{
			retval |= ERROR_NOVERSION << 4 ;
		}
	}
	else
	{
		retval |= tempu << 4 ;				// 0 / 1 for PIC absent / unknown
	}

	printf ("  Status: %d (0x%02X)\r\n", retval & 0xff, retval & 0xff) ;

	return (retval) ;	
}


