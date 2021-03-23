/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             Value Object Functions
 *
 *      Copyright 2014-2021, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef WIN32
#define getcwd   _getcwd
#define PATH_MAX _MAX_PATH
#else
#include <unistd.h>
#include <limits.h>
#endif

#include "udunits.h"

#include "capsBase.h"
#include "capsAIM.h"

/* OpenCSM Defines & Includes */
#include "common.h"
#include "OpenCSM.h"


/*@-incondefs@*/
extern void ut_free(/*@only@*/ ut_unit* const unit);
extern void cv_free(/*@only@*/ cv_converter* const converter);
/*@+incondefs@*/

extern /*@null@*/ /*@only@*/
       char *EG_strdup(/*@null@*/ const char *str);

extern int  caps_integrateData(const capsObject *object, enum capstMethod method,
                               int *rank, double **data, char **units);
extern int  caps_getData(const capsObject *object, int *npts, int *rank,
                         const double **data, const char **units);
extern void caps_geomOutSensit(capsProblem *problem, int ipmtr, int irow,
                               int icol);
extern int  caps_build(capsObject *pobject, int *nErr, capsErrs **errors);
extern int  caps_analysisInfo(const capsObj aobject, char **apath, char **unSys,
                              char **intents, int *nparent, capsObj **parents,
                              int *nField, char ***fnames, int **ranks,
                              int *execution, int *status);
extern int  caps_convert(/*@null@*/ const char  *inUnit, double   inVal,
                         /*@null@*/ const char *outUnit, double *outVal );


int
caps_getValue(capsObject *object, enum capsvType *type, int *nrow, int *ncol,
              /*@null@*/ const void **data, /*@null@*/ const int **partial,
              /*@null@*/ const char **units, int *nErr, capsErrs **errors)
{
  int          i, in, status, nparent, nField, exec, dirty;
  int          *ranks;
  char         *intents, *apath, *unitSys, **fnames;
  char         currentPath[PATH_MAX];
  capsValue    *value, *valu0;
  capsObject   *pobject, *source, *last, **parents;
  capsProblem  *problem;
  capsAnalysis *analysis;
  capsErrs     *errs;
  
  *nErr   = 0;
  *errors = NULL;
  if (object  == NULL) return CAPS_NULLOBJ;
  if (nrow    == NULL) return CAPS_NULLVALUE;
  if (ncol    == NULL) return CAPS_NULLVALUE;
  if (partial == NULL) return CAPS_NULLVALUE;
  if (units   == NULL) return CAPS_NULLVALUE;
  status = caps_findProblem(object, CAPS_GETVALUE, &pobject);
  if (status != CAPS_SUCCESS) return status;

  source = object;
  do {
    if (source->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
    if (source->type        != VALUE)     return CAPS_BADTYPE;
    if (source->blind       == NULL)      return CAPS_NULLBLIND;
    value = (capsValue *) source->blind;
    if (value->link == object)            return CAPS_CIRCULARLINK;
    last   = source;
    source = value->link;
  } while (value->link != NULL);

  /* do we need to update our value? */
  if (last->subtype == ANALYSISOUT) {
    source = last->parent;
    if (source              == NULL)      return CAPS_NULLOBJ;
    if (source->blind       == NULL)      return CAPS_NULLBLIND;
    analysis = (capsAnalysis *) source->blind;
    problem  = analysis->info.problem;
    if (problem             == NULL)      return CAPS_NULLOBJ;
    if (source->parent      == NULL)      return CAPS_NULLOBJ;

    /* check to see if analysis is CLEAN */
    status = caps_analysisInfo(source, &apath, &unitSys, &intents, &nparent,
                               &parents, &nField, &fnames, &ranks, &exec, &dirty);
    if (status != CAPS_SUCCESS) return status;
    if (dirty > 0) return CAPS_DIRTY;

    valu0 = (capsValue *) analysis->analysisOut[0]->blind;
    in    = value - valu0;
    if (last->last.sNum < source->last.sNum) {
      if ((value->type == Boolean) || (value->type == Integer)) {
        if (value->length > 1) {
          EG_free(value->vals.integers);
          value->vals.integers = NULL;
        }
      } else if ((value->type == Double) || (value->type == DoubleDot)) {
        if (value->length > 1) {
          EG_free(value->vals.reals);
          value->vals.reals = NULL;
        }
      } else if (value->type == String) {
        if (value->length > 1) {
          EG_free(value->vals.string);
          value->vals.string = NULL;
        }
      } else if (value->type == Tuple) {
        caps_freeTuple(value->length, value->vals.tuple);
        value->vals.tuple = NULL;
      } else if (value->type != Pointer) {
        return CAPS_BADTYPE;
      }
      caps_freeOwner(&last->last);
      last->last.sNum = 0;
      (void) getcwd(currentPath, PATH_MAX);
      if (chdir(analysis->path) != 0) {
        printf(" caps_getValue: Path %s Error!\n", analysis->path);
        return CAPS_DIRERR;
      }
      status = aim_CalcOutput(problem->aimFPTR, analysis->loadName,
                              analysis->instStore, &analysis->info, in+1, value,
                              errors);
      chdir(currentPath);
      if (*errors != NULL) {
        errs  = *errors;
        *nErr = errs->nError;
        for (i = 0; i < errs->nError; i++)
          errs->errors[i].errObj = analysis->analysisOut[in];
      }
      if (status != CAPS_SUCCESS) return status;
      last->last.sNum = source->last.sNum;
      caps_fillDateTime(last->last.datetime);
    }
  } else if (last->subtype == GEOMETRYOUT) {
    /* make sure geometry is up-to-date */
    status = caps_build(pobject, nErr, errors);
    if ((status != CAPS_SUCCESS) && (status != CAPS_CLEAN)) return status;
  }

  *type    = value->type;
  *nrow    = value->nrow;
  *ncol    = value->ncol;
  *partial = value->partial;
  *units   = value->units;
  if (value->nullVal == IsNull) *nrow = *ncol = 0;
  
  if (data != NULL) {
    *data = NULL;
    if (value->nullVal != IsNull) {
      if ((value->type == Boolean) || (value->type == Integer)) {
        if (value->length == 1) {
          *data = &value->vals.integer;
        } else {
          *data =  value->vals.integers;
        }
      } else if ((value->type == Double) || (value->type == DoubleDot)) {
        if (value->length == 1) {
          *data = &value->vals.real;
        } else {
          *data =  value->vals.reals;
        }
      } else if (value->type == String) {
        *data = value->vals.string;
      } else if (value->type == Tuple) {
        *data = value->vals.tuple;
      } else if (value->type == Pointer) {
        *data = value->vals.AIMptr;
      } else {
        if (value->length == 1) {
          *data = &value->vals.object;
        } else {
          *data =  value->vals.objects;
        }
      }
    }
  }

  return CAPS_SUCCESS;
}


int
caps_makeValue(capsObject *pobject, const char *vname, enum capssType stype,
               enum capsvType vtype, int nrow, int ncol,
               const void *data, /*@null@*/ int *partial,
               /*@null@*/ const char *units, capsObject **vobj)
{
  int         status, i, vlen;
  capsObject  *object, **tmp;
  capsProblem *problem;
  capsValue   *value;
  ut_unit     *utunit;

  if (pobject == NULL)                         return CAPS_NULLOBJ;
  if (pobject->magicnumber != CAPSMAGIC)       return CAPS_BADOBJECT;
  if (pobject->type != PROBLEM)                return CAPS_BADTYPE;
  if (pobject->blind == NULL)                  return CAPS_NULLBLIND;
  if (vname == NULL)                           return CAPS_NULLNAME;
  if ((stype != PARAMETER) && (stype != USER)) return CAPS_BADTYPE;
  if (vtype == Value)                          return CAPS_BADTYPE;
  if (!((vtype == Double) || (vtype == DoubleDot) || (vtype == String))
      && (units != NULL))                      return CAPS_UNITERR;
  vlen = ncol*nrow;
  if ((vlen <= 0) && (vtype != String))        return CAPS_BADINDEX;
  problem = (capsProblem *) pobject->blind;
  problem->funID = CAPS_MAKEVALUE;
  
  /* check the units */
  if (units != NULL) {
    utunit = ut_parse((ut_system *) problem->utsystem, units, UT_ASCII);
    if (utunit == NULL)                        return CAPS_UNITERR;
    ut_free(utunit);
  }

  status = caps_makeVal(vtype, vlen, data, &value);
  if (status != CAPS_SUCCESS) return status;
  value->nrow = nrow;
  value->ncol = ncol;
  value->dim  = Scalar;
  if ((nrow > 1) || (ncol > 1)) value->dim = Vector;
  if ((nrow > 1) && (ncol > 1)) value->dim = Array2D;

  /* set partial if applicable */
  if (partial != NULL) {
    value->partial = (int*) EG_alloc(vlen*sizeof(int));
    if (value->partial == NULL) {
      if (value->length != 1) {
        if (value->type == Integer) {
          EG_free(value->vals.integers);
        } else if ((value->type == Double) || (value->type == DoubleDot)) {
          EG_free(value->vals.reals);
        } else if (value->type == String) {
          EG_free(value->vals.string);
        } else if (value->type == Tuple) {
          caps_freeTuple(value->length, value->vals.tuple);
        } else if (value->type != Pointer) {
          EG_free(value->vals.objects);
        }
      }
      EG_free(value);
      return EGADS_MALLOC;
    }
    for (i = 0; i < vlen; i++) value->partial[i] = partial[i];
    value->nullVal = IsPartial;
  }

  /* make the object */
  status = caps_makeObject(&object);
  if (status != CAPS_SUCCESS) {
    if (value->length != 1) {
      if (value->type == Integer) {
        EG_free(value->vals.integers);
      } else if ((value->type == Double) || (value->type == DoubleDot)) {
        EG_free(value->vals.reals);
      } else if (value->type == String) {
        EG_free(value->vals.string);
      } else if (value->type == Tuple) {
        caps_freeTuple(value->length, value->vals.tuple);
      } else if (value->type != Pointer) {
        EG_free(value->vals.objects);
      }
    }
    EG_free(value->partial);
    EG_free(value);
    return status;
  }
  object->parent = pobject;
  
  /* save away the object in the problem object */
  if (stype == PARAMETER) {
    if (problem->params == NULL) {
      problem->params = (capsObject  **) EG_alloc(sizeof(capsObject *));
      if (problem->params == NULL) {
        if (value->length != 1) {
          if (value->type == Integer) {
            EG_free(value->vals.integers);
          } else if ((value->type == Double) || (value->type == DoubleDot)) {
            EG_free(value->vals.reals);
          } else if (value->type == String) {
            EG_free(value->vals.string);
          } else if (value->type == Tuple) {
            caps_freeTuple(value->length, value->vals.tuple);
          } else if (value->type != Pointer) {
            EG_free(value->vals.objects);
          }
        }
        EG_free(value->partial);
        EG_free(value);
        EG_free(object);
        return EGADS_MALLOC;
      }
    } else {
      tmp = (capsObject  **) EG_reall( problem->params,
                                      (problem->nParam+1)*sizeof(capsObject *));
      if (tmp == NULL) {
        if (value->length != 1) {
          if (value->type == Integer) {
            EG_free(value->vals.integers);
          } else if ((value->type == Double) || (value->type == DoubleDot)) {
            EG_free(value->vals.reals);
          } else if (value->type == String) {
            EG_free(value->vals.string);
          } else if (value->type == Tuple) {
            caps_freeTuple(value->length, value->vals.tuple);
          } else if (value->type != Pointer) {
            EG_free(value->vals.objects);
          }
        }
        EG_free(value->partial);
        EG_free(value);
        EG_free(object);
        return EGADS_MALLOC;
      }
      problem->params = tmp;
    }
    problem->params[problem->nParam] = object;
    problem->nParam  += 1;
    problem->sNum    += 1;
    object->last.sNum = problem->sNum;
  }

  /* finish the object off */
  if (units != NULL) value->units = EG_strdup(units);
  object->name    = EG_strdup(vname);
  object->type    = VALUE;
  object->subtype = stype;
  object->blind   = value;
/*@-kepttrans@*/
  object->parent  = pobject;
/*@+kepttrans@*/
  
  *vobj = object;

  return CAPS_SUCCESS;
}


int
caps_setValue(capsObject *object, enum capsvType vtype, int nrow, int ncol,
              /*@null@*/ const void *data, /*@null@*/ const int *partial,
              /*@null@*/ const char *units, int *nErr, capsErrs **errors)
{
  int         i, j, k, n, vlen, status;
  int         *ints  = NULL;
  double      *reals = NULL, scale = 1.0;
  char        *str   = NULL;
  capsTuple   *tuple = NULL;
  capsObject  **objs = NULL, *pobject, *source, *last;
  capsValue   *value;
  capsProblem *problem;
  
  if  (nErr                == NULL)       return CAPS_NULLVALUE;
  if  (errors              == NULL)       return CAPS_NULLVALUE;
  *nErr   = 0;
  *errors = NULL;
  if (object              == NULL)        return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC)   return CAPS_BADOBJECT;
  if (object->type        != VALUE)       return CAPS_BADTYPE;
  if (object->subtype     == GEOMETRYOUT) return CAPS_BADTYPE;
  if (object->subtype     == ANALYSISOUT) return CAPS_BADTYPE;
  if (object->blind       == NULL)        return CAPS_NULLBLIND;
  value = (capsValue *) object->blind;
  if (value->link         != NULL)        return CAPS_LINKERR;
  vlen = nrow*ncol;
  if (vlen <= 0)                          return CAPS_RANGEERR;
  if ((value->type != String) && (value->sfixed == Fixed))
    if (value->dim == Scalar) {
      if (vlen > 1)                       return CAPS_SHAPEERR;
    } else if (value->dim == Vector) {
      if ((ncol != 1) && (nrow != 1))     return CAPS_SHAPEERR;
    }
  if (partial != NULL) {
    for (i = 0; i < vlen; i++)
      if (!(partial[i] == NotNull ||
            partial[i] == IsNull))        return CAPS_NULLVALUE;
  }
  status = caps_convert(units, 1, value->units, &scale);
  if (status != CAPS_SUCCESS) return status;

  status = caps_findProblem(object, CAPS_SETVALUE, &pobject);
  if (status != CAPS_SUCCESS) return status;
  if (( object->subtype == GEOMETRYIN) &&
      (pobject->subtype == STATIC    ))   return CAPS_READONLYERR;
  problem = (capsProblem *) pobject->blind;

  if (data == NULL) {
    if (value->nullVal == NotAllowed)     return CAPS_NULLVALUE;
    value->nullVal = IsNull;
    return CAPS_SUCCESS;
  }
  if (value->type != vtype) {
    if (!(((value->type == Double) || (value->type == DoubleDot)) && (vtype == Integer)))
      return CAPS_BADTYPE;
  }
  if (value->nullVal == IsNull) value->nullVal = NotNull;
  if (value->type    == String) vlen = strlen(data)+1;
  
  /* are we in range? */
  if (value->type == Integer) {
    ints = (int *) data;
    if (value->limits.ilims[0] != value->limits.ilims[1])
      for (i = 0; i < vlen; i++)
        if ((ints[i] < value->limits.ilims[0]) ||
            (ints[i] > value->limits.ilims[1])) return CAPS_RANGEERR;
  } else if ((value->type == Double) || (value->type == DoubleDot)) {
    reals = (double *) data;
    if (value->limits.dlims[0] != value->limits.dlims[1])
      for (i = 0; i < vlen; i++)
        if ((reals[i] < value->limits.dlims[0]) ||
            (reals[i] > value->limits.dlims[1])) return CAPS_RANGEERR;
  }
  
  /* remove any existing memory */
  if (vlen != value->length) {
    if ((value->lfixed == Fixed) && (value->type != String) &&
        (value->type != Pointer) && (value->type != Tuple)) return CAPS_SHAPEERR;
    if ((value->type == Boolean) || (value->type == Integer)) {
      if (vlen > 1) {
        ints = (int *) EG_alloc(vlen*sizeof(int));
        if (ints == NULL) return EGADS_MALLOC;
      }
      if (value->length > 1) EG_free(value->vals.integers);
      if (ints != NULL) value->vals.integers = ints;
    } else if ((value->type == Double) || (value->type == DoubleDot)) {
      if (vlen > 1) {
        reals = (double *) EG_alloc(vlen*sizeof(double));
        if (reals == NULL) return EGADS_MALLOC;
      }
      if (value->length > 1) EG_free(value->vals.reals);
      if (reals != NULL) value->vals.reals = reals;
    } else if (value->type == String) {
      str = NULL;
      if (vlen > 0) {
        str = (char *) EG_alloc(vlen*sizeof(char));
        if (str == NULL) return EGADS_MALLOC;
      }
      EG_free(value->vals.string);
      value->vals.string = str;
    } else if (value->type == Tuple) {
      status = caps_makeTuple(vlen, &tuple);
      if ((status != CAPS_SUCCESS) || (tuple == NULL)) {
        if (tuple == NULL) status = CAPS_NULLVALUE;
        return status;
      }
      caps_freeTuple(value->length, value->vals.tuple);
      value->vals.tuple = tuple;
    } else if (value->type == Value) {
      if (vlen > 1) {
        objs = (capsObject **) EG_alloc(vlen*sizeof(capsObject *));
        if (objs == NULL) return EGADS_MALLOC;
      }
      if (value->length > 1) EG_free(value->vals.objects);
      if (objs != NULL) value->vals.objects = objs;
    }

    if ((value->nullVal == IsPartial) || (partial != NULL)) {
      ints = (int *) EG_alloc(vlen*sizeof(int));
      if (ints == NULL) return EGADS_MALLOC;
      EG_free(value->partial);
      value->partial = ints;
      for (i = 0; i < vlen; i++) value->partial[i] = NotNull;
      value->nullVal = IsPartial;
    }

    value->length = vlen;
  } else {
    if (value->type == Tuple) {
      status = caps_makeTuple(vlen, &tuple);
      if ((status != CAPS_SUCCESS) || (tuple == NULL)) {
        if (tuple == NULL) status = CAPS_NULLVALUE;
        return status;
      }
      caps_freeTuple(value->length, value->vals.tuple);
      value->vals.tuple = tuple;
    }
  }

  /* set the values */
  if ((value->type == Boolean) || (value->type == Integer)) {
    ints = (int *) data;
    if (vlen == 1) {
      value->vals.integer = ints[0];
    } else {
      for (i = 0; i < vlen; i++) value->vals.integers[i] = ints[i];
    }
  } else if ((value->type == Double) || (value->type == DoubleDot)) {
    if ((vtype == Double) || (vtype == DoubleDot)) {
      reals = (double *) data;
      if (vlen == 1) {
        value->vals.real = scale*reals[0];
      } else {
        for (i = 0; i < vlen; i++) value->vals.reals[i] = scale*reals[i];
      }
    } else {
      ints = (int *) data;
      if (vlen == 1) {
        value->vals.real = scale*ints[0];
      } else {
        for (i = 0; i < vlen; i++) value->vals.reals[i] = scale*ints[i];
      }
    }
  } else if (value->type == String) {
    str = (char *) data;
    for (i = 0; i < vlen; i++) value->vals.string[i] = str[i];
  } else if (value->type == Tuple) {
    tuple = (capsTuple *) data;
    for (i = 0; i < vlen; i++) {
      value->vals.tuple[i].name  = EG_strdup(tuple[i].name);
      value->vals.tuple[i].value = EG_strdup(tuple[i].value);
      if ((tuple[i].name  != NULL) &&
          (value->vals.tuple[i].name  == NULL)) return EGADS_MALLOC;
      if ((tuple[i].value != NULL) &&
          (value->vals.tuple[i].value == NULL)) return EGADS_MALLOC;
    }
  } else if (value->type == Pointer) {
/*@-kepttrans@*/
    value->vals.AIMptr = (void *) data;
/*@+kepttrans@*/
  } else {
    objs = (capsObject **) data;
    if (vlen == 1) {
      value->vals.object = objs[0];
    } else {
      for (i = 0; i < vlen; i++) value->vals.objects[i] = objs[i];
    }
  }
  if (partial != NULL) {
    if (value->partial == NULL) {
      ints = (int *) EG_alloc(vlen*sizeof(int));
      if (ints == NULL) return EGADS_MALLOC;
      value->partial = ints;
    }
    for (i = 0; i < vlen; i++) value->partial[i] = partial[i];
    value->nullVal = IsPartial;
  }

  value->nrow = nrow;
  value->ncol = ncol;
  
  if (object->subtype != USER) {
    caps_freeOwner(&object->last);
    problem->sNum    += 1;
    object->last.sNum = problem->sNum;
    caps_fillDateTime(object->last.datetime);
  }
  
  /* apply GeometryIn changes now -- direct and linkages */
  if (object->subtype == GEOMETRYIN) {
    if (vlen == 1) {
      reals = &value->vals.real;
    } else {
      reals = value->vals.reals;
    }
    for (n = k = 0; k < nrow; k++)
      for (j = 0; j < ncol; j++, n++) {
        if (value->nullVal == IsPartial) {
          status = ocsmSetValuD(problem->modl, value->pIndex,
                                k+1, j+1, value->partial[n] == NotNull ? scale*reals[n] : -HUGEQ);
        } else {
          status = ocsmSetValuD(problem->modl, value->pIndex,
                                k+1, j+1, scale*reals[n]);
        }
        if (status != SUCCESS)
          printf(" CAPS Warning: Cant change %s[%d,%d] = %d (caps_setValue)!\n",
                   object->name, k+1, j+1, status);
      }
  } else {
    for (i = 0; i < problem->nGeomIn; i++) {
      source = problem->geomIn[i];
      last   = NULL;
      do {
        if (source->magicnumber != CAPSMAGIC)  break;
        if (source->type        != VALUE)      break;
        if (source->blind       == NULL)       break;
        value = (capsValue *) source->blind;
        if (value->link == problem->geomIn[i]) break;
        last   = source;
        source = value->link;
      } while (value->link != NULL);
      if (last != object) continue;
      /* we hit our object from a GeometryIn link */
      source = problem->geomIn[i];
      value  = (capsValue *) source->blind;
      if ((value->nrow != nrow) || (value->ncol != ncol)) {
        printf(" CAPS Warning: Shape problem with link %s %s (caps_setValue)!\n",
               object->name, source->name);
        continue;
      }
      status = caps_convert(units, 1, value->units, &scale);
      if (status != CAPS_SUCCESS) return status;
      if ((vtype == Double) || (vtype == DoubleDot)) {
        reals = (double *) data;
        if (vlen == 1) {
          value->vals.real = scale*reals[0];
        } else {
          for (i = 0; i < vlen; i++) value->vals.reals[i] = scale*reals[i];
        }
      } else {
        ints = (int *) data;
        if (vlen == 1) {
          value->vals.real = scale*ints[0];
          reals = &value->vals.real;
        } else {
          for (i = 0; i < vlen; i++) value->vals.reals[i] = scale*ints[i];
          reals = value->vals.reals;
        }
      }
      for (n = k = 0; k < nrow; k++)
        for (j = 0; j < ncol; j++, n++) {
          if (value->nullVal == IsPartial) {
            status = ocsmSetValuD(problem->modl, value->pIndex,
                                  k+1, j+1, value->partial[n] == NotNull ? scale*reals[n] : -HUGEQ);
          } else {
            status = ocsmSetValuD(problem->modl, value->pIndex,
                                  k+1, j+1, scale*reals[n]);
          }
          if (status != SUCCESS)
            printf(" CAPS Warning: Cant change %s[%d,%d] = %d (caps_setValue)!\n",
                   object->name, k+1, j+1, status);
        }
      caps_freeOwner(&source->last);
      source->last       = object->last;
      source->last.pname = EG_strdup(object->last.pname);
      source->last.pID   = EG_strdup(object->last.pID);
      source->last.user  = EG_strdup(object->last.user);
    }
  }
  
  return CAPS_SUCCESS;
}


int
caps_getLimits( const capsObj object, enum capsvType *vtype,
                const void **limits, const char **units )
{
  int        status;
  capsValue  *value;
  capsObject *pobject;

  if (limits              == NULL)      return CAPS_NULLVALUE;
  *limits = NULL;
  if (units               == NULL)      return CAPS_NULLVALUE;
  *units = NULL;
  if (vtype               == NULL)      return CAPS_NULLVALUE;
  *vtype = 0;

  if (object              == NULL)      return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if (object->type        != VALUE)     return CAPS_BADTYPE;
  if (object->blind       == NULL)      return CAPS_NULLBLIND;
  value  = (capsValue *) object->blind;
  status = caps_findProblem(object, CAPS_GETLIMITS, &pobject);
  if (status != CAPS_SUCCESS) return status;
  
  if (value->type == Integer) {
    if (value->limits.ilims[0] == value->limits.ilims[1]) return CAPS_SUCCESS;
    *limits = value->limits.ilims;
  } else if ((value->type == Double) || (value->type == DoubleDot)) {
    if (value->limits.dlims[0] == value->limits.dlims[1]) return CAPS_SUCCESS;
    *limits = value->limits.dlims;
  } else {
    return CAPS_BADTYPE;
  }
  *vtype = value->type;
  *units = value->units;

  return CAPS_SUCCESS;
}


int
caps_setLimits( capsObject *object, enum capsvType vtype,
                void *limits, const char *units )
{
  int        *ints, i, status;
  double     *realp, reals[2], scale;
  capsValue  *value;
  capsObject *pobject;

  if  (object              == NULL)      return CAPS_NULLOBJ;
  if  (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if  (object->type        != VALUE)     return CAPS_BADTYPE;
  if ((object->subtype     != USER) && (object->subtype != PARAMETER))
                                         return CAPS_BADTYPE;
  if  (object->blind       == NULL)      return CAPS_NULLBLIND;
  value  = (capsValue *) object->blind;

  if (limits == NULL) {
    value->limits.ilims[0] = value->limits.ilims[1] = 0;
    value->limits.dlims[0] = value->limits.dlims[1] = 0.0;
    return CAPS_SUCCESS;
  }
  if ( value->type != vtype ) {
    if (!( ((value->type == Double) || (value->type == DoubleDot)) &&
            (vtype == Integer) )) return CAPS_BADTYPE;
  }

  status = caps_convert(units, 1, value->units, &scale);
  if (status != CAPS_SUCCESS) return status;

  status = caps_findProblem(object, CAPS_SETLIMITS, &pobject);
  if (status != CAPS_SUCCESS) return status;

  if (value->type == Integer) {
    if ( units != NULL ) return CAPS_UNITERR;
    ints = (int *) limits;
    if (ints[0] >= ints[1]) return CAPS_RANGEERR;
    if (value->length == 1) {
      if ((value->vals.integer < ints[0]) ||
          (value->vals.integer > ints[1])) return CAPS_RANGEERR;
    } else {
      for (i = 0; i < value->length; i++)
        if ((value->vals.integers[i] < ints[0]) ||
            (value->vals.integers[i] > ints[1])) return CAPS_RANGEERR;
    }
    value->limits.ilims[0] = ints[0];
    value->limits.ilims[1] = ints[1];
  } else if ((value->type == Double) || (value->type == DoubleDot)) {
    if (vtype == Integer) {
      ints = (int *) limits;
      reals[0] = scale*ints[0];
      reals[1] = scale*ints[1];
    } else {
      realp = (double *) limits;
      reals[0] = scale*realp[0];
      reals[1] = scale*realp[1];
    }
    if (reals[0] >= reals[1]) return CAPS_RANGEERR;
    if (value->length == 1) {
      if ((value->vals.real < reals[0]) ||
          (value->vals.real > reals[1])) return CAPS_RANGEERR;
    } else {
      for (i = 0; i < value->length; i++)
        if ((value->vals.reals[i] < reals[0]) ||
            (value->vals.reals[i] > reals[1])) return CAPS_RANGEERR;
    }
    value->limits.dlims[0] = reals[0];
    value->limits.dlims[1] = reals[1];
  } else {
    return CAPS_BADTYPE;
  }
  
  return CAPS_SUCCESS;
}


int
caps_getValueProps(const capsObject *object, int *dim, int *pmtr,
                   enum capsFixed *lfixed, enum capsFixed *sfixed,
                   enum capsNull *nval)
{
  int        status;
  capsValue  *value;
  capsObject *pobject;
  
  if (dim    == NULL) return CAPS_NULLVALUE;
  if (pmtr   == NULL) return CAPS_NULLVALUE;
  if (lfixed == NULL) return CAPS_NULLVALUE;
  if (sfixed == NULL) return CAPS_NULLVALUE;
  if (nval   == NULL) return CAPS_NULLVALUE;

  *dim    = *pmtr = 0;
  *lfixed = *sfixed = Fixed;
  if (object              == NULL)      return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if (object->type        != VALUE)     return CAPS_BADTYPE;
  if (object->blind       == NULL)      return CAPS_NULLBLIND;
  value  = (capsValue *) object->blind;
  status = caps_findProblem(object, CAPS_GETVALUESHAPE, &pobject);
  if (status != CAPS_SUCCESS) return status;
  
  *dim    = value->dim;
  *pmtr   = value->gInType;
  *lfixed = value->lfixed;
  *sfixed = value->sfixed;
  *nval   = value->nullVal;

  return CAPS_SUCCESS;
}


int
caps_setValueProps(capsObject *object, int dim, enum capsFixed lfixed,
                   enum capsFixed sfixed, enum capsNull nval)
{
  int        status;
  capsValue  *value;
  capsObject *pobject;
  
  if  (object              == NULL)      return CAPS_NULLOBJ;
  if  (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if  (object->type        != VALUE)     return CAPS_BADTYPE;
  if ((object->subtype != PARAMETER) &&
      (object->subtype != USER))         return CAPS_BADTYPE;
  if  (object->blind == NULL)            return CAPS_NULLBLIND;
  value = (capsValue *) object->blind;
  if ((nval == NotAllowed) &&
      (value->nullVal == IsNull))        return CAPS_NULLVALUE;
  status = caps_findProblem(object, CAPS_SETVALUESHAPE, &pobject);
  if (status != CAPS_SUCCESS) return status;

  /* check dimension */
  if (dim == 0) {
    if ((value->ncol != 1) || (value->nrow != 1)) return CAPS_SHAPEERR;
  } else if (dim == 1) {
    if ((value->ncol >  1) && (value->nrow >  1)) return CAPS_SHAPEERR;
  } else if (dim != 2) {
    return CAPS_RANGEERR;
  }
  
  value->dim     = dim;
  value->lfixed  = lfixed;
  value->sfixed  = sfixed;
  if ((nval == IsNull)  && (value->nullVal == NotNull)) return CAPS_SUCCESS;
  if ((nval == NotNull) && (value->nullVal == IsNull))  return CAPS_SUCCESS;
  value->nullVal = nval;
  
  return CAPS_SUCCESS;
}


int
caps_convertValue(const capsObject *object, double inp, const char *units,
                  double *outp)
{
  capsValue    *value;

  if (object              == NULL)        return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC)   return CAPS_BADOBJECT;
  if (object->type        != VALUE)       return CAPS_BADTYPE;
  if (object->blind       == NULL)        return CAPS_NULLBLIND;
  value  = (capsValue *) object->blind;
  if (units               == NULL)        return CAPS_UNITERR;
  if (value->units        == NULL)        return CAPS_UNITERR;

  return caps_convert(units, inp, value->units, outp);
}


int
caps_dupValues(capsValue *val1, capsValue *val2)
{
  int i, j, len, status;

  /* this function assumes that val2 has no allocated data */
  val2->type    = val1->type;
  val2->length  = val1->length;
  val2->dim     = val1->dim;
  val2->nrow    = val1->nrow;
  val2->ncol    = val1->ncol;
  val2->lfixed  = val1->lfixed;
  val2->sfixed  = val1->sfixed;
  val2->nullVal = val1->nullVal;
  val2->pIndex  = val1->pIndex;
  val2->gInType = val1->gInType;
  val2->partial = NULL;
  val2->ndot    = 0;
  val2->dots    = NULL;
  if (val1->partial != NULL) {
    val2->partial = (int *) EG_alloc(val1->length*sizeof(int));
    if (val2->partial == NULL) return EGADS_MALLOC;
    for (i = 0; i < val1->length; i++) val2->partial[i] = val1->partial[i];
  }
  if ((val1->ndot != 0) && (val1->dots != NULL)) {
    val2->dots = (capsDot *) EG_alloc(val1->ndot*sizeof(capsDot));
    if (val2->dots == NULL) return EGADS_MALLOC;
    for (i = 0; i < val1->ndot; i++) {
      val2->dots[i].name = EG_strdup(val1->dots[i].name);
      val2->dots[i].rank = val1->dots[i].rank;
      val2->dots[i].dot  = NULL;
    }
    for (i = 0; i < val1->ndot; i++) {
      if (val2->dots[i].name == NULL) return EGADS_MALLOC;
      val2->dots[i].dot = (double *) EG_alloc(
                           val1->length*val1->dots[i].rank*sizeof(double));
      if (val2->dots[i].dot == NULL) return EGADS_MALLOC;
      for (j = 0; j < val1->length*val1->dots[i].rank; j++)
        val2->dots[i].dot[j] = val1->dots[i].dot[j];
    }
    val2->ndot = val1->ndot;
  }
  
  /* set the actual value(s) */
  switch (val1->type) {
    case Boolean:
      if (val1->length == 1) {
        val2->vals.integer  = val1->vals.integer;
      } else {
    	val2->vals.integers = NULL;
    	if (val1->vals.integers == NULL) break;
        val2->vals.integers = (int *) EG_alloc(val1->length*sizeof(int));
        if (val2->vals.integers == NULL) return EGADS_MALLOC;
        for (i = 0; i < val1->length; i++)
          val2->vals.integers[i] = val1->vals.integers[i];
      }
      break;

    case Integer:
      if (val1->length == 1) {
        val2->vals.integer  = val1->vals.integer;
      } else {
        val2->vals.integers = NULL;
        if (val1->vals.integers != NULL) {
          val2->vals.integers = (int *) EG_alloc(val1->length*sizeof(int));
          if (val2->vals.integers == NULL) return EGADS_MALLOC;
          for (i = 0; i < val1->length; i++)
            val2->vals.integers[i] = val1->vals.integers[i];
        }
      }
      val2->limits.ilims[0] = val1->limits.ilims[0];
      val2->limits.ilims[1] = val1->limits.ilims[1];
      break;
      
    case Double:
      if (val1->length == 1) {
        val2->vals.real  = val1->vals.real;
      } else {
    	val2->vals.reals = NULL;
    	if (val1->vals.reals != NULL) {
          val2->vals.reals = (double *) EG_alloc(val1->length*sizeof(double));
          if (val2->vals.reals == NULL) return EGADS_MALLOC;
          for (i = 0; i < val1->length; i++)
            val2->vals.reals[i] = val1->vals.reals[i];
    	}
      }
      val2->limits.dlims[0] = val1->limits.dlims[0];
      val2->limits.dlims[1] = val1->limits.dlims[1];
      break;
      
    case String:
      val2->vals.string = NULL;
      if ((val1->vals.string != NULL)  && (strlen(val1->vals.string) > 0)) {
        j = strlen(val1->vals.string) + 1;
        val2->vals.string = (char *) EG_alloc(j*sizeof(char));
        if (val2->vals.string == NULL) return EGADS_MALLOC;
        for (i = 0; i < j; i++)
          val2->vals.string[i] = val1->vals.string[i];
      }
      break;
      
    case Tuple:
      val2->vals.tuple = NULL;
      if (val1->vals.tuple == NULL)
    	  break;
      status = caps_makeTuple(val1->length, &val2->vals.tuple);
      if (status != CAPS_SUCCESS)   return status;
      if (val2->vals.tuple == NULL) return EGADS_MALLOC;        /* for splint */
      for (i = 0; i < val1->length; i++) {
        val2->vals.tuple[i].name  = EG_strdup(val1->vals.tuple[i].name);
        val2->vals.tuple[i].value = EG_strdup(val1->vals.tuple[i].value);
        if ((val1->vals.tuple[i].name  != NULL) &&
            (val2->vals.tuple[i].name  == NULL)) return EGADS_MALLOC;
        if ((val1->vals.tuple[i].value != NULL) &&
            (val2->vals.tuple[i].value == NULL)) return EGADS_MALLOC;
      }
      break;
      
    case Pointer:
      val2->vals.AIMptr = val1->vals.AIMptr;
      break;
      
    case Value:
      if (val1->length == 1) {
        val2->vals.object  = val1->vals.object;
      } else {
        val2->vals.objects = (capsObject **)
                             EG_alloc(val1->length*sizeof(capsObject *));
        if (val2->vals.objects == NULL) return EGADS_MALLOC;
        for (i = 0; i < val1->length; i++)
          val2->vals.objects[i] = val1->vals.objects[i];
      }
      
  }
  
  /* copy units */
  val2->units = NULL;
  if (val1->units != NULL) {
    len = strlen(val1->units) + 1;
    val2->units = (char *) EG_alloc(len*sizeof(char));
    if (val2->units == NULL) return EGADS_MALLOC;
    for (i = 0; i < len; i++) val2->units[i] = val1->units[i];
  }
  
  /* linkage */
  val2->link       = val1->link;
  val2->linkMethod = val1->linkMethod;
  
  return CAPS_SUCCESS;
}


static int
caps_compatValues(capsValue *val1, capsValue *val2, capsProblem *problem)
{
  int     status;
  ut_unit *utunit1, *utunit2;

  /* check units */
  if ((val1->units != NULL) && (val2->units == NULL)) return CAPS_UNITERR;
  if ((val1->units == NULL) && (val2->units != NULL)) return CAPS_UNITERR;
  if ((val1->units != NULL) && (val2->units != NULL)) {
    if (strcmp(val1->units, val2->units) != 0) {
      utunit1 = ut_parse((ut_system *) problem->utsystem, val1->units, UT_ASCII);
      utunit2 = ut_parse((ut_system *) problem->utsystem, val2->units, UT_ASCII);
      status  = ut_are_convertible(utunit1, utunit2);
      ut_free(utunit1);
      ut_free(utunit2);
      if (status == 0) return CAPS_UNITERR;
    }
  }
  
  /* check type */
  if (val1->type != val2->type)                   return CAPS_BADTYPE;

  /* check shape */
  if (val2->lfixed == Fixed)
    if (val1->length != val2->length)             return CAPS_SHAPEERR;
  if (val2->sfixed == Fixed) {
    if (val2->dim    >  val1->dim)                return CAPS_SHAPEERR;
    if (val2->nrow   != val1->nrow)               return CAPS_SHAPEERR;
    if (val2->ncol   != val1->ncol)               return CAPS_SHAPEERR;
  } else {
    if (val2->dim == Scalar) {
      if (val1->length != 1)                      return CAPS_SHAPEERR;
    } else if (val2->dim == Vector) {
      if ((val1->ncol != 1) && (val1->nrow != 1)) return CAPS_SHAPEERR;
    }
  }
  
  return CAPS_SUCCESS;
}


int
caps_transferValues(capsObject *source, enum capstMethod method,
                    capsObject *target, int *nErr, capsErrs **errors)
{
  int            i, status, in, vlen, rank, ncol, nrow;
  char           *iunits, currentPath[PATH_MAX];
  double         *ireals;
  const void     *data;
  const int      *partial;
  const char     *units;
  const double   *reals;
  capsValue      *value, *sval, *valu0;
  capsProblem    *problem;
  capsAnalysis   *analysis;
  capsObject     *pobject, *src, *last;
  capsErrs       *errs;
  enum capsvType vtype;

  if  (nErr                == NULL)         return CAPS_NULLVALUE;
  if  (errors              == NULL)         return CAPS_NULLVALUE;
  *nErr   = 0;
  *errors = NULL;
  if  (source              == NULL)         return CAPS_NULLOBJ;
  if  (source->magicnumber != CAPSMAGIC)    return CAPS_BADOBJECT;
  if ((source->type        != VALUE) &&
      (source->type        != DATASET))     return CAPS_BADTYPE;
  if  (source->blind       == NULL)         return CAPS_NULLBLIND;
  if  (target              == NULL)         return CAPS_NULLOBJ;
  if  (target->magicnumber != CAPSMAGIC)    return CAPS_BADOBJECT;
  if  (target->type        != VALUE)        return CAPS_BADTYPE;
  if  (target->subtype     == GEOMETRYOUT)  return CAPS_BADTYPE;
  if  (target->subtype     == ANALYSISOUT)  return CAPS_BADTYPE;
  if  (target->blind       == NULL)         return CAPS_NULLBLIND;
  value   = (capsValue *)   target->blind;
  if (value->type          == Value)        return CAPS_BADTYPE;
  status  = caps_findProblem(target, CAPS_TRANSFERVALUES, &pobject);
  if (status               != CAPS_SUCCESS) return status;
  problem = (capsProblem *) pobject->blind;
  
  if (source->type == VALUE) {

    src = source;
    if (method             != Copy)         return CAPS_BADMETHOD;
    do {
      if (src->magicnumber != CAPSMAGIC)    return CAPS_BADOBJECT;
      if (src->type        != VALUE)        return CAPS_BADTYPE;
      if (src->blind       == NULL)         return CAPS_NULLBLIND;
      sval = (capsValue *) src->blind;
      if (sval->type       == Value)        return CAPS_BADTYPE;
      if (sval->link       == source)       return CAPS_CIRCULARLINK;
      last = src;
      src  = sval->link;
    } while (sval->link != NULL);
    /* do we need to update our value? */
    if (last->subtype == ANALYSISOUT) {
      src = last->parent;
      if (src              == NULL)         return CAPS_NULLOBJ;
      if (src->blind       == NULL)         return CAPS_NULLBLIND;
      analysis = (capsAnalysis *) src->blind;
      valu0    = (capsValue *)    analysis->analysisOut[0]->blind;
      in       = sval - valu0;
      if (last->last.sNum < src->last.sNum) {
        if ((sval->type == Boolean) || (sval->type == Integer)) {
          if (sval->length > 1) {
            EG_free(sval->vals.integers);
            sval->vals.integers = NULL;
          }
        } else if ((sval->type == Double) || (sval->type == DoubleDot)) {
          if (sval->length > 1) {
            EG_free(sval->vals.reals);
            sval->vals.reals = NULL;
          }
        } else if (sval->type == String) {
          if (sval->length > 1) {
            EG_free(sval->vals.string);
            sval->vals.string = NULL;
          }
        } else if (sval->type != Pointer) {
          return CAPS_BADTYPE;
        }
        caps_freeOwner(&last->last);
        last->last.sNum = 0;
        (void) getcwd(currentPath, PATH_MAX);
        if (chdir(analysis->path) != 0) {
          printf(" caps_transferValues: Path %s Error!\n", analysis->path);
          return CAPS_DIRERR;
        }
        status = aim_CalcOutput(problem->aimFPTR, analysis->loadName,
                                analysis->instStore, &analysis->info, in+1,
                                sval, errors);
        chdir(currentPath);
        if (*errors != NULL) {
          errs  = *errors;
          *nErr = errs->nError;
          for (i = 0; i < errs->nError; i++)
            errs->errors[i].errObj = analysis->analysisOut[in];
        }
        if (status != CAPS_SUCCESS) return status;
        last->last.sNum = src->last.sNum;
        caps_fillDateTime(last->last.datetime);
      }
    }
    
    /* cleanup nullVal */
    sval->length = sval->nrow*sval->ncol;
    if (sval->nullVal != NotAllowed) {
      if (sval->type == Pointer)
        sval->nullVal = (sval->vals.AIMptr == NULL) ? IsNull : NotNull;
    }

    /* make sure we are compatible */
    if ((sval->nullVal == IsNull) && (value->nullVal == NotAllowed))
      return CAPS_NULLVALUE;

    status = caps_compatValues(sval, value, problem);
    if (status != CAPS_SUCCESS) return status;
    status = caps_getValue(source, &vtype, &nrow, &ncol, &data, &partial,
                           &units, nErr, errors);
    if (status != CAPS_SUCCESS) return status;
    last = value->link;
    value->link = NULL;
    status = caps_setValue(target, vtype, nrow, ncol, data, partial, units,
                           nErr, errors);
    value->link = last;
    if (status != CAPS_SUCCESS) return status;
    
  } else {

    if (method == Copy) {

      sval = (capsValue *) source->blind;
      if (sval->nullVal == IsNull) return CAPS_NULLVALUE;
      status = caps_getData(source, &vlen, &rank, &reals, &units);
      if (status != CAPS_SUCCESS) return status;
      /* check for compatibility */
      status = caps_makeVal(Double, vlen*rank, reals, &sval);
      if (status != CAPS_SUCCESS) return status;
      sval->units = (char *) units;
      sval->ncol  = vlen;
      sval->nrow  = rank;
      if (rank != 1) sval->dim = Array2D;
      status = caps_compatValues(sval, value, problem);
      if (status != CAPS_SUCCESS) {
        if (sval->length > 1) EG_free(sval->vals.reals);
        EG_free(sval);
        return status;
      }
      value->ncol = vlen;
      value->nrow = rank;
      if (rank != 1) value->dim = Array2D;
      last = value->link;
      value->link = NULL;
      status = caps_setValue(target, Double, rank, vlen, reals, NULL, units,
                             nErr, errors);
      value->link = last;
      if (sval->length > 1) EG_free(sval->vals.reals);
      EG_free(sval);
      if (status != CAPS_SUCCESS) return status;
      
    } else {

      if (source->subtype == UNCONNECTED) return CAPS_BADMETHOD;

      /* Integrate / weighted average */
      status = caps_integrateData(source, method, &rank, &ireals, &iunits);
      if (status != CAPS_SUCCESS) return status;
      status = caps_makeVal(Double, rank, ireals, &sval);
      if (status != CAPS_SUCCESS) {
        EG_free(ireals);
        EG_free(iunits);
        return status;
      }
      sval->units = iunits;
      sval->nrow  = rank;
      sval->ncol  = 1;
      status = caps_compatValues(sval, value, problem);
      EG_free(sval->units);
      if (sval->length > 1) EG_free(sval->vals.reals);
      EG_free(sval);
      if (status != CAPS_SUCCESS) {
        EG_free(ireals);
        return status;
      }
      last = value->link;
      value->link = NULL;
      status = caps_setValue(target, Double, rank, 1, ireals, NULL, iunits,
                             nErr, errors);
      value->link = last;
      EG_free(ireals);
      if (status != CAPS_SUCCESS) return status;
    }

  }
  
  /* mark owner */
  caps_freeOwner(&target->last);
  problem->sNum    += 1;
  target->last.sNum = problem->sNum;
  caps_fillDateTime(target->last.datetime);
  
  /* invalidate any link if exists */
  value->linkMethod = Copy;
  value->link       = NULL;
  return CAPS_SUCCESS;
}


int
caps_makeLinkage(/*@null@*/ capsObject *link, enum capstMethod method,
                            capsObject *target)
{
  int          status, rank, vlen;
  const double *reals;
  const char   *units;
  capsValue    *value, *sval;
  capsObject   *source, *pobject;
  capsProblem  *problem;
  
  if (target              == NULL)         return CAPS_NULLOBJ;
  if (target->magicnumber != CAPSMAGIC)    return CAPS_BADOBJECT;
  if (target->type        != VALUE)        return CAPS_BADTYPE;
  if (target->subtype     == GEOMETRYOUT)  return CAPS_BADTYPE;
  if (target->subtype     == ANALYSISOUT)  return CAPS_BADTYPE;
  if (target->blind       == NULL)         return CAPS_NULLBLIND;
  value   = (capsValue *)   target->blind;
  if (value->type         == Value)        return CAPS_BADTYPE;
  status  = caps_findProblem(target, CAPS_MAKELINKAGE, &pobject);
  if (status              != CAPS_SUCCESS) return status;
  problem = (capsProblem *) pobject->blind;
  
  if (target->type == VALUE) {
    if (target->subtype == GEOMETRYIN) {
      if (pobject->subtype == STATIC)      return CAPS_READONLYERR;
      if (method != Copy)                  return CAPS_BADMETHOD;
    }
  }
  
  if (link == NULL) {
    /* mark owner */
    caps_freeOwner(&target->last);
    problem->sNum    += 1;
    target->last.sNum = problem->sNum;
    caps_fillDateTime(target->last.datetime);
    /* remove existing link */
    value->linkMethod = Copy;
    value->link       = NULL;
    return CAPS_SUCCESS;
  }
  
  /* look at link */
  if (link->magicnumber != CAPSMAGIC)       return CAPS_BADOBJECT;
  if (link->type        == VALUE) {

    if (target->subtype == USER)            return CAPS_BADTYPE;
    if (method != Copy)                     return CAPS_BADMETHOD;
    source = link;
    do {
      if (source->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
      if (source->type        != VALUE)     return CAPS_BADTYPE;
      if (source->blind       == NULL)      return CAPS_NULLBLIND;
      sval = (capsValue *) source->blind;
      if (sval->link          == target)    return CAPS_CIRCULARLINK;
      source = sval->link;
    } while (sval->link != NULL);
    if (sval->type            == Value)     return CAPS_BADTYPE;
    
    if (sval->type == Pointer) {
      /* must have the same "units" */
      if  ((sval->units  != NULL) || (value->units != NULL)) {
        if (sval->units  == NULL) return CAPS_UNITERR;
        if (value->units == NULL) return CAPS_UNITERR;
        if (strcmp(value->units, sval->units) != 0) return CAPS_UNITERR;
      }
    } else {
      /* make sure we are compatible */
      status = caps_compatValues(sval, value, problem);
      if (status != CAPS_SUCCESS) return status;
    }
  
  } else if (link->type == DATASET) {

    status = caps_getData(link, &vlen, &rank, &reals, &units);
    if (status != CAPS_SUCCESS) return status;
    
    /* check for compatibility */
    status = caps_makeVal(Double, vlen*rank, reals, &sval);
    if (status != CAPS_SUCCESS) return status;
    sval->units = (char *) units;
    if (rank != 1) {
      sval->dim  = Array2D;
      sval->nrow = rank;
      sval->ncol = vlen;
    }
    status = caps_compatValues(sval, value, problem);
    if (sval->length > 1) EG_free(sval->vals.reals);
    EG_free(sval);
    if (status != CAPS_SUCCESS) return status;
    
  } else {
    return CAPS_BADTYPE;
  }

  caps_freeOwner(&target->last);
  problem->sNum    += 1;
  target->last.sNum = problem->sNum;
  caps_fillDateTime(target->last.datetime);
  value->linkMethod = method;
  value->link       = link;
  return CAPS_SUCCESS;
}


int
caps_hasDot(const capsObject *vobj, int *ndot, char ***names)
{
  int        i, status;
  char       **namex;
  capsValue  *value;
  capsObject *pobject;
  
  *ndot  = 0;
  *names = NULL;
  if (vobj              == NULL)        return CAPS_NULLOBJ;
  if (vobj->magicnumber != CAPSMAGIC)   return CAPS_BADOBJECT;
  if (vobj->type        != VALUE)       return CAPS_BADTYPE;
  if (vobj->blind       == NULL)        return CAPS_NULLBLIND;
  value  = (capsValue *) vobj->blind;
  if (value->type       != DoubleDot)   return CAPS_BADTYPE;
  if (value->ndot       == 0)           return CAPS_SUCCESS;
  status = caps_findProblem(vobj, CAPS_HASDOT, &pobject);
  if (status            != CAPS_SUCCESS) return status;

  namex = (char **) EG_alloc(value->ndot*sizeof(char *));
  if (namex == NULL) return EGADS_MALLOC;
  
  for (i = 0; i < value->ndot; i++) namex[i] = value->dots[i].name;
  *ndot  = value->ndot;
  *names = namex;
  
  return CAPS_SUCCESS;
}


int
caps_getDot(const capsObject *vobj, const char *name, int *len, int *rank,
            double **dot, int *nErr, capsErrs **errors )
{
  int         i, ipmtr, irow, icol, status, nbody, buildTo, builtTo;
  capsValue   *value;
  capsProblem *problem;
  capsObject  *pobject;
  
  if  (nErr             == NULL)         return CAPS_NULLVALUE;
  if  (errors           == NULL)         return CAPS_NULLVALUE;
  if  (len              == NULL)         return CAPS_NULLVALUE;
  if  (rank             == NULL)         return CAPS_NULLVALUE;
  if  (dot              == NULL)         return CAPS_NULLVALUE;
  *nErr = 0;
  *errors = NULL;
  *len = *rank = 0;
  *dot = NULL;
  if (vobj              == NULL)         return CAPS_NULLOBJ;
  if (vobj->magicnumber != CAPSMAGIC)    return CAPS_BADOBJECT;
  if (vobj->type        != VALUE)        return CAPS_BADTYPE;
  if (vobj->blind       == NULL)         return CAPS_NULLBLIND;
  value  = (capsValue *) vobj->blind;
  if (value->type       != DoubleDot)    return CAPS_BADTYPE;
  if (value->ndot       == 0)            return CAPS_NOTFOUND;
  if (name == NULL)                      return CAPS_NULLNAME;
  status = caps_findProblem(vobj, CAPS_GETDOT, &pobject);
  if (status            != CAPS_SUCCESS) return status;
  problem = (capsProblem *) pobject->blind;
  
  /* make sure geometry is up-to-date */
  status = caps_build(pobject, nErr, errors);
  if ((status != CAPS_SUCCESS) && (status != CAPS_CLEAN)) return status;
  
  for (i = 0; i < value->ndot; i++) {
    if (strcmp(name, value->dots[i].name) != 0) continue;
    if ((value->dots[i].dot == NULL) && (vobj->subtype == GEOMETRYOUT)) {
      ipmtr = problem->regGIN[i].index;
      irow  = problem->regGIN[i].irow;
      icol  = problem->regGIN[i].icol;
      /* clear all then set */
      ocsmSetVelD(problem->modl, 0,     0,    0,    0.0);
      ocsmSetVelD(problem->modl, ipmtr, irow, icol, 1.0);
      buildTo = 0;
      nbody   = 0;
      status  = ocsmBuild(problem->modl, buildTo, &builtTo, &nbody, NULL);
/*    printf(" CAPS Info: parameter %d %d %d sensitivity status = %d\n",
             open, irow, icol, status);  */
      if (status != SUCCESS) return status;
      caps_geomOutSensit(problem, ipmtr, irow, icol);
    }
    *len  = value->length;
    *rank = value->dots[i].rank;
    *dot  = value->dots[i].dot;
    return CAPS_SUCCESS;
  }
  
  return CAPS_NOTFOUND;
}
