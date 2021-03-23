/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             Base Object Functions
 *
 *      Copyright 2014-2021, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef WIN32
/* needs Advapi32.lib & Ws2_32.lib */
#include <Windows.h>
#define unlink     _unlink
#define getpid     _getpid
#define snprintf   _snprintf
#define strcasecmp  stricmp
#else
#include <unistd.h>
#endif

#include "udunits.h"

#include "capsTypes.h"
#include "capsFunIDs.h"
#include "capsAIM.h"

/* OpenCSM Defines & Includes */
#include "common.h"
#include "OpenCSM.h"


#define STRING(a)       #a
#define STR(a)          STRING(a)

static char *CAPSprop[2] = {STR(CAPSPROP),
                        "\nCAPSprop: Copyright 2014-2021 MIT. All Rights Reserved."};

/*@-incondefs@*/
extern void ut_free(/*@only@*/ ut_unit* const unit);
/*@+incondefs@*/

extern /*@null@*/ /*@only@*/ char *EG_strdup(/*@null@*/ const char *str);

extern int  caps_filter(capsProblem *problem, capsAnalysis *analysis);
extern int  caps_Aprx1DFree(/*@only@*/ capsAprx1D *approx);
extern int  caps_Aprx2DFree(/*@only@*/ capsAprx2D *approx);
extern int  caps_build(capsObject *pobject, int *nErr, capsErrs **errors);

/* used internally */
int caps_freeError(/*@only@*/ capsErrs *errs);



void
caps_getStaticStrings(char ***signature, char **pID, char **user)
{
#ifdef WIN32
  int     len;
  WSADATA wsaData;
#endif
  char    name[257], ID[301];

  *signature = CAPSprop;
  
#ifdef WIN32
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
  gethostname(name, 257);
#ifdef WIN32
  WSACleanup();
#endif
  snprintf(ID, 301, "%s:%d", name, getpid());
  *pID = EG_strdup(ID);

#ifdef WIN32
  len = 256;
  GetUserName(name, &len);
#else
  /*@-mustfreefresh@*/
  snprintf(name, 257, "%s", getlogin());
  /*@+mustfreefresh@*/
#endif
  *user = EG_strdup(name);
}


void
caps_fillDateTime(short *datetime)
{
#ifdef WIN32
  SYSTEMTIME sysTime;

  GetLocalTime(&sysTime);
  datetime[0] = sysTime.wYear;
  datetime[1] = sysTime.wMonth;
  datetime[2] = sysTime.wDay;
  datetime[3] = sysTime.wHour;
  datetime[4] = sysTime.wMinute;
  datetime[5] = sysTime.wSecond;

#else
  time_t     tloc;
  struct tm  tim, *dum;
  
  tloc = time(NULL);
/*@-unrecog@*/
  dum  = localtime_r(&tloc, &tim);
/*@+unrecog@*/
  if (dum == NULL) {
    datetime[0] = 1900;
    datetime[1] = 0;
    datetime[2] = 0;
    datetime[3] = 0;
    datetime[4] = 0;
    datetime[5] = 0;
  } else {
    datetime[0] = tim.tm_year + 1900;
    datetime[1] = tim.tm_mon  + 1;
    datetime[2] = tim.tm_mday;
    datetime[3] = tim.tm_hour;
    datetime[4] = tim.tm_min;
    datetime[5] = tim.tm_sec;
  }
#endif
}


void
caps_fillLengthUnits(capsProblem *problem, ego body, char **lunits)
{
  int          status, atype, alen;
  const int    *aints;
  const double *areals;
  const char   *astr;
  ut_unit      *utunit;

  *lunits = NULL;
  status  = EG_attributeRet(body, "capsLength", &atype, &alen, &aints, &areals,
                            &astr);
  if ((status != EGADS_SUCCESS) && (status != EGADS_NOTFOUND)) {
    printf(" CAPS Warning: EG_attributeRet = %d in fillLengthUnits!\n", status);
    return;
  }
  if (status == EGADS_NOTFOUND) return;
  if (atype != ATTRSTRING) {
    printf(" CAPS Warning: capsLength w/ incorrect type in fillLengthUnits!\n");
    return;
  }
  
  utunit = ut_parse((ut_system *) problem->utsystem, astr, UT_ASCII);
  if (utunit == NULL) {
    printf(" CAPS Warning: capsLength %s is not a valid unit!\n", astr);
    return;
  }
/*
  printf(" Body with length units = %s\n", astr);
 */
  ut_free(utunit);
  *lunits = EG_strdup(astr);
}


void
caps_geomOutUnits(char *name, /*@null@*/ char *lunits, char **units)
{
  int         i, len;
  static char *names[21] = {"@xmin", "@xmax", "@ymin", "@ymax", "@zmin",
                            "@zmax", "@length", "@area", "@volume", "@xcg",
                            "@ycg", "@zcg", "@Ixx", "@Ixy", "@Ixz", "@Iyx",
                            "@Iyy", "@Iyz", "@Izx", "@Izy", "@Izz" };
  static int  power[21]  = {1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1, 1, 4, 4, 4, 4, 4,
                            4, 4, 4, 4};

  *units = NULL;
  if (lunits == NULL) return;
  
  for (i = 0; i < 21; i++)
    if (strcmp(name, names[i]) == 0) {
      if (power[i] == 1) {
        *units = EG_strdup(lunits);
      } else {
        len    = strlen(lunits) + 3;
        *units = (char *) EG_alloc(len*sizeof(char));
        if (*units == NULL) return;
        snprintf(*units, len, "%s^%d", lunits, power[i]);
      }
      return;
    }
}


int
caps_makeTuple(int n, capsTuple **tuple)
{
  int       i;
  capsTuple *tmp;
  
  *tuple = NULL;
  if (n < 1) return CAPS_RANGEERR;
  
  tmp = (capsTuple *) EG_alloc(n*sizeof(capsTuple));
  if (tmp == NULL) return EGADS_MALLOC;
  
  for (i = 0; i < n; i++) tmp[i].name = tmp[i].value = NULL;
  *tuple = tmp;
  
  return CAPS_SUCCESS;
}


void
caps_freeTuple(int n, /*@only@*/ capsTuple *tuple)
{
  int i;
  
  if (tuple == NULL) return;
  for (i = 0; i < n; i++) {
    if (tuple[i].name  != NULL) EG_free(tuple[i].name);
    if (tuple[i].value != NULL) EG_free(tuple[i].value);
  }
  EG_free(tuple);
}


void
caps_freeOwner(capsOwn *own)
{
  if (own->pname != NULL) {
    EG_free(own->pname);
    own->pname = NULL;
  }
  if (own->pID != NULL) {
    EG_free(own->pID);
    own->pID = NULL;
  }
  if (own->user != NULL) {
    EG_free(own->user);
    own->user = NULL;
  }
}


void
caps_freeAttrs(egAttrs **attrx)
{
  int     i;
  egAttrs *attrs;
  
  attrs = *attrx;
  if (attrs == NULL) return;
  
  *attrx = NULL;
  /* remove any attributes */
  for (i = 0; i < attrs->nseqs; i++) {
    EG_free(attrs->seqs[i].root);
    EG_free(attrs->seqs[i].attrSeq);
  }
  if (attrs->seqs != NULL) EG_free(attrs->seqs);
  for (i = 0; i < attrs->nattrs; i++) {
    if (attrs->attrs[i].name != NULL) EG_free(attrs->attrs[i].name);
    if (attrs->attrs[i].type == ATTRINT) {
      if (attrs->attrs[i].length > 1) EG_free(attrs->attrs[i].vals.integers);
    } else if (attrs->attrs[i].type == ATTRREAL) {
      if (attrs->attrs[i].length > 1) EG_free(attrs->attrs[i].vals.reals);
    } else {
      EG_free(attrs->attrs[i].vals.string);
    }
  }
  EG_free(attrs->attrs);
  EG_free(attrs);
}


void
caps_freeValueObjects(int vflag, int nObjs, capsObject **objects)
{
  int       i, j;
  capsValue *value, *vArray;

  if (objects == NULL) return;
  vArray = (capsValue *) objects[0]->blind;

  for (i = 0; i < nObjs; i++) {

    /* clean up any allocated data arrays */
    value = (capsValue *) objects[i]->blind;
    if (value != NULL) {
      objects[i]->blind = NULL;
      if ((value->type == Boolean) || (value->type == Integer)) {
        if (value->length > 1) EG_free(value->vals.integers);
      } else if ((value->type == Double) || (value->type == DoubleDot)) {
        if (value->length > 1) EG_free(value->vals.reals);
      } else if (value->type == String) {
        if (value->vals.string != NULL) EG_free(value->vals.string);
      } else if (value->type == Tuple) {
        caps_freeTuple(value->length, value->vals.tuple);
      } else if (value->type == Value) {
        if (value->length > 1) {
          caps_freeValueObjects(vflag, value->length, value->vals.objects);
          EG_free(value->vals.objects);
        } else {
          caps_freeValueObjects(vflag, 1, &value->vals.object);
        }
      } else {
        /* pointer type -- nothing should be done here... */
      }
      if (value->units   != NULL) EG_free(value->units);
      if (value->partial != NULL) EG_free(value->partial);
      if (value->dots    != NULL) {
        for (j = 0; j < value->ndot; j++) {
          if (value->dots[j].name != NULL) EG_free(value->dots[j].name);
          if (value->dots[j].dot  != NULL) EG_free(value->dots[j].dot);
        }
        EG_free(value->dots);
      }
      if (vflag == 1) EG_free(value);
    }
    
    /* cleanup and invalidate the object */
    caps_freeAttrs(&objects[i]->attrs);
    caps_freeOwner(&objects[i]->last);
    objects[i]->magicnumber = 0;
    EG_free(objects[i]->name);
    objects[i]->name = NULL;
    EG_free(objects[i]);
  }
  
  if (vflag == 0) EG_free(vArray);
  EG_free(objects);
}


static void
caps_freeEleType(capsEleType *eletype)
{
  eletype->nref  = 0;
  eletype->ndata = 0;
  eletype->ntri  = 0;
  eletype->nmat  = 0;

  EG_free(eletype->gst);
  eletype->gst = NULL;
  EG_free(eletype->dst);
  eletype->dst = NULL;
  EG_free(eletype->matst);
  eletype->matst = NULL;
  EG_free(eletype->tris);
  eletype->tris = NULL;
}


void
caps_freeDiscr(capsDiscr *discr)
{
  int           i;
  capsBodyDiscr *discBody = NULL;


  /* free up this capsDiscr */

  EG_free(discr->verts);
  discr->verts = NULL;
  EG_free(discr->celem);
  discr->celem = NULL;
  EG_free(discr->dtris);
  discr->dtris = NULL;

  discr->nPoints = 0;
  discr->nVerts  = 0;
  discr->nDtris  = 0;

  /* free Types */
  if (discr->types != NULL) {
    for (i = 0; i < discr->nTypes; i++)
      caps_freeEleType(discr->types + i);
    EG_free(discr->types);
    discr->types = NULL;
  }
  discr->nTypes = 0;

  EG_free(discr->tessGlobal);
  discr->tessGlobal = NULL;

  /* free Body discretizations */
  for (i = 0; i < discr->nBodys; i++) {
    discBody = &discr->bodys[i];
    EG_free(discBody->elems);
    EG_free(discBody->gIndices);
    EG_free(discBody->dIndices);
    EG_free(discBody->poly);
  }
  EG_free(discr->bodys);
  discr->bodys  = NULL;
  discr->nBodys = 0;

  /* aim must free discr->ptrm */
  if (discr->ptrm != NULL)
    printf(" CAPS Warning: discr->ptrm is not NULL (caps_freeDiscr)!\n");
}


void
caps_freeAnalysis(int flag, /*@only@*/ capsAnalysis *analysis)
{
  int         i, j, state, npts;
  const char  *eType[4] = {"Info",
                           "Warning",
                           "Error",
                           "Possible Developer Error"};
  ego body;
  capsProblem *problem;

  if (analysis == NULL) return;
  
  problem = analysis->info.problem;
  if (analysis->instStore != NULL)
    aim_cleanup(problem->aimFPTR, analysis->loadName, analysis->instStore);
  for (i = 0; i < analysis->nField; i++) EG_free(analysis->fields[i]);
  EG_free(analysis->fields);
  EG_free(analysis->ranks);
  if (analysis->intents  != NULL) EG_free(analysis->intents);
  if (analysis->loadName != NULL) EG_free(analysis->loadName);
  if (analysis->path     != NULL) EG_free(analysis->path);
  if (analysis->parents  != NULL) EG_free(analysis->parents);
  if (analysis->bodies   != NULL) EG_free(analysis->bodies);

  if (analysis->tess != NULL) {
    for (j = 0; j < analysis->nTess; j++)
      if (analysis->tess[j] != NULL) {
        /* delete the body in the tessellation if it's not on the OpenCSM stack */
        body = NULL;
        if (j >= analysis->nBody) {
          (void) EG_statusTessBody(analysis->tess[j], &body, &state, &npts);
        }
        EG_deleteObject(analysis->tess[j]);
        analysis->tess[j] = NULL;
        if (body != NULL) EG_deleteObject(body);
      }
    EG_free(analysis->tess);
    analysis->tess   = NULL;
    analysis->nTess  = 0;
  }

  if (analysis->info.errs.errors != NULL) {
    printf(" Note: Lost AIM Communication ->\n");
    for (i = 0; i < analysis->info.errs.nError; i++) {
      for (j = 0; j < analysis->info.errs.errors[i].nLines; j++) {
        if (j == 0) {
          printf("   %s: %s\n", eType[analysis->info.errs.errors[i].eType],
                 analysis->info.errs.errors[i].lines[j]);
        } else {
          printf("            %s\n", analysis->info.errs.errors[i].lines[j]);
        }
        EG_free(analysis->info.errs.errors[i].lines[j]);
      }
      EG_free(analysis->info.errs.errors[i].lines);
    }
    EG_free(analysis->info.errs.errors);
    analysis->info.errs.nError = 0;
    analysis->info.errs.errors = NULL;
  }
  if (flag == 1) return;

  if (analysis->analysisIn != NULL)
    caps_freeValueObjects(0, analysis->nAnalysisIn,  analysis->analysisIn);
  
  if (analysis->analysisOut != NULL)
    caps_freeValueObjects(0, analysis->nAnalysisOut, analysis->analysisOut);
  
  caps_freeOwner(&analysis->pre);
  EG_free(analysis);
}


int
caps_makeObject(capsObject **objs)
{
  capsObject *objects;

  *objs   = NULL;
  objects = (capsObject *) EG_alloc(sizeof(capsObject));
  if (objects == NULL) return EGADS_MALLOC;

  objects->magicnumber = CAPSMAGIC;
  objects->type        = UNUSED;
  objects->subtype     = NONE;
  objects->sn          = 0;
  objects->name        = NULL;
  objects->attrs       = NULL;
  objects->blind       = NULL;
  objects->parent      = NULL;
  objects->last.pname  = NULL;
  objects->last.pID    = NULL;
  objects->last.user   = NULL;
  objects->last.sNum   = 0;
  caps_fillDateTime(objects->last.datetime);

  *objs = objects;
  return CAPS_SUCCESS;
}


int
caps_makeVal(enum capsvType type, int len, const void *data, capsValue **val)
{
  char             *chars;
  int              *ints, j;
  double           *reals;
  enum capsBoolean *bools;
  capsValue        *value;
  capsObject       **objs;
  capsTuple        *tuple;
  
  *val  = NULL;
  if (data  == NULL) return CAPS_NULLVALUE;
  value = (capsValue *) EG_alloc(sizeof(capsValue));
  if (value == NULL) return EGADS_MALLOC;
  value->length  = len;
  value->type    = type;
  value->nrow    = value->ncol   = value->dim = value->pIndex = 0;
  value->lfixed  = value->sfixed = Fixed;
  value->nullVal = NotAllowed;
  value->units   = NULL;
  value->link    = NULL;
  value->limits.dlims[0] = value->limits.dlims[1] = 0.0;
  value->linkMethod      = Copy;
  value->gInType         = 0;
  value->partial         = NULL;
  value->ndot            = 0;
  value->dots            = NULL;

  if (data == NULL) {
    value->nullVal = IsNull;
    if (type == Boolean) {
      if (value->length <= 1) {
        value->vals.integer = false;
      } else {
        value->vals.integers = (int *) EG_alloc(value->length*sizeof(int));
        if (value->vals.integers == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.integers[j] = false;
      }
    } else if (type == Integer) {
      if (value->length <= 1) {
        value->vals.integer = 0;
      } else {
        value->vals.integers = (int *) EG_alloc(value->length*sizeof(int));
        if (value->vals.integers == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.integers[j] = 0;
      }
    } else if ((type == Double) || (type == DoubleDot)) {
      if (value->length <= 1) {
        value->vals.real = 0.0;
      } else {
        value->vals.reals = (double *) EG_alloc(value->length*sizeof(double));
        if (value->vals.reals == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.reals[j] = 0.0;
      }
    } else if (type == String) {
      if (len <= 0) value->length = 1;
      value->vals.string = (char *) EG_alloc(value->length*sizeof(char));
      if (value->vals.string == NULL) {
        EG_free(value);
        return EGADS_MALLOC;
      }
      for (j = 0; j < value->length; j++) value->vals.string[j] = 0;
      if (value->length == 1) value->length = 2;
    } else if (type == Tuple) {
      value->vals.tuple = NULL;
      if (len > 0) {
        j = caps_makeTuple(len, &value->vals.tuple);
        if ((j != CAPS_SUCCESS) || (value->vals.tuple == NULL)) {
          if (value->vals.tuple == NULL) j = CAPS_NULLVALUE;
          EG_free(value);
          return j;
        }
      }
    } else if (type == Pointer) {
      value->vals.AIMptr = NULL;
    } else {
      if (value->length <= 1) {
        value->vals.object = NULL;
      } else {
        value->vals.objects = (capsObject **)
                              EG_alloc(value->length*sizeof(capsObject *));
        if (value->vals.objects == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.objects[j] = NULL;
      }
    }
    
  } else {
    
    if (type == Boolean) {
      bools = (enum capsBoolean *) data;
      if (value->length == 1) {
        value->vals.integer = bools[0];
      } else {
        value->vals.integers = (int *) EG_alloc(value->length*sizeof(int));
        if (value->vals.integers == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.integers[j] = bools[j];
      }
    } else if (type == Integer) {
      ints = (int *) data;
      if (value->length == 1) {
        value->vals.integer = ints[0];
      } else {
        value->vals.integers = (int *) EG_alloc(value->length*sizeof(int));
        if (value->vals.integers == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.integers[j] = ints[j];
      }
    } else if ((type == Double) || (type == DoubleDot)) {
      reals = (double *) data;
      if (value->length == 1) {
        value->vals.real = reals[0];
      } else {
        value->vals.reals = (double *) EG_alloc(value->length*sizeof(double));
        if (value->vals.reals == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.reals[j] = reals[j];
      }
    } else if (type == String) {
      chars = (char *) data;
      value->length      = strlen(chars)+1;
      value->vals.string = (char *) EG_alloc(value->length*sizeof(char));
      if (value->vals.string == NULL) {
        EG_free(value);
        return EGADS_MALLOC;
      }
      for (j = 0; j < value->length; j++) value->vals.string[j] = chars[j];
      if (value->length == 1) value->length = 2;
    } else if (type == Tuple) {
      value->vals.tuple = NULL;
      if (len > 0) {
        j = caps_makeTuple(len, &value->vals.tuple);
        if ((j != CAPS_SUCCESS) || (value->vals.tuple == NULL)) {
          if (value->vals.tuple == NULL) j = CAPS_NULLVALUE;
          EG_free(value);
          return j;
        }
        tuple = (capsTuple *) data;
        for (j = 0; j < len; j++) {
          value->vals.tuple[j].name  = EG_strdup(tuple[j].name);
          value->vals.tuple[j].value = EG_strdup(tuple[j].value);
          if ((tuple[j].name  != NULL) &&
              (value->vals.tuple[j].name  == NULL)) {
            EG_free(value);
            return EGADS_MALLOC;
          }
          if ((tuple[j].value != NULL) &&
              (value->vals.tuple[j].value == NULL)) {
            EG_free(value);
            return EGADS_MALLOC;
          }
        }
      }
    } else if (type == Pointer) {
      value->vals.AIMptr = (void *) data;
    } else {
      objs = (capsObject **) data;
      if (value->length == 1) {
        value->vals.object = objs[0];
      } else {
        value->vals.objects = (capsObject **)
        EG_alloc(value->length*sizeof(capsObject *));
        if (value->vals.objects == NULL) {
          EG_free(value);
          return EGADS_MALLOC;
        }
        for (j = 0; j < value->length; j++) value->vals.objects[j] = objs[j];
      }
    }
  }
  if (value->length > 1) value->dim = Vector;
    
  *val = value;
  return CAPS_SUCCESS;
}


int
caps_findProblem(const capsObject *object, int funID, capsObject **pobject)
{
  capsObject  *pobj;
  capsProblem *problem;
  
  *pobject = NULL;
  if (object == NULL) return CAPS_NULLOBJ;

  pobj = (capsObject *) object;
  do {
    if (pobj->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
    if (pobj->type        == PROBLEM) {
      if (pobj->blind == NULL) return CAPS_NULLBLIND;
      problem  = (capsProblem *) pobj->blind;
      problem->funID = funID;
      *pobject = pobj;
      return CAPS_SUCCESS;
    }
    pobj = pobj->parent;
  } while (pobj != NULL);

  return CAPS_NOTPROBLEM;
}


int
caps_makeSimpleErr(/*@null@*/ capsObject *object, const char *line1,
                   /*@null@*/ const char *line2, /*@null@*/ const char *line3,
                   /*@null@*/ const char *line4, capsErrs **errs)
{
  int      i;
  capsErrs *error;
  
  *errs = NULL;
  error = (capsErrs *) EG_alloc(sizeof(capsErrs));
  if (error == NULL) return EGADS_MALLOC;
  error->nError = 1;
  error->errors = (capsError *) EG_alloc(sizeof(capsError));
  if (error->errors == NULL) {
    EG_free(error);
    return EGADS_MALLOC;
  }
  
  i = 0;
  if (line1 != NULL) i++;
  if (line2 != NULL) i++;
  if (line3 != NULL) i++;
  if (line4 != NULL) i++;
  error->errors->errObj = object;
  error->errors->index  = 0;
  error->errors->eType  = CERROR;
  error->errors->nLines = i;
  error->errors->lines  = NULL;
  if (i != 0) {
    error->errors->lines = (char **) EG_alloc(i*sizeof(char *));
    if (error->errors->lines == NULL) {
      EG_free(error->errors);
      EG_free(error);
      return EGADS_MALLOC;
    }
    i = 0;
    if (line1 != NULL) {
      error->errors->lines[i] = EG_strdup(line1);
      i++;
    }
    if (line2 != NULL) {
      error->errors->lines[i] = EG_strdup(line2);
      i++;
    }
    if (line3 != NULL) {
      error->errors->lines[i] = EG_strdup(line3);
      i++;
    }
    if (line4 != NULL) {
      error->errors->lines[i] = EG_strdup(line4);
      i++;
    }
  }
  
  *errs = error;
  return CAPS_SUCCESS;
}


/************************* CAPS exposed functions ****************************/


void
caps_revision(int *major, int *minor)
{
  *major = CAPSMAJOR;
  *minor = CAPSMINOR;
}


int
caps_info(const capsObject *object, char **name, enum capsoType *type,
          enum capssType *subtype, capsObject **link, capsObject **parent,
          capsOwn *last)
{
  int        i, stat;
  capsValue  *value;
  capsObject *pobj;
  
  *name      = NULL;
  *type      = UNUSED;
  *subtype   = NONE;
  *link      = *parent     = NULL;
  last->user = last->pname = last->pID = NULL;
  for (i = 0; i < 6; i++) last->datetime[i] = 0;

  if (object              == NULL)      return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if (object->blind       == NULL)      return CAPS_NULLBLIND;

  stat     = caps_findProblem(object, CAPS_INFO, &pobj);
  if (stat != CAPS_SUCCESS) return stat;
  *type    = object->type;
  *subtype = object->subtype;
  *name    = object->name;
  *parent  = object->parent;
  if (object->type == VALUE) {
    value  = (capsValue *) object->blind;
    *link  = value->link;
  }
  if (object->last.pname == NULL) {
    if (stat == CAPS_SUCCESS) {
      last->pname = pobj->last.pname;
      last->user  = pobj->last.user;
      last->pID   = pobj->last.pID;
    }
  } else {
    last->pname = object->last.pname;
    last->user  = object->last.user;
    last->pID   = object->last.pID;
  }
  last->sNum = object->last.sNum;
  for (i = 0; i < 6; i++) last->datetime[i] = object->last.datetime[i];
  
  return CAPS_SUCCESS;
}


int
caps_size(const capsObject *object, enum capsoType type, enum capssType stype,
          int *size, int *nErr, capsErrs **errors)
{
  int           i, status;
  egAttrs       *attrs;
  capsObject    *pobject;
  capsProblem   *problem;
  capsValue     *value;
  capsAnalysis  *analysis;
  capsBound     *bound;
  capsVertexSet *vertexSet;

  *nErr = 0;
  *errors = NULL;
  *size = 0;
  if (object              == NULL)      return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if (object->blind       == NULL)      return CAPS_NULLBLIND;
  
  if (object->type == PROBLEM) {
  
    problem = (capsProblem *) object->blind;
    problem->funID = CAPS_SIZE;
    if (type == BODIES) {
      /* make sure geometry is up-to-date */
      status = caps_build((capsObject *) object, nErr, errors);
      if ((status != CAPS_SUCCESS) && (status != CAPS_CLEAN)) return status;
      *size = problem->nBodies;
    } else if (type == ATTRIBUTES) {
      attrs = object->attrs;
      if (attrs != NULL) *size = attrs->nattrs;
    } else if (type == VALUE) {
      if (stype == GEOMETRYIN)  *size = problem->nGeomIn;
      if (stype == GEOMETRYOUT) *size = problem->nGeomOut;
      if (stype == BRANCH)      *size = problem->nBranch;
      if (stype == PARAMETER)   *size = problem->nParam;
    } else if (type == ANALYSIS) {
      *size = problem->nAnalysis;
    } else if (type == BOUND) {
      *size = problem->nBound;
    }
    
  } else if (object->type == VALUE) {

    status = caps_findProblem(object, CAPS_SIZE, &pobject);
    if (status != CAPS_SUCCESS) return status;
    value = (capsValue *) object->blind;
    if (type == ATTRIBUTES) {
      attrs = object->attrs;
      if (attrs != NULL) *size = attrs->nattrs;
    } else if ((type == VALUE) && (value->type == Value)) {
      *size = value->length;
    }
    
  } else if (object->type == ANALYSIS) {

    status = caps_findProblem(object, CAPS_SIZE, &pobject);
    if (status != CAPS_SUCCESS) return status;
    analysis = (capsAnalysis *) object->blind;
    if (type == ATTRIBUTES) {
      attrs = object->attrs;
      if (attrs != NULL) *size = attrs->nattrs;
    } else if (type == VALUE) {
      if (stype == ANALYSISIN)  *size = analysis->nAnalysisIn;
      if (stype == ANALYSISOUT) *size = analysis->nAnalysisOut;
    } else if (type == BODIES) {

      /* make sure geometry is up-to-date */
      status = caps_build(pobject, nErr, errors);
      if ((status != CAPS_SUCCESS) && (status != CAPS_CLEAN)) return status;

      problem = (capsProblem *) pobject->blind;
      if ((problem->nBodies > 0) && (problem->bodies != NULL)) {
        if (analysis->bodies == NULL) {
          status = caps_filter(problem, analysis);
          if (status != CAPS_SUCCESS) return status;
        }
        *size = analysis->nBody;
      }
    }
    
  } else if (object->type == BOUND) {
    
    status = caps_findProblem(object, CAPS_SIZE, &pobject);
    if (status != CAPS_SUCCESS) return status;
    if (type == ATTRIBUTES) {
      attrs = object->attrs;
      if (attrs != NULL) *size = attrs->nattrs;
    } else if (type == VERTEXSET) {
      bound = (capsBound *) object->blind;
      for (*size = i = 0; i < bound->nVertexSet; i++)
        if (bound->vertexSet[i]->subtype == stype) *size += 1;
    }
    
  } else if (object->type == VERTEXSET) {
    
    status = caps_findProblem(object, CAPS_SIZE, &pobject);
    if (status != CAPS_SUCCESS) return status;
    if (type == ATTRIBUTES) {
      attrs = object->attrs;
      if (attrs != NULL) *size = attrs->nattrs;
    } else if (type == DATASET) {
      vertexSet = (capsVertexSet *) object->blind;
      *size = vertexSet->nDataSets;
    }
    
  } else if (object->type == DATASET) {

    status = caps_findProblem(object, CAPS_SIZE, &pobject);
    if (status != CAPS_SUCCESS) return status;
    if (type == ATTRIBUTES) {
      attrs = object->attrs;
      if (attrs != NULL) *size = attrs->nattrs;
    }

  }

  return CAPS_SUCCESS;
}


int
caps_childByIndex(const capsObject *object, enum capsoType type,
                  enum capssType stype, int index, capsObject **child)
{
  int           i, j, stat;
  capsObject    *pobject;
  capsProblem   *problem;
  capsValue     *value;
  capsAnalysis  *analysis;
  capsBound     *bound;
  capsVertexSet *vertexSet;
  
  *child = NULL;
  if (object              == NULL)      return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if (object->blind       == NULL)      return CAPS_NULLBLIND;
  if (index               <= 0)         return CAPS_RANGEERR;

  stat = caps_findProblem(object, CAPS_CHILDBYINDEX, &pobject);
  if (stat != CAPS_SUCCESS) return stat;
  
  if (object->type == PROBLEM) {
    
    problem = (capsProblem *) object->blind;
    if (type == VALUE) {
      if (stype == GEOMETRYIN) {
        if (index > problem->nGeomIn)   return CAPS_RANGEERR;
        *child = problem->geomIn[index-1];
      }
      if (stype == GEOMETRYOUT) {
        if (index > problem->nGeomOut)  return CAPS_RANGEERR;
        *child = problem->geomOut[index-1];
      }
      if (stype == BRANCH) {
        if (index > problem->nBranch)   return CAPS_RANGEERR;
        *child = problem->branchs[index-1];
      }
      if (stype == PARAMETER) {
        if (index > problem->nParam)    return CAPS_RANGEERR;
        *child = problem->params[index-1];
      }
    } else if (type == ANALYSIS) {
      if (index > problem->nAnalysis)   return CAPS_RANGEERR;
      *child = problem->analysis[index-1];
    } else if (type == BOUND) {
      if (index  > problem->nBound)     return CAPS_RANGEERR;
      *child = problem->bounds[index-1];
    }
    
  } else if (object->type == VALUE) {
    
    value = (capsValue *) object->blind;
    if ((type == VALUE) && (value->type == Value)) {
      if (index > value->length)        return CAPS_RANGEERR;
      if (value->length == 1) *child = value->vals.object;
      if (value->length != 1) *child = value->vals.objects[index-1];
    }
    
  } else if (object->type == ANALYSIS) {
    
    analysis = (capsAnalysis *) object->blind;
    if (type == VALUE) {
      if (stype == ANALYSISIN) {
        if (index > analysis->nAnalysisIn) return CAPS_RANGEERR;
        *child = analysis->analysisIn[index-1];
      }
      if (stype == ANALYSISOUT) {
        if (index > analysis->nAnalysisOut) return CAPS_RANGEERR;
        *child = analysis->analysisOut[index-1];
      }
    }
    
  } else if (object->type == BOUND) {
    
    bound = (capsBound *) object->blind;
    for (j = i = 0; i < bound->nVertexSet; i++) {
      if (bound->vertexSet[i]->subtype == stype) j++;
      if (j != index) continue;
      *child = bound->vertexSet[i];
      break;
    }
    
  } else if (object->type == VERTEXSET) {
    
    vertexSet = (capsVertexSet *) object->blind;
    if (type == DATASET) {
      if (index > vertexSet->nDataSets) return CAPS_RANGEERR;
      *child = vertexSet->dataSets[index-1];
    }
    
  }
  
  if (*child == NULL) return CAPS_NOTFOUND;
  return CAPS_SUCCESS;
}



static int
caps_findByName(const char *name, int len, capsObject **objs,
                capsObject **child)
{
  int i;
  
  if (objs == NULL) return CAPS_NOTFOUND;
  
  for (i = 0; i < len; i++) {
    if (objs[i]       == NULL) continue;
    if (objs[i]->name == NULL) continue;
    if (strcmp(objs[i]->name, name) == 0) {
      *child = objs[i];
      return CAPS_SUCCESS;
    }
  }
  
  return CAPS_NOTFOUND;
}


int
caps_childByName(const capsObject *object, enum capsoType type,
                 enum capssType stype, const char *name, capsObject **child)
{
  int           stat;
  capsObject    *pobject, **objs;
  capsProblem   *problem;
  capsValue     *value;
  capsAnalysis  *analysis;
  capsVertexSet *vertexSet;
  
  *child = NULL;
  if (object              == NULL)      return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if (object->blind       == NULL)      return CAPS_NULLBLIND;
  if (name                == NULL)      return CAPS_NULLNAME;
  
  stat = caps_findProblem(object, CAPS_CHILDBYINDEX, &pobject);
  if (stat != CAPS_SUCCESS) return stat;
  
  if (object->type == PROBLEM) {
    
    problem = (capsProblem *) object->blind;
    if (type == VALUE) {
      if (stype == GEOMETRYIN)
        return caps_findByName(name, problem->nGeomIn,
                                     problem->geomIn,  child);
      if (stype == GEOMETRYOUT)
        return caps_findByName(name, problem->nGeomOut,
                                     problem->geomOut, child);
      if (stype == BRANCH)
        return caps_findByName(name, problem->nBranch,
                                     problem->branchs, child);
      if (stype == PARAMETER)
        return caps_findByName(name, problem->nParam,
                                     problem->params,  child);
    } else if (type == ANALYSIS) {
      return caps_findByName(name, problem->nAnalysis,
                                   problem->analysis,  child);
    } else if (type == BOUND) {
      return caps_findByName(name, problem->nBound,
                                   problem->bounds,    child);
    }
    
  } else if (object->type == VALUE) {
    
    value = (capsValue *) object->blind;
    if ((type == VALUE) && (value->type == Value)) {
      if (value->length == 1) objs = &value->vals.object;
      if (value->length != 1) objs =  value->vals.objects;
      return caps_findByName(name, value->length, objs, child);
    }
    
  } else if (object->type == ANALYSIS) {
    
    analysis = (capsAnalysis *) object->blind;
    if (type == VALUE) {
      if (stype == ANALYSISIN)
        return caps_findByName(name, analysis->nAnalysisIn,
                                     analysis->analysisIn,  child);
      if (stype == ANALYSISOUT) {
        return caps_findByName(name, analysis->nAnalysisOut,
                                     analysis->analysisOut, child);
      }
    }
    
  } else if (object->type == VERTEXSET) {
    
    vertexSet = (capsVertexSet *) object->blind;
    if (type == DATASET)
      return caps_findByName(name, vertexSet->nDataSets,
                                   vertexSet->dataSets, child);
    
  }

  return CAPS_NOTFOUND;
}


int
caps_bodyByIndex(const capsObject *object, int index, ego *body, char **lunits)
{
  int          i, status;
  capsObject   *pobject;
  capsProblem  *problem;
  capsAnalysis *analysis;
  
  *body   = NULL;
  *lunits = NULL;
  if  (object              == NULL)      return CAPS_NULLOBJ;
  if  (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if ((object->type        != PROBLEM) &&
      (object->type        != ANALYSIS)) return CAPS_BADTYPE;
  if  (object->blind       == NULL)      return CAPS_NULLBLIND;

  status = caps_findProblem(object, CAPS_BODYBYINDEX, &pobject);
  if (status != CAPS_SUCCESS) return status;
  
  if (object->type == PROBLEM) {
    
    if (index               <= 0)        return CAPS_RANGEERR;
    problem = (capsProblem *) object->blind;
    if (index > problem->nBodies)        return CAPS_RANGEERR;
  
    *body   = problem->bodies[index-1];
    *lunits = problem->lunits[index-1];
    
  } else {
    
    if (index             <= 0)          return CAPS_RANGEERR;
    analysis = (capsAnalysis *) object->blind;
    problem  = (capsProblem *) pobject->blind;
    if ((problem->nBodies > 0) && (problem->bodies != NULL)) {
      if (analysis->bodies == NULL) {
        status = caps_filter(problem, analysis);
        if (status != CAPS_SUCCESS) return status;
      }
      if (index > analysis->nBody)         return CAPS_RANGEERR;
      *body = analysis->bodies[index-1];
      for (i = 0; i < problem->nBodies; i++)
        if (*body == problem->bodies[i]) {
          *lunits = problem->lunits[i];
          break;
        }
    }
    
  }
  
  return CAPS_SUCCESS;
}


int
caps_ownerInfo(const capsOwn owner, char **pname, char **pID, char **userID,
               short *datetime, CAPSLONG *sNum)
{
  int i;
  
  *pname  = owner.pname;
  *pID    = owner.pID;
  *userID = owner.user;
  *sNum   = owner.sNum;
  for (i = 0; i < 6; i++) datetime[i] = owner.datetime[i];
  
  return CAPS_SUCCESS;
}


int
caps_setOwner(const capsObject *pobject, const char *pname, capsOwn *owner)
{
  char        **signature;
  capsProblem *problem;

  if (pobject              == NULL)      return CAPS_NULLOBJ;
  if (pobject->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if (pobject->type        != PROBLEM)   return CAPS_BADTYPE;
  if (pobject->blind       == NULL)      return CAPS_NULLBLIND;
  if (pname                == NULL)      return CAPS_NULLNAME;
  problem = (capsProblem *) pobject->blind;

  problem->funID = CAPS_SETOWNER;
  problem->sNum++;
  caps_getStaticStrings(&signature, &owner->pID, &owner->user);
  owner->pname = EG_strdup(pname);
  owner->sNum  = problem->sNum;
  caps_fillDateTime(owner->datetime);
  return CAPS_SUCCESS;
}


int
caps_delete(capsObject *object)
{
  int           i, j, k, status;
  capsObject    *pobject;
  capsProblem   *problem;
  capsValue     *value;
  capsBound     *bound;
  capsAnalysis  *analysis;
  capsVertexSet *vertexSet;
  capsDataSet   *dataSet;
  
  if  (object              == NULL)      return CAPS_NULLOBJ;
  if  (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if ((object->type        != VALUE) &&
      (object->type        != BOUND))    return CAPS_BADTYPE;
  if ((object->type        == VALUE) &&
      (object->subtype     != USER))     return CAPS_BADTYPE;
  if  (object->blind       == NULL)      return CAPS_NULLBLIND;

  status = caps_findProblem(object, CAPS_DELETE, &pobject);
  if (status != CAPS_SUCCESS) return status;

  /* clean up any allocated data arrays */
  if (object->type == VALUE) {
    
    value = (capsValue *) object->blind;
    object->blind = NULL;
    if ((value->type == Boolean) || (value->type == Integer)) {
      if (value->length > 1) EG_free(value->vals.integers);
    } else if ((value->type == Double) || (value->type == DoubleDot)) {
      if (value->length > 1) EG_free(value->vals.reals);
    } else if (value->type == String) {
      if (value->length > 1) EG_free(value->vals.string);
    } else if (value->type == Tuple) {
      caps_freeTuple(value->length, value->vals.tuple);
    } else if (value->type == Pointer) {
      /* do nothing */
    } else {
      if (value->length > 1) EG_free(value->vals.objects);
    }
    if (value->units   != NULL) EG_free(value->units);
    if (value->partial != NULL) EG_free(value->partial);
    if (value->dots    != NULL) {
      for (i = 0; i < value->ndot; i++) {
        if (value->dots[i].name != NULL) EG_free(value->dots[i].name);
        if (value->dots[i].dot  != NULL) EG_free(value->dots[i].dot);
      }
      EG_free(value->dots);
    }
    EG_free(value);
    
  } else {

    problem = (capsProblem *) pobject->blind;
    bound   = (capsBound *)    object->blind;
    if (bound->curve   != NULL) caps_Aprx1DFree(bound->curve);
    if (bound->surface != NULL) caps_Aprx2DFree(bound->surface);
    if (bound->lunits  != NULL) EG_free(bound->lunits);
    for (i = 0; i < bound->nVertexSet; i++) {
      if (bound->vertexSet[i]->magicnumber != CAPSMAGIC) continue;
      if (bound->vertexSet[i]->blind       == NULL)      continue;

      vertexSet = (capsVertexSet *) bound->vertexSet[i]->blind;
      if ((vertexSet->analysis != NULL) && (vertexSet->discr != NULL))
        if (vertexSet->analysis->blind != NULL) {
          analysis = (capsAnalysis *) vertexSet->analysis->blind;
          aim_FreeDiscr(problem->aimFPTR, analysis->loadName, vertexSet->discr);
          EG_free(vertexSet->discr);
        }
      for (j = 0; j < vertexSet->nDataSets; j++) {
        if (vertexSet->dataSets[j]->magicnumber != CAPSMAGIC) continue;
        if (vertexSet->dataSets[j]->blind       == NULL)      continue;

        dataSet = (capsDataSet *) vertexSet->dataSets[j]->blind;
        for (k = 0; k < dataSet->nHist; k++)
          caps_freeOwner(&dataSet->history[k]);
        if (dataSet->data    != NULL) EG_free(dataSet->data);
        if (dataSet->units   != NULL) EG_free(dataSet->units);
        if (dataSet->history != NULL) EG_free(dataSet->history);
        if (dataSet->startup != NULL) EG_free(dataSet->startup);
        EG_free(dataSet);
        
        caps_freeAttrs(&vertexSet->dataSets[j]->attrs);
        caps_freeOwner(&vertexSet->dataSets[j]->last);
        vertexSet->dataSets[j]->magicnumber = 0;
        EG_free(vertexSet->dataSets[j]->name);
        vertexSet->dataSets[j]->name = NULL;
        EG_free(vertexSet->dataSets[j]);
      }
      EG_free(vertexSet->dataSets);
      vertexSet->dataSets = NULL;
      EG_free(vertexSet);

      caps_freeAttrs(&bound->vertexSet[i]->attrs);
      caps_freeOwner(&bound->vertexSet[i]->last);
      bound->vertexSet[i]->magicnumber = 0;
      EG_free(bound->vertexSet[i]->name);
      bound->vertexSet[i]->name = NULL;
      EG_free(bound->vertexSet[i]);
    }
    EG_free(bound->vertexSet);
    bound->vertexSet = NULL;
    EG_free(bound);

    /* remove the bound from the list of bounds in the problem */
    for (i = j = 0; i < problem->nBound; i++) {
      if (problem->bounds[i] == object) continue;
      problem->bounds[j++] = problem->bounds[i];
    }
    problem->nBound--;
  }
  
  /* cleanup and invalidate the object */
  
  caps_freeAttrs(&object->attrs);
  caps_freeOwner(&object->last);
  object->magicnumber = 0;
  EG_free(object->name);
  object->name = NULL;
  EG_free(object);
  
  return CAPS_SUCCESS;
}


int
caps_errorInfo(capsErrs *errs, int eIndex, capsObject **errObj,
               int *nLines, char ***lines)
{
  *errObj = NULL;
  *nLines = 0;
  *lines  = NULL;
  if ((eIndex < 1) || (eIndex > errs->nError)) return CAPS_BADINDEX;
  
  *errObj = errs->errors[eIndex-1].errObj;
  *nLines = errs->errors[eIndex-1].nLines;
  *lines  = errs->errors[eIndex-1].lines;
  return CAPS_SUCCESS;
}


int
caps_freeError(/*@only@*/ capsErrs *errs)
{
  int i, j;
  
  if (errs == NULL) return CAPS_SUCCESS;
  for (i = 0; i < errs->nError; i++) {
    for (j = 0; j < errs->errors[i].nLines; j++) {
      EG_free(errs->errors[i].lines[j]);
    }

    EG_free(errs->errors[i].lines);
  }
  
  EG_free(errs->errors);
  errs->nError = 0;
  errs->errors = NULL;
  EG_free(errs);

  return CAPS_SUCCESS;
}


int
caps_writeParameters(const capsObject *pobject, char *filename)
{
  capsProblem *problem;
  
  if (pobject              == NULL)       return CAPS_NULLOBJ;
  if (pobject->magicnumber != CAPSMAGIC)  return CAPS_BADOBJECT;
  if (pobject->type        != PROBLEM)    return CAPS_BADTYPE;
  if (pobject->subtype     != PARAMETRIC) return CAPS_BADTYPE;
  if (pobject->blind       == NULL)       return CAPS_NULLBLIND;
  if (filename             == NULL)       return CAPS_NULLNAME;
  
  problem = (capsProblem *) pobject->blind;
  problem->funID = CAPS_WRITEPARAMETERS;
  return ocsmSaveDespmtrs(problem->modl, filename);
}


int
caps_readParameters(const capsObject *pobject, char *filename)
{
  int         i, j, k, n, type, nrow, ncol, fill, status;
  char        name[MAX_NAME_LEN];
  double      real, dot, *reals;
  capsObject  *object;
  capsProblem *problem;
  capsValue   *value;
  
  if (pobject              == NULL)       return CAPS_NULLOBJ;
  if (pobject->magicnumber != CAPSMAGIC)  return CAPS_BADOBJECT;
  if (pobject->type        != PROBLEM)    return CAPS_BADTYPE;
  if (pobject->subtype     != PARAMETRIC) return CAPS_BADTYPE;
  if (pobject->blind       == NULL)       return CAPS_NULLBLIND;
  if (filename             == NULL)       return CAPS_NULLNAME;
  
  problem = (capsProblem *) pobject->blind;
  problem->funID = CAPS_READPARAMETERS;
  status  = ocsmUpdateDespmtrs(problem->modl, filename);
  if (status < SUCCESS) {
    printf(" CAPS Error: ocsmSaveDespmtrs = %d (caps_readParameters)!\n",
           status);
    return status;
  }
  
  /* need to reload all GeomIn Values */

  problem->sNum += 1;
  for (i = 0; i < problem->nGeomIn; i++) {
    object = problem->geomIn[i];
    if (object              == NULL)      continue;
    if (object->magicnumber != CAPSMAGIC) continue;
    if (object->type        != VALUE)     continue;
    if (object->blind       == NULL)      continue;
    value = (capsValue *) object->blind;
    if (value               == NULL)      continue;
    if (value->type         != Double)    continue;
    status = ocsmGetPmtr(problem->modl, value->pIndex, &type,
                         &nrow, &ncol, name);
    if (status              != SUCCESS)   continue;
    fill = 0;

    /* has the shape changed? */
    if ((nrow != value->nrow) || (ncol != value->ncol)) {
      reals = NULL;
      if (nrow*ncol != 1) {
        reals = (double *) EG_alloc(nrow*ncol*sizeof(double));
        if (reals == NULL) {
          printf(" CAPS Warning: %s resize %d %d Malloc(caps_readParameters)\n",
                 object->name, nrow, ncol);
          continue;
        }
      }
      if (value->length != 1) EG_free(value->vals.reals);
      value->length = nrow*ncol;
      value->nrow   = nrow;
      value->ncol   = ncol;
      if (value->length != 1) value->vals.reals = reals;
      fill = 1;
    }
    
    /* check if values changed */
    if (fill == 0) {
      reals = value->vals.reals;
      if (value->length == 1) reals = &value->vals.real;
      for (n = k = 0; k < nrow; k++) {
        for (j = 0; j < ncol; j++, n++) {
          status = ocsmGetValu(problem->modl, value->pIndex, k+1, j+1,
                               &real, &dot);
          if (status != SUCCESS) {
            printf(" CAPS Warning: %s GetValu[%d,%d] = %d (caps_readParameters)\n",
                   object->name, k+1, j+1, status);
            continue;
          }
          if (real != reals[n]) {
            fill = 1;
            break;
          }
        }
        if (fill == 1) break;
      }
    }
    
    /* refill the values */
    if (fill == 0) continue;
    reals = value->vals.reals;
    if (value->length == 1) reals = &value->vals.real;
    for (n = k = 0; k < nrow; k++)
      for (j = 0; j < ncol; j++, n++)
        ocsmGetValu(problem->modl, value->pIndex, k+1, j+1, &reals[n], &dot);
    
    caps_freeOwner(&object->last);
    object->last.sNum = problem->sNum;
    caps_fillDateTime(object->last.datetime);
  }
  
  return CAPS_SUCCESS;
}


int
caps_writeGeometry(const capsObject *object, int flag, const char *filename,
                   int *nErr, capsErrs **errors)
{
  int          i, j, n, len, idot, stat;
/*  char         *name; */
  ego          context, model, *bodies, *newBodies;
  capsObject   *pobject;
  capsProblem  *problem;
  capsAnalysis *analysis;
  FILE         *fp;
  
  *nErr    = 0;
  *errors  = NULL;
  if  (object              == NULL)      return CAPS_NULLOBJ;
  if  (object->magicnumber != CAPSMAGIC) return CAPS_BADOBJECT;
  if ((object->type        != PROBLEM) &&
      (object->type        != ANALYSIS)) return CAPS_BADTYPE;
  if  (object->blind       == NULL)      return CAPS_NULLBLIND;
  if  (filename            == NULL)      return CAPS_NULLNAME;

  stat = caps_findProblem(object, CAPS_WRITEGEOMETRY, &pobject);
  if (stat != CAPS_SUCCESS) return stat;
  
  /* find the extension */
  len = strlen(filename);
  for (idot = len-1; idot > 0; idot--)
    if (filename[idot] == '.') break;
  if (idot == 0) return CAPS_BADNAME;
/*@-unrecog@*/
  if ((strcasecmp(&filename[idot],".iges")  != 0) &&
      (strcasecmp(&filename[idot],".igs")   != 0) &&
      (strcasecmp(&filename[idot],".step")  != 0) &&
      (strcasecmp(&filename[idot],".stp")   != 0) &&
      (strcasecmp(&filename[idot],".brep")  != 0) &&
      (strcasecmp(&filename[idot],".egads") != 0)) return CAPS_BADNAME;
/*@+unrecog@*/
  if (strcasecmp(&filename[idot],".egads") == 0)
    if ((flag != 0) && (flag != 1)) return CAPS_RANGEERR;
  
  /* make sure geometry is up-to-date */
  stat = caps_build(pobject, nErr, errors);
  if ((stat != CAPS_SUCCESS) && (stat != CAPS_CLEAN)) return stat;

  if (object->type == PROBLEM) {
    problem  = (capsProblem *) object->blind;
    n        = problem->nBodies;
    bodies   = problem->bodies;
  } else {
    analysis = (capsAnalysis *) object->blind;
    problem  = (capsProblem *) pobject->blind;
    if (analysis->bodies == NULL) {
      stat = caps_filter(problem, analysis);
      if (stat != CAPS_SUCCESS) return stat;
    }
    n        = analysis->nBody;
    bodies   = analysis->bodies;
  }
  context    = problem->context;
  
  if (n <= 0) return CAPS_NOBODIES;

  /* remove existing file (if any) */
  fp = fopen(filename, "r");
  if (fp != NULL) {
    fclose(fp);
    unlink(filename);
  }
  
  if (n == 1) {
    stat =  EG_saveModel(bodies[0], filename);
    if (stat != EGADS_SUCCESS) {
      printf(" CAPS Error: EG_saveModel = %d (caps_writeGeometry)!\n", stat);
      return stat;
    }
  } else {
    newBodies = (ego *) EG_alloc(n*sizeof(ego));
    if (newBodies == NULL) {
      printf(" CAPS Error: MALLOC on %d Bodies (caps_writeGeometry)!\n", n);
      return EGADS_MALLOC;
    }
    for (i = 0; i < n; i++) {
      stat = EG_copyObject(bodies[i], NULL, &newBodies[i]);
      if (stat != EGADS_SUCCESS) {
        printf(" CAPS Error: EG_copyObject %d/%d = %d (caps_writeGeometry)!\n",
               i+1, n, stat);
        for (j = 0; j < i; j++) EG_deleteObject(newBodies[j]);
        EG_free(newBodies);
        return stat;
      }
    }
    /* make a Model */
    stat = EG_makeTopology(context, NULL, MODEL, 0, NULL, n, newBodies, NULL,
                           &model);
    EG_free(newBodies);
    if (stat != EGADS_SUCCESS) {
      printf(" CAPS Error: EG_makeTopology %d = %d (caps_writeGeometry)!\n",
             n, stat);
      return stat;
    }
    stat = EG_saveModel(model, filename);
    EG_deleteObject(model);
    if (stat != EGADS_SUCCESS) {
      printf(" CAPS Error: EG_saveModel = %d (caps_writeGeometry)!\n", stat);
      return stat;
    }
  }
  
  /* write out eto files? */

  if ((strcasecmp(&filename[idot],".egads") != 0) || (flag == 0) ||
      (object->type == PROBLEM)) return CAPS_SUCCESS;
  
  /* writing eto files is depricated and this needs to be reworked */
  return CAPS_IOERR;
#ifdef NEED_TO_RETHINK_THIS
  if (n == 1) {
    name = EG_strdup(filename);
    if (name == NULL) {
      printf(" CAPS warning: MALLOC on nam -- no eto files (caps_writeGeometry)!\n");
      return EGADS_MALLOC;
    }
    name[idot+2] = 't';
    name[idot+3] = 'o';
    name[idot+4] =  0;
    fp = fopen(name, "r");
    if (fp != NULL) {
      fclose(fp);
      unlink(name);
    }
    if (bodies[1] == NULL) {
      EG_free(name);
      return CAPS_SUCCESS;
    }
    stat = EG_saveTess(bodies[1], name);
    EG_free(name);
    return stat;
  }

  /* multiple bodies */
  name = (char *) EG_alloc((len+10)*sizeof(char));
  if (name == NULL) {
    printf(" CAPS warning: MALLOC on name -- no eto files (caps_writeGeometry)!\n");
    return EGADS_MALLOC;
  }
  for (i = 0; i < idot; i++) name[i] = filename[i];
  for (i = 0; i < n; i++) {
    snprintf(&name[idot], len+9, "%d.eto", i+1);
    fp = fopen(name, "r");
    if (fp != NULL) {
      fclose(fp);
      unlink(name);
    }
    if (bodies[n+i] == NULL) continue;
    stat = EG_saveTess(bodies[n+i], name);
    if (stat != EGADS_SUCCESS) {
      printf(" CAPS Warning: EG_saveTess %d/%d = %d (caps_writeGeometry)!\n",
             i+1, n, stat);
      EG_free(name);
      return stat;
    }
  }
  EG_free(name);
  
  return CAPS_SUCCESS;
#endif
}


int
caps_sensitivity(const capsObject *object, int irow, int icol, int funFlag,
                 int *nErr, capsErrs **errors)
{
  int         i, j, k, stat, index, len;
  char        *name;
  capsObject  *pobj;
  capsValue   *value, *vout;
  capsProblem *problem;
  capsRegGIN  *regGIN;
  capsDot     *dots;
  
  *nErr   = 0;
  *errors = NULL;
  if (object              == NULL)         return CAPS_NULLOBJ;
  if (object->magicnumber != CAPSMAGIC)    return CAPS_BADOBJECT;
  if ((object->type       != VALUE)  &&
      (object->subtype    != GEOMETRYIN))  return CAPS_BADTYPE;
  if (object->blind       == NULL)         return CAPS_NULLBLIND;
  if (object->name        == NULL)         return CAPS_NULLNAME;
  value = (capsValue *) object->blind;
  if ((irow < 1) || (irow > value->nrow))  return CAPS_BADINDEX;
  if ((icol < 1) || (icol > value->ncol))  return CAPS_BADINDEX;
  if ((funFlag < 0) || (funFlag > 2))      return CAPS_RANGEERR;
  stat = caps_findProblem(object, CAPS_SENSITIVITY, &pobj);
  if (stat                != CAPS_SUCCESS) return stat;
  problem = (capsProblem *) pobj->blind;

  /* make sure geometry is up-to-date */
  stat = caps_build(pobj, nErr, errors);
  if ((stat != CAPS_SUCCESS) && (stat != CAPS_CLEAN)) return stat;

/*
  printf(" caps_sensitivity called with %d %d %d\n", irow, icol, funFlag);
 */
  for (index = 1; index <= problem->nGeomIn; index++)
    if (problem->geomIn[index-1] == object) break;
  if (index > problem->nGeomIn) {
    printf(" CAPS Internal: Object Not Found in List (caps_sensitivity)!\n");
    return CAPS_NOTFOUND;
  }
  
  /* perform requested function */
  
  if (funFlag == 0) {
    
    /* Register the slot */
    for (i = 0; i < problem->nRegGIN; i++)
      if ((problem->regGIN[i].index == index) &&
          (problem->regGIN[i].irow  == irow)  &&
          (problem->regGIN[i].icol  == icol)) return CAPS_EXISTS;

    /* get the full vector/array name*/
    len = strlen(object->name);
    if ((value->nrow == 1) && (value->ncol == 1)) {
      name = EG_strdup(object->name);
    } else if (value->nrow == 1) {
      len += 10;
      name = (char *) EG_alloc(len*sizeof(char));
      if (name != NULL)
        snprintf(name, len, "%s[%d]", object->name, icol);
    } else if (value->ncol == 1) {
      len += 10;
      name = (char *) EG_alloc(len*sizeof(char));
      if (name != NULL)
        snprintf(name, len, "%s[%d]", object->name, irow);
    } else {
      len += 20;
      name = (char *) EG_alloc(len*sizeof(char));
      if (name != NULL)
        snprintf(name, len, "%s[%d,%d]", object->name, irow, icol);
    }
    if (name == NULL) {
      printf(" CAPS Error: Cannot Malloc %d for %s (caps_sensitivity)!\n", len,
             object->name);
      return EGADS_MALLOC;
    }
    
    /* make room */
    len = problem->nRegGIN + 1;
    if (problem->regGIN == NULL) {
      problem->regGIN = (capsRegGIN *) EG_alloc(sizeof(capsRegGIN));
      if (problem->regGIN == NULL) {
        printf(" CAPS Error: Cant Malloc registry for %s (caps_sensitivity)!\n",
               object->name);
        EG_free(name);
        return EGADS_MALLOC;
      }
      len = 1;
    } else {
      regGIN = (capsRegGIN *) EG_reall(problem->regGIN, len*sizeof(capsRegGIN));
      if (regGIN == NULL) {
        printf(" CAPS Error: Cant ReAlloc registry for %s (caps_sensitivity)!\n",
               object->name);
        EG_free(name);
        return EGADS_MALLOC;
      }
      problem->regGIN = regGIN;
    }
    
    for (i = 0; i < problem->nGeomOut; i++) {
      if (problem->geomOut[i]              == NULL)      continue;
      if (problem->geomOut[i]->magicnumber != CAPSMAGIC) continue;
      if (problem->geomOut[i]->blind       == NULL)      continue;
      vout = (capsValue *) problem->geomOut[i]->blind;
      if (vout->dots == NULL) {
        vout->dots = (capsDot *) EG_alloc(len*sizeof(capsDot));
        if (vout->dots == NULL) {
          printf(" CAPS Error: Cant Malloc dots for %s (caps_sensitivity)!\n",
                 problem->geomOut[i]->name);
          EG_free(name);
          return EGADS_MALLOC;
        }
      } else {
        dots = (capsDot *) EG_reall(vout->dots, len*sizeof(capsDot));
        if (dots == NULL) {
          printf(" CAPS Error: Cant Malloc dots for %s (caps_sensitivity)!\n",
                 problem->geomOut[i]->name);
          EG_free(name);
          return EGADS_MALLOC;
        }
        vout->dots = dots;
      }
      vout->dots[len-1].name = NULL;
      vout->dots[len-1].rank = 1;
      vout->dots[len-1].dot  = NULL;
      vout->ndot             = len;
      vout->type             = DoubleDot;
    }
    for (i = 0; i < problem->nGeomOut; i++) {
      if (problem->geomOut[i]              == NULL)      continue;
      if (problem->geomOut[i]->magicnumber != CAPSMAGIC) continue;
      if (problem->geomOut[i]->blind       == NULL)      continue;
      vout = (capsValue *) problem->geomOut[i]->blind;
      vout->dots[len-1].name = EG_strdup(name);
    }
    
    problem->regGIN[len-1].name  = name;
    problem->regGIN[len-1].index = index;
    problem->regGIN[len-1].irow  = irow;
    problem->regGIN[len-1].icol  = icol;
    problem->nRegGIN             = len;
    
  } else if (funFlag == 1) {
    
    /* call AIMs */
    
  } else {
    
    /* Delete the slot */
    for (j = 0; j < problem->nRegGIN; j++)
      if ((problem->regGIN[j].index == index) &&
          (problem->regGIN[j].irow  == irow)  &&
          (problem->regGIN[j].icol  == icol)) break;
    if (j == problem->nRegGIN) return CAPS_NOTFOUND;
    
    EG_free(problem->regGIN[j].name);
    for (i = j; i < problem->nRegGIN-1; i++)
      problem->regGIN[i] = problem->regGIN[i+1];
    problem->nRegGIN--;
    if (problem->nRegGIN == 0) {
      EG_free(problem->regGIN);
      problem->regGIN = NULL;
    }
    
    for (k = 0; k < problem->nGeomOut; k++) {
      if (problem->geomOut[k]              == NULL)      continue;
      if (problem->geomOut[k]->magicnumber != CAPSMAGIC) continue;
      if (problem->geomOut[k]->blind       == NULL)      continue;
      vout = (capsValue *) problem->geomOut[k]->blind;
      if (vout->dots[j].name != NULL) EG_free(vout->dots[j].name);
      if (vout->dots[j].dot  != NULL) EG_free(vout->dots[j].dot);
      for (i = j; i < problem->nRegGIN; i++)
        vout->dots[i] = vout->dots[i+1];
      vout->ndot = problem->nRegGIN;
      if (vout->ndot == 0) {
        EG_free(vout->dots);
        vout->dots = NULL;
        vout->type = Double;
      }
    }
  }

  return CAPS_SUCCESS;
}
