/*
 ************************************************************************
 *                                                                      *
 * udfEditAttr -- edit Attributes in a Body                             *
 *                                                                      *
 *            Written by John Dannenhoffer @ Syracuse University        *
 *            Patterned after code written by Bob Haimes  @ MIT         *
 *                                                                      *
 ************************************************************************
 */

/*
 * Copyright (C) 2011/2020  John F. Dannenhoffer, III (Syracuse University)
 *
 * This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *     MA  02110-1301  USA
 */

#define NUMUDPARGS 7
#define NUMUDPINPUTBODYS 1
#include "udpUtilities.h"

/* shorthands for accessing argument values and velocities */
#define ATTRNAME( IUDP)  ((char   *) (udps[IUDP].arg[0].val))
#define INPUT(    IUDP)  ((char   *) (udps[IUDP].arg[1].val))
#define OUTPUT(   IUDP)  ((char   *) (udps[IUDP].arg[2].val))
#define OVERWRITE(IUDP)  ((int    *) (udps[IUDP].arg[3].val))[0]
#define FILENAME( IUDP)  ((char   *) (udps[IUDP].arg[4].val))
#define VERBOSE(  IUDP)  ((int    *) (udps[IUDP].arg[5].val))[0]
#define NCHANGE(  IUDP)  ((double *) (udps[IUDP].arg[6].val))[0]

/* data about possible arguments */
static char  *argNames[NUMUDPARGS] = {"attrname",  "input",     "output",   "overwrite", "filename", "verbose", "nchange", };
static int    argTypes[NUMUDPARGS] = {ATTRSTRING,  ATTRSTRING,  ATTRSTRING, ATTRINT,     ATTRSTRING, ATTRINT,   -ATTRREAL  };
static int    argIdefs[NUMUDPARGS] = {0,           0,           0,          0,           0,          0,         0,         };
static double argDdefs[NUMUDPARGS] = {0.,          0.,          0.,         0.,          0.,         0.,        0.,        };

/* get utility routines: udpErrorStr, udpInitialize, udpReset, udpSet,
                         udpGet, udpVel, udpClean, udpMesh */
#include "udpUtilities.c"

#include "OpenCSM.h"

#define           MIN(A,B)        (((A) < (B)) ? (A) : (B))
#define           MAX(A,B)        (((A) < (B)) ? (B) : (A))
#define           EPS06           1.0e-6

/* prototype for function defined below */
static int editAttrs(ego ebody, char attrname[], char input[], char output[],
                     int overwrite, int *nchange);
static int processFile(ego context, ego ebody, char filename[], int *nchange);
static int matches(char pattern[], const char string[]);
static int editEgo(const char attrname[], int atype, int alen,
                     const int *tempIlist, const double *tempRlist, const char *tempClist,
                     ego eobj, int overwrite, int *nchange);
static int getToken(char *text, int nskip, char sep, int maxtok, char *token);
static int getBodyTopos(ego ebody, ego esrc, int oclass, int *nlist, ego **elist);


/*
 ************************************************************************
 *                                                                      *
 *   udpExecute - execute the primitive                                 *
 *                                                                      *
 ************************************************************************
 */

int
udpExecute(ego  emodel,                 /* (in)  Model containing Body */
           ego  *ebody,                 /* (out) Body pointer */
           int  *nMesh,                 /* (out) number of associated meshes */
           char *string[])              /* (out) error message */
{
    int     status = EGADS_SUCCESS;

    int     oclass, mtype, nchild, *senses, nchange;
    double  data[4];
    ego     context, eref, *ebodys;

    ROUTINE(udpExecute);

    /* --------------------------------------------------------------- */

#ifdef DEBUG
    printf("udpExecute(emodel=%llx)\n", (long long)emodel);
    printf("attrname( 0) = %s\n", ATTRNAME( 0));
    printf("input(    0) = %s\n", INPUT(    0));
    printf("output(   0) = %s\n", OUTPUT(   0));
    printf("overwrite(0) = %d\n", OVERWRITE(0));
    printf("filename( 0) = %s\n", FILENAME( 0));
    printf("verbose(  0) = %d\n", VERBOSE(  0));
#endif

    /* default return values */
    *ebody  = NULL;
    *nMesh  = 0;
    *string = NULL;

    /* check that Model was input that contains one Body */
    status = EG_getTopology(emodel, &eref, &oclass, &mtype,
                            data, &nchild, &ebodys, &senses);
    if (status < EGADS_SUCCESS) goto cleanup;

    if (oclass != MODEL) {
        printf(" udpExecute: expecting a Model\n");
        status = EGADS_NOTMODEL;
        goto cleanup;
    } else if (nchild != 1) {
        printf(" udpExecute: expecting Model to contain one Body (not %d)\n", nchild);
        status = EGADS_NOTBODY;
        goto cleanup;
    }

    status = EG_getContext(emodel, &context);
    if (status < EGADS_SUCCESS) goto cleanup;

    /* if filename is specified, then process it and skip everything else */
    if (STRLEN(FILENAME(0)) > 0) {

    /* check arguments */
    } else {
        if        (STRLEN(INPUT( 0)) == 1) {
        } else if (STRLEN(OUTPUT(0)) == 1) {
        } else if (STRLEN(INPUT( 0)) != STRLEN(OUTPUT(0))) {
            printf(" udpExecute: input and output should be same length\n");
            status  = EGADS_RANGERR;
            goto cleanup;
        }

        if        (udps[0].arg[3].size >= 2) {
            printf(" udpExecute: overwrite should be a scalar\n");
            status  = EGADS_RANGERR;
            goto cleanup;

        } else if (NINT(OVERWRITE(0)) < 0 && NINT(OVERWRITE(1)) > 4) {
            printf(" udpExecute: overwrite = %d should be between 0 and 4\n", OVERWRITE(0));
            status  = EGADS_RANGERR;
            goto cleanup;

        }
    }

    /* cache copy of arguments for future use */
    status = cacheUdp();
    if (status < 0) {
        printf(" udpExecute: problem caching arguments\n");
        goto cleanup;
    }

#ifdef DEBUG
    printf("attrname( %d) = %s\n", numUdp, ATTRNAME( numUdp));
    printf("input(    %d) = %s\n", numUdp, INPUT(    numUdp));
    printf("output(   %d) = %s\n", numUdp, OUTPUT(   numUdp));
    printf("overwrite(%d) = %d\n", numUdp, OVERWRITE(numUdp));
    printf("filename( %d) = %s\n", numUdp, FILENAME( numUdp));
    printf("verbose(  %d) = %d\n", numUdp, VERBOSE(  numUdp));
#endif

    /* make a copy of the Body (so that it does not get removed
     when OpenCSM deletes emodel) */
    status = EG_copyObject(ebodys[0], NULL, ebody);
    if (status < EGADS_SUCCESS) goto cleanup;

    /* edit the attributes */
    if (STRLEN(FILENAME(numUdp)) > 0) {
        status = processFile(context, *ebody, FILENAME(numUdp), &nchange);
        if (status < 0) {
            printf(" udpExecute: problem in processFile\n");
            goto cleanup;
        }
    } else {
        status = editAttrs(*ebody, ATTRNAME(numUdp), INPUT(numUdp), OUTPUT(numUdp), OVERWRITE(numUdp), &nchange);
        if (status < 0) {
            printf(" udpExecute: problem in editAttrs\n");
            goto cleanup;
        }
    }

    /* add a special Attribute to the Body to tell OpenCSM that there
       is no topological change and hence it should not adjust the
       Attributes on the Body in finishBody() */
    status = EG_attributeAdd(*ebody, "__noTopoChange__", ATTRSTRING,
                             0, NULL, NULL, "udfEditAttr");
    if (status < 0) {
        printf(" udpExecute: problem setting __noTopoChange__\n");
        goto cleanup;
    }

    /* set the output value */
    NCHANGE(0) = (double)nchange;

    /* the copy of the Body that was annotated is returned */
    udps[numUdp].ebody = *ebody;

cleanup:
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
    }

    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpSensitivity - return sensitivity derivatives for the "real" argument *
 *                                                                      *
 ************************************************************************
 */

int
udpSensitivity(ego    ebody,            /* (in)  Body pointer */
   /*@unused@*/int    npnt,             /* (in)  number of points */
   /*@unused@*/int    entType,          /* (in)  OCSM entity type */
   /*@unused@*/int    entIndex,         /* (in)  OCSM entity index (bias-1) */
   /*@unused@*/double uvs[],            /* (in)  parametric coordinates for evaluation */
   /*@unused@*/double vels[])           /* (out) velocities */
{
    int iudp, judp;

    ROUTINE(udpSensitivity);

    /* --------------------------------------------------------------- */

    /* check that ebody matches one of the ebodys */
    iudp = 0;
    for (judp = 1; judp <= numUdp; judp++) {
        if (ebody == udps[judp].ebody) {
            iudp = judp;
            break;
        }
    }
    if (iudp <= 0) {
        return EGADS_NOTMODEL;
    }

    /* this routine is not written yet */
    return EGADS_NOLOAD;
}


/*
 ************************************************************************
 *                                                                      *
 *   editAttrs - edit the Attributes                                    *
 *                                                                      *
 ************************************************************************
 */

static int
editAttrs(ego    ebody,                 /* (in)  EGADS Body */
          char   attrname[],            /* (in)  name of attribute to edit */
          char   input[],               /* (in)  types of entity to edit from */
          char   output[],              /* (in)  types of entity to edit to */
          int    overwrite,             /* (in)  =0 do not overwrite */
                                        /*       =1        overwrite */
                                        /*       =2        use smaller value */
                                        /*       =3        use larger  value */
                                        /*       =4        use sum or concatenation */
          int    *nchange)              /* (out) number of changes */
{
    int       status = 0;               /* (out) return status */

    int       nnode, inode, nedge, iedge, nface, iface, nlist, ilist;
    int       nedit, iedit, nattr, iattr, atype, alen;
    CINT      *tempIlist;
    CDOUBLE   *tempRlist;
    char      intype, outtype;
    CCHAR     *tempClist, *aname;
    ego       *enodes=NULL, *eedges=NULL, *efaces=NULL, *elist=NULL;

    ROUTINE(editAttrs);

    /* --------------------------------------------------------------- */

    /* initialize the output */
    *nchange = 0;

    /* get the list of Nodes, Edges, and Faces associated with ebody */
    status = EG_getBodyTopos(ebody, NULL, NODE, &nnode, &enodes);
    if (status < EGADS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(ebody, NULL, EDGE, &nedge, &eedges);
    if (status < EGADS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
    if (status < EGADS_SUCCESS) goto cleanup;

    /* determone number of edits */
    nedit = MAX(STRLEN(input), STRLEN(output));

    /* loop through all the edits */
    for (iedit = 0; iedit < nedit; iedit++) {
        if (STRLEN(input) > 1) {
            intype = input[iedit];
        } else {
            intype = input[0];
        }

        if (STRLEN(output) > 1) {
            outtype = output[iedit];
        } else {
            outtype = output[0];
        }

        /* delete an attribute */
        if (intype == 'd' || intype == 'D') {

            /* delete Body attribute */
            if (outtype == 'b' || outtype == 'B') {
                status = EG_attributeNum(ebody, &nattr);
                CHECK_STATUS(EG_attributeNum);

                for (iattr = nattr; iattr > 0; iattr--) {
                    status = EG_attributeGet(ebody, iattr, &aname, &atype, &alen,
                                             &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(EG_attributeGet);

                    if (matches(attrname, aname)) {
                        status = EG_attributeDel(ebody, aname);
                        CHECK_STATUS(EG_attributeDel);

                        (*nchange)++;
                    }
                }

            /* delete Node attribute */
            } else if (outtype == 'n' || outtype == 'N') {
                for (inode = 0; inode < nnode; inode++) {
                    status = EG_attributeNum(enodes[inode], &nattr);
                    CHECK_STATUS(EG_attributeNum);

                    for (iattr = nattr; iattr > 0; iattr--) {
                        status = EG_attributeGet(enodes[inode], iattr, &aname, &atype, &alen,
                                                 &tempIlist, &tempRlist, &tempClist);
                        CHECK_STATUS(EG_attributeGet);

                        if (matches(attrname, aname)) {
                            status = EG_attributeDel(enodes[inode], aname);
                            CHECK_STATUS(EG_attributeDel);

                            (*nchange)++;
                        }
                    }
                }

            /* delete Edge attribute */
            } else if (outtype == 'e' || outtype == 'E') {
                for (iedge = 0; iedge < nedge; iedge++) {
                    status = EG_attributeNum(eedges[iedge], &nattr);
                    CHECK_STATUS(EG_attributeNum);

                    for (iattr = nattr; iattr > 0; iattr--) {
                        status = EG_attributeGet(eedges[iedge], iattr, &aname, &atype, &alen,
                                                 &tempIlist, &tempRlist, &tempClist);
                        CHECK_STATUS(EG_attributeGet);

                        if (matches(attrname, aname)) {
                            status = EG_attributeDel(eedges[iedge], aname);
                            CHECK_STATUS(EG_attributeDel);

                            (*nchange)++;
                        }
                    }
                }

            /* delete Face attribute */
            } else if (outtype == 'f' || outtype == 'F') {
                for (iface = 0; iface < nface; iface++) {
                    status = EG_attributeNum(efaces[iface], &nattr);
                    CHECK_STATUS(EG_attributeNum);

                    for (iattr = nattr; iattr > 0; iattr--) {
                        status = EG_attributeGet(efaces[iface], iattr, &aname, &atype, &alen,
                                                 &tempIlist, &tempRlist, &tempClist);
                        CHECK_STATUS(EG_attributeGet);

                        if (matches(attrname, aname)) {
                            status = EG_attributeDel(efaces[iface], aname);
                            CHECK_STATUS(EG_attributeDel);

                            (*nchange)++;
                        }
                    }
                }
            } else {
                printf(" outype=%c is not valid for intype=D\n", outtype);
                status = OCSM_UDP_ERROR1;
                goto cleanup;
            }

        /* propagate attribute from the Body */
        } else if (intype == 'b' || intype == 'B') {

            /* look through all Body Attributes, looking for a match */
            status = EG_attributeNum(ebody, &nattr);
            CHECK_STATUS(EG_attributeNum);

            for (iattr = 1; iattr <= nattr; iattr++) {
                status = EG_attributeGet(ebody, iattr, &aname, &atype, &alen,
                                         &tempIlist, &tempRlist, &tempClist);
                CHECK_STATUS(EG_attributeGet);

                if (matches(attrname, aname)) {

                    /* propagate from Body to Body */
                    if (outtype == 'b' || outtype == 'B') {

                    /* propagate from Body to Node */
                    } else if (outtype == 'n' || outtype == 'N') {
                        for (inode = 0; inode < nnode; inode++) {
                            status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                             enodes[inode], overwrite, nchange);
                            CHECK_STATUS(editEgo);
                        }

                    /* propagate from Body to Edge */
                    } else if (outtype == 'e' || outtype == 'E') {
                        for (iedge = 0; iedge < nedge; iedge++) {
                            status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                             eedges[iedge], overwrite, nchange);
                            CHECK_STATUS(editEgo);
                        }

                    /* propagate from Body to Face */
                    } else if (outtype == 'f' || outtype == 'F') {
                        for (iface = 0; iface < nface; iface++) {
                            status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                             efaces[iface], overwrite, nchange);
                            CHECK_STATUS(editEgo);
                        }

                    } else {
                        printf(" outtype=%c is not valid for intype=B\n", outtype);
                        status = OCSM_UDP_ERROR1;
                        goto cleanup;
                    }
                }
            }

        /* edit propagate attribute from the Nodes */
        } else if (intype == 'n' || intype == 'N') {
            for (inode = 0; inode < nnode; inode++) {

                /* look through all Node Attributes, looking for a match */
                status = EG_attributeNum(enodes[inode], &nattr);
                CHECK_STATUS(EG_attributeNum);

                for (iattr = 1; iattr <= nattr; iattr++) {
                    status = EG_attributeGet(enodes[inode], iattr, &aname, &atype, &alen,
                                             &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(EG_attributeGet);

                    if (matches(attrname, aname)) {

                        /* propagate from Node to Body */
                        if (outtype == 'b' || outtype == 'B') {
                            status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                             ebody, overwrite, nchange);
                            CHECK_STATUS(editEgo);

                        /* propagate from Node to Node */
                        } else if (outtype == 'n' || outtype == 'N') {

                        /* propagate from Node to Edge */
                        } else if (outtype == 'e' || outtype == 'E') {
                            status = EG_getBodyTopos(ebody, enodes[inode], EDGE, &nlist, &elist);
                            CHECK_STATUS(EG_getBodyTopos);

                            for (ilist = 0; ilist < nlist; ilist++) {
                                status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                                 elist[ilist], overwrite, nchange);
                                CHECK_STATUS(editEgo);
                            }

                            EG_free(elist);
                            elist = NULL;

                        /* propagate from Node to Face */
                        } else if (outtype == 'f' || outtype == 'F') {
                            status = EG_getBodyTopos(ebody, enodes[inode], FACE, &nlist, &elist);
                            CHECK_STATUS(EG_getBodyTopos);

                            for (ilist = 0; ilist < nlist; ilist++) {
                                status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                                 elist[ilist], overwrite, nchange);
                                CHECK_STATUS(editEgo);
                            }

                            EG_free(elist);
                            elist = NULL;

                        } else {
                            printf(" outtype=%c is not valid for intype=N\n", outtype);
                            status = OCSM_UDP_ERROR1;
                            goto cleanup;
                        }
                    }
                }
            }

        /* propagate attribute from the Edges */
        } else if (intype == 'e' || intype == 'E') {
            for (iedge = 0; iedge < nedge; iedge++) {

                /* look through all Edge Attributes, looking for a match */
                status = EG_attributeNum(eedges[iedge], &nattr);
                CHECK_STATUS(EG_attributeNum);

                for (iattr = 1; iattr <= nattr; iattr++) {
                    status = EG_attributeGet(eedges[iedge], iattr, &aname, &atype, &alen,
                                             &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(EG_attributeGet);

                    if (matches(attrname, aname)) {

                        /* propagate from Edge to Body */
                        if (outtype == 'b' || outtype== 'B') {
                            status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                             ebody, overwrite, nchange);
                            CHECK_STATUS(editEgo);

                        /* propagate from Edge to Node */
                        } else if (outtype == 'n' || outtype == 'N') {
                            status = EG_getBodyTopos(ebody, eedges[iedge], NODE, &nlist, &elist);
                            CHECK_STATUS(EG_getBodyTopos);

                            for (ilist = 0; ilist < nlist; ilist++) {
                                status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                                 elist[ilist], overwrite, nchange);
                                CHECK_STATUS(editEgo);
                            }

                            EG_free(elist);
                            elist = NULL;

                        /* propagate from Edge to Edge */
                        } else if (outtype == 'e' || outtype == 'E') {

                        /* propagate from Edge to Face */
                        } else if (outtype == 'f' || outtype == 'F') {
                            status = EG_getBodyTopos(ebody, eedges[iedge], FACE, &nlist, &elist);
                            CHECK_STATUS(EG_getBodyTopos);

                            for (ilist = 0; ilist < nlist; ilist++) {
                                status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                                 elist[ilist], overwrite, nchange);
                                CHECK_STATUS(editEgo);
                            }

                            EG_free(elist);
                            elist = NULL;

                        } else {
                            printf(" outtype=%c is not valid for intype=E\n", outtype);
                            status = OCSM_UDP_ERROR1;
                            goto cleanup;
                        }
                    }
                }
            }

        /* propagate attribute from the Faces */
        } else if (intype == 'f' || intype == 'F') {
            for (iface = 0; iface < nface; iface++) {

                /* look through all Face Attributes, looking for a match */
                status = EG_attributeNum(efaces[iface], &nattr);
                CHECK_STATUS(EG_attributeNum);

                for (iattr = 1; iattr <= nattr; iattr++) {
                    status = EG_attributeGet(efaces[iface], iattr, &aname, &atype, &alen,
                                             &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(EG_attributeGet);

                    if (matches(attrname, aname)) {

                        /* propagate from Face to to Body */
                        if (outtype == 'b' || outtype == 'B') {
                            status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                             ebody, overwrite, nchange);
                            CHECK_STATUS(editEgo);

                        /* propagate from Face edit to Node */
                        } else if (outtype == 'n' || outtype == 'N') {
                            status = EG_getBodyTopos(ebody, efaces[iface], NODE, &nlist, &elist);
                            CHECK_STATUS(EG_getBodyTopos);

                            for (ilist = 0; ilist < nlist; ilist++) {
                                status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                                 elist[ilist], overwrite, nchange);
                                CHECK_STATUS(editEgo);
                            }

                            EG_free(elist);
                            elist = NULL;

                        /* propagate from Face to Edge */
                        } else if (outtype == 'e' || outtype == 'E') {
                            status = EG_getBodyTopos(ebody, efaces[iface], EDGE, &nlist, &elist);
                            CHECK_STATUS(EG_getBodyTopos);

                            for (ilist = 0; ilist < nlist; ilist++) {
                                status = editEgo(aname, atype, alen, tempIlist, tempRlist, tempClist,
                                                 elist[ilist], overwrite, nchange);
                                CHECK_STATUS(editEgo);
                            }

                            EG_free(elist);
                            elist = NULL;

                        /* propagate from Face to Face */
                        } else if (outtype == 'f' || outtype == 'F') {

                        } else {
                            printf(" outtype=%c is not valid for intype=F\n", outtype);
                            status = OCSM_UDP_ERROR1;
                            goto cleanup;
                        }
                    }
                }
            }

        } else {
            printf(" intype=%c is not valid\n", intype);
            status = OCSM_UDP_ERROR1;
            goto cleanup;
        }
    }

cleanup:
    if (enodes != NULL) EG_free(enodes);
    if (eedges != NULL) EG_free(eedges);
    if (efaces != NULL) EG_free(efaces);
    if (elist  != NULL) EG_free(elist );

    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   processFile - special file processing mode                         *
 *                                                                      *
 ************************************************************************
 */

static int
processFile(ego    context,             /* (in)  EGADS context */
            ego    ebody,               /* (in)  EGADS Body */
            char   filename[],          /* (in)  file that contains directives */
            int    *nchange)            /* (out) number of changes */
{
    int       status = 0;               /* (out) return status */

    int       nnode, nedge, nface, isel, nsel, istype, itoken;
    int       nlist, ilist, nnbor, inbor, iremove, atype, alen, i;
    int       status1, status2, outLevel;
    CINT      *tempIlist;
    double     value, dot, *newList=NULL;
    CDOUBLE   *tempRlist;
    char      templine[256], token1[256], token2[256], token3[256];
    char      attrName[256], attrValu[256], newValu[256], str[256];
    CCHAR     *tempClist;
    ego       *esel=NULL, *elist=NULL, *enbor=NULL;
    void      *modl;
    FILE      *fp;

    ROUTINE(processFile);

    /* --------------------------------------------------------------- */

    /* default return */
    *nchange = 0;

    /* get pointer to model */
    status = EG_getUserPointer(context, (void**)(&(modl)));
    if (status != EGADS_SUCCESS) {
        printf(" udpExecute: bad return from getUserPointer\n");
        goto cleanup;
    }

    /* get the outLevel from OpenCSM */
    outLevel = ocsmSetOutLevel(       0);
    (void)     ocsmSetOutLevel(outLevel);

    /* find number of Nodes, Edges, and Faces in ebody */
    status = EG_getBodyTopos(ebody, NULL, NODE, &nnode, NULL);
    if (status < EGADS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(ebody, NULL, EDGE, &nedge, NULL);
    if (status < EGADS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(ebody, NULL, FACE, &nface, NULL);
    if (status < EGADS_SUCCESS) goto cleanup;

    /* allocate a list of EGOs that is big enough for all Nodes, Edges, or Faces */
    if (nnode > nedge && nnode > nface) {
        nsel = nnode;
    } else if (nedge > nface) {
        nsel = nedge;
    } else {
        nsel = nface;
    }

    esel = (ego*) malloc(nsel*sizeof(ego));
    nsel = 0;

    istype = 0;

    /* open the file */
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf(" processFile: could not open file \"%s\"\n", filename);
        status = EGADS_NOTFOUND;
        goto cleanup;
    }

    /* read until end of file */
    while (!feof(fp)) {

        if (VERBOSE(0) > 0) {
            if        (istype == OCSM_FACE) {
                printf("       current Faces selected:");
                for (i = 0; i < nsel; i++) {
                    printf(" %3d", EG_indexBodyTopo(ebody, esel[i]));
                }
                printf("\n");
            } else if (istype == OCSM_EDGE) {
                printf("       current Edges selected:");
                for (i = 0; i < nsel; i++) {
                    printf(" %3d", EG_indexBodyTopo(ebody, esel[i]));
                }
                printf("\n");
            } else if (istype == OCSM_NODE) {
                printf("       current Nodes selected:");
                for (i = 0; i < nsel; i++) {
                    printf(" %3d", EG_indexBodyTopo(ebody, esel[i]));
                }
                printf("\n");
            } else {
                printf("       nothing currently selected\n");
            }
        }

        /* read the next line */
        (void) fgets(templine, 255, fp);

        if (feof(fp)) break;

        if (outLevel >= 1) printf("    processing: %s", templine);

        if (templine[0] == '#') continue;

        /* overwite the \n and \r at the end */
        if (STRLEN(templine) > 0 && templine[STRLEN(templine)-1] == '\n') {
            templine[STRLEN(templine)-1] = '\0';
        }
        if (STRLEN(templine) > 0 && templine[STRLEN(templine)-1] == '\r') {
            templine[STRLEN(templine)-1] = '\0';
        }

        /* get and process the first token (command) */
        itoken = 0;
        status = getToken(templine, itoken, ' ', 255, token1);

        /* make a list (esel) of the entities to which Attributes will be added */
        if (status == 0) {              /* null token */
            continue;

        /* comment not at beginning of line */
        } else if (token1[0] == '#') {
            continue;

        /* do not read any more from file */
        } else if (strcmp(token1, "END") == 0 || strcmp(token1, "end") == 0) {
            break;

       /* esel will contain all Faces */
        } else if (strcmp(token1, "FACE") == 0 || strcmp(token1, "face") == 0) {
            istype = OCSM_FACE;
            status = EG_getBodyTopos(ebody, NULL, FACE, &nlist, &elist);
            if (status < EGADS_SUCCESS) goto cleanup;

            nsel = nlist;
            for (ilist = 0; ilist < nlist; ilist++) {
                esel[ilist] = elist[ilist];
            }

            EG_free(elist);
            elist = NULL;

        /* esel will contain all Edges */
        } else if (strcmp(token1, "EDGE") == 0 || strcmp(token1, "edge") == 0) {
            istype = OCSM_EDGE;
            status = EG_getBodyTopos(ebody, NULL, EDGE, &nlist, &elist);
            if (status < EGADS_SUCCESS) goto cleanup;

            nsel = nlist;
            for (ilist = 0; ilist < nlist; ilist++) {
                esel[ilist] = elist[ilist];
            }

            EG_free(elist);
            elist = NULL;

        /* esel will contain all Nodes */
        } else if (strcmp(token1, "NODE") == 0 || strcmp(token1, "node") == 0) {
            istype = OCSM_NODE;
            status = EG_getBodyTopos(ebody, NULL, NODE, &nlist, &elist);
            if (status < EGADS_SUCCESS) goto cleanup;

            nsel = nlist;
            for (ilist = 0; ilist < nlist; ilist++) {
                esel[ilist] = elist[ilist];
            }

            EG_free(elist);
            elist = NULL;

        /* entries will (possibly) be removed from esel */
        } else if (strcmp(token1, "AND") == 0 || strcmp(token1, "and") == 0) {
            if        (nsel   == 0) {
                printf("                    *** AND being skipped since nothing is selected\n");
            } else if (istype == 0) {
                printf(" processFile: AND has to follow NODE, EDGE, FACE, or AND\n");
                status = OCSM_UDP_ERROR2;
                goto cleanup;
            }

        /* entries will (possibly) be removed from esel */
        } else if (strcmp(token1, "ANDNOT") == 0 || strcmp(token1, "andnot") == 0) {
            if        (nsel   == 0) {
                printf("                    *** ANDNOT being skipped since nothing is selected\n");
            } else if (istype == 0) {
                printf(" processFile: ANDNOT has to follow NODE, EDGE, FACE, or AND\n");
                status = OCSM_UDP_ERROR2;
                goto cleanup;
            }

        /* all entries in esel will get specified Attribute name/value pairs */
        } else if (strcmp(token1, "SET") == 0 || strcmp(token1, "set") == 0) {

            /* apply the Attribute name/value to everything in esel */
            itoken = 0;
            while (1) {
                status = getToken(templine, ++itoken, ' ', 255, token3);
                if (status <= 0) break;

                status1 = getToken(token3, 0, '=', 255, attrName);
                status2 = getToken(token3, 1, '=', 255, attrValu);

                if (status1 <= 0) {
                    printf(" processFile: token is not name=value or name=\n");
                    status = OCSM_UDP_ERROR3;
                    goto cleanup;
                } else if (istype == 0) {
                    printf("                    *** nothing selected, so not setting %s=%s\n",
                           attrName, attrValu);
                    break;
                }

                /* if first character of attrName is an exclamation point,
                   evaluate the expression */
                if (attrName[0] == '!') {
                    status = ocsmEvalExpr(modl, attrName, &value, &dot, str);
                    if (status < EGADS_SUCCESS) {
                        printf(" processFile: unable to evaluate \"%s\"\n", attrName);
                        status = EGADS_NONAME;
                        goto cleanup;
                    }
                    strcpy(attrName, str);
                }

                /* if attrValu is not set, delete the attribute */
                if (status2 <= 0) {
                    for (isel = 0; isel < nsel; isel++) {
                        status = EG_attributeDel(esel[isel], attrName);
                        if (status < EGADS_SUCCESS) goto cleanup;
                        (*nchange)++;
                    }
                    
                /* if first character of attrValu is an exclamation point,
                   evaluate the expression */
                } else if (attrValu[0] == '!') {
                    status = ocsmEvalExpr(modl, attrValu, &value, &dot, str);
                    if (status < EGADS_SUCCESS) {
                        printf(" processFile: unable to evaluate \"%s\"\n", attrValu);
                        status = EGADS_NONAME;
                        goto cleanup;
                    }

                    /* attrValue evaluates to a string */
                    if (strlen(str) > 0) {
                        for (isel = 0; isel < nsel; isel++) {
                            status = EG_attributeAdd(esel[isel], attrName, ATTRSTRING, 0,
                                                     NULL, NULL, str);
                            if (status < EGADS_SUCCESS) goto cleanup;
                            (*nchange)++;
                        }

                     /* attrValue evaluates to a number */
                    } else {
                        for (isel = 0; isel < nsel; isel++) {
                            status = EG_attributeAdd(esel[isel], attrName, ATTRREAL, 1,
                                                     NULL, &value, NULL);
                            if (status < EGADS_SUCCESS) goto cleanup;
                            (*nchange)++;
                        }
                    }

                /* attrValu is an implicit string */
                } else {
                    for (isel = 0; isel < nsel; isel++) {
                        status = EG_attributeAdd(esel[isel], attrName, ATTRSTRING, 0,
                                                 NULL, NULL, attrValu);
                        if (status < EGADS_SUCCESS) goto cleanup;
                        (*nchange)++;
                    }
                }
            }

            continue;

        /* all entries in esel will get specified Attribute name/value pairs added to existing Attributes */
        } else if (strcmp(token1, "ADD") == 0 || strcmp(token1, "add") == 0) {

            /* append the Attribute name/value to everything in esel */
            itoken = 0;
            while (1) {
                status = getToken(templine, ++itoken, ' ', 255, token3);
                if (status <= 0) break;

                status1 = getToken(token3, 0, '=', 255, attrName);
                status2 = getToken(token3, 1, '=', 255, attrValu);

                if (status1 <= 0 || status2 <= 0) {
                    printf(" processFile: token is not name=value\n");
                    status = OCSM_UDP_ERROR3;
                    goto cleanup;
                } else if (istype == 0) {
                    printf("                    *** nothing selected, so not setting %s=%s\n",
                           attrName, attrValu);
                    break;
                }

                /* if first chanacter of attrName is an exclamation point,
                   evaluate the expression */
                if (attrName[0] == '!') {
                    status = ocsmEvalExpr(modl, attrName, &value, &dot, str);
                    if (status < EGADS_SUCCESS) {
                        printf(" processFile: unable to evaluate \"%s\"\n", attrName);
                        status = EGADS_NONAME;
                        goto cleanup;
                    }
                    strcpy(attrName, str);
                }

                /* if the first character of attrValu is an exclamation point,
                   evaluate the expression */
                if (attrValu[0] == '!') {
                    status = ocsmEvalExpr(modl, attrValu, &value, &dot, str);
                    if (status < EGADS_SUCCESS) {
                        printf(" processFile: unable to evaluate \"%s\"\n", attrValu);
                        status = EGADS_NONAME;
                        goto cleanup;
                    }

                    /* attrValu evaluates to a string, so concatenate if the
                       attribute already contains a string */
                    if (strlen(str) > 0) {
                        for (isel = 0; isel < nsel; isel++) {
                            status = EG_attributeRet(esel[isel], attrName, &atype, &alen,
                                                     &tempIlist, &tempRlist, &tempClist);

                            if (status == EGADS_SUCCESS && atype == ATTRSTRING) {
                                snprintf(newValu, 255, "%s;%s", tempClist, str);
                            } else if (status == EGADS_SUCCESS) {
                                printf(" processFile: cannot concatenate a string to a non-string\n");
                                status = OCSM_UDP_ERROR3;
                                goto cleanup;
                            } else {
                                snprintf(newValu, 255, "%s",               str);
                            }

                            status = EG_attributeAdd(esel[isel], attrName, ATTRSTRING, 0,
                                                     NULL, NULL, newValu);
                            if (status < EGADS_SUCCESS) goto cleanup;
                            (*nchange)++;
                        }

                    /* attrValu evaluates to a number, so concatenate if the attribiute
                       already contains a number(s) */
                    } else {
                        for (isel = 0; isel < nsel; isel++) {
                            status = EG_attributeRet(esel[isel], attrName, &atype, &alen,
                                                     &tempIlist, &tempRlist, &tempClist);

                            if (status == EGADS_SUCCESS && atype == ATTRREAL) {
                                newList = (double *) malloc((alen+1)*sizeof(double));
                                if (newList == NULL) {
                                    status = EGADS_MALLOC;
                                    goto cleanup;
                                }
                                for (i = 0; i < alen; i++) {
                                    newList[i] = tempRlist[i];
                                }
                                newList[alen] = value;
                                alen++;
                            } else if (status == EGADS_SUCCESS) {
                                printf(" processFile: cannot concatenate a non-string to a string\n");
                                status = OCSM_UDP_ERROR3;
                                goto cleanup;
                            } else {
                                newList = (double *) malloc(sizeof(double));
                                if (newList == NULL) {
                                    status = EGADS_MALLOC;
                                    goto cleanup;
                                }
                                newList[0] = value;
                                alen = 1;
                            }

                            status = EG_attributeAdd(esel[isel], attrName, ATTRREAL, alen,
                                                     NULL, newList, NULL);
                            if (status < EGADS_SUCCESS) goto cleanup;

                            free(newList);
                            newList = NULL;
                            (*nchange)++;
                        }
                    }

                /* attrValue is an implicit string */
                } else {
                    for (isel = 0; isel < nsel; isel++) {
                        status = EG_attributeRet(esel[isel], attrName, &atype, &alen,
                                                 &tempIlist, &tempRlist, &tempClist);

                        if (status == EGADS_SUCCESS && atype == ATTRSTRING) {
                            snprintf(newValu, 255, "%s;%s", tempClist, attrValu);
                        } else if (status == EGADS_SUCCESS) {
                            printf(" processFile: cannot concatenate a string to a string\n");
                            status = OCSM_UDP_ERROR3;
                            goto cleanup;
                        } else {
                            snprintf(newValu, 255, "%s",               attrValu);
                        }

                        status = EG_attributeAdd(esel[isel], attrName, ATTRSTRING, 0,
                                                 NULL, NULL, newValu);
                        if (status < EGADS_SUCCESS) goto cleanup;
                        (*nchange)++;
                    }
                }
            }

            continue;

        /* command type is not known */
        } else {
            printf(" processFile: unexpected command \"%s\"\n", token1);
            status =  EGADS_NONAME;
            goto cleanup;
        }

        /* do not process rest of tokens (on this line) if esel is empty */
        if (nsel <= 0) {
            istype = 0;
            continue;
        }

        /* process the rest of the tokens (specifier attrName=AttrValue ...) */
        status = getToken(templine, 1, ' ', 255, token2);

        /* make a list (elist) of the entities that match the specified name/valu pairs */

        /* start elist with all the Faces */
        if        (strcmp(token2, "ADJ2FACE") == 0 || strcmp(token2, "adj2face") == 0 ||
                   ((strcmp(token2, "HAS") == 0 || strcmp(token2, "has") == 0 || status == 0) && istype == OCSM_FACE)) {
            status = EG_getBodyTopos(ebody, NULL, FACE, &nlist, &elist);
            if (status < EGADS_SUCCESS) goto cleanup;

        /* start elist with all the Edges */
        } else if (strcmp(token2, "ADJ2EDGE") == 0 || strcmp(token2, "adj2edge") == 0 ||
                   ((strcmp(token2, "HAS") == 0 || strcmp(token2, "has") == 0 || status == 0) && istype == OCSM_EDGE)) {
            status = EG_getBodyTopos(ebody, NULL, EDGE, &nlist, &elist);
            if (status < EGADS_SUCCESS) goto cleanup;


        /* start elist with all the Nodes */
        } else if (strcmp(token2, "ADJ2NODE") == 0 || strcmp(token2, "adj2node") == 0 ||
                   ((strcmp(token2, "HAS") == 0 || strcmp(token2, "has") == 0 || status == 0) && istype == OCSM_NODE)) {
            status = EG_getBodyTopos(ebody, NULL, NODE, &nlist, &elist);
            if (status < EGADS_SUCCESS) goto cleanup;

        /* there will be no list if there is not a specifier and this is either 
           a FACE, EDGE, or NODE statement */
        } else if (strcmp(token1, "FACE") == 0 || strcmp(token1, "face") == 0 ||
                   strcmp(token1, "EDGE") == 0 || strcmp(token1, "edge") == 0 ||
                   strcmp(token1, "NODE") == 0 || strcmp(token1, "node") == 0   ) {
            nlist = 0;
            elist = NULL;

        /* unknown specifier */
        } else {
            printf(" processFile: illegal specifier (not HAS, ADJ2NODE, ADJ2EDGE, or ADJ2FACE");
            status = OCSM_UDP_ERROR4;
            goto cleanup;
        }

         /* remove entries from elist if they don't match all mentioned name/value pairs */
        for (itoken = 2; itoken < 10; itoken++) {

            /* get the name/value */
            status = getToken(templine, itoken, ' ', 255, token3);
            if (status <= 0) break;

            status1 = getToken(token3, 0, '=', 255, attrName);
            status2 = getToken(token3, 1, '=', 255, attrValu);

            if (status1 <= 0 || status2 <= 0) {
                printf(" processFile: token is not name=value\n");
                status = OCSM_UDP_ERROR3;
                goto cleanup;
            }

            /* if first character of attrName is an exclamation point,
               evaluate the expression */
            if (attrName[0] == '!') {
                status = ocsmEvalExpr(modl, attrName, &value, &dot, str);
                if (status < EGADS_SUCCESS) {
                    printf(" processFile: unable to evaluate \"%s\"\n", attrName);
                    status = EGADS_NONAME;
                    goto cleanup;
                }
                strcpy(attrName, str);
            }

            /* if the first character of attrValu is an exclamation point,
               evaluate the expression */
            if (attrValu[0] == '!') {
                status = ocsmEvalExpr(modl, attrValu, &value, &dot, str);
                if (status < EGADS_SUCCESS) {
                    printf(" processFile: unable to evaluate \"%s\"\n", attrValu);
                    status = EGADS_NONAME;
                    goto cleanup;
                }

                /* attrValue evaluates to a string, so remove entries from elist
                   if they do not match name/value pairs */
                if (strlen(str) > 0) {
                    for (ilist = nlist-1; ilist >= 0; ilist--) {
                        if (strcmp(attrName, "*") == 0 && strcmp(attrValu, "*") == 0) continue;

                        status = EG_attributeRet(elist[ilist], attrName,
                                                 &atype, &alen, &tempIlist, &tempRlist, &tempClist);
                        if (status != EGADS_SUCCESS) {
                            elist[ilist] = elist[nlist-1];
                            nlist--;
                        } else if (atype != ATTRSTRING) {
                            elist[ilist] = elist[nlist-1];
                            nlist--;
                        } else if (matches(str, tempClist) == 0) {
                            elist[ilist] = elist[nlist-1];
                            nlist--;
                        }
                    }

                /* attrValue evaluates to a number, so remove entries from elist
                   if they do not match name/value pairs */
                } else {
                    for (ilist = nlist-1; ilist >= 0; ilist--) {
                        if (strcmp(attrName, "*") == 0) continue;

                        status = EG_attributeRet(elist[ilist], attrName,
                                                 &atype, &alen, &tempIlist, &tempRlist, &tempClist);
                        if (status != EGADS_SUCCESS) {
                            elist[ilist] = elist[nlist-1];
                            nlist--;
                        } else if (atype != ATTRREAL) {
                            elist[ilist] = elist[nlist-1];
                            nlist--;
                        } else {
                            for (i = 0; i < alen; i++) {
                                if (fabs(value-tempRlist[i]) > EPS06) {
                                    elist[ilist] = elist[nlist-1];
                                    nlist--;
                                    break;
                                }
                            }
                        }
                    }
                }

            /* attrValue is an implicit string */
            } else {
                for (ilist = nlist-1; ilist >= 0; ilist--) {
                    if (strcmp(attrName, "*") == 0 && strcmp(attrValu, "*") == 0) continue;

                    status = EG_attributeRet(elist[ilist], attrName,
                                             &atype, &alen, &tempIlist, &tempRlist, &tempClist);
                    if (status != EGADS_SUCCESS) {
                        elist[ilist] = elist[nlist-1];
                        nlist--;
                    } else if (atype != ATTRSTRING) {
                        elist[ilist] = elist[nlist-1];
                        nlist--;
                    } else if (matches(attrValu, tempClist) == 0) {
                        elist[ilist] = elist[nlist-1];
                        nlist--;
                    }
                }
            }
        }

        /* if ANDNOT command, remove those entries from esel that are
           contained in elist */
        if (strcmp(token1, "ANDNOT") == 0 || strcmp(token1, "andnot") == 0) {

            if        (strcmp(token2, "ADJ2FACE") == 0 || strcmp(token2, "adj2face") == 0) {
                for (isel = nsel-1; isel >= 0; isel--) {
                    status = getBodyTopos(ebody, esel[isel], FACE, &nnbor, &enbor);
                    if (status < EGADS_SUCCESS) goto cleanup;

                    iremove = 0;
                    for (ilist = 0;  ilist < nlist; ilist++) {
                        for (inbor = 0; inbor < nnbor; inbor++) {
                            if (elist[ilist] == enbor[inbor]) {
                                iremove = 1;
                            }
                        }
                    }

                    if (iremove > 0) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }

                    EG_free(enbor);
                    enbor = NULL;
                }

            } else if (strcmp(token2, "ADJ2EDGE") == 0 || strcmp(token2, "adj2edge") == 0) {
                for (isel = nsel-1; isel >= 0; isel--) {
                    status = getBodyTopos(ebody, esel[isel], EDGE, &nnbor, &enbor);
                    if (status < EGADS_SUCCESS) goto cleanup;

                    iremove = 0;
                    for (ilist = 0;  ilist < nlist; ilist++) {
                        for (inbor = 0; inbor < nnbor; inbor++) {
                            if (elist[ilist] == enbor[inbor]) {
                                iremove = 1;
                            }
                        }
                    }

                    if (iremove > 0) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }

                    EG_free(enbor);
                    enbor = NULL;
                }

            } else if (strcmp(token2, "ADJ2NODE") == 0 || strcmp(token2, "adj2node") == 0) {
                for (isel = nsel-1; isel >= 0; isel--) {
                    status = getBodyTopos(ebody, esel[isel], NODE, &nnbor, &enbor);
                    if (status < EGADS_SUCCESS) goto cleanup;

                    iremove = 0;
                    for (ilist = 0;  ilist < nlist; ilist++) {
                        for (inbor = 0; inbor < nnbor; inbor++) {
                            if (elist[ilist] == enbor[inbor]) {
                                iremove = 1;
                            }
                        }
                    }

                    if (iremove > 0) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }

                    EG_free(enbor);
                    enbor = NULL;
                }

            } else {
                for (isel = nsel-1; isel >= 0; isel--) {
                    iremove = 0;
                    for (ilist = 0; ilist < nlist; ilist++) {
                        if (esel[isel] == elist[ilist]) {
                            iremove = 1;
                            break;
                        }
                    }
                
                    if (iremove == 1) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }
                }
            }

        /* otherwise, remove this entries from esel that are NOT
           contained in elist */
        } else {

            if        (strcmp(token2, "ADJ2FACE") == 0 || strcmp(token2, "adj2face") == 0) {
                for (isel = nsel-1; isel >= 0; isel--) {
                    status = getBodyTopos(ebody, esel[isel], FACE, &nnbor, &enbor);
                    if (status < EGADS_SUCCESS) goto cleanup;

                    iremove = 1;
                    for (ilist = 0;  ilist < nlist; ilist++) {
                        for (inbor = 0; inbor < nnbor; inbor++) {
                            if (elist[ilist] == enbor[inbor]) {
                                iremove = 0;
                            }
                        }
                    }

                    if (iremove > 0) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }

                    EG_free(enbor);
                    enbor = NULL;
                }
            } else if (strcmp(token2, "ADJ2EDGE") == 0 || strcmp(token2, "adj2edge") == 0) {
                for (isel = nsel-1; isel >= 0; isel--) {
                    status = getBodyTopos(ebody, esel[isel], EDGE, &nnbor, &enbor);
                    if (status < EGADS_SUCCESS) goto cleanup;

                    iremove = 1;
                    for (ilist = 0;  ilist < nlist; ilist++) {
                        for (inbor = 0; inbor < nnbor; inbor++) {
                            if (elist[ilist] == enbor[inbor]) {
                                iremove = 0;
                            }
                        }
                    }

                    if (iremove > 0) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }

                    EG_free(enbor);
                    enbor = NULL;
                }
            } else if (strcmp(token2, "ADJ2NODE") == 0 || strcmp(token2, "adj2node") == 0) {
                for (isel = nsel-1; isel >= 0; isel--) {
                    status = getBodyTopos(ebody, esel[isel], NODE, &nnbor, &enbor);
                    if (status < EGADS_SUCCESS) goto cleanup;

                    iremove = 1;
                    for (ilist = 0;  ilist < nlist; ilist++) {
                        for (inbor = 0; inbor < nnbor; inbor++) {
                            if (elist[ilist] == enbor[inbor]) {
                                iremove = 0;
                            }
                        }
                    }

                    if (iremove > 0) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }

                    EG_free(enbor);
                    enbor = NULL;
                }
            } else {
                for (isel = nsel-1; isel >= 0; isel--) {
                    iremove = 1;
                    for (ilist = 0; ilist < nlist; ilist++) {
                        if (esel[isel] == elist[ilist]) {
                            iremove = 0;
                            break;
                        }
                    }
                
                    if (iremove == 1) {
                        esel[isel] = esel[nsel-1];
                        nsel--;
                    }
                }
            }
        }

        EG_free(elist);
        elist = NULL;
    }

    fclose(fp);

cleanup:
    if (newList != NULL) free(newList);

    EG_free(elist);
    EG_free(enbor);

    if (esel != NULL) free(esel);

    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   matches - check if two strings match (with * + ? wildcards)        *
 *                                                                      *
 ************************************************************************
 */

static int
matches(char pattern[],                /* (in)  pattern */
  const char string[])                 /* (in)  string to check */
{

    /* --------------------------------------------------------------- */

    /* matching rules:
       ? matches any one character
       + matches one or more characters
       * matches zero or more characters

       use an adaptation of the recursive code found at:
       http://codegolf.stackexchange.com/questions/467/implement-glob-matcher
    */

    if        (*pattern == '*') {
        return matches(pattern+1, string) || (*string && matches(pattern, string+1));

    } else if (*pattern == '+') {
        return *string && (matches(pattern+1, ++string) || matches(pattern, string));

    } else if (*pattern == '?') {
        return *string && matches(pattern+1, string+1);

    } else {
        return *string == *pattern++ && (!*string++ || matches(pattern, string));

    }
}


/*
 ************************************************************************
 *                                                                      *
 *   editEgo - edit an Attribute on an ego                              *
 *                                                                      *
 ************************************************************************
 */

static int
editEgo(
    const char   attrname[],            /* (in)  Attribute name */
          int    atype,                 /* (in)  Attribute type */
          int    alen,                  /* (in)  Attribute length */
    const int    *tempIlist,            /* (in)  Attribute int  values */
    const double *tempRlist,            /* (in)  Attribute real values */
    const char   *tempClist,            /* (in)  Attribute char values */
          ego    eobj,                  /* (in)  ego to receive Attribute */
          int    overwrite,             /* (in)  =0 do not overwrite */
                                        /*       =1        overwrite */
                                        /*       =2        use smaller value */
                                        /*       =3        use larger  value */
                                        /*       =4        use sum or concatenation */
          int    *nchange)              /* (both) number of changes */
{
    int     status = EGADS_SUCCESS;     /* (out) return status */

    int     btype, blen, ivalue;
    CINT    *tmpIlist;
    double  value;
    CDOUBLE *tmpRlist;
    char    *strnew=NULL;
    CCHAR   *tmpClist;

    /* --------------------------------------------------------------- */

    /* check to see if Attribute already exists */
    status = EG_attributeRet(eobj, attrname, &btype, &blen,
                             &tmpIlist, &tmpRlist, &tmpClist);

    /* if overwrite==0 (do not overwrite) only save Attribute if it
                        does not already exist */
    if (overwrite == 0) {
        if (status != EGADS_SUCCESS) {
            status = EG_attributeAdd(eobj, attrname, atype, alen,
                                     tempIlist, tempRlist, tempClist);
            (*nchange)++;
        }

    /* if overwrite==1 (overwrite) save the Attribute */
    } else if (overwrite == 1) {
        status = EG_attributeAdd(eobj, attrname, atype, alen,
                                 tempIlist, tempRlist, tempClist);
        (*nchange)++;

    /* if overwrite==2 (use smaller value) save the smaller of the values */
    } else if (overwrite == 2) {
        if (status != EGADS_SUCCESS) {
            status = EG_attributeAdd(eobj, attrname, atype, alen,
                                     tempIlist, tempRlist, tempClist);
            (*nchange)++;
        } else if (atype != btype) {
            printf(" existing (%d) and new (%d) Attributes have to be same type\n", atype, btype);
            status = EGADS_ATTRERR;
        } else if (atype == ATTRINT) {
            if (alen != 1 || blen != 1) {
                printf(" can only take smaller value for scalar Attributes (alen=%d, blen=%d)\n", alen, blen);
                status = EGADS_ATTRERR;
            } else {
                ivalue = MIN(tempIlist[0], tmpIlist[0]);

                status = EG_attributeAdd(eobj, attrname, ATTRINT, 1,
                                         &ivalue, NULL, NULL);
                (*nchange)++;
            }
        } else if (atype == ATTRREAL) {
            if (alen != 1 || blen != 1) {
                printf(" can only take smaller value for scalar Attributes (alen=%d, blen=%d)\n", alen, blen);
                status = EGADS_ATTRERR;
            } else {
                value = MIN(tempRlist[0], tmpRlist[0]);

                status = EG_attributeAdd(eobj, attrname, ATTRREAL, 1,
                                         NULL, &value, NULL);
                (*nchange)++;
            }
        } else if (atype == ATTRSTRING) {
            if (strcmp(tempClist, tmpClist) < 0) {
                status = EG_attributeAdd(eobj, attrname, ATTRSTRING, 0,
                                         NULL, NULL, tempClist);
                (*nchange)++;
            }
        }

    /* if overwrite==3 (use larger value) save the larger of the values */
    } else if (overwrite == 3) {
        if (status != EGADS_SUCCESS) {
            status = EG_attributeAdd(eobj, attrname, atype, alen,
                                     tempIlist, tempRlist, tempClist);
            (*nchange)++;
        } else if (atype != btype) {
            printf(" existing (%d) and new (%d) Attributes have to be same type\n", atype, btype);
            status = EGADS_ATTRERR;
        } else if (atype == ATTRINT) {
            if (alen != 1 || blen != 1) {
                printf(" can only take larger value for scalar Attributes (alen=%d, blen=%d)\n", alen, blen);
                status = EGADS_ATTRERR;
            } else {
                ivalue = MAX(tempIlist[0], tmpIlist[0]);

                status = EG_attributeAdd(eobj, attrname, ATTRINT, 1,
                                         &ivalue, NULL, NULL);
                (*nchange)++;
            }
        } else if (atype == ATTRREAL) {
            if (alen != 1 || blen != 1) {
                printf(" can only take larger value for scalar Attributes (alen=%d, blen=%d)\n", alen, blen);
                status = EGADS_ATTRERR;
            } else {
                value = MAX(tempRlist[0], tmpRlist[0]);

                status = EG_attributeAdd(eobj, attrname, ATTRREAL, 1,
                                         NULL, &value, NULL);
                (*nchange)++;
            }
        } else if (atype == ATTRSTRING) {
            if (strcmp(tempClist, tmpClist) > 0) {
                status = EG_attributeAdd(eobj, attrname, ATTRSTRING, 0,
                                         NULL, NULL, tempClist);
                (*nchange)++;
            }
        }

    /* if overwrite==4 (use sum) save the sum of the values (or concatenate strings) */
    } else if (overwrite == 4) {
        if (status != EGADS_SUCCESS) {
            status = EG_attributeAdd(eobj, attrname, atype, alen,
                                     tempIlist, tempRlist, tempClist);
            (*nchange)++;
        } else if (atype != btype) {
            printf(" existing (%d) and new (%d) Attributes have to be same type\n", atype, btype);
            status = EGADS_ATTRERR;
        } else if (atype == ATTRINT) {
            if (alen != 1 || blen != 1) {
                printf(" can only take sum for scalar Attributes (alen=%d, blen=%d)\n", alen, blen);
                status = EGADS_ATTRERR;
            } else {
                ivalue = tempIlist[0] + tmpIlist[0];

                status = EG_attributeAdd(eobj, attrname, ATTRINT, 1,
                                         &ivalue, NULL, NULL);
                (*nchange)++;
            }
        } else if (atype == ATTRREAL) {
            if (alen != 1 || blen != 1) {
                printf(" can only take sum for scalar Attributes (alen=%d, blen=%d)\n", alen, blen);
                status = EGADS_ATTRERR;
            } else {
                value = tempRlist[0] + tmpRlist[0];

                status = EG_attributeAdd(eobj, attrname, ATTRREAL, 1,
                                         NULL, &value, NULL);
                (*nchange)++;
            }
        } else if (atype == ATTRSTRING) {
            strnew = (char*) malloc((STRLEN(tempClist)+STRLEN(tmpClist)+2)*sizeof(char));
            if (strnew == NULL) {
                status = EGADS_MALLOC;
            } else {
                strcpy(strnew, tmpClist );
                strcat(strnew, tempClist);

                status = EG_attributeAdd(eobj, attrname, ATTRSTRING, 0,
                                         NULL, NULL, strnew);
                (*nchange)++;

                free(strnew);
            }
        }
    }

    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   getToken - get a token from a string                               *
 *                                                                      *
 ************************************************************************
 */

static int
getToken(char   *text,                  /* (in)  full text */
         int    nskip,                  /* (in)  tokens to skip */
         char   sep,                    /* (in)  seperator character */
         int    maxtok,                 /* (in)  size of token */
         char   *token)                 /* (out) token */
{
    int     lentok, ibeg, i, j, count, iskip;

    /* --------------------------------------------------------------- */

    token[0] = '\0';
    lentok   = 0;

    /* convert tabs to spaces */
    for (i = 0; i < STRLEN(text); i++) {
        if (text[i] == '\t') {
            text[i] = ' ';
        }
    }

    /* skip past white spaces at beginning of text */
    ibeg = 0;
    while (ibeg < STRLEN(text)) {
        if (text[ibeg] != ' ' && text[ibeg] != '\t' && text[ibeg] != '\r') break;
        ibeg++;
    }

    /* count the number of separators */
    count = 0;
    for (i = ibeg; i < STRLEN(text); i++) {
        if (text[i] == sep) {
            count++;
            for (j = i+1; j < STRLEN(text); j++) {
                if (text[j] != sep) break;
                i++;
            }
        }
    }

    if (count < nskip) return 0;

    /* skip over nskip tokens */
    i = ibeg;
    for (iskip = 0; iskip < nskip; iskip++) {
        while (text[i] != sep) {
            i++;
        }
        while (text[i] == sep) {
            i++;
        }
    }

    /* extract the token we are looking for */
    while (text[i] != sep && i < STRLEN(text)) {
        token[lentok++] = text[i++];
        token[lentok  ] = '\0';

        if (lentok >= maxtok-1) {
            printf("ERROR:: token exceeds maxtok=%d\n", maxtok);
            break;
        }
    }

    return STRLEN(token);
}


/*
 ************************************************************************
 *                                                                      *
 *   getBodyTopos - extension to get neighbors of same oclass           *
 *                                                                      *
 ************************************************************************
 */

static int
getBodyTopos(ego    ebody,
             ego    esrc,
             int    oclass,
             int    *nlist,
             ego    **elist)
{

    int    status = EGADS_SUCCESS;

    int    inode, nnode, iedge, nedge, iface, nface, ilist;
    ego    *enodes, *eedges, *efaces;

    /* --------------------------------------------------------------- */

    *nlist = 0;
    *elist = NULL;

    if (ebody == NULL) {
        printf(" ebody is NULL\n");
        status = OCSM_UDP_ERROR5;
        goto cleanup;
    } else if (ebody->magicnumber != MAGIC) {
        printf(" ebody has wrong magic number\n");
        status = OCSM_UDP_ERROR5;
        goto cleanup;
    } else if (ebody->oclass != BODY) {
        printf(" ebody is not a Body\n");
        status = OCSM_UDP_ERROR5;
        goto cleanup;
    } else if (esrc == NULL) {
        printf(" ersc is NULL\n");
        status = OCSM_UDP_ERROR5;
        goto cleanup;
    } else if (esrc->magicnumber != MAGIC) {
        printf(" esrc has wrong magic number\n");
        status = OCSM_UDP_ERROR5;
        goto cleanup;
    }

    if        (oclass == NODE) {
        if (esrc->oclass == EDGE || esrc->oclass == FACE) {
            status = EG_getBodyTopos(ebody, esrc, oclass, nlist, elist);
        } else {
            printf(" cannot proocess NODE/?\n");
            status = OCSM_UDP_ERROR5;
        }
    } else if (oclass == EDGE) {
        if (esrc->oclass == NODE || esrc->oclass == FACE) {
            status = EG_getBodyTopos(ebody, esrc, oclass, nlist, elist);
        } else if (esrc->oclass != EDGE) {
            printf(" cannot process EDGE/?\n");
            status = OCSM_UDP_ERROR5;
        } else {

            /* allocate the returned array (big enough for all Edges,
               which is probably a bit wasteful) */
            status = EG_getBodyTopos(ebody, NULL, FACE, &nedge, NULL);
            if (status < EGADS_SUCCESS) goto cleanup;

            *elist = EG_alloc(nedge*sizeof(ego));
            if (*elist == NULL) {
                status = EGADS_MALLOC;
                goto cleanup;
            }

            /* make a list of all Nodes that are adjacent to Edge */
            status = EG_getBodyTopos(ebody, esrc, NODE, &nnode, &enodes);
            if (status < EGADS_SUCCESS) goto cleanup;

            /* loop through all Nodes (that are adjacent to esrc) and add
               its Edges to elist (if not esrc and if not already in list) */
            for (inode = 0; inode < nnode; inode++) {
                status = EG_getBodyTopos(ebody, enodes[inode], EDGE, &nedge, &eedges);
                if (status < EGADS_SUCCESS) goto cleanup;

                for (iedge = 0; iedge < nedge; iedge++) {
                    if (eedges[iedge] == esrc) continue;

                    /* add the Edge to the list (possibly removed below) */
                    (*elist)[(*nlist)++] = eedges[iedge];

                    /* remove the Edge if it already was in list */
                    for (ilist = 0; ilist < (*nlist)-1; ilist++) {
                        if (eedges[iedge] == (*elist)[ilist]) {
                            (*nlist)--;
                            break;
                        }
                    }
                }

                EG_free(eedges);
            }

            EG_free(enodes);
        }
    } else if (oclass == FACE) {
        if (esrc->oclass == NODE || esrc->oclass == EDGE) {
            status = EG_getBodyTopos(ebody, esrc, oclass, nlist, elist);
        } else if (esrc->oclass != FACE) {
            printf(" cannot process FACE/?\n");
            status = OCSM_UDP_ERROR5;
        } else {

            /* allocate the returned array (big enough for all Faces,
               which is probably a bit wasteful) */
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, NULL);
            if (status < EGADS_SUCCESS) goto cleanup;

            *elist = EG_alloc(nface*sizeof(ego));
            if (*elist == NULL) {
                status = EGADS_MALLOC;
                goto cleanup;
            }

            /* make a list of all Edges that are adjacent to Face */
            status = EG_getBodyTopos(ebody, esrc, EDGE, &nedge, &eedges);
            if (status < EGADS_SUCCESS) goto cleanup;

            /* loop through all Edges (that are adjacent to esrc) and add
               its Faces to elist (if not esrc and if not already in list) */
            for (iedge = 0; iedge < nedge; iedge++) {
                status = EG_getBodyTopos(ebody, eedges[iedge], FACE, &nface, &efaces);
                if (status < EGADS_SUCCESS) goto cleanup;

                for (iface = 0; iface < nface; iface++) {
                    if (efaces[iface] == esrc) continue;

                    /* add the Face to the list (possibly removed below) */
                    (*elist)[(*nlist)++] = efaces[iface];

                    /* remove the Face if it already was in list */
                    for (ilist = 0; ilist < (*nlist)-1; ilist++) {
                        if (efaces[iface] == (*elist)[ilist]) {
                            (*nlist)--;
                            break;
                        }
                    }
                }

                EG_free(efaces);
            }

            EG_free(eedges);
        }
    }

cleanup:
    return status;
}
