#ifndef _USRP_AVNET_REGS_H_
#define _USRP_AVNET_REGS_H_

#include "fx2regs.h"

/*
 * Port A (bit addressable):
 */

#define USRP_PA                 IOA             // Port A
#define USRP_PA_OE              OEA             // Port A direction register

/* assume we use XERR for both overrun and underrun,
 * and SLED as clear error */
#define bmPA_RX_OVERRUN         bmBIT2          // misc pin to FPGA (overflow)
#define bmPA_TX_UNDERRUN        bmBIT2          // misc pin to FPGA (underflow)
sbit at 0x80+3 FPGA_CLR_STATUS;  // see pin assignment comments below

#if 0  /* the rest pretty much don't exist on LLRF4 */
#define bmPA_S_CLK              bmBIT0          // SPI serial clock
#define bmPA_S_DATA_TO_PERIPH   bmBIT1          // SPI SDI (peripheral rel name)
#define bmPA_S_DATA_FROM_PERIPH bmBIT2          // SPI SDO (peripheral rel name)
#define bmPA_SEN_FPGA           bmBIT3          // serial enable for FPGA (active low)
#define bmPA_SEN_CODEC_A        bmBIT4          // serial enable AD9862 A (active low)
#define bmPA_SEN_CODEC_B        bmBIT5          // serial enable AD9862 B (active low)

sbit at 0x80+0 bitS_CLK;                // 0x80 is the bit address of PORT A
sbit at 0x80+1 bitS_OUT;                // out from FX2 point of view
sbit at 0x80+2 bitS_IN;                 // in from FX2 point of view

/* all outputs except S_DATA_FROM_PERIPH, RX_OVERRUN, TX_UNDERRUN */

#define bmPORT_A_OUTPUTS \
	( bmPA_S_CLK               \
	| bmPA_S_DATA_TO_PERIPH    \
	| bmPA_SEN_FPGA            \
	| bmPA_SEN_CODEC_A         \
	| bmPA_SEN_CODEC_B         \
	)
#define bmPORT_A_INITIAL (bmPA_SEN_FPGA | bmPA_SEN_CODEC_A | bmPA_SEN_CODEC_B)
#else
#define bmPORT_A_OUTPUTS (0x79)
#define bmPORT_A_INITIAL (0)
#endif

#define bmPORT_C_OUTPUTS (0xd0)       /* enable bits 7, 6, 4: TCK, TMS, TDI */
#define bmPORT_C_INITIAL (0)

#define USRP_PE                 IOE             // Port E
#define USRP_PE_OE              OEE             // Port E direction register

#define bmPORT_E_OUTPUTS (0x0f)
#define bmPORT_E_INITIAL (0x01)

/*
 * FPGA output lines that are tied to FX2 RDYx inputs.
 * These are readable using GPIFREADYSTAT.
 */
#define bmFPGA_HAS_SPACE     bmBIT0  // usbrdy[0] has room for 512 byte packet
#define bmFPGA_PKT_AVAIL     bmBIT1  // usbrdy[1] has >= 512 bytes available

/*
 * FPGA input lines that are tied to the FX2 CTLx outputs.
 *
 * These are controlled by the GPIF microprogram...
 */
// WR                                   bmBIT0  // usbctl[0]
// RD                                   bmBIT1  // usbctl[1]
// OE                                   bmBIT2  // usbctl[2]

/* Pin/port assignment specific to LLRF4.1 board
 * http://recycle.lbl.gov/llrf4/
 *
 *  U2-33  PA0/INT0#        RN1-8 RN1-1  JTAG_TCK
 *  U2-34  PA1/INT1#        RN1-7 RN1-2  JTAG_TMS
 *  U2-35  PA2/SLOE                      XERR
 *  U2-36  PA3/WU2          RN1-6 RN1-3  JTAG_TDI
 *
 *  U2-37  PA4/FIFOADR0                  PROG_B
 *  U2-38  PA5/FIFOADR1                  INIT_B
 *  U2-39  PA6/PKTEND                    SLED
 *  U2-40  PA7/FLAGD/SLCS#  RN1-5 RN1-4  JTAG_TDO
 *
 *  TDO and TDI are defined from the perspective of the chip
 *  being "tested".  This controller needs to drive TDI and
 *  read TDO.
 */
xdata at 0xE670 volatile unsigned char PORTACFG ;  // I/O PORTA Alternate Configuration
xdata at 0xE671 volatile unsigned char PORTCCFG ;  // I/O PORTC Alternate Configuration
sfr at 0x80 IOA;
sfr at 0xA0 IOC;
sfr at 0xB2 OEA;
sfr at 0xB4 OEC;

sbit at 0x80+0 JTAG_TCK;
sbit at 0x80+1 JTAG_TMS;
sbit at 0x80+3 JTAG_TDI;
sbit at 0x80+7 JTAG_TDO;

/* Output enable bits 3, 1, 0: TCK, TMS, TDI */
/* clash with bmPORT_A_OUTPUTS */
#define JTAG_PORT_SETUP() do { PORTACFG = 0x00; OEA = 0x5B; } while (0)

#endif
