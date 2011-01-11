#include <Python.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>


#include "io_exception.h"
#include "ioparport.h"
#ifndef EMBEDDED
#include "iofx2.h"
#include "ioftdi.h"
#include "ioxpc.h"
#endif
#include "bitfile.h"
#include "jtag.h"
#include "devicedb.h"
#include "progalgxcf.h"
#include "progalgxcfp.h"
#include "javr.h"
#include "progalgxc3s.h"
#include "jedecfile.h"
#include "mapfile_xc2c.h"
#include "progalgxc95x.h"
#include "progalgxc2c.h"
#include "progalgavr.h"
#include "progalgspiflash.h"
#include "utilities.h"

#define IDCODE_TO_FAMILY(id)        ((id>>21) & 0x7f)
#define IDCODE_TO_MANUFACTURER(id)  ((id>>1) & 0x3ff)

#define MANUFACTURER_ATMEL          0x01f
#define MANUFACTURER_XILINX         0x049


typedef struct {
	PyObject_HEAD
	IOBase *io;
  DeviceDB *db;
  Jtag *jtag;
	/* Type-specific fields go here. */
} JTAG;

static void
JTAG_dealloc(JTAG* self)
{
	delete self->jtag;
	delete self->io;
	delete self->db;
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *
JTAG_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	JTAG *self;
	
	self = (JTAG *)type->tp_alloc(type, 0);
	if (self != NULL) {
		self->io = 0;
		self->db = new DeviceDB(0);
		self->jtag = 0;
	}

	return (PyObject *)self;
}

extern int init_chain(Jtag &jtag, DeviceDB &db);
extern unsigned long get_id(Jtag &jtag, DeviceDB &db, int chainpos);
extern int programXC3S(Jtag &g, BitFile &file, bool verify, bool reconfig, int family);

static PyObject *
JTAG_connect(JTAG *self, PyObject *args, PyObject *keywds)
{
	CABLES_TYPES cable		= CABLE_NONE;
	
	cable = CABLE_FTDI;
#ifndef EMBEDDED
	int subtype = FTDI_NO_EN;
#else
	int subtype = 0;
#endif
	int channel = 0;
	int vendor = 0;
	int product = 0;
	char *dev = 0;
	char *desc = 0;
	char *serial = 0;
	int verbose = 2;
	int chainpos = 0;
	int res;
	
	const char *kwlist[] = {"cable", "product", "vendor", NULL};
	char *s_cable;
	
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|ii", (char**)kwlist, &s_cable, &product, &vendor))
		return NULL;

#ifndef EMBEDDED	
	if (!strcmp(s_cable, "ftdi"))
		cable = CABLE_FTDI;
	else if (!strcmp(s_cable, "xpc"))
		cable = CABLE_XPC;
	else if (!strcmp(s_cable, "fx2"))
		cable = CABLE_FX2;
	else
	{
		PyErr_SetString(PyExc_TypeError, "Cable type must be {ftdi|xpc|fx2}");
		return NULL;
	}
#else
	cable = CABLE_EMBEDDED;
#endif

	std::auto_ptr<IOBase> io;
	res = getIO( &io, cable, subtype, channel, vendor, product, dev, desc, serial);
	if (res) /* some error happend*/
	{
		PyErr_SetString(PyExc_IOError, "Error connecting JTAG");
		return NULL;
	}
	
	self->io = io.release();

	self->io->setVerbose(verbose);
	
	self->jtag = new Jtag(self->io);
	self->jtag->setVerbose(verbose);
	
	int num_devices = init_chain(*self->jtag, *self->db);
	
	PyObject *result = 0;
	
	if (num_devices != 0)
	{
		result = PyList_New(num_devices);
		if (!result)
			return NULL;
		int i;
		for (i = 0; i < num_devices; ++i)
		{
			int id = get_id(*self->jtag, *self->db, chainpos);
			PyList_SetItem(result, i, PyLong_FromUnsignedLong(id));
		}
	} else
	{
		PyErr_SetString(PyExc_IOError, "Error init chain");
		return NULL;
	}
	
	detect_chain(self->jtag, self->db);
	
	if (!result)
	{
		Py_INCREF(Py_None);
		return Py_None;
	} else
	{
		return result;
	}
}

static PyObject *
JTAG_program(JTAG *self, PyObject *args)
{
	int chainpos = 0;
	int verify = 0;
	int reconfigure = 0;
	
	PyObject *file;
	
	if (!PyArg_ParseTuple(args, "O!|i", &PyFile_Type, &file, &chainpos))
		return NULL;
	
	int id = get_id(*self->jtag, *self->db, chainpos);
	
	if (id == 0)
	{
		PyErr_SetString(PyExc_IOError, "Illegal chain position");
		return NULL;
	}
	
	unsigned int family = IDCODE_TO_FAMILY(id);
  unsigned int manufacturer = IDCODE_TO_MANUFACTURER(id);

	switch (manufacturer)
	{
	case MANUFACTURER_XILINX:
		switch (family)
		{
		case FAMILY_XC3S:
		case FAMILY_XC3SE:
		case FAMILY_XC3SA:
		case FAMILY_XC3SAN:
		case FAMILY_XC3SD:
		case FAMILY_XC6S:
		case FAMILY_XCF:
		case FAMILY_XC2V:
		case FAMILY_XC2VP:
		case FAMILY_XC5VLX:
		case FAMILY_XC5VLXT:
		case FAMILY_XC5VSXT:
		case FAMILY_XC5VFXT:
		case FAMILY_XC5VTXT:
		{
			BitFile bitfile;
			FILE_STYLE in_style  = STYLE_BIT;
			bitfile.readFile(PyFile_AsFile(file), in_style);
			try 
			{
				int res = programXC3S(*self->jtag, bitfile, verify, reconfigure, family);
				printf("result is %d\n", res);
			}
			catch (io_exception &e)
			{
				PyErr_SetString(PyExc_IOError, e.getMessage().c_str());
				return NULL;
			}
			break;
		}
		default:
			PyErr_SetString(PyExc_IOError, "unknown xilinx device");
			return NULL;
		}
		break;
	default:
			PyErr_SetString(PyExc_IOError, "unknown manufacturer");
			return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
JTAG_scanuser(JTAG *self, PyObject *args)
{
	int chainpos = 0;
	unsigned char *buffer;
	Py_ssize_t size;
	if (!PyArg_ParseTuple(args, "w#|i", &buffer, &size, &chainpos))
		return NULL;

	unsigned char USER1[] = {0xC2, 0xFF};
	
	if (self->jtag->selectDevice(chainpos) != chainpos)
	{
		PyErr_SetString(PyExc_IOError, "illegal chain position");
		return NULL;
	}

	self->jtag->shiftIR(USER1);
	self->jtag->shiftDR(buffer, buffer, size * 8);

	Py_INCREF(Py_None);
	return Py_None;	
}

static PyMethodDef JTAG_methods[] = {
	{"connect", (PyCFunction)JTAG_connect, METH_VARARGS | METH_KEYWORDS,
	 "Connect JTAG"
	},
	{"program", (PyCFunction)JTAG_program, METH_VARARGS,
	"program bitstream"
	},
	{"scanuser", (PyCFunction)JTAG_scanuser, METH_VARARGS,
	"scan user chain"
	},
	{NULL}	/* Sentinel */
};

static PyTypeObject jtag_JTAGType = {
	PyObject_HEAD_INIT(NULL)
	0,						 /*ob_size*/
	"jtag.JTAG",			 /*tp_name*/
	sizeof(JTAG),	/*tp_basicsize*/
	0,						 /*tp_itemsize*/
	(destructor)JTAG_dealloc,						 /*tp_dealloc*/
	0,						 /*tp_print*/
	0,						 /*tp_getattr*/
	0,						 /*tp_setattr*/
	0,						 /*tp_compare*/
	0,						 /*tp_repr*/
	0,						 /*tp_as_number*/
	0,						 /*tp_as_sequence*/
	0,						 /*tp_as_mapping*/
	0,						 /*tp_hash */
	0,						 /*tp_call*/
	0,						 /*tp_str*/
	0,						 /*tp_getattro*/
	0,						 /*tp_setattro*/
	0,						 /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,		/*tp_flags*/
	"JTAG objects",			 /* tp_doc */
  0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	JTAG_methods,             /* tp_methods */
	0,             /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,      /* tp_init */
	0,                         /* tp_alloc */
  JTAG_new,                 /* tp_new */
};


static PyMethodDef jtag_methods[] = {
	{NULL}	/* Sentinel */
};

extern "C" {
#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initjtag(void) 
{
	PyObject* m;

	if (PyType_Ready(&jtag_JTAGType) < 0)
		return;

	m = Py_InitModule3("jtag", jtag_methods,
		"JTAG module.");

	Py_INCREF(&jtag_JTAGType);
	PyModule_AddObject(m, "JTAG", (PyObject *)&jtag_JTAGType);
}
}
