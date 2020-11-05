#include "UVMAP_LIB.h"

/*
 * UVMAP : TRIA-FACE SURFACE MESH UV MAPPING GENERATOR
 *         DERIVED FROM AFLR4, UG, UG2, and UG3 LIBRARIES
 * $Id: uvmap_solve.c,v 1.16 2020/06/11 23:34:56 marcum Exp $
 * Copyright 1994-2020, David L. Marcum
 */

void uvmap_solve (
  INT_ bnd_flag,
  INT_ nnode,
  INT_ xyz_scale,
  INT_ *ibfin,
  INT_ *iccin,
  INT_3D *inibf,
  INT_ *libfin,
  double *dumax,
  double relax,
  DOUBLE_2D *u,
  DOUBLE_3D *x)
{
  // Do one iteration of pseudo elliptic equation solver for uv mapping.

  INT_ ibface, inode, inode1, inode2, inode3, loc, loc1, loc2;

  double du1, du2,
         dx201, dx202, dx203, dx211, dx212, dx213, 
         dx301, dx302, dx303, dx311, dx312, dx313,
         lhs, rhs1, rhs2, w1, w2;

  // do not use xyz physical space scaling.

  if (xyz_scale == 0 || x == NULL) {

    // loop over nodes

    for (inode = 1; inode <= nnode; inode++) {

      // if bnd_flag=0 then only consider interior nodes
      // if bnd_flag=1 then only consider interior nodes and those on
      // boundary-edges of inner closed curves

      if (iccin[inode] == 0 || (iccin[inode] > 1 && bnd_flag == 1)) {

        lhs  = 0.0;
        rhs1 = 0.0;
        rhs2 = 0.0;

        // loop over tria-faces attached to node inode

        loc1 = libfin[inode];
        loc2 = libfin[inode+1];

        for (loc = loc1; loc < loc2; loc++) {

          ibface = ibfin[loc];

          if (inode == inibf[ibface][0]) {
            inode2 = inibf[ibface][1];
            inode3 = inibf[ibface][2];
          }
          else if (inode == inibf[ibface][1]) {
            inode2 = inibf[ibface][2];
            inode3 = inibf[ibface][0];
          }
          else {
            inode2 = inibf[ibface][0];
            inode3 = inibf[ibface][1];
          }

          // sum RHS and LHS

          rhs1 = rhs1 + u[inode2][0] + u[inode3][0];
          rhs2 = rhs2 + u[inode2][1] + u[inode3][1];

          lhs = lhs + 2.0;
        }

        // solve for new uv coordinates with relaxation

        du1 = relax * (rhs1 / lhs - u[inode][0]);
        du2 = relax * (rhs2 / lhs - u[inode][1]);

        u[inode][0] = u[inode][0] + du1;
        u[inode][1] = u[inode][1] + du2;

        // set maximum change in uv coordinates

        *dumax = MAX (fabs (du1), *dumax);
        *dumax = MAX (fabs (du2), *dumax);
      }
    }
  }

  // use xyz physical space scaling

  else {

    for (inode = 1; inode <= nnode; inode++) {

      // if bnd_flag=0 then only consider interior nodes
      // if bnd_flag=1 then only consider interior nodes and those on
      // boundary-edges of inner closed curves

      if ((bnd_flag == 0 && iccin[inode] == 0) ||
          (bnd_flag == 1 && iccin[inode] > 1)) {

        lhs  = 0.0;
        rhs1 = 0.0;
        rhs2 = 0.0;

        // loop over tria-faces attached to node inode

        loc1 = libfin[inode];
        loc2 = libfin[inode+1];

        for (loc = loc1; loc < loc2; loc++) {

          ibface = ibfin[loc];

          if (inode == inibf[ibface][0]) {
            inode1 = inibf[ibface][0];
            inode2 = inibf[ibface][1];
            inode3 = inibf[ibface][2];
          }
          else if (inode == inibf[ibface][1]) {
            inode1 = inibf[ibface][1];
            inode2 = inibf[ibface][2];
            inode3 = inibf[ibface][0];
          }
          else {
            inode1 = inibf[ibface][2];
            inode2 = inibf[ibface][0];
            inode3 = inibf[ibface][1];
          }

          // set edge weights for interior trias

          dx211 = x[inode2][0] - x[inode1][0];
          dx212 = x[inode2][1] - x[inode1][1];
          dx213 = x[inode2][2] - x[inode1][2];
          dx311 = x[inode3][0] - x[inode1][0];
          dx312 = x[inode3][1] - x[inode1][1];
          dx313 = x[inode3][2] - x[inode1][2];

          dx201 = x[inode2][0] - 0.5 * (x[inode1][0] + x[inode3][0]);
          dx202 = x[inode2][1] - 0.5 * (x[inode1][1] + x[inode3][1]);
          dx203 = x[inode2][2] - 0.5 * (x[inode1][2] + x[inode3][2]);
          dx301 = x[inode3][0] - 0.5 * (x[inode1][0] + x[inode2][0]);
          dx302 = x[inode3][1] - 0.5 * (x[inode1][1] + x[inode2][1]);
          dx303 = x[inode3][2] - 0.5 * (x[inode1][2] + x[inode2][2]);

          w1 = sqrt ((dx301 * dx301 + dx302 * dx302 + dx303 * dx303) * (4.0/3.0)
                   / (dx211 * dx211 + dx212 * dx212 + dx213 * dx213));
          w2 = sqrt ((dx201 * dx201 + dx202 * dx202 + dx203 * dx203) * (4.0/3.0)
                   / (dx311 * dx311 + dx312 * dx312 + dx313 * dx313));

          // sum RHS and LHS

          rhs1 = rhs1 + w1 * u[inode2][0] + w2 * u[inode3][0];
          rhs2 = rhs2 + w1 * u[inode2][1] + w2 * u[inode3][1];

          lhs = lhs + w1 + w2;
        }

        // solve for new uv coordinates with relaxation

        du1 = relax * (rhs1 / lhs - u[inode][0]);
        du2 = relax * (rhs2 / lhs - u[inode][1]);

        u[inode][0] = u[inode][0] + du1;
        u[inode][1] = u[inode][1] + du2;

        // set maximum change in uv coordinates

        *dumax = MAX (fabs (du1), *dumax);
        *dumax = MAX (fabs (du2), *dumax);
      }
    }
  }

  return;
}
