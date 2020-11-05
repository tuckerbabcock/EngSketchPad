###################################################################
#                                                                 #
# pyOcsm --- Python version of OpenCSM API                        #
#                                                                 #
#              Written by John Dannenhoffer @ Syracuse University #
#                                                                 #
###################################################################

import ctypes
import os
import sys

# get the value of ESP_ROOT
ESP_ROOT = os.environ["ESP_ROOT"]
if ESP_ROOT == None:
    raise RuntimeError("ESP_ROOT must be set -- Please fix the environment...")

# load the shared library
if sys.platform.startswith('darwin'):
    _ocsm = ctypes.CDLL(ESP_ROOT + "/lib/libocsm.dylib")
elif sys.platform.startswith('linux'):
    _ocsm = ctypes.CDLL(ESP_ROOT + "/lib/libocsm.so")
elif sys.platform.startswith('win32'):
    _ocsm = ctypes.CDLL(ESP_ROOT + "/lib/ocsm.dll")
else:
    raise IOError("Unknown platform: " + sys.platform)

print("libocsm was loaded")

#-----------------------------------------------------------------------
# Ocsm class
#-----------------------------------------------------------------------

class Ocsm:
    """
    official Python bindings for the OpenCSM API

    written by John Dannenhoffer (author of OpenCSM)
    """

    # constants
    ACTIVE       = 300
    SUPPRESSED   = 301
    INACTIVE     = 302
    DEFERRED     = 303

    SOLID_BODY   = 400
    SHEET_BODY   = 401
    WIRE_BODY    = 402
    NODE_BODY    = 403
    NULL_BODY    = 404

    DESPMTR      = 500
    CFGPMTR      = 501
    CONPMTR      = 502
    LOCALVAR     = 503
    OUTPMTR      = 504

    NODE         = 600
    EDGE         = 601
    FACE         = 602
    BODY         = 603

    def __init__(self):
        """
        Ocsm.__init__ - create an empty MODL
        """

        self.modl = None

        return

    def Version(self):
        """
        Ocsm.Version - return current version

        inputs:
            (None)
        outputs:
            imajor      major version number
            iminor      minor version number
        """
        _ocsm.ocsmVersion.argtypes = [ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmVersion.restype  =  ctypes.c_int

        imajor = ctypes.c_int()
        iminor = ctypes.c_int()

        status = _ocsm.ocsmVersion(imajor, iminor)
        _processStatus(status, "Version")

        return (imajor.value, iminor.value)

    def SetOutLevel(self, ilevel):
        """
        Ocsm.SetOutLevel - set output level

        inputs:
            ilevel      0  errors only
                        1  nominal (default)
                        2  debug
        outputs:
            (None)
        """
        _ocsm.ocsmSetOutLevel.argtypes = [ctypes.c_int]
        _ocsm.ocsmSetOutLevel.restype  =  ctypes.c_int

        status = _ocsm.ocsmSetOutLevel(ilevel)
        _processStatus(status, "SetOutLevel")

        return

    def Load(self, filename):
        """
        Ocsm.Load - create a MODL by reading a .csm file

        inputs:
            filename    file to be read (with .csm)
        output:
            (None)
        """
        _ocsm.ocsmLoad.argtypes = [ctypes.c_char_p,
                                   ctypes.POINTER(ctypes.c_long)]
        _ocsm.ocsmLoad.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        self.modl = ctypes.c_long()
        status    = _ocsm.ocsmLoad(filename, self.modl)
        _processStatus(status, "Load")

        return

    def LoadDict(self, dictname):
        """
        Ocsm.LoadDist - load dictionary from dictname

        inputs:
            dictname    file that contains dictionary
        outputs:
            (None)
        """
        _ocsm.ocsmLoadDict.argtypes = [ctypes.c_long,
                                       ctypes.c_char_p]
        _ocsm.ocsmLoadDict.restype  =  ctypes.c_int

        if (isinstance(dictname, str)):
            dictname = dictname.encode()

        status    = _ocsm.ocsmLoadDict(self.modl, dictname)
        _processStatus(status, "LoadDict")

        return

    def UpdateDespmtrs(self, filename):
        """
        Ocsm.UpdateDespmtrs - update CFGPMTRs and DESPMTRs from filename

        inputs:
            filename    file that contains DESPMTRs
        outputs:
            (None)
        """
        _ocsm.ocsmUpdateDespmtrs.argtypes = [ctypes.c_long,
                                             ctypes.c_char_p]
        _ocsm.ocsmUpdateDespmtrs.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmUpdateDespmtrs(self.modl, filename)
        _processStatus(status, "UpdateDespmtrs")

        return

    def GetFilelist(self):
        """
        Ocsm.GetFilelist - get a list of all .csm, .cpc, and .udc files

        inputs:
            filelist    bar-separated list of files
        outputs:
            (None)
        """
        _ocsm.ocsmGetFilelist.argtypes = [ctypes.c_long,
                                          ctypes.POINTER(ctypes.c_char_p)]
        _ocsm.ocsmGetFilelist.restype  =  ctypes.c_int

        filelist = ctypes.c_char_p(b"\0"*4097)

        status = _ocsm.ocsmGetFilelist(self.modl, filelist)
        _processStatus(status, "GetFilelist")

        return filelist.value.decode("utf-8")

    def Save(self, filename):
        """
        Ocsm.Save - save a MODL to a file

        inputs:
            filename    file to be written (with extension)
        output:
            (None)
        """
        _ocsm.ocsmSave.argtypes = [ctypes.c_long,
                                   ctypes.c_char_p]
        _ocsm.ocsmSave.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmSave(self.modl, filename)
        _processStatus(status, "Save")

        return

    def SaveDespmtrs(self, filename):
        """
        Ocsm.SaveDespmtrs - save CFGPMTRs and DESPMTRs to file

        inputs:
            filename    file to be written
        outputs:
            (None)
        """
        _ocsm.ocsmSaveDespmtrs.argtypes = [ctypes.c_long,
                                           ctypes.c_char_p]
        _ocsm.ocsmSaveDespmtrs.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmSaveDespmtrs(self.modl, filename)
        _processStatus(status, "SaveDespmtrs")

        return

    def Copy(self):
        """
        Ocsm.Copy - copy a MODL (but not the Bodys)

        inputs:
            (None)
        outputs:
            (None)
        side effect:
            a new Ocsm MODL is created
        """
        _ocsm.ocsmCopy.argtypes = [ctypes.c_long,
                                   ctypes.POINTER(ctypes.c_long)]
        _ocsm.ocsmCopy.restype  =  ctypes.c_int

        new = ctypes.c_long()
        status = _ocsm.ocsmCopy(self.modl, new)
        _processStatus(status, "Copy")

        newModl = Ocsm()
        newModl.modl = new

        return newModl

    def Free(self):
        """
        Ocsm.Free - free up all strorage associated with a MODL

        inputs:
            (None)
        outputs:
            (None)
        """
        _ocsm.ocsmFree.argtypes = [ctypes.c_long]
        _ocsm.ocsmFree.restype  =  ctypes.c_int

        status = _ocsm.ocsmFree(self.modl)
        _processStatus(status, "Free")

        self.modl = 0

        return

    def Info(self):
        """
        Ocsm.Info - get info about a MODL

        inputs:
            (None)
        outputs:
            nbrch       number of Branches
            npmtr       number of Parameters
            nbody       number of Bodys
        """
        _ocsm.ocsmInfo.argtypes = [ctypes.c_long,
                                   ctypes.POINTER(ctypes.c_int),
                                   ctypes.POINTER(ctypes.c_int),
                                   ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmInfo.restype  =  ctypes.c_int

        nbrch  = ctypes.c_int()
        npmtr  = ctypes.c_int()
        nbody  = ctypes.c_int()
        status = _ocsm.ocsmInfo(self.modl, nbrch, npmtr, nbody)
        _processStatus(status, "Info")

        return (nbrch.value, npmtr.value, nbody.value)

    def Check(self):
        """
        Ocsm.Check - check that Branches are properly ordered

        inputs:
            (None)
        outputs:
            (None)
        """
        _ocsm.ocsmCheck.argtypes = [ctypes.c_long]
        _ocsm.ocsmCheck.restype  =  ctypes.c_int

        status = _ocsm.ocsmCheck(self.modl)
        _processStatus(status, "Check")

        return

    def RegMesgCB(self, callback):
        """
        Ocsm.RegMesgCB - register a callback function for exporting messages

        not implemented
        """

        _processStatus(-298, "RegMesgCB")

        return

    def RegSizeCB(self, callback):
        """
        Ocsm.RegSizeCB - register a callback function for DESPMTR size changes

        not implemented
        """

        _processStatus(-298, "RegSizeCB")

        return

    def Build(self, buildTo, nbody):
        """
        Ocsm.Build - build Bodys by executing the MODL up to a given Branch

        inputs:
            buildTo     last Branch to execute (or 0 for all, or -1 for no recycling)
            nbody       number of entres allowed in body
        outputs:
            builtTo     last Branch executed successfully
            nbody       number of Bodys on the stack
            body        array of Bodys on the stack
        """
        _ocsm.ocsmBuild.argtypes = [ctypes.c_long,
                                    ctypes.c_int,
                                    ctypes.POINTER(ctypes.c_int),
                                    ctypes.POINTER(ctypes.c_int),
                                    ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmBuild.restype  =  ctypes.c_int

        builtTo = ctypes.c_int()
        nbody_  = ctypes.c_int(nbody)

        if (nbody == 0):
            status = _ocsm.ocsmBuild(self.modl, buildTo, builtTo, nbody_, None)
            _processStatus(status, "Build")

            return (builtTo.value, nbody_.value, None)
        else:
            body   = (ctypes.c_int * nbody)()
            status = _ocsm.ocsmBuild(self.modl, buildTo, builtTo, nbody_, body)
            _processStatus(status, "Build")

            return (builtTo.value, nbody_.value, list(body[0:nbody_.value]))

    def Perturb(self, npmtrs, ipmtrs, irows, icols, values):
        """
        Ocsm.Perturb - create a perturbed MODL

        inputs:
            npmtrs   number of perturbed Parameters (or 0 to remove)
            ipmtrs   list of Parameter indices (1:npmtr)
            irows    list of row       indices (1:nrow)
            icols    list of column    indices (1:ncol)
            values   list of perturbed values
        outputs:
            (None)
        """
        _ocsm.ocsmPerturb.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmPerturb.restype  =  ctypes.c_int

        if (npmtrs > 0):
            ipmtrs_ = (ctypes.c_int    * (npmtrs))(*ipmtrs)
            irows_  = (ctypes.c_int    * (npmtrs))(*irows )
            icols_  = (ctypes.c_int    * (npmtrs))(*icols )
            values_ = (ctypes.c_double * (npmtrs))(*values)

            status = _ocsm.ocsmPerturb(self.modl, npmtrs, ipmtrs_, irows_, icols_, values_)
            _processStatus(status, "Perturb")
        else:
            status = _ocsm.ocsmPerturb(self.modl, 0, None, None, None, None)
            _processStatus(status, "Perturb")

        return

    def UpdateTess(self, ibody, filename):
        """
        Ocsm.UpdateTess - update a tessellation from a file

        inputs:
            filename    name of .tess file
        outputs:
            (None)
        """
        _ocsm.ocsmUpdateTess.argtypes = [ctypes.c_long,
                                         ctypes.c_int,
                                         ctypes.c_char_p]
        _ocsm.ocsmUpdateTess.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmUpdateTess(self.modl, ibody, filename)
        _processStatus(status, "UpdateTess")

        return

    def NewBrch(self, iafter, type, filename, linenum, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9):
        """
        Ocsm.NewBrch - create a new Branch

        inputs:
            iafter      Branch index (0-nbrch) after which to add
            type        Branch type
            filename    filename when Branch is defined
            linenum     linenumber where Branch is defined (bias-1)
            arg1        Argument 1
            arg2        Argument 2
            arg3        Argument 3
            arg4        Argument 4
            arg5        Argument 5
            arg6        Argument 6
            arg7        Argument 7
            arg8        Argument 8
            arg9        Argument 9
        outputs:
            (None)
        """
        _ocsm.ocsmNewBrch.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p]
        _ocsm.ocsmNewBrch.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()
        if (isinstance(arg1, str)):
            arg1 = arg1.encode()
        if (isinstance(arg2, str)):
            arg2 = arg2.encode()
        if (isinstance(arg3, str)):
            arg3 = arg3.encode()
        if (isinstance(arg4, str)):
            arg4 = arg4.encode()
        if (isinstance(arg5, str)):
            arg5 = arg5.encode()
        if (isinstance(arg6, str)):
            arg6 = arg6.encode()
        if (isinstance(arg7, str)):
            arg7 = arg7.encode()
        if (isinstance(arg8, str)):
            arg8 = arg8.encode()
        if (isinstance(arg9, str)):
            arg9 = arg9.encode()

        status = _ocsm.ocsmNewBrch(self.modl, iafter, type, filename, linenum, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
        _processStatus(status, "NewBrch")

        return

    def GetBrch(self, ibrch):
        """
        Ocsm.GetBrch - get info about a Branch

        inputs:
            ibrch       Branch index (1:nbrch)
        outputs:
            type        Branch type
            bclass      Branch class
            actv        Branch activity
            ichld       ibrch of child (or 0 if root)
            ileft       ibrch of left parent (or 0)
            irite       ibrch of rite parent (or 0)
            narg        number of Arguments
            nattr       number of Attributes
        """
        _ocsm.ocsmGetBrch.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmGetBrch.restype  =  ctypes.c_int

        type   = ctypes.c_int()
        bclass = ctypes.c_int()
        actv   = ctypes.c_int()
        ichld  = ctypes.c_int()
        ileft  = ctypes.c_int()
        irite  = ctypes.c_int()
        narg   = ctypes.c_int()
        nattr  = ctypes.c_int()

        status = _ocsm.ocsmGetBrch(self.modl, ibrch, type, bclass, actv, ichld, ileft, irite, narg, nattr)
        _processStatus(status, "GetBrch")

        return (type.value, bclass.value, actv.value, ichld.value, ileft.value, irite.value, narg.value, nattr.value)

    def SetBrch(self, ibrch, actv):
        """
        Ocsm.SetBrch - set activity for a Branch

        inputs:
            ibrch       Branch index (1:nbrch)
            actv        Branch activity
        outputs:
            (none)
        """
        _ocsm.ocsmSetBrch.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int]
        _ocsm.ocsmSetBrch.restype  =  ctypes.c_int

        status = _ocsm.ocsmSetBrch(self.modl, ibrch, actv)
        _processStatus(status, "SetBrch")

        return

    def DelBrch(self,ibrch):
        """
        Ocsm.DelBrch - delete a Branch (or whole Sketch if SKBEG)

        inputs:
            ibrch       Branch index (1:nbrch)
        outputs:
            (None)
        """
        _ocsm.ocsmDelBrch.argtypes = [ctypes.c_long,
                                      ctypes.c_int]
        _ocsm.ocsmDelBrch.restype  =  ctypes.c_int

        status = _ocsm.ocsmDelBrch(self.modl, ibrch)
        _processStatus(status, "DelBrch")

        return

    def PrintBrchs(self, filename):
        """
        Ocsm.PrintBrchs - print Branches to a file

        inputs:
            filename    file to which output is appended (or "" for stdout)
        outputs:
            (None)
        """
        _ocsm.ocsmPrintBrchs.argtypes = [ctypes.c_long,
                                         ctypes.c_char_p]
        _ocsm.ocsmPrintBrchs.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmPrintBrchs(self.modl, filename)
        _processStatus(status, "PrintBrchs")

        return

    def GetArg(self, ibrch, iarg):
        """
        Ocsm.GetArg - get an Argument for a Branch

        inputs:
            ibrch       Branch   index (1:nbrch)
            iarg        Argument index (1:narg)
        outputs:
            defn        Argument definition
            value       Argument value
            dot         Arfument velocity
        """
        _ocsm.ocsmGetArg.argtypes = [ctypes.c_long,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_char_p,
                                     ctypes.POINTER(ctypes.c_double),
                                     ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetArg.restype  =  ctypes.c_int

        defn  = ctypes.create_string_buffer(257)
        value = ctypes.c_double()
        dot   = ctypes.c_double()

        status = _ocsm.ocsmGetArg(self.modl, ibrch, iarg, defn, value, dot)
        _processStatus(status, "GetArg")

        return (defn.value.decode("utf-8"), value.value, dot.value)

    def SetArg(self, ibrch, iarg, defn):
        """
        Ocsm.SetArg - set an Argument for a Branch

        inputs:
            ibrch       Branch   index (1:nbrch)
            iarg        Argument index (1:narg)
            defn        Argument definition
        outputs:
            (None)
        """
        _ocsm.ocsmSetArg.argtypes = [ctypes.c_long,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_char_p]
        _ocsm.ocsmSetArg.restype  =  ctypes.c_int

        if (isinstance(defn, str)):
            defn = defn.encode()

        status = _ocsm.ocsmSetArg(self.modl, ibrch, iarg, defn)
        _processStatus(status, "SetArg")

        return

    def RetAttr(self, ibrch, iattr):
        """
        Ocsm.RetAttr - return an Attribute for a Branch by index

        inputs:
            ibrch       Branch    index (1:nbrch)
            iattr       Attribute index (1:nbrch)
        outputs:
            aname       Attribute name
            avalue      Attribute value
        """
        _ocsm.ocsmRetAttr.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p]
        _ocsm.ocsmRetAttr.restype  =  ctypes.c_int

        aname  = ctypes.create_string_buffer(257)
        avalue = ctypes.create_string_buffer(257)

        status = _ocsm.ocsmRetAttr(self.modl, ibrch, iattr, aname, avalue)
        _processStatus(status, "RetAttr")

        return (aname.value.decode("utf-8"), avalue.value.decode("utf-8"))

    def GetAttr(self, ibrch, aname):
        """
        Ocsm.RetAttr - return an Attribute for a Branch by name

        inputs:
            ibrch       Branch    index (1:nbrch)
            aname       Attribute name
        outputs:
            avalue      Attribute value
        """
        _ocsm.ocsmGetAttr.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p]
        _ocsm.ocsmGetAttr.restype  =  ctypes.c_int

        if (isinstance(aname, str)):
            aname = aname.encode()

        avalue = ctypes.create_string_buffer(257)

        status = _ocsm.ocsmGetAttr(self.modl, ibrch, aname, avalue)
        _processStatus(status, "GetAttr")

        return avalue.value.decode("utf-8")

    def SetAttr(self, ibrch, aname, avalue):
        """
        Ocsm.SetAttr - set an Attribute for a Branch

        inputs:
            ibrch       Branch index (1:nbrch) or 0 for global
            aname       Attribute name
            avalue      Attribute value (or blank to delete)
        outputs:
            (None)
        """
        _ocsm.ocsmSetAttr.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p]
        _ocsm.ocsmSetAttr.restype  =  ctypes.c_int

        if (isinstance(aname, str)):
            aname = aname.encode()
        if (isinstance(avalue, str)):
            avalue = avalue.encode()

        status = _ocsm.ocsmSetAttr(self.modl, ibrch, aname, avalue)
        _processStatus(status, "SetAttr")

        return

    def RetCsys(self, ibrch, icsys):
        """
        Ocsm.RetCsys - return a Csystem for a Branch by index

        inputs:
            ibrch       Branch  index (1:nbrch)
            icsys       Csystem index (1:nattr)
        outputs:
            cname       Csystem name
            cvalue      Csystem value
        """
        _ocsm.ocsmRetCsys.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p]
        _ocsm.ocsmRetCsys.restype  =  ctypes.c_int

        cname  = ctypes.create_string_buffer(257)
        cvalue = ctypes.create_string_buffer(257)

        status = _ocsm.ocsmRetCsys(self.modl, ibrch, icsys, cname, cvalue)
        _processStatus(status, "RetCsys")

        return (cname.value.decode("utf-8"), cvalue.value.decode("utf-8"))

    def GetCsys(self, ibrch, cname):
        """
        Ocsm.GetCsys - return a Csystem for a Branch by name

        inputs:
            ibrch       Branch index  (1:nbrch)
            cname       Csystem name
        outputs:
            cvalue      Csystem value
        """
        _ocsm.ocsmGetCsys.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p]
        _ocsm.ocsmGetCsys.restype  =  ctypes.c_int

        if (isinstance(cname, str)):
            cname = cname.encode()

        cvalue = ctypes.create_string_buffer(257)

        status = _ocsm.ocsmGetCsys(self.modl, ibrch, cname, cvalue)
        _processStatus(status, "GetCsys")

        return cvalue.value.decode("utf-8")

    def SetCsys(self, ibrch, cname, cvalue):
        """
        Ocsm.SetCsys - set a Csystem for a Branch

        inputs:
            ibrch       Branch index (1:nbrch)
            cname       Csystem name
            cvalue      Csystem value
        outputs:
            (None)
        """
        _ocsm.ocsmSetCsys.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_char_p,
                                      ctypes.c_char_p]
        _ocsm.ocsmSetCsys.restype  =  ctypes.c_int

        if (isinstance(cname, str)):
            cname = cname.encode()
        if (isinstance(cvalue, str)):
            cvalue = cvalue.encode()

        status = _ocsm.ocsmSetCsys(self.modl, ibrch, cname, cvalue)
        _processStatus(status, "SetCsys")

        return

    def PrintAttrs(self, filename):
        """
        Ocsm.PrintAttrs - print global Attributes to file

        inputs:
            filename    file to which output is appended (or "" for stdout)
        outputs:
            (None)
        """
        _ocsm.ocsmPrintAttrs.argtypes = [ctypes.c_long,
                                         ctypes.c_char_p]
        _ocsm.ocsmPrintAttrs.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmPrintAttrs(self.modl, filename)
        _processStatus(status, "PrintAttrs")

        return

    def GetName(self, ibrch):
        """
        Ocsm.GetName - get the name of a Branch

        inputs:
            ibrch       Branch index (1:nbrch)
        outputs:
            name        Brchch name
        """
        _ocsm.ocsmGetName.argtypes = [ctypes.c_long,
                                      ctypes.c_int]
        _ocsm.ocsmGetName.restype  =  ctypes.c_int

        name = ctypes.create_string_buffer(257)

        status = _ocsm.ocsmGetName(self.modl, ibrch, name)
        _processStatus(status, "GetName")

        return name.value.decode("utf-8")

    def SetName(self, ibrch, name):
        """
        Ocsm.SetName - set the name for a Branch

        inputs:
            ibrch       Branch index (1:nbrch)
            name        Branch name
        outputs:
            (None)
        """
        _ocsm.ocsmSetName.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_char_p]
        _ocsm.ocsmSetName.restype  =  ctypes.c_int

        if (isinstance(name, str)):
            name = name.encode()

        status = _ocsm.ocsmSetName(self.modl, ibrch, name)
        _processStatus(status, "SetName")

        return

    def GetSketch(self, ibrch, maxlen):
        """
        Ocsm.GetSketch - get string data associated with a Sketch

        not implemented
        """
        _ocsm.ocsmGetSketch.argtypes = [ctypes.c_long,
                                        ctypes.c_int,
                                        ctypes.c_int,
                                        ctypes.c_char_p,
                                        ctypes.c_char_p,
                                        ctypes.c_char_p,
                                        ctypes.c_char_p]
        _ocsm.ocsmGetSketch.restype  =  ctypes.c_int

        _processStatus(-298, "GetSketch")

        return # (begs, vars, cons, segs)

    def SolveSketch(self, vars_in, cons, vars_out):
        """
        Ocsm.SolveSketch - solve for new Sketch variables

        not implemented
        """
        _ocsm.ocsmSolveSketch.argtypes = [ctypes.c_long,
                                          ctypes.c_char_p,
                                          ctypes.c_char_p,
                                          ctypes.c_char_p]
        _ocsm.ocsmSolveSketch.restype  =  ctypes.c_int

        _processStatus(-298, "SolveSketch")

        return # vars_out

    def SaveSketch(self, ibrch, vars, cons, segs):
        """
        Ocsm.SaveSketch - overwrite Branches associated with a Sketch

        not implemented
        """
        _ocsm.ocsmSaveSketch.argtypes = [ctypes.c_long,
                                         ctypes.c_int,
                                         ctypes.c_char_p,
                                         ctypes.c_char_p,
                                         ctypes.c_char_p]
        _ocsm.ocsmSaveSketch.restype  =  ctypes.c_int

        _processStatus(-298, "SaveSketch")

        return

    def NewPmtr(self, name, type, nrow, ncol):
        """
        Ocsm.NewPmtr - create a new Parameter

        inputs:
            name        Parameter name
            type        Ocsm.DESPMTR
                        Ocsm.CFGPMTR
                        Ocsm.CONPMTR
                        Ocsm.LOCALVAR
                        Ocsm.OUTPMTR
            nrow        number of rows
            ncol        number of columns
        outputs:
            (None)
        """
        _ocsm.ocsmNewPmtr.argtypes = [ctypes.c_long,
                                      ctypes.c_char_p,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_int]
        _ocsm.ocsmNewPmtr.restype = ctypes.c_int

        if (isinstance(name, str)):
            name = name.encode()

        ipmtr  = ctypes.c_int()
        status = _ocsm.ocsmNewPmtr(self.modl, name, type, nrow, ncol)
        _processStatus(status, "NewPmtr")

        return

    def DelPmtr(self, ipmtr):
        """
        Ocsm.DelPmtr = delete a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
        outputs:
            (None)
        """
        _ocsm.ocsmDelPmtr.argtypes = [ctypes.c_long,
                                      ctypes.c_int]
        _ocsm.ocsmDelPmtr.restype  =  ctypes.c_int

        status = _ocsm.ocsmDelPmtr(self.modl, ipmtr)
        _processStatus(status, "DelPmtr")

        return

    def FindPmtr(self, name, type, nrow, ncol):
        """
        Ocsm.FindPmtr - find (or create) a Parameter

        inputs:
            name        Parameter name
            type        Ocsm.DESPMTR
                        Ocsm.CFGPMTR
                        Ocsm.CONPMTR
                        Ocsm.LOCALVAR
                        Ocsm.OUTPMTR
            nrow        number of rows
            ncol        number of columns
        outputs:
            (None)
        """
        _ocsm.ocsmFindPmtr.argtypes = [ctypes.c_long,
                                       ctypes.c_char_p,
                                       ctypes.c_int,
                                       ctypes.c_int,
                                       ctypes.c_int,
                                       ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmFindPmtr.restype = ctypes.c_int

        if (isinstance(name, str)):
            name = name.encode()

        ipmtr  = ctypes.c_int()
        status = _ocsm.ocsmFindPmtr(self.modl, name, type, nrow, ncol, ipmtr)
        _processStatus(status, "FindPmtr")

        return ipmtr.value

    def GetPmtr(self, ipmtr):
        """
        Ocsm.GetPmtr - get info about a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
        outputs:
            type        Ocsm.DESPMTR, Ocsm.CFGPMTR, Ocsm.CONPMTR,
                            Ocsm.LOCALVAL, or Ocsm.OUTPMTR
            nrow        number of rows
            ncol        number of columns
            name        Parameter name
        """
        _ocsm.ocsmGetPmtr.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.c_char_p]
        _ocsm.ocsmGetPmtr.restype  =  ctypes.c_int

        type  = ctypes.c_int()
        nrow  = ctypes.c_int()
        ncol  = ctypes.c_int()
        name  = ctypes.c_char_p(b"\0"*33)
        status = _ocsm.ocsmGetPmtr(self.modl, ipmtr, type, nrow, ncol, name)
        _processStatus(status, "GetPmtr")

        return (type.value, nrow.value, ncol.value, name.value.decode("utf-8"))

    def PrintPmtrs(self, filename):
        """
        Ocsm.PrintPmtrs - print DESPMTRs and OUTPMTRs to file

        inputs:
            filename    file to which output is appended (or "" for stdout)
        outputs:
            (None)
        """
        _ocsm.ocsmPrintPmtrs.argtypes = [ctypes.c_long,
                                         ctypes.c_char_p]
        _ocsm.ocsmPrintPmtrs.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmPrintPmtrs(self.modl, filename)
        _processStatus(status, "PrintPmtrs")

        return

    def GetValu(self, ipmtr, irow, icol):
        """
        Ocsm.GetValue - get the Value of a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
            irow        row       index (1:nrow)
            icol        column    index (1:ncol)
        outputs:
            value       Parameter Value
            dot         Parameter velocity
        """
        _ocsm.ocsmGetValu.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_double),
                                      ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetValu.restype  =  ctypes.c_int

        value  = ctypes.c_double()
        dot    = ctypes.c_double()
        status = _ocsm.ocsmGetValu(self.modl, ipmtr, irow, icol, value, dot)
        _processStatus(status, "GetValu")

        return (value.value, dot.value)

    def GetValuS(self, ipmtr):
        """
        Ocsm.GetValue - get the Value of a string Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
        outputs:
            str         Parameter Value
        """
        _ocsm.ocsmGetValuS.argtypes = [ctypes.c_long,
                                       ctypes.c_int,
                                       ctypes.c_char_p]
        _ocsm.ocsmGetValuS.restype  =  ctypes.c_int

        str = ctypes.create_string_buffer(257)

        status = _ocsm.ocsmGetValuS(self.modl, ipmtr, str)
        _processStatus(status, "GetValuS")

        return str.value.decode("utf-8")

    def SetValu(self, ipmtr, irow, icol, defn):
        """
        Ocsm.SetValu - set a Value for a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
            irow        row       index (1:nrow)
            icol        column    indes (1:ncol)
            defn        definieiotn of Value
        outputs:
            (None)
        """
        _ocsm.ocsmSetValu.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_char_p]
        _ocsm.ocsmSetValu.restype  =  ctypes.c_int

        if (isinstance(defn, str)):
            defn = defn.encode()

        status = _ocsm.ocsmSetValu(self.modl, ipmtr, irow, icol, defn)
        _processStatus(status, "SetValu")

        return

    def SetValuD(self, ipmtr, irow, icol, value):
        """
        Ocsm.SetValuD - set the (double) value of a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
            irow        row       index (1:nrow)
            icol        column    index (1:ncol)
            value       value to set
        outputs:
            (None)
        """
        _ocsm.ocsmSetValuD.argtypes = [ctypes.c_long,
                                       ctypes.c_int,
                                       ctypes.c_int,
                                       ctypes.c_int,
                                       ctypes.c_double]
        _ocsm.ocsmSetValuD.restype  =  ctypes.c_int

        status = _ocsm.ocsmSetValuD(self.modl, ipmtr, irow, icol, value)
        _processStatus(status, "SetValuD")

        return

    def GetBnds(self, ipmtr, irow, icol):
        """
        Ocsm.GetBnds - get the Bounds of a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
            irow        row       index (1:nrow)
            icol        column    index (1:ncol)
        outputs:
            lbound      lower Bound
            ubound      upper Bound
        """
        _ocsm.ocsmGetBnds.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_double),
                                      ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetBnds.restype  =  ctypes.c_int

        lbound = ctypes.c_double()
        ubound = ctypes.c_double()

        status = _ocsm.ocsmGetBnds(self.modl, ipmtr, irow, icol, lbound, ubound)
        _processStatus(status, "GetBnds")

        return (lbound.value, ubound.value)

    def SetBnds(self, ipmtr, irow, icol, lbound, ubound):
        """
        Ocsm.SetBnds - set teh Bounds of a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
            irow        row       index (1:nrow)
            icol        column    index (1:ncol)
            lbound      lower Bound
            ubound      upper Bound
        outputs:
            (None)
        """
        _ocsm.ocsmSetBnds.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_double,
                                      ctypes.c_double]
        _ocsm.ocsmSetBnds.restype  =  ctypes.c_int

        status = _ocsm.ocsmSetBnds(self.modl, ipmtr, irow, icol, lbound, ubound)
        _processStatus(status, "SetBnds")

        return

    def SetDtime(self, dtime):
        """
        Ocsm.SetDtime - set sensitivity FD time step (or select analytic)

        inputs:
            dtime       time step (or 0 to choose analytic)
        outputs:
            (None)
        """
        _ocsm.ocsmSetDtime.argtypes = [ctypes.c_long,
                                       ctypes.c_double]
        _ocsm.ocsmSetDtime.restype  =  ctypes.c_int

        status = _ocsm.ocsmSetDtime(self.modl, dtime)
        _processStatus(status, "SetDtime")

        return

    def SetVel(self, ipmtr, irow, icol, defn):
        """
        Ocsm.SetVel - set the velocity for a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
            irow        row index       (1:nrow)
            icol        column index    (1:ncol)
            defn        definition of Velocity
        outputs:
            (None)
        """
        _ocsm.ocsmSetVel.argtypes = [ctypes.c_long,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_char_p]
        _ocsm.ocsmSetVel.restype  =  ctypes.c_int

        if (isinstance(defn, str)):
            defn = defn.encode()

        status = _ocsm.ocsmSetVel(self.modl, ipmtr, irow, icol, defn)
        _processStatus(status, "SetVel")

        return

    def SetVelD(self, ipmtr, irow, icol, dot):
        """
        Ocsm.SetVelD - set the (double) velocity for a Parameter

        inputs:
            ipmtr       Parameter index (1:npmtr)
            irow        row index       (1:nrow)
            icol        column index    (1:ncol)
            dot         velocity to set
        outputs:
            (None)
        """
        _ocsm.ocsmSetVelD.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_double]
        _ocsm.ocsmSetVelD.restype  =  ctypes.c_int

        status = _ocsm.ocsmSetVelD(self.modl, ipmtr, irow, icol, dot)
        _processStatus(status, "SetVelD")

        return

    def GetUV(self, ibody, seltype, iselect, npnt, xyz):
        """
        Ocsm.GetUV - get the parametric coordinates on an Edge or Face

        inputs:
            ibody       Body index (1:nbody)
            seltype     Ocsm.EDGE or Ocsm.FACE
            iselect     iedge or iface
            npnt        number of points
            xyz         x[0], y[0], z[0], x[1], ... z[npnt-1]
        outputs:
            uv          u[0], v[0], u[1], ... v[npnt-1]
        """
        _ocsm.ocsmGetUV.argtypes = [ctypes.c_long,
                                    ctypes.c_int,
                                    ctypes.c_int,
                                    ctypes.c_int,
                                    ctypes.c_int,
                                    ctypes.POINTER(ctypes.c_double),
                                    ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetUV.restype  =  ctypes.c_int

        xyz_ = (ctypes.c_double * (3*npnt))(*xyz)
        uv   = (ctypes.c_double * (2*npnt))()

        status = _ocsm.ocsmGetUV(self.modl, ibody, seltype, iselect, npnt, xyz_, uv)
        _processStatus(status, "GetUV")

        if (iselect == self.EDGE):
            return list(uv[0:npnt])
        else:
            return list(uv[0:2*npnt])

    def GetEnt(self, ibody, seltype, entID):
        """
        Ocsm.GetEnt - get entity number given faceID or edgeID */

        inputs:
            ibody       Body index (1:nbody)
            seltype     Ocsm.EDGE or Ocsm.FACE
            entID       edgeID or faceID
        outputs:
            ient        entity number
        """
        _ocsm.ocsmGetEnt.argtypes = [ctypes.c_long,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_int),
                                     ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmGetEnt.restype  =  ctypes.c_int

        entID_ = (ctypes.c_int * 5)(*entID)
        ient   =  ctypes.c_int()

        status = _ocsm.ocsmGetEnt(self.modl, ibody, seltype, entID_, ient)
        _processStatus(status, "GetEnt")

        return ient.value

    def GetXYZ(self, ibody, seltype, iselect, npnt, uv):
        """
        Ocsm.GetXYZ - get the coordinates of a Node, Edge, or Face

        inputs:
            ibody       Body index (1:nbody)
            seltype     Ocsm.NODE, Ocsm.EDGE, or Ocsm.FACE
            iselect     inode, iedge, or iface
            npnt        number of points
            uv          u[0], v[0], u[1], ... v[npnt-1]
        outputs:
            xyz         x[0], y[0], z[0], x[1], ... z[npnt-1]
        """
        _ocsm.ocsmGetXYZ.argtypes = [ctypes.c_long,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_double),
                                     ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetXYZ.restype  =  ctypes.c_int

        if   (seltype == self.NODE):
            npnt = 1
            uv   =  ctypes.c_double
        elif (seltype == self.EDGE):
            uv_  = (ctypes.c_double * (  npnt))(*uv)
        else:
            uv_  = (ctypes.c_double * (2*npnt))(*uv)
        xyz = (ctypes.c_double * (3*npnt))()

        status = _ocsm.ocsmGetXYZ(self.modl, ibody, seltype, iselect, npnt, uv_, xyz)
        _processStatus(status, "GetXYZ")

        return list(xyz[0:3*npnt])

    def GetNorm(self, ibody, iface, npnt, uv):
        """
        Ocsm.GetNorm - get the unit normals for a Face

        inputs:
            ibody       Body index (1:nbody)
            iface       Face index (1:nface)
            npnt        number of points
            uv          u[0], v[0], u[1], ... v[npnt-1]
        outputs:
            norm        normx[0], normy[0], normz[0], normx[1], ... normz[npnt-1]
        """
        _ocsm.ocsmGetNorm.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_double),
                                      ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetNorm.restype  =  ctypes.c_int

        uv_  = (ctypes.c_double * (2*npnt))(*uv)
        norm = (ctypes.c_double * (3*npnt))()

        status = _ocsm.ocsmGetNorm(self.modl, ibody, iface, npnt, uv_, norm)
        _processStatus(status, "GetNorm")

        return list(norm[0:3*npnt])

    def GetVel(self, ibody, seltype, iselect, npnt, uv):
        """
        Ocsm.GetVel - get the velocities on a Node, Edge, or Face

        inputs:
            ibody       Body index (1:nbody)
            seltype     Ocsm.NODE, Ocsm.EDGE, or Ocsm.FACE
            iselect     inode, iedge, or iface
            npnt        number of points
            uv          u[0], v[0], u[1], ... v[npnt-1]
        outputs:
            vel         dx[0], dy[0], dz[0], dx[1], ... dz[npnt-1]
        """
        _ocsm.ocsmGetXYZ.argtypes = [ctypes.c_long,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_double),
                                     ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetXYZ.restype  =  ctypes.c_int

        if   (seltype == self.NODE):
            npnt = 1
            uv_  = None
        elif (seltype == self.EDGE):
            uv_  = (ctypes.c_double * (  npnt))(*uv)
        else:
            uv_  = (ctypes.c_double * (2*npnt))(*uv)
        vel = (ctypes.c_double * (3*npnt))()

        status = _ocsm.ocsmGetVel(self.modl, ibody, seltype, iselect, npnt, uv_, vel)
        _processStatus(status, "GetVel")

        return list(vel[0:3*npnt])

    def SetEgg(self, eggname):
        """
        Ocsm.SetEgg - set up alternative tessellation by an external grid generator

        not implemented
        """

        return

    def GetTessNpnt(self, ibody, seltype, iselect):
        """
        Ocsm.GetTessNpnt - get the number of tess points in an Edge or Face

        inputs:
            ibody       Body index (1:nbody)
            seltype     Ocsm.EDGE or Ocsm.FACE
            iselect     Edge or Face index (bias-1)
        outputs:
            npnt        number of points
        """
        _ocsm.ocsmGetTessNpnt.argtypes = [ctypes.c_long,
                                          ctypes.c_int,
                                          ctypes.c_int,
                                          ctypes.c_int,
                                          ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmGetTessNpnt.restype  =  ctypes.c_int

        npnt = ctypes.c_int()

        status = _ocsm.ocsmGetTessNpnt(self.modl, ibody, seltype, iselect, npnt)
        _processStatus(status, "GetTessNpnt")

        return npnt.value

    def GetTessVel(self, ibody, seltype, iselect):
        """
        Ocsm.GetTessVel - get the tessellation velocities on a Node, Edge, or Face

        inputs:
            ibody       Body index (1:nbody)
            seltype     Ocsm.NODE, Ocsm.EDGE, or Ocsm.FACE
            iselect     Node, Edge, or Face index (bias-1)
        outputs:
            dxyz        velocities
        """
        _ocsm.ocsmGetTessVel.argtypes = [ctypes.c_long,
                                         ctypes.c_int,
                                         ctypes.c_int,
                                         ctypes.POINTER(ctypes.c_double)]
        _ocsm.ocsmGetTessVel.restype  =  ctypes.int

        if   (seltype == Ocsm.NODE):
            pass
        elif (seltype == Ocsm.EDGE):
            npnt = self.GetTessNpnt(ibody, seltype, iselect)
            dxyz = ctypes.c_double * (3 * npnt)
        elif (seltype == Ocsm.FACE):
            npnt = self.GetTessNpnt(ibody, seltype, iselect)
            dxyz = ctypes.c_double * (3 * npnt)
        else:
            dxyz = ctypes.c_double()

        status = _ocsm.ocsmGetTessVel(self.modl, ibody, seltype, iselect, dxyz)
        _processStatus(status, "GetTessVel")

        return list(dxyz[0:3*npnt])

    def GetBody(self, ibody):
        """
        Ocsm.GetBody - get info about a Body

        inputs:
            ibody       Body index (1:nbody)
        outputs:
            type        Branch type
            ichild      ibody of child (or 0 if root)
            ileft       ibody of left parent (or 0)
            irite       ibody of rite parent (or 0)
            vals        array of Argument values
            nnode       number of Nodes
            nedge       number of Edges
            nface       number of Faces
        """
        _ocsm.ocsmGetBody.argtypes = [ctypes.c_long,
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_double),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_int)]
        _ocsm.ocsmGetBody.restype  =  ctypes.c_int

        type  = ctypes.c_int()
        ichld = ctypes.c_int()
        ileft = ctypes.c_int()
        irite = ctypes.c_int()
        vals  = (ctypes.c_double * 9)()
        nnode = ctypes.c_int()
        nedge = ctypes.c_int()
        nface = ctypes.c_int()

        status = _ocsm.ocsmGetBody(self.modl, ibody, type, ichld, ileft, irite, vals, nnode, nedge, nface)
        _processStatus(status, "GetBody")

        outvals = list(vals[0:9])

        return (type.value, ichld.value, ileft.value, irite.value, outvals, nnode.value, nedge.value, nface.value)

    def PrintBodys(self, filename):
        """
        Ocsm.PrintBodys - print all Bodys to file

        inputs:
            filename    file to which output is appended (or "" for stdout)
        outputs:
            (None)
        """
        _ocsm.ocsmPrintBodys.argtypes = [ctypes.c_long,
                                         ctypes.c_char_p]
        _ocsm.ocsmPrintBodys.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmPrintBodys(self.modl, filename)
        _processStatus(status, "PrintBodys")

        return

    def PrintBrep(self, ibody, filename):
        """
        Ocsm.PrintBrep - print the BRep associated with a specific Body

        inputs:
            ibody       Body index (1-nbody)
            filename    file to which output is appended (or "" for stdout)
        outputs:
            (None)
        """
        _ocsm.ocsmPrintBrep.argtypes = [ctypes.c_long,
                                        ctypes.c_int,
                                        ctypes.c_char_p]
        _ocsm.ocsmPrintBrep.restype  =  ctypes.c_int

        if (isinstance(filename, str)):
            filename = filename.encode()

        status = _ocsm.ocsmPrintBrep(self.modl, ibody, filename)
        _processStatus(status, "PrintBrep")

        return

    def EvalExpr(self, express):
        """
        Ocsm.EvalExpr - evaluate an expression

        inputs:
            express     expression
        outputs:
            value       value
            dot         velocity
            string      value if string-valued
        """
        _ocsm.ocsmEvalExpr.argtypes = [ctypes.c_long,
                                       ctypes.c_char_p,
                                       ctypes.POINTER(ctypes.c_double),
                                       ctypes.POINTER(ctypes.c_double),
                                       ctypes.c_char_p]
        _ocsm.ocsmEvalExpr.restype  =  ctypes.c_int

        if (isinstance(express, str)):
            express = express.encode()

        value  = ctypes.c_double()
        dot    = ctypes.c_double()
        string = ctypes.create_string_buffer(257)

        status = _ocsm.ocsmEvalExpr(self.modl, express, value, dot, string)
        _processStatus(status, "EvalExpr")

        return (value.value, dot.value, string.value.decode("utf-8"))

    def GetText(self, code):
        """
        Ocsm.GetText - convert an OCSM numeric code to text

        inputs:
            code        code to look up
        outputs:
            text        text string
        """
        _ocsm.ocsmGetText.argtypes = [ctypes.c_int]
        _ocsm.ocsmGetText.restype  =  ctypes.c_char_p

        text = _ocsm.ocsmGetText(code)

        return text.decode("utf-8")

    def GetCode(self, text):
        """
        Ocsm.GetCode - convert text to an OCSM numeric code

        inputs:
            text        text string
        outputs:
            code        numeric code
        """
        _ocsm.ocsmGetCode.argtypes = [ctypes.c_char_p]
        _ocsm.ocsmGetCode.restype  =  ctypes.c_int

        if (isinstance(text, str)):
            text = text.encode()

        code = _ocsm.ocsmGetCode(text)

        return code

#-----------------------------------------------------------------------
# Helper routines
#-----------------------------------------------------------------------

def _processStatus(status, routine):
    """
    _processStatus - raise error when status < 0

    inputs:
        status      return status
        routine     routine where error occurred
    outputs:
        (None)
    """

    # this list comes from OpenCSM.h
    if   (status == -201):
        raise OcsmError("FILE_NOT_FOUND",              routine)
    elif (status == -202):
        raise OcsmError("ILLEGAL_STATEMENT",           routine)
    elif (status == -203):
        raise OcsmError("NOT_ENOUGH_ARGS",             routine)
    elif (status == -204):
        raise OcsmError("NAME_ALREADY_DEFINED",        routine)
    elif (status == -205):
        raise OcsmError("NESTED_TOO_DEEPLY",           routine)
    elif (status == -206):
        raise OcsmError("IMPROPER_NESTING",            routine)
    elif (status == -207):
        raise OcsmError("NESTING_NOT_CLOSED",          routine)
    elif (status == -208):
        raise OcsmError("NOT_MODL_STRUCTURE",          routine)
    elif (status == -209):
        raise OcsmError("PROBLEM_CREATING_PERTURB",    routine)
    elif (status == -211):
        raise OcsmError("MISSING_MARK",                routine)
    elif (status == -212):
        raise OcsmError("INSUFFICIENT_BODYS_ON_STACK", routine)
    elif (status == -213):
        raise OcsmError("WRONG_TYPES_ON_STACK",        routine)
    elif (status == -214):
        raise OcsmError("DID_NOT_CREATE_BODY",         routine)
    elif (status == -215):
        raise OcsmError("CREATED_TOO_MANY_BODYS",      routine)
    elif (status == -216):
        raise OcsmError("TOO_MANY_BODYS_ON_STACK",     routine)
    elif (status == -217):
        raise OcsmError("ERROR_IN_BODYS_ON_STACK",     routine)
    elif (status == -218):
        raise OcsmError("MODL_NOT_CHECKED",            routine)
    elif (status == -219):
        raise OcsmError("NEED_TESSELLATION",           routine)
    elif (status == -221):
        raise OcsmError("BODY_NOT_FOUND",              routine)
    elif (status == -222):
        raise OcsmError("FACE_NOT_FOUND",              routine)
    elif (status == -223):
        raise OcsmError("EDGE_NOT_FOUND",              routine)
    elif (status == -224):
        raise OcsmError("NODE_NOT_FOUND",              routine)
    elif (status == -225):
        raise OcsmError("ILLEGAL_VALUE",               routine)
    elif (status == -226):
        raise OcsmError("ILLEGAL_ATTRIBUTE",           routine)
    elif (status == -227):
        raise OcsmError("ILLEGAL_CSYSTEM",             routine)
    elif (status == -228):
        raise OcsmError("NO_SELECTION",                routine)
    elif (status == -231):
        raise OcsmError("SKETCH_IS_OPEN",              routine)
    elif (status == -232):
        raise OcsmError("SKETCH_IS_NOT_OPEN",          routine)
    elif (status == -233):
        raise OcsmError("COLINEAR_SKETCH_POINTS",      routine)
    elif (status == -234):
        raise OcsmError("NON_COPLANAR_SKETCH_POINTS",  routine)
    elif (status == -235):
        raise OcsmError("TOO_MANY_SKETCH_POINTS",      routine)
    elif (status == -236):
        raise OcsmError("TOO_FEW_SPLINE_POINTS",       routine)
    elif (status == -237):
        raise OcsmError("SKETCH_DOES_NOT_CLOSE",       routine)
    elif (status == -238):
        raise OcsmError("SELF_INTERSECTING",           routine)
    elif (status == -239):
        raise OcsmError("ASSERT_FAILED",               routine)
    elif (status == -241):
        raise OcsmError("ILLEGAL_CHAR_IN_EXPR",        routine)
    elif (status == -242):
        raise OcsmError("CLOSE_BEFORE_OPEN",           routine)
    elif (status == -243):
        raise OcsmError("MISSING_CLOSE",               routine)
    elif (status == -244):
        raise OcsmError("ILLEGAL_TOKEN_SEQUENCE",      routine)
    elif (status == -245):
        raise OcsmError("ILLEGAL_NUMBER",              routine)
    elif (status == -246):
        raise OcsmError("ILLEGAL_PMTR_NAME",           routine)
    elif (status == -247):
        raise OcsmError("ILLEGAL_FUNC_NAME",           routine)
    elif (status == -248):
        raise OcsmError("ILLEGAL_TYPE",                routine)
    elif (status == -249):
        raise OcsmError("ILLEGAL_NARG",                routine)
    elif (status == -251):
        raise OcsmError("NAME_NOT_FOUND",              routine)
    elif (status == -252):
        raise OcsmError("NAME_NOT_UNIQUE",             routine)
    elif (status == -253):
        raise OcsmError("PMTR_IS_DESPMTR",             routine)
    elif (status == -254):
        raise OcsmError("PMTR_IS_LOCALVAR",            routine)
    elif (status == -255):
        raise OcsmError("PMTR_IS_OUTPMTR",             routine)
    elif (status == -256):
        raise OcsmError("PMTR_IS_CONPMTR",             routine)
    elif (status == -257):
        raise OcsmError("WRONG_PMTR_TYPE",             routine)
    elif (status == -258):
        raise OcsmError("FUNC_ARG_OUT_OF_BOUNDS",      routine)
    elif (status == -259):
        raise OcsmError("VAL_STACK_UNDERFLOW",         routine)
    elif (status == -260):
        raise OcsmError("VAL_STACK_OVERFLOW",          routine)
    elif (status == -261):
        raise OcsmError("ILLEGAL_BRCH_INDEX",          routine)
    elif (status == -262):
        raise OcsmError("ILLEGAL_PMTR_INDEX",          routine)
    elif (status == -263):
        raise OcsmError("ILLEGAL_BODY_INDEX",          routine)
    elif (status == -264):
        raise OcsmError("ILLEGAL_ARG_INDEX",           routine)
    elif (status == -265):
        raise OcsmError("ILLEGAL_ACTIVITY",            routine)
    elif (status == -266):
        raise OcsmError("ILLEGAL_MACRO_INDEX",         routine)
    elif (status == -267):
        raise OcsmError("ILLEGAL_ARGUMENT",            routine)
    elif (status == -268):
        raise OcsmError("CANNOT_BE_SUPPRESSED",        routine)
    elif (status == -269):
        raise OcsmError("STORAGE_ALREADY_USED",        routine)
    elif (status == -270):
        raise OcsmError("NOTHING_PREVIOUSLY_STORED",   routine)
    elif (status == -271):
        raise OcsmError("SOLVER_IS_OPEN",              routine)
    elif (status == -272):
        raise OcsmError("SOLVER_IS_NOT_OPEN",          routine)
    elif (status == -273):
        raise OcsmError("TOO_MANY_SOLVER_VARS",        routine)
    elif (status == -274):
        raise OcsmError("UNDERCONSTRAINED",            routine)
    elif (status == -275):
        raise OcsmError("OVERCONSTRAINED",             routine)
    elif (status == -276):
        raise OcsmError("SINGULAR_MATRIX",             routine)
    elif (status == -277):
        raise OcsmError("NOT_CONVERGED",               routine)
    elif (status == -281):
        raise OcsmError("UDP_ERROR1",                  routine)
    elif (status == -282):
        raise OcsmError("UDP_ERROR2",                  routine)
    elif (status == -283):
        raise OcsmError("UDP_ERROR3",                  routine)
    elif (status == -284):
        raise OcsmError("UDP_ERROR4",                  routine)
    elif (status == -285):
        raise OcsmError("UDP_ERROR5",                  routine)
    elif (status == -286):
        raise OcsmError("UDP_ERROR6",                  routine)
    elif (status == -287):
        raise OcsmError("UDP_ERROR7",                  routine)
    elif (status == -288):
        raise OcsmError("UDP_ERROR8",                  routine)
    elif (status == -289):
        raise OcsmError("UDP_ERROR9",                  routine)
    elif (status == -291):
        raise OcsmError("OP_STACK_UNDERFLOW",          routine)
    elif (status == -292):
        raise OcsmError("OP_STACK_OVERFLOW",           routine)
    elif (status == -293):
        raise OcsmError("RPN_STACK_UNDERFLOW",         routine)
    elif (status == -294):
        raise OcsmError("RPN_STACK_OVERFLOW",          routine)
    elif (status == -295):
        raise OcsmError("TOKEN_STACK_UNDERFLOW",       routine)
    elif (status == -296):
        raise OcsmError("TOKEN_STACK_OVERFLOW",        routine)
    elif (status == -298):
        raise OcsmError("UNSUPPORTED",                 routine)
    elif (status == -299):
        raise OcsmError("INTERNAL_ERROR",              routine)

#-----------------------------------------------------------------------
# OcsmError class
#-----------------------------------------------------------------------

class OcsmError(Exception):

    # Constructor or Initializer
    def __init__(self, value, routine):
        self.value   = value
        self.routine = routine

    # __str__ is to print() the value
    def __str__(self):
        return("OcsmError: "+repr(self.value)+" detected during call to "+self.routine)
