/*
 ************************************************************************
 *                                                                      *
 * sensCSM.c -- test configuration and tessellation sensitivities       *
 *                                                                      *
 *              Written by John Dannenhoffer @ Syracuse University      *
 *                                                                      *
 ************************************************************************
 */

/*
 * Copyright (C) 2013/2020  John F. Dannenhoffer, III (Syracuse University)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include "egads.h"

#define CINT    const int
#define CDOUBLE const double
#define CCHAR   const char

#define STRNCPY(A, B, LEN) strncpy(A, B, LEN); A[LEN-1] = '\0';

#include "common.h"
#include "OpenCSM.h"
#include "udp.h"

#if !defined(__APPLE__) && !defined(WIN32)
// floating point exceptions
   #define __USE_GNU
   #include <fenv.h>
#endif

/***********************************************************************/
/*                                                                     */
/* macros (including those that go along with common.h)                */
/*                                                                     */
/***********************************************************************/

#ifdef DEBUG
   #define DOPEN {if (dbg_fp == NULL) dbg_fp = fopen("sensCSM.dbg", "w");}
   static  FILE *dbg_fp=NULL;
#endif

                                               /* used by RALLOC macro */
//static void *realloc_temp=NULL;

#define  RED(COLOR)      (float)(COLOR / 0x10000        ) / (float)(255)
#define  GREEN(COLOR)    (float)(COLOR / 0x00100 % 0x100) / (float)(255)
#define  BLUE(COLOR)     (float)(COLOR           % 0x100) / (float)(255)

/***********************************************************************/
/*                                                                     */
/* global variables                                                    */
/*                                                                     */
/***********************************************************************/

/* global variable holding a MODL */
       char      casename[255];        /* name of case */
       char      pmtrname[255];        /* name of single design Parameter */
static void      *modl;                /* pointer to MODL */

static int       config   = 0;         /* =1 for configuration sensitivites */
static double    dtime    = 1.0e-6;    /* nominal dtime for perturbation */
static int       outLevel = 1;         /* default output level */
static int       tessel   = 0;         /* =1 for tessellation sensitivities */
static int       showAll  = 0;         /* =1 to show all velocities */
static double    errlist  = 1.0e-4;    /* maximum error to list */
static int       maxlist  = 10;        /* maximum number of errors to list */

/***********************************************************************/
/*                                                                     */
/* declarations                                                        */
/*                                                                     */
/***********************************************************************/

/* declarations for high-level routines defined below */
static int checkConfigSens(int ipmtr, int irow, int icol, int *ntotal, int *nsuppress, double *errmaxConf);
static int checkTesselSens(int ipmtr, int irow, int icol, int *ntotal, /*@unused@*/int *nsuppress, double *errmaxTess);


/***********************************************************************/
/*                                                                     */
/*   main - main program                                               */
/*                                                                     */
/***********************************************************************/

int
main(int       argc,                    /* (in)  number of arguments */
     char      *argv[])                 /* (in)  array of arguments */
{

    int       status, status2, i, nbody, ibody;
    int       imajor, iminor, builtTo, showUsage=0;
    int       ipmtr, irow, icol, ntotal, nsuppress=0;
    double    errmaxConf=0, errmaxTess=0;
    char      filename[255];
    CCHAR     *OCC_ver;
    ego       context;

    modl_T    *MODL;

    ROUTINE(MAIN);

    /* --------------------------------------------------------------- */

    DPRINT0("starting sensCSM");

    /* get the flags and casename from the command line */
    casename[0] = '\0';
    pmtrname[0] = '\0';

    for (i = 1; i < argc; i++) {
        if        (strcmp(argv[i], "--") == 0) {
            /* ignore (needed for gdb) */
        } else if (strcmp(argv[i], "-config") == 0) {
            config = 1;
        } else if (strcmp(argv[i], "-despmtr") == 0) {
            if (i < argc-1) {
                strcpy(pmtrname, argv[++i]);
            } else {
                showUsage = 1;
                break;
            }
        } else if (strcmp(argv[i], "-dtime") == 0) {
            if (i < argc-1) {
                sscanf(argv[++i], "%lf", &dtime);
            } else {
                showUsage = 1;
                break;
            }
        } else if (strcmp(argv[i], "-help") == 0 ||
                   strcmp(argv[i], "-h"   ) == 0   ) {
            showUsage = 1;
            break;
        } else if (strcmp(argv[i], "-outLevel") == 0) {
            if (i < argc-1) {
                sscanf(argv[++i], "%d", &outLevel);
                if (outLevel < 0) outLevel = 0;
                if (outLevel > 3) outLevel = 3;
            } else {
                showUsage = 1;
                break;
            }
        } else if (strcmp(argv[i], "-showAll") == 0) {
            showAll = 1;
        } else if (strcmp(argv[i], "-tessel") == 0) {
            tessel = 1;
        } else if (strcmp(argv[i], "--version") == 0 ||
                   strcmp(argv[i], "-version" ) == 0 ||
                   strcmp(argv[i], "-v"       ) == 0   ) {
            (void) ocsmVersion(&imajor, &iminor);
            SPRINT2(0, "OpenCSM version: %2d.%02d", imajor, iminor);
            EG_revision(&imajor, &iminor, &OCC_ver);
            SPRINT3(0, "EGADS   version: %2d.%02d (with %s)", imajor, iminor, OCC_ver);
            exit(EXIT_SUCCESS);
        } else if (strlen(casename) == 0) {
            strcpy(casename, argv[i]);
        } else {
            SPRINT0(0, "two casenames given");
            showUsage = 1;
            break;
        }
    }

    (void) ocsmVersion(&imajor, &iminor);

    if (showUsage) {
        SPRINT2(0, "sensCSM version %2d.%02d\n", imajor, iminor);
        SPRINT0(0, "proper usage: 'sensCSM [casename[.csm]] [options...]");
        SPRINT0(0, "   where [options...] = -config");
        SPRINT0(0, "                        -despmtr pmtrname");
        SPRINT0(0, "                        -dtime dtime");
        SPRINT0(0, "                        -help  -or-  -h");
        SPRINT0(0, "                        -outLevel X");
        SPRINT0(0, "                        -showAll");
        SPRINT0(0, "                        -tessel");
        SPRINT0(0, "STOPPING...\a");
        return EXIT_FAILURE;
    }

    /* if neither config or tessel are set, set tessel flag */
    if (config == 0 && tessel == 0) {
        SPRINT0(0, "ERROR:: either -config or -tessel must be set");
        SPRINT0(0, "STOPPING...\a");
        return EXIT_FAILURE;
    }

    /* welcome banner */
    SPRINT0(1, "**********************************************************");
    SPRINT0(1, "*                                                        *");
    SPRINT0(1, "*                    Program sensCSM                     *");
    SPRINT2(1, "*                     version %2d.%02d                      *", imajor, iminor);
    SPRINT0(1, "*                                                        *");
    SPRINT0(1, "*        written by John Dannenhoffer, 2010/2020         *");
    SPRINT0(1, "*                                                        *");
    SPRINT0(1, "**********************************************************\n");

    SPRINT1(1, "    casename   = %s", casename  );
    SPRINT1(1, "    config     = %d", config    );
    SPRINT1(1, "    despmtr    = %s", pmtrname  );
    SPRINT1(1, "    dtime      = %f", dtime     );
    SPRINT1(1, "    outLevel   = %d", outLevel  );
    SPRINT1(1, "    showAll    = %d", showAll   );
    SPRINT1(1, "    tessel     = %d", tessel    );
    SPRINT0(1, " ");

    /* set OCSMs output level */
    (void) ocsmSetOutLevel(outLevel);

    /* strip off .csm (which is assumed to be at the end) if present */
    if (strlen(casename) > 0) {
        strcpy(filename, casename);
        if (strstr(casename, ".csm") == NULL) {
            strcat(filename, ".csm");
        }
    } else {
        filename[0] = '\0';
    }

    /* read the .csm file and create the MODL */
    status   = ocsmLoad(filename, &modl);
    MODL = (modl_T*)modl;

    SPRINT3(1, "--> ocsmLoad(%s) -> status=%d (%s)",
            filename, status, ocsmGetText(status));
    CHECK_STATUS(ocsmLoad);

    /* check that Branches are properly ordered */
    status   = ocsmCheck(modl);
    SPRINT2(0, "--> ocsmCheck() -> status=%d (%s)",
            status, ocsmGetText(status));
    CHECK_STATUS(ocsmCheck);

    /* print out the global Attributes, Parameters, and Branches */
    SPRINT0(1, "External Parameter(s):");
    if (outLevel > 0) {
        status = ocsmPrintPmtrs(modl, stdout);
        CHECK_STATUS(ocsmPrintPmtrs);
    }

    SPRINT0(1, "Branch(es):");
    if (outLevel > 0) {
        status = ocsmPrintBrchs(modl, stdout);
        CHECK_STATUS(ocsmPrintBrchs);
    }

    SPRINT0(1, "Global Attribute(s):");
    if (outLevel > 0) {
        status = ocsmPrintAttrs(modl, stdout);
        CHECK_STATUS(ocsmPrintAttrs);
    }

    /* build the Bodys from the MODL */
    nbody    = 0;
    status   = ocsmBuild(modl, 0, &builtTo, &nbody, NULL);
    SPRINT4(1, "--> ocsmBuild -> status=%d (%s), builtTo=%d, nbody=%d",
            status, ocsmGetText(status), builtTo, nbody);
    CHECK_STATUS(ocsmBuild);

    /* print out the Bodys */
    SPRINT0(1, "Body(s):");
    if (outLevel > 0) {
        status = ocsmPrintBodys(modl, stdout);
        CHECK_STATUS(ocsmPrintBodys);
    }

    /* check configuration and tessellation sensitivities */
    ntotal     = 0;              // total number of errors exceeding tolerance
    errmaxConf = 0;              // maximmum error   (configuration)
    errmaxTess = 0;              // maximum error (tessellation)

    /* loop through all design Parameters */
    for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
        if (MODL->pmtr[ipmtr].type != OCSM_EXTERNAL) continue;

        if (strlen(pmtrname) > 0 &&
            strcmp(pmtrname, MODL->pmtr[ipmtr].name) != 0) continue;

        for (irow = 1; irow <= MODL->pmtr[ipmtr].nrow; irow++) {
            for (icol = 1; icol <= MODL->pmtr[ipmtr].ncol; icol++) {

//$$$                /* set the velocity */
//$$$                status = ocsmSetVelD(MODL, 0, 0, 0, 0.0);
//$$$                CHECK_STATUS(ocsmSetVelD);
//$$$                status = ocsmSetVelD(MODL, ipmtr, irow, icol, 1.0);
//$$$                CHECK_STATUS(ocsmSetVelD);
//$$$
//$$$                /* build needed to propagate velocities to the Branch arguments */
//$$$                ntemp    = 0;
//$$$                status   = ocsmBuild(MODL, 0, &builtTo, &ntemp, NULL);
//$$$                CHECK_STATUS(ocsmBuild);

                if (config) {
                    status = checkConfigSens(ipmtr, irow, icol, &ntotal, &nsuppress, &errmaxConf);
                    CHECK_STATUS(check_status);
                }

                if (tessel) {
                    status = checkTesselSens(ipmtr, irow, icol, &ntotal, &nsuppress, &errmaxTess);
                    CHECK_STATUS(checkTesselSens);
                }
            }
        }
        SPRINT0(0, " ");
    }

    SPRINT0(0, "==> sensCSM completed successfully");
    status = EXIT_SUCCESS;

    /* cleanup and exit */
cleanup:
    context = MODL->context;

    /* remove all Bodys and etess objects */
    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        if (MODL->body[ibody].etess != NULL) {
            status2 = EG_deleteObject(MODL->body[ibody].etess);
            SPRINT3(2, "--> EG_deleteObject(etess[%d]) -> status=%d (%s)",
                    ibody, status2, ocsmGetText(status2));

            MODL->body[ibody].etess = NULL;
        }

        if (MODL->body[ibody].ebody != NULL) {
            status2 = EG_deleteObject(MODL->body[ibody].ebody);
            SPRINT3(2, "--> EG_deleteObject(ebody[%d]) => status=%d (%s)",
                    ibody, status2, ocsmGetText(status2));

            MODL->body[ibody].ebody = NULL;
        }
    }

    /* free up the modl */
    status2 = ocsmFree(modl);
    SPRINT2(1, "--> ocsmFree() -> status=%d (%s)",
            status2, ocsmGetText(status2));

    /* remove tmp files (if they exist) and clean up the udp storage */
    status2 = ocsmFree(NULL);
    SPRINT2(1, "--> ocsmFree(NULL) -> status=%d (%s)",
            status2, ocsmGetText(status2));

    /* remove the context */
    if (context != NULL) {
        status2 = EG_setOutLevel(context, 0);
        if (status2 < 0) {
            SPRINT2(0, "EG_setOutLevel -> status=%d (%s)",
                    status2, ocsmGetText(status2));
        }

        status2 = EG_close(context);
        SPRINT2(1, "--> EG_close() -> status=%d (%s)",
                status2, ocsmGetText(status2));
    }

    /* report final statistics */
    if (status == SUCCESS) {
        if (config) {
            SPRINT3(0, "\nSensitivity checks complete with %8d total errors (max config err=%12.4e) with %d suppressions",
                    ntotal, errmaxConf+1.0e-20, nsuppress);
        }
        if (tessel) {
            SPRINT2(0, "\nSensitivity checks complete with %8d total errors (max tessel err=%12.4e)",
                ntotal, errmaxTess+1.0e-20);
        }
    } else {
        SPRINT1(0, "\nSensitivity checks not complete because error \"%s\" was detected",
                ocsmGetText(status));
    }

    /* these statements are in case we used an error return to go to cleanup */
    if (status < 0) status = EXIT_FAILURE;

    return status;
}


/***********************************************************************/
/*                                                                     */
/*   checkConfigSens - check configuration sensitivities               */
/*                                                                     */
/***********************************************************************/

static int
checkConfigSens(int    ipmtr,           /* (in)  Parameter index (bias-1) */
                int    irow,            /* (in)  row    index (bias-1) */
                int    icol,            /* (in)  column index (bias-1) */
                int    *ntotal,         /* (out) total number of points beyond toler */
                int    *nsuppress,      /* (out) number of suppressions */
                double *errmaxConf)     /* (out) maximum error */
{
    int       status = SUCCESS;

    int       i, ibody, nerror, nerror2, inode, iedge, iface, ipnt, npnt_tess, ntri_tess;
    int       ntemp, builtTo, atype, alen;
    CINT      *ptype, *pindx, *tris, *tric, *tempIlist;
    double    face_errmaxConf, edge_errmaxConf, node_errmaxConf, err;
    double    **face_anal=NULL, **face_fdif=NULL, face_err;
    double    **edge_anal=NULL, **edge_fdif=NULL, edge_err;
    double    **node_anal=NULL, **node_fdif=NULL, node_err;
    CDOUBLE   *xyz, *uv, *tempRlist;
    CCHAR     *tempClist;

    modl_T    *MODL = (modl_T *)modl;

    ROUTINE(checkConfigSens);

    /* --------------------------------------------------------------- */

    SPRINT0(0, "\n*********************************************************");
    if (MODL->pmtr[ipmtr].nrow == 1 &&
        MODL->pmtr[ipmtr].ncol == 1   ) {
        SPRINT1(0, "Starting configuration sensitivity wrt \"%s\"",
                MODL->pmtr[ipmtr].name);
    } else {
        SPRINT3(0, "Starting configuration sensitivity wrt \"%s[%d,%d]\"",
                MODL->pmtr[ipmtr].name, irow, icol);
    }
    SPRINT0(0, "*********************************************************\n");

    SPRINT0(0, "Propagating velocities throughout feature tree");

    ibody = MODL->nbody + 1;            /* protect for warning in cleanup: */

    /* set the velocity */
    status = ocsmSetVelD(MODL, 0, 0, 0, 0.0);
    CHECK_STATUS(ocsmSetVelD);
    status = ocsmSetVelD(MODL, ipmtr, irow, icol, 1.0);
    CHECK_STATUS(ocsmSetVelD);

    /* build needed to propagate velocities to the Branch arguments */
    ntemp    = 0;
    status   = ocsmBuild(MODL, 0, &builtTo, &ntemp, NULL);
    CHECK_STATUS(ocsmBuild);

    /* perform configuration sensitivity checks for each Body on the stack */
    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        if (MODL->body[ibody].onstack != 1) continue;

        assert(MODL->body[ibody].nnode > 0);
        assert(MODL->body[ibody].nedge > 0);
        assert(MODL->body[ibody].nface > 0);

        /* analytic configuration sensitivities (if possible) */
        SPRINT1(0, "Computing analytic sensitivities (if possible) for ibody=%d",
                ibody);
//$$$                        status = removePerturbation(MODL);
//$$$                        CHECK_STATUS(removePerturbation);
        status = ocsmSetDtime(MODL, 0);
        CHECK_STATUS(ocsmSetDtime);

        /* save analytic configuration sensitivity of each Face */
        MALLOC(face_anal, double*, MODL->body[ibody].nface+1);
        for (iface = 0; iface <= MODL->body[ibody].nface; iface++) {
            face_anal[iface] = NULL;
        }

        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            status = EG_getTessFace(MODL->body[ibody].etess, iface,
                                    &npnt_tess, &xyz, &uv, &ptype, &pindx,
                                    &ntri_tess, &tris, &tric);
            CHECK_STATUS(EG_getTessFace);
            if (npnt_tess <= 0) {
                SPRINT3(0, "ERROR:: EG_getTessFace -> status=%d (%s), npnt_tess=%d",
                        status, ocsmGetText(status), npnt_tess);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            MALLOC(face_anal[iface], double, 3*npnt_tess);

            status = ocsmGetVel(MODL, ibody, OCSM_FACE, iface, npnt_tess, NULL, face_anal[iface]);
            CHECK_STATUS(ocsmGetVel);
        }

        /* save analytic configuration sensitivity of each Edge */
        MALLOC(edge_anal, double*, MODL->body[ibody].nedge+1);
        for (iedge = 0; iedge <= MODL->body[ibody].nedge; iedge++) {
            edge_anal[iedge] = NULL;
        }

        for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
            status = EG_getTessEdge(MODL->body[ibody].etess, iedge,
                                    &npnt_tess, &xyz, &uv);
            CHECK_STATUS(EG_getTessEdge);
            if (npnt_tess <= 0) {
                SPRINT3(0, "ERROR:: EG_getTessEdge -> status=%d (%s), npnt_tess=%d",
                        status, ocsmGetText(status), npnt_tess);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            MALLOC(edge_anal[iedge], double, 3*npnt_tess);

            status = ocsmGetVel(MODL, ibody, OCSM_EDGE, iedge, npnt_tess, NULL, edge_anal[iedge]);
            CHECK_STATUS(ocsmGetVel);
        }

        /* save analytic configuration sensitivity of each Node */
        MALLOC(node_anal, double*, MODL->body[ibody].nnode+1);
        for (inode = 0; inode <= MODL->body[ibody].nnode; inode++) {
            node_anal[inode] = NULL;
        }

        for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
            npnt_tess = 1;

            MALLOC(node_anal[inode], double, 3*npnt_tess);

            status = ocsmGetVel(MODL, ibody, OCSM_NODE, inode, npnt_tess, NULL, node_anal[inode]);
            CHECK_STATUS(ocsmGetVel);
        }

        /* if there is a perturbation, return that finite differences were used */
        if (MODL->perturb != NULL) {
            SPRINT1(0, "\nSensitivity checks complete with %8d total errors (    finite diffs   )",
                    (*ntotal));
            status = EXIT_SUCCESS;
            goto cleanup;
        }

        /* finite difference configuration sensitivities */
        SPRINT1(0, "Computing finite difference sensitivities for ibody=%d",
                ibody);
//$$$                        status = createPerturbation(MODL);
//$$$                        CHECK_STATUS(createPerturbation);
        status = ocsmSetDtime(MODL, dtime);
        CHECK_STATUS(ocsmSetDtime);

        /* save finite difference configuration sensitivity of each Face */
        MALLOC(face_fdif, double*, MODL->body[ibody].nface+1);
        for (iface = 0; iface <= MODL->body[ibody].nface; iface++) {
            face_fdif[iface] = NULL;
        }

        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            status = EG_getTessFace(MODL->body[ibody].etess, iface,
                                    &npnt_tess, &xyz, &uv, &ptype, &pindx,
                                    &ntri_tess, &tris, &tric);
            CHECK_STATUS(EG_getTessFace);

            MALLOC(face_fdif[iface], double, 3*npnt_tess);

            status = ocsmGetVel(MODL, ibody, OCSM_FACE, iface, npnt_tess, NULL, face_fdif[iface]);
            CHECK_STATUS(ocsmGetVel);
        }

        /* save finite difference configuration sensitivity of each Edge */
        MALLOC(edge_fdif, double*, MODL->body[ibody].nedge+1);
        for (iedge = 0; iedge <= MODL->body[ibody].nedge; iedge++) {
            edge_fdif[iedge] = NULL;
        }

        for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
            status = EG_getTessEdge(MODL->body[ibody].etess, iedge,
                                    &npnt_tess, &xyz, &uv);
            CHECK_STATUS(EG_getTessEdge);

            MALLOC(edge_fdif[iedge], double, 3*npnt_tess);

            status = ocsmGetVel(MODL, ibody, OCSM_EDGE, iedge, npnt_tess, NULL, edge_fdif[iedge]);
            CHECK_STATUS(ocsmGetVel);
        }

        /* save finite difference configuration sensitivity of each Node */
        MALLOC(node_fdif, double*, MODL->body[ibody].nnode+1);
        for (inode = 0; inode <= MODL->body[ibody].nnode; inode++) {
            node_fdif[inode] = NULL;
        }

        for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
            npnt_tess = 1;

            MALLOC(node_fdif[inode], double, 3*npnt_tess);

            status = ocsmGetVel(MODL, ibody, OCSM_NODE, inode, npnt_tess, NULL, node_fdif[inode]);
            CHECK_STATUS(ocsmGetVel);
        }

//$$$                        status = removePerturbation(MODL);
//$$$                        CHECK_STATUS(removePerturbation);
        status = ocsmSetDtime(MODL, 0);
        CHECK_STATUS(ocsmSetDtime);

        /* compare configuration sensitivities */
        node_errmaxConf = 0;
        edge_errmaxConf = 0;
        face_errmaxConf = 0;

        if (MODL->pmtr[ipmtr].nrow == 1 &&
            MODL->pmtr[ipmtr].ncol == 1   ) {
            SPRINT2(0, "\nComparing configuration sensitivities wrt \"%s\" for ibody=%d",
                    MODL->pmtr[ipmtr].name, ibody);
        } else {
            SPRINT4(0, "\nComparing configuration sensitivities wrt \"%s[%d,%d]\" for ibody=%d",
                    MODL->pmtr[ipmtr].name, irow, icol, ibody);
        }

        /* print velocities for each Face, Edge, and Node */
#ifndef __clang_analyzer__
        if (showAll == 1) {
            for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
                status = EG_getTessFace(MODL->body[ibody].etess, iface,
                                        &npnt_tess, &xyz, &uv, &ptype, &pindx,
                                        &ntri_tess, &tris, &tric);
                CHECK_STATUS(EG_getTessFace);

                for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                    err = sqrt( pow(face_anal[iface][3*ipnt  ]-face_fdif[iface][3*ipnt  ],2)
                              + pow(face_anal[iface][3*ipnt+1]-face_fdif[iface][3*ipnt+1],2)
                              + pow(face_anal[iface][3*ipnt+2]-face_fdif[iface][3*ipnt+2],2));
                    SPRINT10(0, "Face %3d:%-3d %5d  %10.5f %10.5f  %10.5f %10.5f  %10.5f %10.5f  %15.10f",
                             ibody, iface, ipnt, face_anal[iface][3*ipnt  ], face_fdif[iface][3*ipnt  ],
                                                 face_anal[iface][3*ipnt+1], face_fdif[iface][3*ipnt+1],
                                                 face_anal[iface][3*ipnt+2], face_fdif[iface][3*ipnt+2], err);
                }
            }
            for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
                status = EG_getTessEdge(MODL->body[ibody].etess, iedge,
                                        &npnt_tess, &xyz, &uv);
                CHECK_STATUS(EG_getTessEdge);

                for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                    err = sqrt( pow(edge_anal[iedge][3*ipnt  ]-edge_fdif[iedge][3*ipnt  ],2)
                              + pow(edge_anal[iedge][3*ipnt+1]-edge_fdif[iedge][3*ipnt+1],2)
                              + pow(edge_anal[iedge][3*ipnt+2]-edge_fdif[iedge][3*ipnt+2],2));
                    SPRINT10(0, "Edge %3d:%-3d %5d  %10.5f %10.5f  %10.5f %10.5f  %10.5f %10.5f  %15.10f",
                             ibody, iedge, ipnt, edge_anal[iedge][3*ipnt  ], edge_fdif[iedge][3*ipnt  ],
                                                 edge_anal[iedge][3*ipnt+1], edge_fdif[iedge][3*ipnt+1],
                                                 edge_anal[iedge][3*ipnt+2], edge_fdif[iedge][3*ipnt+2], err);
                }
            }
            for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
                ipnt = 0;
                err = sqrt( pow(node_anal[inode][3*ipnt  ]-node_fdif[inode][3*ipnt  ],2)
                          + pow(node_anal[inode][3*ipnt+1]-node_fdif[inode][3*ipnt+1],2)
                          + pow(node_anal[inode][3*ipnt+2]-node_fdif[inode][3*ipnt+2],2));
                SPRINT10(0, "Node %3d:%-3d %5d  %10.5f %10.5f  %10.5f %10.5f  %10.5f %10.5f  %15.10f",
                         ibody, inode, ipnt, node_anal[inode][3*ipnt  ], node_fdif[inode][3*ipnt  ],
                                             node_anal[inode][3*ipnt+1], node_fdif[inode][3*ipnt+1],
                                             node_anal[inode][3*ipnt+2], node_fdif[inode][3*ipnt+2], err);
            }
        }
#endif

        /* compare configuration sensitivities for each Face */
        nerror = 0;
        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            status = EG_getTessFace(MODL->body[ibody].etess, iface,
                                    &npnt_tess, &xyz, &uv, &ptype, &pindx,
                                    &ntri_tess, &tris, &tric);
            CHECK_STATUS(EG_getTessFace);

            status = EG_attributeRet(MODL->body[ibody].face[iface].eface, "_sensCheck",
                                     &atype, &alen, &tempIlist, &tempRlist, &tempClist);
            if (status == EGADS_SUCCESS && atype == ATTRSTRING && strcmp(tempClist, "skip") == 0) {
                SPRINT2(0, "Tests suppressed for ibody=%3d, iface=%3d", ibody, iface);
                (*nsuppress)++;
            } else {
                status = EGADS_NOTFOUND;
            }

#ifndef __clang_analyzer__
            nerror2 = 0;
            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                for (i = 0; i < 3; i++) {
                    face_err = face_anal[iface][3*ipnt+i] - face_fdif[iface][3*ipnt+i];
                    if (status == EGADS_NOTFOUND && fabs(face_err) > face_errmaxConf) {
                        face_errmaxConf = fabs(face_err);
                    }
                    if (fabs(face_err) > errlist) {
                        if (nerror2 < maxlist) {
                            SPRINT9(0, "iface=%4d,  ipnt=%4d,  %1d anal=%16.8f,  fd=%16.8f,  err=%16.8f (at %16.8f %16.8f %16.8f)",
                                    iface, ipnt, i, face_anal[iface][3*ipnt+i], face_fdif[iface][3*ipnt+i], face_err,
                                    xyz[3*ipnt], xyz[3*ipnt+1], xyz[3*ipnt+2]);
                        } else if (nerror2 == maxlist) {
                            SPRINT0(0, "...too many errors to list");
                        }
                        nerror2++;
                        nerror++;
                        (*ntotal)++;
                    }
                }
            }
            status = EGADS_SUCCESS;

            FREE(face_anal[iface]);
            FREE(face_fdif[iface]);
#endif
        }
        (*errmaxConf) = MAX((*errmaxConf), face_errmaxConf);
        SPRINT3(0, "    d(Face)/d(%s) check complete with %8d total errors (errmaxConf=%12.4e)",
                MODL->pmtr[ipmtr].name, nerror, face_errmaxConf);

        /* compare configuration sensitivities for each Edge */
        nerror = 0;
        for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
            status = EG_getTessEdge(MODL->body[ibody].etess, iedge,
                                    &npnt_tess, &xyz, &uv);
            CHECK_STATUS(EG_getTessEdge);

            status = EG_attributeRet(MODL->body[ibody].edge[iedge].eedge, "_sensCheck",
                                     &atype, &alen, &tempIlist, &tempRlist, &tempClist);
            if (status == EGADS_SUCCESS && atype == ATTRSTRING && strcmp(tempClist, "skip") == 0) {
                SPRINT2(0, "Tests suppressed for ibody=%3d, iedge=%3d", ibody, iedge);
                (*nsuppress)++;
            } else {
                status = EGADS_NOTFOUND;
            }

#ifndef __clang_analyzer__
            nerror2 = 0;
            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                for (i = 0; i < 3; i++) {
                    edge_err = edge_anal[iedge][3*ipnt+i] - edge_fdif[iedge][3*ipnt+i];
                    if (status == EGADS_NOTFOUND && fabs(edge_err) > edge_errmaxConf) {
                        edge_errmaxConf = fabs(edge_err);
                    }
                    if (fabs(edge_err) > errlist) {
                        if (nerror2 < maxlist) {
                            SPRINT9(0, "iedge=%4d,  ipnt=%4d,  %1d anal=%16.8f,  fd=%16.8f,  err=%16.8f (at %16.8f %16.8f %16.8f)",
                                    iedge, ipnt, i, edge_anal[iedge][3*ipnt+i],
                                    edge_fdif[iedge][3*ipnt+i], edge_err,
                                    xyz[3*ipnt], xyz[3*ipnt+1], xyz[3*ipnt+2]);
                        } else if (nerror2 == maxlist) {
                            SPRINT0(0, "...too many errors to list");
                        }
                        nerror2++;
                        nerror++;
                        (*ntotal)++;
                    }
                }
            }
            status = EGADS_SUCCESS;

            FREE(edge_anal[iedge]);
            FREE(edge_fdif[iedge]);
#endif
        }
        (*errmaxConf) = MAX((*errmaxConf), edge_errmaxConf);
        SPRINT3(0, "    d(Edge)/d(%s) check complete with %8d total errors (errmaxConf=%12.4e)",
                MODL->pmtr[ipmtr].name, nerror, edge_errmaxConf);

        /* compare configuration sensitivities for each Node */
        nerror = 0;
        for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
            npnt_tess = 1;

            status = EG_attributeRet(MODL->body[ibody].node[inode].enode, "_sensCheck",
                                     &atype, &alen, &tempIlist, &tempRlist, &tempClist);
            if (status == EGADS_SUCCESS && atype == ATTRSTRING && strcmp(tempClist, "skip") == 0) {
                SPRINT2(0, "Tests suppressed for ibody=%3d, inode=%3d", ibody, inode);
                (*nsuppress)++;
            } else {
                status = EGADS_NOTFOUND;
            }

#ifndef __clang_analyzer__
            nerror2 = 0;
            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                for (i = 0; i < 3; i++) {
                    node_err = node_anal[inode][3*ipnt+i] - node_fdif[inode][3*ipnt+i];
                    if (status == EGADS_NOTFOUND && fabs(node_err) > node_errmaxConf) {
                        node_errmaxConf = fabs(node_err);
                    }
                    if (fabs(node_err) > errlist) {
                        if (nerror2 < maxlist) {
                            SPRINT9(0, "inode=%4d,  ipnt=%4d,  %1d anal=%16.8f,  fd=%16.8f,  err=%16.8f (at %16.8f %16.8f %16.8f)",
                                    inode, ipnt, i, node_anal[inode][3*ipnt+i], node_fdif[inode][3*ipnt+i], node_err,
                                    MODL->body[ibody].node[inode].x,
                                    MODL->body[ibody].node[inode].y,
                                    MODL->body[ibody].node[inode].z);
                        } else if (nerror2 == maxlist) {
                            SPRINT0(0, "...too many errors to list");
                        }
                        nerror2++;
                        nerror++;
                        (*ntotal)++;
                    }
                }
            }
            status = EGADS_SUCCESS;

            FREE(node_anal[inode]);
            FREE(node_fdif[inode]);
#endif
        }
        (*errmaxConf) = MAX((*errmaxConf), node_errmaxConf);
        SPRINT3(0, "    d(Node)/d(%s) check complete with %8d total errors (errmaxConf=%12.4e)",
                MODL->pmtr[ipmtr].name, nerror, node_errmaxConf);

        FREE(face_anal);
        FREE(face_fdif);
        FREE(edge_anal);
        FREE(edge_fdif);
        FREE(node_anal);
        FREE(node_fdif);
    }

cleanup:
    /* these FREEs only get used if an error was encountered above */
    if (ibody <= MODL->nbody) {
#ifndef __clang_analyzer__
        if (face_anal != NULL) {
            for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
                FREE(face_anal[iface]);
            }
        }
        if (face_fdif != NULL) {
            for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
                FREE(face_fdif[iface]);
            }
        }
        if (edge_anal != NULL) {
            for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
                FREE(edge_anal[iedge]);
            }
        }
        if (edge_fdif != NULL) {
            for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
                FREE(edge_fdif[iedge]);
            }
        }
        if (node_anal != NULL) {
            for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
                FREE(node_anal[inode]);
            }
        }
        if (node_fdif != NULL) {
            for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
                FREE(node_fdif[inode]);
            }
        }
#endif
    }

    FREE(face_anal);
    FREE(face_fdif);
    FREE(edge_anal);
    FREE(edge_fdif);
    FREE(node_anal);
    FREE(node_fdif);

    return status;
}


/***********************************************************************/
/*                                                                     */
/*   checkTesselSens - check tessellation sensitivities                */
/*                                                                     */
/***********************************************************************/

static int
checkTesselSens(int    ipmtr,           /* (in)  Parameter index (bias-1) */
                int    irow,            /* (in)  row    index (bias-1) */
                int    icol,            /* (in)  column index (bias-1) */
                int    *ntotal,         /* (out) ntotal number of points beyond toler */
    /*@unused@*/int    *nsuppress,      /* (out) number of suppressions */
                double *errmaxTess)     /* (out) maximum error */
{
    int       status = SUCCESS;

    int       ibody, jnode, jedge, jface, itype, nlist;
    int       nerror, inode, iedge, iface, ipnt, jpnt, periodic, iokay;
    int       npnt_tess, ntri_tess, oclass, mtype, nchild, *senses;
    CINT      *ptype, *pindx, *tris, *tric, *tempIlist;
    double    data[18], data2[18], trange[4];
    double    dist, dist2, xyz_pred[3], uv_close[2], xyz_close[3], toler;
    double    EPS05=1.0e-5;
    double    face_errmaxTess, edge_errmaxTess, node_errmaxTess;
    double    **face_anal=NULL, **edge_anal=NULL, **node_anal=NULL;
    CDOUBLE   *xyz, *uv, *dxyz, *xyz_ptrb, *uv_ptrb;
    ego       eref, *echilds;

    modl_T    *MODL = (modl_T *)modl;

    ROUTINE(checkTesselSens);

    /* --------------------------------------------------------------- */

    SPRINT0(0, "\n*********************************************************");
    if (MODL->pmtr[ipmtr].nrow == 1 &&
        MODL->pmtr[ipmtr].ncol == 1   ) {
        SPRINT1(0, "Starting tessellation sensitivity wrt \"%s\"",
                MODL->pmtr[ipmtr].name);
    } else {
        SPRINT3(0, "Starting tessellation sensitivity wrt \"%s[%d,%d]\"",
                MODL->pmtr[ipmtr].name, irow, icol);
    }
    SPRINT0(0, "*********************************************************\n");

    /* perform tessellation sensitivity checks for each Body on the stack */
    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        if (MODL->body[ibody].onstack != 1) continue;

        assert(MODL->body[ibody].nnode > 0);
        assert(MODL->body[ibody].nedge > 0);
        assert(MODL->body[ibody].nface > 0);

        /* analytic tessellation sensitivities (if possible) */
        SPRINT1(0, "Computing analytic sensitivities (if possible) for ibody=%d",
                ibody);
//$$$                        status = removePerturbation(MODL);
//$$$                        CHECK_STATUS(removePerturbation);
        status = ocsmSetDtime(MODL, 0);
        CHECK_STATUS(ocsmSetDtime);

        /* save analytic tessellation sensitivity of each Face */
        MALLOC(face_anal, double*, MODL->body[ibody].nface+1);
        for (iface = 0; iface <= MODL->body[ibody].nface; iface++) {
            face_anal[iface] = NULL;
        }

        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            status = EG_getTessFace(MODL->body[ibody].etess, iface,
                                    &npnt_tess, &xyz, &uv, &ptype, &pindx,
                                    &ntri_tess, &tris, &tric);
            CHECK_STATUS(EG_getTessFace);
            if (npnt_tess <= 0) {
                SPRINT3(0, "ERROR:: EG_getTessFace -> status=%d (%s), npnt_tess=%d",
                        status, ocsmGetText(status), npnt_tess);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            status = ocsmGetTessVel(MODL, ibody, OCSM_FACE, iface, &dxyz);
            CHECK_STATUS(ocsmGetTessVel);

            MALLOC(face_anal[iface], double, 3*npnt_tess);

            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                face_anal[iface][3*ipnt  ] = dxyz[3*ipnt  ];
                face_anal[iface][3*ipnt+1] = dxyz[3*ipnt+1];
                face_anal[iface][3*ipnt+2] = dxyz[3*ipnt+2];
            }
        }

        /* save analytic tessellation sensitivity of each Edge */
        MALLOC(edge_anal, double*, MODL->body[ibody].nedge+1);
        for (iedge = 0; iedge <= MODL->body[ibody].nedge; iedge++) {
            edge_anal[iedge] = NULL;
        }

        for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
            status = EG_getTessEdge(MODL->body[ibody].etess, iedge,
                                    &npnt_tess, &xyz, &uv);
            CHECK_STATUS(EG_getTessEdge);
            if (npnt_tess <= 0) {
                SPRINT3(0, "ERROR:: EG_getTessEdge -> status=%d (%s), npnt_tess=%d",
                        status, ocsmGetText(status), npnt_tess);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            status = ocsmGetTessVel(MODL, ibody, OCSM_EDGE, iedge, &dxyz);
            CHECK_STATUS(ocsmGetTessVel);

            MALLOC(edge_anal[iedge], double, 3*npnt_tess);

            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                edge_anal[iedge][3*ipnt  ] = dxyz[3*ipnt  ];
                edge_anal[iedge][3*ipnt+1] = dxyz[3*ipnt+1];
                edge_anal[iedge][3*ipnt+2] = dxyz[3*ipnt+2];
            }
        }

        /* save analytic tessellation sensitivity of each Node */
        MALLOC(node_anal, double*, MODL->body[ibody].nnode+1);
        for (inode = 0; inode <= MODL->body[ibody].nnode; inode++) {
            node_anal[inode] = NULL;
        }

        for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
            npnt_tess = 1;

            status = ocsmGetTessVel(MODL, ibody, OCSM_NODE, inode, &dxyz);
            CHECK_STATUS(ocsmGetTessVel);

            MALLOC(node_anal[inode], double, 3*npnt_tess);

            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                node_anal[inode][3*ipnt  ] = dxyz[3*ipnt  ];
                node_anal[inode][3*ipnt+1] = dxyz[3*ipnt+1];
                node_anal[inode][3*ipnt+2] = dxyz[3*ipnt+2];
            }
        }

        /* if there is a perturbation, return that finite differences were used */
        if (MODL->perturb != NULL) {
            SPRINT1(0, "\nSensitivity checks complete with %8d total errors (    finite diffs   )",
                    (*ntotal));
            status = EXIT_SUCCESS;
            goto cleanup;
        }

        /* finite difference tessellation sensitivities */
        SPRINT1(0, "Computing finite difference sensitivities for ibody=%d",
                ibody);
//$$$                        status = createPerturbation(MODL);
//$$$                        CHECK_STATUS(createPerturbation);
        status = ocsmSetDtime(MODL, dtime);
        CHECK_STATUS(ocsmSetDtime);

        /* compare tessellation sensitivities */
        node_errmaxTess = 0;
        edge_errmaxTess = 0;
        face_errmaxTess = 0;

        if (MODL->pmtr[ipmtr].nrow == 1 &&
            MODL->pmtr[ipmtr].ncol == 1   ) {
            SPRINT2(0, "\nComparing tessellation sensitivities wrt \"%s\" for ibody=%d",
                    MODL->pmtr[ipmtr].name, ibody);
        } else {
            SPRINT4(0, "\nComparing tessellation sensitivities wrt \"%s[%d,%d]\" for ibody=%d",
                    MODL->pmtr[ipmtr].name, irow, icol, ibody);
        }

        /* find the error between the analytical tessellation sensitivity with
           the perturbed Faces (made by finite differences) */
        nerror = 0;
        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            status = EG_getTessFace(MODL->body[ibody].etess, iface,
                                    &npnt_tess, &xyz, &uv, &ptype, &pindx,
                                    &ntri_tess, &tric, &tric);
            CHECK_STATUS(EG_getTessFace);

            status = EG_attributeRet(MODL->perturb->body[ibody].ebody, ".fMap",
                                     &itype, &nlist, &tempIlist, NULL, NULL);
            if (status == SUCCESS) {
                jface = tempIlist[iface-1];
            } else {
                jface = iface;
            }

            status = EG_getTolerance(MODL->perturb->body[ibody].face[iface].eface, &toler);
            CHECK_STATUS(EG_getTolerance);

            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
#ifndef __clang_analyzer__
                xyz_pred[0] = xyz[3*ipnt  ] + MODL->dtime * face_anal[iface][3*ipnt  ];
                xyz_pred[1] = xyz[3*ipnt+1] + MODL->dtime * face_anal[iface][3*ipnt+1];
                xyz_pred[2] = xyz[3*ipnt+2] + MODL->dtime * face_anal[iface][3*ipnt+2];
#endif
                uv_close[0] = uv[2*ipnt  ];
                uv_close[1] = uv[2*ipnt+1];

                status = EG_invEvaluateGuess(MODL->perturb->body[ibody].face[jface].eface,
                                             xyz_pred, uv_close, xyz_close);
                CHECK_STATUS(EG_getTessFace);

                dist = sqrt(SQR(xyz_pred[0]-xyz_close[0])
                            +SQR(xyz_pred[1]-xyz_close[1])
                            +SQR(xyz_pred[2]-xyz_close[2]));

                /* check to see if any tessellation point is closer than the
                   values returned by EG_invEvaluate */
                if (dist > toler) {
                    status = EG_getTessFace(MODL->perturb->body[ibody].etess, jface,
                                            &npnt_tess, &xyz_ptrb, &uv_ptrb, &ptype, &pindx,
                                            &ntri_tess, &tric, &tric);
                    CHECK_STATUS(EG_getTessFace);

                    iokay = 1;         /* means that EG_invEvaluate worked */
                    for (jpnt = 0; jpnt < npnt_tess; jpnt++) {
                        dist2 = sqrt(SQR(xyz_pred[0]-xyz_ptrb[3*jpnt  ])
                                     +SQR(xyz_pred[1]-xyz_ptrb[3*jpnt+1])
                                     +SQR(xyz_pred[2]-xyz_ptrb[3*jpnt+2]));
                        if (dist2 < dist) {
                            dist  = dist2;
                            iokay = 0;
                        }
                    }
                    if (iokay == 0) {
                        SPRINT5(0, "WARNING:: EG_invEvaluateGuess failed for ibody=%5d, jface=%5d, ipnt=%5d (%5d,%5d)",
                                ibody, jface, ipnt, ptype[ipnt], pindx[ipnt]);
                    }
                }

                if (dist > face_errmaxTess) face_errmaxTess = dist;
                if (dist > EPS05) {
                    if (nerror < 20 || dist >= face_errmaxTess) {
                        SPRINT6(0, "iface=%4d,  ipnt=%4d,  dist=%16.8f (at %16.8f %16.8f %16.8f)",
                                iface, ipnt, dist, xyz[3*ipnt], xyz[3*ipnt+1], xyz[3*ipnt+2]);
                    }
                    nerror++;
                    (*ntotal)++;
                }
            }
#ifndef __clang_analyzer__
            FREE(face_anal[iface]);
#endif
        }
        (*errmaxTess) = MAX((*errmaxTess), face_errmaxTess);
        SPRINT3(0, "    d(Face)/d(%s) check complete with %8d total errors (errmaxTess=%12.4e)",
                MODL->pmtr[ipmtr].name, nerror, face_errmaxTess);

        /* find the error between the analytical tessellation sensitivity with
           the perturbed Edges (made by finite differences) */
        nerror = 0;
        for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
            status = EG_getTopology(MODL->body[ibody].edge[iedge].eedge,
                                    &eref, &oclass, &mtype,
                                    data, &nchild, &echilds, &senses);
            CHECK_STATUS(EG_getTopology);

            if (mtype == DEGENERATE) {
#ifndef __clang_analyzer__
                FREE(edge_anal[iedge]);
#endif
                continue;
            }

            status = EG_getTessEdge(MODL->body[ibody].etess, iedge,
                                    &npnt_tess, &xyz, &uv);
            CHECK_STATUS(EG_getTessEdge);

            status = EG_attributeRet(MODL->perturb->body[ibody].ebody, ".eMap",
                                     &itype, &nlist, &tempIlist, NULL, NULL);
            if (status == SUCCESS) {
                jedge = tempIlist[iedge-1];
            } else {
                jedge = iedge;
            }

            status = EG_getTolerance(MODL->perturb->body[ibody].edge[iedge].eedge, &toler);
            CHECK_STATUS(EG_getTolerance);

            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
#ifndef __clang_analyzer__
                xyz_pred[0] = xyz[3*ipnt  ] + MODL->dtime * edge_anal[iedge][3*ipnt  ];
                xyz_pred[1] = xyz[3*ipnt+1] + MODL->dtime * edge_anal[iedge][3*ipnt+1];
                xyz_pred[2] = xyz[3*ipnt+2] + MODL->dtime * edge_anal[iedge][3*ipnt+2];
#endif
                uv_close[0] = uv[ipnt];

                status = EG_invEvaluateGuess(MODL->perturb->body[ibody].edge[jedge].eedge,
                                             xyz_pred, uv_close, xyz_close);
                CHECK_STATUS(EG_invEvaluate);

                dist = sqrt(SQR(xyz_pred[0]-xyz_close[0])
                           +SQR(xyz_pred[1]-xyz_close[1])
                           +SQR(xyz_pred[2]-xyz_close[2]));

                /* check to see if either of the endpoints is closer than the
                   values returned by EG_invEvaluate */
                if (dist > toler) {
                    status = EG_getRange(MODL->perturb->body[ibody].edge[jedge].eedge, trange, &periodic);
                    CHECK_STATUS(EG_getRange);

                    status = EG_evaluate(MODL->perturb->body[ibody].edge[jedge].eedge, &(trange[0]), data2);
                    CHECK_STATUS(EG_evaluate);

                    dist2 = sqrt(SQR(xyz_pred[0]-data2[0])
                                 +SQR(xyz_pred[1]-data2[1])
                                 +SQR(xyz_pred[2]-data2[2]));
                    if (dist2 < dist) {
                        SPRINT3(0, "WARNING:: EG_invEvaluateGuess failed for ibody=%5d, jedge=%5d, ipnt=%5d",
                                ibody, jedge, ipnt);

                        dist = dist2;
                    }

                    status = EG_evaluate(MODL->perturb->body[ibody].edge[jedge].eedge, &(trange[1]), data2);
                    CHECK_STATUS(EG_evaluate);

                    dist2 = sqrt(SQR(xyz_pred[0]-data2[0])
                                 +SQR(xyz_pred[1]-data2[1])
                                 +SQR(xyz_pred[2]-data2[2]));
                    if (dist2 < dist) {
                        SPRINT3(0, "WARNING:: EG_invEvaluateGuess failed for ibody=%5d, jedge=%5d, ipnt=%5d",
                                ibody, jedge, ipnt);

                        dist = dist2;
                    }
                }

                if (dist > edge_errmaxTess) edge_errmaxTess = dist;
                if (dist > EPS05) {
                    if (nerror < 20 || dist >= edge_errmaxTess) {
                        SPRINT6(0, "iedge=%4d,  ipnt=%4d,  dist=%16.8f (at %16.8f %16.8f %16.8f)",
                                iedge, ipnt, dist, xyz[3*ipnt], xyz[3*ipnt+1], xyz[3*ipnt+2]);
                    }
                    nerror++;
                    (*ntotal)++;
                }
            }
#ifndef __clang_analyzer__
            FREE(edge_anal[iedge]);
#endif
        }
        (*errmaxTess) = MAX((*errmaxTess), edge_errmaxTess);
        SPRINT3(0, "    d(Edge)/d(%s) check complete with %8d total errors (errmaxTess=%12.4e)",
                MODL->pmtr[ipmtr].name, nerror, edge_errmaxTess);

        /* find the error between the analytical tessellation sensitivity with
           the perturbed Nodes (made by finite differences) */
        nerror = 0;
        for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
            npnt_tess = 1;

            /* if .nMap exists, use mapped Node location in the perturbed Body */
#ifndef __clang_analyzer__
            status = EG_attributeRet(MODL->perturb->body[ibody].ebody, ".nMap",
                                     &itype, &nlist, &tempIlist, NULL, NULL);
            if (status == SUCCESS) {
                jnode = tempIlist[inode-1];
            } else {
                jnode = inode;
            }

            for (ipnt = 0; ipnt < npnt_tess; ipnt++) {
                xyz_pred[0] = MODL->body[ibody].node[inode].x + MODL->dtime * node_anal[inode][0];
                xyz_pred[1] = MODL->body[ibody].node[inode].y + MODL->dtime * node_anal[inode][1];
                xyz_pred[2] = MODL->body[ibody].node[inode].z + MODL->dtime * node_anal[inode][2];

                dist = sqrt(SQR(xyz_pred[0]-MODL->perturb->body[ibody].node[jnode].x)
                           +SQR(xyz_pred[1]-MODL->perturb->body[ibody].node[jnode].y)
                           +SQR(xyz_pred[2]-MODL->perturb->body[ibody].node[jnode].z));
                if (dist > node_errmaxTess) node_errmaxTess = dist;
                if (dist > EPS05) {
                    if (nerror < 20 || dist >= node_errmaxTess) {
                        SPRINT6(0, "inode=%4d,  ipnt=%4d,  dist=%16.8f (at %16.8f %16.8f %16.8f)",
                                inode, ipnt, dist,
                                MODL->body[ibody].node[inode].x,
                                MODL->body[ibody].node[inode].y,
                                MODL->body[ibody].node[inode].z);
                    }
                    nerror++;
                    (*ntotal)++;
                }
            }

            FREE(node_anal[inode]);
#endif
        }
        (*errmaxTess) = MAX((*errmaxTess), node_errmaxTess);
        SPRINT3(0, "    d(Node)/d(%s) check complete with %8d total errors (errmaxTess=%12.4e)",
                MODL->pmtr[ipmtr].name, nerror, node_errmaxTess);

        FREE(face_anal);
        FREE(edge_anal);
        FREE(node_anal);

//$$$                        status = removePerturbation(MODL);
//$$$                        CHECK_STATUS(removePerturbation);
        status = ocsmSetDtime(MODL, 0);
        CHECK_STATUS(ocsmSetDtime);
    }

cleanup:
    /* these FREEs only get used if an error was encountered above */
    if (ibody <= MODL->nbody) {
#ifndef __clang_analyzer__
        if (face_anal != NULL) {
            for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
                FREE(face_anal[iface]);
            }
        }
        if (edge_anal != NULL) {
            for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
                FREE(edge_anal[iedge]);
            }
        }
        if (node_anal != NULL) {
            for (inode = 1; inode <= MODL->body[ibody].nnode; inode++) {
                FREE(node_anal[inode]);
            }
        }
#endif
    }

    FREE(face_anal);
    FREE(edge_anal);
    FREE(node_anal);

    return status;
}
