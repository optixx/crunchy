/********************************************************************\
*
*  Atmel AVR JTAG Programmer for Altera Byteblaster Hardware
*
*  Dec 2001     Ver 0.1 Initial Version
*                       Compiled using Turbo C - Only Supports 64K
*                       Only ATMega128 Supported
*
*  Jun 2002     Ver 1.0 Updated to Compile using GCC
*                       Supports 128K
*
*  May 2003     Ver 2.0 Added support for ATMega32 and ATMega16
*                       Use ioperm in cygwin environment
*                       Should then work on Win2000 and WinXP under
*                       cygwin
*
*  Jun 2003     Ver 2.1 Added support for ATMega64 (untested)
*  Jul 2003     Ver 2.2 Added support for ATMega162
*  Sep 2003     Ver 2.3 Added support for ATMega169 (no fuses yet)
*  Sep 2003     Ver 2.4 Added verify flag (-V)
*
*  $Id: javr.h,v 1.4 2003/09/28 14:31:04 anton Exp $
*  $Log: javr.h,v $
*  Revision 1.4  2003/09/28 14:31:04  anton
*  Added --help command, display GPL
*
*  Revision 1.3  2003/09/28 12:49:45  anton
*  Updated Version, Changed Error printing in Verify
*
*  Revision 1.2  2003/09/27 19:21:59  anton
*  Added support for Linux compile
*
*  Revision 1.1.1.1  2003/09/24 15:35:57  anton
*  Initial import into CVS
*
\********************************************************************/

#define MAX_FLASH_SIZE  (1024UL*128UL)   /* 128K */
#define MAX_EEPROM_SIZE 16384
#define FILL_BYTE       0xFF

typedef struct
{
  unsigned version:4;
  unsigned manuf_id:11;
  unsigned short partnumber;
}JTAG_ID;


typedef struct
{
  unsigned long Instruction;
  char BREAKPT;
}SCAN_1_Info;

typedef struct
{
  unsigned short jtag_id;
  unsigned short eeprom;
  unsigned long flash;
  unsigned short ram;
  unsigned char Index;  /* Use To access array of Device Specific routines */
  const char *name;
}AVR_Data;

typedef struct
{
  unsigned short size[4]; /* BOOTSZ0, BOOTSZ1 Mapping size in bytes */
}BOOT_Size;




/* These defines must be in the same order as data in gAVR_Data[]  array */

#define ATMEGA128       0
#define ATMEGA64        1
#define ATMEGA323       2
#define ATMEGA32        3
#define ATMEGA16        4
#define ATMEGA162       5
#define ATMEGA169       6
#define AT90CAN128      7
#define UNKNOWN_DEVICE  0xFF


/* Flash Write Timing: T_WLRH is 5 ms in the datasheet */
#define T_WLRH 7000
/* Chip erase Timing: T_WLRH_CE is 10 ms in the datasheet */
#define T_WLRH_CE 12000

#ifdef JAVR_M

char gVersionStr[]="2.4";
JTAG_ID gJTAG_ID;


int gPort;
//int debug=UP_FUNCTIONS|UP_DETAILS|UL_FUNCTIONS|UL_DETAILS;
//int debug = UP_FUNCTIONS|PP_FUNCTIONS|PP_DETAILS;
//int debug = UL_FUNCTIONS|UP_FUNCTIONS|HW_FUNCTIONS|HW_DETAILS;
//int debug = UL_FUNCTIONS|UP_FUNCTIONS|PP_FUNCTIONS|HW_FUNCTIONS|HW_DETAILS|PP_DETAILS;
int debug = 0;
//int debug = HW_FUNCTIONS|HW_DETAILS;
char gOldTAPState,gTAPState;
char gTMS;

const char *gTAPStateNames[16]={
                                "EXIT2_DR",
                                "EXIT1_DR",
                                "SHIFT_DR",
                                "PAUSE_DR",
                                "SELECT_IR_SCAN",
                                "UPDATE_DR",
                                "CAPTURE_DR",
                                "SELECT_DR_SCAN",
                                "EXIT2_IR",
                                "EXIT1_IR",
                                "SHIFT_IR",
                                "PAUSE_IR",
                                "RUN_TEST_IDLE",
                                "UPDATE_IR",
                                "CAPTURE_IR",
                                "TEST_LOGIC_RESET"
                               };



const AVR_Data gAVR_Data[]=
                        {/* jtag_id  eeprom  flash       ram    Index  name  */
                            {0x9702, 4096  , 131072UL  , 4096  , 0 ,   "ATMega128"},
                            {0x9602, 2048  , 65536UL   , 4096  , 0 ,   "ATMega64"},
                            {0x9501, 1024  , 32768UL   , 2048  , 0 ,   "ATMega323"},
                            {0x9502, 1024  , 32768UL   , 2048  , 0 ,   "ATMega32"},
                            {0x9403, 512   , 16384UL   , 1024  , 0 ,   "ATMega16"},
                            {0x9404, 512   , 16384UL   , 1024  , 0 ,   "ATMega162"},
                            {0x9405, 512   , 16384UL   , 1024  , 0 ,   "ATMega169"},
                            {0x9781, 4096  , 131072UL  , 4096  , 0 ,   "AT90CAN128"},
                            {0,0, 0, 0, 0, "Unknown"}
                          };

/* Must be in same sequence as gAVR_Data[] */
const BOOT_Size gBOOT_Size[]={
                              {{(4096*2),(2048*2),(1024*2),(512*2)}}, /* ATMega128 */
                              {{(4096*2),(2048*2),(1024*2),(512*2)}}, /* ATMega64 */
                              {{(2048*2),(1024*2),(512*2 ),(256*2)}}, /* ATMega323 */
                              {{(2048*2),(1024*2),(512*2 ),(256*2)}}, /* ATMega32 */
                              {{(1024*2),(512*2 ),(256*2 ),(128*2)}}, /* ATMega16 */
                              {{(1024*2),(512*2 ),(256*2 ),(128*2)}}, /* ATMega162 */
                              {{(1024*2),(512*2 ),(256*2 ),(128*2)}}, /* ATMega169 */
                              {{(4096*2),(2048*2),(1024*2),(512*2)}}  /* AT90CAN128 */
                             };


AVR_Data gDeviceData;
BOOT_Size gDeviceBOOTSize;
unsigned char *gFlashBuffer;
unsigned char gEEPROMBuffer[MAX_EEPROM_SIZE];
unsigned long gFlashBufferSize;

#else
extern int gPort;
extern int debug;
extern char gOldTAPState,gTAPState;
extern char gTMS;
extern const char *gTAPStateNames[16];
extern const char *gICEBreakerRegs[16];
extern const char gICEBreakerRegSizes[16];

extern JTAG_ID gJTAG_ID;
extern AVR_Data gDeviceData;
extern BOOT_Size gDeviceBOOTSize;

extern unsigned char *gFlashBuffer;
extern unsigned char gEEPROMBuffer[];
extern unsigned long gFlashBufferSize;

#endif



void DisplayJTAG_ID(void);
void AllocateFlashBuffer(void);

