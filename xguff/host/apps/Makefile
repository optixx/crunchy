
FLTK_CXXFLAGS := $(shell fltk-config --use-gl --use-glut --cxxflags)
FLTK_LDFLAGS  := $(shell fltk-config --use-gl --use-glut --ldflags)
FLTK_LIBS     := $(shell fltk-config --use-gl --use-glut --libs)

# This g++ code has problems with -Wshadow and -Wwrite-strings
GXX_WARNS = -Wundef -Wredundant-decls -Wpointer-arith -Wcast-qual \
            -Wpointer-arith -Wcast-align -Wcast-qual -Waggregate-return \
            -Wsign-compare
GCC_WARNS = ${GXX_WARNS} -Wnested-externs -Wwrite-strings -Wstrict-prototypes -Wshadow

CXXFLAGS = -Wall -W -g -O2 ${GXX_WARNS} ${CXXFLAGS_$@}
CFLAGS   = -Wall -W -g -O2 ${GCC_WARNS} ${CFLAGS_$@}
LDFLAGS  = $(LDFLAGS_$@)
LDLIBS   = $(LDLIBS_$@)

LINK.o = $(LINK.cc)   # gcc can't link c++ code, switch to g++

CXXFLAGS_CubeViewUI.o = $(FLTK_CXXFLAGS)
CXXFLAGS_CubeView.o   = $(FLTK_CXXFLAGS)
LDFLAGS_rgui          = $(FLTK_LDFLAGS)
LDFLAGS_rgui88        = $(FLTK_LDFLAGS)
LDLIBS_rgui           = $(FLTK_LDLIBS) -lglut
LDLIBS_rgui88         = $(FLTK_LDLIBS) -lglut

CXXFLAGS_usrper.o     = -I../lib
LDFLAGS_usrper        = -L../lib
LDLIBS_usrper         = -lusrp -lusb
CXXFLAGS_jdrive3.o    = -I../lib
LDFLAGS_jdrive3       = -L../lib
LDLIBS_jdrive3        = -lusrp -lusb -lrt

CXXFLAGS_test_usrp_standard_rx.o = -I../lib
LDFLAGS_test_usrp_standard_rx    = -L../lib
LDLIBS_test_usrp_standard_rx     = -lusrp -lusb

CXXFLAGS_rdrive.o   = -I../lib
CXXFLAGS_uxo_util.o = -I../lib
LDFLAGS_rdrive      = -L../lib
LDLIBS_rdrive       = -lusrp -lusb -lrt

PROGS = usrper bit2rbf test_usrp_standard_rx rgui rdrive jdrive3 runpack
 
all: ${PROGS}

CubeView.o runpack.o rfile_util.o: rfile_util.h

CubeView.o: CubeView.h

CubeViewUI.o rgui.o rgui88.o: CubeViewUI.h

rdrive.o uxo_util.o: uxo_util.h

usrper: usrper.o

bit2rbf: bit2rbf.o

test_usrp_standard_rx: test_usrp_standard_rx.o time_stuff.o

rdrive: rdrive.o uxo_util.o aux_check.o ds_decode.o

jdrive3: jdrive3.o uxo_util.o

runpack: runpack.o rfile_util.o aux_check.o ds_decode.o

rgui: rgui.o CubeViewUI.o CubeView.o rfile_util.o

rgui88: rgui88.o CubeViewUI.o CubeView.o rfile_util.o

clean:
	rm -f ${PROGS} *.o
