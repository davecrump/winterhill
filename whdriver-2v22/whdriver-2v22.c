#define VERSION "whdriver-2v22"

// winterhill kernel module device driver: /dev/winterhill2v22

/* This was derived from:
 *
 * @file   ebbchar.c
 * @author Derek Molloy
 * @date   7 April 2015
 * @version 0.1
 * @brief   An introductory character driver to support the second article of my series on
 * Linux loadable kernel module (LKM) development. This module maps to /dev/ebbchar and
 * comes with a helper C program that can be run in Linux user space to communicate with
 * this the LKM.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
*/
 
#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>        // Required for the copy to user function
#include <linux/gpio.h>           // Required for the GPIO functions
#include <linux/interrupt.h>      // Required for the IRQ code
#include <linux/delay.h>      	
#include <linux/timer.h>
#include <linux/jiffies.h>

 #include <linux/timer.h>
 #include <linux/jiffies.h>

typedef signed int		int32 ;
typedef unsigned int	uint32 ;
typedef unsigned char	uint8 ;

#define TIMEOUT100ms	100 		// milliseconds in a timer tick

#define DEVICE_NAME 	VERSION 	///< The device will appear at /dev/winterhill2v22 using this value
#define CLASS_NAME  	"ebb"       ///< The device class -- this is a character device driver

// spiA for PIC_A

#define SPISS_A             8                                       // select 	on BCMGPIO  8 J:24 (active low)
#define SPICLK_A            11                                      // clock 	on BCMGPIO 11 J:23
#define SPIMISO_A           9                                      	// data in 	on BCMGPIO  9 J:21
#define SPIRDY_A           	10 	                                    // ready in on BCMGPIO 10 J:19 (active low)
#define ALT_A				FSEL_ALT0								// CLK and MISO  on alternate function 0

// spiB for PIC_B

#define SPISS_B             18                                      
#define SPICLK_B            21   
#define SPIMISO_B           19                                   
#define SPIRDY_B           	20 	   
#define ALT_B				FSEL_ALT3								// CLK and MISO on alternate function 3

// physical addresses for RPi4

#define SPIA_BASE				(0xfe204000)
#define SPIB_BASE				(0xfe204c00)
#define GPIO_BASE				(0xfe200000)
#define PACTL_BASE				(0xfe204e00)

// GPIO register indexes

#define GPSET0					(0x1c / 4)							// make output pin high register
#define GPCLR0					(0x28 / 4)							// make output pin low register
#define GPLEV0					(0x34 / 4)							// input register
#define GPFSEL0 				(0x00 / 4) 							// io function select for BCMGPIO 0-9
#define GPFSEL1 				(0x04 / 4) 							// io function select for BCMGPIO 10-19
#define GPFSEL2 				(0x08 / 4) 							// io function select for BCMGPIO 20-29 

// GPIO alternate pin functions

#define FSEL_ALT0               4           		 
#define FSEL_ALT3               7
#define FSEL_ALT4               3           		 
#define FSEL_ALT5               2           		
#define FSEL_OUTPUT				1					
#define FSEL_INPUT				0

// SPI registers byte indexes

#define SPI_CS					(0x00 / 4)
#define SPI_FIFO				(0x04 / 4)
#define SPI_CLK					(0x08 / 4)
#define SPI_DLEN				(0x0c / 4)
#define SPI_LTOH				(0x10 / 4)
#define SPI_DC					(0x14 / 4)
#define CLOCKDIV_SPI			32

#define AUX             		(0x15000)   	// enable register for aux uart, spiA, spi1
#define AUX_ENABLES             (0x04 / 4) 		// enable register for aux uart, spiA, spi1

// SPI bit masks used by SPI assembler routines

#define SSMASK_A				(1 << SPISS_A)
#define READYMASK_A				(1 << SPIRDY_A)
#define SSMASK_B				(1 << SPISS_B)
#define READYMASK_B				(1 << SPIRDY_B)

#define	RXR_SPI					(1 << 19)
#define	TXD_SPI					(1 << 18)
#define	RXD_SPI					(1 << 17)
#define DONE_SPI				(1 << 16)
#define ADCS_SPI				(1 << 11)
#define INTR_SPI				(1 << 10)
#define INTD_SPI				(1 << 9)
#define TA_SPI					(1 << 7)
#define CLEARRX_SPI				(1 << 5)
#define CLEARTX_SPI				(1 << 4)	
#define CLEAR_SPI				(1 << 4)	
#define CPOL_SPI				(1 << 3)
#define CPHA_SPI				(1 << 2)

#define BOARD_RESET				7				// board reset on BCM GPIO 7 (active low)
 
MODULE_LICENSE			("GPL");            							///< The license type -- this affects available functionality
MODULE_AUTHOR			("Brian Jordan (see source for derivation credit)"); ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION		("Linux char driver for winterhill2v22");  		///< The description -- see modinfo
MODULE_VERSION			("2v22");            							///< A version number to inform users

static volatile uint32*	gpio ;
static volatile uint32*	spiA ;
static volatile uint32*	spiB ;
static volatile uint32*	pactl ;

static struct task_struct*	sleeping_task ;
static struct task_struct*	sleeping_task2 ;

static volatile	int32	timerticks ;
static volatile	int32	spi5interruptnumber ;
static volatile	int32	readstatus ;
static volatile	uint32	debugA ;
static volatile	uint32	debugB ;
static volatile	uint32	stateA ;
static volatile	uint32	stateB ;
static volatile	uint32	readtoggleAB ;
static volatile	uint32	debug0 ;
static volatile int 	countspiinterrupts ;			
static volatile int 	spiBinterruptcount ;			
static volatile int		countreadyAinterrupts ;			
static volatile int		countreadyBinterrupts ;			
static volatile int		count1 ;			
static volatile int		count2 ;			
static volatile int		deviceopen ;
static volatile int	 	spiAreadyintno ;		/// interrupt number for PIC_A READY 
static volatile int	 	spiBreadyintno ;		/// interrupt number for PIC_B READY 
static volatile int		majorNumber;            ///< Stores the device number -- determined automatically
static volatile int		numberOpens = 0;      	///< Counts the number of times the device is opened
static struct class*  	winterhillClass  = NULL; 	///< The device-driver class struct pointer
static struct device* 	winterhillDevice = NULL; 	///< The device-driver device struct pointer

static volatile uint8	spiAbuff [192] ;
static volatile uint8*	spiAptr ;
static volatile uint8*	spiAstart ;
static volatile uint32	spiAtxcount ;
static volatile uint32	spiArxcount ;
static volatile uint8	spiBbuff [192] ;
static volatile uint8*	spiBptr ;
static volatile uint8*	spiBstart ;
static volatile uint32	spiBtxcount ;
static volatile uint32	spiBrxcount ;
static volatile int		spihandled ;

#define RXBUFFERSIZE	192						// 188 byte packet + 4 status
#define MAXRXBUFFERS	4096

static volatile uint8 	rxbuffersA [MAXRXBUFFERS] [RXBUFFERSIZE] ;
static volatile int		rxbuffindexinA ;
static volatile int		rxbuffindexoutA ;
static volatile int		rxbuffersreadyA ;
static volatile int		rxbuffersfetchedA ;

static volatile uint8 	rxbuffersB [MAXRXBUFFERS] [RXBUFFERSIZE] ;
static volatile int		rxbuffindexinB ;
static volatile int		rxbuffindexoutB ;
static volatile int		rxbuffersreadyB ;
static volatile int		rxbuffersfetchedB ;
 
// The prototype functions for the character driver -- must come before the struct definition

static int     			dev_open	(struct inode *, struct file *);
static int     			dev_release	(struct inode *, struct file *);
static ssize_t 			dev_read	(struct file *, char *, size_t, loff_t *);
static ssize_t 			dev_write	(struct file *, const char *, size_t, loff_t *);

static irq_handler_t 	picready_handler 	(unsigned int irq, void *dev_id, struct pt_regs *regs) ;
static irq_handler_t 	spi_handler 		(unsigned int irq, void *dev_id, struct pt_regs *regs) ;

///static uint32			gpioget 			(uint32) ;
static void					gpioset 			(uint32, uint32) ;
static void					gpioconfig 			(uint32, uint32) ;
static struct timer_list 	mytimer ;
 
/*
 * @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
*/

static struct file_operations fops =
{
   .open 	= dev_open,
   .read 	= dev_read,
   .write 	= dev_write,
   .release = dev_release,
} ;


// 100ms timer tick handler

void mytimer_handler (struct timer_list *data)
{
	struct task_struct		*temp ;
	
	mod_timer (&mytimer, jiffies + msecs_to_jiffies(TIMEOUT100ms)) ;

	timerticks++ ;
	if (timerticks >= 5 && sleeping_task)
	{
		readstatus = -5 ;
		temp = sleeping_task ;
		sleeping_task = 0 ;
		wake_up_process (temp) ;
	}	
}
                                       

/*
 *  @brief The LKM initialization function
 *
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
*/

static int __init winterhill_init (void)
{
   printk(KERN_INFO "winterhill: Initializing the winterhill LKM\n");
 
// Try to dynamically allocate a major number for the device -- more difficult but worth it

   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber < 0)
   {
      printk (KERN_ALERT "winterhill failed to register a major number\n");
      return (majorNumber) ;
   }
   printk (KERN_INFO "winterhill: registered correctly with major number %d\n", majorNumber);
 
// Register the device class

   winterhillClass = class_create (THIS_MODULE, CLASS_NAME);
   if (IS_ERR(winterhillClass))						   		// Check for error and clean up if there is
   {
      unregister_chrdev (majorNumber, DEVICE_NAME);
      printk (KERN_ALERT "Failed to register device class\n");
      return (PTR_ERR(winterhillClass));          			// Correct way to return an error on a pointer
   }
   printk (KERN_INFO "winterhill: device class registered correctly\n");
 
// Register the device driver

   winterhillDevice = device_create(winterhillClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(winterhillDevice))
   {               											// Clean up if there is an error
      class_destroy (winterhillClass);           			// Repeated code but the alternative is goto statements
      unregister_chrdev (majorNumber, DEVICE_NAME);
      printk (KERN_ALERT "Failed to create the device\n");
      return (PTR_ERR(winterhillDevice));
   }

	pactl 				= 0 ;
	spiA  				= 0 ;
	spiB  				= 0 ;
	gpio  				= 0 ;
	stateA 				= 0 ;
	stateB 				= 0 ;
	debugA 				= 0 ;
	debugB 				= 0 ;
	sleeping_task 		= 0 ;
	deviceopen 			= 0 ;
	spiAptr 			= 0 ;
	spiBptr 			= 0 ;
	rxbuffersreadyA		= 0 ;
	rxbuffersfetchedA	= 0 ;
	rxbuffindexinA		= 0 ;
	rxbuffindexoutA		= 0 ;
	rxbuffersreadyB		= 0 ;
	rxbuffersfetchedB	= 0 ;
	rxbuffindexinB		= 0 ;
	rxbuffindexoutB		= 0 ;
	spi5interruptnumber = 0 ;
	timerticks			= 0 ;

// initialise the 100ms timer

   	timer_setup	(&mytimer, mytimer_handler, 0) ;
   	mod_timer	(&mytimer, jiffies + msecs_to_jiffies(TIMEOUT100ms)) ;                  

// done 

   	printk (KERN_INFO "winterhill: device class created correctly\n") ; 	
   	printk (KERN_INFO "winterhill: /dev/%s installed \n", VERSION); 	

   	return (0) ;
}
 
 
/*
 *  @brief The device open function that is called each time the device is opened
 *
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
 
static int dev_open (struct inode *inodep, struct file *filep)
{
	int		result ;
	
	numberOpens++;

	if ((deviceopen == 0) && spi5interruptnumber)		// must first provide the interrupt number via 'write' 
	{
		rxbuffindexinA 		= 0 ;
		rxbuffindexoutA		= 0 ;
		rxbuffersreadyA		= 0 ;
		rxbuffersfetchedA	= 0 ;
		rxbuffindexinB 		= 0 ;
		rxbuffindexoutB		= 0 ;
		rxbuffersreadyB		= 0 ;
		rxbuffersfetchedB	= 0 ;
		spiBptr 			= 0 ;
		spiAptr 			= 0 ;
		sleeping_task 		= 0 ;
		readstatus 	   		= 0 ;
	
//	set up the virtual addresses
	
		gpio = ioremap_nocache (GPIO_BASE, PAGE_SIZE) ;
   		printk (KERN_INFO "winterhill: GPIO virtual address is 0x%08X\n", (uint32)gpio) ;
	
		spiA = ioremap_nocache (SPIA_BASE, PAGE_SIZE) ;
   		printk (KERN_INFO "winterhill: SPIA virtual address is 0x%08X\n", (uint32)spiA) ;
   		printk (KERN_INFO "winterhill: SPIA[DC] is 0x%08X\n", spiA[5]) ;
	
		spiB = ioremap_nocache (SPIB_BASE, PAGE_SIZE) ;
   		printk (KERN_INFO "winterhill: SPIB virtual address is 0x%08X\n", (uint32)spiB) ;
   		printk (KERN_INFO "winterhill: SPIB[DC] is 0x%08X\n", spiB[5]) ;

		pactl = ioremap_nocache (PACTL_BASE, PAGE_SIZE) ;
   		printk (KERN_INFO "winterhill: GPIO virtual address is 0x%08X\n", (uint32)gpio) ;

// reset the board
		
		asm (" dmb") ;

		gpioset    				(BOARD_RESET, 0) ;
		gpioconfig 				(BOARD_RESET, FSEL_OUTPUT) ;
		msleep        			(100) ;
		gpioset    				(BOARD_RESET, 1) ;
		msleep          		(1000) ;

// configure the GPIO		

		asm (" dmb") ;

		gpioconfig (SPISS_A,	FSEL_OUTPUT) ;
		gpioconfig (SPICLK_A,	ALT_A) ;
		gpioconfig (SPIMISO_A,	ALT_A) ;
		gpioconfig (SPIRDY_A,	FSEL_INPUT) ;
		gpioset	   (SPISS_A,	1) ;
	
		asm (" dmb") ;

		spiA [SPI_CS] 			= 0 ;													// reset
		spiA [SPI_CLK]			= CLOCKDIV_SPI ;										// clock divider to give 16MHz at max RPI4 revs	
		spiA [SPI_CS]			= CLEARTX_SPI | CLEARRX_SPI | CPOL_SPI ;				// clear fifos and set clock / data polarity

 		asm (" dmb") ;

		gpioconfig (SPISS_B,	FSEL_OUTPUT) ;
		gpioconfig (SPICLK_B,	ALT_B) ;
		gpioconfig (SPIMISO_B,	ALT_B) ;
		gpioconfig (SPIRDY_B,	FSEL_INPUT) ;
		gpioset	   (SPISS_B,	1) ;

 		asm (" dmb") ;

		spiB [SPI_CS] 			= 0 ;													// reset
		spiB [SPI_CLK]			= CLOCKDIV_SPI ;										// clock divider to give 16MHz at max RPI4 revs	
		spiB [SPI_CS]			= CLEARTX_SPI | CLEARRX_SPI | CPOL_SPI ;				// clear fifos and set clock / data polarity
		
// set up the interrupts	

	   	spiAreadyintno = gpio_to_irq (SPIRDY_A);
   		printk (KERN_INFO "winterhill: SPIRDY_A is mapped to IRQ: %d\n", spiAreadyintno);
	   	result = request_irq						// This next call requests an interrupt line
  		(
			spiAreadyintno,       					// The interrupt number requested
	        (irq_handler_t) picready_handler, 		// The pointer to the handler function below
    	    IRQF_TRIGGER_FALLING,					// Interrupt on rising edge (button press, not release)
        	"whdriver-2v22_picA_handler",			// Used in /proc/interrupts to identify the owner
	        NULL                 					// The *dev_id for shared interrupt lines, NULL is okay
    	) ;
	    printk (KERN_INFO "winterhill: The SPIRDY_A interrupt request result is: %d\n", result);

	   	spiBreadyintno = gpio_to_irq (SPIRDY_B);
   		printk (KERN_INFO "winterhill: SPIRDY_B is mapped to IRQ: %d\n", spiBreadyintno);
	   	result = request_irq						// This next call requests an interrupt line
  		(
			spiBreadyintno,       					// The interrupt number requested
	        (irq_handler_t) picready_handler, 		// The pointer to the handler function below
    	    IRQF_TRIGGER_FALLING,					// Interrupt on rising edge (button press, not release)
        	"whdriver-2v22_picB_handler",			// Used in /proc/interrupts to identify the owner
	        NULL                 					// The *dev_id for shared interrupt lines, NULL is okay
    	) ;
	    printk (KERN_INFO "winterhill: The SPIRDY_B interrupt request result is: %d\n", result);

   		printk (KERN_INFO "winterhill: SPI is mapped to IRQ: %d\n", spi5interruptnumber);
	   	result = request_irq						// This next call requests an interrupt line
  		(
			spi5interruptnumber, 					// The interrupt number requested
	        (irq_handler_t) spi_handler, 			// The pointer to the handler function below
    	    IRQF_TRIGGER_HIGH | IRQF_SHARED,		// Interrupt when high
        	"whdriver-2v22_spi_handler",			// Used in /proc/interrupts to identify the owner
	        DEVICE_NAME         					// The *dev_id for shared interrupt lines, NULL is okay
    	) ;
	    printk (KERN_INFO "winterhill: The SPI interrupt request result is: %d\n", result);

		stateA = 1 ;
		stateB = 1 ;

 		asm (" dmb") ;

		spiA [SPI_CS] |= INTD_SPI | INTR_SPI ;		// enable INTD and INTR interrupts

 		asm (" dmb") ;

		spiB [SPI_CS] |= INTD_SPI | INTR_SPI ;		// enable INTD and INTR interrupts

	    deviceopen = 1 ;
	}


	if (spi5interruptnumber == 0)
	{
	   	printk (KERN_INFO "winterhill: Opened, but spi5interruptnumber not yet provided\n");
	}

   	printk (KERN_INFO "winterhill: Opened: %d time(s)\n", numberOpens);
	
   	return (0) ;
}

 
/*
 *  @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
*/

 static ssize_t dev_read (struct file *filep, char *buffer, size_t len, loff_t *offset)
 {
   		int 		error_count = 0 ;
static	int			toggleAB ;
		int32		temp ;

	asm (" dmb") ;

	timerticks = 0 ;								// reset the timeout

	if (spi5interruptnumber == 0)
	{
	   	printk (KERN_INFO "winterhill: Read, but spi5interruptnumber not yet provided\n");
	   	return (-4) ;
	}	

	asm (" dmb") ;

	if (len != RXBUFFERSIZE)						// requested length must be RXBUFFERSIZE
	{
	   	printk (KERN_INFO "winterhill: len = %d\n", len);
		return (-1) ;
	}

	while ((rxbuffersreadyA == rxbuffersfetchedA) && (rxbuffersreadyB == rxbuffersfetchedB) && (readstatus == 0))
	{
		if (sleeping_task == 0)
		{
			sleeping_task = current ;
		}
		set_current_state (TASK_INTERRUPTIBLE) ;
		schedule() ;		
	}

	asm (" dmb") ;

	if (readstatus)
	{
		temp = readstatus ;
		readstatus = 0 ;
		return (temp) ;
	}

 	while (1)
  	{
		if ((toggleAB++ & 1) == 0)
		{
			if (rxbuffersreadyB != rxbuffersfetchedB)
			{
				error_count = copy_to_user (buffer, (void*)(&rxbuffersB[rxbuffindexoutB]), len) ;
				if (error_count)
				{
				   	printk (KERN_INFO "winterhill: error_count B = %d\n", error_count);		
				}
				asm (" dmb") ;

				rxbuffersfetchedB++ ;
				rxbuffindexoutB++ ;
				if (rxbuffindexoutB >= MAXRXBUFFERS)
				{
					rxbuffindexoutB = 0 ;
				}
				break ;
			}
		}	
		else 
		{
			if (rxbuffersreadyA != rxbuffersfetchedA)
			{
				error_count = copy_to_user (buffer, (void*)(&rxbuffersA[rxbuffindexoutA]), len) ;
				if (error_count)
				{
				   	printk (KERN_INFO "winterhill: error_count A = %d\n", error_count);		
				}
	
				asm (" dmb") ;
	
				rxbuffersfetchedA++ ;
				rxbuffindexoutA++ ;
				if (rxbuffindexoutA >= MAXRXBUFFERS)
				{
					rxbuffindexoutA = 0 ;
				}
				break ;
			}
		}
	}
	
	asm (" dmb") ;
		
	return (RXBUFFERSIZE) ;
}
 
/*
 *  @brief This function is called whenever the device is being written to from user space i.e.
 *
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
*/
 
static ssize_t dev_write (struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	if (len == 4)
	{
		spi5interruptnumber = * (uint32*) buffer ;
	   	printk (KERN_INFO "winterhill: spi5interruptnumber (%d) provided\n", spi5interruptnumber) ;
		return (0) ;
	}
	else
	{
   		return (-2) ;
   	}	
}

 
/*
 * @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
 
static int dev_release (struct inode *inodep, struct file *filep)
{
	if (deviceopen)
	{	
		readstatus = -3 ;
		if (sleeping_task)
		{
			sleeping_task2 = sleeping_task ;
			sleeping_task = 0 ;
			wake_up_process (sleeping_task2) ;
		}
		debug0 = spiA [SPI_CS] ;

 		asm (" dmb") ;

		spiB[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA

 		asm (" dmb") ;

		spiA[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA
		msleep (100) ;

 		asm (" dmb") ;

		spiB[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA

 		asm (" dmb") ;

		spiA[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA
		msleep (100) ;

		free_irq (spi5interruptnumber, DEVICE_NAME);    		// Free the IRQ number
		free_irq (spiBreadyintno, NULL);            // Free the IRQ number, no *dev_id required in this case
   		free_irq (spiAreadyintno, NULL);            // Free the IRQ number, no *dev_id required in this case
		iounmap (pactl) ;
		iounmap (spiB) ;
		iounmap (spiA) ;
		iounmap (gpio) ;
		pactl = 0 ;
		spiB  = 0 ;
		spiA  = 0 ;
		gpio  = 0 ;
		spiBptr = 0 ;
		spiAptr = 0 ;
		spi5interruptnumber = 0 ;
   		deviceopen 			= 0 ;
	}

   	printk (KERN_INFO "winterhill: Closed\n") ;

   	return (0);
}

 
/*
 *  @brief The LKM cleanup function
 *
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */

 static void __exit winterhill_exit (void)
 {
    del_timer (&mytimer) ;
    
    if (deviceopen)
 	{
	   	printk (KERN_INFO "winterhill: Device is open\n");
		readstatus = -3 ;
		if (sleeping_task)
		{	
		   	printk (KERN_INFO "winterhill: Sleeping task active\n");
			wake_up_process (sleeping_task) ;
			sleeping_task = 0 ;
		}

 		asm (" dmb") ;
		debug0 = spiA [SPI_CS] ;
 		asm (" dmb") ;
		spiB[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA
 		asm (" dmb") ;
		spiA[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA

		msleep (100) ;

 		asm (" dmb") ;
		spiB[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA
 		asm (" dmb") ;
		spiA[SPI_CS] &= ~(INTD_SPI | INTR_SPI | (3 * CLEAR_SPI) | TA_SPI) ;	// disable INTD and INTR interrupts and TA
 		asm (" dmb") ;

		msleep (100) ;

	   	printk (KERN_INFO "winterhill: AAA\n");
		free_irq (spi5interruptnumber, DEVICE_NAME);// Free the IRQ number
		free_irq (spiBreadyintno, NULL);            // Free the IRQ number, no *dev_id required in this case
   		free_irq (spiAreadyintno, NULL);            // Free the IRQ number, no *dev_id required in this case
	   	printk (KERN_INFO "winterhill: BBB\n");
		iounmap (pactl) ;
		iounmap (spiB) ;
		iounmap (spiA) ;
		iounmap (gpio) ;
	   	printk (KERN_INFO "winterhill: CCC\n");
		pactl = 0 ;
		spiB  = 0 ;
		spiA  = 0 ;
		gpio  = 0 ;
	   	printk (KERN_INFO "winterhill: DDD\n");
		spiBptr = 0 ;
		spiAptr = 0 ;
	   	printk (KERN_INFO "winterhill: Interrupts freed\n");
        deviceopen = 0 ;
   	}
   	
   	device_destroy		(winterhillClass, MKDEV(majorNumber, 0));	// remove the device
   	class_unregister	(winterhillClass);            		       	// unregister the device class
   	class_destroy		(winterhillClass);                         	// remove the device class
   	unregister_chrdev	(majorNumber, DEVICE_NAME);             	// unregister the major number
   	printk				(KERN_INFO "winterhill: Driver removed from the kernel\n") ;
}

static irq_handler_t picready_handler (unsigned int irq, void *dev_id, struct pt_regs *regs)
{
	uint32			handled ;
	uint32			gplev ;

	handled = 0 ;

	asm (" dmb") ;

	gplev = gpio[GPLEV0] ;

	asm (" dmb") ;

	if (spiAptr == 0)
	{
 		asm (" dmb") ;

		if ((gplev & READYMASK_A) == 0)
		{
			spiAtxcount 	= 192 ;
			spiArxcount 	= 192 ;
	 		asm (" dmb") ;	 		
			spiAptr			= rxbuffersA [rxbuffindexinA] ;
			spiAstart		= spiAptr ;
			memset ((void*)spiAptr, 0xcd, RXBUFFERSIZE) ; /////////
			rxbuffindexinA  ++ ;
			if (rxbuffindexinA >= MAXRXBUFFERS)
			{
				rxbuffindexinA = 0 ;
			}
				
	 		asm (" dmb") ;
			gpio [GPCLR0]   = SSMASK_A ;						// SS low
 			asm (" dmb") ;
			spiA [SPI_CS]  |= TA_SPI ;							// enable a transfer
			handled++ ;
		}
	}

	if (spiBptr == 0)
	{
 		asm (" dmb") ;
		if ((gplev & READYMASK_B) == 0)
		{
			spiBtxcount 	= 192 ;
			spiBrxcount 	= 192 ;
 			asm (" dmb") ;
			spiBptr			= rxbuffersB [rxbuffindexinB] ;
			spiBstart		= spiBptr ;
			memset ((void*)spiBptr, 0xcd, RXBUFFERSIZE) ; /////////
			rxbuffindexinB  ++ ;
			if (rxbuffindexinB >= MAXRXBUFFERS)
			{
				rxbuffindexinB = 0 ;
			}

 			asm (" dmb") ;
			gpio [GPCLR0]  = SSMASK_B ;							// SS low
 			asm (" dmb") ;
			spiB [SPI_CS] |= TA_SPI ;							// enable a transfer
			handled++ ;
		}
	}

	asm (" dmb") ;

	if (handled)
	{
   		return ((irq_handler_t)IRQ_HANDLED) ;      				// Announce that the IRQ has been handled correctly
   	}
   	else
	{
   		return ((irq_handler_t)IRQ_NONE) ;      				// Announce that the IRQ has been handled correctly
   	}
}


static irq_handler_t spi_handler (unsigned int irq, void *dev_id, struct pt_regs *regs)
{
	uint32		spihandled ;
	uint32		pactlstatus ;
		
	asm (" dmb") ;

	pactlstatus = *pactl ;										// get the interrupt status for several peripherals

	asm (" dmb") ;	

	spihandled = 0 ;
	countspiinterrupts++ ;     	           	   

	if (pactlstatus & 1)											// SPI0 has interrupted		
	{
		if (spiAptr)							
		{
			asm (" dmb") ;
		  	if (spiA[SPI_CS] & RXR_SPI)
			{
				spihandled++ ;
				while (spiArxcount && (spiA[SPI_CS] & RXD_SPI))
				{
					*spiAptr++ = spiA[SPI_FIFO] ;
					spiArxcount-- ;				
				}
			}

			asm (" dmb") ;
			if (spiA[SPI_CS] & DONE_SPI)
			{
				stateA = 3 ;
				spihandled++ ;
				if (spiAtxcount == 0)
				{
					while (spiArxcount)
					{
						if (spiA[SPI_CS] & RXD_SPI)
						{
							*spiAptr++ = spiA[SPI_FIFO] ;
							spiArxcount-- ;				
						}					
					}
					asm (" dmb") ;
					spiA[SPI_CS] &= ~TA_SPI ;							// disable transfer
					asm (" dmb") ;
					spiAptr = 0 ;
					rxbuffersreadyA++ ;
					asm (" dmb") ;
					if (sleeping_task)
					{
						wake_up_process (sleeping_task) ;
						sleeping_task = 0 ;
					}
		 			asm (" dmb") ;
					gpio[GPSET0] = SSMASK_A ;							// SS high
					stateA = 1 ;
				}
				else
				{
		 			asm (" dmb") ;
					while (spiAtxcount && (spiA[SPI_CS] & TXD_SPI))			// room for tx data
					{
						spiA[SPI_FIFO] = 0xff ;								// dummy transmit byte
						spiAtxcount-- ;
					}
				}
			}
		}
	}
	
	if (pactlstatus & 0x40)													// SPI6 has interrupted		
	{
		if (spiBptr)
		{
			asm (" dmb") ;
		  	if (spiB[SPI_CS] & RXR_SPI)
			{
				spihandled++ ;
				while (spiBrxcount && (spiB[SPI_CS] & RXD_SPI))
				{
					*spiBptr++ = spiB[SPI_FIFO] ;
					spiBrxcount-- ;				
				}
			}

			asm (" dmb") ;
			if (spiB[SPI_CS] & DONE_SPI)
			{
				stateB = 3 ;
				spihandled++ ;
				if (spiBtxcount == 0)
				{
					while (spiBrxcount)
					{
						if (spiB[SPI_CS] & RXD_SPI)
						{
							*spiBptr++ = spiB[SPI_FIFO] ;
							spiBrxcount-- ;				
						}					
					}
					asm (" dmb") ;
					spiB[SPI_CS] &= ~TA_SPI ;							// disable transfer
					asm (" dmb") ;
					spiBptr = 0 ;
					rxbuffersreadyB++ ;
					asm (" dmb") ;
					if (sleeping_task)
					{
						wake_up_process (sleeping_task) ;
						sleeping_task = 0 ;
					}
					asm (" dmb") ;
					gpio[GPSET0] = SSMASK_B ;							// SS high
					stateB = 1 ;
				}
				else
				{
					asm (" dmb") ;
					while (spiBtxcount && (spiB[SPI_CS] & TXD_SPI))			// room for tx data
					{
						spiB[SPI_FIFO] = 0xff ;								// dummy transmit byte
						spiBtxcount-- ;
					}
				}
			}	
		}
  	}

	asm (" dmb") ;

	if (spihandled)
	{
   		return ((irq_handler_t)IRQ_HANDLED) ;      			// Announce that the IRQ has been handled correctly
	}
	else
	{
   		return ((irq_handler_t)IRQ_NONE) ;      			// Announce that the IRQ has been handled correctly
	}
}

        
static void gpioconfig (uint32 bcmportno, uint32 altfunction)
{
    uint32                    index ;
    uint32                    pos ;
    uint32                    temp ;

	asm (" dmb") ;
    index                   = bcmportno / 10 ;                  // 10 gpio settings per word
    pos                     = (bcmportno - (index * 10)) * 3 ;  // get the bit position
    temp                    = gpio [GPFSEL0 + index] ;         	// get the function settings register
    temp                   &= ~(7 << pos) ;                     // make the pin an input
    gpio [GPFSEL0 + index]  = temp ;
    temp                   |= altfunction << pos ;              // set the alternate function
    gpio [GPFSEL0 + index]  = temp ;
}

/*
static uint32 gpioget (uint32 bcmbitno)
{
    uint32      temp ;
    
	asm (" dmb") ;
    temp    = gpio [GPLEV0] & (1 << bcmbitno) ;
    temp  	>>= bcmbitno ;
    return 	(temp) ;
}
*/                                                        

static void gpioset (uint32 bcmbitno, uint32 value)
{
	asm (" dmb") ;
    if (value)
    {
	    gpio [GPSET0] = 1 << bcmbitno ;
    }
    else
   	{
    	gpio [GPCLR0] = 1 << bcmbitno ;
    }                                    
}                                        

                                        
/*
 * @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
*/

module_init (winterhill_init);
module_exit (winterhill_exit);

