#ifndef AIMU_H
#define AIMU_H
/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             AIM Utility Function Prototypes
 *
 *      Copyright 2014-2020, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "capsTypes.h"

#ifdef __ProtoExt__
#undef __ProtoExt__
#endif
#ifdef __cplusplus
extern "C" {
#define __ProtoExt__
#else
#define __ProtoExt__ extern
#endif

__ProtoExt__ int
  aim_getBodies( void *aimInfo, const char **intent, int *nBody, ego **bodies );
  
__ProtoExt__ int
  aim_newGeometry( void *aimInfo );
  
__ProtoExt__ int
  aim_getUnitSys( void *aimInfo, char **unitSys );

__ProtoExt__ int
  aim_convert( void *aimInfo, const char  *inUnits, double   inValue,
                              const char *outUnits, double *outValue );

__ProtoExt__ int
  aim_unitMultiply( void *aimInfo, const char  *inUnits1, const char *inUnits2,
                    char **outUnits );

__ProtoExt__ int
  aim_unitDivide( void *aimInfo, const char  *inUnits1, const char *inUnits2,
                  char **outUnits );

__ProtoExt__ int
  aim_unitInvert( void *aimInfo, const char  *inUnits,
                  char **outUnits );

__ProtoExt__ int
  aim_unitRaise( void *aimInfo, const char  *inUnits, const int power,
                 char **outUnits );

__ProtoExt__ int
  aim_getIndex( void *aimInfo, /*@null@*/ const char *name,
                enum capssType subtype );
  
__ProtoExt__ int
  aim_getValue( void *aimInfo, int index, enum capssType subtype,
                capsValue ** value );
  
__ProtoExt__ int
  aim_getName( void *aimInfo, int index, enum capssType subtype,
               const char **name );

__ProtoExt__ int
  aim_getGeomInType( void *aimInfo, int index );
  
__ProtoExt__ int
  aim_getData( void *aimInfo, const char *name, enum capsvType *vtype,
               int *rank, int *nrow, int *ncol, void **data, char **units );
  
__ProtoExt__ int
  aim_link( void *aimInfo, const char *name, enum capssType stype,
            capsValue *val );
  
__ProtoExt__ int
  aim_setTess( void *aimInfo, ego tess );
  
__ProtoExt__ int
  aim_getDiscr( void *aimInfo, const char *bname, capsDiscr **discr );
  
__ProtoExt__ int
  aim_getDiscrState( void *aimInfo, const char *bname );
  
__ProtoExt__ int
  aim_getDataSet( capsDiscr *discr, const char *dname, enum capsdMethod *method,
                  int *npts, int *rank, double **data );

__ProtoExt__ int
  aim_getBounds( void *aimInfo, int *nTname, char ***tnames );
  
__ProtoExt__ int
  aim_setSensitivity( void *aimInfo, const char *GIname, int irow, int icol );
  
__ProtoExt__ int
  aim_getSensitivity( void *aimInfo, ego body, int ttype, int index, int *npts,
                      double **dxyz );

__ProtoExt__ int
  aim_sensitivity( void *aimInfo, const char *GIname, int irow, int icol,
                   ego tess, int *npts, double **dxyz );

__ProtoExt__ int
  aim_isNodeBody( ego body, double *xyz );


  /* utility functions for nodal and cell centered data transfers */
__ProtoExt__ int
  aim_nodalTriangleType(capsEleType *eletype);

__ProtoExt__ int
  aim_nodalQuadType(capsEleType *eletype);

__ProtoExt__ int
  aim_cellTriangleType(capsEleType *eletype);

__ProtoExt__ int
  aim_cellQuadType(capsEleType *eletype);

__ProtoExt__ void
  aim_freeEleType(capsEleType *eletype);

__ProtoExt__ int
  aim_FreeDiscr(capsDiscr *discr);

__ProtoExt__ int
  aim_locateElement( capsDiscr *discr, double *params,
                     double *param,    int *eIndex,
                     double *bary);
__ProtoExt__ int
  aim_interpolation(capsDiscr *discr, const char *name, int eIndex,
                    double *bary, int rank, double *data, double *result);

__ProtoExt__ int
  aim_interpolateBar(capsDiscr *discr, const char *name, int eIndex,
                     double *bary, int rank, double *r_bar, double *d_bar);

__ProtoExt__ int
  aim_integration(capsDiscr *discr, const char *name, int eIndex, int rank,
                  double *data, double *result);

__ProtoExt__ int
  aim_integrateBar(capsDiscr *discr, const char *name, int eIndex, int rank,
                   double *r_bar, double *d_bar);

__ProtoExt__ void
  aim_status(void *aimInfo, const int status, const char *file, const int line, const char *func,
             const int narg, ...);

__ProtoExt__ void
  aim_error(void *aimInfo, const char *file, const int line, const char *func,
            const char *format, ...);

__ProtoExt__ void
  aim_warning(void *aimInfo, const char *file, const int line, const char *func,
              const char *format, ...);

__ProtoExt__ void
  aim_info(void *aimInfo, const char *file, const int line, const char *func,
           const char *format, ...);

__ProtoExt__ void
  aim_continuation(void *aimInfo, const char *file, const int line,
                   const char *func, const char *format, ...);

__ProtoExt__ void
  aim_setIndexError(void *aimInfo, int index);

__ProtoExt__ void
  aim_removeError(void *aimInfo);


/* Macro for counting varadaic arguments
 *
 * https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments/2124385#2124385
 */

#ifdef _MSC_VER // Microsoft compilers

#if (_MSC_VER < 1900)
#define __func__  __FUNCTION__
#endif

#  define GET_ARG_COUNT(...) INTERNAL_EXPAND_ARGS_PRIVATE(INTERNAL_ARGS_AUGMENTER(__VA_ARGS__))

#  define INTERNAL_ARGS_AUGMENTER(...) unused, __VA_ARGS__
#  define INTERNAL_EXPAND(x) x
#  define INTERNAL_EXPAND_ARGS_PRIVATE(...) \
    INTERNAL_EXPAND(INTERNAL_GET_ARG_COUNT_PRIVATE(__VA_ARGS__, 69, 68, 67, 66, 65, 64, \
                                                    63, 62, 61, 60, 59, 58, 57, 56, 55, \
                                                    54, 53, 52, 51, 50, 49, 48, 47, 46, \
                                                    45, 44, 43, 42, 41, 40, 39, 38, 37, \
                                                    36, 35, 34, 33, 32, 31, 30, 29, 28, \
                                                    27, 26, 25, 24, 23, 22, 21, 20, 19, \
                                                    18, 17, 16, 15, 14, 13, 12, 11, 10, \
                                                    9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#  define INTERNAL_GET_ARG_COUNT_PRIVATE(_1_, _2_, _3_, _4_, _5_, \
                                         _6_, _7_, _8_, _9_, _10_, \
                                         _11_, _12_, _13_, _14_, _15_, \
                                         _16_, _17_, _18_, _19_, _20_, \
                                         _21_, _22_, _23_, _24_, _25_, \
                                         _26_, _27_, _28_, _29_, _30_, \
                                         _31_, _32_, _33_, _34_, _35_, \
                                         _36, _37, _38, _39, _40, _41, \
                                         _42, _43, _44, _45, _46, _47, \
                                         _48, _49, _50, _51, _52, _53, \
                                         _54, _55, _56, _57, _58, _59, \
                                         _60, _61, _62, _63, _64, _65, \
                                         _66, _67, _68, _69, _70, count, ...) count

#else // Non-Microsoft compilers

#  define GET_ARG_COUNT(...) \
    INTERNAL_GET_ARG_COUNT_PRIVATE(0, ## __VA_ARGS__, 70, 69, 68, 67, 66, 65, 64, \
                                  63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, \
                                  51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
                                  39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, \
                                  27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, \
                                  15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#  define INTERNAL_GET_ARG_COUNT_PRIVATE(_0, _1_, _2_, _3_, _4_, _5_, \
                                         _6_, _7_, _8_, _9_, _10_, \
                                         _11_, _12_, _13_, _14_, _15_, \
                                         _16_, _17_, _18_, _19_, _20_, \
                                         _21_, _22_, _23_, _24_, _25_, \
                                         _26_, _27_, _28_, _29_, _30_, \
                                         _31_, _32_, _33_, _34_, _35_, \
                                         _36, _37, _38, _39, _40, _41, \
                                         _42, _43, _44, _45, _46, _47, \
                                         _48, _49, _50, _51, _52, _53, \
                                         _54, _55, _56, _57, _58, _59, \
                                         _60, _61, _62, _63, _64, _65, \
                                         _66, _67, _68, _69, _70, count, ...) count

#endif

#define AIM_STATUS(aimInfo, status, ...) \
 if (status != CAPS_SUCCESS) { \
   aim_status(aimInfo, status, __FILE__, __LINE__, __func__, GET_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__); \
   goto cleanup; \
 }

#define AIM_ERROR(aimInfo, format, ...) \
 aim_error(aimInfo, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__);

#define AIM_WARNING(aimInfo, format, ...) \
 aim_warning(aimInfo, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__);

#define AIM_INFO(aimInfo, format, ...) \
 aim_warning(aimInfo, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__);

#define AIM_CONTINUATION(aimInfo, format, ...) \
 aim_continuation(aimInfo, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__);


/*************************************************************************/
/* Prototypes of aim entry points to catch incorrect function signatures */

int
aimInitialize( int ngIn, /*@null@*/ capsValue *gIn, int *qeFlag,
               /*@null@*/ const char *unitSys, int *nIn, int *nOut,
               int *nFields, char ***fnames, int **ranks );

int
aimInputs( int inst, void *aimInfo, int index, char **ainame,
           capsValue *defval );

int
aimOutputs( int inst, void *aimInfo, int index, char **aoname,
            capsValue *form );

int
aimPreAnalysis( int inst, void *aimInfo, const char *apath,
                /*@null@*/ capsValue *inputs, capsErrs **errs );

int
aimPostAnalysis( int inst, void *aimInfo, const char *apath, capsErrs **errs );

void
aimCleanup( );

int
aimCalcOutput( int inst, void *aimInfo, const char *apath, int index,
               capsValue *val, capsErrs **errors );

int
aimDiscr( char *tname, capsDiscr *discr );

int
aimFreeDiscr( capsDiscr *discr );

int
aimLocateElement( capsDiscr *discr, double *params, double *param, int *eIndex,
                  double *bary );

int
aimUsesDataSet( int inst, void *aimInfo, const char *bname, const char *dname,
                enum capsdMethod dMethod );

int
aimTransfer( capsDiscr *discr, const char *fname, int npts, int rank,
             double *data, char **units );

int
aimInterpolation( capsDiscr *discr, const char *name, int eIndex,
                  double *bary, int rank, double *data, double *result );

int
aimInterpolateBar( capsDiscr *discr, const char *name, int eIndex,
                   double *bary, int rank, double *r_bar, double *d_bar );

int
aimIntegration( capsDiscr *discr, const char *name, int eIndex, int rank,
                /*@null@*/ double *data, double *result );

int
aimIntegrateBar( capsDiscr *discr, const char *name, int eIndex, int rank,
                 double *r_bar, double *d_bar );

int
aimData( int inst, const char *name, enum capsvType *vtype, int *rank,
         int *nrow, int *ncol, void **data, char **units );

int
aimBackdoor( int inst, void *aimInfo, const char *JSONin, char **JSONout );

/*************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
