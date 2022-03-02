/* Generated by Cython 0.29.24 */

#ifndef __PYX_HAVE__fun3dNamelist
#define __PYX_HAVE__fun3dNamelist

#include "Python.h"
#include "egadsTypes.h"
#include "egads.h"
#include "egadsErrors.h"
#include "prm.h"
#include "capsTypes.h"
#include "caps.h"
#include "capsErrors.h"
#include "cfdTypes.h"
#include "aimUtil.h"

    #if defined(WIN32)
      #define PATH_MAX _MAX_PATH
    #else
      #include <limits.h>
    #endif
    
#ifdef _OPENMP
#include <omp.h>
#endif /* _OPENMP */

#ifndef __PYX_HAVE_API__fun3dNamelist

#ifndef __PYX_EXTERN_C
  #ifdef __cplusplus
    #define __PYX_EXTERN_C extern "C"
  #else
    #define __PYX_EXTERN_C extern
  #endif
#endif

#ifndef DL_IMPORT
  #define DL_IMPORT(_T) _T
#endif

__PYX_EXTERN_C int fun3d_writeNMLPython(void *, capsValue *, cfdBoundaryConditionStruct);

#endif /* !__PYX_HAVE_API__fun3dNamelist */

/* WARNING: the interface of the module init function changed in CPython 3.5. */
/* It now returns a PyModuleDef instance instead of a PyModule instance. */

#if PY_MAJOR_VERSION < 3
PyMODINIT_FUNC initfun3dNamelist(void);
#else
PyMODINIT_FUNC PyInit_fun3dNamelist(void);
#endif

#endif /* !__PYX_HAVE__fun3dNamelist */
