#ifndef _USRP_AVNET_REGS_H_
#define _USRP_AVNET_REGS_H_

#include "fx2regs.h"

/*
 * Port A (bit addressable):
 */

#define USRP_PA                 IOA             // Port A
#define USRP_PA_OE              OEA             // Port A direction register

#define bmPA_S_CLK              bmBIT0          // SPI serial clock
#define bmPA_S_DATA_TO_PERIPH   bmBIT1          // SPI SDI (peripheral rel name)
#define bmPA_S_DATA_FROM_PERIPH bmBIT2          // SPI SDO (peripheral rel name)
#define bmPA_SEN_FPGA           bmBIT3          // serial enable for FPGA (active low)
#define bmPA_SEN_CODEC_A        bmBIT4          // serial enable AD9862 A (active low)
#define bmPA_SEN_CODEC_B        bmBIT5          // serial enable AD9862 B (active low)
#define bmPA_RX_OVERRUN         bmBIT6          // misc pin to FPGA (overflow)
#define bmPA_TX_UNDERRUN        bmBIT7          // misc pin to FPGA (underflow)

sbit at 0x80+0 bitS_CLK;                // 0x80 is the bit address of PORT A
sbit at 0x80+1 bitS_OUT;                // out from FX2 point of view
sbit at 0x80+2 bitS_IN;                 // in from FX2 point of view
sbit at 0x80+5 FPGA_CLR_STATUS;

/* all outputs except S_DATA_FROM_PERIPH, RX_OVERRUN, TX_UNDERRUN */

#define bmPORT_A_OUTPUTS \
	( bmPA_S_CLK               \
	| bmPA_S_DATA_TO_PERIPH    \
	| bmPA_SEN_FPGA            \
	| bmPA_SEN_CODEC_A         \
	| bmPA_SEN_CODEC_B         \
	)
#define bmPORT_A_INITIAL (bmPA_SEN_FPGA | bmPA_SEN_CODEC_A | bmPA_SEN_CODEC_B)

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

/* Pin/port assignment specific to Avnet Virtex 4LX Evaluation board
 *
 *  58  PC1/GPIFADR1                 FPGA_M2
 *  59  PC2/GPIFADR2                 FPGA_M1
 *  60  PC3/GPIFADR3                 FPGA_M0
 *
 *  61  PC4/GPIFADR4  RP96-8 RP96-1  JTAG_TDI
 *  62  PC5/GPIFADR5  RP96-7 RP96-2  JTAG_TDO
 *  63  PC6/GPIFADR6  RP96-6 RP96-3  JTAG_TMS
 *  64  PC7/GPIFADR7  RP96-5 RP96-4  JTAG_TCK
 *
 *  TDO and TDI are defined from the perspective of the chip
 *  being "tested".  This controller needs to drive TDI and
 *  read TDO.
 */
xdata at 0xE671 volatile unsigned char PORTCCFG ;  // I/O PORTC Alternate Configuration
sfr at 0xB4 OEC;
sfr at 0xA0 IOC;

sbit at 0xA0+4 JTAG_TDI;
sbit at 0xA0+5 JTAG_TDO;
sbit at 0xA0+6 JTAG_TMS;
sbit at 0xA0+7 JTAG_TCK;

/* Output enable bits 7, 6, 4: TCK, TMS, TDI */
#define JTAG_PORT_SETUP() do { PORTCCFG = 0x00; OEC = 0xd0; } while (0)

#endif
