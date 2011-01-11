CFLAGS=-O2 -I . -I xguff  -I /usr/include/python2.6 -g
LDFLAGS=-lusb -lftdi

.cpp.o:
	$(CROSS)g++ $(CFLAGS) -c -o $@ $*.cpp

all: xc3sprog jtag.so

OBJS = bitfile.o \
	devicedb.o iobase.o iodebug.o \
	ioftdi.o ioparport.o ioxpc.o iofx2.o javr.o \
	jedecfile.o jtag.o mapfile_xc2c.o progalgavr.o \
	progalgspiflash.o progalgxc2c.o progalgxc3s.o progalgxc95x.o progalgxcf.o \
	progalgxcfp.o srecfile.o \
	utilities.o xc3sprog.o

xc3sprog: $(OBJS)
	$(CROSS)g++ $(OBJS) $(LDFLAGS) -o xc3sprog

jtag.so: $(OBJS) python.o
	$(CROSS)g++ $(OBJS) python.o $(LDFLAGS) -o jtag.so -shared

clean:
	rm -f $(OBJS)
