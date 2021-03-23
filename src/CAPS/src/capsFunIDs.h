#ifndef CAPSFUNIDS_H
#define CAPSFUNIDS_H
/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             Function ID Defines
 *
 *      Copyright 2014-2021, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */


/* base-level object functions */

#define CAPS_REVISION         1
#define CAPS_INFO             2
#define CAPS_SIZE             3
#define CAPS_CHILDBYINDEX     4
#define CAPS_CHILDBYNAME      5
#define CAPS_BODYBYINDEX      6
#define CAPS_OWNERINFO        7
#define CAPS_SETOWNER         8
#define CAPS_FREEOWNER        9
#define CAPS_DELETE          10
#define CAPS_ERRORINFO       11
#define CAPS_FREEERROR       12
#define CAPS_FREEVALUE       13


/* problem & I/O functions */

#define CAPS_OPEN             0
#define CAPS_START           20
#define CAPS_CLOSE           21
#define CAPS_OUTLEVEL        22
#define CAPS_WRITEPARAMETERS 23
#define CAPS_READPARAMETERS  24
#define CAPS_WRITEGEOMETRY   25
#define CAPS_SENSITIVITY     26
#define CAPS_SAVE            29


/* attribute functions */

#define CAPS_ATTRBYNAME      30
#define CAPS_ATTRBYINDEX     31
#define CAPS_SETATTR         32
#define CAPS_DELETEATTR      33


/* analysis functions */

#define CAPS_QUERYANALYSIS   40
#define CAPS_GETBODIES       41
#define CAPS_GETINPUT        42
#define CAPS_GETOUTPUT       43
#define CAPS_LOAD            44
#define CAPS_DUPANALYSIS     45
#define CAPS_DIRTYANALYSIS   46
#define CAPS_ANALYSISINFO    47
#define CAPS_PREANALYSIS     48
#define CAPS_RUNANALYSIS     49
#define CAPS_POSTANALYSIS    50
#define CAPS_AIMBACKDOOR     51
  
  
/* bound, vertexset and dataset functions */

#define CAPS_MAKEBOUND       60
#define CAPS_BOUNDINFO       61
#define CAPS_COMPLETEBOUND   62
#define CAPS_FILLVERTEXSETS  63
#define CAPS_MAKEVERTEXSET   64
#define CAPS_VERTEXSETINFO   65
#define CAPS_OUTPUTVERTEXSET 66
#define CAPS_FILLUNVERTEXSET 67
#define CAPS_MAKEDATASET     68
#define CAPS_INITDATASET     69
#define CAPS_SETDATA         70
#define CAPS_GETDATA         71
#define CAPS_GETDATASETS     72
#define CAPS_TRIANGULATE     73
#define CAPS_GETHISTORY      74

  
/* value functions */

#define CAPS_GETVALUE        80
#define CAPS_MAKEVALUE       81
#define CAPS_SETVALUE        82
#define CAPS_GETLIMITS       83
#define CAPS_SETLIMITS       84
#define CAPS_GETVALUESHAPE   85
#define CAPS_SETVALUESHAPE   86
#define CAPS_CONVERT         87
#define CAPS_TRANSFERVALUES  88
#define CAPS_MAKELINKAGE     89
#define CAPS_HASDOT          90
#define CAPS_GETDOT          91


#endif
