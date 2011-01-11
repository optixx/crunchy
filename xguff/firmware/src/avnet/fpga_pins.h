#ifndef _FPGA_PINS_H_
#define _FPGA_PINS_H_

#define Avnet  0x0018
#define LLRF4  0x0019
#define LLRF41 0x001a

#if TARGET_BOARD == Avnet
#include "fpga_pins_avnet.h"
#else
#if TARGET_BOARD == LLRF4
#include "fpga_pins_llrf4.h"
#else
#if TARGET_BOARD == LLRF41
#include "fpga_pins_llrf41.h"
#else
#error unspecified target board
#endif
#endif
#endif

#endif
