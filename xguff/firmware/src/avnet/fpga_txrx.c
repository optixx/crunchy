#include "fpga_txrx.h"
#include "fpga_regs_common.h"
#include "usrp_common.h"
#include "usrp_globals.h"
#include "spi.h"

unsigned char g_tx_reset = 0;
unsigned char g_rx_reset = 0;

static void
fpga_write_reg (unsigned char regno, const xdata unsigned char *regval)
{
	spi_write (0, 0x00 | (regno & 0x7f),
			SPI_ENABLE_FPGA,
			SPI_FMT_MSB | SPI_FMT_HDR_1,
			regval, 4);
}

static xdata unsigned char regval[4] = {0, 0, 0, 0};

static void
write_fpga_master_ctrl (void)
{
#if 1
	unsigned char v = 0;
	if (g_tx_enable) v |= bmFR_MC_ENABLE_TX;
	if (g_rx_enable) v |= bmFR_MC_ENABLE_RX;
	if (g_tx_reset)  v |= bmFR_MC_RESET_TX;
	if (g_rx_reset)  v |= bmFR_MC_RESET_RX;
	regval[3] = v;

	fpga_write_reg (FR_MASTER_CTRL, regval);
#endif
}

void
fpga_set_tx_enable (unsigned char on)
{ /* placeholder */
	(void) on; /* unused */
}

void
fpga_set_rx_enable (unsigned char on)
{
	on &= 0x1;
	g_rx_enable = on;
	write_fpga_master_ctrl ();
	if (on){
		g_rx_overrun = 0;
		fpga_clear_flags ();
	}
}

void
fpga_set_tx_reset (unsigned char on)
{ /* placeholder */
	(void) on; /* unused */
}

void
fpga_set_rx_reset (unsigned char on)
{
	on &= 0x1;
	g_rx_reset = on;
	if (1) {
		FIFORESET = bmNAKALL;  SYNCDELAY;
		FIFORESET = 6;         SYNCDELAY;
		FIFORESET = 0;         SYNCDELAY;
	}
	write_fpga_master_ctrl ();
}
