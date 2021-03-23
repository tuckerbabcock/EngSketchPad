###########################################################################
#                                                                         #
# pyCAPS --- Python version of CAPS API                                 #
#                                                                         #
#                                                                         #
#      Copyright 2011-2021, Massachusetts Institute of Technology         #
#      Licensed under The GNU Lesser General Public License, version 2.1  #
#      See http://www.opensource.org/licenses/lgpl-2.1.php                #
#                                                                         #
###########################################################################

import ctypes
from ctypes import POINTER, c_short, c_int, c_ulong, c_size_t, c_double, c_void_p, c_char, c_char_p
import os
import sys
import json
import copy
from sys import version_info
#import xml.etree.ElementTree as xmlElementTree

from pyEGADS import egads

# get the value of _ESP_ROOT
try:
    _ESP_ROOT = os.environ["ESP_ROOT"]
except:
    raise RuntimeError("ESP_ROOT must be set -- Please fix the environment...")

# load the shared library
if sys.platform.startswith('darwin'):
    _caps = ctypes.CDLL(_ESP_ROOT + "/lib/libcaps.dylib")
elif sys.platform.startswith('linux'):
    _caps = ctypes.CDLL(_ESP_ROOT + "/lib/libcaps.so")
elif sys.platform.startswith('win32'):
    _caps = ctypes.CDLL(_ESP_ROOT + "/lib/caps.dll")
else:
    raise IOError("Unknown platform: " + sys.platform)

__all__ = []

# =============================================================================
# alias for clarification
enum_capsoType   = c_int
enum_capssType   = c_int
enum_capsvType   = c_int
enum_capsFixed   = c_int
enum_capsNull    = c_int
enum_capstMethod = c_int
enum_capsdMethod = c_int
enum_capsState   = c_int

# =============================================================================
# define c_capsTuple structure
class c_capsTuple(ctypes.Structure):
    pass

c_capsTuple.__slots__ = [
    'name',
    'value',
]
c_capsTuple._fields_ = [
    ('name', c_char_p),
    ('value', c_char_p),
]

# =============================================================================
# define c_capsOwn structure
class c_capsOwn(ctypes.Structure):
    pass

c_capsOwn.__slots__ = [
    'pname',
    'pID',
    'user',
    'datetime',
    'sNum',
]
c_capsOwn._fields_ = [
    ('pname', c_char_p),
    ('pID', c_char_p),
    ('user', c_char_p),
    ('datetime', c_short * int(6)),
    ('sNum', c_ulong),
]

# =============================================================================
# define c_capsObject structure
class c_capsObject(ctypes.Structure):
    pass

c_capsObject.__slots__ = [
    'magicnumber',
    'type',
    'subtype',
    'sn',
    'name',
    'attrs',
    'blind',
    'last',
    'parent',
]
c_capsObject._fields_ = [
    ('magicnumber', c_int),
    ('type', c_int),
    ('subtype', c_int),
    ('sn', c_int),
    ('name', c_char_p),
    ('attrs', POINTER(egads.c_egAttrs)),
    ('blind', POINTER(None)),
    ('last', c_capsOwn),
    ('parent', POINTER(c_capsObject)),
]

# define the c_capsObj pointer
c_capsObj = POINTER(c_capsObject)

# =============================================================================
# define c_capsError structure
class c_capsError(ctypes.Structure):
    pass

c_capsError.__slots__ = [
    'errObj',
    'eType',
    'index',
    'nLines',
    'lines',
]
c_capsError._fields_ = [
    ('errObj', POINTER(c_capsObject)),
    ('eType', c_int),
    ('index', c_int),
    ('nLines', c_int),
    ('lines', POINTER(c_char_p)),
]

# =============================================================================
# define c_capsErrs structure
class c_capsErrs(ctypes.Structure):
    pass

c_capsErrs.__slots__ = [
    'nError',
    'errors',
]
c_capsErrs._fields_ = [
    ('nError', c_int),
    ('errors', POINTER(c_capsError)),
]

# =============================================================================
# define c_capsDot structure
class c_capsDot(ctypes.Structure):
    pass

c_capsDot.__slots__ = [
    'name',
    'rank',
    'dot',
]
c_capsDot._fields_ = [
    ('name', c_char_p),
    ('rank', c_int),
    ('dot', POINTER(c_double)),
]

# =============================================================================
# define c_capsValue structure
class union_capsValue_vals(ctypes.Union):
    pass

union_capsValue_vals.__slots__ = [
    'integer',
    'integers',
    'real',
    'reals',
    'string',
    'tuple',
    'object',
    'objects',
    'AIMptr',
]
union_capsValue_vals._fields_ = [
    ('integer', c_int),
    ('integers', POINTER(c_int)),
    ('real', c_double),
    ('reals', POINTER(c_double)),
    ('string', c_char_p),
    ('tuple', POINTER(c_capsTuple)),
    ('object', POINTER(c_capsObject)),
    ('objects', POINTER(POINTER(c_capsObject))),
    ('AIMptr', POINTER(None)),
]

class union_capsValue_limits(ctypes.Union):
    pass

union_capsValue_limits.__slots__ = [
    'ilims',
    'dlims',
]
union_capsValue_limits._fields_ = [
    ('ilims', c_int * int(2)),
    ('dlims', c_double * int(2)),
]

class c_capsValue(ctypes.Structure):
    pass

c_capsValue.__slots__ = [
    'type',
    'length',
    'dim',
    'nrow',
    'ncol',
    'lfixed',
    'sfixed',
    'nullVal',
    'pIndex',
    'gInType',
    'vals',
    'limits',
    'units',
    'link',
    'linkMethod',
    'partial',
    'ndot',
    'dots',
]
c_capsValue._fields_ = [
    ('type', c_int),
    ('length', c_int),
    ('dim', c_int),
    ('nrow', c_int),
    ('ncol', c_int),
    ('lfixed', c_int),
    ('sfixed', c_int),
    ('nullVal', c_int),
    ('pIndex', c_int),
    ('gInType', c_int),
    ('vals', union_capsValue_vals),
    ('limits', union_capsValue_limits),
    ('units', c_char_p),
    ('link', POINTER(c_capsObject)),
    ('linkMethod', c_int),
    ('partial', POINTER(c_int)),
    ('ndot', c_int),
    ('dots', POINTER(c_capsDot)),
]


# =============================================================================
#
# caps.h functions
#
# =============================================================================

_caps.caps_revision.argtypes = [POINTER(c_int), POINTER(c_int)]
_caps.caps_revision.restype = None

_caps.caps_info.argtypes = [c_capsObj, POINTER(c_char_p), POINTER(enum_capsoType), POINTER(enum_capssType), POINTER(c_capsObj), POINTER(c_capsObj), POINTER(c_capsOwn)]
_caps.caps_info.restype = c_int

_caps.caps_size.argtypes = [c_capsObj, enum_capsoType, enum_capssType, POINTER(c_int), POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_size.restype = c_int

_caps.caps_childByIndex.argtypes = [c_capsObj, enum_capsoType, enum_capssType, c_int, POINTER(c_capsObj)]
_caps.caps_childByIndex.restype = c_int

_caps.caps_childByName.argtypes = [c_capsObj, enum_capsoType, enum_capssType, c_char_p, POINTER(c_capsObj)]
_caps.caps_childByName.restype = c_int

_caps.caps_bodyByIndex.argtypes = [c_capsObj, c_int, POINTER(egads.c_ego), POINTER(c_char_p)]
_caps.caps_bodyByIndex.restype = c_int

_caps.caps_ownerInfo.argtypes = [c_capsOwn, POINTER(c_char_p), POINTER(c_char_p), POINTER(c_char_p), POINTER(c_short), POINTER(c_ulong)]
_caps.caps_ownerInfo.restype = c_int

_caps.caps_setOwner.argtypes = [c_capsObj, c_char_p, POINTER(c_capsOwn)]
_caps.caps_setOwner.restype = c_int

_caps.caps_freeOwner.argtypes = [POINTER(c_capsOwn)]
_caps.caps_freeOwner.restype = None

_caps.caps_delete.argtypes = [c_capsObj]
_caps.caps_delete.restype = c_int

_caps.caps_errorInfo.argtypes = [POINTER(c_capsErrs), c_int, POINTER(c_capsObj), POINTER(c_int), POINTER(POINTER(c_char_p))]
_caps.caps_errorInfo.restype = c_int

_caps.caps_freeError.argtypes = [POINTER(c_capsErrs)]
_caps.caps_freeError.restype = c_int

_caps.caps_freeValue.argtypes = [POINTER(c_capsValue)]
_caps.caps_freeValue.restype = None

_caps.caps_writeParameters.argtypes = [c_capsObj, c_char_p]
_caps.caps_writeParameters.restype = c_int

_caps.caps_readParameters.argtypes = [c_capsObj, c_char_p]
_caps.caps_readParameters.restype = c_int

_caps.caps_writeGeometry.argtypes = [c_capsObj, c_int, c_char_p, POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_writeGeometry.restype = c_int

_caps.caps_attrByName.argtypes = [c_capsObj, c_char_p, POINTER(c_capsObj)]
_caps.caps_attrByName.restype = c_int

_caps.caps_attrByIndex.argtypes = [c_capsObj, c_int, POINTER(c_capsObj)]
_caps.caps_attrByIndex.restype = c_int

_caps.caps_setAttr.argtypes = [c_capsObj, c_char_p, c_capsObj]
_caps.caps_setAttr.restype = c_int

_caps.caps_deleteAttr.argtypes = [c_capsObj, c_char_p]
_caps.caps_deleteAttr.restype = c_int

_caps.caps_open.argtypes = [c_char_p, c_char_p, POINTER(c_capsObj)]
_caps.caps_open.restype = c_int

_caps.caps_save.argtypes = [c_capsObj, c_char_p]
_caps.caps_save.restype = c_int

_caps.caps_close.argtypes = [c_capsObj]
_caps.caps_close.restype = c_int

_caps.caps_outLevel.argtypes = [c_capsObj, c_int]
_caps.caps_outLevel.restype = c_int

_caps.caps_sensitivity.argtypes = [c_capsObj, c_int, c_int, c_int, POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_sensitivity.restype = c_int

_caps.caps_queryAnalysis.argtypes = [c_capsObj, c_char_p, POINTER(c_int), POINTER(c_int), POINTER(c_int)]
_caps.caps_queryAnalysis.restype = c_int

_caps.caps_getBodies.argtypes = [c_capsObj, POINTER(c_int), POINTER(POINTER(egads.c_ego))]
_caps.caps_getBodies.restype = c_int

_caps.caps_getInput.argtypes = [c_capsObj, c_char_p, c_int, POINTER(c_char_p), POINTER(c_capsValue)]
_caps.caps_getInput.restype = c_int

_caps.caps_getOutput.argtypes = [c_capsObj, c_char_p, c_int, POINTER(c_char_p), POINTER(c_capsValue)]
_caps.caps_getOutput.restype = c_int

_caps.caps_AIMbackdoor.argtypes = [c_capsObj, c_char_p, POINTER(c_char_p)]
_caps.caps_AIMbackdoor.restype = c_int

_caps.caps_makeAnalysis.argtypes = [c_capsObj, c_char_p, c_char_p, c_char_p, c_char_p, c_int, POINTER(c_capsObj), POINTER(c_capsObj)]
_caps.caps_makeAnalysis.restype = c_int

_caps.caps_dupAnalysis.argtypes = [c_capsObj, c_char_p, c_int, POINTER(c_capsObj), POINTER(c_capsObj)]
_caps.caps_dupAnalysis.restype = c_int

_caps.caps_dirtyAnalysis.argtypes = [c_capsObj, POINTER(c_int), POINTER(POINTER(c_capsObj))]
_caps.caps_dirtyAnalysis.restype = c_int

_caps.caps_analysisInfo.argtypes = [c_capsObj, POINTER(c_char_p), POINTER(c_char_p), POINTER(c_char_p), POINTER(c_int), POINTER(POINTER(c_capsObj)), POINTER(c_int), POINTER(POINTER(c_char_p)), POINTER(POINTER(c_int)), POINTER(c_int), POINTER(c_int)]
_caps.caps_analysisInfo.restype = c_int

_caps.caps_preAnalysis.argtypes = [c_capsObj, POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_preAnalysis.restype = c_int

_caps.caps_postAnalysis.argtypes = [c_capsObj, c_capsOwn, POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_postAnalysis.restype = c_int

_caps.caps_makeBound.argtypes = [c_capsObj, c_int, c_char_p, POINTER(c_capsObj)]
_caps.caps_makeBound.restype = c_int

_caps.caps_boundInfo.argtypes = [c_capsObj, POINTER(enum_capsState), POINTER(c_int), POINTER(c_double)]
_caps.caps_boundInfo.restype = c_int

_caps.caps_completeBound.argtypes = [c_capsObj]
_caps.caps_completeBound.restype = c_int

_caps.caps_fillVertexSets.argtypes = [c_capsObj, POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_fillVertexSets.restype = c_int

_caps.caps_makeVertexSet.argtypes = [c_capsObj, c_capsObj, c_char_p, POINTER(c_capsObj)]
_caps.caps_makeVertexSet.restype = c_int

_caps.caps_vertexSetInfo.argtypes = [c_capsObj, POINTER(c_int), POINTER(c_int), POINTER(c_capsObj), POINTER(c_capsObj)]
_caps.caps_vertexSetInfo.restype = c_int

_caps.caps_outputVertexSet.argtypes = [c_capsObj, c_char_p]
_caps.caps_outputVertexSet.restype = c_int

_caps.caps_fillUnVertexSet.argtypes = [c_capsObj, c_int, POINTER(c_double)]
_caps.caps_fillUnVertexSet.restype = c_int

_caps.caps_makeDataSet.argtypes = [c_capsObj, c_char_p, enum_capsdMethod, c_int, POINTER(c_capsObj)]
_caps.caps_makeDataSet.restype = c_int

_caps.caps_initDataSet.argtypes = [c_capsObj, c_int, POINTER(c_double)]
_caps.caps_initDataSet.restype = c_int

_caps.caps_setData.argtypes = [c_capsObj, c_int, c_int, POINTER(c_double), c_char_p]
_caps.caps_setData.restype = c_int

_caps.caps_getData.argtypes = [c_capsObj, POINTER(c_int), POINTER(c_int), POINTER(POINTER(c_double)), POINTER(c_char_p)]
_caps.caps_getData.restype = c_int

_caps.caps_getHistory.argtypes = [c_capsObj, POINTER(c_capsObj), POINTER(c_int), POINTER(POINTER(c_capsOwn))]
_caps.caps_getHistory.restype = c_int

_caps.caps_getDataSets.argtypes = [c_capsObj, c_char_p, POINTER(c_int), POINTER(POINTER(c_capsObj))]
_caps.caps_getDataSets.restype = c_int

_caps.caps_triangulate.argtypes = [c_capsObj, POINTER(c_int), POINTER(POINTER(c_int)), POINTER(c_int), POINTER(POINTER(c_int))]
_caps.caps_triangulate.restype = c_int

_caps.caps_getValue.argtypes = [c_capsObj, POINTER(enum_capsvType), POINTER(c_int), POINTER(c_int), POINTER(POINTER(None)), POINTER(POINTER(c_int)), POINTER(c_char_p), POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_getValue.restype = c_int

_caps.caps_makeValue.argtypes = [c_capsObj, c_char_p, enum_capssType, enum_capsvType, c_int, c_int, POINTER(None), POINTER(c_int), c_char_p, POINTER(c_capsObj)]
_caps.caps_makeValue.restype = c_int

_caps.caps_setValue.argtypes = [c_capsObj, enum_capsvType, c_int, c_int, POINTER(None), POINTER(c_int), c_char_p, POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_setValue.restype = c_int

_caps.caps_getLimits.argtypes = [c_capsObj, POINTER(enum_capsvType), POINTER(POINTER(None)), POINTER(c_char_p)]
_caps.caps_getLimits.restype = c_int

_caps.caps_setLimits.argtypes = [c_capsObj, enum_capsvType, POINTER(None), c_char_p]
_caps.caps_setLimits.restype = c_int

_caps.caps_getValueProps.argtypes = [c_capsObj, POINTER(c_int), POINTER(c_int), POINTER(enum_capsFixed), POINTER(enum_capsFixed), POINTER(enum_capsNull)]
_caps.caps_getValueProps.restype = c_int

_caps.caps_setValueProps.argtypes = [c_capsObj, c_int, enum_capsFixed, enum_capsFixed, enum_capsNull]
_caps.caps_setValueProps.restype = c_int

_caps.caps_convertValue.argtypes = [c_capsObj, c_double, c_char_p, POINTER(c_double)]
_caps.caps_convertValue.restype = c_int

_caps.caps_transferValues.argtypes = [c_capsObj, enum_capstMethod, c_capsObj, POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_transferValues.restype = c_int

_caps.caps_makeLinkage.argtypes = [c_capsObj, enum_capstMethod, c_capsObj]
_caps.caps_makeLinkage.restype = c_int

_caps.caps_hasDot.argtypes = [c_capsObj, POINTER(c_int), POINTER(POINTER(c_char_p))]
_caps.caps_hasDot.restype = c_int

_caps.caps_getDot.argtypes = [c_capsObj, c_char_p, POINTER(c_int), POINTER(c_int), POINTER(POINTER(c_double)), POINTER(c_int), POINTER(POINTER(c_capsErrs))]
_caps.caps_getDot.restype = c_int

# units

_caps.caps_convert.argtypes = [c_char_p, c_double, c_char_p, POINTER(c_double)]
_caps.caps_convert.restype = c_int

_caps.caps_unitMultiply.argtypes = [c_char_p, c_char_p, POINTER(c_char_p)]
_caps.caps_unitMultiply.restype = c_int

_caps.caps_unitDivide.argtypes = [c_char_p, c_char_p, POINTER(c_char_p)]
_caps.caps_unitDivide.restype = c_int

# others

_caps.printObjects.argtypes = [c_capsObj, c_int]
_caps.printObjects.restype = None

# =============================================================================

# Extract CAPS error codes
_caps_error_codes = {}
globalDict = globals()
with open(os.path.join(_ESP_ROOT,"include","capsErrors.h")) as fp:
    lines = fp.readlines()
    for line in lines:
        define = line.split()
        if len(define) < 3: continue
        if define[0] != "#define": continue
        _caps_error_codes[int(define[2])] = define[1]
        globalDict[define[1]] = int(define[2])
        __all__.append(define[1])
del fp
del lines
del define

# Extract CAPS enums
__all__.append("oType")
class oType:
     BODIES     = -2
     ATTRIBUTES = -1
#    UNUSED     =  0
     PROBLEM    =  1
     VALUE      =  2
     ANALYSIS   =  3
     BOUND      =  4
     VERTEXSET  =  5
     DATASET    =  6

__all__.append("sType")
class sType:
    NONE        = 0
    STATIC      = 1
    PARAMETRIC  = 2
    GEOMETRYIN  = 3
    GEOMETRYOUT = 4
    BRANCH      = 5
    PARAMETER   = 6
    USER        = 7
    ANALYSISIN  = 8
    ANALYSISOUT = 9
    CONNECTED   = 10
    UNCONNECTED = 11

__all__.append("eType")
class eType:
    CONTINUATION = -1
    CINFO        =  0
    CWARN        =  1
    CERROR       =  2
    CSTAT        =  3

__all__.append("vType")
class vType:
    Boolean   = 0
    Integer   = 1
    Double    = 2
    String    = 3
    Tuple     = 4
    Value     = 5
    DoubleDot = 6
    # extra value for Python API
    Dict      = 7
    Null      = 8

__all__.append("vDim")
class vDim:
    Scalar  = 0
    Vector  = 1
    Array2D = 2

__all__.append("Fixed")
class Fixed:
    Change = 0
    Fixed  = 1

__all__.append("Null")
class Null:
    NotAllowed = 0
    NotNull    = 1
    IsNull     = 2
    IsPartial  = 3

__all__.append("tMethod")
class tMethod:
    Copy      = 0
    Integrate = 1
    Average   = 2

__all__.append("dMethod")
class dMethod:
    BuiltIn     = 0
    Sensitivity = 1
    Analysis    = 2
    Interpolate = 3
    Conserve    = 4
    User        = 5

__all__.append("State")
class State:
    MultipleError = -2
    Open          = -1
    Empty         =  0
    Single        =  1
    Multiple      =  2

# =============================================================================

# Extract OpenCSM error codes
_ocsm_error_codes = {}
with open(os.path.join(_ESP_ROOT,"include","OpenCSM.h")) as fp:
    lines = fp.readlines()
    for line in lines:
        define = line.split()
        if len(define) < 3: continue
        if define[0] != "#define": continue
        try:
            code = int(define[2])
        except:
            continue
        if code > -200: continue
        _ocsm_error_codes[code] = define[1]
del fp
del lines
del define
del code

# =============================================================================

def _decode(data):
    if version_info.major > 2 and isinstance(data, bytes):
        return data.decode()
    return data

# =============================================================================

__all__.append("revision")
def revision():
    """
    Revision - return CAPS revision

    Returns
    -------
        imajor:  major version number
        iminor:  minor version number
    """
    imajor = c_int()
    iminor = c_int()

    stat = _caps.caps_revision(ctypes.byref(imajor), ctypes.byref(iminor))
    if stat: _raiseStatus(stat)

    return (imajor.value, iminor.value)

# =============================================================================

# CAPS error exception class. 
__all__.append("CAPSError")
class CAPSError(Exception):
    
    InternalError = "CAPS Internal Error: "
    
    def __init__(self, status, msg=None, errors=None):
        
        # Combine errors 
        self.errorDict = dict(egads._egads_error_codes)
        self.errorDict.update(_ocsm_error_codes)
        self.errorDict.update(_caps_error_codes)
        
        ## Error code encountered when running the CAPS
        self.status = status
        
        ## Name of error code encountered
        self.errorName = "UNKNOWN_ERROR"
        self.errors = errors

        if status not in self.errorDict:
            msgPre = "CAPS error code " + str(status) + " (undefined error)"
        else:
            msgPre = self.errorDict[status]
            self.errorName = self.errorDict[status]
            
        if msg is not None:
            msg = ": " + msg
        else:
            msg = ""
            
        if errors is not None:
            msg += ":\n"
            msg += "="*80 + "\n"
            for line in errors.info():
                msg += line + "\n"
            msg += "="*80 + "\n"

        super(CAPSError,self).__init__(msgPre + msg)

# =============================================================================

def _raiseStatus(status, msg=None, errors=None):
    # Check status flag returned by CAPS
    
    if status >= CAPS_SUCCESS: return

    raise CAPSError(status, msg=msg, errors=errors)

# =============================================================================
# __all__.append("c_to_py")
# def c_to_py(c_obj):
#     """
#     Creates a Python class for an exiting c_ego object.
#     
#     Parameters
#     ----------
#     c_obj:
#         the c_capsObj, c_capsOwn, or c_capsErrs instance
#     """
#     if isinstance(c_obj, c_capsObj):
#         return capsObj(c_obj, None, None)
#     elif isinstance(c_obj, c_capsOwn):
#         return capsOwn(c_obj)
#     elif isinstance(c_obj, c_capsErrs):
#         return capsErrs(c_obj)
#     else:
#         raise CAPSError(msg='c_obj must be a c_capsObj, c_capsOwn, or c_capsErrs instance')

# =============================================================================

# Free CAPS owner.
def _caps_freeOwner(owner):
    
    stat = _caps.caps_freeOwner(owner)
    if stat: _raiseStatus(stat, msg = "while freeing CAPS capsOwner")

#=============================================================================-
__all__.append("capsOwn")
class capsOwn:
    """
    Wrapper to represent a capsOwn structure
    """
    def __init__(self, owner, freeOwner=False):
        """
        Constructor for internal use only

        Parameters
        ----------
        owner: 
            c_capsOwn instance

        freeOwner: 
            if true, caps_freeOwner is called during garbage collection
        """
        self._finalize = None
        if not isinstance(owner, c_capsOwn): raise CAPSError(msg=CAPSError.InternalError+"owner must be type c_capsOwn")
        self._owner = owner
        if freeOwner:
            self._finalize = egads.finalize(self, _caps_freeOwner, self._owner)

#=============================================================================-
    def __del__(self):
        # free the owner
        if self._finalize is not None:
            self._finalize()

#=============================================================================-
    def info(self):
        """
        Retrieve owner information

        Returns
        -------
        pname:
            the process name
        
        pID:
            the process ID
            
        userID:
            the user ID

        lines:
            Line-by-line list of descriptions
            
        datetime:
            the filled date/time stamp info - 6 in length:
            year, month, day, hour, minute, second
            
        sNum:
            the sequence number (always increasing)
        """
        pname  = c_char_p()
        pID    = c_char_p()
        userID = c_char_p()
        nLines = c_int()
        plines  = POINTER(c_char_p)()
        pdatetime = (c_short*6)()
        sNum   = c_ulong()
        # EnCAPS
#         stat = _caps.caps_ownerInfo(self._owner, ctypes.byref(pname), ctypes.byref(pID),
#                                     ctypes.byref(userID), ctypes.byref(nLines), ctypes.byref(plines),
#                                     pdatetime, ctypes.byref(sNum))
#         if stat: _raiseStatus(stat)
# 
#         name   = _decode(pname.value)
#         pID    = pID.value.decode()
#         userID = userID.value.decode()
#         lines = []
#         for i in range(nLines.value):
#             lines.append(plines[i].value.decode())
#         datetime = list(pdatetime[:])
#         sNum     = sNum.value
# 
#         return name, pID, userID, lines, datetime, sNum

        # CAPS
        stat = _caps.caps_ownerInfo(self._owner, ctypes.byref(pname), ctypes.byref(pID),
                                    ctypes.byref(userID), 
                                    pdatetime, ctypes.byref(sNum))
        if stat: _raiseStatus(stat)

        name     = _decode(pname.value)
        pID      = _decode(pID.value)
        userID   = _decode(userID.value)
        datetime = list(pdatetime[:])
        sNum     = sNum.value

        return name, pID, userID, datetime, sNum
        # end CAPS


# =============================================================================

# Free CAPS errors.
def _caps_freeError(errors):
    
    stat = _caps.caps_freeError(errors)
    if stat: _raiseStatus(stat, msg = "while freeing CAPS capsErrs structure")

#=============================================================================-
__all__.append("capsErrs")
class capsErrs:
    """
    Wrapper to represent a list of c_capsErrs structures
    """
    def __init__(self, nErr, errors, errObj):
        """
        Constructor for internal use only

        Parameters
        ----------
        nErr: 
            c_int number of errors
            
        errors:
            ctypes.POINTER(c_capsErrs) instance of errors

        errObj:
            c_capsObj offending CAPS object
        """
        self._finalize = None
        if not isinstance(nErr, c_int): raise CAPSError(msg=CAPSError.InternalError+"nErr must be type c_int")
        if not isinstance(errors, ctypes.POINTER(c_capsErrs)): raise CAPSError(msg=CAPSError.InternalError+"errors must be type ctypes.POINTER(c_capsErr)")
        if not isinstance(errObj, c_capsObj): raise CAPSError(msg=CAPSError.InternalError+"obj must be type c_capsObj")
        self._nErr = nErr.value
        self._errors = errors
        self._errObj = errObj
        self._finalize = egads.finalize(self, _caps_freeError, self._errors)

#=============================================================================-
    def __del__(self):
        # free the errors
        if self._finalize is not None:
            self._finalize()

#=============================================================================-
    def info(self):
        """
        Retrieve error information

        Returns
        -------
            Line-by-line list of error messages 
        """
        lines = []
        for i in range(self._nErr):
            nLines = c_int()
            plines  = ctypes.POINTER(c_char_p)()
            stat = _caps.caps_errorInfo(self._errors, c_int(i+1), self._errObj,
                                        ctypes.byref(nLines), ctypes.byref(plines))
            if stat: _raiseStatus(stat)
            
            for i in range(nLines.value):
                lines.append(_decode(plines[i]))

        return lines

# =============================================================================

# Close a CAPS problem.
def _close_capsProblem(problemObj):
    
    stat = _caps.caps_close(problemObj)
    if stat: _raiseStatus(stat, msg = "while closing CAPS Problem")

#=============================================================================-

# free a CAPS Object (only User defined Value and Bound Objects)
def _caps_delete(obj):
    
    stat = _caps.caps_delete(obj)
    if stat: _raiseStatus(stat, msg = "while deleting CAPS Object")

#=============================================================================-
__all__.extend("open")
def open(pname, ptr):
    """
    Open a CAPS Problem Object

    Parameters
    ----------
    pname:
        the full-path ending with the CAPS problem name
        if exists the stored data initializes the problem, 
        otherwise the directory is created

    ptr:
        the full-path input file name (not needed when restarting) - based on file extension: 
        *.csm initialize the project using the specified OpenCSM file
        *.egads initialize the project based on the static geometry
        - or - 
        pointer to an OpenCSM Model Structure - left open after caps_close
    """    
    pname = pname.encode() if isinstance(pname, str) else pname
    if isinstance(ptr,(str, bytes)):
        ptr = ptr.encode()
        flag = 0
        # EnCAPS
        #ptr = ctypes.cast(ptr, c_void_p)

    problemObj = c_capsObj()
    # EnCAPS
    #stat = _caps.caps_open(pname, c_int(flag), ptr, ctypes.byref(self._obj))
    # CAPS
    stat = _caps.caps_open(ptr, pname, ctypes.byref(problemObj))
    # end CAPS
    if stat: _raiseStatus(stat)

    return capsObj(problemObj, None, deleteFunction=_close_capsProblem)

#=============================================================================-
class capsObj:
    """
    Base class Wrapper to represent a CAPS Object
    """
#=============================================================================-
    def __init__(self, obj, problemObj, deleteFunction):
        """
        Constructor only intended for internal use.

        Parameters
        ----------
        obj:
            c_capsObj Object

        problemObj:
            capsObj Problem Object (None if c_capsObj is a Problem Object)

        deleteFunction:
            if not None. the class instance will call deleteFunction
            for the provided c_capsObj during garbage collection
        """
        self._finalize = None
        if not isinstance(obj, c_capsObj): raise CAPSError(msg=CAPSError.InternalError+"obj must be type c_capsObj")
        self._obj = obj
        self._problemObj = problemObj
        if deleteFunction is not None:
            self._finalize = egads.finalize(self, deleteFunction, self._obj)

#=============================================================================-
    def __del__(self):
        # free the value object
        if self._finalize is not None:
            self._finalize()

#=============================================================================-
    def __eq__(self, obj):
        """
        checks pointer equality of underlying c_capsObj memory address
        """
        return isinstance(obj, capsObj) and ctypes.addressof(obj._obj.contents) == ctypes.addressof(self._obj.contents)

#=============================================================================-
    def __ne__(self, obj):
        return not self == obj

#=============================================================================-
    def problemObj(self):
        """
        Returns the problemObj associated with this capsObj (self of self is a problemObj)
        """
        return self if self._problemObj is None else self._problemObj

#=============================================================================-
    def info(self):
        """
        Return information about the object

        Returns
        -------
        name:
            the returned Object name pointer (if any)

        otype:
           the returned Object type: Problem, Value, Analysis, Bound, VertexSet, DataSet

        stype:
            the returned subtype (depending on type)

        link:
            the returned linkage Value Object (None - no link)

        parent:
            the returned parent Object (None for a Problem or an Attribute generated User Value)

        last:
            the returned last owner/history to touch the Object
        """
        name   = c_char_p()
        otype  = c_int()
        stype  = c_int()
        link   = c_capsObj()
        parent = c_capsObj()
        last   = c_capsOwn()
        stat = _caps.caps_info(self._obj, ctypes.byref(name), ctypes.byref(otype),
                               ctypes.byref(stype), ctypes.byref(link), ctypes.byref(parent), ctypes.byref(last))
        if stat: _raiseStatus(stat)
        
        name   = _decode(name.value)
        otype  = otype.value
        stype  = stype.value
        link   = capsObj(link  , self.problemObj(), None) if link   else None
        parent = capsObj(parent, self.problemObj(), None) if parent else None
        last   = capsOwn(last)                            if last   else None
        
        return name, otype, stype, link, parent, last

#=============================================================================-
    def size(self, type, stype):
        """
        Children sizing information from a Patent Object

        Parameters
        ----------
        type:
           the data type to size: Bodies, Problem, Value, Analysis, Bound, VertexSet, DataSet

        stype:
            the subtype to size (depending on type)

        Returns
        -------
        the size
        """
        size = c_int()
        nErr = c_int()
        errs = POINTER(c_capsErrs)()
        stat = _caps.caps_size(self._obj, c_int(type), c_int(stype), ctypes.byref(size), 
                               ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))
        return size.value

#=============================================================================-
    def childByName(self, otype, stype, name):
        """
        Retrieve a CAPS child object by name

        Parameters
        ----------
        otype:
            the Object type to return: Value, Analysis, Bound, VertexSet, DataSet

        stype:
            the subtype to find (depending on type)

        name:
            string name of the object

        Returns
        -------
        child:
            the returned CAPS Object
        """
        name   = name.encode() if isinstance(name,str) else name
        child  = c_capsObj()
        units  = c_char_p()
        stat = _caps.caps_childByName(self._obj, c_int(otype), c_int(stype), 
                                      c_char_p(name), ctypes.byref(child))
        if stat: _raiseStatus(stat)
        return capsObj(child, self.problemObj(), None)

#=============================================================================-
    def childByIndex(self, otype, stype, index):
        """
        Retrieve a CAPS child object by name

        Parameters
        ----------
        otype:
            the Object type to return: Value, Analysis, Bound, VertexSet, DataSet

        stype:
            the subtype to find (depending on type)

        index:
            the index [1 - size]

        Returns
        -------
        child:
            the returned CAPS Object
        """
        child  = c_capsObj()
        units  = c_char_p()
        stat = _caps.caps_childByIndex(self._obj, c_int(otype), c_int(stype), 
                                       c_int(index), ctypes.byref(child))
        if stat: _raiseStatus(stat)
        return capsObj(child, self.problemObj(), None)

#=============================================================================-
    def getHistory(self):
        """
        Retrieve history of a CAPS Object

        Returns
        -------
            list of capsOwn structures
        """
        nhist = c_int()
        phist  = ctypes.POINTER(c_capsOwn)()
        stat = _caps.caps_getHistory(self._obj, ctypes.byref(nhist), ctypes.byref(phist))
        if stat: _raiseStatus(stat)
        
        nhist = nhist.value
        hist = [None]*nhist
        for i in range(nhist):
            hist[i] = capsOwn(phist[i])

        return hist

#=============================================================================-
    def addHistory(self, hist):
        """
        Add history to a CAPS Object

        Parameters
        ----------
        hist:
            a CAPS capsOwn structure to add to the history for the Object 
        """
        stat = _caps.caps_addHistory(self._obj, hist._owner)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def attrByName(self, name):
        """
        Retrieve an attribute by name

        Parameters
        ----------
        name:
            string name of the Attribute
            
        Returns
        -------
            User Value Object
        """
        name = name.encode() if isinstance(name,str) else name
        
        attr = c_capsObj()
        
        stat = _caps.caps_attrByName(self._obj, name, ctypes.byref(attr))
        if stat: _raiseStatus(stat)

        deleteFunc = _caps_delete if attr.contents.subtype == sType.USER else None

        return capsObj(attr, self.problemObj(), deleteFunction=deleteFunc)

#=============================================================================-
    def attrByIndex(self, index):
        """
        Retrieve an Attribute by index

        Parameters
        ----------
        index:
            the index (bias 1) to the list of Attributes
            
        Returns
        -------
            User Value Object
        """
        attr = c_capsObj()
        
        stat = _caps.caps_attrByIndex(self._obj, c_int(index), ctypes.byref(attr))
        if stat: _raiseStatus(stat)
        
        deleteFunc = _caps_delete if attr.contents.subtype == sType.USER else None

        return capsObj(attr, self.problemObj(), deleteFunction=deleteFunc)

#=============================================================================-
    def setAttr(self, attr, name=None):
        """
        Set an Attribute

        Parameters
        ----------
        attr:
            the Value Object containing the attribute
            The attribute will not maintain the Value Object's shape
            
        name:
            a string referring to the Attribute name - None: use name in attr
            Note: an existing Attribute of this name is overwritten with the new value
        """
        name = name.encode() if isinstance(name,str) else name

        stat = _caps.caps_setAttr(self._obj, name, attr._obj)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def deleteAttr(self, name):
        """
        Set an Attribute

        Parameters
        ----------
        name:
            a string referring to the Attribute to delete
            None deletes all attributes attached to the Object
        """
        name = name.encode() if isinstance(name,str) else name
       
        stat = _caps.caps_deleteAttr(self._obj, name)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def setOutLevel(self, outlevel=1):
        """
        Sets the CAPS verbosity level

        Parameters
        ----------
        outlevel: 0 <= outlevel <= 2
            0 - minimal, 1 - standard (default), 2 - debug

        Returns
        -------
        The old outLevel
        """
        stat = _caps.caps_outLevel(self._obj, c_int(outlevel))
        if stat < 0: _raiseStatus(stat)
        return stat

#=============================================================================-
    def bodyByIndex(self, index):
        """
        Retrived an EGADS body ego based on index

        Parameters
        ----------
        index:
            the index [1-size] - see caps_size

        Returns
        -------
        body:
            the returned EGADS Body Object
        
        units:
            the string declaring the length units - None for unitless values
        """
        body   = egads.c_ego()
        units  = c_char_p()
        stat = _caps.caps_bodyByIndex(self._obj, c_int(index), 
                                      ctypes.byref(body), ctypes.byref(units))
        if stat: _raiseStatus(stat)
        if units: units = _decode(units.value)
        return egads.c_to_py(body), units

#=============================================================================-
    def getBodies(self):
        """
        Retrived EGADS ego bodies from an Analysis Object

        Returns
        -------
        bodies:
            the returned list of EGADS ego Body Objects
        """
        nbody  = c_int()
        pbodies = POINTER(egads.c_ego)()
        units  = c_char_p()
        stat = _caps.caps_getBodies(self._obj, ctypes.byref(nbody), ctypes.byref(pbodies))
        if stat: _raiseStatus(stat)
        
        bodies = [egads.ego(egads.c_ego(pbodies[i].contents)) for i in range(nbody.value)]
        
        return bodies

#=============================================================================-
    def getRootPath(self):
        """
        Retrieve the problem root path

        Note: All other uses of path is relative to this point.
        
        Returns
        -------
            the root path
        """
        fullPath = c_char_p()
        stat = _caps.caps_getRootPath(self._obj, ctypes.byref(fullPath))
        if stat: _raiseStatus(stat)
        
        return _decode(fullPath.value)

#=============================================================================-
    def convertValue(self, inVal, inUnit):
        """
        Perform unit conversion

        Parameters
        ----------
        inVal:
            the input value to convert
            
        inUint:
            the units of the input value (may be None if self is a Value Object)
        
        Returns
        -------
            inVal in the units of the Value Object units
        """
        inUnit = inUnit.encode() if isinstance(inUnit, str) else inUnit

        outVal = c_double()
        stat = _caps.caps_convertValue(self._obj, c_double(inVal), inUnit, ctypes.byref(outVal))
        if stat: _raiseStatus(stat)
        
        return outVal.value

#=============================================================================-
    def reset(self):
        """
        Reset the problem. This is equivalent to a clean slate restart.
        """
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_reset(self._obj, ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def writeParameters(self, fileName):
        """
        This outputs an OpenCSM Design Parameter file.
        
        Parameters
        ----------
        fileName:
            the name of the parameter file to write
        """
        fileName = fileName.encode() if isinstance(fileName,str) else fileName

        stat = _caps.caps_writeParameters(self._obj, fileName)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def readParameters(self, fileName):
        """
        This reads an OpenCSM Design Parameter file and overwrites (makes dirty) 
        the current state for the GeometryIn Values in the file.
        
        Parameters
        ----------
        fileName:
            the name of the parameter file to read
        """
        fileName = fileName.encode() if isinstance(fileName,str) else fileName

        stat = _caps.caps_readParameters(self._obj, fileName)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def writeGeometry(self, fileName):
        """
        Writes all bodies in the Problem Object to a file
        
        Parameters
        ----------
        fileName:
            the name of the file to write - typed by extension (case insensitive): 
            iges/igs - IGES File
            step/stp - STEP File
            brep - OpenCASCADE File
            egads - EGADS file (which includes attribution)
        """
        fileName = fileName.encode() if isinstance(fileName,str) else fileName

        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_writeGeometry(self._obj, c_int(0), fileName, ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def setOwner(self, pname, lines, owner):
        """
        Sets owner data
        
        This increases the Problem's sequence number

        Parameters
        ----------
        pname:
            a pointer to the process name character string 

        lines:
            a list of strings with the description
            
        owner:
            a capsOwn structure
        """
        pname = pname.encode() if isinstance(pname,str) else pname
        
        nLines = len(lines)
        plines = (c_char_p*nLines)()
        for i in range(nLines):
            plines[i] = lines[i].encode() if isinstance(lines[i],str) else lines[i]

        stat = _caps.caps_setOwner(self._obj, pname, c_int(nLines), plines, owner._owner)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def makeValue(self, vname, stype, data):
        """
        Create a value object
        
        Parameters
        ----------
        vname:
            the Value Object name to be created

        stype:
            the Object subtype: Parameter or User
             
        data:
            the data (may be Units instances) to store in the Value Object
            if data is string and units is "PATH" - slashes are converted automatically

        Returns
        -------
            the CAPS Value Object
        """
        if isinstance(data, Units):
            units = data._units
            data  = data._value
        else:
            units = None

        vname = vname.encode() if isinstance(vname,str) else vname
        units = units.encode() if isinstance(units,str) else units

        vtype, nrow, ncol, pdata, partial = self._convertData(data)
 
        # Convert integers with units to doubles with units
        if vtype == vType.Integer and units is not None:
            tmp = (c_double*(nrow*ncol))()
            for i in range(nrow*ncol):
                tmp[i] = pdata[i]
            pdata = tmp
            vtype = vType.Double
 
        val = c_capsObj()
        stat = _caps.caps_makeValue(self._obj, vname, c_int(stype), c_int(vtype), 
                                    c_int(nrow), c_int(ncol), pdata, partial, units, ctypes.byref(val))
        if stat: _raiseStatus(stat)

        deleteFunc = _caps_delete if stype == sType.USER else None

        return capsObj(val, self.problemObj(), deleteFunction=deleteFunc)

#=============================================================================-
    def queryAnalysis(self, aname):
        """
        Query Analysis -- Does not 'load' or create an object
        
        Note: this causes the the DLL/Shared-Object to be loaded (if not already resident)

        Parameters
        ----------
        aname:
            the Value Object name to be created

        Returns
        -------
        nIn:
            the number of inputs
            
        nOut:
            the number of outputs
             
        execute:
            execution flag: 0 - no exec, 1 - AIM Execute exists
        """
        aname = aname.encode() if isinstance(aname,str) else aname
 
        nIn = c_int()
        nOut = c_int()
        execute = c_int()
        stat = _caps.caps_queryAnalysis(self._obj, aname, 
                                        ctypes.byref(nIn), ctypes.byref(nOut), 
                                        ctypes.byref(execute))
        if stat: _raiseStatus(stat)
        
        nIn = nIn.value
        nOut = nOut.value
        execute = execute.value

        return nIn, nOut, execute

#=============================================================================-
    def getInput(self):
        raise CAPSError(msg="Implement")

#=============================================================================-
    def getOutput(self):
        raise CAPSError(msg="Implement")

#=============================================================================-
    def makeAnalysis(self, aname, name, unitSys, intent=None, execute=2, 
                     # Legacy CAPS arguments
                     analysisDir=".", parents=None):
        """
        Load Analysis into the Problem

        Notes: 
            This causes the the DLL/Shared-Object to be loaded (if not already resident)

            If execute is 2 and the AIM has aimExecute, aimExecute automatically runs after preAnalysis
            and if the execution is not asynchronous aimPostAnalysis is automatically run. 
            Any errors can be retrieved via a call to checkAnalysis.

        Parameters
        ----------
        aname:
            the Analysis (and AIM plugin) name

        name:
            the unique supplied name for this instance (can be  None)

        unitSys: 
            string describing the unit system to be used by the AIM (can be None)
            see specific AIM documentation for a list of strings for which the AIM will respond

        intent: 
            the intent string used to pass Bodies to the AIM, None - no filtering

        execute: 
            the execution flag:  0 - no exec, 1 - AIM Execute performs analysis, 2 - Auto Exec

        Returns
        -------
            the Analysis Object
        """
        aname   = aname.encode()   if isinstance(aname  ,str) else aname
        unitSys = unitSys.encode() if isinstance(unitSys,str) else unitSys
        
        if isinstance(intent, (str, bytes)):
            intent = intent.encode() if isinstance(intent, str) else intent
        elif isinstance(intent, list):
            intent = b";".join( [i.encode() if isinstance(i ,str) else i for i in intent] )

        if intent == b"": intent = None 
        analysis = c_capsObj()
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()

        # EnCAPS
        #stat = _caps.caps_makeAnalysis(self._obj, aname, unitSys, intent,
        #                               c_int(execute), ctypes.byref(analysis), 
        #                               ctypes.byref(nErr), ctypes.byref(errs))
        #if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))
        # CAPS
        
        # Check to see if directory exists
        if not os.path.isdir(analysisDir):
            print("Analysis directory does not currently exist - it will be made automatically")
            os.makedirs(analysisDir)

        if parents is None:
            nparent = 0
            pparents = None
        else:
            if not isinstance(parents, list): parents = [parents]
            nparent = len(parents)
            pparents = (c_capsObj*nparent)()
            for i in range(nparent):
                pparents[i] = parents[i]._obj
        analysisDir = analysisDir.encode() if isinstance(analysisDir,str) else analysisDir
        stat = _caps.caps_makeAnalysis(self._obj, aname, analysisDir, unitSys, intent,
                                       c_int(nparent), pparents, ctypes.byref(analysis))
        if stat: _raiseStatus(stat, msg="Failed to load AIM: " + _decode(aname))
        # end CAPS
        
        return capsObj(analysis, self.problemObj(), None)

#=============================================================================-
    def dirtyAnalysis(self):
        """
        Resturns the list of dirty Analysis Objects
        """

        nAobj = c_int()
        aobjs = POINTER(c_capsObj())
        stat = _caps.caps_dirtyAnalysis(self._obj, ctypes.byref(aobjs), ctypes.byref(aobjs))
        if stat: _raiseStatus(stat)

        anlysis = [Analysis(c_capsObj(aobjs[i].contents)) for i in range(nAobj.value)]
 
        return anlysis

#=============================================================================-
    def makeBound(self, dim, bname):
        """
        Creates an Bound Object

        Parameters
        ----------
        dim:
              the dimensionality of the Bound (1 - 3)
              
        bname:
             string associated with "capsBound" attribute on bodies

        Returns
        -------
            the Bound Object
        """
        bname = bname.encode() if isinstance(bname ,str) else bname

        bound = c_capsObj()
        stat = _caps.caps_makeBound(self._obj, c_int(dim), bname, ctypes.byref(bound))
        if stat: _raiseStatus(stat)

        return capsObj(bound, self.problemObj(), deleteFunction=_caps_delete)

#=============================================================================-
    def getValue(self):
        """
        Returns the data in the Value Object
        
        Returns
        ----------
        data:
            the data stored in the Value Object
            
        units:
            pointer to the string declaring the units
        """
        vtype   = c_int()
        nrow    = c_int()
        ncol    = c_int()
        partial = POINTER(c_int)()
        pdata   = c_void_p()
        units   = c_char_p()
        nErr    = c_int()
        errs    = POINTER(c_capsErrs)()

        stat = _caps.caps_getValue(self._obj, ctypes.byref(vtype), 
                                   ctypes.byref(nrow), ctypes.byref(ncol), 
                                   ctypes.byref(pdata), ctypes.byref(partial), 
                                   ctypes.byref(units), ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))
        
        vtype = vtype.value
        nrow = nrow.value
        ncol = ncol.value
        units = _decode(units.value) if units else None

        if not pdata:
            return _withUnits(None, units)

        if vtype == vType.String:
            
            #if nrow*ncol == 1:
            pdata = ctypes.cast(pdata, c_char_p)
            data = _decode(pdata.value)
            return _withUnits(data, units)

            #pdata = ctypes.cast(pdata, POINTER(c_char_p))

            #data = [None]*nrow
            #for i in range(nrow):
            #    data[i] = [None]*ncol
            #    for j in range(ncol):
            #        data[i][j] = _decode(pdata[i*ncol+j])

        if vtype == vType.Boolean or \
           vtype == vType.Integer or \
           vtype == vType.Double  or \
           vtype == vType.DoubleDot:
            
            if vtype == vType.Double or vtype == vType.DoubleDot:
                pdata = ctypes.cast(pdata, ctypes.POINTER(c_double))
            else:
                pdata = ctypes.cast(pdata, ctypes.POINTER(c_int))
        
            data = [None]*nrow
            for i in range(nrow):
                data[i] = [None]*ncol
                for j in range(ncol):
                    data[i][j] = pdata[i*ncol+j]

        elif vtype == vType.Tuple:
            pdata = ctypes.cast(pdata, ctypes.POINTER(c_capsTuple))
            data = [None]*nrow
            for i in range(nrow):
                data[i] = [None]*ncol
                for j in range(ncol):
                    
                    name = _decode(pdata[i*ncol+j].name)
                    valu = _decode(pdata[i*ncol+j].value)
                    try:
                        valu = json.loads( valu )
                    except:
                        pass

                    data[i][j] = (name, valu)

        if nrow == 1 and ncol == 1:
            data = data[0][0]
        elif ncol == 1:
            for i in range(nrow):
                data[i] = data[i][0]
        elif nrow == 1:
            for i in range(nrow):
                data = data[0][:]

        return _withUnits(data, units)

#=============================================================================-
    # Determine the equivalent CAPS value object type for a Python object     
    def _getType(self, data):
    
        # If we have a list loop back in to see what type of value the element is 
        if isinstance(data, list):
            vtype = self._getType(data[0])
            for i in range(1,len(data)):
                # Allow convertsion from Integer to Double
                vtypei = self._getType(data[i])
                if vtype == vType.Integer and vtypei == vType.Double:
                    vtype = vType.Double
                if vtypei == vType.Integer and vtype == vType.Double:
                    vtypei = vType.Double
                if vtype != vtypei:
                    raise CAPSError(CAPS_BADVALUE, "List entries must all be same type!")
            return vtype

        if isinstance(data, bool) and (str(data) == 'True' or str(data) == 'False'):
            return vType.Boolean
        
        if isinstance(data, int):
            return vType.Integer
        
        if isinstance(data, float):
            return vType.Double
        
        if isinstance(data, tuple):
            return vType.Tuple

        if isinstance(data, tuple):
            return vType.Tuple

        if isinstance(data, str):
            return vType.String

        # dictionaries are converted to json strings
        if isinstance(data, dict):
            return vType.Dict

        if data is None:
            return vType.Null

        raise CAPSError(CAPS_BADVALUE, "Unsupported data type!")

#=============================================================================-
    def _getShape(self, data):
        
        nrow = 0
        ncol = 0
        
        if not isinstance(data, list):
            data = [data] # Convert to list
                
        nrow = len(data)
            
        if isinstance(data[0], list):
            ncol = len(data[0]) 
        else:
            ncol = 1
            
        # Make sure shape is consistent
        if ncol != 1:
            for i in range(nrow):
                if len(data[i]) != ncol:
                    raise CAPSError(CAPS_BADVALUE, "Inconsistent list sizes!")
        
        return nrow, ncol

#=============================================================================-
    def _convertData(self, data):

        vtype = self._getType(data)
        
        # Convert data to list 
        if not isinstance(data, list):
            data = [data] # Convert to list
        
        nrow, ncol = self._getShape(data)
            
        length = nrow*ncol
        partial = None

        # Boolean, Integer, or Double
        if vtype == vType.Boolean or vtype == vType.Integer or vtype == vType.Double:
            
            partial = (enum_capsNull*(length))(Null.NotNull)
            isPartial = False

            if vtype == vType.Double:
                val = (c_double*length)()
            else:
                val = (c_int*length)()
        
            for i in range(nrow):
                for j in range(ncol):
                    if ncol == 1:
                        if data[i] is None:
                            partial[i*ncol+j] = Null.IsNull
                            val[i*ncol+j] = 0.0
                            isPartial = True
                            continue
                        else:
                            val[i*ncol+j] = data[i]
                    else:
                        if data[i][j] is None:
                            partial[i*ncol+j] = Null.IsNull
                            val[i*ncol+j] = 0.0
                            isPartial = True
                            continue
                        else:
                            val[i*ncol+j] = data[i][j]

            if not isPartial: partial = None

        # String
        elif vtype == vType.String:
            if nrow*ncol != 1:
                raise CAPSError(CAPS_BADVALUE, msg="Lists of strings are not supported")
            
            val = c_char_p()
            val = data[0].encode() if isinstance(data[0], str) else data[0]
       
        # Tuple
        elif vtype == vType.Tuple:
    
            val = (c_capsTuple*length)()

            for i in range(nrow):
               
                if (isinstance(data[i][1], bytes) or 
                    isinstance(data[i][1], str)  or 
                    isinstance(data[i][1], int)  or 
                    isinstance(data[i][1], float)):
                    
                    if isinstance(data[i][1], int)  or isinstance(data[i][1], float):
                        temp = str(data[i][1])
                    else:
                        temp = data[i][1] 
                else:
                    temp = json.dumps(data[i][1])

                byteName = data[i][0].encode() if isinstance(data[i][0], str) else data[i][0]
                byteString = temp.encode() if isinstance(temp, str) else temp

                val[i].name  = byteName
                val[i].value = byteString
                
        # Dict
        elif vtype == vType.Dict:
            val = ';'.join( [ json.dumps(i) for i in data ] )
            val = val.encode() if isinstance(val, str) else val
            vtype = vType.String

        # Null (None)
        elif vtype == vType.Null:
            val = None

        # Unknown type
        else:
            raise CAPSError(CAPS_BADVALUE, "Can not convert Python object to type: " + str(vtype))

        return vtype, nrow, ncol, val, partial

#=============================================================================-
    def setValue(self, data):
        """
        Set data in the Value object
        
        Parameters
        ----------
        data:
            data to store in the Value Object
            
        units:
            pointer to the string declaring the units - None for unitless values
            if data is string and units is "PATH" - slashes are converted automatically
        """
        if isinstance(data,Units):
            units = data._units
            data  = data._value
        else:
            units = None

        units = units.encode() if isinstance(units,str) else units
        if units is not None and not isinstance(units, bytes):
            raise CAPSError(CAPS_UNITERR, "units must be a string: units = {!r}".format(units))
 
        vtype, nrow, ncol, pdata, partial = self._convertData(data)

        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_setValue(self._obj, c_int(vtype), 
                                   c_int(nrow), c_int(ncol), 
                                   pdata, partial, units, ctypes.byref(nErr), ctypes.byref(errs))
        if stat:
            name, otype, stype, link, parent, last = self.info()
            _raiseStatus(stat, msg="Trying to set value for: {!r}".format(name), errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def getLimits(self):
        """
        Returns the value limits
        
        Returns
        -------
        limits:
            list of length 2 or None if not set 

        units:
            units of the limits or None
        """
        vtype = c_int()
        plimits = c_void_p()
        punits = c_char_p()

        stat = _caps.caps_getLimits(self._obj, ctypes.byref(vtype), ctypes.byref(plimits), ctypes.byref(punits))
        if stat: _raiseStatus(stat)
        
        if not plimits:
            return None
        
        vtype = vtype.value
        units = _decode(punits.value) if punits else None

        if vtype == vType.Integer:
            limits = list(ctypes.cast(plimits, ctypes.POINTER(c_int))[0:2])
        elif vtype == vType.Double:
            limits = list(ctypes.cast(plimits, ctypes.POINTER(c_double))[0:2])
        else:
            raise CAPSError(CAPS_BADVALUE, CAPSError.InternalError+"Unknown vtype: " + str(vtype))

        return _withUnits(limits, units)

#=============================================================================-
    def setLimits(self, limits):
        """
        Set the value limits
        
        Paramters
        -------
        limits:
            list (or Units) of length 2 of int or float
        """
        if isinstance(limits, Units):
            units  = limits._units
            limits = limits._value
        else:
            units = None
        
        if limits is None:
            vtype = 0
            plimits = None
        else:
            if not (isinstance(limits, list) or \
                    isinstance(limits, tuple)) or \
               len(limits) != 2:
                raise CAPSError(CAPS_BADVALUE, "limits should be 2 element list - [min value, max value]!")
            
            for i in limits:
                if not isinstance(i, (int, float)):
                    raise CAPSError(CAPS_BADVALUE, "Invalid element type for limits value, only int or float values are valid!")

            vtype = self._getType(limits)
           
            if vtype == vType.Integer:
                plimits = (c_int*2)()
            elif vtype == vType.Double:
                plimits = (c_double*2)()
            else:
                raise CAPSError(CAPS_BADVALUE, CAPSError.InternalError+"Unknown vtype: " + str(vtype))

            plimits[:2] = limits[:]
            plimits = ctypes.cast(plimits, ctypes.c_void_p)
            
        units = units.encode() if isinstance(units, str) else units
        if units is not None and not isinstance(units, bytes):
            raise CAPSError(CAPS_UNITERR, "units must be a string: units = {!r}".format(units))

        stat = _caps.caps_setLimits(self._obj, c_int(vtype), plimits, units)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def getValueProps(self):
        """
        Returns the value properties
        
        Returns
        -------
        dim:
            the returned dimensionality:
            0 - scalar only, 1 - vector or scalar, 2 - scalar, vector, or 2D array

        pmtr:
            the returned flag: 0 - normal, 1 - GeometryIn type -> OCSM_CFGPMTR
            
        lfix:
            0 - the length(s) can change, 1 -- the length is fixed
            
        sfix:
            0 - the Shape can change, 1 - Shape is fixed

        ntype:
            0 - None invalid, 1 - not None, 2 - is None, 3 - partial None
        """
        dim   = c_int()
        pmtr  = c_int()
        lfix  = c_int()
        sfix  = c_int()
        ntype = c_int()

        stat = _caps.caps_getValueProps(self._obj, ctypes.byref(dim), ctypes.byref(pmtr), 
                                        ctypes.byref(lfix), ctypes.byref(sfix), ctypes.byref(ntype))
        if stat: _raiseStatus(stat)
        
        dim   = dim.value
        pmtr  = pmtr.value
        lfix  = lfix.value
        sfix  = sfix.value
        ntype = ntype.value

        return dim, pmtr, lfix, sfix, ntype

#=============================================================================-
    def setValueProps(self, dim, lfix, sfix, ntype):
        """
        Sets the value properties (only for the User & Parameter subtypes) 
        
        Parameters
        -------
        dim:
            the dimensionality:
            0 - scalar only, 1 - vector or scalar, 2 - scalar, vector, or 2D array
            
        lfix:
            0 - the length(s) can change, 1 -- the length is fixed
            
        sfix:
            0 - the Shape can change, 1 - Shape is fixed

        ntype:
            0 - None invalid, 1 - not None, 2 - is None, 3 - partial None
        """
        stat = _caps.caps_setValueProps(self._obj, c_int(dim),
                                        c_int(lfix), c_int(sfix), c_int(ntype))
        if stat: _raiseStatus(stat)

#=============================================================================-
    def transferValues(self, tmethod, dst):
        """
        Transfers values from Value Object (not for AIM pointer or Tuple vtypes) - or - DataSet Object
        to a destination Value Object
        
        Parameters
        ----------
        tmethod:
            0 - copy, 1 - integrate, 2 - weighted average  -- (1 & 2 only for DataSet self)
            
        dst:
            the destination Value Object to receive the data
            Notes: 
                Must not be GeometryOut or AnalysisOut
                Shapes must be compatible
                Overwrites any Linkage
        """
        if not isinstance(dst, Value):
            raise CAPSError(CAPS_BADVALUE, "dst must be of type caps.Value")

        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()

        stat = _caps.caps_transferValues(self._obj, c_int(tmethod), dst._obj, 
                                         ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def makeLinkage(self, tmethod, trgt, irow=0, icol=0):
        """
        Links values from Value Object (not for Tuple vtype or Value subtype User)  - or - DataSet Object
        to a target Value Object
        
        Note: circular linkages are not allowed!
        
        Parameters
        ----------
        tmethod:
            0 - copy, 1 - integrate, 2 - weighted average  -- (1 & 2 only for DataSet self)
            
        trgt:
            the target Value Object which will the data
            Notes: 
                Must not be GeometryOut or AnalysisOut
                (Sub)shapes must be compatible
                
        irow:
            the row to link in the target (or 0 for all)

        icol:
            the column to link in the target (or 0 for all)
            if both irow and icol are 0 the entire Object is linked
        """
        if not isinstance(trgt, capsObj):
            raise CAPSError(CAPS_BADVALUE, "trgt must be of type capsObj")

        if trgt._obj.contents.type != oType.VALUE:
            raise CAPSError(CAPS_BADVALUE, "trgt must be a Value Object")

        stat = _caps.caps_makeLinkage(self._obj, c_int(tmethod), trgt._obj, 
                                      c_int(irow), c_int(icol))
        if stat: _raiseStatus(stat)

#=============================================================================-
    def removeLinkage(self):
        """
        Remove a link to this Value Object
        """
        stat = _caps.caps_makeLinkage(None, c_int(0), self._obj, 
                                      c_int(0), c_int(0))
        if stat: _raiseStatus(stat)

#=============================================================================-
    def hasDot(self):
        """
        Get a list of the Derivatives available
        
        Only for GeometryOut/AnalysisOut or Double /w Dot Value Objects
        
        Returns
        -------
            the list of derivative names
            derivatives derived from vectors/arrays will have "[n]" or "[n,m]" appended
        """
        
        ndot = c_int()
        pnames = POINTER(c_char_p)()
        
        stat = _caps.caps_hasDot(self._obj, ctypes.byref(ndot), ctypes.byref(pnames))
        if stat: _raiseStatus(stat)
        
        if not pnames:
            return None
        
        ndot = ndot.value
        names = [None]*ndot
        for i in range(ndot):
            names[i] = _decode(pnames[i])
        
        egads.free(pnames)
        
        return names

#=============================================================================-
    def getDot(self, name):
        """
        Get Derivative values
        
        Only for GeometryOut/AnalysisOut or Double /w Dot Value Objects
        
        Parameters
        ----------
        name:
            the name of the derivative

        Returns
        -------
            list of values (tuples if rank > 1) of sensitvities
        """
        
        name = name.encode() if isinstance(name,str) else name
        len = c_int()
        rank = c_int()
        pdot = POINTER(c_double)()
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()

        stat = _caps.caps_getDot(self._obj, name, ctypes.byref(len), ctypes.byref(rank), ctypes.byref(pdot),
                                 ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))
        
        len = len.value
        rank = rank.value
        
        if len == 0:
            return None
        
        if rank == 1:
            if len == 1:
                dot = pdot[0]
            else:
                dot = pdot[0:len]
        else:
            dot = [None]*ndot
            for i in range(len):
                dot[i] = tuple(dot[i*rank:(i+1)*rank])
        
        return dot

#=============================================================================-
    def sensitivity(self, irow, icol, funFlag):
        """
        Compute geometric sensitvities
        
        Side effects:
            Makes a slot in all GeometryOut Value Objects to store the results with regFlag of 0
            
            Invokes all AIMs with a defined aimSensitivity function when called with regFlag of 1
            
            This function (regFlag is 1), Value.getDot or a DataSet request for the sensitivity will 
            fill the appropriate slots in the DataSets and GeometryOuts
            
            Any GeometryOut that is a function of the mass properties will have its value slightly changed
            (to be consistent with the derivative calculation)

        Parameters
        ----------
        irow:
            the input row to use (for vector/arrays)

        icol:
            the input column to use (for vector/array Value Objects)

        funFlag:
            0 register slot, 1 call AIMs, 2 remove slot
        """
        len = c_int()
        rank = c_int()
        pdot = POINTER(c_double)()
        
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()

        stat = _caps.caps_sensitivity(self._obj, c_int(irow), c_int(icol), c_int(funFlag),
                                      ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def dupAnalysis(self, analysisDir, parents):
        """
        Initializeds a new analysis based on this Analysis Object
        """
        analysis = c_capsObj()
        # EnCAPS
        #stat = _caps.caps_dupAnalysis(self._obj, ctypes.byref(analysis))
        # CAPS
        if parents is None:
            nparents = 0
            pparents = None
        else:
            nparents = len(parents)
            pparents = (c_capsObj*nparents)()
            for i in range(nparents):
                pparents[i] = parents._obj
        
        analysisDir = analysisDir.encode() if isinstance(analysisDir,str) else analysisDir
        stat = _caps.caps_dupAnalysis(self._obj, analysisDir, c_int(nparents), pparents, ctypes.byref(analysis))
        # end CAPS
        if stat: _raiseStatus(stat)

        return capsObj(analysis, self.problemObj(), None)

#=============================================================================-
    def dirtyAnalysis(self):
        """
        Resturns the list of dirty Analysis Objects
        """

        nAobj = c_int()
        aobjs = POINTER(c_capsObj())
        stat = _caps.caps_dirtyAnalysis(self._obj, ctypes.byref(aobjs), ctypes.byref(aobjs))
        if stat: _raiseStatus(stat)

        anlysis = [capsObj(c_capsObj(aobjs[i].contents), self.problemObj(), None) for i in range(nAobj.value)]
 
        return anlysis

#=============================================================================-
    def analysisInfo(self):
        """
        Get information about the Anaysis Object

        Returns
        -------
        dir:
            string specifying the directory for file I/O

        unitSys:
            string describing the unit system used by the AIM (can be None)
            
        major:
            the returned AIM major version number
            
        minor:
            the returned AIM minor version number
            
        intent:
            the intent string used to pass Bodies to the AIM
            
        fnames:
            a list of strings with the field/DataSet names
            
        ranks:
            a list of ranks associated with each field
            
        execute:
            execution flag: 0 - no exec, 1 - AIM Execute runs analysis, 2 - Auto Exec

        status:
            0 - up to date, 1 - dirty Analysis inputs, 2 - dirty Geometry inputs
            3 - both Geometry & Analysis inputs are dirty, 4 - new geometry,
            5 - post Analysis required, 6 -- Execution & post Analysis required
        """ 
        dir     = c_char_p()
        unitSys = c_char_p()
        major   = c_int()
        minor   = c_int()
        intent  = c_char_p()
        nfields = c_int()
        pfnames = POINTER(c_char_p)()
        pranks  = POINTER(c_int)()
        execute = c_int()
        status  = c_int()
        # EnCAPS
#         stat = _caps.caps_analysisInfo(self._obj, ctypes.byref(dir), ctypes.byref(unitSys), 
#                                         ctypes.byref(major), ctypes.byref(minor), ctypes.byref(intent), 
#                                         ctypes.byref(nfields), ctypes.byref(pfnames), ctypes.byref(pranks), 
#                                         ctypes.byref(execute), ctypes.byref(status))
#         if stat: _raiseStatus(stat)
#         
#         dir     = _decode(dir.value)
#         unitSys = _decode(unitSys.value) if unitSys else None
#         major   = major.value
#         minor   = minor.value
#         intent  = _decode(intent.value) if intent and intent.value != b"" else None
#         nfields = nfields.value
#         fnames  = [_decode(pfnames[i]) for i in range(nfields)]
#         ranks   = [pranks[i]           for i in range(nfields)]
#         execute = execute.value
#         status  = status.value
#         
#         if intent is not None and ";" in intent: intent = intent.split(";")
#
#         return dir, unitSys, major, minor, intent, fnames, ranks, execute, status

        # CAPS
        nParent = c_int()
        parents = POINTER(c_capsObj)()
        stat = _caps.caps_analysisInfo(self._obj, 
                                       ctypes.byref(dir), 
                                       ctypes.byref(unitSys), 
                                       ctypes.byref(intent), 
                                       ctypes.byref(nParent), 
                                       ctypes.byref(parents), 
                                       ctypes.byref(nfields), 
                                       ctypes.byref(pfnames),
                                       ctypes.byref(pranks), 
                                       ctypes.byref(execute), 
                                       ctypes.byref(status))
        if stat: _raiseStatus(stat)

        dir     = _decode(dir.value)
        unitSys = _decode(unitSys.value) if unitSys else None
        nParent = nParent.value
        parents = [capsObj(c_capsObj(parents[i].contents), self.problemObj(), None) for i in range(nParent)]
        intent  = _decode(intent.value) if intent and intent.value != b"" else None
        nfields = nfields.value
        fnames  = [_decode(pfnames[i]) for i in range(nfields)]
        ranks   = [pranks[i]           for i in range(nfields)]
        execute = execute.value
        status  = status.value
        
        if intent is not None and ";" in intent: intent = intent.split(";")

        return dir, unitSys, intent, parents, fnames, ranks, execute, status
        # end CAPS

#=============================================================================-
    def preAnalysis(self):
        """
        Generate Analysis Inputs
        """
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_preAnalysis(self._obj, ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def runAnalysis(self):
        """
        Execute Analysis if AIM does execution or AutoExec
        
        Returns
        -------
        integer status (0 - done, 1 - running)
        """
        status = c_int()
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_runAnalysis(self._obj, ctypes.byref(status), ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))
        
        return status.value

#=============================================================================-
    def postAnalysis(self):
        """
        Mark Analysis as run
        
        Note: this clears all Analysis Output Objects to force reloads/recomputes
        """
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        # EnCAPS
        #stat = _caps.caps_postAnalysis(self._obj, ctypes.byref(nErr), ctypes.byref(errs))
        # CAPS
        name, otype, stype, link, parent, last = self.info()
        name, otype, stype, link, parent, last = parent.info()
        stat = _caps.caps_postAnalysis(self._obj, last._owner, ctypes.byref(nErr), ctypes.byref(errs))
        # end CAPS
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def checkAnalysis(self):
        """
        Has the analysis completed?
        
        Returns
        -------
        status:
            CAPS_SUCCESS - completed, CAPS_RUNNING - not done
            
        phase:
            the returned phase where errors were generated:
            0 - no errors, 1 - during the check, 2 - execution, 3 - post (if automatic)
            
        errors:
            caps.capsErrs object with error information
        """
        phase = c_int()
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_checkAnalysis(self._obj, ctypes.byref(phase), ctypes.byref(nErr), ctypes.byref(errs))
        if stat < 0: _raiseStatus(stat)
        
        return stat, phase, capsErrs(nErr, errs, self._obj)

#=============================================================================-
    def writeGeometry(self, fileName):
        """
        Writes all bodies in the Analysis Object to a file
        
        Parameters
        ----------
        fileName:
            the name of the file to write - typed by extension (case insensitive): 
            iges/igs - IGES File
            step/stp - STEP File
            brep - OpenCASCADE File
            egads - EGADS file (which includes attribution)
        """
        fileName = fileName.encode() if isinstance(fileName,str) else fileName

        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_writeGeometry(self._obj, c_int(0), fileName, ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def boundInfo(self):
        """
        Get information about the Bound Object

        Returns
        -------
        state:
            the Bound state: 
            -1 - Open, 0 - Empty & Closed, 1 - single BRep entity, 
            2 - multiple BRep entities, -2 - multiple BRep entities with Errors in reparameterization

        dim:
            the dimensiounalty of the Bound (1 - 3
            
        plims:
            the parametrization limits  (2 values when dim is 1, 4 when dim is 2)
        """ 
        state   = c_int()
        dim     = c_int()
        plims  = POINTER(c_double)()
        stat = _caps.caps_boundInfo(self._obj, ctypes.byref(state), 
                                    ctypes.byref(dim), ctypes.byref(plims))
        if stat: _raiseStatus(stat)
        
        state   = state.value
        dim     = dim.value
        plims   = plims[:2*dim]
        
        return state, dim, plims

#=============================================================================-
    def completeBound(self):
        """
        Completes the Bound Object
        """ 
        stat = _caps.caps_completeBound(self._obj)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def makeVertexSet(self, analysis, vname=None):
        """
        Create a VertexSet on the Bound Object

        Parameters
        ----------
        analysis:
            the Analysis Object (None - Unconnected) 

        vname:
            string naming the VertexSet (can be None for a Connected VertexSet)
        
        Returns
        -------
        the new VertexSet Object
        """ 
        vname = vname.encode() if isinstance(vname,str) else vname

        vset  = c_capsObj()
        stat = _caps.caps_makeVertexSet(self._obj, analysis._obj, 
                                        vname, ctypes.byref(vset))
        if stat: _raiseStatus(stat)
        
        return capsObj(vset, self.problemObj(), None)

#=============================================================================-
    def fillVertexSets(self):
        """
        Fill VertexSets for cyclic/incremental transfers
        
        Note: Causes the filling of the VertexSets owned by the Bound 
        by forcing the invocation of the appropriate aimDiscr functions 
        in the AIM. Under normal circumstances this is deferred to the 
        last postAnalysis call of the collected VertexSets.
        """
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_fillVertexSets(self._obj, ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))

#=============================================================================-
    def getDataSets(self, dname):
        """
        Get DataSet Objects by name
        
        Parameters
        ----------
        dname:
            string containing the name of the DataSet 
        """
        dname = dname.encode() if isinstance(dname, str) else dname
        nobj  = c_int()
        dsets = POINTER(c_capsObj())
        stat = _caps.caps_getDataSets(self._obj, dname, ctypes.byref(nobj), ctypes.byref(dsets))
        if stat: _raiseStatus(stat)
        
        datasets = [DataSet(c_capsObj(dsets[i].contents)) for i in range(nobj.value)]
        egads.free(dsets)
        
        return datasets

#=============================================================================-
    def triangulate(self):
        """
        Get Triangulations for a 2D VertexSet
        
        Returns
        -------
        Gtris: 
            a list of indices (bias 1) referencing Geometry-based points
    
        Dtris:
            a list of indices (bias 1) referencing Data-based points
        """
        nGtris  = c_int()
        pGtris  = POINTER(c_int)()
        nDtris  = c_int()
        pDtris  = POINTER(c_int)()
        stat = _caps.caps_triangulate(self._obj, ctypes.byref(nGtris), ctypes.byref(pGtris), 
                                                 ctypes.byref(nDtris), ctypes.byref(pDtris))
        if stat: _raiseStatus(stat)
        
        Gtris = None
        if pGtris:
            Gtris = [tuple(pGtris[3*i:3*(i+1)]) for i in range(nGtris.value)]
        
        Dtris = None
        if pDtris:
            Dtris = [tuple(pGtris[3*i:3*(i+1)]) for i in range(nDtris.value)]

        egads.free(pGtris)
        egads.free(pDtris)
        
        return Gtris, Dtris

#=============================================================================-
    def vertexSetInfo(self):
        """
        Get information about the VertexSet Object

        Returns
        -------
        nGpts: 
            the returned number of Geometry points in the VertexSet
            
        nDpts:
            the returned number of point Data positions in the VertexSet
            
        bound:
            the returned associated Bound Object
            
        analysis:
            the returned associated Analysis Object (None -- Unconnected)
        """ 
        nGpts    = c_int()
        nDpts    = c_int()
        bound    = c_capsObj()
        analysis = c_capsObj()
        stat = _caps.caps_vertexSetInfo(self._obj, ctypes.byref(nGpts), ctypes.byref(nDpts), 
                                        ctypes.byref(bound), ctypes.byref(analysis))
        if stat: _raiseStatus(stat)
        
        nGpts    = nGpts.value
        nDpts    = nDpts.value
        bound    = Bound(bound, deleteObject=False)
        analysis = Analysis(analysis, deleteObject=False) if analysis else None
        
        return nGpts, nDpts, bound, analysis

#=============================================================================-
    def fillUnVertexSet(self, xyzs):
        """
        Fill an Unconnected VertexSet
        
        Parameters
        ----------
        xyzs:
            list of 3 valued tuple point coorediantes
        """
        npts = len(xyzs)
        pxyzs = (c_double*(3*npts))()
        for i in range(npts):
            pxyzs[3*i:3*(i+1)] = xyzs[i][:]
        
        stat = _caps.caps_fillUnVertexSets(self._obj, c_int(npts), pxyzs)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def makeDataSet(self, dname, method, rank):
        """
        Create a DataSet Object -- associated Bound must be Open
        
        Parameters
        ----------
        dname:
            a string containing the name of the DataSet (i.e., "pressure")
        
        method:
            the method used for data transfers: (Sensitivity, Analysis, Interpolate, Conserve, User)
            
        rank:
            the rank of the data (e.g., 1 -- scalar, 3 -- vector)
        """
        dname = dname.encode() if isinstance(dname,str) else dname

        dset = c_capsObj()
        stat = _caps.caps_makeDataSet(self._obj, dname, c_int(method), c_int(rank), ctypes.byref(dset))
        if stat: _raiseStatus(stat)

        return capsObj(dset, self.problemObj(), None)

#=============================================================================-
    def initDataSet(self, startup):
        """
        Initialize DataSet for cyclic/incremental startup

        Note: invocations of caps_getData and aim_getDataSet will 
        return this data (and a length of 1) until properly filled.
        
        Parameters
        ----------
        startup:
            the constant startup data (scalar or tuple)
        """
        rank = 1 if isinstance(startup,float) else len(startup)
        pstartup = (c_double*rank)()
        pstartup[:] = startup
        
        stat = _caps.caps_initDataSet(self._obj, c_int(rank), pstartup)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def getData(self):
        """
        Get data from the DataSet

        Returns
        -------
        data:
            the data (rank long tuples list, npts in legnth)
            
        units:
            string declaring the units
        """ 
        npt   = c_int()
        rank  = c_int()
        pdata = POINTER(c_double)()
        units = c_char_p()
        
        nErr = c_int()
        errs = ctypes.POINTER(c_capsErrs)()
        stat = _caps.caps_getData(self._obj, ctypes.byref(npt), ctypes.byref(rank), 
                                  ctypes.byref(pdata), ctypes.byref(units), 
                                  ctypes.byref(nErr), ctypes.byref(errs))
        if stat: _raiseStatus(stat, errors=capsErrs(nErr, errs, self._obj))
        
        npt    = npt.value
        rank   = rank.value
        data   = [tuple(pdata[rank*i:rank*(i+1)]) for i in range(npt)]
        units  = _decode(units.value) if units else None
        
        return _withUnits(data, units)

#=============================================================================-
    def setData(self, data):
        """
        Get data from the DataSet

        Parameters
        ----------
        data:
            the data (or Units) (rank long tuples list, npts in legnth)
        """ 
        if isinstance(data,Units):
            units = data._units
            data  = data._value
        else:
            units = None
        
        npt   = len(data)
        rank  = 1 if isinstance(data[0],float) else len(data[0])
        pdata = POINTER(c_double*(rank*npt))()
        units = units.encode() if isinstance(units, str) else units
        
        for i in range(npt):
            pdata[rank*i:rank*(i+1)] = data[i]
        
        stat = _caps.caps_setData(self._obj, c_int(npt), c_int(rank), 
                                  pdata, units)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def fillUnVertexSet(self, xyzs):
        """
        Fill an Unconnected VertexSet
        
        Parameters
        ----------
        xyzs:
            list of 3 valued tuple point coorediantes
        """
        npts = len(xyzs)
        pxyzs = (c_double*(3*npts))()
        for i in range(npts):
            pxyzs[3*i:3*(i+1)] = xyzs[i][:]
        
        stat = _caps.caps_fillUnVertexSets(self._obj, c_int(npts), pxyzs)
        if stat: _raiseStatus(stat)

#=============================================================================-
    def AIMbackdoor(self, JSONin):
        
        JSONin = JSONin.encode() if isinstance(JSONin, str) else JSONin
        
        pJSONout = c_char_p()
        stat = _caps.caps_AIMbackdoor(self._obj, JSONin, ctypes.byref(pJSONout))
        if stat: _raiseStatus(stat)
        
        JSONout = _decode(pJSONout.value)
        egads.free(pJSONout)

        return JSONout

#=============================================================================-
    def save(self, filename):
        
        filename = filename.encode() if isinstance(filename, str) else filename
        
        stat = _caps.caps_save(self._obj, filename)
        if stat: _raiseStatus(stat)


#=============================================================================-
# Helper function to return Unit class or values
def _withUnits(value, units):
    if units is None:
        return value
    else:
        return Units(value, units)

#=============================================================================-
class Units(object):
    """
    Represents a Python value object with units
    """

    #
    # Make Units dominant when multiplying with a numpy.array for left or right
    #
    __array_priority__ = 20.0

#=============================================================================-
    def __init__(self, value, units):
        """
        Parameters
        ----------
        value:
            the scalar value

        units:
            the units of value
        """
        self._value = value
        self._units = units

#=============================================================================-
    def convert(self, toUnits):
        """
        Convertes to other units
        
        Parameters
        ----------
        toUnits:
            Units to convert to
        
        Retunrs
        -------
        value converted to toUnits
        """
        tounits = toUnits.encode() if isinstance(toUnits, str) else toUnits

        scale = c_double()
        stat = _caps.caps_convert(self._units.encode(), c_double(1), 
                                  tounits, ctypes.byref(scale))
        if stat: _raiseStatus(stat, "Cannot convert {!r} to {!r}".format(self, toUnits))
        scale = scale.value
        
        if hasattr(self._value, '__len__'):
            value = [None]*len(self._value)
            for i in range(len(self._value)):
                value[i] = scale*self._value[i]
        else:
            value = scale*self._value

        return Units(value, toUnits)

#=============================================================================-
    def __hash__(self):
        return hash( (self.__class__.__name__, self._value, self._units) )

#=============================================================================-
    def __repr__(self):
        '''x.__repr__() <==> repr(x)
        '''
        return '<{0} {1}>'.format(self.__class__.__name__, self)

#=============================================================================-
    def __str__(self):
        '''x.__str__() <==> str(x)
        '''
        return str(self._value) + ' ' + self._units

#=============================================================================-
    def __copy__(self):
        return Unit(self._value, self._unit)

    def __deepcopy__(self, memo):
        '''Used if copy.deepcopy is called on the variable.
        '''
        other = self.__copy__()
        memo[id(self)] = other
        return other

#=============================================================================-
    def __bool__(self):
        '''Truth value testing and the built-in operation ``bool``

            x.__bool__() <==> x != 0

        '''
        return bool(self._value)

#=============================================================================-
    def __float__(self): # conversion to float (makes trig functions work)
        return self / unit("rad")

#=============================================================================-
    def __len__(self):
        return len(self._value)

#=============================================================================-
    def __getitem__(self,index):
        ''' returns self's value sliced to index with self's units
        '''
        if hasattr(self._value, '__len__'):
            return Units(self._value[index], self._units)
        elif index == 0 or index == -1:
            return Units(self._value, self._units)
        else:
            raise IndexError

#=============================================================================-
    def __setitem__(self,index,other):
        ''' makes a slice assignment on self's value based on index
        '''
        other = other.convert(self._units)
        if hasattr(self._value, '__len__'):
            self._value[index] = other._value
        elif index == 0 or index == -1:
           self._value = other._value
        else:
           raise IndexError

#=============================================================================-
    def __eq__(self, other):
        '''Comparison operator: x.__eq__(y) <==> x == y
        '''
        if isinstance(other, self.__class__):
            try:
                other = other.convert(self._units)
            except CAPSError:
                return False

            return self._value == other._value and \
                   self._units == other._units
        else:
            return False

#=============================================================================-
    def __ne__(self, other):
        '''Comparison operator: x.__ne__(y) <==> x != y
        '''
        return not self.__eq__(other)

#=============================================================================-
    def __gt__(self, other):
        '''Comparison operator: x.__gt__(y) <==> x > y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} <= {!r}".format(self, other))

        other = other.convert(self._units)

        return self._value > other._value

#=============================================================================-
    def __ge__(self, other):
        '''Comparison operator: x.__ge__(y) <==> x >= y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} <= {!r}".format(self, other))

        other = other.convert(self._units)

        return self._value >= other._value

#=============================================================================-
    def __lt__(self, other):
        '''Comparison operator: x.__lt__(y) <==> x < y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} <= {!r}".format(self, other))

        other = other.convert(self._units)

        return self._value < other._value

#=============================================================================-
    def __le__(self, other):
        '''Comparison operator: x.__le__(y) <==> x <= y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} <= {!r}".format(self, other))

        other = other.convert(self._units)

        return self._value <= other._value

#=============================================================================-
    def __sub__(self, other):
        '''Binary operator: x.__sub__(y) <==> x - y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} - {!r}".format(self, other))

        other = other.convert(self._units)

        return Units(self._value - other._value, self._units)

#=============================================================================-
    def __add__(self, other):
        '''Binary operator: x.__add__(y) <==> x + y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} + {!r}".format(self, other))

        other = other.convert(self._units)

        return Units(self._value + other._value, self._units)

#=============================================================================-
    def __mul__(self, other):
        '''Binary operator: x.__mul__(y) <==> x * y
        '''
        if isinstance(other, self.__class__):

            newUnits = c_char_p()
            stat = _caps.caps_unitMultiply(self._units.encode(), other._units.encode(), 
                                           ctypes.byref(newUnits))
            if stat: _raiseStatus(stat, "Cannot {!r} * {!r}".format(self, other))
            units = _decode(newUnits.value)
            egads.free(newUnits)
        
            scale = float(units.split(" ")[0]) if " " in units else 1
        
            if hasattr(self._value, '__len__'):
                value = copy.copy(self._value)
                for i in range(len(self._value)):
                    value[i] = scale * self._value[i] * other._value
            elif hasattr(other._value, '__len__'):
                value = copy.copy(other._value)
                for i in range(len(other._value)):
                    value[i] = scale * self._value * other._value[i]
            else:
                value = scale * self._value*other._value
                
            if " " in units or units == "1":
                return value
            else:
                return Units(value, units)

        elif isinstance(other, (float,int)):

            return Units(self._value*other, self._units)

        elif hasattr(other, '__len__'):
            
            value = copy.copy(other)
            for i in range(len(other)):
                value[i] = self._value * other[i]

            return Units(value, self._units)

        _raiseStatus(CAPS_BADVALUE, "Cannot {!r} * {!r}".format(self, other))

#=============================================================================-
    def __div__(self, other):
        '''Binary operator: x.__div__(y) <==> x/y
        '''
        if isinstance(other, self.__class__):

            newUnits = c_char_p()
            stat = _caps.caps_unitDivide(self._units.encode(), other._units.encode(), 
                                         ctypes.byref(newUnits))
            if stat: _raiseStatus(stat, "Cannot {!r} / {!r}".format(self, other))
            units = _decode(newUnits.value)
            egads.free(newUnits)
            
            scale = float(units.split(" ")[0]) if " " in units else 1
        
            if hasattr(self._value, '__len__'):
                value = copy.copy(self._value)
                for i in range(len(self._value)):
                    value[i] = scale * self._value[i] / other._value
            else:
                value = scale * self._value/other._value
                
            if " " in units or units == "1":
                return value
            else:
                return Units(value, units)
        
        elif isinstance(other, (float,int)):

            return Units(self._value/other, self._units)

        _raiseStatus(CAPS_BADVALUE, "Cannot {!r} / {!r}".format(self, other))

#=============================================================================-
    def __pow__(self, other, modulo=None):
        '''Binary operator: x.__pow__(y) <==> x ** y
        '''
        if modulo is not None:
            raise CAPSError(CAPS_UNITERR, "3-argument power not supported")

        if not isinstance(other, int):
            raise CAPSError(CAPS_UNITERR, "Units can only be raised by integer powers! Power = {!r}".format(other))

        newUnits = c_char_p()
        stat = _caps.caps_unitRaise(self._units.encode(), c_int(other), 
                                     ctypes.byref(newUnits))
        if stat: _raiseStatus(stat, "Cannot {!r} ** {!r}".format(self, other))
        units = _decode(newUnits.value)
        egads.free(newUnits)

        return Units(self._value**other, units)

#=============================================================================-
    def __isub__(self, other):
        '''In-place operator: x.__isub__(y) <==> x -= y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} + {!r}".format(self, other))

        other = other.convert(self._units)
        self._value -= other._value

        return self

#=============================================================================-
    def __iadd__(self, other):
        '''In-place operator: x.__iadd__(y) <==> x += y
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} + {!r}".format(self, other))

        other = other.convert(self._units)
        self._value += other._value

        return self

#=============================================================================-
    def __imul__(self, other):
        '''In-place operator: x.__imul__(y) <==> x *= y
        '''
        newval = self * other
        if not isinstance(newval, Units):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} *= {!r}".format(self, other))

        self._value = newval._value
        self._units = newval._units
        return self

#=============================================================================-
    def __idiv__(self, other):
        '''In-place operator: x.__idiv__(y) <==> x /= y
        '''
        newval = self / other
        if not isinstance(newval, Units):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} /= {!r}".format(self, other))

        self._value = newval._value
        self._units = newval._units
        return self

#=============================================================================-
    def __ipow__(self, other):
        '''In-place operator: x.__ipow__(y) <==> x **= y
        '''
        newval = self ** other
        if not isinstance(newval, Units):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} **= {!r}".format(self, other))

        self._value = newval._value
        self._units = newval._units
        return self

#=============================================================================-
    def __rsub__(self, other):
        '''Reflected binary operator: x.__rsub__(y) <==> y - x
        '''
        try:
            return -self + other
        except CAPSError as e:
            raise CAPSError(e.errorCode, "Cannot {!r} - {!r}".format(other, self))

#=============================================================================-
    def __radd__(self, other):
        '''Reflected binary operator: x.__radd__(y) <==> y + x
        '''
        return self + other

#=============================================================================-
    def __rmul__(self, other):
        '''Reflected binary operator: x.__rmul__(y) <==> y * x
        '''
        return self * other

#=============================================================================-
    def __rdiv__(self, other):
        '''Reflected binary operator: x.__rdiv__(y) <==> y / x
        '''
        try:
            return (self ** -1) * other
        except CAPSError as e:
            raise CAPSError(e.errorCode, "Cannot {!r} / {!r}".format(other, self))

#=============================================================================-
    def __floordiv__(self, other):
        '''x.__floordiv__(y) <==> x//y <==> x / y
        '''
        return self / other

#=============================================================================-
    def __ifloordiv__(self, other):
        '''x.__ifloordiv__(y) <==> x//=y <==> x /= y
        '''
        return self / other

#=============================================================================-
    def __rfloordiv__(self, other):
        '''x.__rfloordiv__(y) <==> y//x <==> y / x
        '''
        try:
            return (self ** -1) * other
        except CAPSError as e:
            raise CAPSError(e.errorCode, "Cannot {!r} // {!r}".format(other, self))

#=============================================================================-
    def __truediv__(self, other):
        '''x.__truediv__(y) <==> x / y
        '''
        return self.__div__(other)

#=============================================================================-
    def __itruediv__(self, other):
        '''x.__itruediv__(y) <==> x /= y
        '''
        return self.__idiv__(other)

#=============================================================================-
    def __rtruediv__(self, other):
        '''x.__rtruediv__(y) <==> y / x
        '''
        return self.__rdiv__(other)

#=============================================================================-
    def __mod__(self, other):
        '''x.__mod__(y) <==> y % x
        '''
        if not isinstance(other, self.__class__):
            _raiseStatus(CAPS_UNITERR, "Cannot {!r} + {!r}".format(self, other))

        other = other.convert(self._units)

        return Units(self._value % other._value, self._units)

#=============================================================================-
    def __neg__(self):
        '''Unary operator: x.__neg__() <==> -x
        '''
        return self * -1

#=============================================================================-
    def __pos__(self):
        '''Unary operator: x.__pos__() <==> +x
        '''
        return self

#=============================================================================-
def unit(units):
    return Units(1, units)

#=============================================================================-
# class units:
#     # Collection of all available units
#     pass
# 
# UDUNITS2_XML_PATH = os.environ['UDUNITS2_XML_PATH']
# tree = xmlElementTree.parse(UDUNITS2_XML_PATH)
# root = tree.getroot()
# prefix = []
# 
# for imp in root:
#     tree = xmlElementTree.parse(os.path.dirname(UDUNITS2_XML_PATH) + os.sep + imp.text)
#     for pfx in tree.findall('prefix'):
#         for sym in pfx.iter('symbol'):
#             try:
#                 prefix.append(_decode(sym.text.encode('ascii')))
#             except UnicodeEncodeError:
#                 continue
# 
# for imp in root:
#     tree = xmlElementTree.parse(os.path.dirname(UDUNITS2_XML_PATH) + os.sep + imp.text)
#     for ut in tree.findall('unit'):
#         for sym in ut.iter('symbol'):
#             try:
#                 symbol = _decode(sym.text.encode('ascii'))
#             except UnicodeEncodeError:
#                 continue
#             setattr(units, symbol, unit(symbol))
#             for pfx in prefix:
#                 setattr(units, pfx+symbol, unit(pfx+symbol))
