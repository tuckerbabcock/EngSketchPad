#include <math.h>
#include "egads.h"
#include "egads_dot.h"
#include "egadsSplineVels.h"

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define __func__  __FUNCTION__
#endif

#define TWOPI 6.2831853071795862319959269
#define PI    (TWOPI/2.0)
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define DOT(a,b)          (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])
#define CROSS(a,b,c)       a[0] = (b[1]*c[2]) - (b[2]*c[1]);\
                           a[1] = (b[2]*c[0]) - (b[0]*c[2]);\
                           a[2] = (b[0]*c[1]) - (b[1]*c[0])
#define CROSS_DOT(a_dot,b,b_dot,c,c_dot) a_dot[0] = (b_dot[1]*c[2]) + (b[1]*c_dot[2]) - (b_dot[2]*c[1]) - (b[2]*c_dot[1]);\
                                         a_dot[1] = (b_dot[2]*c[0]) + (b[2]*c_dot[0]) - (b_dot[0]*c[2]) - (b[0]*c_dot[2]);\
                                         a_dot[2] = (b_dot[0]*c[1]) + (b[0]*c_dot[1]) - (b_dot[1]*c[0]) - (b[1]*c_dot[0])

/*****************************************************************************/
/*                                                                           */
/*  pingBodies                                                               */
/*                                                                           */
/*****************************************************************************/

int
pingBodies(ego tess1, ego tess2, double dtime, int iparam, const char *shape, double ftol, double etol, double ntol)
{
  int    status = EGADS_SUCCESS;
  int    n, d, np1, np2, nt1, nt2, nerr=0;
  int    nface, nedge, nnode, iface, iedge, inode, oclass, mtype;
  double p1_dot[18], p1[18], p2[18], fd_dot[3];
  const int    *pt1, *pi1, *pt2, *pi2, *ts1, *tc1, *ts2, *tc2;
  const double *t1, *t2, *x1, *x2, *uv1, *uv2;
  ego    ebody1, ebody2;
  ego    *efaces1=NULL, *efaces2=NULL, *eedges1=NULL, *eedges2=NULL, *enodes1=NULL, *enodes2=NULL;
  ego    top, prev, next;

  status = EG_statusTessBody( tess1, &ebody1, &np1, &np2 );
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_statusTessBody( tess2, &ebody2, &np1, &np2 );
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Faces from the Body 1 */
  status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, &efaces1);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Edges from the Body 1 */
  status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, &eedges1);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Nodes from the Body 1 */
  status = EG_getBodyTopos(ebody1, NULL, NODE, &nnode, &enodes1);
  if (status != EGADS_SUCCESS) goto cleanup;


  /* get the Faces from the Body 2 */
  status = EG_getBodyTopos(ebody2, NULL, FACE, &nface, &efaces2);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Edges from the Body 2 */
  status = EG_getBodyTopos(ebody2, NULL, EDGE, &nedge, &eedges2);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Nodes from the Body 2 */
  status = EG_getBodyTopos(ebody2, NULL, NODE, &nnode, &enodes2);
  if (status != EGADS_SUCCESS) goto cleanup;


  for (iface = 0; iface < nface; iface++) {

    /* extract the face tessellation */
    status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                            &nt1, &ts1, &tc1);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_getTessFace(tess2, iface+1, &np2, &x2, &uv2, &pt2, &pi2,
                                            &nt2, &ts2, &tc2);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (n = 0; n < np1; n++) {

      /* evaluate original edge and velocities*/
      status = EG_evaluate_dot(efaces1[iface], &uv1[2*n], NULL, p1, p1_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* evaluate perturbed edge */
      status = EG_evaluate(efaces2[iface], &uv2[2*n], p2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* compute the configuration velocity based on finite difference */
      fd_dot[0] = (p2[0] - p1[0])/dtime - p2[3]*(uv2[2*n] - uv1[2*n])/dtime - p2[6]*(uv2[2*n+1] - uv1[2*n+1])/dtime;
      fd_dot[1] = (p2[1] - p1[1])/dtime - p2[4]*(uv2[2*n] - uv1[2*n])/dtime - p2[7]*(uv2[2*n+1] - uv1[2*n+1])/dtime;
      fd_dot[2] = (p2[2] - p1[2])/dtime - p2[5]*(uv2[2*n] - uv1[2*n])/dtime - p2[8]*(uv2[2*n+1] - uv1[2*n+1])/dtime;

      for (d = 0; d < 3; d++) {
        if (fabs(p1_dot[d] - fd_dot[d]) > ftol) {
          printf("%s Face %d iparam=%d, [%d] diff fabs(%+le - %+le) = %+le > %e\n",
                 shape, iface+1, iparam, d, p1_dot[d], fd_dot[d], fabs(p1_dot[d] - fd_dot[d]), ftol);
          nerr++;
        }
      }

      //printf("p1_dot = (%+f, %+f, %+f)\n", p1_dot[0], p1_dot[1], p1_dot[2]);
      //printf("fd_dot = (%+f, %+f, %+f)\n", fd_dot[0], fd_dot[1], fd_dot[2]);
      //printf("\n");
    }
  }

  for (iedge = 0; iedge < nedge; iedge++) {

    status = EG_getInfo(eedges1[iedge], &oclass, &mtype, &top, &prev, &next);
    if (status != EGADS_SUCCESS) goto cleanup;
    if (mtype == DEGENERATE) continue;

    /* extract the tessellation from the original edge */
    status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the tessellation from the perturbed edge */
    status = EG_getTessEdge(tess2, iedge+1, &np2, &x2, &t2);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (n = 0; n < np1; n++) {

      /* evaluate original edge and velocities*/
      status = EG_evaluate_dot(eedges1[iedge], &t1[n], NULL, p1, p1_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* evaluate perturbed edge */
      status = EG_evaluate(eedges2[iedge], &t2[n], p2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* compute the configuration velocity based on finite difference */
      fd_dot[0] = (p2[0] - p1[0])/dtime - p2[3]*(t2[n] - t1[n])/dtime;
      fd_dot[1] = (p2[1] - p1[1])/dtime - p2[4]*(t2[n] - t1[n])/dtime;
      fd_dot[2] = (p2[2] - p1[2])/dtime - p2[5]*(t2[n] - t1[n])/dtime;

      for (d = 0; d < 3; d++) {
        if (fabs(p1_dot[d] - fd_dot[d]) > etol) {
          printf("%s Edge %d iparam=%d, [%d] diff fabs(%+le - %+le) = %+le > %e\n",
                 shape, iedge+1, iparam, d, p1_dot[d], fd_dot[d], fabs(p1_dot[d] - fd_dot[d]), etol);
          nerr++;
        }
      }

      //printf("p1_dot = (%+f, %+f, %+f)\n", p1_dot[0], p1_dot[1], p1_dot[2]);
      //printf("fd_dot = (%+f, %+f, %+f)\n", fd_dot[0], fd_dot[1], fd_dot[2]);
      //printf("\n");
    }
  }

  for (inode = 0; inode < nnode; inode++) {

    /* evaluate original node and velocities*/
    status = EG_evaluate_dot(enodes1[inode], NULL, NULL, p1, p1_dot);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* evaluate perturbed edge */
    status = EG_evaluate(enodes2[inode], NULL, p2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* compute the configuration velocity based on finite difference */
    fd_dot[0] = (p2[0] - p1[0])/dtime;
    fd_dot[1] = (p2[1] - p1[1])/dtime;
    fd_dot[2] = (p2[2] - p1[2])/dtime;

    for (d = 0; d < 3; d++) {
      if (fabs(p1_dot[d] - fd_dot[d]) > etol) {
        printf("%s Node %d iparam=%d, [%d] diff fabs(%+le - %+le) = %+le > %e\n",
               shape, inode+1, iparam, d, p1_dot[d], fd_dot[d], fabs(p1_dot[d] - fd_dot[d]), etol);
        nerr++;
      }
    }

    //printf("p1_dot = (%+f, %+f, %+f)\n", p1_dot[0], p1_dot[1], p1_dot[2]);
    //printf("fd_dot = (%+f, %+f, %+f)\n", fd_dot[0], fd_dot[1], fd_dot[2]);
    //printf("\n");
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  EG_free(efaces1);
  EG_free(eedges1);
  EG_free(enodes1);

  EG_free(efaces2);
  EG_free(eedges2);
  EG_free(enodes2);

  return status + nerr;
}


/*****************************************************************************/
/*                                                                           */
/*  equivDotVels                                                             */
/*                                                                           */
/*****************************************************************************/

int velocityOfNode( void* usrData, const ego *sections, int i, ego node, ego edge,
                    double *xyz, double *xyz_dot )
{
  int status = EGADS_SUCCESS;

  /* evaluate the point and sensitivity */
  status = EG_evaluate_dot(node, NULL, NULL, xyz, xyz_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }

  return status;
}

int velocityOfEdge( void* usrData, const ego *sections, int i, ego edge,
                    const int jmax, const double *ts, const double *ts_dot,
                    double *xyzs, double *xyzs_dot,
                    double *tbeg, double *tbeg_dot,
                    double *tend, double *tend_dot )
{
  int status = EGADS_SUCCESS;
  int j;
  double x[18], x_dot[18];

  for (j = 0; j < jmax; j++) {
    /* evaluate the points and sensitivity */
    status = EG_evaluate_dot(edge, &ts[j], &ts_dot[j], x, x_dot);
    if (status != EGADS_SUCCESS) goto cleanup;

    xyzs[3*j+0] = x[0];
    xyzs[3*j+1] = x[1];
    xyzs[3*j+2] = x[2];

    xyzs_dot[3*j+0] = x_dot[0];
    xyzs_dot[3*j+1] = x_dot[1];
    xyzs_dot[3*j+2] = x_dot[2];

    /* set the sensitivity of the tangent at the beginning and end */
    if (j == 0) {
      tbeg[0] = x[3];
      tbeg[1] = x[4];
      tbeg[2] = x[5];

      tbeg_dot[0] = x_dot[3];
      tbeg_dot[1] = x_dot[4];
      tbeg_dot[2] = x_dot[5];
    }

    if (j == jmax-1) {
      tend[0] = x[3];
      tend[1] = x[4];
      tend[2] = x[5];

      tend_dot[0] = x_dot[3];
      tend_dot[1] = x_dot[4];
      tend_dot[2] = x_dot[5];
    }
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }

  return status;
}

int
equivDotVels(ego tess1, ego tess2, int iparam, const char *shape, double ftol, double etol, double ntol)
{
  int    status = EGADS_SUCCESS;
  int    n, d, np1, np2, nt1, nt2, nerr=0;
  int    nface, nedge, nnode, iface, iedge, inode, oclass, mtype;
  double p1[18], p1_dot[18], p2[18], p2_dot[18];
  const int    *pt1, *pi1, *pt2, *pi2, *ts1, *tc1, *ts2, *tc2;
  const double *t1, *t2, *x1, *x2, *uv1, *uv2;
  ego    ebody1, ebody2;
  ego    *efaces1=NULL, *efaces2=NULL, *eedges1=NULL, *eedges2=NULL, *enodes1=NULL, *enodes2=NULL;
  ego    top, prev, next;

  status = EG_statusTessBody( tess1, &ebody1, &np1, &np2 );
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_statusTessBody( tess2, &ebody2, &np1, &np2 );
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Faces from the Body 1 */
  status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, &efaces1);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Edges from the Body 1 */
  status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, &eedges1);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Nodes from the Body 1 */
  status = EG_getBodyTopos(ebody1, NULL, NODE, &nnode, &enodes1);
  if (status != EGADS_SUCCESS) goto cleanup;


  /* get the Faces from the Body 2 */
  status = EG_getBodyTopos(ebody2, NULL, FACE, &nface, &efaces2);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Edges from the Body 2 */
  status = EG_getBodyTopos(ebody2, NULL, EDGE, &nedge, &eedges2);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Nodes from the Body 2 */
  status = EG_getBodyTopos(ebody2, NULL, NODE, &nnode, &enodes2);
  if (status != EGADS_SUCCESS) goto cleanup;


  for (iface = 0; iface < nface; iface++) {

    /* extract the face tessellation */
    status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                            &nt1, &ts1, &tc1);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_getTessFace(tess2, iface+1, &np2, &x2, &uv2, &pt2, &pi2,
                                            &nt2, &ts2, &tc2);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (n = 0; n < np1; n++) {

      /* evaluate face velocities with _dot functions */
      status = EG_evaluate_dot(efaces1[iface], &uv1[2*n], NULL, p1, p1_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* evaluate face velocities with _vels functions */
      status = EG_evaluate_dot(efaces2[iface], &uv2[2*n], NULL, p2, p2_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      for (d = 0; d < 3; d++) {
        if (fabs(p1_dot[d] - p2_dot[d]) > ftol) {
          printf("%s Face %d iparam=%d, [%d] diff fabs(%+le - %+le) = %+le > %e\n",
                 shape, iface+1, iparam, d, p1_dot[d], p2_dot[d], fabs(p1_dot[d] - p2_dot[d]), ftol);
          nerr++;
        }
      }

      //printf("p1_dot = (%+f, %+f, %+f)\n", p1_dot[0], p1_dot[1], p1_dot[2]);
      //printf("p2_dot = (%+f, %+f, %+f)\n", p2_dot[0], p2_dot[1], p2_dot[2]);
      //printf("\n");
    }
  }

  for (iedge = 0; iedge < nedge; iedge++) {

    status = EG_getInfo(eedges1[iedge], &oclass, &mtype, &top, &prev, &next);
    if (status != EGADS_SUCCESS) goto cleanup;
    if (mtype == DEGENERATE) continue;

    /* extract the tessellation from the original edge */
    status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the tessellation from the perturbed edge */
    status = EG_getTessEdge(tess2, iedge+1, &np2, &x2, &t2);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (n = 0; n < np1; n++) {

      /* evaluate edge velocities with _dot functions */
      status = EG_evaluate_dot(eedges1[iedge], &t1[n], NULL, p1, p1_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* evaluate face velocities with _vels functions */
      status = EG_evaluate_dot(eedges2[iedge], &t2[n], NULL, p2, p2_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      for (d = 0; d < 3; d++) {
        if (fabs(p1_dot[d] - p2_dot[d]) > etol) {
          printf("%s Edge %d iparam=%d, [%d] diff fabs(%+le - %+le) = %+le > %e\n",
                 shape, iedge+1, iparam, d, p1_dot[d], p2_dot[d], fabs(p1_dot[d] - p2_dot[d]), etol);
          nerr++;
        }
      }

      //printf("p1_dot = (%+f, %+f, %+f)\n", p1_dot[0], p1_dot[1], p1_dot[2]);
      //printf("p2_dot = (%+f, %+f, %+f)\n", p2_dot[0], p2_dot[1], p2_dot[2]);
      //printf("\n");
    }
  }

  for (inode = 0; inode < nnode; inode++) {

    /* evaluate node velocities with _dot functions */
    status = EG_evaluate_dot(enodes1[inode], NULL, NULL, p1, p1_dot);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* evaluate node velocities with _vels functions */
    status = EG_evaluate_dot(enodes2[inode], NULL, NULL, p2, p2_dot);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (d = 0; d < 3; d++) {
      if (fabs(p1_dot[d] - p2_dot[d]) > etol) {
        printf("%s Node %d iparam=%d, [%d] diff fabs(%+le - %+le) = %+le > %e\n",
               shape, inode+1, iparam, d, p1_dot[d], p2_dot[d], fabs(p1_dot[d] - p2_dot[d]), etol);
        nerr++;
      }
    }

    //printf("p1_dot = (%+f, %+f, %+f)\n", p1_dot[0], p1_dot[1], p1_dot[2]);
    //printf("p2_dot = (%+f, %+f, %+f)\n", p2_dot[0], p2_dot[1], p2_dot[2]);
    //printf("\n");
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  EG_free(efaces1);
  EG_free(eedges1);
  EG_free(enodes1);

  EG_free(efaces2);
  EG_free(eedges2);
  EG_free(enodes2);

  return status + nerr;
}


/*****************************************************************************/
/*                                                                           */
/*  Transform                                                                */
/*                                                                           */
/*****************************************************************************/

int
makeTransform( ego eobj,       /* (in) the object to be transformed */
               double *xforms, /* (in) sequence of transformations  */
               ego *result )   /* (out) transformed object          */
{
  int    status = EGADS_SUCCESS;
  double scale, offset[3];
  ego    context, exform;

  double mat[12] = {1.00, 0.00, 0.00, 0.0,
                    0.00, 1.00, 0.00, 0.0,
                    0.00, 0.00, 1.00, 0.0};

  status = EG_getContext(eobj, &context);
  if (status != EGADS_SUCCESS) goto cleanup;

  scale     = xforms[0];
  offset[0] = xforms[1];
  offset[1] = xforms[2];
  offset[2] = xforms[3];

  mat[ 0] = scale; mat[ 1] =    0.; mat[ 2] =    0.; mat[ 3] = offset[0];
  mat[ 4] =    0.; mat[ 5] = scale; mat[ 6] =    0.; mat[ 7] = offset[1];
  mat[ 8] =    0.; mat[ 9] =    0.; mat[10] = scale; mat[11] = offset[2];

  status = EG_makeTransform(context, mat, &exform); if (status != EGADS_SUCCESS) goto cleanup;
  status = EG_copyObject(eobj, exform, result);     if (status != EGADS_SUCCESS) goto cleanup;
  status = EG_deleteObject(exform);                 if (status != EGADS_SUCCESS) goto cleanup;

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
setTransform_dot(ego eobj,           /* (in) the object to be transformed           */
                 double *xforms,     /* (in) sequence of transformations            */
                 double *xforms_dot, /* (in) velocity of transformations            */
                 ego result)         /* (in/out) transformed object with velocities */
{
  int status = EGADS_SUCCESS;
  double scale, scale_dot, offset[3], offset_dot[3];

  double mat[12] = {1.00, 0.00, 0.00, 0.0,
                    0.00, 1.00, 0.00, 0.0,
                    0.00, 0.00, 1.00, 0.0};
  double mat_dot[12];

  scale     = xforms[0];
  offset[0] = xforms[1];
  offset[1] = xforms[2];
  offset[2] = xforms[3];

  scale_dot     = xforms_dot[0];
  offset_dot[0] = xforms_dot[1];
  offset_dot[1] = xforms_dot[2];
  offset_dot[2] = xforms_dot[3];

  mat[ 0] = scale; mat[ 1] =    0.; mat[ 2] =    0.; mat[ 3] = offset[0];
  mat[ 4] =    0.; mat[ 5] = scale; mat[ 6] =    0.; mat[ 7] = offset[1];
  mat[ 8] =    0.; mat[ 9] =    0.; mat[10] = scale; mat[11] = offset[2];

  mat_dot[ 0] = scale_dot; mat_dot[ 1] =        0.; mat_dot[ 2] =        0.; mat_dot[ 3] = offset_dot[0];
  mat_dot[ 4] =        0.; mat_dot[ 5] = scale_dot; mat_dot[ 6] =        0.; mat_dot[ 7] = offset_dot[1];
  mat_dot[ 8] =        0.; mat_dot[ 9] =        0.; mat_dot[10] = scale_dot; mat_dot[11] = offset_dot[2];

  status = EG_copyGeometry_dot(eobj, mat, mat_dot, result);
  if (status != EGADS_SUCCESS) goto cleanup;

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}

/*****************************************************************************/
/*                                                                           */
/*  Line                                                                     */
/*                                                                           */
/*****************************************************************************/

int
makeLineLoop( ego context,      /* (in)  EGADS context                   */
              const double *x0, /* (in)  coordinates of the first point  */
              const double *x1, /* (in)  coordinates of the second point */
              ego *eloop )      /* (out) Line loop created from points   */
{
  int    status = EGADS_SUCCESS;
  int    senses[1] = {SFORWARD};
  double data[6], tdata[2];
  ego    eline, eedge, enodes[2];

  /* create Nodes for the Edge */
  data[0] = x0[0];
  data[1] = x0[1];
  data[2] = x0[2];
  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[0]);
  if (status != EGADS_SUCCESS) goto cleanup;

  data[0] = x1[0];
  data[1] = x1[1];
  data[2] = x1[2];
  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[1]);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* create the Line (point and direction) */
  data[0] = x0[0];
  data[1] = x0[1];
  data[2] = x0[2];
  data[3] = x1[0] - x0[0];
  data[4] = x1[1] - x0[1];
  data[5] = x1[2] - x0[2];

  status = EG_makeGeometry(context, CURVE, LINE, NULL, NULL,
                           data, &eline);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* make the Edge on the Line */
  tdata[0] = 0;
  tdata[1] = sqrt(data[3]*data[3] + data[4]*data[4] + data[5]*data[5]);

  status = EG_makeTopology(context, eline, EDGE, TWONODE,
                           tdata, 2, enodes, NULL, &eedge);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_makeTopology(context, NULL, LOOP, OPEN,
                           NULL, 1, &eedge, senses, eloop);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EGADS_SUCCESS;

cleanup:
  return status;
}


int
setLineLoop_dot( const double *x0,     /* (in)  coordinates of the first point  */
                 const double *x0_dot, /* (in)  velocity of the first point     */
                 const double *x1,     /* (in)  coordinates of the second point */
                 const double *x1_dot, /* (in)  velocity of the second point    */
                 ego eloop )           /* (in/out) Line loop with velocities    */
{
  int    status = EGADS_SUCCESS;
  int    nnode, nedge, oclass, mtype, *senses;
  double data[6], data_dot[6];
  ego    eline, *enodes, *eedges, eref;

  /* get the Edge from the Loop */
  status = EG_getTopology(eloop, &eref, &oclass, &mtype,
                          data, &nedge, &eedges, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Nodes and the Line from the Edge */
  status = EG_getTopology(eedges[0], &eline, &oclass, &mtype,
                          data, &nnode, &enodes, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* set the sensitivity of the Nodes */
  status = EG_setGeometry_dot(enodes[0], NODE, 0, NULL, x0, x0_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_setGeometry_dot(enodes[1], NODE, 0, NULL, x1, x1_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* Compute the Line data and velocity */
  data[0] = x0[0];
  data[1] = x0[1];
  data[2] = x0[2];
  data[3] = x1[0] - x0[0];
  data[4] = x1[1] - x0[1];
  data[5] = x1[2] - x0[2];

  data_dot[0] = x0_dot[0];
  data_dot[1] = x0_dot[1];
  data_dot[2] = x0_dot[2];
  data_dot[3] = x1_dot[0] - x0_dot[0];
  data_dot[4] = x1_dot[1] - x0_dot[1];
  data_dot[5] = x1_dot[2] - x0_dot[2];

  status = EG_setGeometry_dot(eline, CURVE, LINE, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EGADS_SUCCESS;

cleanup:
  return status;
}


int
pingLineRuled(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[6], x_dot[6], *p1, *p2, *p1_dot, *p2_dot, params[3], dtime = 1e-7;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2;

  p1 = x;
  p2 = x+3;

  p1_dot = x_dot;
  p2_dot = x_dot+3;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 0.75;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    p1[0] = 0.00; p1[1] = 0.00; p1[2] = 0.00;
    p2[0] = 1.00; p2[1] = 0.00; p2[2] = 0.00;
    status = makeLineLoop(context, p1, p2, &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_ruled(nsec, secs1, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.01;
    params[2] = 12.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Line Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Line Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 6; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 6; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setLineLoop_dot(p1, p1_dot, p2, p2_dot, secs1[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled_dot(ebody1, nsec, secs1);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = makeLineLoop(context, p1, p2, &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled(nsec, secs2, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Ruled Line", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
pingLineBlend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[6], x_dot[6], *p1, *p2, *p1_dot, *p2_dot, params[3], dtime = 1e-7;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2;

  p1 = x;
  p2 = x+3;

  p1_dot = x_dot;
  p2_dot = x_dot+3;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    p1[0] = 0.00; p1[1] = 0.00; p1[2] = 0.00;
    p2[0] = 1.00; p2[1] = 0.00; p2[2] = 0.00;
    status = makeLineLoop(context, p1, p2, &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_blend(nsec, secs1, NULL, NULL, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.01;
    params[2] = 12.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Blend Line Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Blend Line Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 6; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 6; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setLineLoop_dot(p1, p1_dot, p2, p2_dot, secs1[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend_dot(ebody1, nsec, secs1, NULL, NULL, NULL, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = makeLineLoop(context, p1, p2, &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend(nsec, secs2, NULL, NULL, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Blend Line", 5e-6, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
equivLineRuled(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[6], x_dot[6], *p1, *p2, *p1_dot, *p2_dot, params[3];
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs[3], ebody1, ebody2, tess1, tess2;

  egadsSplineVels vels;
  vels.usrData = NULL;
  vels.velocityOfNode = &velocityOfNode;
  vels.velocityOfEdge = &velocityOfEdge;

  p1 = x;
  p2 = x+3;

  p1_dot = x_dot;
  p2_dot = x_dot+3;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 0.75;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the body for _dot sensitivities */
    p1[0] = 0.00; p1[1] = 0.00; p1[2] = 0.00;
    p2[0] = 1.00; p2[1] = 0.00; p2[2] = 0.00;
    status = makeLineLoop(context, p1, p2, &secs[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs[0], xform1, &secs[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs[0], xform2, &secs[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_ruled(nsec, secs, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;


    /* make the body for _vels sensitivities */
    status = EG_ruled(nsec, secs, &ebody2);
    if (status != EGADS_SUCCESS) goto cleanup;


    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.01;
    params[2] = 12.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* map the tessellation */
    status = EG_mapTessBody(tess1, ebody2, &tess2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Ruled Line Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Ruled Line Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 6; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 6; iparam++) {

      /* set the velocity with _dot functions */
      x_dot[iparam] = 1.0;
      status = setLineLoop_dot(p1, p1_dot, p2, p2_dot, secs[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs[0], xform1, xform_dot, secs[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs[0], xform2, xform_dot, secs[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled_dot(ebody1, nsec, secs);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* set the velocity with _vels functions */
      status = EG_ruled_vels(nsec, secs, &vels, ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* ping the bodies */
      status = equivDotVels(tess1, tess2, iparam, "Equiv Ruled Line", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

    }
    EG_deleteObject(tess2);
    EG_deleteObject(ebody2);

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
equivLineBlend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[6], x_dot[6], *p1, *p2, *p1_dot, *p2_dot, params[3];
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs[3], ebody1, ebody2, tess1, tess2;

  egadsSplineVels vels;

  vels.usrData = NULL;
  vels.velocityOfNode = &velocityOfNode;
  vels.velocityOfEdge = &velocityOfEdge;

  p1 = x;
  p2 = x+3;

  p1_dot = x_dot;
  p2_dot = x_dot+3;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 0.75;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the body for _dot sensitivities */
    p1[0] = 0.00; p1[1] = 0.00; p1[2] = 0.00;
    p2[0] = 1.00; p2[1] = 0.00; p2[2] = 0.00;
    status = makeLineLoop(context, p1, p2, &secs[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs[0], xform1, &secs[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs[0], xform2, &secs[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_blend(nsec, secs, NULL, NULL, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;


    /* make the body for _vels sensitivities */
    status = EG_blend(nsec, secs, NULL, NULL, &ebody2);
    if (status != EGADS_SUCCESS) goto cleanup;


    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.01;
    params[2] = 12.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* map the tessellation */
    status = EG_mapTessBody(tess1, ebody2, &tess2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Blend Line Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Blend Line Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 6; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 6; iparam++) {

      /* set the velocity with _dot functions */
      x_dot[iparam] = 1.0;
      status = setLineLoop_dot(p1, p1_dot, p2, p2_dot, secs[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs[0], xform1, xform_dot, secs[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs[0], xform2, xform_dot, secs[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend_dot(ebody1, nsec, secs, NULL, NULL, NULL, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* set the velocity with _vels functions */
      status = EG_blend_vels(nsec, secs, NULL, NULL, NULL, NULL, &vels, ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* check sensitivity equivalence */
      status = equivDotVels(tess1, tess2, iparam, "Equiv Blend Line", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

    }
    EG_deleteObject(tess2);
    EG_deleteObject(ebody2);

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


/*****************************************************************************/
/*                                                                           */
/*  Line2                                                                    */
/*                                                                           */
/*****************************************************************************/

int
makeLine2Loop( ego context,      /* (in)  EGADS context                   */
               const double *x0, /* (in)  the point                       */
               const double *v0, /* (in)  the vector                      */
               double *ts,       /* (in)  t values for end nodes          */
               ego *eloop )      /* (out) Line loop created from points   */
{
  int    status = EGADS_SUCCESS;
  int    senses[1] = {SFORWARD};
  double data[18];
  ego    eline, eedge, enodes[2];

  /* create the Line (point and direction) */
  data[0] = x0[0];
  data[1] = x0[1];
  data[2] = x0[2];
  data[3] = v0[0];
  data[4] = v0[1];
  data[5] = v0[2];

  status = EG_makeGeometry(context, CURVE, LINE, NULL, NULL,
                           data, &eline);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* create Nodes for the Edge */
  status = EG_evaluate(eline, &ts[0], data);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[0]);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_evaluate(eline, &ts[1], data);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[1]);
  if (status != EGADS_SUCCESS) goto cleanup;


  /* make the Edge on the Line */
  status = EG_makeTopology(context, eline, EDGE, TWONODE,
                           ts, 2, enodes, NULL, &eedge);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_makeTopology(context, NULL, LOOP, OPEN,
                           NULL, 1, &eedge, senses, eloop);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EGADS_SUCCESS;

cleanup:
  return status;
}


int
setLine2Loop_dot( const double *x0,     /* (in)  the point                       */
                  const double *x0_dot, /* (in)  velocity of the point           */
                  const double *v0,     /* (in)  the vector                      */
                  const double *v0_dot, /* (in)  velocity of the vector          */
                  const double *ts,     /* (in)  t values for end nodes          */
                  const double *ts_dot, /* (in)  velocity of t-values            */
                  ego eloop )           /* (in/out) Line loop with velocities    */
{
  int    status = EGADS_SUCCESS;
  int    nnode, nedge, oclass, mtype, *senses;
  double data[18], data_dot[18];
  ego    eline, *enodes, *eedges, eref;

  /* get the Edge from the Loop */
  status = EG_getTopology(eloop, &eref, &oclass, &mtype,
                          data, &nedge, &eedges, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Nodes and the Line from the Edge */
  status = EG_getTopology(eedges[0], &eline, &oclass, &mtype,
                          data, &nnode, &enodes, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;


  /* Compute the Line data and velocity */
  data[0] = x0[0];
  data[1] = x0[1];
  data[2] = x0[2];
  data[3] = v0[0];
  data[4] = v0[1];
  data[5] = v0[2];

  data_dot[0] = x0_dot[0];
  data_dot[1] = x0_dot[1];
  data_dot[2] = x0_dot[2];
  data_dot[3] = v0_dot[0];
  data_dot[4] = v0_dot[1];
  data_dot[5] = v0_dot[2];

  status = EG_setGeometry_dot(eline, CURVE, LINE, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;


  /* set the sensitivity of the Nodes */
  status = EG_evaluate_dot(eline, &ts[0], &ts_dot[0], data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_setGeometry_dot(enodes[0], NODE, 0, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_evaluate_dot(eline, &ts[1], &ts_dot[1], data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_setGeometry_dot(enodes[1], NODE, 0, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EGADS_SUCCESS;

cleanup:
  return status;
}


int
pingLine2Ruled(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[8], x_dot[8], params[3], dtime = 1e-7;
  double *x0, *v0, *x0_dot, *v0_dot, *ts, *ts_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2;

  x0 = x;
  v0 = x+3;
  ts = x+6;

  x0_dot = x_dot;
  v0_dot = x_dot+3;
  ts_dot = x_dot+6;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    x0[0] = 0.00; x0[1] = 0.00; x0[2] = 0.00;
    v0[0] = 1.00; v0[1] = 0.00; v0[2] = 0.00;
    ts[0] = -1; ts[1] = 1;
    status = makeLine2Loop(context, x0, v0, ts, &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_ruled(nsec, secs1, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.01;
    params[2] = 12.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Line2 Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Line2 Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 8; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 8; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setLine2Loop_dot(x0, x0_dot, v0, v0_dot, ts, ts_dot, secs1[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled_dot(ebody1, nsec, secs1);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = makeLine2Loop(context, x0, v0, ts, &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled(nsec, secs2, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Ruled Line2", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
pingLine2Blend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[8], x_dot[8], params[3], dtime = 1e-7;
  double *x0, *v0, *x0_dot, *v0_dot, *ts, *ts_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2;

  x0 = x;
  v0 = x+3;
  ts = x+6;

  x0_dot = x_dot;
  v0_dot = x_dot+3;
  ts_dot = x_dot+6;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    x0[0] = 0.00; x0[1] = 0.00; x0[2] = 0.00;
    v0[0] = 1.00; v0[1] = 0.00; v0[2] = 0.00;
    ts[0] = -1; ts[1] = 1;
    status = makeLine2Loop(context, x0, v0, ts, &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_blend(nsec, secs1, NULL, NULL, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.01;
    params[2] = 12.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Line2 Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Line2 Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 8; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 8; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setLine2Loop_dot(x0, x0_dot, v0, v0_dot, ts, ts_dot, secs1[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend_dot(ebody1, nsec, secs1, NULL, NULL, NULL, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = makeLine2Loop(context, x0, v0, ts, &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend(nsec, secs2, NULL, NULL, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Ruled Line2", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


/*****************************************************************************/
/*                                                                           */
/*  Circle                                                                   */
/*                                                                           */
/*****************************************************************************/

int
makeCircle( ego context,         /* (in)  EGADS context        */
            const int btype,     /* (in)  WIREBODY or FACEBODY */
            const double *xcent, /* (in)  Center               */
            const double *xax,   /* (in)  x-axis               */
            const double *yax,   /* (in)  y-axis               */
            const double r,      /* (in)  radius               */
            ego *eobj )          /* (out) Circle Face/Loop     */
{
  int    status = EGADS_SUCCESS;
  int    senses[1] = {SFORWARD}, oclass, mtype, *ivec=NULL;
  double data[10], tdata[2], dx[3], dy[3], *rvec=NULL;
  ego    ecircle, eedge, enode, eloop, eplane, eface, eref;

  /* create the Circle */
  data[0] = xcent[0]; /* center */
  data[1] = xcent[1];
  data[2] = xcent[2];
  data[3] = xax[0];   /* x-axis */
  data[4] = xax[1];
  data[5] = xax[2];
  data[6] = yax[0];  /* y-axis */
  data[7] = yax[1];
  data[8] = yax[2];
  data[9] = r;        /* radius */
  status = EG_makeGeometry(context, CURVE, CIRCLE, NULL, NULL,
                           data, &ecircle);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_getGeometry(ecircle, &oclass, &mtype, &eref, &ivec, &rvec);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the axes for the circle */
  dx[0] = rvec[3];
  dx[1] = rvec[4];
  dx[2] = rvec[5];
  dy[0] = rvec[6];
  dy[1] = rvec[7];
  dy[2] = rvec[8];

  /* create the Node for the Edge */
  data[0] = xcent[0] + dx[0]*r;
  data[1] = xcent[1] + dx[1]*r;
  data[2] = xcent[2] + dx[2]*r;
  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enode);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* make the Edge on the Circle */
  tdata[0] = 0;
  tdata[1] = TWOPI;

  status = EG_makeTopology(context, ecircle, EDGE, ONENODE,
                           tdata, 1, &enode, NULL, &eedge);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_makeTopology(context, NULL, LOOP, CLOSED,
                           NULL, 1, &eedge, senses, &eloop);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (btype == LOOP) {

    *eobj = eloop;

  } else {

    /* create the Plane */
    data[0] = xcent[0]; /* center */
    data[1] = xcent[1];
    data[2] = xcent[2];
    data[3] = dx[0];   /* x-axis */
    data[4] = dx[1];
    data[5] = dx[2];
    data[6] = dy[0];   /* y-axis */
    data[7] = dy[1];
    data[8] = dy[2];
    status = EG_makeGeometry(context, SURFACE, PLANE, NULL, NULL,
                             data, &eplane);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_makeTopology(context, eplane, FACE, SFORWARD,
                             NULL, 1, &eloop, senses, &eface);
    if (status != EGADS_SUCCESS) goto cleanup;

    *eobj = eface;
  }

  status = EGADS_SUCCESS;

cleanup:
  EG_free(ivec); ivec = NULL;
  EG_free(rvec); rvec = NULL;

  return status;
}


int
setCircle_dot( const double *xcent,     /* (in)  Center          */
               const double *xcent_dot, /* (in)  Center velocity */
               const double *xax,       /* (in)  x-axis          */
               const double *xax_dot,   /* (in)  x-axis velocity */
               const double *yax,       /* (in)  y-axis          */
               const double *yax_dot,   /* (in)  y-axis velocity */
               const double r,          /* (in)  radius          */
               const double r_dot,      /* (in)  radius velocity */
               ego eobj )               /* (in/out) Circle with velocities */
{
  int    status = EGADS_SUCCESS;
  int    nnode, nedge, nloop, oclass, mtype, *senses, btype;
  double data[10], data_dot[10], dx[3], dx_dot[3], dy[3], dy_dot[3];
  double *rvec=NULL, *rvec_dot=NULL;
  ego    ecircle, eplane, *enodes, *eloops, *eedges, eref;

  /* get the type */
  status = EG_getTopology(eobj, &eplane, &oclass, &mtype,
                          data, &nloop, &eloops, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (oclass == LOOP) {
    nloop = 1;
    eloops = &eobj;
    btype = LOOP;
  } else {
    btype = FACE;
  }

  /* get the Edge from the Loop */
  status = EG_getTopology(eloops[0], &eref, &oclass, &mtype,
                          data, &nedge, &eedges, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Node and the Circle from the Edge */
  status = EG_getTopology(eedges[0], &ecircle, &oclass, &mtype,
                          data, &nnode, &enodes, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* set the Circle data and velocity */
  data[0] = xcent[0]; /* center */
  data[1] = xcent[1];
  data[2] = xcent[2];
  data[3] = xax[0];   /* x-axis */
  data[4] = xax[1];
  data[5] = xax[2];
  data[6] = yax[0];   /* y-axis */
  data[7] = yax[1];
  data[8] = yax[2];
  data[9] = r;        /* radius */

  data_dot[0] = xcent_dot[0]; /* center */
  data_dot[1] = xcent_dot[1];
  data_dot[2] = xcent_dot[2];
  data_dot[3] = xax_dot[0];   /* x-axis */
  data_dot[4] = xax_dot[1];
  data_dot[5] = xax_dot[2];
  data_dot[6] = yax_dot[0];   /* y-axis */
  data_dot[7] = yax_dot[1];
  data_dot[8] = yax_dot[2];
  data_dot[9] = r_dot;        /* radius */

  status = EG_setGeometry_dot(ecircle, CURVE, CIRCLE, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_getGeometry_dot(ecircle, &rvec, &rvec_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the axes for the circle */
  dx[0] = rvec[3];
  dx[1] = rvec[4];
  dx[2] = rvec[5];
  dy[0] = rvec[6];
  dy[1] = rvec[7];
  dy[2] = rvec[8];
  dx_dot[0] = rvec_dot[3];
  dx_dot[1] = rvec_dot[4];
  dx_dot[2] = rvec_dot[5];
  dy_dot[0] = rvec_dot[6];
  dy_dot[1] = rvec_dot[7];
  dy_dot[2] = rvec_dot[8];

  /* set the sensitivity of the Node */
  data[0] = xcent[0] + dx[0]*r;
  data[1] = xcent[1] + dx[1]*r;
  data[2] = xcent[2] + dx[2]*r;
  data_dot[0] = xcent_dot[0] + dx_dot[0]*r + dx[0]*r_dot;
  data_dot[1] = xcent_dot[1] + dx_dot[1]*r + dx[1]*r_dot;
  data_dot[2] = xcent_dot[2] + dx_dot[2]*r + dx[2]*r_dot;
  status = EG_setGeometry_dot(enodes[0], NODE, 0, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (btype == FACE) {
    data[0] = xcent[0]; /* center */
    data[1] = xcent[1];
    data[2] = xcent[2];
    data[3] = dx[0];    /* x-axis */
    data[4] = dx[1];
    data[5] = dx[2];
    data[6] = dy[0];    /* y-axis */
    data[7] = dy[1];
    data[8] = dy[2];

    data_dot[0] = xcent_dot[0]; /* center */
    data_dot[1] = xcent_dot[1];
    data_dot[2] = xcent_dot[2];
    data_dot[3] = dx_dot[0];    /* x-axis */
    data_dot[4] = dx_dot[1];
    data_dot[5] = dx_dot[2];
    data_dot[6] = dy_dot[0];    /* y-axis */
    data_dot[7] = dy_dot[1];
    data_dot[8] = dy_dot[2];

    status = EG_setGeometry_dot(eplane, SURFACE, PLANE, NULL, data, data_dot);
    if (status != EGADS_SUCCESS) goto cleanup;
  }

  status = EGADS_SUCCESS;

cleanup:
  EG_free(rvec); rvec = NULL;
  EG_free(rvec_dot); rvec_dot = NULL;

  return status;
}


int
pingCircleRuled(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[10], x_dot[10], params[3], dtime = 1e-7;
  double *xcent, *xax, *yax, *xcent_dot, *xax_dot, *yax_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2, eloop1, eloop2;

  xcent = x;
  xax   = x+3;
  yax   = x+6;

  xcent_dot = x_dot;
  xax_dot   = x_dot+3;
  yax_dot   = x_dot+6;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    xcent[0] = 0.0; xcent[1] = 0.0; xcent[2] = 0.0;
    xax[0]   = 1.0; xax[1]   = 0.0; xax[2]   = 0.0;
    yax[0]   = 0.0; yax[1]   = 1.0; yax[2]   = 0.0;
    x[9] = 1.0;
    status = makeCircle(context, FACE, xcent, xax, yax, x[9], &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop1);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_ruled(nsec, secs1, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.2;
    params[2] = 20.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Circle Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled Circle Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 10; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 10; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], secs1[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], eloop1);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled_dot(ebody1, nsec, secs1);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = makeCircle(context, FACE, xcent, xax, yax, x[9], &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop2);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled(nsec, secs2, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Ruled Circle", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(eloop2);
      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(eloop1);
    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
pingCircleBlend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[10], x_dot[10], params[3], dtime = 1e-7;
  double *xcent, *xax, *yax, *xcent_dot, *xax_dot, *yax_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2, eloop1, eloop2;

  xcent = x;
  xax   = x+3;
  yax   = x+6;

  xcent_dot = x_dot;
  xax_dot   = x_dot+3;
  yax_dot   = x_dot+6;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    xcent[0] = 0.0; xcent[1] = 0.0; xcent[2] = 0.0;
    xax[0]   = 1.0; xax[1]   = 0.0; xax[2]   = 0.0;
    yax[0]   = 0.0; yax[1]   = 1.0; yax[2]   = 0.0;
    x[9] = 1.0;
    status = makeCircle(context, FACE, xcent, xax, yax, x[9], &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop1);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_blend(nsec, secs1, NULL, NULL, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.2;
    params[2] = 20.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Blend Circle Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Blend Circle Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 10; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 10; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], secs1[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], eloop1);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend_dot(ebody1, nsec, secs1, NULL, NULL, NULL, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = makeCircle(context, FACE, xcent, xax, yax, x[9], &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop2);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend(nsec, secs2, NULL, NULL, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Blend Circle", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(eloop2);
      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(eloop1);
    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
equivCircleRuled(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[10], x_dot[10], params[3];
  double *xcent, *xax, *yax, *xcent_dot, *xax_dot, *yax_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs[3], ebody1, ebody2, tess1, tess2, eloop;

  egadsSplineVels vels;
  vels.usrData = NULL;
  vels.velocityOfNode = &velocityOfNode;
  vels.velocityOfEdge = &velocityOfEdge;

  xcent = x;
  xax   = x+3;
  yax   = x+6;

  xcent_dot = x_dot;
  xax_dot   = x_dot+3;
  yax_dot   = x_dot+6;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 0.75;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the body for _dot sensitivities */
    xcent[0] = 0.0; xcent[1] = 0.0; xcent[2] = 0.0;
    xax[0]   = 1.0; xax[1]   = 0.0; xax[2]   = 0.0;
    yax[0]   = 0.0; yax[1]   = 1.0; yax[2]   = 0.0;
    x[9] = 1.0;
    status = makeCircle(context, FACE, xcent, xax, yax, x[9], &secs[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop, xform1, &secs[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs[0], xform2, &secs[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_ruled(nsec, secs, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;


    /* make the body for _vels sensitivities */
    status = EG_ruled(nsec, secs, &ebody2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.2;
    params[2] = 20.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* map the tessellation */
    status = EG_mapTessBody(tess1, ebody2, &tess2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Ruled Circle Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Ruled Circle Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 10; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 10; iparam++) {

      /* set the velocity with _dot functions */
      x_dot[iparam] = 1.0;
      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], secs[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], eloop);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop, xform1, xform_dot, secs[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs[0], xform2, xform_dot, secs[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled_dot(ebody1, nsec, secs);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* set the velocity with _vels functions */
      status = EG_ruled_vels(nsec, secs, &vels, ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      /* ping the bodies */
      status = equivDotVels(tess1, tess2, iparam, "Equiv Ruled Circle", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;
    }

    EG_deleteObject(eloop);

    EG_deleteObject(tess2);
    EG_deleteObject(ebody2);

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
equivCircleBlend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  double x[10], x_dot[10], params[3];
  double *xcent, *xax, *yax, *xcent_dot, *xax_dot, *yax_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs[3], ebody1, ebody2, tess1, tess2, eloop;

  egadsSplineVels vels;
  vels.usrData = NULL;
  vels.velocityOfNode = &velocityOfNode;
  vels.velocityOfEdge = &velocityOfEdge;

  xcent = x;
  xax   = x+3;
  yax   = x+6;

  xcent_dot = x_dot;
  xax_dot   = x_dot+3;
  yax_dot   = x_dot+6;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 0.75;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the body for _dot sensitivities */
    xcent[0] = 0.0; xcent[1] = 0.0; xcent[2] = 0.0;
    xax[0]   = 1.0; xax[1]   = 0.0; xax[2]   = 0.0;
    yax[0]   = 0.0; yax[1]   = 1.0; yax[2]   = 0.0;
    x[9] = 1.0;
    status = makeCircle(context, FACE, xcent, xax, yax, x[9], &secs[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop, xform1, &secs[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs[0], xform2, &secs[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_blend(nsec, secs, NULL, NULL, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;


    /* make the body for _vels sensitivities */
    status = EG_blend(nsec, secs, NULL, NULL, &ebody2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.2;
    params[2] = 20.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* map the tessellation */
    status = EG_mapTessBody(tess1, ebody2, &tess2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Blend Circle Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Blend Circle Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 10; iparam++) x_dot[iparam] = 0;


    for (iparam = 0; iparam < 10; iparam++) {

      /* set the velocity with _dot functions */
      x_dot[iparam] = 1.0;
      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], secs[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], eloop);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop, xform1, xform_dot, secs[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs[0], xform2, xform_dot, secs[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend_dot(ebody1, nsec, secs, NULL, NULL, NULL, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* set the velocity with _vels functions */
      status = EG_blend_vels(nsec, secs, NULL, NULL, NULL, NULL, &vels, ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;


      /* ping the bodies */
      status = equivDotVels(tess1, tess2, iparam, "Equiv Blend Circle", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;
    }

    EG_deleteObject(eloop);

    EG_deleteObject(tess2);
    EG_deleteObject(ebody2);

    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}

/*****************************************************************************/
/*                                                                           */
/*  Nose treatment                                                           */
/*                                                                           */
/*****************************************************************************/

int
makeCircle2( ego context,         /* (in)  EGADS context        */
             const int btype,     /* (in)  WIREBODY or FACEBODY */
             const double *xcent, /* (in)  Center               */
             const double *xax,   /* (in)  x-axis               */
             const double *yax,   /* (in)  y-axis               */
             const double r,      /* (in)  radius               */
             ego *eobj )          /* (out) Circle Face/Loop     */
{
  int    status = EGADS_SUCCESS;
  int    senses[2] = {SFORWARD,SFORWARD}, oclass, mtype, *ivec=NULL;
  double data[10], tdata[2], dx[3], dy[3], *rvec=NULL;
  ego    ecircle, eedges[2], enodes[3], eloop, eplane, eface, eref;

  /* create the Circle */
  data[0] = xcent[0]; /* center */
  data[1] = xcent[1];
  data[2] = xcent[2];
  data[3] = xax[0];   /* x-axis */
  data[4] = xax[1];
  data[5] = xax[2];
  data[6] = yax[0];  /* y-axis */
  data[7] = yax[1];
  data[8] = yax[2];
  data[9] = r;        /* radius */
  status = EG_makeGeometry(context, CURVE, CIRCLE, NULL, NULL,
                           data, &ecircle);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_getGeometry(ecircle, &oclass, &mtype, &eref, &ivec, &rvec);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the axes for the circle */
  dx[0] = rvec[3];
  dx[1] = rvec[4];
  dx[2] = rvec[5];
  dy[0] = rvec[6];
  dy[1] = rvec[7];
  dy[2] = rvec[8];

  /* create the Nodes for the Edge */
  data[0] = xcent[0] + dx[0]*r;
  data[1] = xcent[1] + dx[1]*r;
  data[2] = xcent[2] + dx[2]*r;
  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[0]);
  if (status != EGADS_SUCCESS) goto cleanup;

  data[0] = xcent[0] - dx[0]*r;
  data[1] = xcent[1] - dx[1]*r;
  data[2] = xcent[2] - dx[2]*r;
  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[1]);
  if (status != EGADS_SUCCESS) goto cleanup;

  enodes[2] = enodes[0];

  /* make the Edges on the Circle */
  tdata[0] = 0;
  tdata[1] = PI;

  status = EG_makeTopology(context, ecircle, EDGE, TWONODE,
                           tdata, 2, &enodes[0], NULL, &eedges[0]);
  if (status != EGADS_SUCCESS) goto cleanup;

  tdata[0] = PI;
  tdata[1] = TWOPI;

  status = EG_makeTopology(context, ecircle, EDGE, TWONODE,
                           tdata, 2, &enodes[1], NULL, &eedges[1]);
  if (status != EGADS_SUCCESS) goto cleanup;


  status = EG_makeTopology(context, NULL, LOOP, CLOSED,
                           NULL, 2, eedges, senses, &eloop);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (btype == LOOP) {

    *eobj = eloop;

  } else {

    /* create the Plane */
    data[0] = xcent[0]; /* center */
    data[1] = xcent[1];
    data[2] = xcent[2];
    data[3] = dx[0];   /* x-axis */
    data[4] = dx[1];
    data[5] = dx[2];
    data[6] = dy[0];   /* y-axis */
    data[7] = dy[1];
    data[8] = dy[2];
    status = EG_makeGeometry(context, SURFACE, PLANE, NULL, NULL,
                             data, &eplane);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_makeTopology(context, eplane, FACE, SFORWARD,
                             NULL, 1, &eloop, senses, &eface);
    if (status != EGADS_SUCCESS) goto cleanup;

    *eobj = eface;
  }

  status = EGADS_SUCCESS;

cleanup:
  EG_free(ivec); ivec = NULL;
  EG_free(rvec); rvec = NULL;

  return status;
}


int
setCircle2_dot( const double *xcent,     /* (in)  Center          */
                const double *xcent_dot, /* (in)  Center velocity */
                const double *xax,       /* (in)  x-axis          */
                const double *xax_dot,   /* (in)  x-axis velocity */
                const double *yax,       /* (in)  y-axis          */
                const double *yax_dot,   /* (in)  y-axis velocity */
                const double r,          /* (in)  radius          */
                const double r_dot,      /* (in)  radius velocity */
                ego eobj )               /* (in/out) Circle with velocities */
{
  int    status = EGADS_SUCCESS;
  int    nnode, nedge, nloop, oclass, mtype, *senses, btype;
  double data[10], data_dot[10], dx[3], dx_dot[3], dy[3], dy_dot[3];
  double *rvec=NULL, *rvec_dot=NULL;
  ego    ecircle, eplane, *enodes, *eloops, *eedges, eref;

  /* get the type */
  status = EG_getTopology(eobj, &eplane, &oclass, &mtype,
                          data, &nloop, &eloops, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (oclass == LOOP) {
    nloop = 1;
    eloops = &eobj;
    btype = LOOP;
  } else {
    btype = FACE;
  }

  /* get the Edge from the Loop */
  status = EG_getTopology(eloops[0], &eref, &oclass, &mtype,
                          data, &nedge, &eedges, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the Nodes and the Circle from the Edge */
  status = EG_getTopology(eedges[0], &ecircle, &oclass, &mtype,
                          data, &nnode, &enodes, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* set the Circle data and velocity */
  data[0] = xcent[0]; /* center */
  data[1] = xcent[1];
  data[2] = xcent[2];
  data[3] = xax[0];   /* x-axis */
  data[4] = xax[1];
  data[5] = xax[2];
  data[6] = yax[0];   /* y-axis */
  data[7] = yax[1];
  data[8] = yax[2];
  data[9] = r;        /* radius */

  data_dot[0] = xcent_dot[0]; /* center */
  data_dot[1] = xcent_dot[1];
  data_dot[2] = xcent_dot[2];
  data_dot[3] = xax_dot[0];   /* x-axis */
  data_dot[4] = xax_dot[1];
  data_dot[5] = xax_dot[2];
  data_dot[6] = yax_dot[0];   /* y-axis */
  data_dot[7] = yax_dot[1];
  data_dot[8] = yax_dot[2];
  data_dot[9] = r_dot;        /* radius */

  status = EG_setGeometry_dot(ecircle, CURVE, CIRCLE, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_getGeometry_dot(ecircle, &rvec, &rvec_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the axes for the circle */
  dx[0] = rvec[3];
  dx[1] = rvec[4];
  dx[2] = rvec[5];
  dy[0] = rvec[6];
  dy[1] = rvec[7];
  dy[2] = rvec[8];
  dx_dot[0] = rvec_dot[3];
  dx_dot[1] = rvec_dot[4];
  dx_dot[2] = rvec_dot[5];
  dy_dot[0] = rvec_dot[6];
  dy_dot[1] = rvec_dot[7];
  dy_dot[2] = rvec_dot[8];

  /* set the sensitivity of the Nodes */
  data[0] = xcent[0] + dx[0]*r;
  data[1] = xcent[1] + dx[1]*r;
  data[2] = xcent[2] + dx[2]*r;
  data_dot[0] = xcent_dot[0] + dx_dot[0]*r + dx[0]*r_dot;
  data_dot[1] = xcent_dot[1] + dx_dot[1]*r + dx[1]*r_dot;
  data_dot[2] = xcent_dot[2] + dx_dot[2]*r + dx[2]*r_dot;
  status = EG_setGeometry_dot(enodes[0], NODE, 0, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  data[0] = xcent[0] - dx[0]*r;
  data[1] = xcent[1] - dx[1]*r;
  data[2] = xcent[2] - dx[2]*r;
  data_dot[0] = xcent_dot[0] - dx_dot[0]*r - dx[0]*r_dot;
  data_dot[1] = xcent_dot[1] - dx_dot[1]*r - dx[1]*r_dot;
  data_dot[2] = xcent_dot[2] - dx_dot[2]*r - dx[2]*r_dot;
  status = EG_setGeometry_dot(enodes[1], NODE, 0, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (btype == FACE) {
    data[0] = xcent[0]; /* center */
    data[1] = xcent[1];
    data[2] = xcent[2];
    data[3] = dx[0];    /* x-axis */
    data[4] = dx[1];
    data[5] = dx[2];
    data[6] = dy[0];    /* y-axis */
    data[7] = dy[1];
    data[8] = dy[2];

    data_dot[0] = xcent_dot[0]; /* center */
    data_dot[1] = xcent_dot[1];
    data_dot[2] = xcent_dot[2];
    data_dot[3] = dx_dot[0];    /* x-axis */
    data_dot[4] = dx_dot[1];
    data_dot[5] = dx_dot[2];
    data_dot[6] = dy_dot[0];    /* y-axis */
    data_dot[7] = dy_dot[1];
    data_dot[8] = dy_dot[2];

    status = EG_setGeometry_dot(eplane, SURFACE, PLANE, NULL, data, data_dot);
    if (status != EGADS_SUCCESS) goto cleanup;
  }

  status = EGADS_SUCCESS;

cleanup:
  EG_free(rvec); rvec = NULL;
  EG_free(rvec_dot); rvec_dot = NULL;

  return status;
}


int
pingNoseCircleBlend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 5;
  double x[26], x_dot[26], params[3], dtime = 1e-7;
  double *xcent, *xax, *yax, *xcent_dot, *xax_dot, *yax_dot, *rc1, *rc1_dot, *rcN, *rcN_dot;
  double xform1[4], xform2[4], xform3[4], xform4[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[5], secs2[5], ebody1, ebody2, tess1, tess2, eloop1, eloop2;

  xcent = x;
  xax   = x+3;
  yax   = x+6;
  rc1   = x+10;
  rcN   = x+18;

  xcent_dot = x_dot;
  xax_dot   = x_dot+3;
  yax_dot   = x_dot+6;
  rc1_dot   = x_dot+10;
  rcN_dot   = x_dot+18;

  rc1[0] = 0.2; /* radius of curvature */
  rc1[1] = 1.;  /* first axis */
  rc1[2] = 0.;
  rc1[3] = 0.;

  rc1[4] = 0.1; /* radius of curvature */
  rc1[5] = 0.;  /* first axis */
  rc1[6] = 1.;
  rc1[7] = 0.;


  rcN[0] = 0.1; /* radius of curvature */
  rcN[1] = 1.;  /* first axis */
  rcN[2] = 0.;
  rcN[3] = 0.;

  rcN[4] = 0.2; /* radius of curvature */
  rcN[5] = 0.;  /* first axis */
  rcN[6] = 1.;
  rcN[7] = 0.;

  /* circle parameters */
  xcent[0] = 0.0; xcent[1] = 0.0; xcent[2] = 0.0;
  xax[0]   = 1.0; xax[1]   = 0.0; xax[2]   = 0.0;
  yax[0]   = 0.0; yax[1]   = 1.0; yax[2]   = 0.0;
  x[9] = 1.0;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = -0.1;
    xform2[2] = -0.2;
    xform2[3] = 2.*dir;

    xform3[0] = 1.0;
    xform3[1] = 0.;
    xform3[2] = 0.;
    xform3[3] = 3.*dir;

    xform4[0] = 1.0;
    xform4[1] = 0.;
    xform4[2] = 0.;
    xform4[3] = 4.*dir;

    /* make the body */
    status = EG_makeTopology(context, NULL, NODE, 0,
                             xcent, 0, NULL, NULL, &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop1);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform3, &secs1[3] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform4, &secs1[4] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_blend(nsec, secs1, rc1, rcN, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.2;
    params[2] = 20.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Nose Circle Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Nose Circle Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 26; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 26; iparam++) {
      if (iparam == 10+1 ||
          iparam == 10+2 ||
          iparam == 10+3 ||
          iparam == 10+5 ||
          iparam == 10+6 ||
          iparam == 10+7 ||

          iparam == 18+1 ||
          iparam == 18+2 ||
          iparam == 18+3 ||
          iparam == 18+5 ||
          iparam == 18+6 ||
          iparam == 18+7 ) continue; /* skip axis as they must be orthogonal */

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = EG_setGeometry_dot(secs1[0], NODE, 0, NULL, xcent, xcent_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setCircle_dot(xcent, xcent_dot,
                             xax, xax_dot,
                             yax, yax_dot,
                             x[9], x_dot[9], eloop1);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform3, xform_dot, secs1[3] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform4, xform_dot, secs1[4] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend_dot(ebody1, nsec, secs1, rc1, rc1_dot, rcN, rcN_dot);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = EG_makeTopology(context, NULL, NODE, 0,
                               xcent, 0, NULL, NULL, &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeCircle(context, LOOP, xcent, xax, yax, x[9], &eloop2);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform3, &secs2[3] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform4, &secs2[4] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend(nsec, secs2, rc1, rcN, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Nose Circle", 5e-7, 5e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(eloop2);
      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(eloop1);
    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
pingNoseCircle2Blend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 5;
  double x[26], x_dot[26], params[3], dtime = 1e-7;
  double *xcent, *xax, *yax, *xcent_dot, *xax_dot, *yax_dot, *rc1, *rc1_dot, *rcN, *rcN_dot;
  double xform1[4], xform2[4], xform3[4], xform4[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[5], secs2[5], ebody1, ebody2, tess1, tess2, eloop1, eloop2;

  xcent = x;
  xax   = x+3;
  yax   = x+6;
  rc1   = x+10;
  rcN   = x+18;

  xcent_dot = x_dot;
  xax_dot   = x_dot+3;
  yax_dot   = x_dot+6;
  rc1_dot   = x_dot+10;
  rcN_dot   = x_dot+18;

  rc1[0] = 0.2; /* radius of curvature */
  rc1[1] = 1.;  /* first axis */
  rc1[2] = 0.;
  rc1[3] = 0.;

  rc1[4] = 0.1; /* radius of curvature */
  rc1[5] = 0.;  /* first axis */
  rc1[6] = 1.;
  rc1[7] = 0.;


  rcN[0] = 0.1; /* radius of curvature */
  rcN[1] = 1.;  /* first axis */
  rcN[2] = 0.;
  rcN[3] = 0.;

  rcN[4] = 0.2; /* radius of curvature */
  rcN[5] = 0.;  /* first axis */
  rcN[6] = 1.;
  rcN[7] = 0.;

  /* circle parameters */
  xcent[0] = 0.0; xcent[1] = 0.0; xcent[2] = 0.0;
  xax[0]   = 1.0; xax[1]   = 0.0; xax[2]   = 0.0;
  yax[0]   = 0.0; yax[1]   = 1.0; yax[2]   = 0.0;
  x[9] = 1.0;

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = -0.1;
    xform2[2] = -0.2;
    xform2[3] = 2.*dir;

    xform3[0] = 1.0;
    xform3[1] = 0.;
    xform3[2] = 0.;
    xform3[3] = 3.*dir;

    xform4[0] = 1.0;
    xform4[1] = 0.;
    xform4[2] = 0.;
    xform4[3] = 4.*dir;

    /* make the body */
    status = EG_makeTopology(context, NULL, NODE, 0,
                             xcent, 0, NULL, NULL, &secs1[0]);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeCircle2(context, LOOP, xcent, xax, yax, x[9], &eloop1);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform3, &secs1[3] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform4, &secs1[4] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_blend(nsec, secs1, rc1, rcN, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.4;
    params[1] =  0.2;
    params[2] = 20.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Nose Circle2 Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Nose Circle2 Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 26; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 26; iparam++) {
      if (iparam == 10+1 ||
          iparam == 10+2 ||
          iparam == 10+3 ||
          iparam == 10+5 ||
          iparam == 10+6 ||
          iparam == 10+7 ||

          iparam == 18+1 ||
          iparam == 18+2 ||
          iparam == 18+3 ||
          iparam == 18+5 ||
          iparam == 18+6 ||
          iparam == 18+7 ) continue; /* skip axis as they must be orthogonal */

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = EG_setGeometry_dot(secs1[0], NODE, 0, NULL, xcent, xcent_dot);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setCircle2_dot(xcent, xcent_dot,
                              xax, xax_dot,
                              yax, yax_dot,
                              x[9], x_dot[9], eloop1);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform3, xform_dot, secs1[3] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform4, xform_dot, secs1[4] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend_dot(ebody1, nsec, secs1, rc1, rc1_dot, rcN, rcN_dot);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = EG_makeTopology(context, NULL, NODE, 0,
                               xcent, 0, NULL, NULL, &secs2[0]);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeCircle2(context, LOOP, xcent, xax, yax, x[9], &eloop2);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform3, &secs2[3] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform4, &secs2[4] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend(nsec, secs2, rc1, rcN, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Nose Circle2", 5e-7, 5e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(eloop2);
      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(eloop1);
    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


/*****************************************************************************/
/*                                                                           */
/*  NACA Airfoil                                                             */
/*                                                                           */
/*****************************************************************************/

#define NUMPNTS    101
#define DXYTOL     1.0e-8

/* set KNOTS to 0 for arc-lenght knots, and -1 for equally spaced knots
 * Equally spaced knots is required for the finite difference sensitivities to be correct
 */
#define KNOTS      -1


int makeNaca( ego context,     /* (in) EGADS context */
              int btype,       /* (in) result type (LOOP or FACE) */
              int sharpte,     /* (in) sharp or blunt TE */
              double m,        /* (in) camber */
              double p,        /* (in) maxloc */
              double t,        /* (in) thickness */
              ego *eobj )      /* (out) result pointer */
{
  int status = EGADS_SUCCESS;

  int     ipnt, *header=NULL, sizes[2], sense[3], nedge, oclass, mtype;
  double  *pnts = NULL, *rdata = NULL;
  double  data[18], tdata[2], tle;
  double  x, y, zeta, s, yt, yc, theta, ycm, dycm;
  ego     eref, enodes[4], eedges[3], ecurve, eline, eloop, eplane;

  /* mallocs required by Windows compiler */
  pnts = (double*)EG_alloc((3*NUMPNTS)*sizeof(double));
  if (pnts == NULL) {
    status = EGADS_MALLOC;
    goto cleanup;
  }

  /* points around airfoil (upper and then lower) */
  for (ipnt = 0; ipnt < NUMPNTS; ipnt++) {
    zeta = TWOPI * ipnt / (NUMPNTS-1);
    s    = (1 + cos(zeta)) / 2;

    if (sharpte == 0) {
      yt   = t/0.20 * (0.2969 * sqrt(s) + s * (-0.1260 + s * (-0.3516 + s * ( 0.2843 + s * (-0.1015)))));
    } else {
      yt   = t/0.20 * (0.2969 * sqrt(s) + s * (-0.1260 + s * (-0.3516 + s * ( 0.2843 + s * (-0.1036)))));
    }

    if (s < p) {
      ycm  = (s * (2*p -   s)) / (p*p);
      dycm = (    (2*p - 2*s)) / (p*p);
    } else {
      ycm  = ((1-2*p) + s * (2*p -   s)) / pow(1-p,2);
      dycm = (              (2*p - 2*s)) / pow(1-p,2);
    }
    yc    = m * ycm;
    theta = atan(m * dycm);

    if (ipnt < NUMPNTS/2) {
      x = s  - yt * sin(theta);
      y = yc + yt * cos(theta);
    } else if (ipnt == NUMPNTS/2) {
      x = 0.;
      y = 0.;
    } else {
      x = s  + yt * sin(theta);
      y = yc - yt * cos(theta);
    }

    pnts[3*ipnt  ] = x;
    pnts[3*ipnt+1] = y;
    pnts[3*ipnt+2] = 0.;
  }

  /* create spline curve from upper TE, to LE, to lower TE
   *
   * finite difference must use knots equally spaced (sizes[1] == -1)
   * arc-length based knots (sizes[1] == 0) causes the t-space to change.
   */
  sizes[0] = NUMPNTS;
  sizes[1] = KNOTS;
  status = EG_approximate(context, 0, DXYTOL, sizes, pnts, &ecurve);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the B-spline CURVE data */
  status = EG_getGeometry(ecurve, &oclass, &mtype, &eref, &header, &rdata);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* create Node at trailing edge */
  ipnt = 0;
  data[0] = pnts[3*ipnt  ];
  data[1] = pnts[3*ipnt+1];
  data[2] = pnts[3*ipnt+2];
  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[0]);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* node at leading edge as a function of the spline */
  ipnt = (NUMPNTS - 1) / 2 + 3; /* leading edge index, with knot offset of 3 (cubic)*/
  tle = rdata[ipnt];            /* leading edge t-value (should be very close to (0,0,0) */

  status = EG_evaluate(ecurve, &tle, data);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = EG_makeTopology(context, NULL, NODE, 0,
                           data, 0, NULL, NULL, &enodes[1]);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (sharpte == 0) {
    /* create Node at lower trailing edge */
    ipnt = NUMPNTS - 1;
    data[0] = pnts[3*ipnt  ];
    data[1] = pnts[3*ipnt+1];
    data[2] = pnts[3*ipnt+2];
    status = EG_makeTopology(context, NULL, NODE, 0,
                             data, 0, NULL, NULL, &enodes[2]);
    if (status != EGADS_SUCCESS) goto cleanup;

    enodes[3] = enodes[0];
  } else {
    enodes[2] = enodes[0];
  }

  /* make Edge for upper surface */
  tdata[0] = 0;   /* t value at lower TE because EG_spline1dFit was used */
  tdata[1] = tle;

  /* construct the upper Edge */
  status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                           tdata, 2, &enodes[0], NULL, &eedges[0]);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* make Edge for lower surface */
  tdata[0] = tdata[1]; /* t-value at leading edge */
  tdata[1] = 1;        /* t value at upper TE because EG_spline1dFit was used */

  /* construct the lower Edge */
  status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                           tdata, 2, &enodes[1], NULL, &eedges[1]);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (sharpte == 0) {
    nedge = 3;

    /* create line segment at trailing edge */
    ipnt = NUMPNTS - 1;
    data[0] = pnts[3*ipnt  ];
    data[1] = pnts[3*ipnt+1];
    data[2] = pnts[3*ipnt+2];
    data[3] = pnts[0] - data[0];
    data[4] = pnts[1] - data[1];
    data[5] = pnts[2] - data[2];

    status = EG_makeGeometry(context, CURVE, LINE, NULL, NULL, data, &eline);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* make Edge for this line */
    tdata[0] = 0;
    tdata[1] = sqrt(data[3]*data[3] + data[4]*data[4] + data[5]*data[5]);

    status = EG_makeTopology(context, eline, EDGE, TWONODE,
                             tdata, 2, &enodes[2], NULL, &eedges[2]);
    if (status != EGADS_SUCCESS) goto cleanup;
  } else {
    nedge = 2;
  }

  /* create loop of the Edges */
  sense[0] = SFORWARD;
  sense[1] = SFORWARD;
  sense[2] = SFORWARD;

  status = EG_makeTopology(context, NULL, LOOP, CLOSED,
                           NULL, nedge, eedges, sense, &eloop);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (btype == FACE) {
    /* create a plane for the loop */
    data[0] = 0.;
    data[1] = 0.;
    data[2] = 0.;
    data[3] = 1.; data[4] = 0.; data[5] = 0.;
    data[6] = 0.; data[7] = 1.; data[8] = 0.;

    status = EG_makeGeometry(context, SURFACE, PLANE, NULL, NULL, data, &eplane);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* create the face from the plane and the loop */
    status = EG_makeTopology(context, eplane, FACE, SFORWARD,
                             NULL, 1, &eloop, sense, eobj);
    if (status != EGADS_SUCCESS) goto cleanup;
  } else {
    /* return the loop */
    *eobj = eloop;
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  EG_free(header);
  EG_free(rdata);
  EG_free(pnts);

  return status;
}


int setNaca_dot( int sharpte,     /* (in) sharp or blunt TE */
                 double m,        /* (in) camber */
                 double m_dot,    /* (in) camber sensitivity */
                 double p,        /* (in) maxloc */
                 double p_dot,    /* (in) maxloc sensitivity */
                 double t,        /* (in) thickness */
                 double t_dot,    /* (in) thickness sensitivity */
                 ego eobj )       /* (out) ego with sensitivities */
{
  int status = EGADS_SUCCESS;

  int     ipnt, nedge, oclass, btype, mtype, nchild, nloop, *senses, sizes[2];
  double  data[18], data_dot[18], trange[4];
  double  zeta, s, *pnts=NULL, *pnts_dot=NULL;
  double  yt, yt_dot, yc, yc_dot, theta, theta_dot;
  double  ycm, ycm_dot, dycm, dycm_dot, tle, tle_dot;
  double  x, x_dot, y, y_dot, *rvec=NULL, *rvec_dot=NULL;
  ego     enodes[3], *eedges, ecurve, eline, *eloops, eplane, eref, *echildren;

  /* mallocs required by Windows compiler */
  pnts     = (double*)EG_alloc((3*NUMPNTS)*sizeof(double));
  pnts_dot = (double*)EG_alloc((3*NUMPNTS)*sizeof(double));
  if (pnts == NULL || pnts_dot == NULL) {
    status = EGADS_MALLOC;
    goto cleanup;
  }

  /* points around airfoil (upper and then lower) */
  for (ipnt = 0; ipnt < NUMPNTS; ipnt++) {
    zeta = TWOPI * ipnt / (NUMPNTS-1);
    s    = (1 + cos(zeta)) / 2;

    if (sharpte == 0) {
      yt     = t     / 0.20 * (0.2969 * sqrt(s) + s * (-0.1260 + s * (-0.3516 + s * ( 0.2843 + s * (-0.1015)))));
      yt_dot = t_dot / 0.20 * (0.2969 * sqrt(s) + s * (-0.1260 + s * (-0.3516 + s * ( 0.2843 + s * (-0.1015)))));
    } else {
      yt     = t     / 0.20 * (0.2969 * sqrt(s) + s * (-0.1260 + s * (-0.3516 + s * ( 0.2843 + s * (-0.1036)))));
      yt_dot = t_dot / 0.20 * (0.2969 * sqrt(s) + s * (-0.1260 + s * (-0.3516 + s * ( 0.2843 + s * (-0.1036)))));
    }

    if (s < p) {
      ycm      =         ( s * (2*p -   s)) / (p*p);
      ycm_dot  = p_dot * (-2 * s * (p - s)) / (p*p*p);
      dycm     =         (     (2*p - 2*s)) / (p*p);
      dycm_dot = p_dot * (-2 * (p   - 2*s)) / (p*p*p);
    } else {
      ycm      =         ( (1-2*p) + s * (2*p -   s)) / pow(1-p,2);
      ycm_dot  = p_dot * (         2*(s - p)*(s - 1)) / pow(p-1,3);
      dycm     =         (               (2*p - 2*s)) / pow(1-p,2);
      dycm_dot = p_dot * (          -2*(1 + p - 2*s)) / pow(p-1,3);
    }
    yc        = m * ycm;
    yc_dot    = m_dot * ycm + m * ycm_dot;
    theta     = atan(m * dycm);
    theta_dot = (m_dot * dycm + m * dycm_dot) / (1 + m*m*dycm*dycm);

    if (ipnt < NUMPNTS/2) {
      x = s  - yt * sin(theta);
      y = yc + yt * cos(theta);
      x_dot =        - yt_dot * sin(theta) - theta_dot * yt * cos(theta);
      y_dot = yc_dot + yt_dot * cos(theta) - theta_dot * yt * sin(theta);
    } else if (ipnt == NUMPNTS/2) {
      x = 0;
      y = 0;
      x_dot = 0;
      y_dot = 0;
    } else {
      x = s  + yt * sin(theta);
      y = yc - yt * cos(theta);
      x_dot =        + yt_dot * sin(theta) + theta_dot * yt * cos(theta);
      y_dot = yc_dot - yt_dot * cos(theta) + theta_dot * yt * sin(theta);
    }

    pnts[3*ipnt  ] = x;
    pnts[3*ipnt+1] = y;
    pnts[3*ipnt+2] = 0.;

    pnts_dot[3*ipnt  ] = x_dot;
    pnts_dot[3*ipnt+1] = y_dot;
    pnts_dot[3*ipnt+2] = 0.;
  }


  /* get the type */
  status = EG_getTopology(eobj, &eplane, &oclass, &mtype,
                          data, &nloop, &eloops, &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  if (oclass == LOOP) {
    nloop = 1;
    eloops = &eobj;
    btype = LOOP;
  } else {
    btype = FACE;
  }

  /* get the edges */
  status = EG_getTopology(eloops[0], &eref, &oclass, &mtype, data, &nedge, &eedges,
                          &senses);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* get the nodes and the curve from the first edge */
  status = EG_getTopology(eedges[0], &ecurve, &oclass, &mtype, trange, &nchild, &echildren,
                          &senses);
  if (status != EGADS_SUCCESS) goto cleanup;
  enodes[0] = echildren[0]; // upper trailing edge
  enodes[1] = echildren[1]; // leading edge

  /* populate spline curve sensitivities */
  sizes[0] = NUMPNTS;
  sizes[1] = KNOTS;
  status = EG_approximate_dot(ecurve, 0, DXYTOL, sizes, pnts, pnts_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* set the sensitivity of the Node at trailing edge */
  ipnt = 0;
  status = EG_setGeometry_dot(enodes[0], NODE, 0, NULL, &pnts[3*ipnt], &pnts_dot[3*ipnt]);
  if (status != EGADS_SUCCESS) goto cleanup;

  /* set the sensitivity of the Node at leading edge */
  status = EG_getGeometry_dot(ecurve, &rvec, &rvec_dot);
  if (status != EGADS_SUCCESS) goto cleanup;

  ipnt = (NUMPNTS - 1) / 2 + 3; /* leading edge index, with knot offset of 3 (cubic)*/
  tle     = rvec[ipnt];         /* leading edge t-value (should be very close to (0,0,0) */
  tle_dot = rvec_dot[ipnt];

  status = EG_evaluate_dot(ecurve, &tle, &tle_dot, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;
  status = EG_setGeometry_dot(enodes[1], NODE, 0, NULL, data, data_dot);
  if (status != EGADS_SUCCESS) goto cleanup;


  if (sharpte == 0) {
    /* get trailing edge line and the lower trailing edge node from the 3rd edge */
    status = EG_getTopology(eedges[2], &eline, &oclass, &mtype, data, &nchild, &echildren,
                            &senses);
    if (status != EGADS_SUCCESS) goto cleanup;
    enodes[2] = echildren[0]; // lower trailing edge

    /* set the sensitivity of the Node at lower trailing edge */
    ipnt = NUMPNTS - 1;
    status = EG_setGeometry_dot(enodes[2], NODE, 0, NULL, &pnts[3*ipnt], &pnts_dot[3*ipnt]);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* set the sensitivity of the line segment at trailing edge */
    ipnt = NUMPNTS - 1;
    data[0] = pnts[3*ipnt  ];
    data[1] = pnts[3*ipnt+1];
    data[2] = pnts[3*ipnt+2];
    data[3] = pnts[0] - data[0];
    data[4] = pnts[1] - data[1];
    data[5] = pnts[2] - data[2];

    data_dot[0] = pnts_dot[3*ipnt  ];
    data_dot[1] = pnts_dot[3*ipnt+1];
    data_dot[2] = pnts_dot[3*ipnt+2];
    data_dot[3] = pnts_dot[0] - data_dot[0];
    data_dot[4] = pnts_dot[1] - data_dot[1];
    data_dot[5] = pnts_dot[2] - data_dot[2];

    status = EG_setGeometry_dot(eline, CURVE, LINE, NULL, data, data_dot);
    if (status != EGADS_SUCCESS) goto cleanup;
  }

  if (btype == FACE) {
    /* plane data */
    data[0] = 0.;
    data[1] = 0.;
    data[2] = 0.;
    data[3] = 1.; data[4] = 0.; data[5] = 0.;
    data[6] = 0.; data[7] = 1.; data[8] = 0.;

    /* set the sensitivity of the plane */
    data_dot[0] = 0.;
    data_dot[1] = 0.;
    data_dot[2] = 0.;
    data_dot[3] = 0.; data_dot[4] = 0.; data_dot[5] = 0.;
    data_dot[6] = 0.; data_dot[7] = 0.; data_dot[8] = 0.;

    status = EG_setGeometry_dot(eplane, SURFACE, PLANE, NULL, data, data_dot);
    if (status != EGADS_SUCCESS) goto cleanup;
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  EG_free(rvec);
  EG_free(rvec_dot);
  EG_free(pnts);
  EG_free(pnts_dot);

  return status;
}


int
pingNacaRuled(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  int    sharpte = 0;
  double x[3], x_dot[3], params[3], dtime = 1e-8;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2, eloop1, eloop2;
  enum naca { im, ip, it };

  x[im] = 0.1;  /* camber */
  x[ip] = 0.4;  /* max loc */
  x[it] = 0.16; /* thickness */

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    status = makeNaca( context, FACE, sharpte, x[im], x[ip], x[it], &secs1[0] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeNaca( context, LOOP, sharpte, x[im], x[ip], x[it], &eloop1 );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop1, xform1, &secs1[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs1[0], xform2, &secs1[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_ruled(nsec, secs1, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.5;
    params[1] =  4.0;
    params[2] = 35.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled NACA Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Ping Ruled NACA Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 3; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 3; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setNaca_dot( sharpte,
                            x[im], x_dot[im],
                            x[ip], x_dot[ip],
                            x[it], x_dot[it],
                            secs1[0] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setNaca_dot( sharpte,
                            x[im], x_dot[im],
                            x[ip], x_dot[ip],
                            x[it], x_dot[it],
                            eloop1 );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop1, xform1, xform_dot, secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled_dot(ebody1, nsec, secs1);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* make a perturbed body for finite difference */
      x[iparam] += dtime;
      status = makeNaca( context, FACE, sharpte, x[im], x[ip], x[it], &secs2[0] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeNaca( context, LOOP, sharpte, x[im], x[ip], x[it], &eloop2 );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop2, xform1, &secs2[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs2[0], xform2, &secs2[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled(nsec, secs2, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x[iparam] -= dtime;

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* ping the bodies */
      status = pingBodies(tess1, tess2, dtime, iparam, "Ping Ruled NACA", 5e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

      EG_deleteObject(eloop2);
      EG_deleteObject(tess2);
      EG_deleteObject(ebody2);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs2[i]);
    }

    EG_deleteObject(eloop1);
    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs1[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
pingNacaBlend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  int    sharpte = 0;
  double x[7], x_dot[7], params[3], dtime = 1e-8;
  double *RC1, *RC1_dot, *RCn, *RCn_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs1[3], secs2[3], ebody1, ebody2, tess1, tess2, eloop1, eloop2;
  enum naca { im, ip, it };

  RC1 = x + 3;
  RCn = x + 5;

  RC1_dot = x_dot + 3;
  RCn_dot = x_dot + 5;

  x[im] = 0.1;  /* camber */
  x[ip] = 0.4;  /* max loc */
  x[it] = 0.16; /* thickness */

  RC1[0] = 0;
  RC1[1] = 1;

  RCn[0] = 0;
  RCn[1] = 2;

  for (sharpte = 0; sharpte <= 1; sharpte++) {
    for (dir = -1; dir <= 1; dir += 2) {

      xform1[0] = 1.0;
      xform1[1] = 0.1;
      xform1[2] = 0.2;
      xform1[3] = 1.*dir;

      xform2[0] = 1.0;
      xform2[1] = 0.;
      xform2[2] = 0.;
      xform2[3] = 2.*dir;

      /* make the ruled body */
      status = makeNaca( context, FACE, sharpte, x[im], x[ip], x[it], &secs1[0] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeNaca( context, LOOP, sharpte, x[im], x[ip], x[it], &eloop1 );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop1, xform1, &secs1[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs1[0], xform2, &secs1[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend(nsec, secs1, RC1, RCn, &ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* tessellation parameters */
      params[0] =  0.5;
      params[1] =  4.0;
      params[2] = 35.0;

      /* make the tessellation */
      status = EG_makeTessBody(ebody1, params, &tess1);

      /* get the Faces from the Body */
      status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* get the Edges from the Body */
      status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;

      for (iedge = 0; iedge < nedge; iedge++) {
        status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
        if (status != EGADS_SUCCESS) goto cleanup;
        printf(" Ping Blend NACA Edge %d np1 = %d\n", iedge+1, np1);
      }

      for (iface = 0; iface < nface; iface++) {
        status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                &nt1, &ts1, &tc1);
        if (status != EGADS_SUCCESS) goto cleanup;
        printf(" Ping Blend NACA Face %d np1 = %d\n", iface+1, np1);
      }

      /* zero out velocities */
      for (iparam = 0; iparam < 7; iparam++) x_dot[iparam] = 0;

      for (iparam = 0; iparam < 7; iparam++) {
        if (iparam == 3) continue; // RC1[0] swithc (not a parameter)
        if (iparam == 5) continue; // RCn[0] swithc (not a parameter)

        /* set the velocity of the original body */
        x_dot[iparam] = 1.0;
        status = setNaca_dot( sharpte,
                              x[im], x_dot[im],
                              x[ip], x_dot[ip],
                              x[it], x_dot[it],
                              secs1[0] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = setNaca_dot( sharpte,
                              x[im], x_dot[im],
                              x[ip], x_dot[ip],
                              x[it], x_dot[it],
                              eloop1 );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = setTransform_dot( eloop1, xform1, xform_dot, secs1[1] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = setTransform_dot( secs1[0], xform2, xform_dot, secs1[2] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_blend_dot(ebody1, nsec, secs1, RC1, RC1_dot, RCn, RCn_dot);
        if (status != EGADS_SUCCESS) goto cleanup;
        x_dot[iparam] = 0.0;

        status = EG_hasGeometry_dot(ebody1);
        if (status != EGADS_SUCCESS) goto cleanup;


        /* make a perturbed body for finite difference */
        x[iparam] += dtime;
        status = makeNaca( context, FACE, sharpte, x[im], x[ip], x[it], &secs2[0] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = makeNaca( context, LOOP, sharpte, x[im], x[ip], x[it], &eloop2 );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = makeTransform( eloop2, xform1, &secs2[1] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = makeTransform( secs2[0], xform2, &secs2[2] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_blend(nsec, secs2, RC1, RCn, &ebody2);
        if (status != EGADS_SUCCESS) goto cleanup;
        x[iparam] -= dtime;

        /* map the tessellation */
        status = EG_mapTessBody(tess1, ebody2, &tess2);
        if (status != EGADS_SUCCESS) goto cleanup;

        /* ping the bodies */
        status = pingBodies(tess1, tess2, dtime, iparam, "Ping Blend NACA", 5e-7, 1e-7, 1e-7);
        if (status != EGADS_SUCCESS) goto cleanup;

        EG_deleteObject(eloop2);
        EG_deleteObject(tess2);
        EG_deleteObject(ebody2);
        for (i = 0; i < nsec; i++)
          EG_deleteObject(secs2[i]);
      }

      EG_deleteObject(eloop1);
      EG_deleteObject(tess1);
      EG_deleteObject(ebody1);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs1[i]);
    }
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
equivNacaRuled(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  int    sharpte = 0;
  double x[3], x_dot[3], params[3];
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs[3], ebody1, ebody2, tess1, tess2, eloop;
  enum naca { im, ip, it };

  egadsSplineVels vels;

  vels.usrData = NULL;
  vels.velocityOfNode = &velocityOfNode;
  vels.velocityOfEdge = &velocityOfEdge;

  x[im] = 0.1;  /* camber */
  x[ip] = 0.4;  /* max loc */
  x[it] = 0.16; /* thickness */

  for (dir = -1; dir <= 1; dir += 2) {

    xform1[0] = 1.0;
    xform1[1] = 0.1;
    xform1[2] = 0.2;
    xform1[3] = 1.*dir;

    xform2[0] = 1.0;
    xform2[1] = 0.;
    xform2[2] = 0.;
    xform2[3] = 2.*dir;

    /* make the ruled body */
    status = makeNaca( context, FACE, sharpte, x[im], x[ip], x[it], &secs[0] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeNaca( context, LOOP, sharpte, x[im], x[ip], x[it], &eloop );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( eloop, xform1, &secs[1] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = makeTransform( secs[0], xform2, &secs[2] );
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_ruled(nsec, secs, &ebody1);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* make the body for _vels sensitivities */
    status = EG_ruled(nsec, secs, &ebody2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* tessellation parameters */
    params[0] =  0.5;
    params[1] =  4.0;
    params[2] = 35.0;

    /* make the tessellation */
    status = EG_makeTessBody(ebody1, params, &tess1);

    /* map the tessellation */
    status = EG_mapTessBody(tess1, ebody2, &tess2);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Faces from the Body */
    status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* get the Edges from the Body */
    status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
    if (status != EGADS_SUCCESS) goto cleanup;

    for (iedge = 0; iedge < nedge; iedge++) {
      status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Ruled NACA Edge %d np1 = %d\n", iedge+1, np1);
    }

    for (iface = 0; iface < nface; iface++) {
      status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                              &nt1, &ts1, &tc1);
      if (status != EGADS_SUCCESS) goto cleanup;
      printf(" Equiv Ruled NACA Face %d np1 = %d\n", iface+1, np1);
    }

    /* zero out velocities */
    for (iparam = 0; iparam < 3; iparam++) x_dot[iparam] = 0;

    for (iparam = 0; iparam < 3; iparam++) {

      /* set the velocity of the original body */
      x_dot[iparam] = 1.0;
      status = setNaca_dot( sharpte,
                            x[im], x_dot[im],
                            x[ip], x_dot[ip],
                            x[it], x_dot[it],
                            secs[0] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setNaca_dot( sharpte,
                            x[im], x_dot[im],
                            x[ip], x_dot[ip],
                            x[it], x_dot[it],
                            eloop );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( eloop, xform1, xform_dot, secs[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = setTransform_dot( secs[0], xform2, xform_dot, secs[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_ruled_dot(ebody1, nsec, secs);
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_hasGeometry_dot(ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;


      /* set the velocity with _vels functions */
      status = EG_ruled_vels(nsec, secs, &vels, ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;
      x_dot[iparam] = 0.0;


      /* ping the bodies */
      status = equivDotVels(tess1, tess2, iparam, "Equiv Ruled NACA", 1e-7, 1e-7, 1e-7);
      if (status != EGADS_SUCCESS) goto cleanup;

    }

    EG_deleteObject(eloop);
    EG_deleteObject(tess1);
    EG_deleteObject(ebody1);
    for (i = 0; i < nsec; i++)
      EG_deleteObject(secs[i]);
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int
equivNacaBlend(ego context)
{
  int    status = EGADS_SUCCESS;
  int    i, iparam, np1, nt1, iedge, nedge, iface, nface, dir, nsec = 3;
  int    sharpte = 0;
  double x[7], x_dot[7], params[3];
  double *RC1, *RC1_dot, *RCn, *RCn_dot;
  double xform1[4], xform2[4], xform_dot[4]={0,0,0,0};
  const int    *pt1, *pi1, *ts1, *tc1;
  const double *t1, *x1, *uv1;
  ego    secs[3], ebody1, ebody2, tess1, tess2, eloop;
  enum naca { im, ip, it };

  egadsSplineVels vels;

  vels.usrData = NULL;
  vels.velocityOfNode = &velocityOfNode;
  vels.velocityOfEdge = &velocityOfEdge;

  RC1 = x + 3;
  RCn = x + 5;

  RC1_dot = x_dot + 3;
  RCn_dot = x_dot + 5;

  x[im] = 0.1;  /* camber */
  x[ip] = 0.4;  /* max loc */
  x[it] = 0.16; /* thickness */

  RC1[0] = 0;
  RC1[1] = 1;

  RCn[0] = 0;
  RCn[1] = 2;

  for (sharpte = 0; sharpte <= 1; sharpte++) {
    for (dir = -1; dir <= 1; dir += 2) {

      xform1[0] = 1.0;
      xform1[1] = 0.1;
      xform1[2] = 0.2;
      xform1[3] = 1.*dir;

      xform2[0] = 1.0;
      xform2[1] = 0.;
      xform2[2] = 0.;
      xform2[3] = 2.*dir;

      /* make the ruled body */
      status = makeNaca( context, FACE, sharpte, x[im], x[ip], x[it], &secs[0] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeNaca( context, LOOP, sharpte, x[im], x[ip], x[it], &eloop );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( eloop, xform1, &secs[1] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = makeTransform( secs[0], xform2, &secs[2] );
      if (status != EGADS_SUCCESS) goto cleanup;

      status = EG_blend(nsec, secs, RC1, RCn, &ebody1);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* make the body for _vels sensitivities */
      status = EG_blend(nsec, secs, RC1, RCn, &ebody2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* tessellation parameters */
      params[0] =  0.5;
      params[1] =  4.0;
      params[2] = 35.0;

      /* make the tessellation */
      status = EG_makeTessBody(ebody1, params, &tess1);

      /* map the tessellation */
      status = EG_mapTessBody(tess1, ebody2, &tess2);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* get the Faces from the Body */
      status = EG_getBodyTopos(ebody1, NULL, FACE, &nface, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;

      /* get the Edges from the Body */
      status = EG_getBodyTopos(ebody1, NULL, EDGE, &nedge, NULL);
      if (status != EGADS_SUCCESS) goto cleanup;

      for (iedge = 0; iedge < nedge; iedge++) {
        status = EG_getTessEdge(tess1, iedge+1, &np1, &x1, &t1);
        if (status != EGADS_SUCCESS) goto cleanup;
        printf(" Equiv Blend NACA Edge %d np1 = %d\n", iedge+1, np1);
      }

      for (iface = 0; iface < nface; iface++) {
        status = EG_getTessFace(tess1, iface+1, &np1, &x1, &uv1, &pt1, &pi1,
                                &nt1, &ts1, &tc1);
        if (status != EGADS_SUCCESS) goto cleanup;
        printf(" Equiv Blend NACA Face %d np1 = %d\n", iface+1, np1);
      }

      /* zero out velocities */
      for (iparam = 0; iparam < 7; iparam++) x_dot[iparam] = 0;

      for (iparam = 0; iparam < 7; iparam++) {
        if (iparam == 3) continue; // RC1[0] swithc (not a parameter)
        if (iparam == 5) continue; // RCn[0] swithc (not a parameter)

        /* set the velocity of the original body */
        x_dot[iparam] = 1.0;
        status = setNaca_dot( sharpte,
                              x[im], x_dot[im],
                              x[ip], x_dot[ip],
                              x[it], x_dot[it],
                              secs[0] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = setNaca_dot( sharpte,
                              x[im], x_dot[im],
                              x[ip], x_dot[ip],
                              x[it], x_dot[it],
                              eloop );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = setTransform_dot( eloop, xform1, xform_dot, secs[1] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = setTransform_dot( secs[0], xform2, xform_dot, secs[2] );
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_blend_dot(ebody1, nsec, secs, RC1, RC1_dot, RCn, RCn_dot);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_hasGeometry_dot(ebody1);
        if (status != EGADS_SUCCESS) goto cleanup;


        /* set the velocity with _vels functions */
        status = EG_blend_vels(nsec, secs, RC1, RC1_dot, RCn, RCn_dot, &vels, ebody2);
        if (status != EGADS_SUCCESS) goto cleanup;
        x_dot[iparam] = 0.0;


        /* ping the bodies */
        status = equivDotVels(tess1, tess2, iparam, "Equiv Blend NACA", 1e-7, 1e-7, 1e-7);
        if (status != EGADS_SUCCESS) goto cleanup;

      }

      EG_deleteObject(eloop);
      EG_deleteObject(tess1);
      EG_deleteObject(ebody1);
      for (i = 0; i < nsec; i++)
        EG_deleteObject(secs[i]);
    }
  }

cleanup:
  if (status != EGADS_SUCCESS) {
    printf(" Failure %d in %s\n", status, __func__);
  }
  return status;
}


int main(int argc, char *argv[])
{
  int status;
  ego context;

  /* create an EGADS context */
  status = EG_open(&context);
  if (status != EGADS_SUCCESS) {
    printf(" EG_open return = %d\n", status);
    return EXIT_FAILURE;
  }

  /*-------*/
  status = pingLineRuled(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = pingLineBlend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = equivLineRuled(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = equivLineBlend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  /*-------*/
  status = pingLine2Ruled(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = pingLine2Blend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  /*-------*/
  status = pingCircleRuled(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = pingCircleBlend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = equivCircleRuled(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = equivCircleBlend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  /*-------*/
  status = pingNoseCircleBlend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = pingNoseCircle2Blend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  /*-------*/
  status = pingNacaRuled(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = pingNacaBlend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = equivNacaRuled(context);
  if (status != EGADS_SUCCESS) goto cleanup;

  status = equivNacaBlend(context);
  if (status != EGADS_SUCCESS) goto cleanup;

cleanup:

  EG_close(context);

  /* these statements are in case we used an error return to go to cleanup */
  if (status != EGADS_SUCCESS) {
    printf(" Overall Failure %d\n", status);
    status = EXIT_FAILURE;
  } else {
    printf(" EGADS_SUCCESS!\n");
    status = EXIT_SUCCESS;
  }

  return status;
}
