from crunchy.fpga.build import *

class Nexys2(Board):
	TARGET = "xc3s1200e-fg320-4"
	PROJECT = "Project_Reset_Clock_XilinxUSER_LED4"
	FILES  = [ProjectFile(ProjectFile.VHDL, "top.vhd")]
	FILES += [ProjectFile(ProjectFile.UCF, "main.ucf")]
