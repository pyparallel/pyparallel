# This script generates the Sound interface for Python.
# It uses the "bgen" package to generate C code.
# It execs the file sndgen.py which contain the function definitions
# (sndgen.py was generated by sndscan.py, scanning the <Sound.h> header file).

import addpack
addpack.addpack(':Tools:bgen:bgen')

from macsupport import *


# define our own function and module generators

class SndMixIn: pass

class SndFunction(SndMixIn, OSErrFunctionGenerator): pass
class SndMethod(SndMixIn, OSErrMethodGenerator): pass


# includestuff etc. are imported from macsupport

includestuff = includestuff + """
#include <Sound.h>

#ifndef HAVE_UNIVERSAL_HEADERS
#define SndCallBackUPP ProcPtr
#define NewSndCallBackProc(x) ((SndCallBackProcPtr)(x))
#define SndListHandle Handle
#endif
"""

initstuff = initstuff + """
"""


# define types used for arguments (in addition to standard and macsupport types)

class SndChannelPtrType(OpaqueByValueType):
	def declare(self, name):
		# Initializing all SndChannelPtr objects to 0 saves
		# special-casing NewSndChannel(), where it is formally an
		# input-output parameter but we treat it as output-only
		# (since Python users are not supposed to allocate memory)
		Output("SndChannelPtr %s = 0;", name)

SndChannelPtr = SndChannelPtrType('SndChannelPtr', 'SndCh')

SndCommand = OpaqueType('SndCommand', 'SndCmd')
SndCommand_ptr = OpaqueType('SndCommand', 'SndCmd')
SndListHandle = OpaqueByValueType("SndListHandle", "ResObj")

class SndCallBackType(InputOnlyType):
	def __init__(self):
		Type.__init__(self, 'PyObject*', 'O')
	def getargsCheck(self, name):
		Output("if (%s != Py_None && !PyCallable_Check(%s))", name, name)
		OutLbrace()
		Output('PyErr_SetString(PyExc_TypeError, "callback must be callable");')
		Output("goto %s__error__;", name)
		OutRbrace()
	def passInput(self, name):
		return "NewSndCallBackProc(SndCh_UserRoutine)"
	def cleanup(self, name):
		# XXX This knows it is executing inside the SndNewChannel wrapper
		Output("if (_res != NULL && %s != Py_None)", name)
		OutLbrace()
		Output("SndChannelObject *p = (SndChannelObject *)_res;")
		Output("p->ob_itself->userInfo = (long)p;")
		Output("Py_INCREF(%s);", name)
		Output("p->ob_callback = %s;", name)
		OutRbrace()
		DedentLevel()
		Output(" %s__error__: ;", name)
		IndentLevel()

SndCallBackProcPtr = SndCallBackType()
SndCallBackUPP = SndCallBackProcPtr

SndCompletionProcPtr = FakeType('(SndCompletionProcPtr)0') # XXX
SndCompletionUPP = SndCompletionProcPtr

NumVersion = OpaqueByValueType('NumVersion', 'NumVer')

InOutBuf128 = FixedInputOutputBufferType(128)

AudioSelectionPtr = FakeType('0') # XXX

ProcPtr = FakeType('0') # XXX
FilePlayCompletionUPP = FakeType('0') # XXX

SCStatus = StructOutputBufferType('SCStatus')
SMStatus = StructOutputBufferType('SMStatus')
CompressionInfo = StructOutputBufferType('CompressionInfo')

includestuff = includestuff + """
#include <OSUtils.h> /* for Set(Current)A5 */

/* Create a SndCommand object (an (int, int, int) tuple) */
static PyObject *
SndCmd_New(SndCommand *pc)
{
	return Py_BuildValue("hhl", pc->cmd, pc->param1, pc->param2);
}

/* Convert a SndCommand argument */
static int
SndCmd_Convert(PyObject *v, SndCommand *pc)
{
	int len;
	pc->param1 = 0;
	pc->param2 = 0;
	if (PyTuple_Check(v)) {
		if (PyArg_ParseTuple(v, "h|hl", &pc->cmd, &pc->param1, &pc->param2))
			return 1;
		PyErr_Clear();
		return PyArg_ParseTuple(v, "hhs#", &pc->cmd, &pc->param1, &pc->param2, &len);
	}
	return PyArg_Parse(v, "h", &pc->cmd);
}

/* Create a NumVersion object (a quintuple of integers) */
static PyObject *
NumVer_New(NumVersion nv)
{
	return Py_BuildValue("iiiii",
	                     nv.majorRev,
#ifdef THINK_C
	                     nv.minorRev,
	                     nv.bugFixRev,
#else
	                     (nv.minorAndBugRev>>4) & 0xf,
	                     nv.minorAndBugRev & 0xf,
#endif
	                     nv.stage,
	                     nv.nonRelRev);
}

static pascal void SndCh_UserRoutine(SndChannelPtr chan, SndCommand *cmd); /* Forward */
"""


finalstuff = finalstuff + """
/* Routine passed to Py_AddPendingCall -- call the Python callback */
static int
SndCh_CallCallBack(arg)
	void *arg;
{
	SndChannelObject *p = (SndChannelObject *)arg;
	PyObject *args;
	PyObject *res;
	args = Py_BuildValue("(O(hhl))",
	                     p, p->ob_cmd.cmd, p->ob_cmd.param1, p->ob_cmd.param2);
	res = PyEval_CallObject(p->ob_callback, args);
	Py_DECREF(args);
	if (res == NULL)
		return -1;
	Py_DECREF(res);
	return 0;
}

/* Routine passed to NewSndChannel -- schedule a call to SndCh_CallCallBack */
static pascal void
SndCh_UserRoutine(SndChannelPtr chan, SndCommand *cmd)
{
	SndChannelObject *p = (SndChannelObject *)(chan->userInfo);
	if (p->ob_callback != NULL) {
		long A5 = SetA5(p->ob_A5);
		p->ob_cmd = *cmd;
		Py_AddPendingCall(SndCh_CallCallBack, (void *)p);
		SetA5(A5);
	}
}
"""


# create the module and object definition and link them

class SndObjectDefinition(ObjectDefinition):

	def outputStructMembers(self):
		ObjectDefinition.outputStructMembers(self)
		Output("/* Members used to implement callbacks: */")
		Output("PyObject *ob_callback;")
		Output("long ob_A5;");
		Output("SndCommand ob_cmd;")

	def outputInitStructMembers(self):
		ObjectDefinition.outputInitStructMembers(self)
		Output("it->ob_callback = NULL;")
		Output("it->ob_A5 = SetCurrentA5();");

	def outputCleanupStructMembers(self):
		ObjectDefinition.outputCleanupStructMembers(self)
		Output("Py_XDECREF(self->ob_callback);")
	
	def outputFreeIt(self, itselfname):
		Output("SndDisposeChannel(%s, 1);", itselfname)


sndobject = SndObjectDefinition('SndChannel', 'SndCh', 'SndChannelPtr')
module = MacModule('Snd', 'Snd', includestuff, finalstuff, initstuff)
module.addobject(sndobject)


# create lists of functions and object methods

functions = []
sndmethods = []


# populate the lists

execfile('sndgen.py')


# add the functions and methods to the module and object, respectively

for f in functions: module.add(f)
for f in sndmethods: sndobject.add(f)


# generate output

SetOutputFileName('Sndmodule.c')
module.generate()
