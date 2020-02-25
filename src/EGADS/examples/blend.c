/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             An example using the blend function
 *
 *      Copyright 2011-2020, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <assert.h>
#include <math.h>
#include "egads.h"

//#define PERIODIC
//#define OPENLOOP
//#define SENSITIVITY

#ifdef SENSITIVITY
#include "egadsBlendSens.h"
#endif


int main(int argc, char *argv[])
{
  int    i, j, beg, end, brnd, ernd, status, senses[2], per, nface, rev = 1;
  int    btgt, etgt, atype, alen, nsec;
  double xyz[3], data[10], range[2], xform[12], rc[8], *rc1, *rcN;
  double btan[4], etan[4];
  ego    context, nodes[3], curve, edges[2], objs[2], oform, secs[7];
  ego    others[2], body, model, *faces;
  const  char   *str;
  const  int    *ints;
  const  double *reals;
  static char   *types[4] = {"Open", "Node", "Face", "Tip"};

  rc1   = rcN = NULL;
  rc[0] = 0.05;
  rc[4] = 0.4;
  rc[1] = rc[6] = 1.0;
  rc[2] = rc[3] = rc[5] = rc[7] = 0.0;
  brnd  = ernd = 0;
  btgt  = etgt = 0;
  if (argc != 3) {
    printf("\n Usage: blend 0/1/2/3 0/1/2/3 -- 0-open, 1-node, 2-face, 3-tip\n\n");
    return 1;
  }
  sscanf(argv[1], "%d", &beg);
  sscanf(argv[2], "%d", &end);
  if (beg == -1) beg = brnd = 1;
  if (end == -1) end = ernd = 1;
  if (beg == -2) beg = btgt = 2;
  if (end == -2) end = etgt = 2;
  if (beg <   0) beg = 0;
  if (beg >   3) beg = 3;
  if (end <   0) end = 0;
  if (end >   3) end = 3;
  printf("\n Blend with start %s and end %s\n\n", types[beg], types[end]);
  if (btgt == 2) {
    btan[0] = 1.0;
    printf(" Enter tangent at beginning: ");
    scanf(" %lf %lf %lf", &btan[1], &btan[2], &btan[3]);
    printf("\n");
  }
  if (etgt == 2) {
    etan[0] = 1.0;
    printf(" Enter tangent at end: ");
    scanf(" %lf %lf %lf", &etan[1], &etan[2], &etan[3]);
    printf("\n");
  }
  if (beg == 3) {
    btan[0] = 0.0;
    btan[1] = 1.0;
    rc1     = btan;
  }
  if (end == 3) {
    etan[0] = 0.0;
    etan[1] = 1.0;
    rcN     = etan;
  }
  
  printf(" EG_open            = %d\n", EG_open(&context));

  /* make the Nodes */
  xyz[0] =  1.0;
  xyz[1] = xyz[2] = 0.0;
  printf(" EG_makeTopology N0 = %d\n", EG_makeTopology(context, NULL, NODE, 0,
                                                       xyz, 0, NULL, NULL,
                                                       &nodes[0]));
  xyz[0] = -1.0;
  printf(" EG_makeTopology N1 = %d\n", EG_makeTopology(context, NULL, NODE, 0,
                                                       xyz, 0, NULL, NULL,
                                                       &nodes[1]));
#ifdef OPENLOOP
  xyz[0] = 0.0;
  xyz[1] = 1.0;
  printf(" EG_makeTopology N2 = %d\n", EG_makeTopology(context, NULL, NODE, 0,
                                                       xyz, 0, NULL, NULL,
                                                       &nodes[2]));
#endif

  /* make the Curve */
  data[0] = data[1] = data[2] = data[4] = data[5] = data[6] = data[8] = 0.0;
  data[3] = data[7] = data[9] = 1.0;
  printf(" EG_makeGeometry C0 = %d\n", EG_makeGeometry(context, CURVE, CIRCLE,
                                                       NULL, NULL, data,
                                                       &curve));
  status = EG_getRange(curve, range, &per);
  printf(" EG_getRange     C0 = %d -  %lf %lf\n", status, range[0], range[1]);

  /* construct the Edges */
#ifdef PERIODIC
  objs[0] = nodes[0];
  objs[1] = nodes[1];
  data[0] = range[0];
  data[1] = range[1];
  printf(" EG_makeTopology E0 = %d\n", EG_makeTopology(context, curve, EDGE,
                                                       ONENODE, data, 1,
                                                       objs, NULL, &edges[0]));
  /* make the Loop */
  senses[0] = rev;
  printf(" EG_makeTopology L  = %d\n", EG_makeTopology(context, NULL, LOOP,
                                                       CLOSED, NULL, 1, edges,
                                                       senses, &secs[0]));
#else
#ifdef OPENLOOP
  objs[0] = nodes[0];
  objs[1] = nodes[2];
  data[0] = range[0];
  data[1] = range[0] + 0.25*(range[1]-range[0]);
  printf(" EG_makeTopology E0 = %d\n", EG_makeTopology(context, curve, EDGE,
                                                       TWONODE, data, 2,
                                                       objs, NULL, &edges[0]));
  objs[0] = nodes[2];
  objs[1] = nodes[1];
  data[0] = data[1];
  data[1] = range[0] + 0.5*(range[1]-range[0]);
  printf(" EG_makeTopology E1 = %d\n", EG_makeTopology(context, curve, EDGE,
                                                       TWONODE, data, 2,
                                                       objs, NULL, &edges[1]));
  /* make the Loop */
  senses[0] = rev;
  senses[1] = rev;
  printf(" EG_makeTopology L  = %d\n", EG_makeTopology(context, NULL, LOOP,
                                                       OPEN, NULL, 2, edges,
                                                       senses, &secs[0]));
#else
  objs[0] = nodes[0];
  objs[1] = nodes[1];
  data[0] = range[0];
  data[1] = range[0] + 0.5*(range[1]-range[0]);
  printf(" EG_makeTopology E0 = %d\n", EG_makeTopology(context, curve, EDGE,
                                                       TWONODE, data, 2,
                                                       objs, NULL, &edges[0]));
  objs[0] = nodes[1];
  objs[1] = nodes[0];
  data[0] = data[1];
  data[1] = range[1];
  printf(" EG_makeTopology E1 = %d\n", EG_makeTopology(context, curve, EDGE,
                                                       TWONODE, data, 2,
                                                       objs, NULL, &edges[1]));
  /* make the Loop */
  senses[0] = rev;
  senses[1] = rev;
  printf(" EG_makeTopology L  = %d\n", EG_makeTopology(context, NULL, LOOP,
                                                       CLOSED, NULL, 2, edges,
                                                       senses, &secs[0]));
#endif
#endif

  /* make a transform */
  for (i = 1; i < 11; i++) xform[i] = 0.0;
  xform[0]  = xform[5] = xform[10]  = 1.1;
  xform[11] = 1.0;
  printf(" EG_makeTransform   = %d\n", EG_makeTransform(context, xform,
                                                        &oform));

  /* make the sections */
  for (i = 1; i < 5; i++)
    printf(" EG_copyObject %d    = %d\n", i, EG_copyObject( secs[i-1], oform,
                                                           &secs[i]));
  EG_deleteObject(oform);
  
  /* deal with the ends */
  others[0] = others[1] = NULL;
  if (beg == 1) {
    if (brnd == 1) rc1 = rc;
    EG_deleteObject(secs[0]);
    xyz[0] = xyz[1] = xyz[2] = 0.0;
    printf(" EG_makeTopology Nb = %d\n", EG_makeTopology(context, NULL, NODE, 0,
                                                         xyz, 0, NULL, NULL,
                                                         &secs[0]));
  } else if (beg >= 2) {
    if (btgt == 2) rc1 = btan;
    others[0] = secs[0];
    printf(" EG_makeFace beg    = %d\n", EG_makeFace(others[0], SREVERSE*rev,
                                                     NULL, &secs[0]));
  }
  if (end == 1) {
    if (ernd == 1) rcN = rc;
    EG_deleteObject(secs[4]);
    xyz[0] = xyz[1] = 0.0;
    xyz[2] = 5.0;
    printf(" EG_makeTopology Ne = %d\n", EG_makeTopology(context, NULL, NODE, 0,
                                                         xyz, 0, NULL, NULL,
                                                         &secs[4]));
  } else if (end >= 2) {
    if (etgt == 2) rcN = etan;
    others[1] = secs[4];
    printf(" EG_makeFace end    = %d\n", EG_makeFace(others[1], SFORWARD*rev,
                                                     NULL, &secs[4]));
  }
  nsec = 5;
  
/* multiplicity of sections
  for (i = nsec-1; i > 2; i--) secs[i+1] = secs[i];
  secs[3] = secs[2];
  nsec++;
*/
/*
  for (i = nsec-1; i > 2; i--) secs[i+2] = secs[i];
  secs[3] = secs[4] = secs[2];
  nsec += 2;
*/

  /* blend & save */
  printf(" EG_blend           = %d\n", EG_blend(nsec, secs, rc1, rcN, &body));
  printf(" EG_getBodyTopos    = %d\n", EG_getBodyTopos(body, NULL, FACE,
                                                       &nface, &faces));
#ifdef SENSITIVITY
  {
    int    k, m, npt, imax, nstrip, oclass, mtype, *sens;
    double dist, *ts, uv[2], result[18];
    double X[3*12*7], Xdot[3*12*7], tbeg[3*7], tend[3*7];
    ego    ref, *objs, *dum;
    void   *cache;
    
    printf("\n");
    for (i = 0; i < 8; i++) data[i] = 0.0;
    status = EG_blend_init(nsec, secs, rc1, data, rcN, data, &nstrip, &cache);
    printf(" EG_blend_init       = %d  %d\n", status, nstrip);
    if (status == EGADS_SUCCESS) {
      for (m = 1; m <= nstrip; m++) {
        status = EG_blend_pos(cache, m, &objs, &imax, &ts);
        printf("  Strip = %d  status = %d\n", m, status);
        assert( imax <= 12 );
        assert( status == EGADS_SUCCESS );
        for (npt = i = 0; i < nsec; i++) {
          status = EG_getTopology(objs[i], &ref, &oclass, &mtype, xyz, &j, &dum,
                                  &sens);
          if (status != EGADS_SUCCESS) printf(" EG_getTopology = %d\n", status);
          if (oclass == NODE) {
            for (j = 0; j < imax; j++, npt++) {
              X[3*npt  ]    = xyz[0];
              X[3*npt+1]    = xyz[1];
              X[3*npt+2]    = xyz[2];
              Xdot[3*npt  ] = 0.0;
              Xdot[3*npt+1] = 0.0;
              Xdot[3*npt+2] = 0.0;
            }
            tbeg[3*i  ] = 0.0;
            tbeg[3*i+1] = 0.0;
            tbeg[3*i+2] = 0.0;
            tend[3*i  ] = 0.0;
            tend[3*i+1] = 0.0;
            tend[3*i+2] = 0.0;
          } else {
            for (j = 0; j < imax; j++, npt++) {
              status = EG_evaluate(objs[i], &ts[npt], result);
              if (status != EGADS_SUCCESS)
                printf("  EG_evaluate = %d for %d %d %d\n", status, m, i+1, j+1);
              X[3*npt  ]    = result[0];
              X[3*npt+1]    = result[1];
              X[3*npt+2]    = result[2];
              Xdot[3*npt  ] = 0.0;
              Xdot[3*npt+1] = 0.0;
              Xdot[3*npt+2] = 0.0;
              if (j == 0) {
                tbeg[3*i  ] = result[3]*(xyz[1]-xyz[0]);
                tbeg[3*i+1] = result[4]*(xyz[1]-xyz[0]);
                tbeg[3*i+2] = result[5]*(xyz[1]-xyz[0]);
              }
              if (j == imax-1) {
                tend[3*i  ] = result[3]*(xyz[1]-xyz[0]);
                tend[3*i+1] = result[4]*(xyz[1]-xyz[0]);
                tend[3*i+2] = result[5]*(xyz[1]-xyz[0]);
              }
            }
          }
        }
        status = EG_blend_sens(cache, m, X, Xdot, tbeg, NULL, tend, NULL);
        printf("  EG_blend_sens = %d for strip %d\n", status, m);
      }
    }
    /* spot compare surfaces */
    for (m = 1; m <= nstrip+2; m++) {
      j = m;
      if (m == nstrip+1) {
        if (beg == 3) {
          j = -1;
        } else {
          continue;
        }
      }
      if (m == nstrip+2) {
        if (end == 3) {
          j = -2;
        } else {
          continue;
        }
      }
      uv[0]  = 0.9;
      uv[1]  = 0.9;
      status = EG_blend_seval(cache, j, uv, xyz, data);
      printf("  %d:  %lf %lf %lf  %d  %lf %lf %lf\n",
             m, xyz[0], xyz[1], xyz[2], status, data[0], data[1], data[2]);
      status = EG_evaluate(faces[m-1], uv, result);
      dist   = sqrt((xyz[0]-result[0])*(xyz[0]-result[0]) +
                    (xyz[1]-result[1])*(xyz[1]-result[1]) +
                    (xyz[2]-result[2])*(xyz[2]-result[2]));
      printf("  %d:  %lf %lf %lf  %d  %le\n",
             m, result[0], result[1], result[2], status, dist);
    }
    EG_sens_free(cache);
    printf("\n");
  }
#endif

  for (i = 0; i < nface; i++) {
    status = EG_attributeRet(faces[i], ".blendSamples", &atype, &alen,
                             &ints, &reals, &str);
    if (status == EGADS_SUCCESS) {
      printf("   Face %d/%d: blendSamples =", i+1, nface);
      if (atype == ATTRREAL) {
        for (j = 0; j < alen; j++) printf(" %lf", reals[j]);
      } else {
        printf(" atype = %d, alen = %d", atype, alen);
      }
      printf("\n");
    }
    status = EG_attributeRet(faces[i], ".blendSenses", &atype, &alen,
                             &ints, &reals, &str);
    if (status == EGADS_SUCCESS) {
      printf("   Face %d/%d: blendSenses =", i+1, nface);
      if (atype == ATTRINT) {
        for (j = 0; j < alen; j++) printf(" %d", ints[j]);
      } else {
        printf(" atype = %d, alen = %d", atype, alen);
      }
      printf("\n");
    }
  }
  printf(" EG_makeTopology M  = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                       NULL, 1, &body,
                                                       NULL, &model));
  printf(" EG_saveModel       = %d\n", EG_saveModel(model, "blend.egads"));
  printf("\n");
  EG_free(faces);

  /* cleanup */
  EG_deleteObject(model);
  for (i = nsec-1; i >= 0; i--) EG_deleteObject(secs[i]);
  if (others[0] != NULL) EG_deleteObject(others[0]);
  if (others[1] != NULL) EG_deleteObject(others[1]);
#ifndef PERIODIC
  EG_deleteObject(edges[1]);
#endif
  EG_deleteObject(edges[0]);
  EG_deleteObject(curve);
  EG_deleteObject(nodes[1]);
  EG_deleteObject(nodes[0]);
#ifdef OPENLOOP
  EG_deleteObject(nodes[2]);
#endif

  EG_setOutLevel(context, 2);
  printf(" EG_close           = %d\n", EG_close(context));

  return 0;
}
