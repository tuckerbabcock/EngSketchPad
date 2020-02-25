/*
 ************************************************************************
 *                                                                      *
 * udpEllipse -- udp file to generate an ellipse                        *
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

#define NUMUDPARGS 3
#include "udpUtilities.h"

/* shorthands for accessing argument values and velocities */
#define RX(    IUDP)  ((double *) (udps[IUDP].arg[0].val))[0]
#define RX_DOT(IUDP)  ((double *) (udps[IUDP].arg[0].dot))[0]
#define RY(    IUDP)  ((double *) (udps[IUDP].arg[1].val))[0]
#define RY_DOT(IUDP)  ((double *) (udps[IUDP].arg[1].dot))[0]
#define RZ(    IUDP)  ((double *) (udps[IUDP].arg[2].val))[0]
#define RZ_DOT(IUDP)  ((double *) (udps[IUDP].arg[2].dot))[0]

/* data about possible arguments */
static char  *argNames[NUMUDPARGS] = {"rx",        "ry",        "rz",        };
static int    argTypes[NUMUDPARGS] = {ATTRREALSEN, ATTRREALSEN, ATTRREALSEN, };
static int    argIdefs[NUMUDPARGS] = {0,           0,           0,           };
static double argDdefs[NUMUDPARGS] = {0.,          0.,          0.,          };

/* get utility routines: udpErrorStr, udpInitialize, udpReset, udpSet,
                         udpGet, udpVel, udpClean, udpMesh */
#include "udpUtilities.c"

#define  EPS06   1.0e-6


/*
 ************************************************************************
 *                                                                      *
 *   udpExecute - execute the primitive                                 *
 *                                                                      *
 ************************************************************************
 */

int
udpExecute(ego  context,                /* (in)  EGADS context */
           ego  *ebody,                 /* (out) Body pointer */
           int  *nMesh,                 /* (out) number of associated meshes */
           char *string[])              /* (out) error message */
{
    int     status = EGADS_SUCCESS;

    int     senses[2], add=1;
    double  params[11], node[3], data[18], trange[4];
    ego     enodes[3], ecurve, eedges[2], eloop, eface;

#ifdef DEBUG
    printf("udpExecute(context=%llx)\n", (long long)context);
    printf("rx(0)     = %f\n", RX(    0));
    printf("rx_dot(0) = %f\n", RX_DOT(0));
    printf("ry(0)     = %f\n", RY(    0));
    printf("ry_dot(0) = %f\n", RY_DOT(0));
    printf("rz(0)     = %f\n", RZ(    0));
    printf("rz_dot(0) = %f\n", RZ_DOT(0));
#endif

    /* default return values */
    *ebody  = NULL;
    *nMesh  = 0;
    *string = NULL;

    /* check arguments */
    if (udps[0].arg[0].size > 1) {
        printf(" udpExecute: rx should be a scalar\n");
        status  = EGADS_RANGERR;
        goto cleanup;

    } else if (RX(0) < 0) {
        printf(" udpExecute: rx = %f < 0\n", RX(0));
        status  = EGADS_RANGERR;
        goto cleanup;

    } else if (udps[0].arg[1].size > 1) {
        printf(" udpExecute: ry should be a scalar\n");
        status  = EGADS_RANGERR;
        goto cleanup;

    } else if (RY(0) < 0) {
        printf(" udpExecute: ry = %f < 0\n", RY(0));
        status  = EGADS_RANGERR;
        goto cleanup;

    } else if (udps[0].arg[2].size > 1) {
        printf(" udpExecute: rz should be a scalar\n");
        status  = EGADS_RANGERR;
        goto cleanup;

    } else if (RZ(0) < 0) {
        printf(" udpExecute: rz = %f < 0\n", RZ(0));
        status  = EGADS_RANGERR;
        goto cleanup;
    }

    /* cache copy of arguments for future use */
    status = cacheUdp();
    if (status < 0) {
        printf(" udpExecute: problem caching arguments\n");
        goto cleanup;
    }

#ifdef DEBUG
    printf("rx(%d) = %f\n", numUdp, RX(numUdp));
    printf("ry(%d) = %f\n", numUdp, RY(numUdp));
    printf("rz(%d) = %f\n", numUdp, RZ(numUdp));
#endif

    /* ellipses are centered at origin */
    params[0] = 0;
    params[1] = 0;
    params[2] = 0;

    /* ellipse in y-z plane */
    if (RX(0) == 0 && RY(0) > 0 && RZ(0) > 0) {
        node[0] =  0;
        node[1] =  0;
        node[2] = -RZ(0);

        if (RY(0) >= RZ(0)) {
            params[ 3] = 0;
            params[ 4] = 1;
            params[ 5] = 0;

            params[ 6] = 0;
            params[ 7] = 0;
            params[ 8] = 1;

            params[ 9] = RY(0);
            params[10] = RZ(0);
        } else {
            params[ 3] = 0;
            params[ 4] = 0;
            params[ 5] = 1;

            params[ 6] = 0;
            params[ 7] = -1;
            params[ 8] = 0;

            params[ 9] = RZ(0);
            params[10] = RY(0);
        }

    /* ellipse in z-x plane */
    } else if (RY(0) == 0 && RZ(0) > 0 && RX(0) > 0) {
        node[0] = -RX(0);
        node[1] =  0;
        node[2] =  0;

        if (RZ(0) >= RX(0)) {
            params[ 3] = 0;
            params[ 4] = 0;
            params[ 5] = 1;

            params[ 6] = 1;
            params[ 7] = 0;
            params[ 8] = 0;

            params[ 9] = RZ(0);
            params[10] = RX(0);
        } else {
            params[ 3] = 1;
            params[ 4] = 0;
            params[ 5] = 0;

            params[ 6] = 0;
            params[ 7] = 0;
            params[ 8] = -1;

            params[ 9] = RX(0);
            params[10] = RZ(0);
        }

    /* ellipse in x-y plane */
    } else if (RZ(0) == 0 && RX(0) > 0 && RY(0) > 0) {
        node[0] =  0;
        node[1] = -RY(0);
        node[2] =  0;

        if (RX(0) >= RY(0)) {
            params[ 3] = 1;
            params[ 4] = 0;
            params[ 5] = 0;

            params[ 6] = 0;
            params[ 7] = 1;
            params[ 8] = 0;

            params[ 9] = RX(0);
            params[10] = RY(0);
        } else {
            params[ 3] = 0;
            params[ 4] = 1;
            params[ 5] = 0;

            params[ 6] = -1;
            params[ 7] = 0;
            params[ 8] = 0;

            params[ 9] = RY(0);
            params[10] = RX(0);
        }

    /* illegal combination of rx, ry, and zz */
    } else {
        status = EGADS_GEOMERR;
        goto cleanup;
    }

    /* make the Curve */
    status = EG_makeGeometry(context, CURVE, ELLIPSE, NULL, NULL, params, &ecurve);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* make the two Nodes and find the associated parameter values */
    status = EG_makeTopology(context, NULL, NODE, 0, node, 0, NULL, NULL, &(enodes[0]));
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_invEvaluate(ecurve, node, &(trange[0]), data);
    if (status != EGADS_SUCCESS) goto cleanup;

    node[0] *= -1;
    node[1] *= -1;
    node[2] *= -1;

    status = EG_makeTopology(context, NULL, NODE, 0, node, 0, NULL, NULL, &(enodes[1]));
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_invEvaluate(ecurve, node, &(trange[1]), data);
    if (status != EGADS_SUCCESS) goto cleanup;

    if (trange[1] < trange[0]) {
        trange[1] += TWOPI;
    }

    trange[2] = trange[0] + TWOPI;

    enodes[2] = enodes[0];

    /* make the Edges */
    status = EG_makeTopology(context, ecurve, EDGE, TWONODE, &(trange[0]), 2, &(enodes[0]), NULL, &(eedges[0]));
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_makeTopology(context, ecurve, EDGE, TWONODE, &(trange[1]), 2, &(enodes[1]), NULL, &(eedges[1]));
    if (status != EGADS_SUCCESS) goto cleanup;

    /* make Loop from this Edge */
    senses[0] = SFORWARD;
    senses[1] = SFORWARD;

    status = EG_makeTopology(context, NULL, LOOP, CLOSED, NULL, 2, eedges, senses, &eloop);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* make Face from the loop */
    status = EG_makeFace(eloop, SFORWARD, NULL, &eface);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* since this will make a PLANE, we need to add an Attribute
       to tell OpenCSM to scale the UVs when computing sensitivities */
    status = EG_attributeAdd(eface, "_scaleuv", ATTRINT, 1,
                             &add, NULL, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* create the FaceBody (which will be returned) */
    status = EG_makeTopology(context, NULL, BODY, FACEBODY, NULL, 1, &eface, senses, ebody);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* set the output value(s) */

    /* remember this model (body) */
    udps[numUdp].ebody = *ebody;

#ifdef DEBUG
    printf("udpExecute -> *ebody=%llx\n", (long long)(*ebody));
#endif

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
    int    status = EGADS_SUCCESS;

    int    iudp, judp, ipnt, nnode, nedge, nface, nchild, oclass, mtype, *senses;
    double data[18];
    ego    eref, *echilds, *enodes, *eedges, *efaces, eent;

#ifdef DEBUG
    printf("udpSensitivity(ebody=%llx, npnt=%d, entType=%d, entIndex=%d, uvs=%f %f)\n",
           (long long)ebody, npnt, entType, entIndex, uvs[0], uvs[1]);
#endif

    /* check that ebody matches one of the ebodys */
    iudp = 0;
    for (judp = 1; judp <= numUdp; judp++) {
        if (ebody == udps[judp].ebody) {
            iudp = judp;
            break;
        }
    }
    if (iudp <= 0) {
        status = EGADS_NOTMODEL;
        goto cleanup;
    }

    /* find the ego entity */
    if (entType == OCSM_NODE) {
        status = EG_getBodyTopos(ebody, NULL, NODE, &nnode, &enodes);
        if (status != EGADS_SUCCESS) goto cleanup;

        eent = enodes[entIndex-1];

        EG_free(enodes);
    } else if (entType == OCSM_EDGE) {
        status = EG_getBodyTopos(ebody, NULL, EDGE, &nedge, &eedges);
        if (status != EGADS_SUCCESS) goto cleanup;

        eent = eedges[entIndex-1];

        EG_free(eedges);
    } else if (entType == OCSM_FACE) {
        status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
        if (status != EGADS_SUCCESS) goto cleanup;

        eent = efaces[entIndex-1];

        EG_free(efaces);
    } else {
        printf("udpSensitivity: bad entType=%d\n", entType);
        status = EGADS_ATTRERR;
        goto cleanup;
    }

    /* loop through the points */
    for (ipnt = 0; ipnt < npnt; ipnt++) {

        /* find the physical coordinates */
        if        (entType == OCSM_NODE) {
            status = EG_getTopology(eent, &eref, &oclass, &mtype,
                                    data, &nchild, &echilds, &senses);
            if (status != EGADS_SUCCESS) goto cleanup;
        } else if (entType == OCSM_EDGE) {
            status = EG_evaluate(eent, &(uvs[ipnt]), data);
            if (status != EGADS_SUCCESS) goto cleanup;
        } else if (entType == OCSM_FACE) {
            status = EG_evaluate(eent, &(uvs[2*ipnt]), data);
            if (status != EGADS_SUCCESS) goto cleanup;
        }

        /* compute the sensitivity */
        if (fabs(RX(iudp)) > EPS06) {
            vels[3*ipnt  ] = data[0] / RX(iudp) * RX_DOT(iudp);
        } else {
            vels[3*ipnt  ] = 0;
        }
        if (fabs(RY(iudp)) > EPS06) {
            vels[3*ipnt+1] = data[1] / RY(iudp) * RY_DOT(iudp);
        } else {
            vels[3*ipnt+1] = 0;
        }
        if (fabs(RZ(iudp)) > EPS06) {
            vels[3*ipnt+2] = data[2] / RZ(iudp) * RZ_DOT(iudp);
        } else {
            vels[3*ipnt+2] = 0;
        }
    }

    status = EGADS_SUCCESS;

cleanup:
    return status;
}
