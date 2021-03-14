/* -------------------------------------------------------------------------------------------------- */
/* The WinterHill receiver: main.c                                                                    */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/* Copyright 2020 Brian Jordan, G4EWJ                                                                 */
/* -------------------------------------------------------------------------------------------------- */
/*
    This file is part of WinterHill

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

	This software is based on LongMynd, copyright 2019 by Heather Lomond, M0HMO
	Modified by Brian Jordan, G4EWJ, 2020 for the WinterHill dual serial NIM project.
*/

// winterhill main application 3v20
		
#define VERSIONX	"3v20" 		// main version ID
#define VERSIONX2	"a" 		// sub version ID


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "globals.h"
#include <netdb.h>
#include <ifaddrs.h>
#include <time.h>

#define CR		13
#define LF		10
#define TAB		9

/*

    This is the main routine for the BATC WinterHill project
    It receives 4 signals from 2 Serit NIMs.
    A PIC interfaces between the each NIM and the Raspberry Pi using SPI
    This routine uses modified WinterHill software
    Commands, transport stream output and status output are all via IP
*/

/*
    usage:  sudo ./winterhill-3v20 IP_Address IP_Port Interface_IP_Address VLCid1 VLCid2 VLCid3 VLCid4

    E.g.   sudo ./winterhill-3v20 192.168.1.230 9900 0 w1 w2 w3 w4
 
    Default operation:
    
    Port    Direction   Function
    ----------------------------------------------------------------------
    9901    OUT         send text info stream for all receivers
	9902	OUT			send 4 line rx status info 
    9903    OUT         same as 9901
	9904	OUT			same as 9902
	9920	IN			listen for manual QuickTune like commmands
    9921    IN          listen for QuickTune commands for receiver 1         
    9922    IN          listen for QuickTune commands for receiver 2        
    9923    IN          listen for QuickTune commands for receiver 3        
    9924    IN          listen for QuickTune commands for receiver 4
    9941    OUT         send transport stream for receiver 1    
    9942    OUT         send transport stream for receiver 2    
    9943    OUT         send transport stream for receiver 3    
    9944    OUT         send transport stream for receiver 4    
    9961    OUT         send LM info stream for receiver 1    
    9962    OUT         send LM info stream for receiver 2    
    9963    OUT         send LM info stream for receiver 3    
    9964    OUT         send LM info stream for receiver 4    
*/ 

#define EIT_PID				18
#define ESC					27
#define EVENTID				0						// for EIT
#define INFOPERIOD      	500 	               	// time in ms between info outputs 
#define MAXFREQ				2600000
#define MAXFREQSTOSCAN		16
#define MAXINFOS			100
#define MAXINICOMMANDS		16						// maximum number of commands in the ini file
#define MAXINPACKETS      	256						// packets in each input ring buffers
#define MAXOUTPACKETS      	256						// packets in the  output ring buffer
#define MAXPIDS				8						// numbers of allowed pids for a program
#define MAXSRSTOSCAN		16						// number of SRs to scan
#define MAXRECEIVERS       	4
#define MINFREQ				144000
#define MAXSR				45000
#define MINSR				25
#define NETWORK				0						// not needed by VLC for EIT
#define NULL_PID			8191
#define NULL2_PID			8190					// fake null packet insert by some modulators
#define	ON					1
#define OFF					0
#define PAT_PID				0
#define PORTINFOLMEX		1						// textual status for all receivers
#define PORTINFOMULTIRX		2						// 4 line receiver summary
#define PORTINFOLMEX2		3						// copy of 1
#define PORTINFOMULTIRX2	4						// copy of 2
#define PORTINFOLMBASE		60						// LongMynd textual status for receivers
#define PORTLISTENBASE		20						// listen for receive commands on this + RX number (1-4)
#define PORTTSBASE			40						// output TS to this + RX number
#define QO100NO				0
#define QO100BAND			1
#define QO100BEACON			2
#define QTHEADER			1						// QuickTune command header seen
#define SDT_PID				17
#define SERVICE_H262		0x02		
#define SERVICE_H264		0x1b
#define SERVICE_H265		0x24
#define SERVICE_MPA			0x03			
#define SERVICE_AAC			0x0f
#define SERVICE_AC3			0x77	// ???
#define TSID				0						// not needed by VLC for EIT
#define DAY0 				0xc957 					// 31 December 1999 in Julian days
#define WHHEADER			2						// WinterHill command header seen

#define MODE_ANYWHERE		0						// TS is sent to where the command came from
#define MODE_MULTICAST		1						// TS is sent to the multicast address
#define MODE_LOCAL			2						// TS is sent to the address supplied at startup
#define MODE_FIXED			3						// TS is sent to a fixed address

#define IP_OFFNET			0
#define IP_MYPC				1
#define IP_MYNET			2
#define IP_MULTI			3
	

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@  control structure for each of the total of 4 possible receivers on the total of 2 possible NIMs
//@  there are 5 structures: 0 is used by the system for IP control
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

struct rxcontrol
{
    uint8      		    receiver ;        	        	// 1-4 receiver number in the system
    uint8               nimreceiver ;              		// 1/2 = first/second receiver in each NIM
    uint8               xlnaexists ;            		// not all NIMs have external LNAs
    uint8               nim ;                   		// 1/2 = NIMA/NIMB
    char	            nimtype			[16] ;  		// FTS4334L, FTS4335
    uint8         		active ;						//  /1 = actively receiving or searching
    uint8               antenna ;               		// 1/2 = TOP/BOT
    uint8         		scanstate ;             		// searching/locked etc

	uint32				audiotype ;						// audio service type indicator
	char				commandip		[16] ;			// the IP address that the command came from
	uint32				commandreceivedtime ;			// the time at which the command arrived (ms)
	uint32				debug0 ;
	uint8				sendnullpackets ;	    		//  /1 = send null packets on the output UDP streams
	int32				demodfreq ;						// frequency offset detected by the demodulator
    uint32              enablefreqscan ;        		// scan the 'frequencies' list
    uint32      		frequencies [MAXFREQSTOSCAN] ;	// kHz; multiple frequencies may be scanned
	uint16              freqindex ;             		// the index of the current frequency in 'frequencies'
	uint8				eitcontinuity ;					// sequence number for injected EIT packets
	uint8				eitversion ;					// incrementing version number for injected EIT packets
	char				eitlist 		[64] ;  		// list of info item numbers to put into a pid 18 EIT packet; 0 = end of list
    uint32              enablesrscan ;          		// scan the 'symbolrates' list           
	uint32				errors_outsequence ;
	uint32				errors_insequence ;
	uint32				errors_restart ;
	uint32				errors_overflow ;
	uint32				errors_sync ;
	uint32				forbidden ;						// cannot send this TS to RPi VLC
	uint32				hardwarefreq ;					// frequency passed to the tuner
	uint32				highsideloc ;					// /1 = LO is on the high side
    uint16      		summaryport ;			    	// port for sending 4 line info 
	int					summarysock ;					// . . (same info for all receivers)
	struct sockaddr_in 	summarysockaddr ;	    			 
    uint16      		summary2port ;			
	int					summary2sock ;					
	struct sockaddr_in 	summary2sockaddr ;	    			 
    uint16      		lminfoport ;			   		// port for sending original LM $ info   	
	int					lminfosock ;		    			
	struct sockaddr_in 	lminfosockaddr ;	    			 
    uint16      		expinfoport ;			   		// port for sending expanded WH $ info 
	int					expinfosock ;		    		// 	. . (same for all receivers)	
	struct sockaddr_in 	expinfosockaddr ;	    			
    uint16      		expinfo2port ;			    		// future expansion
	int					expinfo2sock ;		    			 
	struct sockaddr_in 	expinfo2sockaddr ;	    			 
	uint32				insequence ;					// 4 bit counter inserted by the PIC for each packet
    char	    		interfaceaddress[16] ;			// network interface to use, if more than one is available
    char	    		ipaddress 		[16] ;			// address for all outgoing operations
	uint32				ipchanges ;						// incremented when a command comes from a different IP
	uint32				iptype ;						// destination IP type: MYPC, MYNET, OFFNET, MULTICAST
	uint32				lastmodulation ;				// last modcod / FEC seen
    uint16      		listenport ;	        		// port for incoming commands
	int					listensock ;		    		// 
	struct sockaddr_in 	listensockaddr ;	    		// 
	uint32				modechanges ;					// increments on new command, new callsign, new codec
    char	    		newipaddress 	[16] ;			// command came from a new IP address
	uint32				programcount ;					// number of programs in the TS
	int32				qo100locerror ;
	uint32				qo100mode ;						// 0 / 1 / 2 = NO / QO-100 band / QO-100 beacon
	uint32				requestedfreq ;					// the frequency in the incoming command
	uint32				requestedloc ;					// the local oscillator the incoming command
	uint32				requestedprog ;					// the program number in the incoming command
    uint32      		symbolrates [MAXSRSTOSCAN] ;	// kS;  multiple symbol rates may be scanned
	uint16              srindex ;               		// the index of the current symbol rate in 'symbolrates'
	uint32				timeoutholdoffcount ;			// info is sent a number of times after timing out
    uint16      		tsport ;			    		// port    for transport stream output
	int					tssock ;		    			// socket  for transport stream output
	struct sockaddr_in 	tssockaddr ;	    			//
	uint32				outsequence ;					// 4 bit counter inserted by each PIC 
	uint32				packetcountprogram ;			// total since the program started
	uint32				packetcountrx ;					// total for this reception
	uint32				nullpacketcountprogram ;		// total null packets since the program started
	uint32				nullpacketcountrx ;				// total null packets for this reception
	uint16				network ;						// TS network number; 0xffff for beacon; not needed for VLC EIT
	uint32				signalacquiredtime ;			// time when the received signal was first acquired
	uint32				signallosttime ;				// time when the received signal was lost
	uint32				timedouttime ;					// time when the transmission timeout occurred
	uint16				tsid ;							// TS ID; 0xaaaa for beacon; not needed for VLC EIT
	uint16				pmtpid ;						// program map table pid
	uint16				pcrpid ;						
	uint16				serviceid ;						// TS program service ID; 0x0001 for beacon; NEEDED for VLC EIT
	uint32				videotype ;						// video service type indicator
	uint32				vlcnextcount ;					// incremented when N command sent to VLC
	uint32				vlcstopcount ;					// incremented when S command sent to VLC
	uint32				vlcstopped ;					// S command sent to VLC
	uint32				xdotoolid ;						// xdotool ID for the VLC window			
    int32               rawinfos  [MAXINFOS] ;  		// raw values of the info items  
    char                textinfos [MAXINFOS][256] ;  	// formatted items for info or EIT output, indexed by parameter number  
} ; 


typedef struct
{
    uint8               data [188] ;
    union
    {
        uint32          statusreg ;
        struct
        {
            uint32      crc8            :8 ;
            uint32     	valid           :1 ;
            uint32      restart         :1 ;
            uint32      overflow        :1 ;
            uint32      filtering       :1 ;
            uint32      receiver        :2 ;
            uint32      spare0          :1 ;
            uint32      highwater       :1 ;
            uint32      crcnim          :1 ;          	// 1 = CRC provided by NIM
            uint32      crcstatus       :1 ;           
            uint32      spare1          :2 ;                                        
            uint32      nullpackets     :4 ;
            uint32      outsequence     :4 ;
            uint32      insequence      :4 ;
        } ;
    } ;
} packetx_t ;


struct eitx
{
    uint                sync            :8  ;
	
    uint                pid1208         :5  ;
    uint                tspriority      :1  ;
    uint                payloadstart    :1  ;    
    uint                tserror         :1  ;
	
    uint                pid0700         :8  ;

    uint                continuity      :4  ;
    uint                adaption        :2  ;
    uint                scrambling      :2  ;
	
    uint                pointer         :8  ;

    uint8               tableid             ;

    uint                filler1         :4  ;                
    uint                reserved0       :2  ;
    uint                filler2         :1  ;
    uint                syntax          :1  ;
	
    uint8               sectionlength       ;

    uint                servicehigh     :8  ;   // upper byte of service id
    uint                servicelow      :8  ;

    uint                currentnext     :1  ;
    uint                version         :5  ;
    uint                reserved1       :2  ;
	
    uint                section         :8  ;
    uint                lastsection     :8  ; 

    uint                tsidhigh        :8  ;   // upper byte of ts id
    uint                tsidlow	        :8  ;

    uint                networkhigh     :8  ;   // upper byte of network id
    uint                networklow      :8  ;

    uint                lastsegment     :8  ;
    uint                lasttable       :8  ;
    
    uint8               loop                ;

    uint8               filler0 [168]       ;
} ;

 
struct modinfo
{
    char    modtext [16] ;								// MODCOD gives modulation and FEC
    int32 	minmer ;									// required MER threshold for decode in tenths
} ;

 
const struct modinfo    modinfo_S [9] =
{
    {"",0},
    {"QPSK 1/2",17},
    {"QPSK 2/3",33},
    {"QPSK 3/4",42},
    {"",0},
    {"QPSK 5/6",51},
    {"QPSK 6/7",55},
    {"QPSK 7/8",58},
    {"",0}
} ;


const struct modinfo    modinfo_S2 [32] =
{                                                                                                                                                   
    {"",0},
    {"QPSK 1/4",   -23},
    {"QPSK 1/3",   -12},
    {"QPSK 2/5",    -3},
    {"QPSK 1/2",    10},
    {"QPSK 3/5",    22},
    {"QPSK 2/3",    31},
    {"QPSK 3/4",    40},
    {"QPSK 4/5",    47},
    {"QPSK 5/6",    52},
    {"QPSK 8/9",    62},
    {"QPSK 9/10",   64},
    {"8PSK 3/5",    55},
    {"8PSK 2/3",    66},
    {"8PSK 3/4",    79},
    {"8PSK 5/6",    94},
    {"8PSK 8/9",   107},
    {"8PSK 9/10",  110},
    {"16APSK 2/3",  90},
    {"16APSK 3/4", 102},
    {"16APSK 4/5", 110},
    {"16APSK 5/6",116},
    {"16APSK 8/9",129,},
    {"16APSK 9/10",131},
    {"32APSK 3/4",127},
    {"32APSK 4/5",136},
    {"32APSK 5/6",143},
    {"32APSK 8/9",157},
    {"32APSK 9/10",160},
    {"",0},
    {"",0},
    {"",0}
} ;

 
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@  global data 
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

volatile 	uint32				times[256] ;
volatile 	uint32				timpacks[256] ;
volatile 	uint32				timsyncs[256] ;
volatile	uint8				tin ;
volatile	uint8				tout ;
volatile	uint32				lastin ;

            char        		baseinterfaceaddress	[16] ;
            char				baseipaddress 			[16] ;                  // the IP address   for all I/O
            uint16				baseipport ;            	                    // the base IP port for all I/O
            char                buff     		        [256] ;
            char                commandreplybuff        [512] ;
            char                commandrxbuff  			[256] ;
            char                commandrxbuff2 			[256] ;
            char                commandrxbuff3 			[256] ;
const 		uint32 				daysinmonth 			[]   = {0,31,28,31,30,31,30,31,31,30,31,30,31} ;
			char				expandedtextinfo 		[65536] ;
            char				EITDEFAULTS				[64] = {13,12,6,9,12,18,0} ;
			struct eitx     	eit ;    				// EIT packet
			uint32				eitinsert ;				// an EIT packet with info is inserted into the TS
			uint32				eitremove ;				// EIT packets are removed from the incoming TS
            uint32				GLOBALNIM ; 
volatile	int32				lminfoutenabled ;		// enable info sending
volatile	uint32				i2csharedaccess ;		// controls i2c access
			pthread_t			info_thread ;
		    uint32      		lastinfotime ;			// time of last info transmission (ms)
			char				logfilename [64] ;		// name of the log file
			uint32				h265max ;				// maximum number of VLC windows to use the hardware decoder
			uint32				idletime ;				// receiver is disabled after this many seconds of inactivity
			uint32				inicommandcount ;		// number of commands in the ini file
volatile	int32				inicommandenabled ;		// enable ini command sending
			char				inicommands 			[MAXINICOMMANDS][256] ;	// commands in the ini file
			pthread_t			inicommand_thread ;		// used to send commands from the .ini file
			uint32				modex ;					// program and TS distribution mode
			uint32				null8190 ;				// PID 8190 is treated as a NULL packet
			uint32				nullremove ;			// NULL packets are not sent to VLC
			uint32				offnettime ;			// off net transmission is stopped after this many seconds
														// of no commands when sending off net
            uint32              peripherals_virtual_address ;	// virtual address of the peripherals
			uint32				rxbase ;				// the 4 receivers are numbered starting at this value
volatile	uint32				terminate ;
			pthread_t			tsproc_thread ;
volatile    uint32             	txpbindexin ;           // indexes for the UDP sending ring buffer       
volatile    uint32            	txpbindexout ;                                    
volatile	int32				tsprocenabled ;			// enable UDP packet sending
			uint32				qo100beaconfreq ;		// nominal frequency of the beacon
			uint32				vgxtone ;				// voltage generator X tone
			uint32				vgxen ;					// voltage generator X enable				--> bit0
			uint32				vgxsel ;				// voltage generator X low / high select	--> bit2
			uint32				vgytone ;				// voltage generator Y tone
			uint32				vgyen ;					// voltage generator Y enable				--> bit4
			uint32				vgysel ;				// voltage generator Y low / high select	--> bit6
			int					whfd ;
			char				nullpacket [188] ;			
			struct rxcontrol	rcv       				[MAXRECEIVERS+1] ;      // receivers 1-4; 0 is used by the system
			
/*
	i2csharedaccess		

	0: main loop has sole access, 	only main loop may change (to 2)
	1: main loop wants access,		only info loop may change (to 0)
	2: info loop has sole access,	only main loop may change (to 1) 
*/

#define I2CMAINHAS		0
#define I2CMAINWANTS	1
#define I2CINFOHAS		2
 
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@  function prototypes
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

			uint32			calculateCRC32				(uint8*, uint32) ;
            int32 			configsockets 				(uint32, uint32) ;
			void*			info_loop					(void*) ;
			void			getdatetime					(char*) ;
			int32			getiptype					(char*) ;
			void* 			inicommand_loop 			(void*) ;
			void			juliandate					(int32*, int32*) ;
			void			logit						(char*) ;
            uint32			monotime_ms					(void) ;
            int32 			opensocket 					(uint32, char*, uint16*, uint32, int*, struct sockaddr_in*) ;
			uint32			reverse						(uint32) ;
			void			setup_eit					(void*, uint32, char*) ;
            int             setup_io_map        		(void) ;
			void			setup_titlebar				(char*, uint32) ;
			void*			tsproc_loop					(void*) ;
			void			whexit						(int32) ;

void sig_handler (int signum)
{
	printf ("\r\nSignal %d\r\n", signum) ;
	
    terminate = 1 ;
}

    
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@  main process
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 
int main (int argc, char *argv[])
{
	char				buff [256] ;
	int					spi0interruptnumber ;
	int					spi5interruptnumber ;
	int					spi6interruptnumber ;
	int					fd ;
	FILE				*ip ;
	uint32				x ;
	uint32				y ;
	uint32				r ;
	char*				pos ;
	int32				rx ;
    int32               temp ;
    uint8				tempc0 ;
    uint8				tempc1 ;
	char				temps [512] ;
	int					status ;
	struct sockaddr_in	sourceaddress ;
	socklen_t			sourceaddresslength ;
	int32				goodx ;
	int32				headerx ;
	int32				rxx ;
	int32              	freqx ;
	int32              	locx  ;
    int32              	srx   ;
    int32		        antx  ;
	int32		        voltx ;
	int32				reqprogx ;
	uint8				nimOK ;
	uint8				xlnaOK ;
	uint8				chipid0910 ;
	uint8				devid0910 ;
	uint8				id6120 ;
	uint32				nowms ;
	uint32				tempu ;


	printf ("\r\n\r\n") ;
    printf ("============================================================================================\r\n") ;                         
    printf ("============================================================================================\r\n") ;                         
    printf ("BATC WinterHill 4 Channel Digital TV Receiver - version %s%s \r\n", VERSIONX, VERSIONX2) ;
    printf ("============================================================================================\r\n") ;                         

	terminate 			= 0 ;
	tsproc_thread 		= 0 ;
	info_thread			= 0 ;
	inicommand_thread 	= 0 ;

	whfd			= -1 ;
	eitremove		= 0 ;							// winterhill.ini file items
	eitinsert		= 0 ;
	null8190		= 0 ;
	nullremove		= 0 ;
	h265max			= 1 ;							// one H.265 hardware decoder allowed by default
    qo100beaconfreq	= 10491500 ;
    offnettime		= 3600 ;						// 1 hour timeout when sending off net
    idletime		= 0 ;							// do not switch off receivers after inactivity
	inicommandcount = 0 ;
	memset (inicommands, 0, sizeof(inicommands)) ;

    signal (SIGINT,  sig_handler);
    signal (SIGTERM, sig_handler);

	sprintf (logfilename, "/home/pi/winterhill/whlog.txt") ;
    
	sprintf (temps, "<<<<<<<<<< Program started:  winterhill-%s%s  ", VERSIONX, VERSIONX2) ;
	for (x = 0 ; x < (uint32)argc ; x++)
	{
		sprintf (temps+strlen(temps), "%s ", argv[x]) ;
	}
	logit (temps) ;

	if (geteuid() != 0)
	{
		logit ("This program must be run as sudo") ;
		printf ("This program must be run as sudo\r\n") ;
		printf ("\r\n") ;
		whexit (55) ;
	}

    if (argc != 8)
    {
        sprintf (temps, "Usage sudo ./winterhill-%s Output_IP_Address Base_IP_Port Interface_IP_Address ", VERSIONX) ;
        sprintf (temps+strlen(temps), "VLCid1 VLCid2 VLCid3 VLCid") ;
        logit (temps) ;
        printf ("Usage sudo ./winterhill-%s Output_IP_Address Base_IP_Port Interface_IP_Address ", VERSIONX) ;
        printf ("VLCid1 VLCid2 VLCid3 VLCid4 \r\n") ;
        printf ("\r\n") ;
        whexit (1) ;
    }

// look for winterhill.ini and parse it if present

	ip = fopen ("/home/pi/winterhill/winterhill.ini","rt") ;
	if (ip == 0)
	{
		printf ("/home/pi/winterhill/winterhill.ini not found\r\n") ;
	}
	else
	{
		printf ("Processing /home/pi/winterhill/winterhill.ini\r\n") ;
		while (!feof(ip))
		{
			memset (buff,0,sizeof(buff)) ;
			fgets (buff,sizeof(buff)-1,ip) ;
			pos = strchr (buff,CR) ;
			if (pos)
			{
				*pos = 0 ;
			}
			pos = strchr (buff,LF) ;
			if (pos)
			{
				*pos = 0 ;
			}
			pos = strchr (buff,'#') ;
			if (pos)
			{
				*pos = 0 ;
			}		

			do
			{
				pos = strchr(buff,' ') ; 
				if (pos)
				{
					strcpy (pos,pos+1) ;
				}
			} while (pos) ;		

			do
			{
				pos = strchr(buff,TAB) ; 
				if (pos)
				{
					strcpy (pos,pos+1) ;
				}
			} while (pos) ;

			if (strlen(buff))
			{
				pos = strchr (buff, '=') ;
				if (pos)
				{
					*pos = 0 ;
					if (strcasecmp(buff, "H265_MAX") == 0)
					{
						printf ("H265_MAX    %d\r\n", atoi(pos+1)) ;
						h265max = atoi (pos+1) ;						// maximum number of VLC windows to use the hardware decoder
					}			
					else if (strcasecmp(buff, "NULL_8190") == 0)
					{
						printf ("NULL_8190   %d\r\n", atoi(pos+1)) ;
						null8190 = atoi (pos+1) ;						// PID 8190 is treated as a NULL packet
					}			
					else if (strcasecmp(buff, "NULL_REMOVE") == 0)
					{
						printf ("NULL_REMOVE %d\r\n", atoi(pos+1)) ;
						nullremove = atoi (pos+1) ;						// NULL packets are not sent to VLC
					}			
					else if (strcasecmp(buff, "EIT_REMOVE") == 0)
					{
						printf ("EIT_REMOVE  %d\r\n", atoi(pos+1)) ;
						eitremove = atoi (pos+1) ;						// EIT packets are removed from the incoming TS
					}			
					else if (strcasecmp(buff, "EIT_INSERT") == 0)
					{
						printf ("EIT_INSERT  %d\r\n", atoi(pos+1)) ;
						eitinsert = atoi (pos+1) ;						// an EIT packet with info is inserted into the TS
					}			
					else if (strcasecmp(buff, "IDLE_TIME") == 0)
					{
						printf ("EIT_INSERT  %d\r\n", atoi(pos+1)) ;
						idletime = atoi (pos+1) ;						// disable receiver after no receive
					}			
					else if (strcasecmp(buff, "OFFNET_TIME") == 0)
					{
						printf ("OFFNET_TIME %d\r\n", atoi(pos+1)) ;
						offnettime = atoi (pos+1) ;						// stop off net sending after no commands
					}			
					else if (strcasecmp(buff, "CALIB_FREQ") == 0)
					{
						printf ("CALIB_FREQ  %d\r\n", atoi(pos+1)) ;
						qo100beaconfreq = atoi (pos+1) ;				// calibration frequency
					}			
					else if (strcasecmp(buff, "COMMAND") == 0)
					{
						printf ("COMMAND     %s\r\n", pos+1) ;
						if (inicommandcount < MAXINICOMMANDS)
						{
							strncpy (inicommands[inicommandcount++], pos+1, sizeof(inicommands[0])-1) ;	
						}												// save the command
					}			
					else
					{
						printf ("%-12s*unknown*\r\n", buff) ;
					}			
				}
			}
		}
		fclose (ip) ;
	    printf ("============================================================================================\r\n") ;                         
	}

// look for the I2C port

    fd = open ("/dev/i2c-1", O_RDWR) ;
    if (fd < 1)
    {
        logit ("Cannot open /dev/i2c-1; has the I2C port been enabled in RPi Configuration?") ;
        printf ("Cannot open /dev/i2c-1; has the I2C port been enabled in RPi Configuration?\r\n") ;
        printf ("\r\n") ;
        whexit (3) ;
    }
    close (fd) ;
 
// get the interrupt number for IRQ5; the driver will attach SPI0 / SPI6 interrupts to this

    ip = fopen ("/proc/interrupts", "rt") ;
    if (ip == 0)
    {
        logit ("Cannot open /proc/interrupts") ;
        printf ("Cannot open /proc/interrupts\r\n") ;
        printf ("\r\n") ;
        whexit (3) ;
    }

    spi0interruptnumber = 0 ;
    spi5interruptnumber = 0 ;
    spi6interruptnumber = 0 ;
    while (!feof(ip))
    {
        fgets (temps, sizeof(temps), ip) ;
        if (strstr(temps, "fe204a00.spi"))
        {
            spi5interruptnumber = atoi (temps) ;
        }
        if (strstr(temps, "fe204000.spi"))
        {
            spi0interruptnumber = atoi (temps) ;
        }
        if (strstr(temps, "fe204c00.spi"))
        {
            spi6interruptnumber = atoi (temps) ;
        }
    }
    fclose (ip) ;

    if (spi0interruptnumber)
    {
        logit ("The SPI0 device is enabled - this will interfere with the driver") ;
        printf ("The SPI0 device is enabled - this will interfere with the driver\r\n") ;
    }
    if (spi6interruptnumber)
    {
        logit ("The SPI6 device is enabled - this will interfere with the driver") ;
        printf ("The SPI6 device is enabled - this will interfere with the driver\r\n") ;
    }
    if (spi0interruptnumber || spi6interruptnumber)
    {
        logit ("Disable with RPi Configuration or comment out in /boot/config.txt") ;
        printf ("Disable with RPi Configuration or comment out in /boot/config.txt\r\n") ;
        printf ("\r\n") ;
        whexit (4) ;
    }

    if (spi5interruptnumber)
    {
        printf ("spi5 interrupt number is %d - the driver will attach SPI0 / SPI6 interrupt handlers to this\r\n", spi5interruptnumber) ;
    }
    else
    {
        logit  ("Cannot find the spi5 interrupt number - does /boot/config.text contain dtoverlay=spi5-1cs ?") ;
        logit  ("The driver needs to attach SPI0 / SPI6 interrupt handlers to this") ;
        logit  ("Note - spi5 may be shown as /dev/spidev3.0") ;
        printf ("Cannot find the spi5 interrupt number - does /boot/config.text contain dtoverlay=spi5-1cs ?\r\n") ;
        printf ("The driver needs to attach SPI0 / SPI6 interrupt handlers to this\r\n") ;
        printf ("Note - spi5 may be shown as /dev/spidev3.0 \r\n") ;
        printf ("\r\n") ;
        whexit (5) ;
    }

	    printf ("============================================================================================\r\n") ;                         

// look for the winterhill driver

    sprintf (temps,"/dev/whdriver-%s", VERSIONX) ;
    whfd = open (temps, O_RDWR) ;
    if (whfd < 0)
    {
        sprintf 
        (
	        temps,
        	"Cannot open /dev/whdriver-%s; has the whdriver-%s.ko kernel module been inserted?", 
        	VERSIONX, VERSIONX
        ) ;
        logit (temps) ;
        printf 
        (
        	"Cannot open /dev/whdriver-%s; has the whdriver-%s.ko kernel module been inserted?\r\n", 
        	VERSIONX, VERSIONX
        ) ;
        printf ("\r\n") ;
        whexit (2) ;
    }

// write spi5interruptnumber to the driver

    *(uint32*)buff = spi5interruptnumber ;
    write (fd, buff, 4) ;

// close and re-open the driver

	close (whfd) ;
    whfd = open (temps, O_RDWR) ;
    *(uint32*)buff = spi5interruptnumber ;			// do it again for good measure
    write (fd, buff, 4) ;

  
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@  clear data    
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    
    memset ((void*)&rcv,0,sizeof(rcv)) ;                    // clear the receiver control structures

    tsprocenabled	    = 0 ;
    lminfoutenabled     = 0 ;
    i2csharedaccess 	= I2CMAINHAS ;						// main loop has sole I2C access
	vgxen			    = 0 ;								// voltage generators
	vgxsel			    = 0 ;	
	vgxtone				= 0 ;
	vgyen			    = 0 ;
	vgysel				= 0 ;
	vgytone				= 0 ;
	modex 				= 0 ;

	memset (nullpacket, 0xff, sizeof(nullpacket)) ;
	nullpacket [0] = 0x47 ;
	nullpacket [1] = 0x1f ;
	nullpacket [2] = 0xff ;
	nullpacket [3] = 0 ;


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@  apply the supplied parameters
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    memset ((void*)baseipaddress,0,sizeof(baseipaddress)) ;
    strncpy (baseipaddress,argv[1],sizeof(baseipaddress)-1) ;               // base IP address
    if (atoi(baseipaddress) == 0)
    {
        strcpy (baseipaddress,"") ;	                		        		// no interface address
		modex = MODE_ANYWHERE ;												// TS sent to command IP
    }
	else 
	{
		temp = getiptype (baseipaddress) ;
		if (temp < 0)
		{
			logit ("IP address is invalid") ;
			printf ("IP address is invalid\r\n") ;
			whexit (3) ;
		}
		else if (temp == IP_MULTI)
		{
	        modex = MODE_MULTICAST ;                                    	// TS sent to multicast address
		}
		else if (temp == IP_MYPC)
		{
	    	modex = MODE_LOCAL ;											// TS sent to the local address
		}
		else
		{
	    	modex = MODE_FIXED ;											// TS sent to a fixed address
		}
	}		

	temp =  atoi (argv[2]) ;       		                        			// base IP port
	if ((temp < 1024) || (temp >= 65536))
    {
		logit ("IP port is invalid") ;
		printf ("IP port is invalid\r\n") ;
		whexit (2) ;
	}    	
	else
	{
		baseipport = (uint16) temp ;
	}

    memset ((void*)baseinterfaceaddress,0,sizeof(baseinterfaceaddress)) ;
    strncpy (baseinterfaceaddress,argv[3],sizeof(baseinterfaceaddress)-1) ; // default interface

    if (atoi(baseinterfaceaddress) == 0)
    {
        strcpy (baseinterfaceaddress,"") ;                 		        	// no interface address
    }
    else
    {
		temp = getiptype (baseinterfaceaddress) ;
		if (temp < 0)
		{
			logit ("Interface IP address is invalid") ;
			printf ("Interface IP address is invalid\r\n") ;
			whexit (3) ;
		}
		else if (temp != IP_MYPC)
		{	
			logit ("Interface IP address must be local\r\n") ;
			printf ("Interface IP address must be local\r\n") ;
			whexit (9) ;
		}
	}
    
	rxbase = baseipport % 100 ;
	if (rxbase != 0 && rxbase != 4 && rxbase != 8 && rxbase != 12)
	{
		sprintf (temps, "Base IP Port must be xx00 or xx04 or xx08 or xx12") ;
		logit (temps) ;
		printf ("%s\r\n", temps) ;
		whexit (37) ;
	}
	
	rcv[1].xdotoolid = atoi (argv[4]) ;										// xdotool window IDs
	rcv[2].xdotoolid = atoi (argv[5]) ;
	rcv[3].xdotoolid = atoi (argv[6]) ;
	rcv[4].xdotoolid = atoi (argv[7]) ;


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@  detect and initialise the chips in the NIMs
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	memset (nimspresent,  0, sizeof(nimspresent)) ;			// declared in nim.c
	memset (xlnaspresent, 0, sizeof(xlnaspresent)) ;		// "

    GLOBALNIM = NIM_A ;

   	nimspresent  [NIM_A] = true ;							// assume presence for the moment
   	xlnaspresent [NIM_A] = true ;

    status = nim_init (&nimOK, &xlnaOK, &chipid0910, &devid0910, &id6120) ;
    if (status == 0 && nimOK)
    {
    	nimspresent [NIM_A] = true ;

		rcv[1].nim			= NIM_A ;
		rcv[2].nim			= NIM_A ;
		rcv[1].receiver		= 1 ;							// receiver in system
		rcv[2].receiver		= 2 ;
		rcv[1].nimreceiver	= 1 ;							// receiver in NIM
		rcv[2].nimreceiver	= 2 ;
		rcv[1].scanstate	= STATE_IDLE ;
		rcv[2].scanstate	= STATE_IDLE ;

		sprintf (temps,"%s","") ;
		if (xlnaOK == 0)
		{
	    	xlnaspresent [NIM_A] = false ;

			sprintf (temps+strlen(temps),"NO") ;
			
			strcpy (rcv[1].nimtype,"FTS4335") ;
			strcpy (rcv[2].nimtype,"FTS4335") ;
		}
		else
		{
	    	xlnaspresent [NIM_A] = true ;

			strcpy (rcv[1].nimtype,"FTS4334L") ;
			strcpy (rcv[2].nimtype,"FTS4334L") ;

			if (xlnaOK & 2) 
			{
		    	rcv[1].xlnaexists = 1 ;
				sprintf (temps+strlen(temps),"TOP") ;
			}
			if (xlnaOK & 1) 
			{
    			rcv[2].xlnaexists = 1 ;
    			if (xlnaOK & 2)
    			{
    				sprintf (temps+strlen(temps),"/") ;
    			}
				sprintf (temps+strlen(temps),"BOT") ;
			}
		}

		printf 
		(
			"stv0910chipid = 0x%02X, stv0910devid = 0x%02X, stv6120id = %d, external lna = %s \r\n",
			chipid0910, devid0910, id6120, temps
		) ;
		sprintf (temps, "+++ NIM_A %s initialised +++", rcv[1].nimtype) ;
		logit (temps) ;
		printf ( "%s\r\n", temps) ;
	}
    else
    {
    	nimspresent  [NIM_A] = false ;
    	xlnaspresent [NIM_A] = false ;
    	sprintf (temps, "--- NIM_A could not be initialised ---") ;
    	logit (temps) ;
    	printf ("%s\r\n", temps) ;
    }

    GLOBALNIM = NIM_B ;

   	nimspresent  [NIM_B] = true ;							// assume presence for the moment
   	xlnaspresent [NIM_B] = true ;

    status = nim_init (&nimOK, &xlnaOK, &chipid0910, &devid0910, &id6120) ;
    if (status == 0 && nimOK)
    {
    	nimspresent [NIM_B] = true ;

		rcv[3].nim			= NIM_B ;
		rcv[4].nim			= NIM_B ;
		rcv[3].receiver		= 3 ;
		rcv[4].receiver		= 4 ;
		rcv[3].nimreceiver	= 1 ;
		rcv[4].nimreceiver	= 2 ;
		rcv[3].scanstate	= STATE_IDLE ;
		rcv[4].scanstate	= STATE_IDLE ;

		sprintf (temps,"%s","") ;
		if (xlnaOK == 0)
		{
	    	xlnaspresent [NIM_B] = false ;

			sprintf (temps+strlen(temps),"NO") ;

			strcpy (rcv[3].nimtype,"FTS4335") ;
			strcpy (rcv[4].nimtype,"FTS4335") ;
		}
		else
		{
	    	xlnaspresent [NIM_B] = true ;

			strcpy (rcv[3].nimtype,"FTS4334L") ;
			strcpy (rcv[4].nimtype,"FTS4334L") ;

			if (xlnaOK & 2) 
			{
		    	rcv[3].xlnaexists = 1 ;
				sprintf (temps+strlen(temps),"TOP") ;
			}
			if (xlnaOK & 1) 
			{
    			rcv[4].xlnaexists = 1 ;
    			if (xlnaOK & 2)
    			{
    				sprintf (temps+strlen(temps),"/") ;
    			}
				sprintf (temps+strlen(temps),"BOT") ;
			}
		}

		printf 
		(
			"stv0910chipid = 0x%02X, stv0910devid = 0x%02X, stv6120id = %d, external lna = %s \r\n",
			chipid0910, devid0910, id6120, temps
		) ;
		sprintf (temps, "+++ NIM_B %s initialised +++", rcv[1].nimtype) ;
		logit (temps) ;
		printf ("%s\r\n", temps) ;
	}
    else
    {
    	nimspresent  [NIM_B] = false ;
    	xlnaspresent [NIM_B] = false ;
    	sprintf (temps, "--- NIM_B could not be initialised ---") ;
    	logit (temps) ;
    	printf ("%s\r\n", temps) ;
    }

    printf ("============================================================================================\r\n") ;                         

    
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 set up default IP configuration
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    for (rx = 0 ; rx <= MAXRECEIVERS ; rx++)
    {
    	if (rx == 0)
		{
			strcpy (rcv[rx].ipaddress, "127.0.0.1") ;					// rx0 always outputs to localhost
		}
		else
		{			
   			strcpy (rcv[rx].ipaddress, baseipaddress) ;
		}
		rcv[rx].iptype = getiptype (rcv[rx].ipaddress) ;

        rcv[rx].listenport 		= baseipport + PORTLISTENBASE + rx ;	// listen for commands
        rcv[rx].lminfoport  	= baseipport + PORTINFOLMBASE + rx ;	// send original LM $ info 				
        rcv[rx].tsport 			= baseipport + PORTTSBASE + rx ;		// send TS
        rcv[rx].summaryport  	= baseipport + PORTINFOMULTIRX ;		// send multi rx summary	
        rcv[rx].summary2port  	= baseipport + PORTINFOMULTIRX2 ;		// send multi rx summary	
        rcv[rx].expinfoport  	= baseipport + PORTINFOLMEX ;			// send expanded LM $ info 				
        rcv[rx].expinfo2port  	= baseipport + PORTINFOLMEX2 ;			// same info on another port 			
    }


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 // set up all incoming and outgoing UDP sockets    
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    
	for (rx = 0 ; rx <= MAXRECEIVERS ; rx++)
	{
		status = configsockets (rx, ON) ;
		if (status) 
		{
			sprintf (temps, "Error setting up UDP sockets for receiver %d",rx) ;
			logit (temps) ;
			printf ("Error setting up UDP sockets for receiver %d\r\n",rx) ;
			whexit (10) ;
		}
	}
    
    
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 create threads
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	status = pthread_create (&inicommand_thread, 0, inicommand_loop, 0) ;
	if (status != 0)
	{
		logit ("Cannot create thread for inicommand_loop") ;
		printf ("Cannot create thread for inicommand_loop\r\n") ;
		whexit (86) ;
	}	
	
	status = pthread_create (&tsproc_thread, 0, tsproc_loop, 0) ;
	if (status != 0)
	{
		logit ("Cannot create thread for tsproc_loop") ;
		printf ("Cannot create thread for tsproc_loop\r\n") ;
		whexit (87) ;
	}	

	status = pthread_create (&info_thread, 0, info_loop, 0) ;
	if (status != 0)
	{
		logit ("Cannot create thread for info_loop\r\n") ;
		printf ("Cannot create thread for info_loop\r\n") ;
		whexit (88) ;
	}	

    
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 main process
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	status  = i2c_read_pic8  (PIC_ADDR_0, 0x96, &tempc0) ;
	status |= i2c_read_pic8  (PIC_ADDR_0, 0x97, &tempc1) ;
	if (status != 0)
	{
		logit  ("PIC_A not found - cannot continue") ;
		printf ("PIC_A not found - cannot continue\r\n") ;
		whexit (46) ;
	}
	else
	{
		sprintf (temps, "PIC_A version is %0Xv%02X", tempc1, tempc0) ;
		logit (temps) ;
		printf ("%s\r\n", temps) ;
	}
	
	status  = i2c_read_pic8  (PIC_ADDR_1, 0x96, &tempc0) ;
	status |= i2c_read_pic8  (PIC_ADDR_1, 0x97, &tempc1) ;
	if (status != 0)
	{
		logit  ("PIC_B not found") ;
		printf ("PIC_B not found\r\n") ;
		rcv[3].nim 			= 0 ;							// disable RX3 and RX4
		rcv[3].receiver 	= 0 ;
		rcv[3].nimreceiver 	= 0 ;
		rcv[4].nim 			= 0 ;
		rcv[4].receiver 	= 0 ;
		rcv[4].nimreceiver 	= 0 ;
	}
	else
	{
		sprintf (temps, "PIC_B version is %0Xv%02X", tempc1, tempc0) ;
		logit (temps) ;
		printf ("%s\r\n", temps) ;
	}
	
    printf ("============================================================================================\r\n") ;                         

	i2csharedaccess 	= I2CINFOHAS ;						// give info loop sole I2C access

    printf ("============================================================================================\r\n") ;                         
	printf ("\r\n\r\n\r\n\r\n\r\n\r\n\r\n") ;
	
	lastinfotime 	 	= monotime_ms() ;
	lminfoutenabled  	= 1 ; 								// enable the 4 line status display
	inicommandenabled 	= 1 ;								// enable thread to send ini commands

// main loop

    while (1)
    {
    	usleep (100 * 1000) ;

// check for CTRL-C and other termination commands

		if (terminate)
		{
			lminfoutenabled = -1 ;
			tsprocenabled = -1 ;
			usleep (1000 * 1000) ;
		
			printf ("\r\n\r\nTerminating . . . \r\n") ;

			if (inicommand_thread)
			{
				pthread_join (inicommand_thread, 0) ;
				inicommand_thread = 0 ;
			}
			if (tsproc_thread)
			{
				pthread_join (tsproc_thread, 0) ;
				tsproc_thread = 0 ;
			}
			if (whfd >= 0)
			{
				close (whfd) ;
				whfd = -1 ;
			}
			printf ("WinterHill terminated\r\n\r\n") ;
			whexit (0) ;
		}
               
// check for commands received         
        
        for (r = 0 ; r <= MAXRECEIVERS ; r++)
        {
			goodx   		=  0  ;
			headerx 		= -1 ;
			rxx   			= -1 ;
            freqx 			= -1 ;
            locx  			=  0 ;
            srx   			= -1 ;
            antx  			= -1 ;
            voltx 			= -1 ;
            reqprogx		= 0 ;
 
            memset (commandrxbuff2,0,sizeof(commandrxbuff)) ;
			memset ((void*)&sourceaddress, 0, sizeof(sourceaddress)) ;
            sourceaddresslength = sizeof (sourceaddress) ;
			status = recvfrom 	
            (
			    rcv[r].listensock,commandrxbuff2, sizeof(commandrxbuff2)-1,MSG_DONTWAIT,
                (struct sockaddr*)&sourceaddress, &sourceaddresslength
            ) ;

            if (status > 0)                                                         // common command interface
            {
				commandrxbuff2 [status] = 0 ;

				y = 0 ;
				for (x = 0 ; x < strlen(commandrxbuff2) ; x++)						// convert to upper case
				{
					if (commandrxbuff2[x] >= ' ')
					{
						commandrxbuff[y]  = toupper (commandrxbuff2[x]) ;			// upper case with non-printing removed
						commandrxbuff3[y] = commandrxbuff2[x] ;						// non printing removed, for reply
						y++ ;
					}
				}
				commandrxbuff  [y] 	= 0 ;
				commandrxbuff3 [y] 	= 0 ;

// parse the command

///                if (r == 0)															// pseudo QuickTune command
				{
					pos = strstr (commandrxbuff, "[TO@WH]") ;
					if (pos)
					{
						headerx = WHHEADER ;
						pos = strstr (commandrxbuff, "RCV=") ;
						if (pos)
						{
							pos += strlen ("RCV=") ;
							rxx  = atoi (pos) ;
							if (r)
							{
								rxx -= rxbase ;
							}
							if (rxx != (int)r && r)
							{
								headerx = 0 ;
							}
						}
	                   	else
    	               	{
							headerx = 0 ;
            	       	}
					}
					else if (strncmp (commandrxbuff, "[GLOBALMSG]", 11) == 0)      		// QuickTune command
					{	
						headerx = QTHEADER ;
					}
				}
				
				pos = strstr (commandrxbuff,"FREQ=") ;
				if (pos)
				{
					pos += strlen ("FREQ=") ;
					freqx = atoi (pos) ;
				}

				pos = strstr (commandrxbuff,"OFFSET=") ;
				if (pos)
				{
					pos += strlen ("OFFSET=") ;
					locx = atoi (pos) ;
				}

				pos = strstr (commandrxbuff,"SRATE=") ;
				if (pos)
				{
					pos += strlen ("SRATE=") ;
					srx = atoi (pos) ;
				}

				pos = strstr (commandrxbuff,"FPLUG=") ;
				if (pos)
				{
					pos += strlen ("FPLUG=") ;
					antx = *pos ;
					if (antx == 'A')
					{
						antx = 1 ;
					}
					else if (antx == 'B')
					{
						antx = 2 ;
					}
					else
					{
						antx = -1 ;
					}
				}

				pos = strstr (commandrxbuff,"PRG=") ;
				if (pos)
				{
					pos += strlen ("PRG=") ;
					reqprogx = atoi (pos) ;
				}

				pos = strstr (commandrxbuff,"VGX=") ;
				if (pos)
				{	
					pos += strlen ("VGX=") ;
					memset ((void*)temps, 0, sizeof(temps)) ;
					sscanf (pos, "%s", temps) ;
					if (strcmp(temps, "OFF") == 0)
					{
						vgxen  	= 0 ;									
						vgxsel  = 0 ;
						vgxtone = 0 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "LO") == 0)
					{
						vgxen   = 1 ;									
						vgxsel  = 0 ;
						vgxtone = 0 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "HI") == 0)
					{
						vgxen   = 1 ;									
						vgxsel  = 1 ;
						vgxtone = 0 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "LOT") == 0)
					{
						vgxen   = 1 ;									
						vgxsel  = 0 ;
						vgxtone = 1 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "HIT") == 0)
					{
						vgxen   = 1 ;									
						vgxsel  = 1 ;
						vgxtone = 1 ;
						voltx   = 1 ;
					}
					else
					{
						voltx   = 0 ;
					}					
				}

				pos = strstr (commandrxbuff,"VGY=") ;
				if (pos)
				{	
					pos += strlen ("VGY=") ;
					memset ((void*)temps, 0, sizeof(temps)) ;
					sscanf (pos, "%s", temps) ;
					if (strcmp(temps, "OFF") == 0)
					{
						vgyen  	= 0 ;									
						vgysel  = 0 ;
						vgytone = 0 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "LO") == 0)
					{
						vgyen   = 1 ;									
						vgysel  = 0 ;
						vgytone = 0 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "HI") == 0)
					{
						vgyen   = 1 ;									
						vgysel  = 1 ;
						vgytone = 0 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "LOT") == 0)
					{
						vgyen   = 1 ;									
						vgysel  = 0 ;
						vgytone = 1 ;
						voltx   = 1 ;
					}
					else if (strcmp(temps, "HIT") == 0)
					{
						vgyen   = 1 ;									
						vgysel  = 1 ;
						vgytone = 1 ;
						voltx   = 1 ;
					}
					else
					{
						voltx   = 0 ;
					}					
				}

				pos = strstr (commandrxbuff,"VOLTAGE=") ;
				if (pos)
				{
					pos += strlen ("VOLTAGE=") ;
					voltx = atoi (pos) ;
					if (voltx == 0)
					{
						vgxen   = 0 ;									
						vgxsel  = 0 ;
						voltx   = 1 ;
					}
					else if (voltx == 13)
					{
						vgxen   = 1 ;									
						vgxsel  = 0 ;
						voltx   = 1 ;
					}
					else if (voltx == 18)
					{
						vgxen   = 1 ;									
						vgxsel  = 1 ;
						voltx   = 1 ;
					}
					else 
					{
						voltx = 0 ;									// error
					}                        
				}

				pos = strstr (commandrxbuff,"22KHZ=") ;
				if (pos)
				{
					pos += strlen ("22KHZ=") ;
					if (strncmp(pos,"OFF",3) == 0)
					{
						vgxtone = 0 ;
					}
					else if (strncmp(pos,"ON",2) == 0)
					{
						vgxtone = 1 ;
					}
				}	

				if (r == 0)
				{
					if (rxx > 0 && rxx <= MAXRECEIVERS)
					{
						rx = rxx ;															// pseudo QT command
					}
					else
					{
						rx = 0 ;
						rxx = 0 ;
					}
       		    }                     
       		    else
       		    {
       		    	rx  = r ;
       		    	rxx = r ;
       		    }

				if (headerx >= 0)
				{
					sprintf 
					(
						temps, 
						"Command received on port %d (BASE+%d) for RX%d from %s:%d", 
						rcv[r].listenport, rcv[r].listenport - baseipport, rx, inet_ntoa(sourceaddress.sin_addr), sourceaddress.sin_port
					) ;
				}
				else
				{
					sprintf 
					(
						temps, 
						"Unrecognised data received on port %d (BASE+%d) from %s:%d", 
						rcv[r].listenport, rcv[r].listenport - baseipport, 
						inet_ntoa(sourceaddress.sin_addr), sourceaddress.sin_port
					) ;
					if (r)
					{
						logit (temps) ;
						logit (commandrxbuff3) ;
					} 
				}
				if (rx)
				{
					printf 
					(
						"\r\nFrom%d: %s:%d\r\n", 
						r, inet_ntoa(sourceaddress.sin_addr), sourceaddress.sin_port
					) ;
				    printf ("%s\r\n", commandrxbuff2) ;											
				}
				
                if (rx > 0 && headerx > 0 && freqx >= 0 && locx >= 0 && srx > 0 && antx >= 0 && voltx != 0)		// voltx=0 = error
				{
					if (freqx < locx)
					{
						rcv[rx].highsideloc = 1 ;										// LO is on the high side
					}
					else
					{
						rcv[rx].highsideloc = 0 ;										// LO is on the low side
					}
					if (((abs(freqx - locx) >= MINFREQ) && (abs(freqx - locx) <= MAXFREQ)) || freqx == 0)
					{
						if ((srx >= MINSR) && (srx <= MAXSR))
						{
	                     	goodx = 1 ;													// good command

							if (rcv[rx].vlcstopped == 0)
							{
								rcv[rx].vlcstopped = 1 ;
								rcv[rx].vlcstopcount++ ;
								rcv[rx].rawinfos[STATUS_VLCSTOPS] = rcv[rx].vlcstopcount ;
								if (rcv[rx].xdotoolid)
								{
									sprintf (temps,	"xdotool windowfocus --sync %d key --window %d s", rcv[rx].xdotoolid, rcv[rx].xdotoolid) ;
									system (temps) ;										// send STOP to VLC
								}
							}
							if ((rx & 1) && (abs(freqx - qo100beaconfreq) <= 200))	
							{
								rcv[rx].qo100mode     = QO100BEACON ;
								freqx 			      = qo100beaconfreq ;
								rcv[rx].qo100locerror = 0 ;
							}
							else if ((freqx >= 10490000) && (freqx < 10500000))			// QO-100
							{
								rcv[rx].qo100mode 	= QO100BAND ;
							}
							else
							{
								rcv[rx].qo100mode   = QO100NO ;
							}

							rcv[rx].requestedfreq   = freqx ;                            
							rcv[rx].requestedloc 	= locx ;
							if (freqx != 0)
							{                            
								rcv[rx].hardwarefreq	= abs (rcv[rx].requestedfreq - rcv[rx].requestedloc) ;
								if (rcv[rx].qo100mode == QO100BAND)
								{
									rcv[rx].hardwarefreq -= rcv[rx].qo100locerror ;
								}
							}
							else
							{
								rcv[rx].hardwarefreq = 0 ;
							}
							rcv[rx].frequencies[0]  = freqx ;
							rcv[rx].freqindex       = 0 ;
							rcv[rx].enablefreqscan  = 0 ;
							rcv[rx].symbolrates[0]  = srx ;
							rcv[rx].srindex         = 0 ;
							rcv[rx].enablesrscan    = 0 ;
							rcv[rx].antenna         = antx ;
							rcv[rx].pmtpid			= 0 ;
							rcv[rx].scanstate       = 0 ;
							rcv[rx].requestedprog	= reqprogx ;
    	            		rcv[rx].forbidden 	    = 0 ;
							if (rcv[rx].receiver)										// see if receiver exists
							{
								rcv[rx].active      = 1 ;
								memset ((void*)&rcv[rx].rawinfos,0,sizeof(rcv[rx].rawinfos)) ;
								memset ((void*)&rcv[rx].textinfos,0,sizeof(rcv[rx].textinfos)) ;
								memcpy ((void*)&rcv[rx].eitlist,EITDEFAULTS,sizeof(EITDEFAULTS)) ;  
								rcv[rx].modechanges++ ;									// count a mode change

								y = STATUS_ANTENNA ;
								rcv[rx].rawinfos[y] = antx ;
			
								rcv[rx].signalacquiredtime 		= 0 ;
								rcv[rx].signallosttime 			= 0 ;
								rcv[rx].packetcountrx  	   		= 0 ;					// clear packet count				
								rcv[rx].nullpacketcountrx  		= 0 ;					// clear null packet count				
								rcv[rx].errors_sync 			= 0 ;
								rcv[rx].errors_insequence		= 0 ;
								rcv[rx].lastmodulation			= 0 ;

								temp = ((rx - 1) & 2) + 1 ;								// force 1 or 3 
								rcv[temp].errors_outsequence	= 0 ;
								rcv[temp].errors_restart		= 0 ;
								
								if (i2csharedaccess != I2CMAINHAS)
								{
									if (i2csharedaccess == I2CINFOHAS)
									{
										i2csharedaccess = I2CMAINWANTS ;
									}
									while (i2csharedaccess != I2CMAINHAS)
									{
										usleep (10 * 1000) ;
									}	
								}					
												
								GLOBALNIM = rcv[rx].nim ;
								stv6120_init 							// configure receiver
								(
									rcv[rx].nimreceiver, rcv[rx].hardwarefreq, 
									rcv[rx].antenna,	 rcv[rx].symbolrates[0]				
								) ;
								
								if (rcv[rx].qo100locerror && rcv[rx].qo100mode != QO100NO)
								{
									temp = 1 ;							// in the QO100 band 
								}										// and rx has been calibrated
								else
								{
									temp = 0 ;
								}
								
								stv0910_setup_receive 					// configure demodulator
								(
									rcv[rx].nimreceiver, 
									rcv[rx].symbolrates[0], 
									temp								// restrict frequency scan when calibrated
								) ; 

								stv0910_start_scan (rcv[rx].nimreceiver) ;       						// look for a signal

								y = STATUS_STATE ;
								rcv[rx].scanstate  	= STATE_SEARCH ;
								rcv[rx].rawinfos[y] = STATE_SEARCH ;
								tsprocenabled 		= 1 ; 					// enable the packet processing
								lminfoutenabled 	= 1 ;


/*
        GLOBALNIM = NIM_A ;
                stv6120_print_settings() ;
                        printf ("\r\n") ;
                                GLOBALNIM = NIM_B ;
                                        stv6120_print_settings() ;
                                                printf ("\r\n") ;
                                                        GLOBALNIM = NIM_B ;
                                                                stvvglna_read_regs (0xce) ;
                                                                        stvvglna_read_regs (0xc8) ;
                                                                                printf ("\r\n") ;

       printf ("\r\n\r\n\r\n\r\n\r\n") ;
*/                                                                               

                                                                                

								
								printf ("\r\n\r\n\r\n\r\n\r\n\r\n\r\n") ;
								i2csharedaccess 	= I2CINFOHAS ;			// give I2C access back to info loop
							}
						}
					}
				}

				if (rxx > 0)										// don't reply to scan info
				{
					if (goodx)  
					{
   	    	    		sprintf (commandreplybuff,"[from@wh:GOOD] \"%s\"\r\n",commandrxbuff3) ;
						if (modex == MODE_ANYWHERE)
						{
							if (inet_addr(rcv[rx].ipaddress) != sourceaddress.sin_addr.s_addr)	// address change
							{
								rcv[rx].ipchanges++ ;											// count an IP change
								strcpy (rcv[rx].newipaddress, inet_ntoa(sourceaddress.sin_addr)) ;	// new address
							}
						}
						strcpy (rcv[rx].commandip, inet_ntoa(sourceaddress.sin_addr)) ;			// command address
						rcv[rx].commandreceivedtime  = monotime_ms() ;		// command arrival time
						rcv[rx].timedouttime = 0 ;					// reset time of timeout

// update the command time for other receivers going to the same command IP address

						for (x = 1 ; x <= MAXRECEIVERS ; x++)
						{
							if (rcv[x].active && rcv[x].commandreceivedtime)
							{
								if (strcmp(rcv[x].commandip, rcv[rx].commandip) == 0)
								{
									rcv[x].commandreceivedtime = rcv[rx].commandreceivedtime ;
								}
							}
						}						
					}
					else
					{
	       	    		sprintf (commandreplybuff,"[from@wh:BAD]  \"%s\"\r\n",commandrxbuff3) ;
					}        		
					if (r == 0 && rxx != 0)										// only reply to pseudo commands
					{
	   	    		    status = sendto	                           				// reply
	    	   	       	(
    	    	           	rcv[rx].listensock, commandreplybuff, strlen(commandreplybuff)+1, 0,
			    	        (struct sockaddr*) &sourceaddress, sizeof(sourceaddress)
       			    	) ;
       			    }
					if (goodx == 0)
					{
						sprintf 
						(
							temps, 
							"Bad data received on port %d (BASE+%d) from %s:%d", 
							rcv[r].listenport, rcv[r].listenport - baseipport, 
							inet_ntoa(sourceaddress.sin_addr), sourceaddress.sin_port
						) ;
						logit (temps) ;
						logit (commandrxbuff3) ;
       			   	}
       		   	}
			}
		}


// check for timeout for TS leaving the building

		for (rx = 1 ; rx <= MAXRECEIVERS ; rx++)
		{
			if (rcv[rx].active)
			{
				if (rcv[rx].scanstate != STATE_TIMEOUT && rcv[rx].scanstate != STATE_IDLE)
				{
					if (rcv[rx].iptype == IP_OFFNET) 				// sending off my subnet	
					{
						tempu = offnettime * 1000 ;
						if (monotime_ms() - rcv[rx].commandreceivedtime >= tempu)
						{
							rcv[rx].scanstate = STATE_TIMEOUT ;		
							rcv[rx].timeoutholdoffcount = 4 ;		// send info 4 times after timeout
							rcv[rx].commandreceivedtime = 0 ;
							rcv[rx].timedouttime = monotime_ms() ;	// time of timeout
							if (i2csharedaccess != I2CMAINHAS)		// acquire access to I2C
							{
								if (i2csharedaccess == I2CINFOHAS)
								{
									i2csharedaccess = I2CMAINWANTS ;
								}
								while (i2csharedaccess != I2CMAINHAS)
								{
									usleep (10 * 1000) ;
								}	
							}									
							GLOBALNIM = rcv[rx].nim ;
							stv6120_init 							// configure receiver
							(
								rcv[rx].nimreceiver, 0,				// turn off the receiver
								rcv[rx].antenna,	 rcv[rx].symbolrates[0]				
							) ;												
							i2csharedaccess = I2CINFOHAS ;			// give I2C access back to info loop
						}
					}
				}
			}
		}
        
// check for signal loss and set idle mode

        nowms = monotime_ms() ;
      	for (rx = 1 ; rx <= MAXRECEIVERS ; rx++)
		{
			temp = 0 ;
       		if (rcv[rx].active && rcv[rx].scanstate != STATE_IDLE)
       		{
       			if (rcv[rx].scanstate == STATE_LOST)
       			{
       				if (nowms - rcv[rx].signallosttime >= idletime * 1000)
       				{
						temp++ ;										// can go to idle (turn off receiver) 
					}
       			}
       			if (rcv[rx].scanstate == STATE_SEARCH)
       			{
       				if (nowms - rcv[rx].commandreceivedtime >= idletime * 1000)
       				{
						temp++ ;										// can go to idle
       				}
       			}	
       			if (rcv[rx].scanstate == STATE_TIMEOUT)
       			{
       				if (nowms - rcv[rx].timedouttime >= idletime * 1000)
       				{
						temp++ ;										// can go to idle
       				}
       			}	
       			if (temp && idletime)
       			{
					rcv[rx].scanstate   			= STATE_IDLE ;		
					rcv[rx].rawinfos[STATUS_STATE] 	= STATE_IDLE ;		
    	   			rcv[rx].signallosttime 			= 0 ;       			
					rcv[rx].timeoutholdoffcount 	= 4 ;	// send info 4 times after timeout
					if (i2csharedaccess != I2CMAINHAS)		// acquire access to I2C
					{
						if (i2csharedaccess == I2CINFOHAS)
						{
							i2csharedaccess = I2CMAINWANTS ;
						}
						while (i2csharedaccess != I2CMAINHAS)
						{
							usleep (10 * 1000) ;
						}	
					}									
					GLOBALNIM = rcv[rx].nim ;
					stv6120_init 							// configure receiver
					(
						rcv[rx].nimreceiver, 0,				// turn off the receiver
						rcv[rx].antenna,	 rcv[rx].symbolrates[0]				
					) ;												
					i2csharedaccess = I2CINFOHAS ;			// give I2C access back to info loop       		
    	   		}
    	   	}
       	}	               
    }	
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 get the type of an IP address
//@
//@	 Calling:	IP address (string)
//@
//@	 Return:	
//@				-1  error
//@				 0  IP_OFFNET
//@				 1  IP_MYPC
//@				 2  IP_MYNET
//@				 3  IP_MULTI
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int getiptype (char *testipaddress)
{ 
	struct ifaddrs 	*ifaddr ;
	struct ifaddrs 	*ifa ;
    int				s;
    int				family ;
    int				status ;
    int				mypcmatches ;
    int				mynetmatches ;
    uint32			temp ;
    char 			host [NI_MAXHOST] ;
	uint32			netmask ;
	
	mypcmatches 	= 0 ;
	mynetmatches 	= 0 ;
  
// get all the local IP addresses  
    
    status = getifaddrs(&ifaddr) ;
    if (status == -1) 
    {
        return (-1) ;
    }

	if (inet_addr(testipaddress) == INADDR_NONE)
	{
		return (-1) ;											// invalid
	}

	if (atoi(testipaddress) >= 224 && atoi(testipaddress) <= 239)
	{
		return (IP_MULTI) ;
	}

    for (ifa = ifaddr ; ifa != NULL ; ifa = ifa->ifa_next) 
    {
        if (ifa->ifa_addr == NULL)
        {
           continue ;
        }
        family = ifa->ifa_addr->sa_family ;
        if (family == AF_INET) 
        {
      		s = getnameinfo 
           	(
           		ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST
           	) ;
            if (s != 0) 
            {
                return (-1) ;
            }           
			netmask = ((struct sockaddr_in *)(ifa->ifa_netmask))->sin_addr.s_addr ;
            if (inet_addr(testipaddress) == inet_addr(host))
            {
				mypcmatches++ ;
            }
            temp = inet_addr (testipaddress) ^ inet_addr (host) ;
            temp &= netmask ;
            if (temp == 0)
            {
				mynetmatches++ ;
            }
        } 
    }

    if (mypcmatches)
    {
		return (IP_MYPC) ;
    }
    else if (mynetmatches)
    {
		return (IP_MYNET) ;
    }
    
   	return (IP_OFFNET) ;
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 open or close all the UDP sockets for a particular receiver
//@
//@	 Calling:	receiver number 		1-4
//@                                     0 is used by the system
//@				state					0/1 for close/open
//@
//@	 Return:	
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int32 configsockets (uint32 receiver, uint32 offon)
{
	int32		err ;
	int			rx ;

	rx  = receiver ;
	err = 0 ;

   	if (1)
   	{
		err |= opensocket 
		(
			offon, 
			rcv[rx].ipaddress,
			&rcv[rx].listenport,
			1,												// listening
			&rcv[rx].listensock,
			&rcv[rx].listensockaddr
		) ;
		err |= opensocket 
		(
			offon, 
			rcv[rx].ipaddress,
			&rcv[rx].summaryport,
			0,												// not listening
			&rcv[rx].summarysock,
			&rcv[rx].summarysockaddr
		) ;
		err |= opensocket 
		(
			offon, 
			rcv[rx].ipaddress,
			&rcv[rx].summary2port,
			0,												// not listening
			&rcv[rx].summary2sock,
			&rcv[rx].summary2sockaddr
		) ;
		err |= opensocket 
		(
			offon, 
			rcv[rx].ipaddress,
			&rcv[rx].lminfoport,
			0,												// not listening
			&rcv[rx].lminfosock,
			&rcv[rx].lminfosockaddr
		) ;
		err |= opensocket 
		(
			offon, 
			rcv[rx].ipaddress,
			&rcv[rx].expinfoport,
			0,												// not listening
			&rcv[rx].expinfosock,
			&rcv[rx].expinfosockaddr
		) ;
		err |= opensocket 
		(
			offon, 
			rcv[rx].ipaddress,
			&rcv[rx].expinfo2port,
			0,												// not listening
			&rcv[rx].expinfo2sock,
			&rcv[rx].expinfo2sockaddr
		) ;

		err |= opensocket 
		(
			offon,
			rcv[rx].ipaddress,
			&rcv[rx].tsport,
			0,												// not listening
			&rcv[rx].tssock,
			&rcv[rx].tssockaddr 
		) ;	
	}
	return (err) ;
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 open a UDP socket 
//@
//@	 Calling:	IP address
//@				IP port
//@				listening				/1 for receive ports; used for multicast	
//@				UDP socket
//@				UDP socket parameters
//@
//@	 Return:	0  = success
//@				!0 = failure
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int32 opensocket (uint32 offon, char* address, uint16* port, uint32 listening, int* sock, struct sockaddr_in* sockaddr)
{    
	int					status ;
	struct ip_mreq		mreq ;


    if (offon == OFF) 
    {
		if (*sock)
		{
///			printf ("Closing port %s:%d\r\n",address,*port) ;
			if ((getiptype(address) == IP_MULTI) && listening)
			{
///				printf ("Dropping membership\r\n") ;
     			mreq.imr_interface.s_addr = INADDR_ANY ;
		   	    mreq.imr_multiaddr.s_addr = inet_addr (address) ;
    	   		status = setsockopt (*sock,IPPROTO_IP,IP_DROP_MEMBERSHIP,(char*)&mreq,sizeof(mreq)) ;
	        	if (status < 0) 
		   	    {
	    	   	    printf ("Cannot drop membership for port %d \r\n",*port) ; 
	        	   	return (status) ;
				}
   			}
       	    close (*sock) ;
			*sock = 0 ;
        }
    }   
    else if (address[0] || listening)
	{
	 	status = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP) ; 
    	if (status < 0) 
	    {	
	        printf ("Cannot create socket for port %d \r\n",*port) ;
	        return (status) ;
	    }
	    else
	    {
			*sock = status ;
	    }

///		printf ("Opening port %s:%d\r\n",address,*port) ;
	        
	    status = 1 ; 	
	    status = setsockopt (*sock, SOL_SOCKET, SO_REUSEADDR, (char*)&status, sizeof(status)) ;
	    if (status < 0)
	    {
	        printf ("Cannot re-use socket for port %d \r\n",*port) ;		
	        return (status) ;
	    }	
	        
	    sockaddr->sin_family 			= AF_INET ; 
	    sockaddr->sin_port            	= htons (*port) ;
	   	sockaddr->sin_addr.s_addr		= INADDR_ANY ; 
	    memset ((void*)&sockaddr->sin_zero,0,sizeof(sockaddr->sin_zero)) ;         
	    status = bind (*sock,(const struct sockaddr*)sockaddr,sizeof(*sockaddr)) ;
	   	sockaddr->sin_addr.s_addr		= inet_addr (address) ; 

/*	
		if (listening == 0)
		{
		   	sockaddr->sin_addr.s_addr	= inet_addr (address) ; 
	    }
    	else
*/
		{
			if ((getiptype(address) == IP_MULTI) && listening)
			{        
///				printf ("Adding membership\r\n") ;
        		mreq.imr_interface.s_addr = INADDR_ANY ;
		   	    mreq.imr_multiaddr.s_addr = inet_addr (address) ;
    	   		status = setsockopt (*sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char*)&mreq,sizeof(mreq)) ;
	        	if (status < 0) 
		   	    {
	    	   	    printf ("Cannot add membership for port %d \r\n",*port) ; 
	        	   	return (status) ;
				}
   			}
   		}    
   	}
   	return (0) ;
}
    

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 Get monotonic time in milliseconds
//@
//@	 Calling:	
//@
//@	 Return:	time in milliseconds
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

uint32 monotime_ms() 
{  
    struct timespec 	tp ; 
    
    if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0) 
    {
        return (0) ;
    }
    return ((uint32) tp.tv_sec * 1000 + tp.tv_nsec / 1000000) ;
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 a thread to to send commands from the .ini file        
//@
//@	 Calling:	
//@
//@	 Return:	
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void* inicommand_loop (void* dummy)
{	
	uint32		x ;
	uint32		tempu ;
	int32		status ;
	(void)		dummy ;
	
	while (inicommandenabled == 0) 
	{
		usleep (1000) ;
	}

	if (inicommandcount)
	{
		printf ("Sending ini file commands\r\n") ;
		for (x = 0 ; x < inicommandcount ; x++)
		{
    		if ((strlen(inicommands[x]) == 1) && isdigit(inicommands[x][0]))	// single digit
    		{
				tempu = inicommands[x][0] & 0xf ;
				printf ("Delaying %d seconds\r\n", tempu) ;
				usleep (tempu * 1000 * 1000) ;				// delay tempu seconds
    		}
			else
			{
	    		printf ("%s\r\n", inicommands[x]) ;
    			status = sendto
    			(
		    		rcv[0].tssock, inicommands[x], strlen(inicommands[x]) + 1, 0,
			        (struct sockaddr*) &rcv[0].listensockaddr, sizeof(rcv[0].listensockaddr)
    			) ;
    			(void) status ;
	    		usleep (100 * 1000) ;
    		}
    	}
	}
	return (0) ;
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 a thread to check if it's time for an info output        
//@  display the receiver summary and sent info via udp
//@
//@	 Calling:	
//@
//@	 Return:	
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void* info_loop (void* dummy)
{
		int				err ;
		uint32			rx ;
		uint32			x ;
		uint32			y ;
		char			titlebar [4096] ;
		char			temps    [5000] ;
		char			temps2   [4096] ;
		char			temps3   [64] ;
		bool			bool1 ;
		bool			bool2 ;
		uint8			tempc ;	
		int32			temp ;
		uint32			tempu ;
		char			command       [256] ;
		char			output   	  [4096] ;
		char			outputonnet   [4096] ;
		char			outputoffnet  [4096] ;
		int32			status ;
static	uint32			counter ;
		uint32			thenms ;
		struct in_addr	sia ;
	
  		(void) dummy ;


	while (lminfoutenabled == 0) 
	{
		usleep (1000) ;
	}

   	thenms = monotime_ms() ;											// current time
	while (lminfoutenabled == 1)
    {    

		while (((monotime_ms() - thenms) < INFOPERIOD) || (i2csharedaccess != I2CINFOHAS))
		{
			if (i2csharedaccess == I2CMAINWANTS)
			{
				i2csharedaccess = I2CMAINHAS ;
			}
       		usleep (10 * 1000) ; 										// sleep 10ms
       	}
		thenms += INFOPERIOD ;

		counter++ ;														// for EIT injection

// output voltage commands

		tempc = 0 ;
		if (vgxen)  tempc |= 0x01 ;
		if (vgxsel) tempc |= 0x04 ;
		if (vgyen)  tempc |= 0x10 ;
		if (vgysel) tempc |= 0x40 ;
		tempc |= 0xaa ;													// turn on the valid bit for each item		
///		i2c_write_pic16 (PIC_ADDR_A, 0xc5, tempc, tempc ^ 0xff) ; 
		i2c_write_pic8 (PIC_ADDR_A, 0x94, tempc) ;
		i2c_write_pic8 (PIC_ADDR_A, 0x95, tempc ^ 0xff) ; 

// output 22kHz commands

		GLOBALNIM = NIM_A ;
		if (vgxtone == 0)
		{
			tempc = KHZ22OFF ;											// defined in stv0910.h	
		}
		else
		{
			tempc = KHZ22ON ;										
		}
		err = stv0910_write_reg (RSTV0910_P1_DISTXCFG, tempc) ;		

		if (vgytone == 0)
		{
			tempc = KHZ22OFF ;											// defined in stv0910.h	
		}
		else
		{
			tempc = KHZ22ON ;										
		}
		err = stv0910_write_reg (RSTV0910_P2_DISTXCFG, tempc) ;		

		(void) err ;

// status output

		strcpy (output, "") ;
	
		sprintf (output+strlen(output)," RX  STATUS   CALLSIGN           MER      D  FREQUENCY     SR  ") ;
		sprintf (output+strlen(output),"MODULATION   FPRO  CODE      ANT  PACKETS  ") ;
		sprintf (output+strlen(output),"%%NUL  NIMTYPE   ") ;
		sprintf (output+strlen(output),"TS DESTINATION") ;
		sprintf (output+strlen(output),"\r\n") ;

		sprintf (output+strlen(output)," --------------------------------------------------------------") ;
		sprintf (output+strlen(output),"-------------------------------------------") ;
		sprintf (output+strlen(output),"-----------------") ;
		sprintf (output+strlen(output),"--------------") ;
		sprintf (output+strlen(output),"\r\n") ;

		strcpy (outputonnet,  output) ;										// for transmission on net
		strcpy (outputoffnet, output) ;										// for transmission off net

		strcpy (expandedtextinfo, "") ;

        for (rx = 1 ; rx <= MAXRECEIVERS ; rx++)			       
        {
			strcpy (output,"") ;											// reset output line
	   		
	        if ((rcv[rx].active == 0)) 
    	    {
				memset ((void*)&rcv[rx].textinfos, 0, sizeof(rcv[rx].textinfos)) ;
				rcv[rx].scanstate              = STATE_IDLE ;
				rcv[rx].rawinfos[STATUS_STATE] = STATE_IDLE ;
				sprintf (rcv[rx].textinfos[STATUS_STATE], "idle") ;	
				continue ;
          	}
          	else
          	{
				y = STATUS_MODECHANGES ;
				rcv[rx].rawinfos[y] = rcv[rx].modechanges ;				// copy mode changes into infos
				sprintf (rcv[rx].textinfos[y], "%d", rcv[rx].rawinfos[y]) ;		
          		if (rcv[rx].ipchanges)									// IP address has changed
          		{
          			rcv[rx].ipchanges = 0 ;
					if (rcv[rx].expinfosock)
					{
						sprintf (temps, "$0,%d\r\n$99,1\r\n", rx) ;		// create IP changed info
		    		    status = sendto 						
	    		  		(
	    		  			rcv[rx].expinfosock, (char*)temps, strlen(temps) + 1, 0,
   			   				(struct sockaddr*) &rcv[rx].expinfosockaddr, sizeof(rcv[x].expinfosockaddr) 
						) ;
		    		    status = sendto 							
	    		  		(
	    		  			rcv[rx].expinfo2sock, (char*)temps, strlen(temps) + 1, 0,
   			   				(struct sockaddr*) &rcv[rx].expinfo2sockaddr, sizeof(rcv[x].expinfo2sockaddr) 
						) ;
						sprintf (temps, "$0,%d\r\n$99,2\r\n",rx) ;		// send it twice
	    			    status = sendto 						
	    		  		(
		    	  			rcv[rx].expinfosock, (char*)temps, strlen(temps) + 1, 0,
   				   			(struct sockaddr*) &rcv[rx].expinfosockaddr, sizeof(rcv[rx].expinfosockaddr) 
						) ;
	    			    status = sendto 								
	    		  		(
		    	  			rcv[rx].expinfo2sock, (char*)temps, strlen(temps) + 1, 0,
   				   			(struct sockaddr*) &rcv[rx].expinfo2sockaddr, sizeof(rcv[rx].expinfo2sockaddr) 
						) ;
					}
					configsockets (rx, 0) ;							// close all current sockets for this rx
					strcpy (rcv[rx].ipaddress, rcv[rx].newipaddress) ;
					strcpy (rcv[rx].newipaddress, "") ;				// copy the new IP address	
					status = configsockets (rx, 1) ;				// re-open all sockets					
					if (status != 0)
					{
						printf ("Error when re-opening sockets\r\n") ;
					}
					rcv[rx].iptype = getiptype (rcv[rx].ipaddress) ;	// see if the commanding IP address is local
				}							
				
				tempc = rcv[rx].scanstate ;
                GLOBALNIM = rcv[rx].nim ;                                           // set NIM_A or NIM_B
                y = STATUS_STATE ;                                                  // parameter 1
				if (rcv[rx].scanstate != STATE_TIMEOUT && rcv[rx].scanstate != STATE_IDLE)   
				{
    	            stv0910_read_scan_state (rcv[rx].nimreceiver, &tempc) ;        	// get the scan state
                	if (tempc == STATE_SEARCH)
                	{
                		if (rcv[rx].scanstate != STATE_LOST)
                		{
                			if (rcv[rx].signalacquiredtime)
                			{
		   	            		tempc = STATE_LOST ;
		   	            		rcv[rx].signallosttime = monotime_ms() ;
		   	            		rcv[rx].signalacquiredtime = 0 ;					// reset so that next 
		   	            	}														// aquisition will reset NULLs
		   	            }
		   	            else
		   	            {
		   	            	tempc = STATE_LOST ;   				   	            	
						}       	        	
					}
				}
       	        rcv[rx].scanstate 	= tempc ;
           	    rcv[rx].rawinfos[y] = tempc ;				
                switch (tempc)
                {
                    case STATE_IDLE 		: sprintf (rcv[rx].textinfos[y],"idle")    ;  break ;      
                    case STATE_TIMEOUT		: sprintf (rcv[rx].textinfos[y],"timeout") ;  break ;      
                    case STATE_LOST 		: sprintf (rcv[rx].textinfos[y],"lost")    ;  break ;      
                    case STATE_SEARCH 		: sprintf (rcv[rx].textinfos[y],"search")  ;  break ;      
                    case STATE_HEADER_S2	: sprintf (rcv[rx].textinfos[y],"header")  ;  break ;      
                    case STATE_DEMOD_S2 	: sprintf (rcv[rx].textinfos[y],"DVB-S2")  ;  break ;    
                    case STATE_DEMOD_S 		: sprintf (rcv[rx].textinfos[y],"DVB-S")   ;  break ;     
                    default 			    : sprintf (rcv[rx].textinfos[y],"%s","")   ;  break ;   
                }
                
                y = STATUS_CARRIER_FREQUENCY ;                                      // parameter 6 - frequency
                stv0910_read_car_freq (rcv[rx].nimreceiver,&temp) ;                 // get the carrier offset               
				temp /= 1000 ;														// convert to kHz											
				if (temp == 0)
				{
///					temp = 1 ;														// prevent zero value
				}
				rcv[rx].demodfreq = temp ;											// offset detected by the demodulator
                if (rcv[rx].scanstate == STATE_DEMOD_S2 || rcv[rx].scanstate  == STATE_DEMOD_S) 
                {
					if (rcv[rx].qo100mode == QO100BEACON)							// update loc error
					{
						tempu 					   = ((rx - 1) & 2) + 1  ;			// 1,2,3,4 --> 1,1,3,3
						rcv[tempu].qo100locerror   = -temp ;						// first receiver
						rcv[tempu+1].qo100locerror = -temp ;						// second receiver
					}                
                	if (rcv[rx].signalacquiredtime == 0)
	                {
						if (rcv[rx].vlcstopped == 0)
						{
							memset ((void*)&rcv[rx].rawinfos,0,sizeof(rcv[rx].rawinfos)) ;
							memset ((void*)&rcv[rx].textinfos,0,sizeof(rcv[rx].textinfos)) ;			
							rcv[rx].vlcstopped = 1 ;
							rcv[rx].vlcstopcount++ ;
							rcv[rx].rawinfos[STATUS_VLCSTOPS] = rcv[rx].vlcstopcount ;
							if (rcv[rx].xdotoolid)
							{
								sprintf (temps,	"xdotool key --window %d s", rcv[rx].xdotoolid) ;
								system (temps) ;							// send NEXT to VLC
							}
						}               
						rcv[rx].signalacquiredtime = monotime_ms() ;				// signal first acquired
						rcv[rx].signallosttime    = 0 ;								// clear the lost time
						rcv[rx].packetcountrx = 0 ;									// clear null packet count
						rcv[rx].nullpacketcountrx = 0 ;								// clear null packet count
						y = STATUS_VIDEO_TYPE ;
						rcv[rx].rawinfos[y]    	  = 0 ;								// clear the service type
						rcv[rx].textinfos[y][0]	  = 0 ;								
        	        }
        		}        

                y = STATUS_CARRIER_FREQUENCY ;                                      // parameter 6 - frequency
            	rcv[rx].rawinfos[y]      =  rcv[rx].requestedfreq  ;
				if (rcv[rx].qo100mode   != QO100BEACON)
				{
					if (rcv[rx].highsideloc == 0)		
					{
						rcv[rx].rawinfos[y] += rcv[rx].demodfreq ;					// adjust the actual frequency
					}
					else
					{
						rcv[rx].rawinfos[y] -= rcv[rx].demodfreq ;
					}
				}

                y = STATUS_CARRIER_FREQUENCY ;                                      // parameter 6 - frequency
                sprintf (rcv[rx].textinfos[y], "%0.3f" ,(float)rcv[rx].rawinfos[y] / 1000) ;  	// frequency in MHz    

                y = STATUS_SYMBOL_RATE ;                                            // parameter 9
                stv0910_read_sr (rcv[rx].nimreceiver,&tempu) ;                      // get the symbol rate               
                rcv[rx].rawinfos[y] = tempu ;
                sprintf (rcv[rx].textinfos[y], "%d" ,(tempu + 500) / 1000) ;    	// symbol rate in kS  
                
                y = STATUS_MER ;                                                    // parameter 12  
                stv0910_read_mer (rcv[rx].nimreceiver,&temp) ;                      // get the MER in 0.1dB units               
                rcv[rx].rawinfos[y] = temp ;										
                if (temp > 999)
                {
					temp = 999 ;
                }
                else if (temp < -999)
                {
					temp = -999 ;
                }
                sprintf (rcv[rx].textinfos[y], "%.1f" ,((float) temp) / 10) ;		// MER  

           	    if (rcv[rx].scanstate == STATE_HEADER_S2 || rcv[rx].scanstate == STATE_DEMOD_S2) // DVB-S2 header or lock
                {                    
                    y = STATUS_MODCOD ;                                    			// parameter 18
                    stv0910_read_modcod_and_type 
                    (
                    	rcv[rx].nimreceiver, &tempu, &bool1, &bool2  				// modcod, frametype, pilots
					) ;
					if (tempu == rcv[rx].lastmodulation)							// check for consecutive similar
					{
	                    rcv[rx].rawinfos[y] = tempu ;
    	                sprintf (rcv[rx].textinfos[y], "%s" , modinfo_S2[tempu].modtext) ; 			// qpsk 3/4 etc 
	                    y = STATUS_DNUMBER ; 														// decode threshold
						temp = rcv[rx].rawinfos[STATUS_MER] - modinfo_S2[tempu].minmer ;
	                    rcv[rx].rawinfos[y] = temp ; 
    	                sprintf (rcv[rx].textinfos[y], "%.1f" , (float) temp / 10) ;

	                    y = STATUS_FRAME_TYPE ;                                         			// parameter 19
    	                rcv[rx].rawinfos[y] = (bool1 ? 1 : 0) ;
						if (rcv[rx].rawinfos[y])
						{	
							sprintf (rcv[rx].textinfos[y], "S") ;
						}
						else
						{	
							sprintf (rcv[rx].textinfos[y], "L") ;
						}
	
    	                y = STATUS_PILOTS ;                                             			// parameter 20
	                    rcv[rx].rawinfos[y] = (bool2 ? 1 : 0) ;                                    
						if (rcv[rx].rawinfos[y])
						{	
							sprintf (rcv[rx].textinfos[y], "Y") ;
						}
						else
						{	
							sprintf (rcv[rx].textinfos[y], "N") ;
						}
	
						y = STATUS_ROLLOFF ;
	    	            stv0910_read_rolloff (rcv[rx].nimreceiver,&tempu) ; 		                // get the rolloff               
						rcv[rx].rawinfos[y] = tempu ;
						switch (rcv[rx].rawinfos[y])
						{
							case 0 : sprintf (rcv[rx].textinfos[y],"35") ; break ;
							case 1 : sprintf (rcv[rx].textinfos[y],"25") ; break ;
							case 2 : sprintf (rcv[rx].textinfos[y],"20") ; break ;
							case 3 : sprintf (rcv[rx].textinfos[y],"15") ; break ;
						} ;
					}
					else
					{
						rcv[rx].lastmodulation = tempu ;
					}

                }
                else if (rcv[rx].scanstate == STATE_DEMOD_S)									// DVB-S
                {
                    y = STATUS_MODCOD ;                                             			// parameter 18
					stv0910_read_puncture_rate (rcv[rx].nimreceiver,&tempc) ;
					rcv[rx].rawinfos[y] = 100 + tempc ;
					sprintf (rcv[rx].textinfos[y], "%s", modinfo_S[tempc].modtext) ;

					if (tempc == rcv[rx].lastmodulation)							// check for consecutive similar
					{
	                    y = STATUS_DNUMBER ; 														// decode threshold
						temp = rcv[rx].rawinfos[STATUS_MER] - modinfo_S[tempc].minmer ;
                    	rcv[rx].rawinfos[y] = temp ; 
                    	sprintf (rcv[rx].textinfos[y], "%.1f" , (float) temp / 10) ;
                    }
					else
					{
						rcv[rx].lastmodulation = tempc ;
					}
                }
                else 																			// set some parameters to default
                {
/*                
	                y = STATUS_MODCOD ;                                         				// parameter 18
                    rcv[rx].rawinfos[y] = 0 ;
       	            sprintf (rcv[rx].textinfos[y],"%s","") ;                               
                
	                y = STATUS_FRAME_TYPE ;                                     				// parameter 19
                    rcv[rx].rawinfos[y] = 0 ;
       	            sprintf (rcv[rx].textinfos[y],"%s","") ;                               
                
           	        y = STATUS_PILOTS ;                                         				// parameter 20
               	    rcv[rx].rawinfos[y] = 0 ;
                   	sprintf (rcv[rx].textinfos[y],"%s","") ;                               
*/					                	
                    y = STATUS_MER ;                                             				// parameter 12
                    rcv[rx].rawinfos[y] = 0 ;
                    sprintf (rcv[rx].textinfos[y],"%s","") ;                               
					                	
                    y = STATUS_DNUMBER ;                                             	
                    rcv[rx].rawinfos[y] = 0 ;
                    sprintf (rcv[rx].textinfos[y],"%s","") ;                               
                
                    y = STATUS_SYMBOL_RATE ;                                      				// parameter 9
                    rcv[rx].rawinfos[y] = rcv[rx].symbolrates[rcv[rx].srindex] ;
                    sprintf (rcv[rx].textinfos[y],"%d",rcv[rx].rawinfos[y]) ;                              

	                y = STATUS_CARRIER_FREQUENCY ;                                  			// parameter 6 - frequency
					temp = rcv[rx].frequencies[rcv[rx].freqindex] ;
        	        rcv[rx].rawinfos[y] = temp ;
            	    sprintf (rcv[rx].textinfos[y], "%.3f" ,((float)temp) / 1000) ;   			// frequency in MHz    
                }
                
// put info into VLC header bar

				setup_titlebar (titlebar, rx) ;										// create the VLC title bar

				if (rcv[rx].xdotoolid)
				{				
					if ((modex != MODE_MULTICAST) && (rcv[rx].iptype != IP_MYPC))
					{
						sprintf (titlebar, "%d: *TS diverted to another location*", rx) ;
					}			
					else
					{
						setup_titlebar (titlebar, rx) ;								// create the VLC title bar
					}
					sprintf (temps,"xdotool set_window --name \"%s\" %d ", titlebar, rcv[rx].xdotoolid) ;
					system (temps) ;
				}

				setup_titlebar (titlebar, rx) ;										// create the VLC title bar
				strcpy (rcv[rx].textinfos[STATUS_TITLEBAR], titlebar) ;				


// send a modified EIT packet to be displayed in the VLC title bar

				if (eitinsert && rcv[rx].iptype != IP_MYPC)															// this function is enabled
				{
					if ((counter & 1) == 0)
					{
						if ((counter & 3) == 0)
						{
							rcv[rx].eitversion++ ;
						}	
						setup_eit ((void*)&command, rx, titlebar) ;
						if 
						(
							(rcv[rx].scanstate != STATE_TIMEOUT && rcv[rx].scanstate != STATE_IDLE) || 
							rcv[rx].timeoutholdoffcount != 0
						)
						{
		                    status = sendto
    		                (
	    		            	rcv[rx].tssock, (char*)&command, 188, 0,
	            	            (struct sockaddr*) &rcv[rx].tssockaddr, sizeof(rcv[rx].tssockaddr)
    	            	    ) ;
        	            	(void) status ;                                                      
            	        }
            	    }
				}
            } 

            if (rcv[rx].active == 0)
            {
               	strcpy (temps2, "") ;
            }
            else
            {
				if (rcv[rx].packetcountrx < 10000000)
				{
					sprintf (temps2,"%7d", rcv[rx].packetcountrx) ;
				}
				else
				{
					sprintf (temps2, 	 "%-.6fM", (float) rcv[rx].packetcountrx / 1000000) ;	
					if (strlen(temps2) > 7)
					{
						sprintf (temps2 + 6, "M") ;	
					}
				}
			}

			strncpy (temps3, rcv[rx].textinfos[STATUS_SERVICE_NAME], sizeof(temps3)-1) ;
		    temps3 [15] = 0 ;

			y = STATUS_TS_NULL_PERCENTAGE ;
			sprintf 
			(
				rcv[rx].textinfos[y], 
				"%0.1f", 
				(float) 99.9 * rcv[rx].nullpacketcountrx / (rcv[rx].packetcountrx + 1)
			) ;
			rcv[rx].rawinfos[y] = atof (rcv[rx].textinfos[y]) + 0.5 ; 			
	
			sprintf 
			(
				temps,
				"%1s%1s%2s",									
				rcv[rx].textinfos[STATUS_FRAME_TYPE], 
				rcv[rx].textinfos[STATUS_PILOTS], 
				rcv[rx].textinfos[STATUS_ROLLOFF]
			) ;

// start building the info line for this rx
		     
	       	sprintf 
       		(
       			output + strlen(output),
               	" %2d  %-7s  %-15s  %5s  %5s  %9s  %5s  %-11s  ",  
   	            rx + rxbase, 
       			rcv[rx].textinfos [STATUS_STATE], 	
				temps3,						       				// rcv[rx].textinfos[STATUS_SERVICE_NAME]
       			rcv[rx].textinfos [STATUS_MER],
  				rcv[rx].textinfos [STATUS_DNUMBER],
      			rcv[rx].textinfos [STATUS_CARRIER_FREQUENCY],	
       			rcv[rx].textinfos [STATUS_SYMBOL_RATE],
      			rcv[rx].textinfos [STATUS_MODCOD]
			) ;

			sprintf (temps3, "%s-%s", rcv[rx].textinfos[STATUS_VIDEO_TYPE],rcv[rx].textinfos [STATUS_AUDIO_TYPE]) ;
			if (strlen(temps3) == 1)
			{	
				strcpy (temps3, "") ;
			}

	       	sprintf 
       		(
       			output + strlen(output),
               	"%-4s  %-8s  %3s  %7s  %4s  %-8s  ",    						
				temps,											// FPRO
				temps3,											// video-audio	
      			rcv[rx].textinfos [STATUS_ANTENNA],
				temps2,											// packetcount
      			rcv[rx].textinfos [STATUS_TS_NULL_PERCENTAGE],
                rcv[rx].nimtype
       		) ;

			strcat  (outputonnet, output) ;						// add the output line 			
			sprintf (outputonnet+strlen(outputonnet), "%-15s", rcv[rx].ipaddress) ;	// TS destination
	       	sprintf (outputonnet+strlen(outputonnet), "\r\n") ;			

			strcat  (outputoffnet, output) ;					// add the output line 			
			tempu = inet_addr (rcv[rx].ipaddress) ;				// convert to numeric
			tempu &= 0xffff ;									// mask off the 2 lower numbers
			sia.s_addr = tempu ;
			sprintf (outputoffnet+strlen(outputoffnet), "%-15s", inet_ntoa(sia)) ;	// masked TS destination
	       	sprintf (outputoffnet+strlen(outputoffnet), "\r\n") ;			

// end of the loop for each receiver

// build the original LM $ text info for an individual receiver
     
        	sprintf (temps, "%s", "") ;
			for (y = 1 ; y <= STATUS_LNB_POLARISATION_H ; y++)
			{
				if (rcv[rx].textinfos[y][0])
				{
					if (y == STATUS_SERVICE_NAME || y == STATUS_SERVICE_PROVIDER_NAME)
					{
						sprintf (temps+strlen(temps), "$%d,%s\r\n", y, rcv[rx].textinfos[y]) ;
					}
					else				
					{
						sprintf (temps+strlen(temps), "$%d,%d\r\n", y, rcv[rx].rawinfos[y]) ;
					}
				}
			}

// send the orginal LM $ text info for this receiver

			if 
			(
				(rcv[rx].scanstate != STATE_TIMEOUT && rcv[rx].scanstate != STATE_IDLE) || 
				rcv[rx].timeoutholdoffcount != 0
			)
			{
				if (rcv[rx].lminfosock)						// valid socket
				{
			        status = sendto 						// send to base port + 60 + receiver (1-4)
    			  	(
   	    			   	rcv[rx].lminfosock, (char*)temps, strlen(temps) + 1, 0,
	    	   			(struct sockaddr*) &rcv[rx].lminfosockaddr, sizeof(rcv[rx].lminfosockaddr) 
					) ;
   					(void) status ;
				}
			}
		}
		
// end of the 4 receiver loop

		for (rx = 1 ; rx <= 4 ; rx++)
		{
			if (rcv[rx].active == 0)
			{
				sprintf (outputonnet+strlen(outputonnet), "\r\n") ;
				sprintf (outputoffnet+strlen(outputoffnet), "\r\n") ;
			}
		}		

		printf ("%c[6A",ESC) ;						// UP cursor tp prevent scrolling
   	   	printf ("%s", outputonnet) ;

// send info

		for (rx = 0 ; rx <= MAXRECEIVERS ; rx++)
		{
			if (1)
			{
				if 
				(
					rx == 0 ||
					(rcv[rx].scanstate != STATE_TIMEOUT && rcv[rx].scanstate != STATE_IDLE) || 
					rcv[rx].timeoutholdoffcount != 0		// send info a few times after timeout
				)
				{

					if (rcv[rx].antenna == 1)
					{
						sprintf (rcv[rx].textinfos[STATUS_ANTENNA], "TOP") ;
					}
					else if (rcv[rx].antenna == 2)
					{
						sprintf (rcv[rx].textinfos[STATUS_ANTENNA], "BOT") ;
					}
							
// send expanded LM $ text info 
			     
			   		strcpy (temps, "") ;									// expanded LM info
		        	sprintf (temps+strlen(temps),"$0,%d\r\n", rx + rxbase) ;
					sprintf (rcv[rx].textinfos[STATUS_VLCSTOPS], "%d", rcv[rx].vlcstopcount) ;
					sprintf (rcv[rx].textinfos[STATUS_VLCNEXTS], "%d", rcv[rx].vlcnextcount) ;
					for (y = 1 ; y < MAXINFOS ; y++)
					{
						if (rcv[rx].textinfos[y][0])
						{
							sprintf (temps+strlen(temps), "$%d,%s\r\n", y, rcv[rx].textinfos[y]) ;
						}
					}
				
					if (rcv[rx].expinfosock)		// valid socket
					{
	    			    status = sendto 						
   						(
			    	   		rcv[rx].expinfosock, (char*)temps, strlen(temps) + 1, 0,
  							(struct sockaddr*) &rcv[rx].expinfosockaddr, sizeof(rcv[rx].expinfosockaddr) 
						) ;
					}
					if (rcv[rx].expinfo2sock)		// valid socket
					{
	    			    status = sendto 			// duplicate to next port
	    	  			(
  					   		rcv[rx].expinfo2sock, (char*)temps, strlen(temps) + 1, 0,
	   			   			(struct sockaddr*) &rcv[rx].expinfo2sockaddr, sizeof(rcv[rx].expinfo2sockaddr) 
						) ;
		  				(void) status ;
					}
					temp = 0 ;
					for (y = 0 ; y < rx ; y++)				// check if already sent to this address
					{
						if 
						(
							(rcv[y].scanstate != STATE_TIMEOUT && rcv[y].scanstate != STATE_IDLE) || 
							rcv[y].timeoutholdoffcount != 0
						)
						{
							if (strcmp(rcv[y].ipaddress,rcv[rx].ipaddress) == 0)
							{
								temp++ ;						// already sent to this address
							}						
						}
					}
					
// send 4 line info

   					if ((rx == 0) || (temp == 0))				// haven't already sent to this address
					{
						if (rcv[rx].iptype == IP_OFFNET)		// mask TS destinations for off net
						{
							if (rcv[rx].summarysock)			// valid socket
							{
			    			    status = sendto 						
			   		  			(
				    	   			rcv[rx].summarysock, (char*)outputoffnet, strlen(outputoffnet) + 1, 0,
	   					   			(struct sockaddr*) &rcv[rx].summarysockaddr, sizeof(rcv[rx].summarysockaddr) 
								) ;
   								(void) status ;
							}
							if (rcv[rx].summary2sock)			// valid socket
							{
			    			    status = sendto 						
			   		  			(
				    	   			rcv[rx].summary2sock, (char*)outputoffnet, strlen(outputoffnet) + 1, 0,
	   					   			(struct sockaddr*) &rcv[rx].summary2sockaddr, sizeof(rcv[rx].summary2sockaddr) 
								) ;
   								(void) status ;
							}
						}
						else									// on net
						{
							if (rcv[rx].summarysock)			// valid socket
							{
			    			    status = sendto 						
			   		  			(
				    	   			rcv[rx].summarysock, (char*)outputonnet, strlen(outputonnet) + 1, 0,
	   					   			(struct sockaddr*) &rcv[rx].summarysockaddr, sizeof(rcv[rx].summarysockaddr) 
								) ;
   								(void) status ;
							}
							if (rcv[rx].summary2sock)			// valid socket
							{
			    			    status = sendto 						
			   		  			(
				    	   			rcv[rx].summary2sock, (char*)outputonnet, strlen(outputonnet) + 1, 0,
	   					   			(struct sockaddr*) &rcv[rx].summary2sockaddr, sizeof(rcv[rx].summary2sockaddr) 
								) ;
   								(void) status ;
							}
						}
					}
  				}				
   			}
   			if (rcv[rx].timeoutholdoffcount)
   			{
	   			rcv[rx].timeoutholdoffcount-- ;		// decrement the holdoff count
   			}
		}		
	}

	i2csharedaccess = I2CMAINHAS ;					// give i2c access to main loop	if this loop ever exits		

	printf ("INFO   thread exiting\r\n") ;
	return (0) ;
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 tsproc
//@
//@	 a thread to process incoming packets and send them via UDP
//@
//@	 Calling:	
//@
//@	 Return:	
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    	
void* tsproc_loop (void* dummy)
{
		int			x ;
		uint8		tempc ;
		uint32		tempu ;
		char		temps  [256] ;
		char		temps2 [256] ;
		uint32		index ;
		uint32		rx ;
		int32		status ;
		packetx_t	*pp ;
		uint8		*sdtp ;
		uint16		pid ;
		uint32		length ;
		uint32		length2 ;
		uint32		y ;
		uint32		sectionlength ;
		uint32		servicetype ;
		uint32		audioservicetype ;
		uint32		videoservicetype ;
		uint32		prognumber ;
		uint32		pmtpid ;
		uint32		programcount ;
		uint32		changedetected ;
		uint8		rxbuff [256] ;
	
	(void) dummy ;

	while (tsprocenabled == 0) 
	{
		usleep (100*1000) ;
	}

	while (tsprocenabled == 1)
	{
			memset (rxbuff, 0, sizeof(rxbuff)) ;
			status = read (whfd, (void*)rxbuff, 192) ;
			if (status == 192)								// packet received
			{
				pp = (packetx_t*) rxbuff ;
			}
			else if ((status == -1) && (errno == 3))		// kernel driver is unloading
			{
				pp = 0 ;
				printf ("<<Exiting %d>>\r\n",status) ;
				usleep (5 * 1000 * 1000) ;
				whexit (999) ;
			}
			else if ((status == -1) && (errno == 4))
			{
				pp = 0 ;
				printf ("<<Driver does not have spi5interruptnumber>>\r\n") ;
				usleep (5 * 1000 * 1000) ;
				whexit (986) ;
			}
			else if ((status == -1) && (errno == 5))
			{
				pp = 0 ;									// read timeout - normal behaviour
			}
			else if (status < 0)
			{
				printf ("[<%d %d>]\r\n",status,errno) ;
				pp = 0 ;
				continue ;
			}
			else
			{
				pp = 0 ;
			}

			if (pp == 0)
			{
				continue ;
			}

			rx = pp->receiver + 1 ;										// receivers are numbered 0-3 in the PICs						
																		// . . . and 1 to 4 in this program	
			if 
			(
				(rcv[rx].active == 0) || 
				(rcv[rx].scanstate != STATE_DEMOD_S && rcv[rx].scanstate != STATE_DEMOD_S2)) 
			{
			}
	       	else if (pp->data[0] != 0x47)								// sync byte missing
	       	{
				rcv[rx].errors_sync++ ;		       						
				rcv[rx].packetcountprogram++ ;							
				rcv[rx].packetcountrx++ ;				
				continue ;
	       	}
	       	else if (pp->valid)
	       	{	       		       	
				tempc = (pp->receiver & 2) + 1 ;						// rx1, rx3 <==> PIC_A or PIC_B 			
				rcv[tempc].debug0 = pp->crc8 ;

				tempu = pp->outsequence - rcv[tempc].outsequence ;		// 4 bit output sequence for each PIC
				tempu &= 0xf ;
				if (tempu != 1)
				{
					rcv[tempc].errors_outsequence ++ ;		       	
					rcv[tempc].outsequence = pp->outsequence ;		       	
				}
				else
				{
					rcv[tempc].outsequence++ ;		       						
				}
				
				tempu = pp->insequence - rcv[rx].insequence ;			// 4 bit input sequence for each receiver
				tempu &= 0xf ;
				if (tempu != 1)
				{
					rcv[rx].errors_insequence++ ;		       	
					rcv[rx].insequence = pp->insequence ;
				}
				else
				{
					rcv[rx].insequence++ ;
				}

				if (pp->restart)
				{
					rcv[rx].errors_restart++ ;
				}
				
				pid = (pp->data[1] & 0x1f) * 0x100 + pp->data[2] ;
				
				rcv[rx].packetcountprogram++ ;				
				rcv[rx].packetcountrx++ ;				
				rcv[rx].packetcountprogram 		+= pp->nullpackets ;				
				rcv[rx].packetcountrx      		+= pp->nullpackets ;				
				rcv[rx].nullpacketcountprogram  += pp->nullpackets ;				
				rcv[rx].nullpacketcountrx       += pp->nullpackets ;				
				if (pid == NULL_PID || (pid == NULL2_PID && null8190))
				{
					rcv[rx].nullpacketcountprogram++ ;				
					rcv[rx].nullpacketcountrx++ ;				
				}

			  	if ((rcv[rx].scanstate == STATE_DEMOD_S2) || (rcv[rx].scanstate == STATE_DEMOD_S))
			  	{	
			  		if (rcv[rx].tssock)
			  		{
						if (nullremove == 0)
						{
							if (rcv[rx].vlcstopped == 0)
							{
								for (x = 0 ; x < pp->nullpackets ; x++)
								{
						            status = sendto 
				    		      	(
   					    		       	rcv[rx].tssock, (char*)nullpacket, 188, 0,
	       					    	   	(struct sockaddr*) &rcv[rx].tssockaddr, sizeof(rcv[rx].tssockaddr) 
    	       						) ;
								}
							}
						}						
						rcv[rx].forbidden = 0 ;
						
						y = STATUS_VIDEO_TYPE ;
						tempu = 0 ;											// don't send packet if true
						if 
						(									
							(rx == 1) 								|| 
							(rx <= h265max + 1) 					|| 
							(modex == MODE_MULTICAST) 				||
							(rcv[rx].iptype != IP_MYPC)				||
							(rcv[rx].rawinfos[y] == SERVICE_H262) 	||
							(rcv[rx].rawinfos[y] == SERVICE_H264)
						)
						{
						}
						else
						{
	    	           		if (rcv[rx].rawinfos[y])
	    	           		{
								rcv[rx].forbidden = 1 ;
   	    	       			}
							tempu++ ;										// don't send
						}
					
						if (pid == EIT_PID && eitremove)					// EIT packet
						{
							tempu++ ;										// remove fromthe incoming TS
						}
					
						if (pid == NULL_PID && nullremove)					// null packet
						{
							tempu++ ;										// don't send null packets
						}
					
						if (pid == NULL2_PID && nullremove && null8190)		// fake null packet
						{
							tempu++ ;										// don't send fake null packets
						}
					
						if (tempu == 0)
						{
							if (rcv[rx].vlcstopped == 0)
							{
					            status = sendto 
			    		      	(
			    		          	rcv[rx].tssock, (char*)pp, 188, 0,
       					    	   	(struct sockaddr*) &rcv[rx].tssockaddr, sizeof(rcv[rx].tssockaddr) 
   	       						) ;
   	           					(void) status ;
   	           				}
   	           				rcv[rx].forbidden = 0 ; 
   	           			}
    	           		else if (rcv[rx].rawinfos[y])
    	           		{
							rcv[rx].forbidden = 1 ;
    	           		}
					
// select the first PMT with program number > 0

					if (pid == PAT_PID)
					{
						index = 5 ;
						sectionlength = ((pp->data[index+1] & 0xf) << 8) + pp->data[index+2] ;
						if (sectionlength <= 1 || sectionlength >= 188)
						{
							sectionlength = 0 ;
						}
						if (sectionlength)
						{
						    tempu = calculateCRC32 ((uint8*)&pp->data[index],sectionlength-1) ;
							tempu = reverse (tempu) ;									// reverse byte order
							index += sectionlength - 1 ;
							if (tempu == *((uint32*)&pp->data[index]))					// CRC32 OK
							{
								pmtpid = 0 ; 											// PMT to use
								programcount = 0 ;										// number of programs
								index = 0x0d ;
								while (index < 0x08 + sectionlength - 4)
								{
									prognumber = pp->data [index] * 0x100 + pp->data [index + 1] ;
									index += 2 ;										// point to PMT pid
									if (prognumber != 0) 
									{ 
										if ((rcv[rx].requestedprog == 0) || (prognumber == rcv[rx].requestedprog))									
										{
											if (pmtpid == 0)
											{
												pmtpid = (pp->data [index] & 0x1f) * 0x100 + pp->data [index + 1] ;
										    }
										}	
								    	programcount++ ;
									}
									index += 2 ;
								}
								rcv[rx].pmtpid = pmtpid ;
								rcv[rx].programcount = programcount ;
							}
						}
					}

// extract the video and audio service types from the PMT

					else if ((pid == rcv[rx].pmtpid) && rcv[rx].pmtpid)					// the program we want
					{	
						index = 5 ;
						sectionlength = ((pp->data[index+1] & 0xf) << 8) + pp->data[index+2] ;
						if (sectionlength <= 1 || sectionlength >= 188)
						{
							sectionlength = 0 ;
						}
						audioservicetype = 0 ;
						videoservicetype = 0 ;
						servicetype      = 0 ;								
						if (sectionlength)
						{
						    tempu = calculateCRC32 ((uint8*)&pp->data[index],sectionlength-1) ;
							tempu = reverse (tempu) ;								// reverse byte order
							index += sectionlength - 1 ;
							if (tempu == *((uint32*)&pp->data[index]))				// CRC32 OK
							{					
								index = 0x0d ;										// point to first entry
// get the PCR pid
								tempu = (pp->data [index] & 0x1f) * 0x100 + pp->data [index + 1] ;
								rcv[rx].pcrpid = tempu ;
								index += 2 ;
								tempu = (pp->data [index] & 0x0f) * 0x100 + pp->data [index + 1] ; // info length
								index += tempu + 2 ;								// skip over the info										

								audioservicetype = 0 ;
								videoservicetype = 0 ;
								servicetype      = 0 ;								

// look for service types
								while (index < 0x08 + sectionlength - 4)			
								{							
									servicetype = pp->data[index] ;					// H264, AAC etc	
									index++ ;
									index += 2 ;
									tempu = (pp->data [index] & 0x0f) * 0x100 + pp->data [index + 1] ; // info length
									index += tempu + 2 ;								// skip over the info																

									switch (servicetype)
									{
										case SERVICE_H262:
										case SERVICE_H264:
										case SERVICE_H265:
											videoservicetype = servicetype ;
										break ;
										case SERVICE_MPA:
										case SERVICE_AAC:
										case SERVICE_AC3:
											audioservicetype = servicetype ;
										break ;
									}	
								}




							}							
									
							changedetected = 0 ;
							y = STATUS_VIDEO_TYPE ;
							if 
							(
								((int)videoservicetype != rcv[rx].rawinfos[y]) && 
								videoservicetype
							)
							{
								if (rcv[rx].rawinfos[y] != 0 || rcv[rx].vlcstopped)
								{
									 changedetected++ ;					
								}			
								rcv[rx].rawinfos[y] = videoservicetype ;
								switch (videoservicetype)	
								{					
									case SERVICE_H262:
										sprintf (rcv[rx].textinfos[y], "H262") ; break ;
									break ;
									case SERVICE_H264:
										sprintf (rcv[rx].textinfos[y], "H264") ; break ;
									break ;
									case SERVICE_H265:
										sprintf (rcv[rx].textinfos[y], "H265") ; break ;
									break ;
									default:
										sprintf (rcv[rx].textinfos[y], "%s", "") ; break ;
									break ;											
								} ;
							}

							y = STATUS_AUDIO_TYPE ;
							if 
							(
								((int)audioservicetype != rcv[rx].rawinfos[y]) && 
								audioservicetype
							)
							{
								if (rcv[rx].rawinfos[y] != 0 || rcv[rx].vlcstopped)
								{
									 changedetected++ ;					
								}			
								rcv[rx].rawinfos[y] = audioservicetype ;
								switch (audioservicetype)
								{
									case SERVICE_MPA:
										sprintf (rcv[rx].textinfos[y], "MPA") ; break ;
									break ;
									case SERVICE_AAC:
										sprintf (rcv[rx].textinfos[y], "AAC") ; break ;
									break ;
									case SERVICE_AC3:
										sprintf (rcv[rx].textinfos[y], "AC3") ; break ;
									break ;
									default:
										sprintf (rcv[rx].textinfos[y], "%s", "") ; break ;
									break ;
								}					
							}

							if (changedetected)
							{										
							 	rcv[rx].vlcstopped = 0 ;
								y = STATUS_VIDEO_TYPE ;
								rcv[rx].rawinfos[y]= videoservicetype ;
								y = STATUS_AUDIO_TYPE ;
								rcv[rx].rawinfos[y] = audioservicetype ;
								rcv[rx].modechanges++ ;							// count a mode change
								rcv[rx].vlcnextcount++ ;
								if (rcv[rx].xdotoolid)
								{
									sprintf (temps,	"xdotool key --window %d n", rcv[rx].xdotoolid) ;
									system (temps) ;							// send NEXT to VLC
								}
							}
						}
					}
					else if (pid == SDT_PID)								// service descriptor
					{
						index = 5 ;
						sectionlength = ((pp->data[index+1] & 0xf) << 8) + pp->data[index+2] ;
						if (sectionlength <= 1 || sectionlength >= 188)
						{
							sectionlength = 0 ;
						}
						if (sectionlength)
						{
						    tempu = calculateCRC32 ((uint8*)&pp->data[index],sectionlength-1) ;
							tempu = reverse (tempu) ;							// reverse byte order
							index += sectionlength - 1 ;
							if (tempu == *((uint32*)&pp->data[index]))
							{
								sdtp = (uint8*) pp ;
								sdtp += 16 ;									// point to the first entry
								rcv[rx].serviceid  = *sdtp++ * 0x100 ;			// 16 bit service ID, high/low
								rcv[rx].serviceid += *sdtp++ ;
								sdtp += 3 ;										// point to the descriptor tag
								if (*sdtp == 0x48)								// service descriptor
								{					
									sdtp += 3 ;									// point to the provider length
									length = *sdtp++ ;							// provider length
									if (length <=  15)
									{
										length2 = length ;
									}
										else
									{
										length2 = 15 ;
									}
									y = STATUS_SERVICE_PROVIDER_NAME ; 
									strncpy (rcv[rx].textinfos[y], (void*)sdtp, length2) ; // copy the provider name
									rcv[rx].textinfos[y][length2] = 0 ;			// terminate the string
									sdtp += length ;							// point to the service name length 
									length = *sdtp++ ;							// service name length 
									if (length <=  15)
									{
										length2 = length ;
									}
										else
									{
										length2 = 15 ;
									}
									y = STATUS_SERVICE_NAME ; 					// callsign
									if (rcv[rx].programcount != 1 && rcv[rx].textinfos[y][0])
									{
										strcpy (temps, "+") ;
										strncat (temps, (void*)sdtp, length2) ; 	// copy the service name
										temps [length2+1] = 0 ;						// terminate the string	
									}
									else
									{
										strcpy (temps, "") ;
										strncat (temps, (void*)sdtp, length2) ; 	// copy the service name
										temps [length2] = 0 ;						// terminate the string	
									}
									if (strcmp(temps,rcv[rx].textinfos[y]) != 0)
									{
										rcv[rx].modechanges++ ;					// count a mode change			
										rcv[rx].packetcountrx  	   		= 0 ;	// clear packet count		
										rcv[rx].nullpacketcountrx  		= 0 ;	// clear null packet count	
									 	if (rcv[rx].textinfos[y][0] || rcv[rx].vlcstopped)
									 	{
											rcv[rx].vlcstopped = 0 ;
											rcv[rx].vlcnextcount++ ;
											rcv[rx].rawinfos[STATUS_VLCNEXTS] = rcv[rx].vlcnextcount ;
											if (rcv[rx].xdotoolid) 
											{
												sprintf (temps2,	"xdotool key --window %d n", rcv[rx].xdotoolid) ;
												system (temps2) ;					// send NEXT to VLC
											}
										}
									}
									strcpy (rcv[rx].textinfos[y], temps) ;	// copy the service name
								}
								sdtp += length ;								
							}
						}
					}				
		    	}		    	   
			} 
		}
	}

	printf ("TSPROC thread exiting\r\n") ;
	return (0) ;
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 setup_titlebar
//@
//@	 create a string for xdotool to put into the VLC title bar
//@
//@	 Calling:	buffer to populate
//@				receiver number
//@
//@	 Return:	
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void setup_titlebar (char *output, uint32 rx)
{
	int				x ;
	int				y ;
	char			temps  [512] ;
	char			*pos ;
	
	sprintf (output, "%d:", rx + rxbase) ;

// print status	
	
	if (rcv[rx].scanstate == STATE_IDLE)
	{
		sprintf (output+strlen(output), " *idle*") ;
	}
	else
	{
		if (rcv [rx].scanstate != STATE_DEMOD_S2 && rcv[rx].scanstate != STATE_DEMOD_S)
		{
			y = STATUS_STATE ;
			sprintf (temps, "%s", rcv[rx].textinfos[y]) ;
			for (x = 0 ; x < (int)strlen(temps) ; x++)
			{
				temps[x] = tolower(temps[x]) ;
			}
			sprintf (output+strlen(output), " *%s*", temps) ;
		}
	
// display callsign; remove anything after - _ . SP
	
		y = STATUS_SERVICE_NAME ;										// callsign

		sprintf (temps, "%s", rcv[rx].textinfos[y]) ;
		if (temps[0])
		{
			pos = strchr (temps,'_') ;									
			if (pos == 0)
			{
				pos = strchr (temps,'-') ;			
				if (pos == 0)
				{
					pos = strchr (temps,' ') ;			
				}
				{
					if (pos == 0)
					{
						pos = strchr (temps,'.') ;			
					}
				}
			}
			if (pos)
			{
				*pos = 0 ;
			}
			sprintf (output+strlen(output)," %s",temps) ;
		}
		
// display MER		
		
		if (rcv[rx].scanstate == STATE_DEMOD_S2 || rcv[rx].scanstate == STATE_DEMOD_S)
		{
			y = STATUS_MER ;
			if (rcv[rx].textinfos[y][0])
			{
				sprintf (output+strlen(output)," M%s",rcv[rx].textinfos[y]) ;
			}
			y = STATUS_DNUMBER ;
			if (rcv[rx].textinfos[y][0])
			{
				sprintf (output+strlen(output)," D%s",rcv[rx].textinfos[y]) ;
			}
		}
			
// display TS type and video type
			
		if (rcv[rx].scanstate == STATE_DEMOD_S2 || rcv[rx].scanstate == STATE_DEMOD_S)
		{
			y = STATUS_STATE ;
			if (rcv[rx].forbidden)
			{
			sprintf (output+strlen(output), "%s", " *") ;
			}
			else
			{
				sprintf (output+strlen(output), "%s", " ") ;
			}
			sprintf (temps, "%s", rcv[rx].textinfos[y]) ;
			if (strlen(temps))
			{
				if (strncmp(temps, "DVB-", 4) == 0)
				{
					strcpy (temps,temps+4) ;						// S or S2
				}		
				sprintf (output+strlen(output), "%s", temps) ;
			}	
			y = STATUS_VIDEO_TYPE ;								// H265 etc
			sprintf (temps, "%s", rcv[rx].textinfos[y]) ;
			if (strlen(temps) >= 4)
			{
				sprintf (output+strlen(output), "%c%c", temps[0], temps[3]) ;
			}
			if (rcv[rx].forbidden)
			{
				sprintf (output+strlen(output),"%s","*") ;
			}			
		}
		
// display modulation and FEC			

		if (rcv[rx].scanstate == STATE_DEMOD_S2 || rcv[rx].scanstate == STATE_DEMOD_S)
		{
			y = STATUS_MODCOD ;
			sprintf (temps,"%s",rcv[rx].textinfos[y]) ;
			pos = strstr (temps,"QPSK") ;
			if (pos)
			{
				strcpy (pos+2, pos+5) ; 
			}
			else 
			{
				pos = strstr (temps,"8PSK") ;
				if (pos)
				{
					strcpy (pos+2, pos+5) ; 
				}
				else
				{
					pos = strstr (temps,"APSK") ;
					if (pos)
					{
						strcpy (pos+1, pos+5) ; 
					}
				}
			}
			sprintf (output+strlen(output)," %s",temps) ;
		}
		
// display SR and frequency
		
		if (rcv[rx].scanstate == STATE_DEMOD_S2 || rcv[rx].scanstate == STATE_DEMOD_S)
		{
/*
			sprintf (temps,"%0.3fM", (float)rcv[rx].frequencies[0] / 1000) ;
			pos = strstr (temps,".") ;
			if (pos && (pos != temps))
			{
				pos-- ;
			}
			else
			{
				pos = temps ;
			}
			sprintf (output+strlen(output), " %d", rcv[rx].symbolrates[0]) ;
			sprintf (output+strlen(output), " %s", pos) ;	
*/
			sprintf (output+strlen(output), " %s",rcv[rx].textinfos [STATUS_SYMBOL_RATE]) ;
			
			sprintf (temps, " %s", rcv[rx].textinfos [STATUS_CARRIER_FREQUENCY]) ;
			pos = strstr (temps,".") ;
			if (pos && (pos != temps))
			{
				pos-- ;
			}
			else
			{
				pos = temps ;
			}
			sprintf (output+strlen(output), " %sM", pos) ;
		}
		else
		{
			sprintf (output+strlen(output), " SR%d", rcv[rx].symbolrates[0]) ;
			sprintf (output+strlen(output), " %0.3fMHz", (float)rcv[rx].frequencies[0] / 1000) ;
		}
		
// display antenna
		
		y = STATUS_ANTENNA ;
		sprintf (output+strlen(output), " %c", rcv[rx].textinfos[y][0]) ;		

// extra terminating zero		

		sprintf (output+strlen(output), "%c%c", 0, 0) ;				
	}	
}



//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 create an EIT packet that will appear in the VLC title bar
//@
//@	 Calling:	buffer to populate
//@				receiver number
//@
//@	 Return:	
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void setup_eit (void *packet, uint32 rx, char* info)
{
    int				y ;
    uint8       	*p, *q ;
    uint        	utempi ;    
    char        	temps  [256] ;
	int32			jday ;
	int32			jhour ;	
	struct eitx		*eitp ;

	eitp = (struct eitx*) packet ;

    memset ((void*)eitp, 0, sizeof(*eitp)) ;				

    eitp->sync = 0x47 ;
    eitp->payloadstart 	= 1 ;
    eitp->adaption 		= 1 ;
    eitp->pid1208 		= EIT_PID >> 8 ;
    eitp->pid0700 		= EIT_PID & 0xff ;

	eitp->continuity = rcv[rx].eitcontinuity++ & 0xf ;	// increment continuity
     
    eitp->tableid 		= 0x4e ;               				// EIT 'current program' descriptor

    eitp->reserved0 	= 3 ;
    eitp->filler2 		= 1 ;
    eitp->syntax 		= 1 ;

	eitp->sectionlength = 0 ;								// fill in later

    eitp->servicehigh 	= (uint8) (rcv[rx].serviceid >> 8) ;						
    eitp->servicelow  	= (uint8) (rcv[rx].serviceid & 0xff) ;						

    eitp->currentnext 	= 1 ;
    eitp->reserved1   	= 3 ;

    eitp->version 	 	= rcv[rx].eitversion & 0x1f ;		// table table version

    eitp->tsidhigh  	= (uint8) (TSID >> 8) ;				// TSID = 0; not needed by VLC for EIT					
    eitp->tsidlow   	= (uint8) (TSID & 0xff) ;						
    eitp->networkhigh 	= (uint8) (NETWORK >> 8) ;			// NETWORK = 0; not needed by VLC for EIT
    eitp->networklow  	= (uint8) (NETWORK & 0xff) ;
    eitp->lasttable 	= 0x4e ;                 			// service descriptor
    
    p = (uint8*) &eitp->loop ;
 
    *p++ = (uint8) (EVENTID >> 8) ;             			// event id = zero (first event)
    *p++ = (uint8) (EVENTID & 0xff) ;

    juliandate (&jday,&jhour) ;
	jhour -= 1 ;											// start at the beginning of the previous hour
	if (jhour < 0)
	{
		jhour += 24 ;
		jday-- ;
	}
    
    *p++ = (uint8) (jday >> 8) ;                			// start date
    *p++ = (uint8) (jday & 0xff) ;

	utempi = (jhour / 10) ;									// convert to BCD
	utempi *= 0x10 ;
	utempi += (jhour % 10) ;

    *p++ = utempi ;            				       			// start time
    *p++ = 0x00 ;
    *p++ = 0x00 ;
    
    *p++ = 0x03 ;                           				// duration 3 hours
    *p++ = 0x00 ;	
    *p++ = 0x00 ;

    *p++ = 0x80  ;    			                        	// status = running, scrambled = no
        
    q = p ;                                     

    *p++ = 0 ;									    		// descriptors loop length - fill in later

    *p++ = 0x4d ;                               			// short descriptor type 
    
    *p++ = 0 ;                                  			// descriptor length - fill in later

   	sprintf ((char*)p, "eng") ;
    p += 3 ;

	strcpy (temps, info) ; 
	
    *p++ = (uint8) strlen (temps) ;
	sprintf ((char*)p, temps) ;
    p += strlen (temps) ;

	y = STATUS_SERVICE_PROVIDER_NAME ;
    strcpy ((char*)temps, rcv[rx].textinfos[y]) ;

    *p++ = (uint8) strlen (temps) ;
	sprintf ((char*)p, temps) ;
    p += strlen (temps) ;

    *(q+2) = (uint8) (p - q - 1 - 2) ;

    *q = (uint8) (p - q - 1) ;
    
    eitp->sectionlength = (uint8) ((uint32)p - (uint) &eitp->sectionlength - 1 + 4) ;
    
    utempi = calculateCRC32 ((uint8*)&eitp->tableid,eitp->sectionlength-1) ;
    *p++ = (uint8) ((utempi >> 24) & 0xff) ;
    *p++ = (uint8) ((utempi >> 16) & 0xff) ;
    *p++ = (uint8) ((utempi >>  8) & 0xff) ;
    *p++ = (uint8) ((utempi >>  0) & 0xff) ;

    memset (p, 0xff, 188-((uint)p-(uint)eitp)) ;	// pad out the rest of the packet
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@
//@	 create an EIT packet that will appear in the VLC title bar
//@
//@	 Calling:	buffer to populate
//@				receiver number
//@
//@	 Return:	
//@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void setup_eit2 (void *packet, uint32 rx, char* info)
{
    int				y ;
    uint8       	*p, *q ;
    uint        	utempi ;    
    char        	temps  [256] ;
	int32			jday ;
	int32			jhour ;	
	struct eitx		*eitp ;

	eitp = (struct eitx*) packet ;

    memset ((void*)eitp, 0, sizeof(*eitp)) ;				

    eitp->sync = 0x47 ;
    eitp->payloadstart 	= 1 ;
    eitp->adaption 		= 1 ;
    eitp->pid1208 		= EIT_PID >> 8 ;
    eitp->pid0700 		= EIT_PID & 0xff ;

	eitp->continuity = rcv[rx].eitcontinuity++ & 0xf ;	// increment continuity
     
    eitp->tableid 		= 0x4e ;               				// EIT 'current program' descriptor

    eitp->reserved0 	= 3 ;
    eitp->filler2 		= 1 ;
    eitp->syntax 		= 1 ;

	eitp->sectionlength = 0 ;								// fill in later

    eitp->servicehigh 	= (uint8) (rcv[rx].serviceid >> 8) ;						
    eitp->servicelow  	= (uint8) (rcv[rx].serviceid & 0xff) ;						

    eitp->currentnext 	= 1 ;
    eitp->reserved1   	= 3 ;

    eitp->version 	 	= rcv[rx].eitversion & 0x1f ;		// table table version

    eitp->tsidhigh  	= (uint8) (TSID >> 8) ;				// TSID = 0; not needed by VLC for EIT					
    eitp->tsidlow   	= (uint8) (TSID & 0xff) ;						
    eitp->networkhigh 	= (uint8) (NETWORK >> 8) ;			// NETWORK = 0; not needed by VLC for EIT
    eitp->networklow  	= (uint8) (NETWORK & 0xff) ;
    eitp->lasttable 	= 0x4e ;                 			// service descriptor
    
    p = (uint8*) &eitp->loop ;
 
    *p++ = (uint8) (EVENTID >> 8) ;             			// event id = zero (first event)
    *p++ = (uint8) (EVENTID & 0xff) ;

    juliandate (&jday,&jhour) ;
	jhour -= 1 ;											// start at the beginning of the previous hour
	if (jhour < 0)
	{
		jhour += 24 ;
		jday-- ;
	}
    
    *p++ = (uint8) (jday >> 8) ;                			// start date
    *p++ = (uint8) (jday & 0xff) ;

	utempi = (jhour / 10) ;									// convert to BCD
	utempi *= 0x10 ;
	utempi += (jhour % 10) ;

    *p++ = utempi ;            				       			// start time
    *p++ = 0x00 ;
    *p++ = 0x00 ;
    
    *p++ = 0x03 ;                           				// duration 3 hours
    *p++ = 0x00 ;	
    *p++ = 0x00 ;

    *p++ = 0x80  ;    			                        	// status = running, scrambled = no
        
    q = p ;                                     

    *p++ = 0 ;									    		// descriptors loop length - fill in later

    *p++ = 0x4d ;                               			// short descriptor type 
    
    *p++ = 0 ;                                  			// descriptor length - fill in later

   	sprintf ((char*)p, "eng") ;
    p += 3 ;

	strcpy (temps, info) ; 
	
    *p++ = (uint8) strlen (temps) ;
	sprintf ((char*)p, temps) ;
    p += strlen (temps) ;

	y = STATUS_SERVICE_PROVIDER_NAME ;
    strcpy ((char*)temps, rcv[rx].textinfos[y]) ;

    *p++ = (uint8) strlen (temps) ;
	sprintf ((char*)p, temps) ;
    p += strlen (temps) ;

    *(q+2) = (uint8) (p - q - 1 - 2) ;

    *q = (uint8) (p - q - 1) ;
    
    eitp->sectionlength = (uint8) ((uint32)p - (uint) &eitp->sectionlength - 1 + 4) ;
    
    utempi = calculateCRC32 ((uint8*)&eitp->tableid,eitp->sectionlength-1) ;
    *p++ = (uint8) ((utempi >> 24) & 0xff) ;
    *p++ = (uint8) ((utempi >> 16) & 0xff) ;
    *p++ = (uint8) ((utempi >>  8) & 0xff) ;
    *p++ = (uint8) ((utempi >>  0) & 0xff) ;

    memset (p, 0xff, 188-((uint)p-(uint)eitp)) ;	// pad out the rest of the packet
}


void juliandate (int32* julday, int32* julhour)
{
    uint16          utempi ;
    int             x ;
    int             year ;
    int				month ;
    int				day ;
  	time_t 			timex ;
   	struct tm		mytime ;

	timex 	= time (NULL) ;
    mytime 	= *gmtime (&timex) ;

	year 	= mytime.tm_year ;
	while (year >= 100)
	{
		year -= 100 ;										// appears to start from 1900
	}
	month 	= mytime.tm_mon + 1 ;							// returned month = 0-11
	day 	= mytime.tm_mday ;

    utempi 	= DAY0 ;

    for (x = 0 ; x < year ; x++)
    {
        utempi += 365 ;
        if ((x & 3) == 0)
        {
            utempi++ ;
        }
    }

    for (x = 1 ; x < month ; x++)
    {
        utempi += daysinmonth [x] ;
        if (x == 2 && ((year & 3) == 0))
        {
            utempi++ ;
        }
    }

	utempi += day ;

	*julday = utempi ;
	*julhour  = mytime.tm_hour ;
}


uint32 calculateCRC32 (uint8* data, uint32 dataLength)
{
    uint32      crc ;
    uint32      poly, temp, temp2, temp4, bit31 ;
	uint32		x ;

	crc = 0xffffffff ;
	poly = 0x04c11db7 ;

   	while (dataLength-- > 0)
	{
		temp4 = poly ;
		temp2 = (crc >> 24) ^ *data ;
		temp = 0 ;

		for (x = 0 ; x < 8 ; x++)
		{
			if ((temp2 >> x) & 1)
			{
				temp ^= temp4 ;
			}

			bit31 = temp4 >> 31 ;
			temp4 <<= 1 ;
			if (bit31)
			{
				temp4 ^= poly ;
			}
		}
		crc = (crc<<8) ^ temp ;
		data++ ;
   	}
    return crc ;
}

// reverse the byte order in a word

uint32 reverse (uint32 indata)
{
	uint32		tempu ;

	tempu  = 0 ;
	tempu |= ((indata >> 24) & 0xff) <<  0 ;
	tempu |= ((indata >> 16) & 0xff) <<  8 ;
	tempu |= ((indata >>  8) & 0xff) << 16 ;
	tempu |= ((indata >>  0) & 0xff) << 24 ;
	
	return (tempu) ;
}

void getdatetime (char *string)
{
   	struct tm		mt ;
  	time_t 			timex ;

	timex 	= time (NULL) ;
    mt 		= *gmtime (&timex) ;

	sprintf (string, "%04d-%02d-%02d %02d:%02d:%02d", 1900+mt.tm_year, mt.tm_mon+1, mt.tm_mday, mt.tm_hour, mt.tm_min, mt.tm_sec) ;
}


void logit (char *string)
{
	FILE 	*op ;
	char	temps [256] ;

	op = fopen (logfilename, "at") ;
	if (op)
	{
		getdatetime (temps) ;
		fprintf (op, "%s  ", temps) ;	
		fprintf (op, "%s  ", string) ;
		fprintf (op, "\r\n") ;
		fclose  (op) ;			
	}
}	


void whexit (int32 reason)
{
	char		temps [256] ;

    printf ("============================================================================================\r\n") ;                         
	sprintf (temps, "WinterHill stopped: (%d) ", reason) ;
	printf ("%s\r\n", temps) ;
    printf ("============================================================================================\r\n") ;                         

	strcat (temps, ">>>>>>>>>>") ;
	logit (temps) ;

	printf ("\r\n") ;

	usleep (3 * 1000 * 1000) ;
	exit (reason) ;
}	

