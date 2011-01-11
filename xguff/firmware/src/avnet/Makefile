# Relevant targets:
#    Avnet    0x0018   (also UXO)
#    LLRF4    0x0019
#    LLRF41   0x001a

TARGET_BOARD = 0x0018
# Controls conditional settings in this Makefile,
# and is passed to the C compiler and from there
# activates conditional settings in fpga_pins.h.

XCC=sdcc -mmcs51 --no-xinit-opt
XAS=asx8051 -plosgff

# with EA = 0, the FX2 implements a portion of the 8051 "external memory"
# on chip.  This memory is mapped like this:
#
# The bottom 8K of memory (0x0000 - 0x1fff) is used for both data and
# code accesses.  There's also 512 bytes for data only from 0xe000 - 0xe1ff.
#
# We tell the linker to start the xdata segment at 0x1800, 6K up from
# the bottom.

MEMOPTS = --code-loc 0x0000 --code-size 0x1800 --xram-loc 0x1800 --xram-size 0x0800 \
 -Wl '-b USBDESCSEG = 0xE000'

LINKOPTS = $(MEMOPTS) $(LIBOPTS)

# these files are in lib in real usrp build
LIB_OBJS = \
	fx2utils.rel i2c.rel delay.rel usb_common.rel timer.rel isr.rel

# jtag_util.rel jtag_bit_avnet.rel
USRP_OBJS = \
        vectors.rel \
        usrp_main.rel usrp_common.rel board_specific.rel        \
        fpga_load.rel fpga_txrx.rel jtag_8051.rel init_gpif.rel usrp_gpif.rel \
        eeprom_io.rel usb_descriptors.rel

ifeq ($(TARGET_BOARD),0x0018)
USRP_OBJS += spi_real.rel fpga_reset_avnet.rel
else
USRP_OBJS += spi_jtag.rel fpga_reset_llrf.rel
endif

USRP_OBJS += _startup.rel

all: usrp_main.ihx

usrp_main.ihx: $(USRP_OBJS) $(LIB_OBJS)
	$(XCC) $(LINKOPTS) -o $@ $^

%.rel: %.c
	$(XCC) -DTARGET_BOARD=$(TARGET_BOARD) -I../avnet -I../../include -c -o $@ $<

usb_descriptors.a51: usb_descriptors.pre_a51
	sed -e s/_TARGET_ID_/$(TARGET_BOARD)/ < $< > $@

%.rel: %.a51
	test -f `basename '$<'` || ln -s '$<' .
	$(XAS) `basename '$<'`

# fpga_load_jtag.rel jtag_util.rel jtag_bit_avnet.rel: jtag_util.h
i2c.rel jtag_8051.rel fpga_load.rel: jtag_util.h

clean:
	rm -f *.asm *.ihx *.lnk *.lst *.map *.mem *.rel *.rst *.sym
