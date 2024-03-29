/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             Base level function Include
 *
 *      Copyright 2014-2020, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "capsTypes.h"

extern void caps_getStaticStrings(char ***signature, char **pID, char **user);
extern void caps_fillDateTime(short *datetime);
extern void caps_fillLengthUnits(capsProblem *problem, ego body, char **units);
extern void caps_geomOutUnits(char *name, /*@null@*/ char *lunits, char **units);
extern int  caps_makeTuple(int n,            capsTuple **tuple);
extern void caps_freeTuple(int n, /*@only@*/ capsTuple  *tuple);
extern void caps_freeOwner(capsOwn *own);
extern void caps_freeValueObjects(int vflag, int nObjs, capsObject **objs);
extern void caps_freeAnalysis(int flag, /*@only@*/ capsAnalysis *analysis);
extern void caps_freeAttrs(egAttrs **attrx);
extern int  caps_makeObject(capsObject **objs);
extern int  caps_makeVal(enum capsvType type, int len, const void *data,
                         capsValue **val);
extern int  caps_findProblem(const capsObject *object, capsObject **pobject);
extern int  caps_makeSimpleErr(/*@null@*/ capsObject *object, const char *line1,
                               /*@null@*/ const char *line2,
                               /*@null@*/ const char *line3,
                               /*@null@*/ const char *line4, capsErrs **errs);

extern int  caps_delete(capsObject *object);
