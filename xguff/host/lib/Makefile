CXXFLAGS = -Wall -O -I.
CFLAGS = -Wall -O -I .

libusrp.a: fusb.o  fusb_linux.o  fusb_sysconfig_linux.o  usrp_basic.o  usrp_prims.o
	ar -r $@ $^

clean:
	rm -rf *.o *.a
