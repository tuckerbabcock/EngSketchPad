// Mesh related utility functions - Written by Dr. Ryan Durscher AFRL/RQVC

#include "egads.h"
#include "aimUtil.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifdef WIN32
#define strcasecmp  stricmp
#endif

#include "utils.h"

#define REGULARIZED_QUAD 1
#define MIXED_QUAD       2

#define MAX(A,B)  (((A) < (B)) ? (B) : (A))
#define NINT(A)   (((A) < 0)   ? (int)(A-0.5) : (int)(A+0.5))

/*                     Local functions                    */

// Return the desired scale factor "delta" for a given spacing "ds" evaluated at point "epi" along
// interval "I" - Used when only a single end of an edge has a desired spacing - Vinokur 1980
static double eqn_stretchingFactorSingleSided(double delta, double inputVars[]) {

    double value;
    double epi = inputVars[0];
    double I   = inputVars[1];
    double ds  = inputVars[2];

    // Derivative of of 1 + tanh(delta(epi/I - 1))/tanh(delta)
    value = -1.0*(tanh(delta*(epi*I + 1))*(tanh(delta)*tanh(delta) - 1)) / (tanh(delta)*tanh(delta)) +
            ((tanh(delta*(epi*I + 1))*tanh(delta*(epi*I + 1)) - 1)*(epi*I + 1))/tanh(delta);

    return value - ds;
}

// Return the desired scale factor "delta" for a given "B" - Used when both ends of edge have a
// desired spacing - Vinokur 1980
static double eqn_stretchingFactorDoubleSided(double delta, double inputVars[]) {

    double value;
    double B  = inputVars[0];

    value = sinh(delta)/delta;

    return value - B;
}

// Find the root using the Bi-Section method
static double root_BisectionMethod( double (*f)(double, double *),
        double lowerBnd,
        double upperBnd,
        double inputVars[]){

    // BiSection variables
    double epsilon = 1E-6;

    double midBnd = 0.0, midVal = 0.0, upperVal = 0.0;

    do {
        midBnd = (lowerBnd + upperBnd)/2;

        upperVal = (*f)(upperBnd, inputVars);
        midVal   = (*f)(midBnd, inputVars);

        if (midVal == 0.0) {
            upperBnd = midBnd;
            break;
        }

        if (upperVal * midVal >= 0) {
            upperBnd = midBnd;

        } else {
            lowerBnd = midBnd;
        }

    } while ((upperBnd - lowerBnd) > epsilon);

    return upperBnd;
}

// Modify edge vertex counts to maximize TFI (transfinite interpolation)
static int mesh_edgeVertexTFI( ego ebody,
                               int *points,   // (in) 1-based vertex count in each edge, (out) vertex countt maximize TIF
                               int *userSet ) // (in) point counts set by users
{
    int status = 0; // Function return status

    // Body entities
    int numEdge = 0, numFace = 0;
    ego *eedges=NULL, *efaces=NULL;

    int atype, alen; // EGADS return variables
    const int    *ints;
    const double *reals;
    const char *string;

    int     nchange = 0, oclass, mtype, nchild, nchild2, *senses;

    // Edge point distributions
    int    *isouth=NULL, *ieast=NULL, *inorth=NULL, *iwest=NULL;
    double  range[2], data[4], eval[18], eval2[18];
    ego     eref, *echilds, *echilds2, eloop;

    int    i, j, k, face; // Indexing
    double scale;

    status = EG_attributeRet(ebody, ".qParams", &atype, &alen, &ints, &reals, &string);
    if (status == EGADS_SUCCESS && (atype != ATTRREAL || (atype == ATTRREAL && reals[0] <= 0 ))) {
        printf("\tTFI quading on all faces disabled with .qParams attribute on the body\n");
        return CAPS_SUCCESS;
    }

    status = EG_getBodyTopos(ebody, NULL, EDGE, &numEdge, &eedges);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(ebody, NULL, FACE, &numFace, &efaces);
    if (status < EGADS_SUCCESS) goto cleanup;

    // make arrays for "opposite" sides of four-sided Faces (with only one loop)
    isouth = (int *) EG_alloc((numFace+1)*sizeof(int));
    ieast  = (int *) EG_alloc((numFace+1)*sizeof(int));
    inorth = (int *) EG_alloc((numFace+1)*sizeof(int));
    iwest  = (int *) EG_alloc((numFace+1)*sizeof(int));

    if (isouth == NULL ||
        ieast  == NULL  ||
        inorth == NULL ||
        iwest  == NULL   ) { status = EGADS_MALLOC; goto cleanup; }

    for (i = 1; i <= numFace; i++) {
        isouth[i] = 0;
        ieast [i] = 0;
        inorth[i] = 0;
        iwest [i] = 0;

        // check if quading is disabled with .qParams
        status = EG_attributeRet(efaces[i-1], ".qParams", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && (atype != ATTRREAL || (atype == ATTRREAL && reals[0] <= 0 ))) {
            printf("\tFace %d TFI quading disabled with attribute .qParams\n", i);
            continue;
        }

        // quading only works with one loop
        status = EG_getTopology(efaces[i-1], &eref, &oclass, &mtype, data, &nchild, &echilds, &senses);
        if (status < EGADS_SUCCESS) goto cleanup;

        if (nchild != 1) continue;

        // quading only works if the loop has 4 edges
        eloop = echilds[0];
        status = EG_getTopology(eloop, &eref, &oclass, &mtype, data, &nchild, &echilds, &senses);
        if (status < EGADS_SUCCESS) goto cleanup;

        if (nchild != 4) continue;

        // Check to see if two "straight" edges next to each other are parallel - Don't Quad if so
        for (j = 0; j < 4; j++) {

            status = EG_getTopology(echilds[j], &eref, &oclass, &mtype, data, &nchild2, &echilds2, &senses);
            if (mtype == DEGENERATE) { status = EGADS_DEGEN; break; }
            if (status < EGADS_SUCCESS) goto cleanup;

            if (j < 3) k = j+1;
            else k = 0;

            status = EG_getTopology(echilds[k], &eref, &oclass, &mtype, range, &nchild2, &echilds2, &senses);
            if (mtype == DEGENERATE) { status = EGADS_DEGEN; break; }
            if (status < EGADS_SUCCESS) goto cleanup;

            status = EG_evaluate(echilds[j], data, eval);
            if (status < EGADS_SUCCESS) goto cleanup;

            status = EG_evaluate(echilds[k], range, eval2);
            if (status < EGADS_SUCCESS) goto cleanup;

            scale = dot_DoubleVal(&eval[3], &eval[3]);
            eval[3] /= scale;
            eval[4] /= scale;
            eval[5] /= scale;

            scale = dot_DoubleVal(&eval2[3], &eval2[3]);
            eval2[3] /= scale;
            eval2[4] /= scale;
            eval2[5] /= scale;

            if (fabs(fabs(dot_DoubleVal(&eval[3], &eval2[3])) - 1) < 1E-6) {
                status = EGADS_OUTSIDE;
                break;
            }
        }

        if (status == EGADS_OUTSIDE) {
            printf("\tFace %d has parallel edges - no TFI quading\n", i);
            continue;
        }

        if (status == EGADS_DEGEN) {
            printf("\tFace %d has a degenerate edge - no TFI quading\n", i);
            continue;
        }

        isouth[i] = status = EG_indexBodyTopo(ebody, echilds[0]);
        if (status < EGADS_SUCCESS) goto cleanup;

        ieast[i]  = status = EG_indexBodyTopo(ebody, echilds[1]);
        if (status < EGADS_SUCCESS) goto cleanup;

        inorth[i] = status = EG_indexBodyTopo(ebody, echilds[2]);
        if (status < EGADS_SUCCESS) goto cleanup;

        iwest[i]  = status = EG_indexBodyTopo(ebody, echilds[3]);
        if (status < EGADS_SUCCESS) goto cleanup;
    }

    // make "opposite" sides of four-sided Faces (with only one loop) match
    for (i = 0; i < 10*numFace; i++) {
        nchange = 0;

        for (face = 1; face <= numFace; face++) {
            if (isouth[face] <= 0 || ieast[face] <= 0 ||
                inorth[face] <= 0 || iwest[face] <= 0   ) continue;

            // equate west/east biased based on what is specified by the user
                   if (userSet[iwest[face]] == 0 && userSet[ieast[face]] == 1) {
                        points[iwest[face]] = points[ieast[face]];
                       userSet[iwest[face]] = 1;
                nchange++;
            } else if (userSet[ieast[face]] == 0 && userSet[iwest[face]] == 1) {
                        points[ieast[face]] =        points[iwest[face]];
                       userSet[ieast[face]] = 1;
                nchange++;
            }

            // equate north/south biased based on what is specified by the user
                   if (userSet[isouth[face]] == 0 && userSet[inorth[face]] == 1) {
                        points[isouth[face]] =        points[inorth[face]];
                       userSet[isouth[face]] = 1;
                nchange++;
            } else if (userSet[inorth[face]] == 0 && userSet[isouth[face]] == 1) {
                        points[inorth[face]] =        points[isouth[face]];
                       userSet[inorth[face]] = 1;
                nchange++;
            }

            // equate west/east based on maximum count
                   if ( points[iwest[face]] <   points[ieast[face]] &&
                       userSet[iwest[face]] == userSet[ieast[face]] ) {
                        points[iwest[face]] =   points[ieast[face]];
                nchange++;

            } else if ( points[ieast[face]] <   points[iwest[face]] &&
                       userSet[ieast[face]] == userSet[iwest[face]]) {
                        points[ieast[face]] =   points[iwest[face]];
                nchange++;
            }

            // equate north/south based on maximum count
                   if ( points[isouth[face]] <   points[inorth[face]] &&
                       userSet[isouth[face]] == userSet[inorth[face]]) {
                        points[isouth[face]] =   points[inorth[face]];
                nchange++;
            } else if ( points[inorth[face]] <   points[isouth[face]] &&
                       userSet[inorth[face]] == userSet[isouth[face]]) {
                        points[inorth[face]] =   points[isouth[face]];
                nchange++;
            }
        }
        if (nchange == 0) break;
    }
    if (nchange > 0) {
        printf("\tExceeded number of tries making \"opposite\" sides of four-sided Faces (with only one loop) match\n");
    }


    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_edgeVertexTFI, status = %d\n", status);

        EG_free(isouth);
        EG_free(ieast);
        EG_free(inorth);
        EG_free(iwest);

        EG_free(eedges);
        EG_free(efaces);

        return status;
}

/*                     API functions                    */


int mesh_addTess2Dbc(meshStruct *surfaceMesh, mapAttrToIndexStruct *attrMap)
{
    /*
     * Extracts boundary regions for a 2D egads tessellation mesh
     */

    int status; // Function return

    int i, edge, face, elementIndex; // Indexing

    // EGADS function return variables
    int           plen = 0, tlen = 0, tessStatus = 0;
    const int    *tris = NULL, *triNeighbor = NULL, *ptype = NULL, *pindex = NULL;
    const double *points = NULL, *uv = NULL;

    // Totals
    int numFace = 0, numEdge = 0, numPoints = 0, numEdgeSeg = 0;

    // EGO objects
    ego body, *faces = NULL, *edges = NULL;

    int gID = 0; // Global ID

    int  cID = 0; // Component marker

    const char *groupName = NULL;

    ego tess = surfaceMesh->bodyTessMap.egadsTess;

    // Check arrays
    if (surfaceMesh->meshQuickRef.numLine != 0) {
        printf(" Error: Surface mesh already contains line elements!\n" );
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    // Get body from tessellation and total number of global points
    status = EG_statusTessBody(tess, &body, &tessStatus, &numPoints);
    if (tessStatus != 1) { status = EGADS_TESSTATE; goto cleanup; }
    if (status != EGADS_SUCCESS) goto cleanup;

    if (numPoints != surfaceMesh->numNode) {
        printf(" Error:  surfaceMesh does not match EGADS tessellation!\n" );
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    // Get faces and edges so we can check for attributes on them
    status = EG_getBodyTopos(body, NULL, FACE, &numFace, &faces);
    if (status != EGADS_SUCCESS) {
        printf(" Error: mesh_addTess2Dbc = %d!\n", status);
        goto cleanup;
    }

    if (numFace != 1) {
        printf(" Error: mesh_addTess2Dbc body has more than 1 face!\n" );
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    status = EG_getBodyTopos(body, NULL, EDGE, &numEdge, &edges);
    if (status != EGADS_SUCCESS) {
        printf(" Error: mesh_addTess2Dbc = %d!\n", status);
        goto cleanup;
    }

    numEdgeSeg = 0;
    for (face = 1; face <= numFace; face++) {
        status = EG_getTessFace(tess, face, &plen, &points, &uv, &ptype, &pindex,
                                &tlen, &tris, &triNeighbor);
        if (status != EGADS_SUCCESS) {
            printf(" Face %d: EG_getTessFace status = %d (bodyTessellation)!\n", face, status);
            goto cleanup;
        }

        for (edge = 1; edge <= numEdge; edge++) {

            status = EG_getTessEdge(tess, edge, &plen, &points, &uv);
            if (status != EGADS_SUCCESS) goto cleanup;

            numEdgeSeg += plen-1;
        }
    }

    // Get boundary edge information
    elementIndex = surfaceMesh->numElement;
    surfaceMesh->numElement += numEdgeSeg;

    surfaceMesh->element = (meshElementStruct *) EG_reall(surfaceMesh->element, surfaceMesh->numElement*sizeof(meshElementStruct));
    if (surfaceMesh->element == NULL) { status = EGADS_MALLOC; goto cleanup; }

    // Fill up boundary edge list
    for (edge = 1; edge <= numEdge; edge++) {

        status = retrieve_CAPSGroupAttr(edges[edge-1], &groupName);
        if (status == CAPS_SUCCESS) {

          status = get_mapAttrToIndexIndex(attrMap, groupName, &cID);
          if (status != CAPS_SUCCESS) {
              printf("Error: Unable to retrieve boundary index from capsGroup %s\n", groupName);
              goto cleanup;
          }

          status = retrieve_CAPSIgnoreAttr(edges[edge-1], &groupName);
          if (status == CAPS_SUCCESS) {
              printf("\tBoth capsGroup and capsIgnore attribute found for edge - %d!!\n", edge);
              printf("Edge attributes are:\n");
              print_AllAttr( edges[edge-1] );
              status = CAPS_BADVALUE;
              goto cleanup;
          }
        } else {

            status = retrieve_CAPSIgnoreAttr(edges[edge-1], &groupName);
            if (status == CAPS_SUCCESS) {
                printf("\tcapsIgnore attribute found for edge - %d!!\n", edge);
                cID = -1;
            } else {
                printf("Error: No capsGroup/capsIgnore attribute found on edge %d of face %d, unable to assign a boundary index value\n", edge, numFace);
                printf("Available attributes are:\n");
                print_AllAttr( edges[edge-1] );
                goto cleanup;
            }
        }

        status = EG_getTessEdge(tess, edge, &plen, &points, &uv);
        if (status != EGADS_SUCCESS) goto cleanup;

        for (i = 0; i < plen-1; i++) {

            status = initiate_meshElementStruct(&surfaceMesh->element[elementIndex], surfaceMesh->analysisType);
            if (status != CAPS_SUCCESS) goto cleanup;

            surfaceMesh->element[elementIndex].elementType = Line;
            surfaceMesh->element[elementIndex].elementID = elementIndex + 1;

            status = mesh_allocMeshElementConnectivity(&surfaceMesh->element[elementIndex]);
            if (status != CAPS_SUCCESS) goto cleanup;

            status = EG_localToGlobal(tess, -edge, i+1, &gID);
            if (status != EGADS_SUCCESS) goto cleanup;

            surfaceMesh->element[elementIndex].connectivity[0] = gID;

            status = EG_localToGlobal(tess, -edge, i+2, &gID);
            if (status != EGADS_SUCCESS) goto cleanup;

            surfaceMesh->element[elementIndex].connectivity[1] = gID;

            surfaceMesh->element[elementIndex].markerID = cID;
            surfaceMesh->element[elementIndex].topoIndex = edge;

            elementIndex += 1;
        }
    }

    // fill the quick ref list
    status = mesh_fillQuickRefList(surfaceMesh);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) {
            printf("Error: Premature exit in mesh_addTess2Dbc status = %d\n", status);
        }

        EG_free(faces);
        EG_free(edges);

        return status;
}


int mesh_bodyTessellation(ego tess, mapAttrToIndexStruct *attrMap,
                          int *numNodes, double *xyzCoord[],
                          int *numTriFace, int *triFaceConn[], int *triFaceCompID[], int *triFaceTopoID[],
                          int *numBndEdge, int *bndEdgeConn[], int *bndEdgeCompID[], int *bndEdgeTopoID[],
                          int *tessFaceQuadMap,
                          int *numQuadFace, int *quadFaceConn[], int *quadFaceCompID[], int *quadFaceTopoID[])
{

    /*
     * Calculates and returns a complete Body tessellation
     *
     * 	In : tess	 - ego of a body tessellation
     *       attrMap - pointer to an attribute to index mapping for component
     *       tessFaceQuadMap - List to keeping track of whether or not the tessObj has quads that
     *                         have been split into tris. size = [numFace on Tess]. Order of index is assumed
     *                         to correspond to the order of faces.
     *                         In general if the quads have been split they were added to the end
     *                         of the tri-list in the face tessellation. CAN be NULL.
     *
     *  Out :
     *       numNodes   - number of vertices (xyz coordinates)
     *       xyzCoord   - coordinates, 3*numPoints in length -- freeable
     *       numTriFace - number of triangles
     *       triFaceConn- triangle connectivity, 3*numTriFace in length -- freeable
     *       triFaceCompID - triangle boundary marking index found in attrMap, numTriFace in length --freeable
     *       triFaceTopoID - triangle FACE topology index (1-based), numTriFace in length --freeable
     *       numBndEdge - number of boundary segments if body only has 1 face - 2D meshing
     *       bndEdgeConn- edge segment connectivity, 2*numBndEdge in length -- freeable
     *       bndEdgeCompID - edge segment boundary marking index found in attrMap, numBndEdge in length --freeable
     *       bndEdgeTopoID - EDGE topology index (1-based), numBndEdge in length --freeable
     *
     *       numQuadFace - number of quadrilaterals - tessFaceQuadMap be none NULL for this information to be extracted
     *       quadFaceConn - quadrilateral connectivity, 4*numQuadFace in length -- freable
     *       quadFaceCompID - quadrilateral boundary marking index found in attrMap, numQuadFace in length --freeable
     *       quadFaceTopoID - quadrilateral FACE topology index (1-based), numQuadFace in length --freeable
     */

    int status; // Function return

    int i, j, edge, face, offSetIndex; // Indexing

    // EGADS function return variables
    int           plen = 0, tlen = 0, qlen = 0, tessStatus = 0, pointType = 0, pointIndex = 0;
    const int    *tris = NULL, *triNeighbor = NULL, *ptype = NULL, *pindex = NULL;
    const double *points = NULL, *uv = NULL;

    int     *triConn = NULL, *quadConn = NULL;
    double  *xyzs = NULL;

    // Totals
    int numFace = 0, numEdge = 0, numPoints = 0, numTri =0, numQuad = 0, numEdgeSeg = 0;

    // EGO objects
    ego body, *faces = NULL, *edges = NULL;

    int gID = 0; // Global ID

    int  cID = 0, *triCompID = NULL, *quadCompID = NULL; // Component marker
    int  *triTopoID = NULL, *quadTopoID = NULL; // Topology index

    const char *groupName = NULL;

    *numNodes  = 0;
    *numTriFace = 0;
    *xyzCoord  = NULL;
    *triFaceConn = NULL;
    *triFaceCompID = NULL;
    *triFaceTopoID = NULL;

    *numBndEdge = 0;
    *bndEdgeConn = NULL;
    *bndEdgeCompID = NULL;
    *bndEdgeTopoID = NULL;

    *numQuadFace = 0;
    *quadFaceConn = NULL;
    *quadFaceCompID = NULL;
    *quadFaceTopoID = NULL;

    // Get body from tessellation and total number of global points
    status = EG_statusTessBody(tess, &body, &tessStatus, &numPoints);
    if (tessStatus != 1) { status = EGADS_TESSTATE; goto cleanup; }
    if (status != EGADS_SUCCESS) goto cleanup;

    // Allocate memory associated with the nodes

    xyzs = (double *) EG_alloc(3*numPoints*sizeof(double));
    if (xyzs == NULL) {
        printf(" Error: Can not allocate XYZs (bodyTessellation)!\n");
        status = EGADS_MALLOC;
        goto cleanup;
    }

    //  and retrieve the nodes
    for ( j = 0; j < numPoints; j++ ) {
        status = EG_getGlobal(tess, j+1, &pointType, &pointIndex, xyzs + 3*j);
        if (status != EGADS_SUCCESS) goto cleanup;
    }

    // Get faces and edges so we can check for attributes on them
    status = EG_getBodyTopos(body, NULL, FACE, &numFace, &faces);
    if (status != EGADS_SUCCESS) {
        printf(" Error: EG_getBodyTopos = %d!\n", status);
        goto cleanup;
    }

    if (numFace == 1) {
        status = EG_getBodyTopos(body, NULL, EDGE, &numEdge, &edges);
        if (status != EGADS_SUCCESS) {
            printf(" Error: EG_getBodyTopos = %d!\n", status);
            goto cleanup;
        }
    }

    numTri = numEdgeSeg = 0;
    for (face = 1; face <= numFace; face++) {
        status = EG_getTessFace(tess, face, &plen, &points, &uv, &ptype, &pindex,
                                &tlen, &tris, &triNeighbor);

        if (status != EGADS_SUCCESS) {
            printf(" Face %d: EG_getTessFace status = %d (bodyTessellation)!\n", face, status);
            goto cleanup;
        }

        numTri += tlen;

#if 0
        if (numFace == 1) {
            for (edge = 1; edge <= numEdge; edge++) {

                status = EG_getTessEdge(tess, edge, &plen, &points, &uv);
                if (status != EGADS_SUCCESS) goto cleanup;

                numEdgeSeg += plen-1;
            }
        }
#endif
    }

    // WIREBODY
    if (numFace == 0) {
        for (edge = 1; edge <= numEdge; edge++) {

            status = EG_getTessEdge(tess, edge, &plen, &points, &uv);
            if (status != EGADS_SUCCESS) goto cleanup;

            numEdgeSeg += plen-1;
        }
    }

    // Do we have split quads?
    if (tessFaceQuadMap != NULL) {

        // How many?
        numQuad = 0;
        for (face = 0; face < numFace; face++)
          numQuad += tessFaceQuadMap[face];

        // subtract off the split quads from the total tri count
        numTri -= 2*numQuad;
    }

    // Allocate memory associated with triangles

    if (numTri != 0) {
        triConn = (int *) EG_alloc(3*numTri*sizeof(int));
        if (triConn == NULL) {
            printf(" Error: Can not allocate triangles (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }

        triCompID = (int *) EG_alloc(numTri*sizeof(int));
        if (triCompID == NULL) {
            printf(" Error: Can not allocate components (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }

        triTopoID = (int *) EG_alloc(numTri*sizeof(int));
        if (triTopoID == NULL) {
            printf(" Error: Can not allocate topology (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }
    }

    // Allocate memory associated with quads

    if (numQuad != 0) {
        quadConn = (int *) EG_alloc(4*numQuad*sizeof(int));
        if (quadConn == NULL) {
            printf(" Error: Can not allocate quadrilaterals (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }

        quadCompID = (int *) EG_alloc(numQuad*sizeof(int));
        if (quadCompID == NULL) {
            printf(" Error: Can not allocate quad components (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }

        quadTopoID = (int *) EG_alloc(numQuad*sizeof(int));
        if (quadTopoID == NULL) {
            printf(" Error: Can not allocate quad topology (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }
    }

    //Set default value for compID
    for (i = 0; i < numTri; i++) triCompID[i] = 1;
    for (i = 0; i < numQuad; i++) quadCompID[i] = 1;

    // Loop through faces and build global xyz and connectivity
    numTri = 0;
    numQuad = 0;
    for (face = 1; face <= numFace; face++) {

        // Look for component/boundary ID for attribute mapper based on capsGroup

        status = retrieve_CAPSGroupAttr(faces[face-1], &groupName);
        if (status == CAPS_SUCCESS) {

            status = get_mapAttrToIndexIndex(attrMap, groupName, &cID);
            if (status != CAPS_SUCCESS) {
              printf("Error: Unable to retrieve boundary index from capsGroup %s\n", groupName);
              goto cleanup;
            }
            status = retrieve_CAPSIgnoreAttr(faces[face-1], &groupName);
            if (status == CAPS_SUCCESS) {
                printf("\tBoth capsGroup and capsIgnore attribute found for face - %d!!\n", face);
                printf("Face attributes are:\n");
                print_AllAttr( faces[face-1] );
                status = CAPS_BADVALUE;
                goto cleanup;
            }
        } else {

            status = retrieve_CAPSIgnoreAttr(faces[face-1], &groupName);
            if (status == CAPS_SUCCESS) {
                printf("\tcapsIgnore attribute found for face - %d!!\n", face);
                cID = -1;
            } else {
                printf("Error: No capsGroup/capsIgnore attribute found on Face %d, unable to assign a boundary index value\n", face);
                printf("Available attributes are:\n");
                print_AllAttr( faces[face-1] );
                goto cleanup;
            }
        }

        status = EG_getTessFace(tess, face, &plen, &points, &uv, &ptype, &pindex,
                                &tlen, &tris, &triNeighbor);
        if (status != EGADS_SUCCESS) continue;

        // Do we possibly have quads?
        if (tessFaceQuadMap != NULL) {

            qlen = tessFaceQuadMap[face-1];
            tlen -= 2*qlen; // Get the number of "true" triangles

            // offset any triangles on the face
            offSetIndex = 3*tlen;

            // Get quad connectivity in global sense
            for (i = 0; i < qlen; i++){
                status = EG_localToGlobal(tess, face, tris[6*i + offSetIndex + 0], &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                quadConn[4*numQuad + 0] = gID;

                status = EG_localToGlobal(tess, face, tris[6*i + offSetIndex + 1], &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                quadConn[4*numQuad + 1] = gID;

                status = EG_localToGlobal(tess, face, tris[6*i + offSetIndex + 2], &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                quadConn[4*numQuad + 2] = gID;

                status = EG_localToGlobal(tess, face, tris[6*i + offSetIndex + 5], &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                quadConn[4*numQuad + 3] = gID;

                quadCompID[numQuad] = cID;
                quadTopoID[numQuad] = face;

                numQuad +=1;
            }
        }

        // Get triangle connectivity in global sense
        for (i = 0; i < tlen; i++) {

            status = EG_localToGlobal(tess, face, tris[3*i + 0], &gID);
            if (status != EGADS_SUCCESS) goto cleanup;

            triConn[3*numTri + 0] = gID;

            status = EG_localToGlobal(tess, face, tris[3*i + 1], &gID);
            if (status != EGADS_SUCCESS) goto cleanup;

            triConn[3*numTri + 1] = gID;

            status = EG_localToGlobal(tess, face, tris[3*i + 2], &gID);
            if (status != EGADS_SUCCESS) goto cleanup;

            triConn[3*numTri + 2] = gID;

            triCompID[numTri] = cID;
            triTopoID[numTri] = face;

            numTri += 1;
        }
    }

    // Resize triConn and compId (triFace) if we ended up have some quad faces
    if (numQuad != 0 && numTri != 0) {

        triConn = (int *) EG_reall(triConn, 3*numTri*sizeof(int));
        if (triConn == NULL) { status = EGADS_MALLOC; goto cleanup; }

        triCompID = (int *) EG_reall(triCompID, numTri*sizeof(int));
        if (triCompID == NULL) { status = EGADS_MALLOC; goto cleanup; }

        triTopoID = (int *) EG_reall(triTopoID, numTri*sizeof(int));
        if (triTopoID == NULL) { status = EGADS_MALLOC; goto cleanup; }
    }

    // Get boundary edge information there is no face (or 1 face maybe?)
    if ( numFace == 0 && numEdge != 0) {

        *numBndEdge = numEdgeSeg;

        *bndEdgeConn = (int *) EG_alloc(2*numEdgeSeg*sizeof(int));
        if (bndEdgeConn == NULL) {
            printf(" Error: Can not allocate components (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }

        *bndEdgeCompID = (int *) EG_alloc(numEdgeSeg*sizeof(int));
        if (*bndEdgeCompID == NULL) {
            printf(" Error: Can not allocate components (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }

        *bndEdgeTopoID = (int *) EG_alloc(numEdgeSeg*sizeof(int));
        if (*bndEdgeTopoID == NULL) {
            printf(" Error: Can not allocate topology (bodyTessellation)!\n");
            status = EGADS_MALLOC;
            goto cleanup;
        }

        numEdgeSeg = 0;
        // Fill up boundary edge list
        for (edge = 1; edge <= numEdge; edge++) {

            status = retrieve_CAPSGroupAttr(edges[edge-1], &groupName);
            if (status == CAPS_SUCCESS) {

              status = get_mapAttrToIndexIndex(attrMap, groupName, &cID);
              if (status != CAPS_SUCCESS) {
                  printf("Error: Unable to retrieve boundary index from capsGroup %s\n", groupName);
                  goto cleanup;
              }

              status = retrieve_CAPSIgnoreAttr(edges[edge-1], &groupName);
              if (status == CAPS_SUCCESS) {
                  printf("\tBoth capsGroup and capsIgnore attribute found for edge - %d!!\n", edge);
                  printf("Edge attributes are:\n");
                  print_AllAttr( edges[edge-1] );
                  status = CAPS_BADVALUE;
                  goto cleanup;
              }
            } else {

                status = retrieve_CAPSIgnoreAttr(edges[edge-1], &groupName);
                if (status == CAPS_SUCCESS) {
                    printf("\tcapsIgnore attribute found for edge - %d!!\n", edge);
                    cID = -1;
                } else {
                    printf("Error: No capsGroup/capsIgnore attribute found on edge %d of face %d, unable to assign a boundary index value\n", edge, numFace);
                    printf("Available attributes are:\n");
                    print_AllAttr( edges[edge-1] );
                    goto cleanup;
                }
            }

            status = EG_getTessEdge(tess, edge, &plen, &points, &uv);
            if (status != EGADS_SUCCESS) goto cleanup;

            for (i = 0; i < plen -1; i++) {

                status = EG_localToGlobal(tess, -edge, i+1, &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                (*bndEdgeConn)[2*numEdgeSeg + 0] = gID;

                status = EG_localToGlobal(tess, -edge, i+2, &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                (*bndEdgeConn)[2*numEdgeSeg + 1] = gID;

                (*bndEdgeCompID)[numEdgeSeg] = cID;
                (*bndEdgeTopoID)[numEdgeSeg] = edge;

                numEdgeSeg += 1;
            }
        }
    }

    *numNodes     = numPoints;
    *xyzCoord      = xyzs;
    *numTriFace    = numTri;
    *triFaceConn   = triConn;
    *triFaceCompID = triCompID;
    *triFaceTopoID = triTopoID;

    *numQuadFace = numQuad;
    *quadFaceConn = quadConn;
    *quadFaceCompID = quadCompID;
    *quadFaceTopoID = quadTopoID;

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) {
            printf("Error: Premature exit in mesh_bodyTessellation status = %d\n", status);

            EG_free(xyzs);
            EG_free(triConn);
            EG_free(triCompID);
            EG_free(triTopoID);

            EG_free(quadConn);
            EG_free(quadCompID);
            EG_free(quadTopoID);

            *numBndEdge = 0;

            EG_free(*bndEdgeConn);
            *bndEdgeConn = NULL;

            EG_free(*bndEdgeCompID);
            *bndEdgeCompID = NULL;

            EG_free(*bndEdgeTopoID);
            *bndEdgeTopoID = NULL;
        }

        EG_free(faces);
        EG_free(edges);

        return status;
}


// Modify the EGADS body tessellation based on given inputs


// Create a surface mesh in meshStruct format using the EGADS body tessellation
int mesh_surfaceMeshEGADSTess(mapAttrToIndexStruct *attrMap, meshStruct *surfMesh) {

    int status; // Function return integer

    int i, elementIndex = 0; // Indexing

    // EG_statusTessBody
    ego body;
    int tessStatus = 1, numPoints = 0;
    double coord[3];

    int numNode;
    double *xyz = NULL;
    int numNodeElem = 0;
    int numTriFace;
    int *localTriFaceList = NULL;
    int *triFaceMarkList = NULL;
    int *triFaceTopoList = NULL;
    int numBoundaryEdge;
    int *localBoundaryEdgeList = NULL;
    int *boundaryEdgeMarkList = NULL;
    int *boundaryEdgeTopoList = NULL;
    int numQuadFace;
    int *localQuadFaceList = NULL;
    int *quadFaceMarkList = NULL;
    int *quadFaceTopoList = NULL;

    status = mesh_bodyTessellation(surfMesh->bodyTessMap.egadsTess,
                                   attrMap,
                                   &numNode,
                                   &xyz,
                                   &numTriFace,
                                   &localTriFaceList,
                                   &triFaceMarkList,
                                   &triFaceTopoList,
                                   &numBoundaryEdge,
                                   &localBoundaryEdgeList,
                                   &boundaryEdgeMarkList,
                                   &boundaryEdgeTopoList,
                                   surfMesh->bodyTessMap.tessFaceQuadMap,
                                   &numQuadFace,
                                   &localQuadFaceList,
                                   &quadFaceMarkList,
                                   &quadFaceTopoList);
    if (status != CAPS_SUCCESS) goto cleanup;

    if (numBoundaryEdge != 0) surfMesh->meshType = Surface2DMesh;
    else surfMesh->meshType = SurfaceMesh;

    // set the analysis type of the mesh
    surfMesh->analysisType = UnknownMeshAnalysis;

    // Cleanup Nodes and Elements
    if (surfMesh->node != NULL) {
        for (i = 0; i < surfMesh->numNode; i++) {

            status = destroy_meshNodeStruct(&surfMesh->node[i]);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        EG_free(surfMesh->node);
        surfMesh->node = NULL;
    }

    if (surfMesh->element != NULL) {
        for (i = 0;  i < surfMesh->numElement; i++) {

            status = destroy_meshElementStruct( &surfMesh->element[i]);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        EG_free(surfMesh->element);
        surfMesh->element = NULL;
    }

    // Cleanup mesh Quick reference guide
    status = destroy_meshQuickRefStruct(&surfMesh->meshQuickRef);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Allocate nodes
    surfMesh->numNode = numNode;

    surfMesh->node = (meshNodeStruct *) EG_alloc(surfMesh->numNode*sizeof(meshNodeStruct));
    if (surfMesh->node == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    // Initiate nodes  and set nodes
    for (i = 0; i < surfMesh->numNode; i++) {
        status = initiate_meshNodeStruct(&surfMesh->node[i], surfMesh->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;

        memcpy(surfMesh->node[i].xyz, xyz+3*i, 3*sizeof(double));

        surfMesh->node[i].nodeID = i+1;
    }

    // Get body from tessellation and total number of global points
    status = EG_statusTessBody(surfMesh->bodyTessMap.egadsTess, &body, &tessStatus, &numPoints);
    if (tessStatus != 1) { status = EGADS_TESSTATE; goto cleanup; }
    if (status != EGADS_SUCCESS) goto cleanup;

    if (aim_isNodeBody(body, coord) == CAPS_SUCCESS) {
        numNodeElem = 1;
    }

    // Allocate elements
    surfMesh->numElement = numNodeElem + numTriFace + numQuadFace + numBoundaryEdge;

    surfMesh->element = (meshElementStruct *) EG_alloc(surfMesh->numElement*sizeof(meshElementStruct));
    if (surfMesh->element == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    // Initiate elements
    for (i = 0; i < surfMesh->numElement; i++) {
        status = initiate_meshElementStruct(&surfMesh->element[i], surfMesh->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    elementIndex = 0;
    // Node
    for (i = 0; i < numNodeElem; i++) {

        surfMesh->element[elementIndex].elementType = Node;
        surfMesh->element[elementIndex].elementID = elementIndex + 1;

        status = mesh_allocMeshElementConnectivity(&surfMesh->element[elementIndex]);
        if (status != CAPS_SUCCESS) goto cleanup;

        surfMesh->element[elementIndex].connectivity[0] = i+1;

        surfMesh->element[elementIndex].markerID = 0;

        elementIndex += 1;
        surfMesh->numElement = elementIndex;
    }

    // Edges
    for (i = 0; i < numBoundaryEdge; i++) {

        surfMesh->element[elementIndex].elementType = Line;
        surfMesh->element[elementIndex].elementID = elementIndex + 1;

        status = mesh_allocMeshElementConnectivity(&surfMesh->element[elementIndex]);
        if (status != CAPS_SUCCESS) goto cleanup;

        surfMesh->element[elementIndex].connectivity[0] = localBoundaryEdgeList[2*i+0];
        surfMesh->element[elementIndex].connectivity[1] = localBoundaryEdgeList[2*i+1];

        surfMesh->element[elementIndex].markerID  = boundaryEdgeMarkList[i];
        surfMesh->element[elementIndex].topoIndex = boundaryEdgeTopoList[i];

        elementIndex += 1;
        surfMesh->numElement = elementIndex;
    }

    // Triangles
    for (i = 0; i < numTriFace; i++) {
        surfMesh->element[elementIndex].elementType = Triangle;
        surfMesh->element[elementIndex].elementID = elementIndex + 1;

        status = mesh_allocMeshElementConnectivity(&surfMesh->element[elementIndex]);
        if (status != CAPS_SUCCESS) goto cleanup;

        surfMesh->element[elementIndex].connectivity[0] = localTriFaceList[3*i+0];
        surfMesh->element[elementIndex].connectivity[1] = localTriFaceList[3*i+1];
        surfMesh->element[elementIndex].connectivity[2] = localTriFaceList[3*i+2];

        surfMesh->element[elementIndex].markerID  = triFaceMarkList[i];
        surfMesh->element[elementIndex].topoIndex = triFaceTopoList[i];

        elementIndex += 1;
        surfMesh->numElement = elementIndex;
    }

    // Quads
    for (i = 0; i < numQuadFace; i++) {

        surfMesh->element[elementIndex].elementType = Quadrilateral;
        surfMesh->element[elementIndex].elementID = elementIndex + 1;

        status = mesh_allocMeshElementConnectivity(&surfMesh->element[elementIndex]);
        if (status != CAPS_SUCCESS) goto cleanup;

        surfMesh->element[elementIndex].connectivity[0] = localQuadFaceList[4*i+0];
        surfMesh->element[elementIndex].connectivity[1] = localQuadFaceList[4*i+1];
        surfMesh->element[elementIndex].connectivity[2] = localQuadFaceList[4*i+2];
        surfMesh->element[elementIndex].connectivity[3] = localQuadFaceList[4*i+3];

        surfMesh->element[elementIndex].markerID  = quadFaceMarkList[i];
        surfMesh->element[elementIndex].topoIndex = quadFaceTopoList[i];

        elementIndex += 1;
        surfMesh->numElement = elementIndex;
    }

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_surfaceMeshEGADSTess, status = %d\n", status);

        EG_free(xyz);
        EG_free(localTriFaceList);
        EG_free(triFaceMarkList);
        EG_free(triFaceTopoList);
        EG_free(localBoundaryEdgeList);
        EG_free(boundaryEdgeMarkList);
        EG_free(boundaryEdgeTopoList);
        EG_free(localQuadFaceList);
        EG_free(quadFaceMarkList);
        EG_free(quadFaceTopoList);

        return status;
}

// Create a surface mesh in meshStruct format using the EGADS body object
int mesh_surfaceMeshEGADSBody(ego body,
                              double refLen,
                              double tessParams[3],
                              int quadMesh,
                              mapAttrToIndexStruct *attrMap,
                              meshStruct *surfMesh) {

    int status; // Function return integer

    // Meshing related variables
    int       isNodeBody = (int)false;
    double    box[6], params[3];
    egTessel *btess;

    // Quading variables
    int face, numFace, numPoint, numPatch;
    ego tess, *faces = NULL;
    double coord[3];

    int           plen = 0, tlen = 0;
    const double *points = NULL, *uv = NULL;
    const int    *ptype = NULL, *pindex = NULL, *tris = NULL, *triNeighbor = NULL;

    // Get Tessellation
    printf("\tTessellating body\n");

    if (refLen <= 0) {

      // Get bounding box for the body
      status = EG_getBoundingBox(body, box);
      if (status != EGADS_SUCCESS) {
        printf(" EG_getBoundingBox = %d\n\n", status);
        return status;
      }

        // use the body size if refLen not given
        refLen =  sqrt((box[0]-box[3])*(box[0]-box[3]) +
                       (box[1]-box[4])*(box[1]-box[4]) +
                       (box[2]-box[5])*(box[2]-box[5]));

        // Double the size as trinagles will be halved to generate quads
        if (quadMesh == REGULARIZED_QUAD) refLen *= 2;
    }


    params[0] = tessParams[0]*refLen;
    params[1] = tessParams[1]*refLen;
    params[2] = tessParams[2];

    //printf("params[0] = %lf; params[1] = %lf; params[2] = %lf;\n", params[0], params[1], params[2]);

    status = EG_makeTessBody(body, params, &surfMesh->bodyTessMap.egadsTess);
    if (status != EGADS_SUCCESS) {
        printf(" EG_makeTessBody = %d\n", status);
        goto cleanup;
    }

    isNodeBody = aim_isNodeBody(body, coord);

    status = EG_getBodyTopos(body, NULL, FACE, &numFace, &faces);
    if (status != EGADS_SUCCESS) goto cleanup;

    // generate regularized quads
    if ( quadMesh == REGULARIZED_QUAD && (isNodeBody != CAPS_SUCCESS) && numFace > 0) {
        tess = surfMesh->bodyTessMap.egadsTess;
        status = EG_quadTess(tess, &surfMesh->bodyTessMap.egadsTess);
        if (status < EGADS_SUCCESS) {
            printf(" EG_quadTess = %d  -- reverting...\n", status);
            surfMesh->bodyTessMap.egadsTess = tess;
            quadMesh = MIXED_QUAD;
        } else {

            // mark all faces as quadded
            surfMesh->bodyTessMap.tessFaceQuadMap = (int *) EG_alloc(numFace*sizeof(int));
            if (surfMesh->bodyTessMap.tessFaceQuadMap == NULL) { status = EGADS_MALLOC; goto cleanup; }

            for (face = 0; face < numFace; face++) {

                surfMesh->bodyTessMap.tessFaceQuadMap[face] = 0;

                status = EG_getTessFace(surfMesh->bodyTessMap.egadsTess, face+1, &plen, &points, &uv, &ptype, &pindex,
                                        &tlen, &tris, &triNeighbor);
                if (status != EGADS_SUCCESS) goto cleanup;

                surfMesh->bodyTessMap.tessFaceQuadMap[face] = tlen/2;
            }
        }
    }

    // generate quad patches
    if ( quadMesh == MIXED_QUAD && (isNodeBody != CAPS_SUCCESS) && numFace > 0 ) {

        btess = (egTessel *) surfMesh->bodyTessMap.egadsTess->blind;

        surfMesh->bodyTessMap.numTessFace = numFace;
        EG_free(surfMesh->bodyTessMap.tessFaceQuadMap);

        surfMesh->bodyTessMap.tessFaceQuadMap = (int *) EG_alloc(numFace*sizeof(int));
        if (surfMesh->bodyTessMap.tessFaceQuadMap == NULL) { status = EGADS_MALLOC; goto cleanup; }

        // Set default to 0
        for (face = 0; face < numFace; face++) surfMesh->bodyTessMap.tessFaceQuadMap[face] = 0;

        // compute how many quad faces there are on each tfi face
        for (face = 0; face < numFace; face++) {
            if (btess->tess2d[face].tfi == 0) continue;

            status = EG_getTessFace(surfMesh->bodyTessMap.egadsTess, face+1, &plen, &points, &uv, &ptype, &pindex,
                                    &tlen, &tris, &triNeighbor);
            if (status != EGADS_SUCCESS) goto cleanup;

            surfMesh->bodyTessMap.tessFaceQuadMap[face] = tlen/2;

            // Also tag the face with the EG_makeQuads HACK. Really need to get rid of this!
            params[0] = 0.0;
            params[1] = 0.0;
            params[2] = 0.0;
            status = EG_makeQuads(surfMesh->bodyTessMap.egadsTess, params, face+1);
            if (status < EGADS_SUCCESS) {
                printf("Face = %d, failed to make quads\n", face);
                continue;
            }

            status = EG_getQuads(surfMesh->bodyTessMap.egadsTess, face+1, &numPoint, &points, &uv, &ptype, &pindex, &numPatch);
            if (status < EGADS_SUCCESS) goto cleanup;

            if (numPatch != 1) {
                status = CAPS_NOTIMPLEMENT;
                printf("EG_localToGlobal accidentally only works for a single quad patch! This needs to go away!\n");
                goto cleanup;
            }
        }
    }

    status = mesh_surfaceMeshEGADSTess(attrMap, surfMesh);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_surfaceMeshEGADSBody, status = %d\n", status);

        EG_free(faces);

        return status;

}

int mesh_modifyBodyTess(int numMeshProp,
                        meshSizingStruct meshProp[],
                        int minEdgePointGlobal,
                        int maxEdgePointGlobal,
                        int quadMesh,
                        double *refLen,
                        double tessParamGlobal[],
                        mapAttrToIndexStruct attrMap,
                        int numBody,
                        ego bodies[]) {

    int status; // Function return

    int i, j, bodyIndex, faceIndex, edgeIndex, attrIndex; // Indexing

    // Body parameters
    double box[6], boxMax[6] = {0};

    // Face parameters
    int numFace = 0;
    ego *faces = NULL;

    // Edge parameters
    int numEdge = 0; // Number of edges
    ego *edges = NULL; // EGADS edge object;

    int numEdgePoint = 0;

    edgeDistributionEnum edgeDistribution;
    double initialNodeSpacing[2] = {0.0, 0.0};

    int *points = NULL, *userSet = NULL;
    double *rPos = NULL;
    double params[3];
    ego tess = NULL;
    const double *xyzs = NULL, *ts = NULL;

    double A, B, I, U, stretchingFactor = 0.0, epi, inputVars[3]; // Tanh Helpers

    // Mesh attribute parameters
    const char *groupName = NULL;

    if (numBody == 0) {
        printf("Error: numBody == 0 in mesh_modifyBodyTess\n");
        return CAPS_SOURCEERR;
    }

    if (*refLen <= 0) {

        // Determine which body is the bounding body based on size
        for (bodyIndex = 0; bodyIndex < numBody; bodyIndex++) {

            // Get bounding box for the body
            status = EG_getBoundingBox(bodies[bodyIndex], box);
            if (status != EGADS_SUCCESS) {
              printf(" EG_getBoundingBox = %d\n\n", status);
              return status;
            }

            // Just copy the box coordinates on the first go around
            if (bodyIndex == 0) {

                memcpy(boxMax, box, sizeof(box));

            // Else compare with the "max" box size
            } else if ( boxMax[0] >= box[0] &&
                        boxMax[1] >= box[1] &&
                        boxMax[2] >= box[2] &&
                        boxMax[3] <= box[3] &&
                        boxMax[4] <= box[4] &&
                        boxMax[5] <= box[5]) {

                // If bigger copy coordinates
                memcpy(boxMax, box, sizeof(box));
            }
        }

        // use the body size from the largest bounding box
        *refLen = sqrt((boxMax[0]-boxMax[3])*(boxMax[0]-boxMax[3]) +
                       (boxMax[1]-boxMax[4])*(boxMax[1]-boxMax[4]) +
                       (boxMax[2]-boxMax[5])*(boxMax[2]-boxMax[5]));
    }

    // double the body size to generate a coarser triangulation which is then
    // sub-divided to create quads
    if (quadMesh == REGULARIZED_QUAD) (*refLen) *= 2;

    // Set body tessellation parameters
    for (bodyIndex = 0; bodyIndex < numBody; bodyIndex++) {

        status = EG_getBodyTopos(bodies[bodyIndex], NULL, EDGE, &numEdge, &edges);
        if (status !=  EGADS_SUCCESS) goto cleanup;

        status = EG_getBodyTopos(bodies[bodyIndex], NULL, FACE, &numFace, &faces);
        if (status !=  EGADS_SUCCESS) goto cleanup;

        // Flag array to track which edges have user specified spacings
        userSet = (int*) EG_alloc((numEdge+1)*sizeof(int));
        if (userSet == NULL) { status = EGADS_MALLOC; goto cleanup; }

        // Loop over edges for each body and set .tParam
        for (edgeIndex = 0; edgeIndex < numEdge; edgeIndex++) {

          userSet[edgeIndex+1] = 0;

          status = retrieve_CAPSGroupAttr(edges[edgeIndex], &groupName);
          if (status == EGADS_SUCCESS) {
              status = get_mapAttrToIndexIndex(&attrMap, groupName, &attrIndex);
              if (status == CAPS_SUCCESS) {

                  for (i = 0; i < numMeshProp; i++) {

                      if (meshProp[i].attrIndex == attrIndex) {
                          if (meshProp[i].useTessParams == (int) true) {

                              params[0] = meshProp[i].tessParams[0] * (*refLen);
                              params[1] = meshProp[i].tessParams[1] * (*refLen);
                              params[2] = meshProp[i].tessParams[2];

                              // .tParams uses minimum of .tParams and global tessParams.
                              // .tParam always overrides global tessParams
                              status = EG_attributeAdd(edges[edgeIndex],
                                                       ".tParam", ATTRREAL, 3, NULL, params, NULL);
                              if (status != EGADS_SUCCESS) goto cleanup;

                              userSet[edgeIndex+1] = 1;
                          }
                          break;
                      }
                  }
              }
          } else if (status != EGADS_SUCCESS && status != EGADS_NOTFOUND) {
              goto cleanup;
          }
        }

        // Loop over faces for each body and set .tParam
        for (faceIndex = 0; faceIndex < numFace; faceIndex++) {

            status = retrieve_CAPSGroupAttr(faces[faceIndex], &groupName);
            if (status == EGADS_SUCCESS) {
                status = get_mapAttrToIndexIndex(&attrMap, groupName, &attrIndex);
                if (status == CAPS_SUCCESS) {

                    for (i = 0; i < numMeshProp; i++) {

                        if (meshProp[i].attrIndex == attrIndex) {

                            if (meshProp[i].useTessParams == (int) true) {

                                params[0] = meshProp[i].tessParams[0] * (*refLen);
                                params[1] = meshProp[i].tessParams[1] * (*refLen);
                                params[2] = meshProp[i].tessParams[2];

                                // .tParams uses minimum of .tParams and global tessParams.
                                // .tParam always overrides global tessParams
                                status = EG_attributeAdd(faces[faceIndex],
                                                         ".tParam", ATTRREAL, 3, NULL, params, NULL);
                                if (status != EGADS_SUCCESS) goto cleanup;
                            }

                            break;
                        }
                    }
                }
            } else if (status != EGADS_SUCCESS && status != EGADS_NOTFOUND) {
                goto cleanup;
            }
        }

        // Negating the first parameter triggers EGADS to only put vertexes on edges
        params[0] = -tessParamGlobal[0]*(*refLen);
        params[1] =  tessParamGlobal[1]*(*refLen);
        params[2] =  tessParamGlobal[2];

        // Generate pure edge tessellation
        status = EG_makeTessBody(bodies[bodyIndex], params, &tess);
        if (status != EGADS_SUCCESS) goto cleanup;

        // Determine the nominal number of points along each Edge
        points = (int*) EG_alloc((numEdge+1)*sizeof(int));
        if (points == NULL) { status = EGADS_MALLOC; goto cleanup; }

        // Loop over edges for each body and get the point counts
        for (edgeIndex = 0; edgeIndex < numEdge; edgeIndex++) {

          numEdgePoint = 0;

          status = EG_getTessEdge(tess, edgeIndex+1, &numEdgePoint, &xyzs, &ts);
          if (status != EGADS_SUCCESS) goto cleanup;

          status = retrieve_CAPSGroupAttr(edges[edgeIndex], &groupName);
          if (status == EGADS_SUCCESS) {
              status = get_mapAttrToIndexIndex(&attrMap, groupName, &attrIndex);
              if (status == CAPS_SUCCESS) {

                  for (i = 0; i < numMeshProp; i++) {

                      if (meshProp[i].attrIndex == attrIndex) {
                          if (meshProp[i].numEdgePoints > 0) {
                            numEdgePoint = meshProp[i].numEdgePoints;
                            userSet[edgeIndex+1] = 1;

                            // halve the specified count if regularized quads
                            if (quadMesh == REGULARIZED_QUAD)
                              numEdgePoint = numEdgePoint/2 + numEdgePoint % 2;
                          }
                          break;
                      }
                  }
              }
          } else if (status != EGADS_SUCCESS && status != EGADS_NOTFOUND) {
              goto cleanup;
          }

          points[edgeIndex+1] = numEdgePoint;
        }

        status = EG_deleteObject(tess); tess = NULL;
        if (status != EGADS_SUCCESS) goto cleanup;

        if (quadMesh >= REGULARIZED_QUAD) {
            // modify the point counts to maximize TFI
            status = mesh_edgeVertexTFI( bodies[bodyIndex], points, userSet );
            if (status != EGADS_SUCCESS) goto cleanup;
        }

        // Loop over edges for each body
        for (edgeIndex = 0; edgeIndex < numEdge; edgeIndex++) {

            numEdgePoint = points[edgeIndex+1];
            edgeDistribution = UnknownDistribution;

            status = retrieve_CAPSGroupAttr(edges[edgeIndex], &groupName);
            if (status == EGADS_SUCCESS) {
                status = get_mapAttrToIndexIndex(&attrMap, groupName, &attrIndex);
                if (status == CAPS_SUCCESS) {

                    for (i = 0; i < numMeshProp; i++) {

                        if (meshProp[i].attrIndex == attrIndex) {
                            edgeDistribution =  meshProp[i].edgeDistribution;

                            initialNodeSpacing[0] = meshProp[i].initialNodeSpacing[0];
                            initialNodeSpacing[1] = meshProp[i].initialNodeSpacing[1];

                            break;
                        }
                    }
                }
            } else if (status != EGADS_SUCCESS && status != EGADS_NOTFOUND) {
                goto cleanup;
            }

            if (edgeDistribution == UnknownDistribution) {
                if (quadMesh >= REGULARIZED_QUAD) {
                    // only set the point count for quading if no distribution is provided
                    // this is equivalent to the even distribution using .rPos
                    numEdgePoint = MAX(0,numEdgePoint-2);
                    status = EG_attributeAdd(edges[edgeIndex], ".nPos", ATTRINT, 1, &numEdgePoint, NULL, NULL);
                    if (status != EGADS_SUCCESS) goto cleanup;
                }
                continue;
            }

            if (numEdgePoint >= 2) {

                if (numEdgePoint == 2) {

                    i = 0;
                    status = EG_attributeAdd(edges[edgeIndex],
                                             ".rPos", ATTRINT, 1, &i, NULL, NULL);

                    if (status != EGADS_SUCCESS) goto cleanup;

                } else {

                    rPos  = (double *) EG_alloc(numEdgePoint*sizeof(double));
                    if (rPos == NULL) {
                        status = EGADS_MALLOC;
                        goto cleanup;
                    }

                    if (edgeDistribution == EvenDistribution) {

                        // Even distribution
                        for (i = 0; i < numEdgePoint; i++) {
                            rPos[i] = (double) (i+1) / (double)(numEdgePoint-1);
                        }

                    } else if (edgeDistribution == TanhDistribution) {

                        // Default to "even" in case something is wrong
                        for (i = 0; i < numEdgePoint; i++) {
                            rPos[i] = (double) (i+1) / (double)(numEdgePoint-1);
                        }

                        // Tanh distribution - both nodes specified
                        if (initialNodeSpacing[0] > 0.0 &&
                                initialNodeSpacing[1] > 0.0) { // Both ends are specified

                            I  = (double) numEdgePoint;

                            A = sqrt(initialNodeSpacing[1]) / sqrt(initialNodeSpacing[0]);

                            B = 1/ (I*sqrt(initialNodeSpacing[0] * initialNodeSpacing[1]));

                            inputVars[0] = B;

                            stretchingFactor = root_BisectionMethod( &eqn_stretchingFactorDoubleSided, 0, 1000, inputVars);
                            //printf("StretchingFactor = %f\n", stretchingFactor);

                            for (i = 0; i < numEdgePoint; i++) {

                                epi = (double) i+1;

                                U = 0.5 * ( 1.0 + tanh(stretchingFactor*(epi/I-0.5))/tanh(stretchingFactor/2));

                                rPos[i] = U/ (A + (1-A)*U);
                            }

                        } else if (initialNodeSpacing[0] > 0.0 &&
                                initialNodeSpacing[1] <= 0.0) {

                            I = (double)(numEdgePoint);

                            inputVars[0] = 1.0; // epi = 1
                            inputVars[1] = I;
                            inputVars[2] = initialNodeSpacing[0];

                            stretchingFactor = root_BisectionMethod( &eqn_stretchingFactorSingleSided, 0, 1000, inputVars);
                            //printf("0 - stretchingFactor = %f\n", stretchingFactor);

                            for (i = 0; i < numEdgePoint; i++) {

                                epi = (double) i+1;

                                rPos[i] = 1.0 + tanh(stretchingFactor*(epi/I-1)) /tanh(stretchingFactor);
                            }

                        } else if (initialNodeSpacing[0] <= 0.0 &&
                                initialNodeSpacing[1] > 0.0) {

                            I = (double)(numEdgePoint);

                            inputVars[0] = 1.0; // epi = 1
                            inputVars[1] = I;
                            inputVars[2] = initialNodeSpacing[1];

                            stretchingFactor = root_BisectionMethod( &eqn_stretchingFactorSingleSided, 0, 1000, inputVars);
                            //printf("1 - stretchingFactor = %f\n", stretchingFactor);

                            j = numEdgePoint-1;
                            for (i = 0; i < numEdgePoint; i++) {

                                epi = (double) i;

                                rPos[j] = 1.0 - (1.0 + tanh(stretchingFactor*(epi/I-1)) /tanh(stretchingFactor));
                                j -= 1;
                            }
                        }

                        // Debug
                        //for (i = 0; i < numEdgePoint-1; i++) printf("EdgeIndex = %d, Rpos(%d) delta= %f\n", edgeIndex, i, rPos[i+1]-rPos[i]);

                    } else {

                        printf("Unknown distribution function\n");
                        goto cleanup;
                    }

                    //printf("Number of points added to edge %d (body = %d) = %d\n", edgeIndex, bodyIndex, numEdgePoint);
                    status = EG_attributeAdd(edges[edgeIndex],
                                             ".rPos", ATTRREAL, numEdgePoint-2, NULL, rPos, NULL);
                    if (status != EGADS_SUCCESS) goto cleanup;

                    EG_free(rPos); rPos = NULL;
                }
            }
        }

        EG_free(edges); edges = NULL;
        EG_free(faces); faces = NULL;
        EG_free(points); points = NULL;
        EG_free(userSet); userSet = NULL;
    }

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_modifyBodyTess, status = %d\n", status);

        EG_deleteObject(tess);

        EG_free(points);
        EG_free(userSet);
        EG_free(faces);
        EG_free(edges);
        EG_free(rPos);

        return status;
}
// Populate bndCondStruct boundary condition information - Boundary condition values get filled with 99
int populate_bndCondStruct_from_bcPropsStruct(cfdBCsStruct *bcProps, bndCondStruct *bndConds)
{

    // *bcProps [IN]
    // *bndConds [IN/OUT]

    int i;

    bndConds->numBND = bcProps->numBCID;

    // Transfers bcIDS into bndConds
    if (bndConds->numBND > 0){
        bndConds->bndID = (int *) EG_alloc(bndConds->numBND*sizeof(int));
        bndConds->bcVal = (int *) EG_alloc(bndConds->numBND*sizeof(int));
        if ((bndConds->bndID == NULL) || (bndConds->bcVal == NULL)) {

            if (bndConds->bndID != NULL) EG_free(bndConds->bndID);
            bndConds->bndID = NULL;

            if (bndConds->bcVal != NULL) EG_free(bndConds->bcVal);
            bndConds->bcVal = NULL;

            bndConds->numBND = 0;

            return EGADS_MALLOC;
        }
    }

    for (i = 0; i < bndConds->numBND ; i++) {
        bndConds->bndID[i] = bcProps->surfaceProps[i].bcID;
    }

    // Fill in rest of bcVal  with dummy values
    for (i = 0; i < bndConds->numBND ; i++) bndConds->bcVal[i] = 99;

    return CAPS_SUCCESS;
}

// Populate bndCondStruct boundary condition information from attribute map - Boundary condition
// values get filled with 99
int populate_bndCondStruct_from_mapAttrToIndexStruct(mapAttrToIndexStruct *attrMap, bndCondStruct *bndConds)
{
    // *bndConds [OUT]
    // *attrMap [IN]

    int i;

    bndConds->numBND = attrMap->numAttribute;

    // Transfers bcIDS into bndConds
    if (bndConds->numBND > 0){
        bndConds->bndID = (int *) EG_alloc(bndConds->numBND*sizeof(int));
        bndConds->bcVal = (int *) EG_alloc(bndConds->numBND*sizeof(int));
        if ((bndConds->bndID == NULL) || (bndConds->bcVal == NULL)) {

            if (bndConds->bndID != NULL) EG_free(bndConds->bndID);
            bndConds->bndID = NULL;

            if (bndConds->bcVal != NULL) EG_free(bndConds->bcVal);
            bndConds->bcVal = NULL;

            bndConds->numBND = 0;

            return EGADS_MALLOC;
        }
    }

    for (i = 0; i < bndConds->numBND; i++) {
        bndConds->bndID[i] = attrMap->attributeIndex[i];
    }

    // Fill in rest of bcVal  with dummy values
    for (i = 0; i < bndConds->numBND; i++) bndConds->bcVal[i] = 99;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a bodyTessMapping in the bodyTessMappingStruct structure format
int initiate_bodyTessMappingStruct (bodyTessMappingStruct *bodyTessMapping) {
    // EGADS body tessellation storage
    bodyTessMapping->egadsTess = NULL;

    bodyTessMapping->numTessFace = 0; // Number of faces in the tessellation

    bodyTessMapping->tessFaceQuadMap = NULL; // List to keep track of whether or not the tessObj has quads that have been split into tris
                                             // size = [numTessFace]. In general if the quads have been split they should be added to the end
                                             // of the tri list in the face tessellation

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a bodyTessMapping in the bodyTessMappingStruct structure format
int destroy_bodyTessMappingStruct (bodyTessMappingStruct *bodyTessMapping) {

    //int status; // Function return

    // EGADS body tessellation storage
    /*if (bodyTessMapping->egadsTess != NULL) {
        status = EG_deleteObject(bodyTessMapping->egadsTess);
        if (status != EGADS_SUCCESS) printf("Status delete tess object = %d", status);
    }*/
    //bodyTessMapping->egadsTess= NULL;

    bodyTessMapping->numTessFace = 0; // Number of faces in the tessellation

    if (bodyTessMapping->tessFaceQuadMap != NULL) EG_free(bodyTessMapping->tessFaceQuadMap);
    bodyTessMapping->tessFaceQuadMap = NULL; // List to keep track of whether or not the tessObj has quads that have been split into tris
    // size = [numTessFace]. In general if the quads have been split they should be added to the end
    // of the tri list in the face tessellation

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a bndCond in the bndCondStruct structure format
int initiate_bndCondStruct(bndCondStruct *bndCond) {

    bndCond->numBND = 0;
    bndCond->bndID = NULL;
    bndCond->bcVal = NULL;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values, free all arrays, and NULL all pointers) a bndCond in the bndCondStruct structure format
int destroy_bndCondStruct(bndCondStruct *bndCond) {

    bndCond->numBND = 0;

    if (bndCond->bndID != NULL) EG_free(bndCond->bndID);
    bndCond->bndID = NULL;

    if (bndCond->bcVal != NULL) EG_free(bndCond->bcVal);
    bndCond->bcVal = NULL;

    return CAPS_SUCCESS;
}

int populate_regions(tetgenRegionsStruct* regions,
                     int length,
                     const capsTuple* tuples)
{
  int m;
  int n;
  int status;
  double x;
  double v[3];
  char* val = NULL;
  char* dict = NULL;

  // Resize the regions data structure arrays.
  regions->size = length;
  if (regions->x != NULL) EG_free(regions->x);
  if (regions->y != NULL) EG_free(regions->y);
  if (regions->z != NULL) EG_free(regions->z);
  if (regions->attribute != NULL) EG_free(regions->attribute);
  if (regions->volume_constraint != NULL) EG_free(regions->volume_constraint);
  regions->x = (double*) EG_alloc(length * sizeof(double));
  regions->y = (double*) EG_alloc(length * sizeof(double));
  regions->z = (double*) EG_alloc(length * sizeof(double));
  regions->attribute = (int*) EG_alloc(length * sizeof(int));
  regions->volume_constraint = (double*) EG_alloc(length * sizeof(double));

  for (n = 0; n < length; ++n)
  {
    dict = tuples[n].value;

    // Store the seed point that is known to lie strictly inside of the region.
    status = search_jsonDictionary(dict, "seed", &val);
    if (status == CAPS_SUCCESS && string_toDoubleArray(val, 3, v) == CAPS_SUCCESS)
    {
      regions->x[n] = v[0];
      regions->y[n] = v[1];
      regions->z[n] = v[2];
    }
    else
    {
      // no point
    }
    if (val != NULL)
    {
      EG_free(val);
      val = NULL;
    }

    // Store the region attribute.
    status = search_jsonDictionary(dict, "id", &val);
    if (status == CAPS_SUCCESS && string_toInteger(val, &m) == CAPS_SUCCESS)
    {
      regions->attribute[n] = m;
    }
    else
    {
      regions->attribute[n] = 0;
    }
    if (val != NULL)
    {
      EG_free(val);
      val = NULL;
    }

    // Store the region cell volume constraint.
    status = search_jsonDictionary(dict, "volumeConstraint", &val);
    if (status == CAPS_SUCCESS && string_toDouble(val, &x) == CAPS_SUCCESS)
    {
      regions->volume_constraint[n] = x;
    }
    else
    {
      regions->volume_constraint[n] = -1.0;
    }
    if (val != NULL)
    {
      EG_free(val);
      val = NULL;
    }
  }

  return CAPS_SUCCESS;
}

int initiate_regions(tetgenRegionsStruct* regions) {
  regions->size = 0;
  regions->x = NULL;
  regions->y = NULL;
  regions->z = NULL;
  regions->attribute = NULL;
  regions->volume_constraint = NULL;

  return CAPS_SUCCESS;
}

int destroy_regions(tetgenRegionsStruct* regions) {
  EG_free(regions->x);
  EG_free(regions->y);
  EG_free(regions->z);
  EG_free(regions->attribute);
  EG_free(regions->volume_constraint);

  return initiate_regions(regions);
}

int populate_holes(tetgenHolesStruct* holes,
                   int length,
                   const capsTuple* tuples)
{
  int n;
  int status;
  double v[3];
  char* val = NULL;
  char* dict = NULL;

  // Resize the holes data structure arrays.
  holes->size = length;
  if (holes->x != NULL) EG_free(holes->x);
  if (holes->y != NULL) EG_free(holes->y);
  if (holes->z != NULL) EG_free(holes->z);
  holes->x = (double*) EG_alloc(length * sizeof(double));
  holes->y = (double*) EG_alloc(length * sizeof(double));
  holes->z = (double*) EG_alloc(length * sizeof(double));

  for (n = 0; n < length; ++n)
  {
    dict = tuples[n].value;

    // Store the seed point that is known to lie strictly inside of the hole.
    status = search_jsonDictionary(dict, "seed", &val);
    if (status == CAPS_SUCCESS && string_toDoubleArray(val, 3, v) == CAPS_SUCCESS)
    {
      holes->x[n] = v[0];
      holes->y[n] = v[1];
      holes->z[n] = v[2];
    }
    else
    {
      // no point
    }
    if (val != NULL)
    {
      EG_free(val);
      val = NULL;
    }
  }

  return CAPS_SUCCESS;
}

int initiate_holes(tetgenHolesStruct* holes) {
  holes->size = 0;
  holes->x = NULL;
  holes->y = NULL;
  holes->z = NULL;

  return CAPS_SUCCESS;
}

int destroy_holes(tetgenHolesStruct* holes) {
  EG_free(holes->x);
  EG_free(holes->y);
  EG_free(holes->z);

  return initiate_holes(holes);
}

// Initiate (0 out all values and NULL all pointers) a input in the tetgenInputStruct structure format
static int initiate_tetgenInputStruct(tetgenInputStruct *input) {

    int status;

    input->meshQuality_rad_edge = 0.0;// Tetgen: maximum radius-edge ratio
    input->meshQuality_angle = 0.0;   // Tetgen: minimum dihedral angle
    input->meshInputString = NULL;    // Tetgen: Input string (optional) if NULL use default values
    input->verbose = (int) false;              // 0 = False, anything else True - Verbose output from mesh generator
    input->ignoreSurfaceExtract = (int) false; // 0 = False, anything else True - Don't extract the new surface mesh if
    //      Steiner points are added.
    input->meshTolerance = 0.0; // Tetgen : mesh tolerance

    status = initiate_regions(&input->regions);
    if (status != CAPS_SUCCESS) return status;

    return initiate_holes(&input->holes);
}


// Destroy (0 out all values and NULL all pointers) a input in the tetgenInputStruct structure format
static int destroy_tetgenInputStruct(tetgenInputStruct *input) {

    input->meshQuality_rad_edge = 0.0;// Tetgen: maximum radius-edge ratio
    input->meshQuality_angle = 0.0;   // Tetgen: minimum dihedral angle
    if (input->meshInputString != NULL) EG_free(input->meshInputString);

    input->meshInputString = NULL;     // Tetgen: Input string (optional) if NULL use default values
    input->verbose = (int) false;              // 0 = False, anything else True - Verbose output from mesh generator
    input->ignoreSurfaceExtract = (int) false; // 0 = False, anything else True - Don't extract the new surface mesh if
                                               //      Steiner points are added.
    input->meshTolerance = 0.0; // Tetgen : mesh tolerance

    destroy_regions(&input->regions);
    destroy_holes(&input->holes);

    return CAPS_SUCCESS;
}


// Initiate (0 out all values and NULL all pointers) a input in the aflr3InputStruct structure format
static int initiate_aflr3InputStruct(aflr3InputStruct *input) {

    input->meshInputString = NULL;     // AFLR3: Input string (optional) if NULL use default values

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a input in the aflr3InputStruct structure format
static int destroy_aflr3InputStruct(aflr3InputStruct *input) {

    if (input->meshInputString != NULL) EG_free(input->meshInputString);
    input->meshInputString = NULL;     // AFLR3: Input string (optional) if NULL use default values

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a input in the aflr4InputStruct structure format
static int initiate_aflr4InputStruct(aflr4InputStruct *input) {

    input->meshInputString = NULL;     // AFLR4: Input string (optional) if NULL use default values

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a input in the aflr4InputStruct structure format
static int destroy_aflr4InputStruct(aflr4InputStruct *input) {

    if (input->meshInputString != NULL) EG_free(input->meshInputString);
    input->meshInputString = NULL;     // AFLR4: Input string (optional) if NULL use default values

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a input in the hoTessInputStruct structure format
static int initiate_hoTessInputStruct(hoTessInputStruct *input) {

	input->meshElementType           = UnknownMeshElement;
	input->numLocalElevatedVerts     = 0;
	input->weightsLocalElevatedVerts = NULL;
	input->numLocalElevatedTris      = 0;
	input->orderLocalElevatedTris    = NULL;

	return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a input in the hoTessInputStruct structure format
static int destroy_hoTessInputStruct(hoTessInputStruct *input) {

	input->meshElementType       = UnknownMeshElement;
	input->numLocalElevatedVerts = 0;
	input->numLocalElevatedTris  = 0;

	if( input->weightsLocalElevatedVerts != NULL ) EG_free( input->weightsLocalElevatedVerts );
	input->weightsLocalElevatedVerts = NULL;

	if( input->orderLocalElevatedTris != NULL ) EG_free( input->orderLocalElevatedTris );
	input->orderLocalElevatedTris = NULL;

	return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a meshInput in the meshInputStruct structure format
int initiate_meshInputStruct(meshInputStruct *meshInput) {

    int status; // Function return

    meshInput->paramTess[0] = 0;
    meshInput->paramTess[1] = 0;
    meshInput->paramTess[2] = 0;

    meshInput->preserveSurfMesh = (int) false; // 0 = False , anything else True - Use the body tessellation as the surface mesh

    meshInput->quiet = (int) false; // 0 = False , anything else True - No output from mesh generator
    meshInput->outputFormat = NULL;   // Mesh output formats - AFLR3, TECPLOT, VTK, SU2
    meshInput->outputFileName = NULL; // Filename prefix for mesh
    meshInput->outputDirectory = NULL;// Directory to write mesh to
    meshInput->outputASCIIFlag = (int) true;  // 0 = Binary output, anything else for ASCII

    status = initiate_bndCondStruct(&meshInput->bndConds);
    if (status != CAPS_SUCCESS) return status;

    status = initiate_tetgenInputStruct(&meshInput->tetgenInput);
    if (status != CAPS_SUCCESS) return status;

    status = initiate_aflr3InputStruct(&meshInput->aflr3Input);
    if (status != CAPS_SUCCESS) return status;

    status = initiate_aflr4InputStruct(&meshInput->aflr4Input);
    if (status != CAPS_SUCCESS) return status;

    status = initiate_hoTessInputStruct(&meshInput->hoTessInput);
    if (status != CAPS_SUCCESS) return status;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a meshInput in the meshInputStruct structure format
int destroy_meshInputStruct(meshInputStruct *meshInput) {

    int status; // Function return

    meshInput->paramTess[0] = 0;
    meshInput->paramTess[1] = 0;
    meshInput->paramTess[2] = 0;

    meshInput->preserveSurfMesh = (int) false; // 0 = False , anything else True - Use the body tessellation as the surface mesh

    meshInput->quiet = (int) false; // 0 = False , anything else True - No output from mesh generator
    if (meshInput->outputFormat != NULL) EG_free(meshInput->outputFormat);
    meshInput->outputFormat = NULL;   // Mesh output formats - AFLR3, TECPLOT, VTK, SU2

    if (meshInput->outputFileName != NULL) EG_free(meshInput->outputFileName);
    meshInput->outputFileName = NULL; // Filename prefix for mesh

    if (meshInput->outputDirectory != NULL) EG_free(meshInput->outputDirectory);
    meshInput->outputDirectory = NULL;// Directory to write mesh to

    meshInput->outputASCIIFlag = (int) true;  // 0 = Binary output, anything else for ASCII

    status = destroy_bndCondStruct(&meshInput->bndConds);
    if (status != CAPS_SUCCESS) return status;

    status = destroy_tetgenInputStruct(&meshInput->tetgenInput);
    if (status != CAPS_SUCCESS) return status;

    status = destroy_aflr3InputStruct(&meshInput->aflr3Input);
    if (status != CAPS_SUCCESS) return status;

    status = destroy_aflr4InputStruct(&meshInput->aflr4Input);
    if (status != CAPS_SUCCESS) return status;

    status = destroy_hoTessInputStruct(&meshInput->hoTessInput);
    if (status != CAPS_SUCCESS) return status;

    return CAPS_SUCCESS;
}

// Write a *.mapbc file
int write_MAPBC(char *fname,
                int numBnds,
                int *bndID,
                int *bndVals)
{
    int status; // Function return status

    int i, j; // Indexing

    FILE *fp = NULL;
    char *filename = NULL;
    char fileExt[] = ".mapbc";

    int bndIDMin = 0, bndIDMinIndex =0;
    int *wroteBnd = NULL; // Array to keep track if the boundary has been written

    printf("\nWriting MAPBC file ....\n");

    if (numBnds <= 0) {
        printf("Warning: Number of boundaries for MAPBC = 0 !\n");
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    wroteBnd = (int *) EG_alloc(numBnds*sizeof(int));
    if (wroteBnd == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    for (i = 0; i < numBnds; i++) wroteBnd[i] = (int) false;

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    sprintf(filename,"%s%s",fname, fileExt);

    fp = fopen(filename, "w");

    if (fp == NULL) {
        printf("Unable to open file: %s\n", filename);
        status = CAPS_IOERR;
        goto cleanup;
    }

    // Write out the total number of IDs
    fprintf(fp,"%d\n",numBnds);

    // Make sure the boundary IDs are written out in order
    for (j = 0; j < numBnds; j++) {

        // Determine lowest boundary ID that hasn't already been written
        bndIDMin = 1E6;
        for (i = 0; i < numBnds; i++) {
            if (bndIDMin >= bndID[i] && wroteBnd[i] != (int) true) {
                bndIDMin = bndID[i];
                bndIDMinIndex = i;
            }
        }

        // Write out boundary ID and value
        fprintf(fp,"%d %d\n", bndID[bndIDMinIndex], bndVals[bndIDMinIndex]);

        // Mark boundary has being written
        wroteBnd[bndIDMinIndex] = (int) true;
    }

    printf("Finished writing MAPBC file\n\n");

    status = CAPS_SUCCESS;

    cleanup:
        if (filename != NULL) EG_free(filename);

        if (fp != NULL) fclose(fp);

        if (wroteBnd != NULL) EG_free(wroteBnd);

        return status;
}

#ifdef DEFINED_BUT_NOT_USED /* Function isn't used, but retained for reference */
static void get_TriangleArea(double p1[3],
                             double p2[3],
                             double p3[3],
                             double *area) {

    double a = 0.0, b = 0.0, c = 0.0, s = 0.0;

    a = sqrt( (p2[0] - p1[0])*(p2[0] - p1[0]) +
              (p2[1] - p1[1])*(p2[1] - p1[1]) +
              (p2[2] - p1[2])*(p2[2] - p1[2]));

    b = sqrt( (p3[0] - p1[0])*(p3[0] - p1[0]) +
              (p3[1] - p1[1])*(p3[1] - p1[1]) +
              (p3[2] - p1[2])*(p3[2] - p1[2]));

    c = sqrt( (p3[0] - p2[0])*(p3[0] - p2[0]) +
              (p3[1] - p2[1])*(p3[1] - p2[1]) +
              (p3[2] - p2[2])*(p3[2] - p2[2]));

    s = (a + b + c)/2.0;

    *area = sqrt(s*(s - a)*(s - b)*(s - c));
}
#endif

void get_Surface_Norm(double p1[3],
                      double p2[3],
                      double p3[3],
                      double norm[3])
{
    double a[3]={0.0,0.0,0.0};
    double b[3]={0.0,0.0,0.0};
    double mag = 0.0;

    // a x b

    // Create two vectors from points
    a[0]= p2[0]-p1[0];
    a[1]= p2[1]-p1[1];
    a[2]= p2[2]-p1[2];

    b[0]= p3[0]-p1[0];
    b[1]= p3[1]-p1[1];
    b[2]= p3[2]-p1[2];

    // Take the cross product
    norm[0] = a[1]*b[2]-a[2]*b[1];

    norm[1] = a[2]*b[0]-a[0]*b[2];

    norm[2] = a[0]*b[1]-a[1]*b[0];

    // Normalize vector
    mag = sqrt(norm[0]*norm[0] + norm[1]*norm[1] + norm[2]*norm[2]);

    norm[0] = norm[0]/mag;
    norm[1] = norm[1]/mag;
    norm[2] = norm[2]/mag;
}

// Initiate (0 out all values and NULL all pointers) a meshProp in the meshSizingStruct structure format
int initiate_meshSizingStruct (meshSizingStruct *meshProp) {

    meshProp->name = NULL; // Attribute name
    meshProp->attrIndex = 0;  // Attribute index

    meshProp->numEdgePoints = 0; // Number of points along and edge

    meshProp->edgeDistribution = UnknownDistribution; // Distribution function along an edge

    meshProp->minSpacing = 0;            // Minimum allowed spacing on EDGE/FACE
    meshProp->maxSpacing = 0;            // Maximum allowed spacing on EDGE/FACE
    meshProp->avgSpacing = 0;            // Average allowed spacing on EDGE

    meshProp->maxAngle = 0;              // Maximum angle on an EDGE to control spacing
    meshProp->maxDeviation = 0;          // Maximum deviation on an EDGE/FACE to control spacing
    meshProp->boundaryDecay = 0;         // Decay of influence of the boundary spacing on the interior spacing

    meshProp->nodeSpacing = 0;           // Node spacing at a NODE or end points of an EDGE

    meshProp->initialNodeSpacing[0] = 0.0; // Initial node spacing along an edge
    meshProp->initialNodeSpacing[1] = 0.0;

    meshProp->useTessParams = (int) false; // Trigger to use the specified face tessellation parameters
    meshProp->tessParams[0] = 0.0; // Tessellation parameters on a face.
    meshProp->tessParams[1] = 0.0;
    meshProp->tessParams[2] = 0.0;

    meshProp->boundaryLayerThickness = 0.0;   // Boundary layer thickness on a face (3D) or edge (2D)
    meshProp->boundaryLayerSpacing = 0.0;     // Boundary layer spacing on a face (3D) or edge (2D)
    meshProp->boundaryLayerMaxLayers = 0;     // Maximum number of layers
    meshProp->boundaryLayerFullLayers = 0;    // Number of complete layers
    meshProp->boundaryLayerGrowthRate = 0.0;  // Growth rate of the boundary layer

    meshProp->bcType = NULL;     // Name of the meshing boundary condition type
    meshProp->scaleFactor = 0.0; // Scaling factor applied generating face meshes
    meshProp->edgeWeight  =  -1; // Interpolation weight on edge mesh size between faces with large angles

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a meshProp in the meshSizingStruct structure format
int destroy_meshSizingStruct (meshSizingStruct *meshProp) {

    if (meshProp->name != NULL) EG_free(meshProp->name);

    meshProp->name = NULL; // Attribute name
    meshProp->attrIndex = 0;  // Attribute index

    meshProp->numEdgePoints = 0; // Number of points along and edge

    meshProp->edgeDistribution = UnknownDistribution; // Distribution function along an edge

    meshProp->minSpacing = 0;            // Minimum allowed spacing on EDGE/FACE
    meshProp->maxSpacing = 0;            // Maximum allowed spacing on EDGE/FACE
    meshProp->avgSpacing = 0;            // Average allowed spacing on EDGE

    meshProp->maxAngle = 0;              // Maximum angle on an EDGE to control spacing
    meshProp->maxDeviation = 0;          // Maximum deviation on an EDGE/FACE to control spacing
    meshProp->boundaryDecay = 0;         // Decay of influence of the boundary spacing on the interior spacing

    meshProp->nodeSpacing = 0;           // Node spacing at a NODE or end points of an EDGE

    meshProp->initialNodeSpacing[0] = 0.0; // Initial node spacing along an edge
    meshProp->initialNodeSpacing[1] = 0.0;

    meshProp->useTessParams = (int) false; // Trigger to use the specified face tessellation parameters
    meshProp->tessParams[0] = 0.0; // Tessellation parameters on a face.
    meshProp->tessParams[1] = 0.0;
    meshProp->tessParams[2] = 0.0;

    meshProp->boundaryLayerThickness = 0.0;   // Boundary layer thickness on a face (3D) or edge (2D)
    meshProp->boundaryLayerSpacing = 0.0;     // Boundary layer spacing on a face (3D) or edge (2D)
    meshProp->boundaryLayerMaxLayers = 0;     // Maximum number of layers
    meshProp->boundaryLayerFullLayers = 0;    // Number of complete layers
    meshProp->boundaryLayerGrowthRate = 0.0;  // Growth rate of the boundary layer

    if ( meshProp->bcType == NULL) EG_free(meshProp->bcType);
    meshProp->bcType = NULL;     // Name of the meshing boundary condition type
    meshProp->scaleFactor = 0.0; // Scaling factor applied generating face meshes
    meshProp->edgeWeight  =  -1; // Interpolation weight on edge mesh size between faces with large angles

    return CAPS_SUCCESS;
}


// Fill meshProps in a meshBCStruct format with mesh boundary condition information from incoming Mesh Sizing Tuple
int mesh_getSizingProp(int numTuple,
                       capsTuple meshBCTuple[],
                       mapAttrToIndexStruct *attrMap,
                       int *numMeshProp,
                       meshSizingStruct *meshProps[]) {

    /*! \page meshSizingProp Mesh Sizing
     * NOTE: Available mesh sizing parameters differ between mesh generators.<br><br>
     * Structure for the mesh sizing tuple  = ("CAPS Group Name", "Value").
     * "CAPS Group Name" defines the capsGroup on which the sizing information should be applied.
     * The "Value" can either be a JSON String dictionary (see Section \ref jsonStringMeshSizing) or a single string keyword string
     * (see Section \ref keyStringMeshSizing)
     */

    int status; //Function return

    int i; // Indexing
    int stringLen = 0;

    char *keyValue = NULL;
    char *keyWord = NULL;

    // Destroy our meshProps structures coming in if aren't 0 and NULL already
    for (i = 0; i < *numMeshProp; i++) {
        status = destroy_meshSizingStruct(&(*meshProps)[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    if (*meshProps != NULL) EG_free(*meshProps);
    *meshProps = NULL;
    *numMeshProp = 0;

    printf("\nGetting mesh sizing parameters\n");

    *numMeshProp = numTuple;

    if (*numMeshProp > 0) {
        *meshProps = (meshSizingStruct *) EG_alloc(*numMeshProp * sizeof(meshSizingStruct));
        if (*meshProps == NULL) {
            *numMeshProp = 0;
            return EGADS_MALLOC;
        }

    } else {
        printf("\tNumber of mesh sizing values in input tuple is 0\n");
        return CAPS_NOTFOUND;
    }

    for (i = 0; i < *numMeshProp; i++) {
        status = initiate_meshSizingStruct(&(*meshProps)[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    //printf("Number of tuple pairs = %d\n", numTuple);

    for (i = 0; i < *numMeshProp; i++) {

        printf("\tMesh sizing name - %s\n", meshBCTuple[i].name);

        status = get_mapAttrToIndexIndex(attrMap, (const char *) meshBCTuple[i].name, &(*meshProps)[i].attrIndex);
        if (status == CAPS_NOTFOUND) {
            printf("\tMesh Sizing name \"%s\" not found in attrMap\n", meshBCTuple[i].name);
            return status;
        }

        // Copy mesh sizing name
        if ((*meshProps)[i].name != NULL) EG_free((*meshProps)[i].name);

        (*meshProps)[i].name = EG_strdup(meshBCTuple[i].name);
        if ((*meshProps)[i].name == NULL) return EGADS_MALLOC;

        // Do we have a json string?
        if (strncmp(meshBCTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n",meshBCTuple[i].value);

            /*! \page meshSizingProp
             * \section jsonStringMeshSizing JSON String Dictionary
             *
             * If "Value" is a JSON string dictionary
             *  (e.g. "Value" = {"edgeDistribution": "Even", "numEdgePoints": 100})
             *  the following keywords ( = default values) may be used:
             */

            // Edge properties
            /*! \page meshSizingProp
             *
             * \if (AFLR2|| DELAUNDO || EGADSTESS ||  AFLR3 || TETGEN )
             *
             * <ul>
             * <li> <B>edgeDistribution = "Even"</B> </li> <br>
             *      Edge Distribution types. Options: Even (even distribution), Tanh (hyperbolic tangent
             *      distribution).
             * </ul>
             *
             * \endif
             */

            (*meshProps)[i].edgeDistribution = EvenDistribution;
            keyWord = "edgeDistribution";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{Even}
                if      (strcasecmp(keyValue, "\"Even\"") == 0) (*meshProps)[i].edgeDistribution = EvenDistribution;
                else if (strcasecmp(keyValue, "\"Tanh\"") == 0) (*meshProps)[i].edgeDistribution = TanhDistribution;
                else {

                    printf("\tUnrecognized \"%s\" specified (%s) for Mesh_Condition tuple %s, current options are "
                            "\" Even, ... \"\n", keyWord, keyValue,  meshBCTuple[i].name);
                    if (keyValue != NULL) EG_free(keyValue);

                    return CAPS_NOTFOUND;
                }

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
            }

            /*! \page meshSizingProp
             *
             * \if (AFLR2|| DELAUNDO || EGADSTESS || AFLR3 || TETGEN )
             *
             * <ul>
             * <li>  <B>numEdgePoints = 0</B> </li> <br>
             *  Number of points along an edge.
             *  \if (POINTWISE)
             *  <br> This overrides the PW:ConnectorDimension attribute on EDGEs.
             *  \endif
             * </ul>
             *
             * \endif
             */
            keyWord = "numEdgePoints";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &(*meshProps)[i].numEdgePoints);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             * \if (AFLR2|| DELAUNDO || EGADSTESS || AFLR3 || TETGEN )
             *
             *  <ul>
             *   <li>  <B>initialNodeSpacing = [0.0, 0.0]</B> </li> <br>
             *   Initial (scaled) node spacing along an edge. [first node, last node] consistent with the orientation of the edge.
             *  </ul>
             *
             * \endif
             */
            keyWord = "initialNodeSpacing";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleArray(keyValue, 2, (*meshProps)[i].initialNodeSpacing);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            // Edge/Face properties
            /*! \page meshSizingProp
             *
             * \if (AFLR3)
             *
             * <ul>
             * <li>  <B>boundaryLayerThickness = 0.0</B> </li> <br>
             *   Desired lower bound boundary layer thickness on a face.
             *   The minimum thickness in the mesh is is given by<br>
             *   meshBLThickness = capsMeshLength * boundaryLayerThickness
             * </ul>
             *
             * \elseif (DELAUNDO)
             *
             * <ul>
             * <li>  <B>boundaryLayerThickness = 0.0</B> </li> <br>
             *  Desired boundary layer thickness on an edge (2D meshing)
             * </ul>
             * \endif
             */
            keyWord = "boundaryLayerThickness";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].boundaryLayerThickness);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (AFLR3 || POINTWISE)
             *
             * <ul>
             * <li>  <B>boundaryLayerSpacing = 0.0</B> </li> <br>
             *   Initial spacing factor for boundary layer mesh growth on as face.<br>
             *   The spacing in the mesh is is given by<br>
             *   meshBLSpacing = capsMeshLength * boundaryLayerSpacing
             *   \if (POINTWISE)
             *   <br> This overrides the PW:WallSpacing attribute on FACEs.
             *   \endif
             * </ul>
             *
             *\elseif (AFLR2 || DELAUNDO)
             *
             * <ul>
             * <li>  <B>boundaryLayerSpacing = 0.0</B> </li> <br>
             *  Initial spacing for boundary layer mesh growth on an edge (2D meshing).
             * </ul>
             * \endif
             */
            keyWord = "boundaryLayerSpacing";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].boundaryLayerSpacing);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>boundaryLayerMaxLayers = 0.0</B> </li> <br>
             *   Maximum number of layers when growing a boundary layer.
             *   \if (POINTWISE)
             *   <br> This overrides the PW:DomainMaxLayers attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "boundaryLayerMaxLayers";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &(*meshProps)[i].boundaryLayerMaxLayers);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>boundaryLayerFullLayers = 0</B> </li> <br>
             *   Number of complete layers.
             *   \if (POINTWISE)
             *   <br> This overrides the PW:DomainFullLayers attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "boundaryLayerFullLayers";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &(*meshProps)[i].boundaryLayerFullLayers);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>boundaryLayerGrowthRate = 1</B> </li> <br>
             *   Growth rate for boundary layers.
             *   \if (POINTWISE)
             *   <br> This overrides the PW:DomainTRexGrowthRate attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "boundaryLayerGrowthRate";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].boundaryLayerGrowthRate );
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>nodeSpacing = 0.0</B> </li> <br>
             *   Spacing at a NODE or ends of an EDGE.<br>
             *   The spacing in the mesh is is given by<br>
             *   meshNodeSpacing = capsMeshLength * nodeSpacing
             *   \if (POINTWISE)
             *   <br> This overrides the PW:NodeSpacing attribute on the NODEs and PW:ConnectorEndSpacing attribute on EDGEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "nodeSpacing";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].nodeSpacing);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>minSpacing = 0.0</B> </li> <br>
             *   Minimum spacing on a FACE.<br>
             *   The spacing in the mesh is is given by<br>
             *   meshMinSpacing = capsMeshLength * minSpacing
             *   \if (POINTWISE)
             *   <br> This overrides the PW:DomainMinEdge attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "minSpacing";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].maxSpacing);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>maxSpacing = 0.0</B> </li> <br>
             *   Maximum spacing on an EDGE or FACE.<br>
             *   The spacing in the mesh is is given by<br>
             *   meshMaxSpacing = capsMeshLength * maxSpacing
             *   \if (POINTWISE)
             *   <br> This overrides the PW:ConnectorMaxEdge attribute on EDGEs
             *   and PW:DomainMaxEdge attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "maxSpacing";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].maxSpacing);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }


            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>avgSpacing = 0.0</B> </li> <br>
             *   Average spacing on an EDGE.<br>
             *   The spacing in the mesh is is given by<br>
             *   meshAvgSpacing = capsMeshLength * avgSpacing
             *   \if (POINTWISE)
             *   <br> This overrides the PW:ConnectorAverageDS attribute on EDGEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "avgSpacing";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].avgSpacing);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>maxAngle = 0.0</B>  [Range 0 to 180] </li> <br>
             *   Maximum angle to set spacings on an EDGE.
             *   \if (POINTWISE)
             *   <br> This overrides the PW:ConnectorMaxAngle attribute on EDGEs
             *   and PW:DomainMaxAngle attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "maxAngle";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].maxAngle);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>maxDeviation = 0.0</B> </li> <br>
             *   Maximum deviation to set spacing on an EDGE or FACE.<br>
             *   The spacing in the mesh is is given by<br>
             *   meshMaxDeviation = capsMeshLength * maxDeviation
             *   \if (POINTWISE)
             *   <br> This overrides the PW:ConnectorMaxDeviation attribute on EDGEs
             *   and PW:DomainMaxDeviation attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "maxDeviation";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].maxDeviation);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             *\if (POINTWISE)
             *
             * <ul>
             * <li>  <B>boundaryDecay = 0.0</B> [ Range 0 to 1 ]</li> <br>
             *   Decay of influence of the boundary spacing on the interior spacing.
             *   \if (POINTWISE)
             *   <br> This overrides the PW:DomainDecay attribute on FACEs.
             *   \endif
             * </ul>
             * \endif
             */
            keyWord = "boundaryDecay";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].boundaryDecay);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            // Face properties
            /*! \page meshSizingProp
             *
             * \if (AFLR2 || AFLR3 || TETGEN || EGADSTESS)
             *
             * <ul>
             * <li>  <B>tessParams = (no default) </B> </li> <br>
             * Face tessellation parameters, example [0.1, 0.01, 20.0]. (From the EGADS manual) A set of 3 parameters that drive the EDGE discretization
             * and the FACE triangulation. The first is the maximum length of an EDGE segment or triangle side
             * (in physical space). A zero is flag that allows for any length. The second is a curvature-based
             * value that looks locally at the deviation between the centroid of the discrete object and the
             * underlying geometry. Any deviation larger than the input value will cause the tessellation to
             * be enhanced in those regions. The third is the maximum interior dihedral angle (in degrees)
             * between triangle facets (or Edge segment tangents for a WIREBODY tessellation), note that a
             * zero ignores this phase.
             * </ul>
             *
             * \endif
             */
            keyWord = "tessParams";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleArray(keyValue, 3, (*meshProps)[i].tessParams);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;

                (*meshProps)[i].useTessParams = (int) true;

            }

            /*! \page meshSizingProp
             *
             * \if (AFLR4)
             *
             * <ul>
             * <li>  <B>bcType = (no default) </B> </li> <br>
             * bcType sets the AFLR_GBC attribute on faces.<br>
             * <br>
             * See AFLR_GBC in \ref attributeAFLR4 for additional details.
             * </ul>
             *
             * \endif
             */
            keyWord = "bcType";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                // remove "" from the keyValue
                stringLen = strlen(keyValue);
                if (stringLen > 2) {
                    (*meshProps)[i].bcType = (char*)EG_alloc(stringLen-1);
                    memcpy((*meshProps)[i].bcType, keyValue+1, stringLen-2);
                    (*meshProps)[i].bcType[stringLen-2] = '\0';
                } else {
                    printf("**********************************************************\n");
                    printf("Error: \"bcType\" cannot be an empty string\n");
                    printf("**********************************************************\n");
                    status = CAPS_BADVALUE;
                }

                EG_free(keyValue);
                keyValue = NULL;
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             * \if (AFLR4)
             *
             * <ul>
             * <li>  <B>scaleFactor = (no default) </B> </li> <br>
             * scaleFactor sets the AFLR4_Scale_Factor attribute on faces.<br>
             * <br>
             * See AFLR4_Scale_Factor in \ref attributeAFLR4 for additional details.
             * </ul>
             *
             * \endif
             */
            keyWord = "scaleFactor";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].scaleFactor);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page meshSizingProp
             *
             * \if (AFLR4)
             *
             * <ul>
             * <li>  <B>edgeWeight = (no default) [Range 0 to 1]</B> </li> <br>
             * edgeWeight sets the AFLR4_Edge_Scale_Factor_Weight attribute on faces.<br>
             * <br>
             * See AFLR4_Edge_Scale_Factor_Weight in \ref attributeAFLR4 for additional details.
             * </ul>
             *
             * \endif
             */
            keyWord = "edgeWeight";
            status = search_jsonDictionary( meshBCTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*meshProps)[i].edgeWeight);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

        } else {
            /*! \page meshSizingProp
             * \section keyStringMeshSizing Single Value String
             *
             * If "Value" is a single string, the following options maybe used:
             * - (NONE Currently)
             * */

        }
    }

    printf("\tDone getting mesh sizing parameters\n");
    return CAPS_SUCCESS;
}


// Initiate (0 out all values and NULL all pointers) a CFD mesh data in the cfdMeshDataStruct structure format
int initiate_cfdMeshDataStruct (cfdMeshDataStruct *data) {

    if (data == NULL) return CAPS_NULLVALUE;

    data->bcID = 0;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a CFD mesh data in the cfdMeshDataStruct structure format
int destroy_cfdMeshDataStruct (cfdMeshDataStruct *data) {

    if (data == NULL) return CAPS_SUCCESS;

    data->bcID = 0;

    return CAPS_SUCCESS;
}

// Copy a CFD mesh data in the cfdMeshDataStruct structure format
int copy_cfdMeshDataStruct (cfdMeshDataStruct *dataIn, cfdMeshDataStruct *dataOut) {

    if (dataIn == NULL) return CAPS_NULLVALUE;
    if (dataOut == NULL) return CAPS_NULLVALUE;

    dataOut->bcID = dataIn->bcID;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a FEA mesh data in the feaMeshDataStruct structure format
int initiate_feaMeshDataStruct (feaMeshDataStruct *data) {

    if (data == NULL) return CAPS_NULLVALUE;

    data->coordID = 0;
    data->propertyID = 0;

    data->constraintIndex = 0;
    data->loadIndex = 0;
    data->transferIndex = 0;

    data->connectIndex = 0;
    data->connectLinkIndex = 0;

    data->elementSubType = UnknownMeshSubElement;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a FEA mesh data in the feaMeshDataStruct structure format
int destroy_feaMeshDataStruct (feaMeshDataStruct *data) {

    if (data == NULL) return CAPS_SUCCESS;

    data->coordID = 0;
    data->propertyID = 0;

    data->constraintIndex = 0;
    data->loadIndex = 0;
    data->transferIndex = 0;

    data->connectIndex = 0;
    data->connectLinkIndex = 0;

    data->elementSubType = UnknownMeshSubElement;

    return CAPS_SUCCESS;
}

// Copy a FEA mesh data in the feaMeshDataStruct structure format
int copy_feaMeshDataStruct (feaMeshDataStruct *dataIn, feaMeshDataStruct *dataOut) {

    if (dataIn == NULL) return CAPS_NULLVALUE;
    if (dataOut == NULL) return CAPS_NULLVALUE;

    dataOut->coordID = dataIn->coordID;
    dataOut->propertyID = dataIn->propertyID;

    dataOut->constraintIndex = dataIn->constraintIndex;
    dataOut->loadIndex = dataIn->loadIndex;
    dataOut->transferIndex = dataIn->transferIndex;

    dataOut->connectIndex = dataIn->connectIndex;
    dataOut->connectLinkIndex = dataIn->connectLinkIndex;

    dataOut->elementSubType = dataIn->elementSubType;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a Origami mesh data in the origamiMeshDataStruct structure format
int initiate_origamiMeshDataStruct (origamiMeshDataStruct *data) {

    if (data == NULL) return CAPS_NULLVALUE;

    data->propertyID = 0;

    data->constraintIndex = 0;
    data->loadIndex = 0;
    data->transferIndex = 0;

    data->neighborNodes[0] = 0;
    data->neighborNodes[1] = 0;

    data->foldLine = (int) true;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a Origami mesh data in the origamiMeshDataStruct structure format
int destroy_origamiMeshDataStruct (origamiMeshDataStruct *data) {

    if (data == NULL) return CAPS_SUCCESS;

    data->propertyID = 0;

    data->constraintIndex = 0;
    data->loadIndex = 0;
    data->transferIndex = 0;

    data->neighborNodes[0] = 0;
    data->neighborNodes[1] = 0;

    data->foldLine = (int) true;

    return CAPS_SUCCESS;
}


// Copy a Origami mesh data in the origamiMeshDataStruct structure format
int copy_origamiMeshDataStruct (origamiMeshDataStruct *dataIn, origamiMeshDataStruct *dataOut) {

    if (dataIn == NULL) return CAPS_NULLVALUE;
    if (dataOut == NULL) return CAPS_NULLVALUE;

    dataOut->propertyID = dataIn->propertyID;

    dataOut->constraintIndex = dataIn->constraintIndex;
    dataOut->loadIndex = dataIn->loadIndex;
    dataOut->transferIndex = dataIn->transferIndex;

    dataOut->neighborNodes[0] = dataIn->neighborNodes[0];
    dataOut->neighborNodes[1] = dataIn->neighborNodes[1];

    dataOut->foldLine = dataIn->foldLine;

    return CAPS_SUCCESS;
}


// Initiate  (0 out all values and NULL all pointers) and allocate the analysisData void pointer. Creation selected based on type.
int initiate_analysisData(void **analysisData, meshAnalysisTypeEnum analysisType) {

    int status; // Function return status

    cfdMeshDataStruct *cfdData;
    feaMeshDataStruct *feaData;

    origamiMeshDataStruct *origamiData;

    // Unknown type
    if (analysisType == UnknownMeshAnalysis) *analysisData = NULL;

    // Initiate CFD mesh data
    if (analysisType == MeshCFD) {

        //if (*analysisData != NULL) EG_free((cfdMeshDataStruct *) *analysisData);

        cfdData = (cfdMeshDataStruct *) EG_alloc(sizeof(cfdMeshDataStruct));

        status = initiate_cfdMeshDataStruct( cfdData);
        if (status != CAPS_SUCCESS) printf("Error in initiate_cfdMeshDataStruct, status = %d\n", status);

        *analysisData = (void *) cfdData;
    }

    // Initiate FEA mesh data
    if (analysisType == MeshStructure) {

        //if (*analysisData != NULL) EG_free((feaMeshDataStruct *) *analysisData);

        feaData = (feaMeshDataStruct *) EG_alloc(sizeof(feaMeshDataStruct));

        status = initiate_feaMeshDataStruct( feaData);
        if (status != CAPS_SUCCESS) printf("Error in initiate_feaMeshDataStruct, status = %d\n", status);

        *analysisData = (void *) feaData;

    }

    // Initiate Origami mesh data
    if (analysisType == MeshOrigami) {

        origamiData = (origamiMeshDataStruct *) EG_alloc(sizeof(origamiMeshDataStruct));

        status = initiate_origamiMeshDataStruct( origamiData);
        if (status != CAPS_SUCCESS) printf("Error in initiate_origamiMeshDataStruct, status = %d\n", status);

        *analysisData = (void *) origamiData;

    }

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) and free of the analysisData void pointer. Correct destroy function selected based on type.
int destroy_analysisData(void **analysisData, meshAnalysisTypeEnum analysisType) {

    int status; // Function return status

    cfdMeshDataStruct *cfdData;
    feaMeshDataStruct *feaData;


    origamiMeshDataStruct *origamiData;

    if (*analysisData == NULL) return CAPS_SUCCESS;

    // Destroy CFD mesh data
    if (analysisType == MeshCFD) {
        cfdData = (cfdMeshDataStruct *) *analysisData;

        status = destroy_cfdMeshDataStruct( cfdData);
        if (status != CAPS_SUCCESS) printf("Error in destroy_cfdMeshDataStruct, status = %d\n", status);

        EG_free(cfdData);
    }

    // Destroy FEA mesh data
    if (analysisType == MeshStructure) {
        feaData = (feaMeshDataStruct *) *analysisData;

        status = destroy_feaMeshDataStruct( feaData);
        if (status != CAPS_SUCCESS) printf("Error in destroy_feaMeshDataStruct, status = %d\n", status);

        EG_free(feaData);
    }

    // Destroy Origami mesh data
    if (analysisType == MeshOrigami) {
        origamiData = (origamiMeshDataStruct *) *analysisData;

        status = destroy_origamiMeshDataStruct( origamiData);
        if (status != CAPS_SUCCESS) printf("Error in destroy_origamiMeshDataStruct, status = %d\n", status);

        EG_free(origamiData);
    }

    *analysisData = NULL;

    return CAPS_SUCCESS;
}


// Initiate (0 out all values and NULL all pointers) a node data in the meshGeomDataStruct structure format
int initiate_meshGeomDataStruct(meshGeomDataStruct *geom) {

    // These may need to change in the future

    // Node/Vertex geometry information
    geom->uv[0] = 0.0;
    geom->uv[1] = 0.0;

    geom->firstDerivative[0] = 0.0;
    geom->firstDerivative[1] = 0.0;
    geom->firstDerivative[2] = 0.0;
    geom->firstDerivative[3] = 0.0;
    geom->firstDerivative[4] = 0.0;
    geom->firstDerivative[5] = 0.0;

    geom->type = 0; // The point type (-) Face local index, (0) Node, (+) Edge local index
    geom->topoIndex = 0; // The point topological index (1 bias)

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a node data in the meshGeomDataStruct structure format
int destroy_meshGeomDataStruct(meshGeomDataStruct *geom) {

    // These may need to change in the future

    // Node/Vertex geometry information
    geom->uv[0] = 0.0;
    geom->uv[1] = 0.0;

    geom->firstDerivative[0] = 0.0;
    geom->firstDerivative[1] = 0.0;
    geom->firstDerivative[2] = 0.0;
    geom->firstDerivative[3] = 0.0;
    geom->firstDerivative[4] = 0.0;
    geom->firstDerivative[5] = 0.0;

    geom->type = 0; // The point type (-) Face local index, (0) Node, (+) Edge local index
    geom->topoIndex = 0; // The point topological index (1 bias)

    return CAPS_SUCCESS;
}

// Copy geometry mesh data in the meshGeomDataStruct structure format
int copy_meshGeomDataStruct(meshGeomDataStruct *dataIn, meshGeomDataStruct *dataOut) {

    if (dataIn == NULL) return CAPS_NULLVALUE;
    if (dataOut == NULL) return CAPS_NULLVALUE;

    dataOut->uv[0] = dataIn->uv[0];
    dataOut->uv[1] = dataIn->uv[1];

    dataOut->firstDerivative[0] = dataIn->firstDerivative[0];
    dataOut->firstDerivative[1] = dataIn->firstDerivative[1];
    dataOut->firstDerivative[2] = dataIn->firstDerivative[2];
    dataOut->firstDerivative[3] = dataIn->firstDerivative[3];
    dataOut->firstDerivative[4] = dataIn->firstDerivative[4];
    dataOut->firstDerivative[5] = dataIn->firstDerivative[5];

    dataOut->type = dataIn->type;
    dataOut->topoIndex = dataIn->topoIndex;

    return CAPS_SUCCESS;
}


// Initiate (0 out all values and NULL all pointers) a node data in the meshNode structure format
int initiate_meshNodeStruct(meshNodeStruct *node, meshAnalysisTypeEnum meshAnalysisType) {

    if (node == NULL) return CAPS_NULLVALUE;

    node->xyz[0] = 0.0;
    node->xyz[1] = 0.0;
    node->xyz[2] = 0.0;

    node->nodeID = 0;

    node->analysisType = meshAnalysisType;

    (void) initiate_analysisData(&node->analysisData, node->analysisType);

    node->geomData = NULL;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a node data in the meshNode structure format
int destroy_meshNodeStruct(meshNodeStruct *node) {

    if (node == NULL) return CAPS_SUCCESS;

    node->xyz[0] = 0.0;
    node->xyz[1] = 0.0;
    node->xyz[2] = 0.0;

    node->nodeID = 0;

    (void) destroy_analysisData(&node->analysisData, node->analysisType);

    node->analysisType = UnknownMeshAnalysis;
    node->analysisData = NULL;

    if (node->geomData != NULL) {
        (void) destroy_meshGeomDataStruct(node->geomData);

        EG_free(node->geomData);
    }

    node->geomData = NULL;

    return CAPS_SUCCESS;
}

// Update/change the analysis data in a meshNodeStruct
int change_meshNodeAnalysis(meshNodeStruct *node, meshAnalysisTypeEnum meshAnalysisType) {

    if (node == NULL) return CAPS_NULLVALUE;

    if (meshAnalysisType ==  node->analysisType) return CAPS_SUCCESS;

    (void) destroy_analysisData(&node->analysisData, node->analysisType);

    node->analysisType = meshAnalysisType;
    (void) initiate_analysisData(&node->analysisData, node->analysisType);

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a element data in the meshElement structure format
int initiate_meshElementStruct(meshElementStruct *element, meshAnalysisTypeEnum meshAnalysisType) {

    if (element == NULL) return CAPS_NULLVALUE;

    element->elementType = UnknownMeshElement;

    element->elementID = 0;

    element->markerID = 0;

    element->topoIndex = -1;

    element->connectivity = NULL; // size[elementType-specific]

    element->analysisType = meshAnalysisType;

    (void) initiate_analysisData(&element->analysisData, element->analysisType);

    //element->geomData = NULL;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a element data in the meshElementStruct structure format
int destroy_meshElementStruct(meshElementStruct *element) {

    if (element == NULL) return CAPS_SUCCESS;

    element->elementType = UnknownMeshElement;

    element->elementID = 0;

    element->markerID = 0;

    element->topoIndex = -1;

    if (element->connectivity != NULL) EG_free(element->connectivity);
    element->connectivity = NULL; // size[elementType-specific]

    (void) destroy_analysisData(&element->analysisData, element->analysisType);

    element->analysisType = UnknownMeshAnalysis;

    element->analysisData = NULL;

    /*
	if (element->geomData != NULL) {
		(void) destroy_meshGeomDataStruct(element->geomData);

		EG_free(element->geomData);
	}

	element->geomData = NULL;
     */

    return CAPS_SUCCESS;
}

// Update/change the analysis data in a meshElementStruct
int change_meshElementAnalysis(meshElementStruct *element, meshAnalysisTypeEnum meshAnalysisType) {

    if (element == NULL) return CAPS_NULLVALUE;

    if (meshAnalysisType ==  element->analysisType) return CAPS_SUCCESS;

    (void) destroy_analysisData(&element->analysisData, element->analysisType);

    element->analysisType = meshAnalysisType;
    (void) initiate_analysisData(&element->analysisData, element->analysisType );

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) the meshQuickRef data in the meshQuickRefStruct structure format
int initiate_meshQuickRefStruct(meshQuickRefStruct *quickRef) {

    quickRef->useStartIndex = (int) false; // Use the start index reference
    quickRef->useListIndex = (int) false; // Use the list of indexes

    // Number of elements per type
    quickRef->numNode = 0;
    quickRef->numLine = 0;
    quickRef->numTriangle = 0;
    quickRef->numTriangle_6 = 0;
    quickRef->numQuadrilateral = 0;
    quickRef->numQuadrilateral_8 = 0;
    quickRef->numTetrahedral = 0;
    quickRef->numTetrahedral_10 = 0;
    quickRef->numPyramid = 0;
    quickRef->numPrism = 0;
    quickRef->numHexahedral = 0;

    // If the element types are created in order - starting index in the element array of a particular element in type
    quickRef->startIndexNode = -1;
    quickRef->startIndexLine = -1;
    quickRef->startIndexTriangle = -1;
    quickRef->startIndexTriangle_6 = -1;
    quickRef->startIndexQuadrilateral = -1;
    quickRef->startIndexQuadrilateral_8 = -1;
    quickRef->startIndexTetrahedral = -1;
    quickRef->startIndexTetrahedral_10 = -1;
    quickRef->startIndexPyramid = -1;
    quickRef->startIndexPrism = -1;
    quickRef->startIndexHexahedral = -1;

    // Array of element indexes containing a specific element type
    quickRef->listIndexNode = NULL; // size[numNode]
    quickRef->listIndexLine = NULL; // size[numLine]
    quickRef->listIndexTriangle = NULL; // size[numTriangle]
    quickRef->listIndexTriangle_6 = NULL; // size[numTriangle_6]
    quickRef->listIndexQuadrilateral = NULL; // size[numQuadrilateral]
    quickRef->listIndexQuadrilateral_8 = NULL; // size[numQuadrilateral_8]
    quickRef->listIndexTetrahedral = NULL; // size[numTetrahedral]
    quickRef->listIndexTetrahedral_10 = NULL; // size[numTetrahedral_10]
    quickRef->listIndexPyramid = NULL; // size[numPyramid]
    quickRef->listIndexPrism = NULL; // size[numPrism]
    quickRef->listIndexHexahedral = NULL; // size[numHexahedral]

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) the meshQuickRef data in the meshQuickRefStruct structure format
int destroy_meshQuickRefStruct(meshQuickRefStruct *quickRef) {

    quickRef->useStartIndex = (int) false; // Use the start index reference
    quickRef->useListIndex = (int) false; // Use the list of indexes

    // Number of elements per type
    quickRef->numNode = 0;
    quickRef->numLine = 0;
    quickRef->numTriangle = 0;
    quickRef->numTriangle_6 = 0;
    quickRef->numQuadrilateral = 0;
    quickRef->numQuadrilateral_8 = 0;
    quickRef->numTetrahedral = 0;
    quickRef->numTetrahedral_10 = 0;
    quickRef->numPyramid = 0;
    quickRef->numPrism = 0;
    quickRef->numHexahedral = 0;

    // If the element types are created in order - starting index in the element array of a particular element in type
    quickRef->startIndexNode = -1;
    quickRef->startIndexLine = -1;
    quickRef->startIndexTriangle = -1;
    quickRef->startIndexTriangle_6 = -1;
    quickRef->startIndexQuadrilateral = -1;
    quickRef->startIndexQuadrilateral_8 = -1;
    quickRef->startIndexTetrahedral = -1;
    quickRef->startIndexTetrahedral_10 = -1;
    quickRef->startIndexPyramid = -1;
    quickRef->startIndexPrism = -1;
    quickRef->startIndexHexahedral = -1;

    // Array of element indexes containing a specific element type
    if (quickRef->listIndexNode != NULL) EG_free(quickRef->listIndexNode);
    quickRef->listIndexNode = NULL; // size[numNode]

    if (quickRef->listIndexLine != NULL) EG_free(quickRef->listIndexLine);
    quickRef->listIndexLine = NULL; // size[numLine]

    if (quickRef->listIndexTriangle != NULL) EG_free(quickRef->listIndexTriangle);
    quickRef->listIndexTriangle = NULL; // size[numTriangle]

    if (quickRef->listIndexTriangle_6 != NULL) EG_free(quickRef->listIndexTriangle_6);
    quickRef->listIndexTriangle_6 = NULL; // size[numTriangle_6]

    if (quickRef->listIndexQuadrilateral != NULL) EG_free(quickRef->listIndexQuadrilateral);
    quickRef->listIndexQuadrilateral = NULL; // size[numQuadrilateral]

    if (quickRef->listIndexQuadrilateral_8 != NULL) EG_free(quickRef->listIndexQuadrilateral_8);
       quickRef->listIndexQuadrilateral_8 = NULL; // size[numQuadrilateral_8]

    if (quickRef->listIndexTetrahedral != NULL) EG_free(quickRef->listIndexTetrahedral);
    quickRef->listIndexTetrahedral = NULL; // size[numTetrahedral]

    if (quickRef->listIndexTetrahedral_10 != NULL) EG_free(quickRef->listIndexTetrahedral_10);
    quickRef->listIndexTetrahedral_10 = NULL; // size[numTetrahedral_10]

    if (quickRef->listIndexPyramid != NULL) EG_free(quickRef->listIndexPyramid);
    quickRef->listIndexPyramid = NULL; // size[numPyramid]

    if (quickRef->listIndexPrism != NULL) EG_free(quickRef->listIndexPrism);
    quickRef->listIndexPrism = NULL; // size[numPrism]

    if (quickRef->listIndexHexahedral != NULL) EG_free(quickRef->listIndexHexahedral);
    quickRef->listIndexHexahedral = NULL; // size[numHexahedral]

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) all nodes in the mesh
int destroy_meshNodes(meshStruct *mesh) {

    int status; // Function status return
    int i; // Indexing

    if (mesh->node != NULL) {
        for (i = 0; i < mesh->numNode; i++) {
            status = destroy_meshNodeStruct( &mesh->node[i]);
            if (status != CAPS_SUCCESS) printf("Error in destroy_meshNodeStruct, status = %d\n", status);
        }

        EG_free(mesh->node);
    }
    mesh->numNode = 0;
    mesh->node = NULL;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) all elements in the mesh
int destroy_meshElements(meshStruct *mesh) {

    int status; // Function status return
    int i; // Indexing

    if (mesh->element != NULL) {
        for (i = 0; i < mesh->numElement; i++) {

            status = destroy_meshElementStruct( &mesh->element[i]);
            if (status != CAPS_SUCCESS) printf("Error in destroy_meshElementStruct, status = %d\n", status);
        }

        EG_free(mesh->element);
    }

    mesh->numElement = 0;
    mesh->element = NULL;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) a mesh data in the meshStruct structure format
int initiate_meshStruct(meshStruct *mesh) {

    if (mesh == NULL) return CAPS_NULLVALUE;

    //mesh->meshDimensionality = UnknownMeshDimension;
    mesh->meshType = UnknownMeshType;

    mesh->analysisType = UnknownMeshAnalysis;

    mesh->numNode = 0;
    mesh->node = NULL; // size[numNode]

    mesh->numElement = 0;
    mesh->element = NULL; // size[numElements]

    mesh->numReferenceMesh = 0; // Number of reference meshes
    mesh->referenceMesh = NULL; // Pointers to other meshes should be freed, but no individual references, size[numReferenceMesh]

    (void) initiate_meshQuickRefStruct(&mesh->meshQuickRef);

    (void) initiate_bodyTessMappingStruct(&mesh->bodyTessMap);

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) a mesh data in the meshStruct structure format
int destroy_meshStruct(meshStruct *mesh) {

    if (mesh == NULL) return CAPS_NULLVALUE;

    //mesh->meshDimensionality = UnknownMeshDimension;
    mesh->meshType = UnknownMeshType;

    mesh->analysisType = UnknownMeshAnalysis;

    (void) destroy_meshNodes(mesh);

    (void) destroy_meshElements(mesh);

    mesh->numReferenceMesh = 0; // Number of reference meshes

    EG_free(mesh->referenceMesh);
    mesh->referenceMesh = NULL; // Pointers to other meshes, should be freed but not individual references, size[numReferenceMesh]

    (void) destroy_meshQuickRefStruct(&mesh->meshQuickRef);

    (void) destroy_bodyTessMappingStruct(&mesh->bodyTessMap);

    return CAPS_SUCCESS;
}

// Update/change the analysis data in a meshStruct
int change_meshAnalysis(meshStruct *mesh, meshAnalysisTypeEnum meshAnalysisType) {

    int status; // Function return status

    int i; // Indexing

    if (mesh == NULL) return CAPS_NULLVALUE;

    if (meshAnalysisType ==  mesh->analysisType) return CAPS_SUCCESS;

    mesh->analysisType = meshAnalysisType;

    for (i = 0; i < mesh->numNode; i++){
        status = change_meshNodeAnalysis(&mesh->node[i], mesh->analysisType);
        if (status != CAPS_SUCCESS) return status;
    }

    for (i = 0; i < mesh->numElement; i++){
        status = change_meshElementAnalysis(&mesh->element[i], mesh->analysisType);
        if (status != CAPS_SUCCESS) return status;
    }

    return CAPS_SUCCESS;
}


// Return the number of connectivity points based on type
int mesh_numMeshConnectivity(meshElementTypeEnum elementType) {

    int numPoint = 0;

    if (elementType == UnknownMeshElement) numPoint = 0;
    if (elementType == Node)               numPoint = 1;
    if (elementType == Line)               numPoint = 2;
    if (elementType == Triangle)           numPoint = 3;
    if (elementType == Triangle_6)         numPoint = 6;
    if (elementType == Quadrilateral)      numPoint = 4;
    if (elementType == Quadrilateral_8)    numPoint = 8;
    if (elementType == Tetrahedral)        numPoint = 4;
    if (elementType == Tetrahedral_10)     numPoint = 10;
    if (elementType == Pyramid)            numPoint = 5;
    if (elementType == Prism)              numPoint = 6;
    if (elementType == Hexahedral)         numPoint = 8;

    return numPoint;
}

// Return the number of connectivity points based on type of element provided
int mesh_numMeshElementConnectivity(meshElementStruct *element) {

    int numPoint = 0;

    if (element == NULL) return CAPS_NULLVALUE;

    numPoint = mesh_numMeshConnectivity(element->elementType);

    return numPoint;
}

// Allocate mesh element connectivity array based on type
int mesh_allocMeshElementConnectivity(meshElementStruct *element) {

    int i; // Indexing

    int numPoint;

    if (element == NULL) return CAPS_NULLVALUE;

    if (element->connectivity != NULL) EG_free(element->connectivity);
    element->connectivity = NULL;

    if (element->elementType == UnknownMeshElement) return CAPS_BADVALUE;

    numPoint = mesh_numMeshElementConnectivity(element);

    element->connectivity = (int *) EG_alloc(numPoint*sizeof(int));
    if (element->connectivity == NULL) return EGADS_MALLOC;

    // Set to zero
    for (i = 0; i < numPoint; i++) element->connectivity[i] = 0;

    return CAPS_SUCCESS;
}

// Retrieve the number of mesh element of a given type
int mesh_retrieveNumMeshElements(int numElement,
                                 meshElementStruct element[],
                                 meshElementTypeEnum elementType,
                                 int *numElementType) {
    int i; // Indexing

    if (numElement == 0) return CAPS_BADVALUE;

    *numElementType = 0;
    for (i = 0; i < numElement; i++) {
        if (element[i].elementType != elementType) continue;

        *numElementType += 1;
    }

    return CAPS_SUCCESS;
}

// Retrieve the starting index of a given type -assume elements were put in order
int mesh_retrieveStartIndexMeshElements(int numElement,
                                        meshElementStruct element[],
                                        meshElementTypeEnum elementType,
                                        int *numElementType,
                                        int *startIndex) {

    int status;
    int i;
    int found = (int) false;

    if (numElement == 0) return CAPS_BADVALUE;

    *startIndex = -1;
    status = mesh_retrieveNumMeshElements(numElement,
                                          element,
                                          elementType,
                                          numElementType);
    if (status != CAPS_SUCCESS) return status;

    for (i = 0; i < numElement; i++) {
        if (element[i].elementType == elementType) {
            found = (int) true;
            break;
        }
    }

    if (found == (int) false) {
        *startIndex = -1;
        return CAPS_NOTFOUND;
    } else {
        *startIndex = i;
        return CAPS_SUCCESS;
    }
}

// Retrieve list of mesh element of a given type - elementTypeList is freeable
int mesh_retrieveMeshElements(int numElement,
                              meshElementStruct element[],
                              meshElementTypeEnum elementType,
                              int *numElementType,
                              int *elementTypeList[]) {

    int i; // Indexing

    int *tempList = NULL;

    if (element == NULL) return CAPS_NULLVALUE;

    if (*elementTypeList != NULL) EG_free(*elementTypeList);
    *elementTypeList = NULL;

    if (numElement == 0) return CAPS_BADVALUE;

    tempList = (int *) EG_alloc(numElement*sizeof(int));
    if (tempList == NULL) return EGADS_MALLOC;

    *numElementType = 0;
    for (i = 0; i < numElement; i++) {
        tempList[i] = (int) false;

        if (element[i].elementType != elementType) continue;

        tempList[i] = (int) true;
        *numElementType += 1;
    }

    if (*numElementType == 0) {
        EG_free(tempList);
        return CAPS_NOTFOUND;
    }

    *elementTypeList = (int *) EG_alloc((*numElementType)*sizeof(int));
    if (*elementTypeList == NULL) {
        EG_free(tempList);
        return EGADS_MALLOC;
    }

    *numElementType = 0;
    for (i = 0; i < numElement; i++ ) {
        if (tempList[i] == (int) true) {
            (*elementTypeList)[*numElementType] = i;
            *numElementType += 1;
        }
    }

    EG_free(tempList);

    return CAPS_SUCCESS;

}

// Fill out the QuickRef lists for all element types
int mesh_fillQuickRefList( meshStruct *mesh) {

    int status;

    status = destroy_meshQuickRefStruct(&mesh->meshQuickRef);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Node
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Node,
                                       &mesh->meshQuickRef.numNode,
                                       &mesh->meshQuickRef.listIndexNode);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    // Line
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Line,
                                       &mesh->meshQuickRef.numLine,
                                       &mesh->meshQuickRef.listIndexLine);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    // Triangle
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Triangle,
                                       &mesh->meshQuickRef.numTriangle,
                                       &mesh->meshQuickRef.listIndexTriangle);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    // Triangle - 6
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Triangle_6,
                                       &mesh->meshQuickRef.numTriangle_6,
                                       &mesh->meshQuickRef.listIndexTriangle_6);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;


    // Quadrilataral
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Quadrilateral,
                                       &mesh->meshQuickRef.numQuadrilateral,
                                       &mesh->meshQuickRef.listIndexQuadrilateral);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    // Quadrilataral - 8
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Quadrilateral_8,
                                       &mesh->meshQuickRef.numQuadrilateral_8,
                                       &mesh->meshQuickRef.listIndexQuadrilateral_8);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;


    // Tetrahedral
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Tetrahedral,
                                       &mesh->meshQuickRef.numTetrahedral,
                                       &mesh->meshQuickRef.listIndexTetrahedral);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    // Tetrahedral - 10
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Tetrahedral_10,
                                       &mesh->meshQuickRef.numTetrahedral_10,
                                       &mesh->meshQuickRef.listIndexTetrahedral_10);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;


    // Pyramid
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Pyramid,
                                       &mesh->meshQuickRef.numPyramid,
                                       &mesh->meshQuickRef.listIndexPyramid );
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    // Prism
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Prism,
                                       &mesh->meshQuickRef.numPrism,
                                       &mesh->meshQuickRef.listIndexPrism );
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    // Hexahedral
    status = mesh_retrieveMeshElements(mesh->numElement,
                                       mesh->element,
                                       Hexahedral,
                                       &mesh->meshQuickRef.numHexahedral,
                                       &mesh->meshQuickRef.listIndexHexahedral);
    if (status != CAPS_NOTFOUND && status != CAPS_SUCCESS) goto cleanup;

    mesh->meshQuickRef.useListIndex = (int) true;

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_fillQuickRefList, status %d\n", status);

        return status;
}

// Copy the QuickRef structure
int mesh_copyQuickRef(meshQuickRefStruct *in,  meshQuickRefStruct *out){

    int status;

    out->useStartIndex = in->useStartIndex; // Use the start index reference
    out->useListIndex = in->useListIndex; // Use the list of indexes

    // Number of elements per type
    out->numNode = in->numNode;
    out->numLine = in->numLine;
    out->numTriangle = in->numTriangle;
    out->numTriangle_6 = in->numTriangle_6;
    out->numQuadrilateral = in->numQuadrilateral;
    out->numQuadrilateral_8 = in->numQuadrilateral_8;
    out->numTetrahedral = in->numTetrahedral;
    out->numTetrahedral_10 = in->numTetrahedral_10;
    out->numPyramid = in->numPyramid;
    out->numPrism = in->numPrism;
    out->numHexahedral = in->numHexahedral;

    // If the element types are created in order - starting index in the element array of a particular element in type
    out->startIndexNode = in->startIndexNode;
    out->startIndexLine =  in->startIndexLine;
    out->startIndexTriangle = in->startIndexTriangle;
    out->startIndexTriangle_6 =  in->startIndexTriangle_6;
    out->startIndexQuadrilateral = in->startIndexQuadrilateral;
    out->startIndexQuadrilateral_8 = in->startIndexQuadrilateral_8;
    out->startIndexTetrahedral = in->startIndexTetrahedral;
    out->startIndexTetrahedral_10 = in->startIndexTetrahedral_10;
    out->startIndexPyramid = in->startIndexPyramid;
    out->startIndexPrism = in->startIndexPrism;
    out->startIndexHexahedral= in->startIndexHexahedral;

     // Array of element indexes containing a specific element type

    status = copy_intArray(in->numNode, in->listIndexNode, &out->listIndexNode);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numLine, in->listIndexLine, &out->listIndexLine);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numTriangle, in->listIndexTriangle, &out->listIndexTriangle);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numTriangle_6, in->listIndexTriangle_6, &out->listIndexTriangle_6);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numQuadrilateral, in->listIndexQuadrilateral, &out->listIndexQuadrilateral);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numQuadrilateral_8, in->listIndexQuadrilateral_8, &out->listIndexQuadrilateral_8);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numTetrahedral, in->listIndexTetrahedral, &out->listIndexTetrahedral);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numTetrahedral_10, in->listIndexTetrahedral_10, &out->listIndexTetrahedral_10);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numPyramid, in->listIndexPyramid, &out->listIndexPyramid);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numPrism, in->listIndexPrism, &out->listIndexPrism);
    if (status != CAPS_SUCCESS) return status;

    status = copy_intArray(in->numHexahedral, in->listIndexHexahedral, &out->listIndexHexahedral);
    if (status != CAPS_SUCCESS) return status;

    return CAPS_SUCCESS;
}

// Make a copy bodyTessMapping structure
int mesh_copyBodyTessMappingStruct(bodyTessMappingStruct *in, bodyTessMappingStruct *out) {

    int i;

    out->egadsTess = in->egadsTess;
    out->numTessFace = in->numTessFace;

    out->tessFaceQuadMap = NULL;

    if (out->numTessFace != 0) {

        out->tessFaceQuadMap = (int *) EG_alloc(out->numTessFace*sizeof(int));

        if (out->tessFaceQuadMap == NULL) return EGADS_MALLOC;

        for (i = 0; i < out->numTessFace; i++) {
            out->tessFaceQuadMap[i] = in->tessFaceQuadMap[i];
        }
    }

    return CAPS_SUCCESS;
}

// Make a copy of the analysis Data
int mesh_copyMeshAnalysisData(void *in, meshAnalysisTypeEnum analysisType, void *out) {

    int status; // Function return status

    cfdMeshDataStruct *cfdDataIn, *cfdDataOut;
    feaMeshDataStruct *feaDataIn, *feaDataOut;

    origamiMeshDataStruct *origamiDataIn, *origamiDataOut;

    if (analysisType == UnknownMeshAnalysis) return CAPS_SUCCESS;

    if (analysisType == MeshCFD) {
        cfdDataIn  = (cfdMeshDataStruct *) in;
        cfdDataOut = (cfdMeshDataStruct *) out;

        status = copy_cfdMeshDataStruct(cfdDataIn, cfdDataOut);
        if (status != CAPS_SUCCESS) return status;

    } else if (analysisType == MeshStructure) {
        feaDataIn  = (feaMeshDataStruct *) in;
        feaDataOut = (feaMeshDataStruct *) out;

        status = copy_feaMeshDataStruct(feaDataIn, feaDataOut);
        if (status != CAPS_SUCCESS) return status;

    } else if (analysisType == MeshOrigami) {
        origamiDataIn  = (origamiMeshDataStruct *) in;
        origamiDataOut = (origamiMeshDataStruct *) out;

        status = copy_origamiMeshDataStruct(origamiDataIn, origamiDataOut);
        if (status != CAPS_SUCCESS) return status;

    } else {

        printf("Unrecognized analysisType %d\n", analysisType);
        return CAPS_NOTFOUND;
    }

    return CAPS_SUCCESS;
}

// Make a copy of an element - may offset the element and connectivity indexing
int mesh_copyMeshElementStruct(meshElementStruct *in, int elementOffSetIndex, int connOffSetIndex, meshElementStruct *out) {

    int status; // Function status return

    int i; // Indexing

    if (in  == NULL) return CAPS_NULLVALUE;
    if (out == NULL) return CAPS_NULLVALUE;

    if (out->analysisType != in->analysisType) {
        status = destroy_meshElementStruct(out);
        if (status != CAPS_SUCCESS) goto cleanup;

        status = initiate_meshElementStruct(out, in->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    out->elementType = in->elementType;

    out->elementID   = in->elementID+elementOffSetIndex;
    out->markerID    = in->markerID;
    out->topoIndex   = in->topoIndex;

    status = mesh_allocMeshElementConnectivity(out);
    if (status != CAPS_SUCCESS) goto cleanup;

    for (i = 0;  i < mesh_numMeshElementConnectivity(out); i++) {
        out->connectivity[i] = in->connectivity[i] + connOffSetIndex;
    }

    out->analysisType = in->analysisType;

    status = mesh_copyMeshAnalysisData(in->analysisData, in->analysisType, out->analysisData);
    if (status != CAPS_SUCCESS) goto cleanup;

    /*
	if (in->geomData != NULL) {
		status = copy_meshGeomDataStruct(in->geomData, out->geomData);
		if (status != CAPS_SUCCESS) goto cleanup;
	} else {
		out->geomData = NULL;
	}
     */

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_copyMeshElementStruct, status = %d\n", status);

        return status;
}

// Make a copy of an node - may offset the node indexing
int mesh_copyMeshNodeStruct(meshNodeStruct *in, int nodeOffSetIndex, meshNodeStruct *out) {

    int status; // Function status return

    if (in  == NULL) return CAPS_NULLVALUE;
    if (out == NULL) return CAPS_NULLVALUE;

    if (out->analysisType != in->analysisType) {
        status = destroy_meshNodeStruct(out);
        if (status != CAPS_SUCCESS) goto cleanup;

        status = initiate_meshNodeStruct(out, in->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    out->nodeID   = in->nodeID+nodeOffSetIndex;

    out->xyz[0] = in->xyz[0];
    out->xyz[1] = in->xyz[1];
    out->xyz[2] = in->xyz[2];

    out->analysisType = in->analysisType;

    status = mesh_copyMeshAnalysisData(in->analysisData, in->analysisType, out->analysisData);
    if (status != CAPS_SUCCESS) goto cleanup;

    if (in->geomData != NULL) {

        if (out->geomData != NULL) {
            (void) destroy_meshGeomDataStruct(out->geomData);
            EG_free(out->geomData);
            out->geomData = NULL;
        }

        out->geomData = (meshGeomDataStruct *) EG_alloc(sizeof(meshGeomDataStruct));
        if (out->geomData == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        status = copy_meshGeomDataStruct(in->geomData, out->geomData);
        if (status != CAPS_SUCCESS) goto cleanup;

    } else {
        out->geomData = NULL;
    }

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_copyMeshNodeStruct, status = %d\n", status);

        return status;
}

// Copy mesh structures
int mesh_copyMeshStruct( meshStruct *in, meshStruct *out ) {

    int status;
    int i; //Indexing

    if (in  == NULL) return CAPS_NULLVALUE;
    if (out == NULL) return CAPS_NULLVALUE;

    status = destroy_meshStruct(out);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Copy types
    out->analysisType = in->analysisType;
    out->meshType = in->meshType;

    // shallow copy of egads tessellations
    status = mesh_copyBodyTessMappingStruct(&in->bodyTessMap, &out->bodyTessMap);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Nodes
    out->numNode = in->numNode;

    out->node = (meshNodeStruct *) EG_alloc(out->numNode*sizeof(meshNodeStruct));
    if (out->node == NULL) {
        printf("Malloc error during node allocation!\n");
        status = EGADS_MALLOC;
        goto cleanup;
    }

    for (i = 0; i < in->numNode; i++) {
        // Initiate node
        status = initiate_meshNodeStruct(&out->node[i], out->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;

        // Copy node
        status = mesh_copyMeshNodeStruct(&in->node[i], 0, &out->node[i]);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // Elements
    out->numElement = in->numElement;

    out->element = (meshElementStruct *) EG_alloc(out->numElement*sizeof(meshElementStruct));
    if ( out->element == NULL ) {
        printf("Malloc error during element allocation!\n");
        status = EGADS_MALLOC;
        goto cleanup;
    }

    for (i = 0; i < in->numElement; i++){

        // Initiate element
        status = initiate_meshElementStruct(&out->element[i], out->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;

        // Copy element
        status = mesh_copyMeshElementStruct(&in->element[i], 0, 0, &out->element[i]);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    status = mesh_copyQuickRef(&in->meshQuickRef,  &out->meshQuickRef);
    if (status != CAPS_SUCCESS) goto cleanup;
//    status = mesh_fillQuickRefList( out );
//    if (status != CAPS_SUCCESS) goto cleanup;

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_copyMeshStruct, status = %d\n", status);

        return status;
}

// Combine mesh structures
int mesh_combineMeshStruct(int numMesh, meshStruct mesh[], meshStruct *combineMesh ) {

    int status;
    int i, j; //Indexing

    int nodeIndexOffSet = 0, elementIndexOffSet = 0;
    int nodeIDOffset = 0, elementIDOffset = 0;

    meshAnalysisTypeEnum analysisType = UnknownMeshAnalysis;
    //meshDimensionalityEnum meshDimensionality;
    meshTypeEnum meshType = UnknownMeshType;

    meshElementStruct *tempElement;
    meshNodeStruct *tempNode;

    if (combineMesh  == NULL) return CAPS_NULLVALUE;

    // Check analysisType
    for (i = 0; i < numMesh; i++) {
        //printf("Analysis type = %d\n", mesh[i].analysisType);
        if (i == 0) {
            analysisType = mesh[i].analysisType;
            continue;
        }

        if (analysisType != mesh[i].analysisType) {
            printf("Inconsistent mesh analysis types when combining meshes!!\n");
            status = CAPS_MISMATCH;
            goto cleanup;
        }
    }

    // Check meshType
    for (i = 0; i < numMesh; i++) {
        if (i == 0) {
            meshType = mesh[i].meshType;
            continue;
        }

        if (meshType != mesh[i].meshType) {
            printf("Warning: Inconsistent mesh types when combining meshes!!\n");
        }

        if (mesh[i].meshType > meshType) meshType = mesh[i].meshType;
    }

    status = destroy_meshStruct(combineMesh);
    if (status != CAPS_SUCCESS) goto cleanup;

    combineMesh->analysisType = analysisType;
    combineMesh->meshType = meshType;

    for (i = 0; i < numMesh; i++) {

        // Nodes
        combineMesh->numNode += mesh[i].numNode;

        if (combineMesh->numNode != 0 && combineMesh->node == NULL) {
            tempNode = (meshNodeStruct *) EG_alloc(combineMesh->numNode*sizeof(meshNodeStruct));
        } else {
            tempNode = (meshNodeStruct *) EG_reall(combineMesh->node,
                    combineMesh->numNode*sizeof(meshNodeStruct));
        }

        if (tempNode == NULL) {
            printf("Malloc error during node allocation!\n");
            status = EGADS_MALLOC;

            combineMesh->numNode -= mesh[i].numNode;
            goto cleanup;
        }

        combineMesh->node = tempNode;

        for (j = 0; j < mesh[i].numNode; j++) {
            // Initiate node
            status = initiate_meshNodeStruct(&combineMesh->node[nodeIndexOffSet + j], analysisType);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Copy node
            status = mesh_copyMeshNodeStruct(&mesh[i].node[j],
                                             nodeIDOffset,
                                             &combineMesh->node[nodeIndexOffSet + j]);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        // Elements
        combineMesh->numElement += mesh[i].numElement;

        if (combineMesh->numElement != 0 && combineMesh->element == NULL) {
            tempElement = (meshElementStruct *) EG_alloc(combineMesh->numElement*sizeof(meshElementStruct));
        } else {
            tempElement = (meshElementStruct *) EG_reall(combineMesh->element,
                    combineMesh->numElement*sizeof(meshElementStruct));
        }

        if (tempElement == NULL) {
            printf("Malloc error during element allocation!\n");
            status = EGADS_MALLOC;

            combineMesh->numElement -= mesh[i].numElement;
            goto cleanup;
        }
        combineMesh->element = tempElement;

        for (j = 0; j < mesh[i].numElement; j++){

            // Initiate element
            status = initiate_meshElementStruct(&combineMesh->element[elementIndexOffSet + j], analysisType);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Copy element
            status = mesh_copyMeshElementStruct(&mesh[i].element[j],
                                                elementIDOffset,
                                                nodeIndexOffSet,
                                                &combineMesh->element[elementIndexOffSet + j]);
            if (status != CAPS_SUCCESS) goto cleanup;

            // The topoIndex maps into a specific body number, which is lost when meshes are combined
            combineMesh->element[elementIndexOffSet + j].topoIndex = -1;
        }

        // stride the ID's
        nodeIDOffset += mesh[i].node[mesh[i].numNode-1].nodeID;
        elementIDOffset += mesh[i].element[mesh[i].numElement-1].elementID;

        // stride the indexing
        nodeIndexOffSet += mesh[i].numNode;
        elementIndexOffSet += mesh[i].numElement;
    }

    status = mesh_fillQuickRefList( combineMesh );
    if (status != CAPS_SUCCESS) goto cleanup;

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_combineMeshStruct, status = %d\n", status);

        return status;
}

// Write a mesh contained in the mesh structure in AFLR3 format (*.ugrid, *.lb8.ugrid, *.b8.ugrid)
int mesh_writeAFLR3(char *fname,
                    int asciiFlag, // 0 for binary, anything else for ascii
                    meshStruct *mesh,
                    double scaleFactor) // Scale factor for coordinates
{

    int status; // Function return status

    FILE *fp = NULL;
    int i, elementIndex; // Indexing variable
    int marker;

    int sint = sizeof(int); // Size of an integer
    int sdouble = sizeof(double); // Size of a double

    //int numBytes = 0; // Number bytes written (unformatted option) // Un-comment if writing an unformatted file
    int machineENDIANNESS = 99;
    char *filename = NULL;
    char *postFix = NULL;

    double tempDouble;

    cfdMeshDataStruct *cfdData;

    if (mesh == NULL) return CAPS_NULLVALUE;

    if (mesh->meshQuickRef.useStartIndex == (int) false &&
        mesh->meshQuickRef.useListIndex  == (int) false) {

        status = mesh_fillQuickRefList( mesh);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    printf("\nWriting AFLR3 file ....\n");

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    if (asciiFlag == 0) {

        machineENDIANNESS = get_MachineENDIANNESS();

        if (machineENDIANNESS == 0) {
            postFix = ".lb8.ugrid";

        } else if (machineENDIANNESS == 1) {
            postFix = ".b8.ugrid";

        } else {
            printf("\tUnable to determine the ENDIANNESS of the current machine for binary file output\n");
            status = CAPS_IOERR;
            goto cleanup;
        }

        filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(postFix)) *sizeof(char));
        if (filename == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        sprintf(filename, "%s%s", fname, postFix);
        fp = fopen(filename, "wb");

        if (fp == NULL) {
            printf("\tUnable to open file: %s\n", filename);
            status = CAPS_IOERR;
            goto cleanup;
        }

        //numBytes = 7 * sint; // Un-comment if writing an unformatted file
        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

        fwrite(&mesh->numNode, sint ,1,fp);
        fwrite(&mesh->meshQuickRef.numTriangle, 	 sint ,1,fp);
        fwrite(&mesh->meshQuickRef.numQuadrilateral, sint ,1,fp);
        fwrite(&mesh->meshQuickRef.numTetrahedral,   sint ,1,fp);
        fwrite(&mesh->meshQuickRef.numPyramid,     	 sint ,1,fp);
        fwrite(&mesh->meshQuickRef.numPrism,     	 sint ,1,fp);
        fwrite(&mesh->meshQuickRef.numHexahedral,    sint ,1,fp);

        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

        /*
        numBytes = 3*mesh->surfaceMesh.numTriFace  * sint +
        		   4*mesh->surfaceMesh.numQuadFace * sint +
				     mesh->surfaceMesh.numTriFace  * sint +
					 mesh->surfaceMesh.numQuadFace * sint +
				   4*mesh->numTetra * sint +
				   5*mesh->numPyr   * sint +
				   6*mesh->numPrz   * sint +
				   8*mesh->numHex   * sint +
				   3*mesh->numNodes * sdouble;
         */
        // Un-comment if writing an unformatted file

        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

        // Write nodal coordinates
        for (i = 0; i < mesh->numNode; i++) {

            tempDouble = mesh->node[i].xyz[0]*scaleFactor;
            fwrite(&tempDouble, sdouble, 1, fp);

            tempDouble = mesh->node[i].xyz[1]*scaleFactor;
            fwrite(&tempDouble, sdouble, 1, fp);

            tempDouble = mesh->node[i].xyz[2]*scaleFactor;
            fwrite(&tempDouble, sdouble, 1, fp);

        }
        // Write tri-faces
        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {
            if (mesh->meshQuickRef.startIndexTriangle >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexTriangle +i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexTriangle[i];
            }

            fwrite(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
        }

        // Write quad-faces
        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {
            if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[i];
            }
            fwrite(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
        }

        // Write tri-face boundaries
        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {
            if (mesh->meshQuickRef.startIndexTriangle >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexTriangle +i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexTriangle[i];
            }

            if (mesh->element[elementIndex].analysisType == MeshCFD) {
                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                marker = cfdData->bcID;
            } else {
                marker = mesh->element[elementIndex].markerID;
            }

            fwrite(&marker, sint, 1, fp);
        }

        // Write quad-face boundaries
        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {
            if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[i];
            }

            if (mesh->element[elementIndex].analysisType == MeshCFD) {
                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                marker = cfdData->bcID;
            } else {
                marker = mesh->element[elementIndex].markerID;
            }

            fwrite(&marker, sint, 1, fp);
        }

        // Write tetrahedrals connectivity
        for (i = 0; i < mesh->meshQuickRef.numTetrahedral; i++) {
            if (mesh->meshQuickRef.startIndexTetrahedral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexTetrahedral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexTetrahedral[i];
            }

            fwrite(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
        }

        // Write pyramid connectivity
        for (i = 0; i < mesh->meshQuickRef.numPyramid; i++) {
            if (mesh->meshQuickRef.startIndexPyramid >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexPyramid + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexPyramid[i];
            }

            fwrite(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[4], sint, 1, fp);
        }

        // Write prisms connectivity
        for (i = 0; i < mesh->meshQuickRef.numPrism; i++) {
            if (mesh->meshQuickRef.startIndexPrism >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexPrism + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexPrism[i];
            }

            fwrite(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[4], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[5], sint, 1, fp);
        }

        // Write hex connectivity
        for (i = 0; i < mesh->meshQuickRef.numHexahedral; i++) {
            if (mesh->meshQuickRef.startIndexHexahedral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexHexahedral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexHexahedral[i];
            }

            fwrite(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[4], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[5], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[6], sint, 1, fp);
            fwrite(&mesh->element[elementIndex].connectivity[7], sint, 1, fp);
        }

        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

        if (mesh->meshType == Surface2DMesh) {

            fwrite(&mesh->meshQuickRef.numLine, sint, 1, fp);

            // Write line-face boundary elements
            for (i = 0; i < mesh->meshQuickRef.numLine; i++) {

                if (mesh->meshQuickRef.startIndexLine >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexLine + i;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexLine[i];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    marker = cfdData->bcID;
                } else {
                    marker = mesh->element[elementIndex].markerID;
                }

                fwrite(mesh->element[elementIndex].connectivity, sint, 2, fp);
                fwrite(&marker, sint, 1, fp);
            }
        }

    } else {

        postFix = ".ugrid";
        filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(postFix)) *sizeof(char));
        if (filename == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        sprintf(filename, "%s%s", fname, postFix);

        fp = fopen(filename, "w");
        if (fp == NULL) {
            printf("\tUnable to open file: %s\n", filename);
            status = CAPS_IOERR;
            goto cleanup;
        }

        //nodes, tri-face, quad-face, numTetra, numPyr, numPrz, numHex
        fprintf(fp,"%d, %d, %d, %d, %d, %d, %d\n", mesh->numNode,
                                                   mesh->meshQuickRef.numTriangle,
                                                   mesh->meshQuickRef.numQuadrilateral,
                                                   mesh->meshQuickRef.numTetrahedral,
                                                   mesh->meshQuickRef.numPyramid,
                                                   mesh->meshQuickRef.numPrism,
                                                   mesh->meshQuickRef.numHexahedral);
        // Write nodal coordinates
        for (i = 0; i < mesh->numNode; i++) {
            fprintf(fp,"%f %f %f\n", mesh->node[i].xyz[0]*scaleFactor,
                                     mesh->node[i].xyz[1]*scaleFactor,
                                     mesh->node[i].xyz[2]*scaleFactor);
        }

        // Write tri-faces
        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {
            if (mesh->meshQuickRef.startIndexTriangle >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexTriangle +i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexTriangle[i];
            }
            fprintf(fp,"%d %d %d\n", mesh->element[elementIndex].connectivity[0],
                                     mesh->element[elementIndex].connectivity[1],
                                     mesh->element[elementIndex].connectivity[2]);
        }

        // Write quad-faces
        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {
            if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[i];
            }
            fprintf(fp,"%d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
                                        mesh->element[elementIndex].connectivity[1],
                                        mesh->element[elementIndex].connectivity[2],
                                        mesh->element[elementIndex].connectivity[3]);
        }

        // Write tri-face boundaries
        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {
            if (mesh->meshQuickRef.startIndexTriangle >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexTriangle +i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexTriangle[i];
            }

            if (mesh->element[elementIndex].analysisType == MeshCFD) {
                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                marker = cfdData->bcID;
            } else {
                marker = mesh->element[elementIndex].markerID;
            }

            fprintf(fp,"%d\n",marker);
        }

        // Write quad-face boundaries
        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {
            if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[i];
            }

            if (mesh->element[elementIndex].analysisType == MeshCFD) {
                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                marker = cfdData->bcID;
            } else {
                marker = mesh->element[elementIndex].markerID;
            }

            fprintf(fp,"%d\n",marker);
        }

        // Write tetrahedrals connectivity
        for (i = 0; i < mesh->meshQuickRef.numTetrahedral; i++) {
            if (mesh->meshQuickRef.startIndexTetrahedral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexTetrahedral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexTetrahedral[i];
            }
            fprintf(fp,"%d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
                                        mesh->element[elementIndex].connectivity[1],
                                        mesh->element[elementIndex].connectivity[2],
                                        mesh->element[elementIndex].connectivity[3]);
        }

        // Write pyramid connectivity
        for (i = 0; i < mesh->meshQuickRef.numPyramid; i++) {
            if (mesh->meshQuickRef.startIndexPyramid >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexPyramid + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexPyramid[i];
            }
            fprintf(fp,"%d %d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
                                           mesh->element[elementIndex].connectivity[1],
                                           mesh->element[elementIndex].connectivity[2],
                                           mesh->element[elementIndex].connectivity[3],
                                           mesh->element[elementIndex].connectivity[4]);
        }

        // Write prisms connectivity
        for (i = 0; i < mesh->meshQuickRef.numPrism; i++) {
            if (mesh->meshQuickRef.startIndexPrism >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexPrism + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexPrism[i];
            }
            fprintf(fp,"%d %d %d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
                                              mesh->element[elementIndex].connectivity[1],
                                              mesh->element[elementIndex].connectivity[2],
                                              mesh->element[elementIndex].connectivity[3],
                                              mesh->element[elementIndex].connectivity[4],
                                              mesh->element[elementIndex].connectivity[5]);
        }

        // Write hex connectivity
        for (i = 0; i < mesh->meshQuickRef.numHexahedral; i++) {
            if (mesh->meshQuickRef.startIndexHexahedral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexHexahedral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexHexahedral[i];
            }
            fprintf(fp,"%d %d %d %d %d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
                                                    mesh->element[elementIndex].connectivity[1],
                                                    mesh->element[elementIndex].connectivity[2],
                                                    mesh->element[elementIndex].connectivity[3],
                                                    mesh->element[elementIndex].connectivity[4],
                                                    mesh->element[elementIndex].connectivity[5],
                                                    mesh->element[elementIndex].connectivity[6],
                                                    mesh->element[elementIndex].connectivity[7]);
        }

        if (mesh->meshType == Surface2DMesh) {

            fprintf(fp,"%d\n", mesh->meshQuickRef.numLine);

            // Write line-face boundary elements
            for (i = 0; i < mesh->meshQuickRef.numLine; i++) {

                if (mesh->meshQuickRef.startIndexLine >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexLine + i;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexLine[i];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    marker = cfdData->bcID;
                } else {
                    marker = mesh->element[elementIndex].markerID;
                }

                fprintf(fp,"%d %d %d\n", mesh->element[elementIndex].connectivity[0],
                                         mesh->element[elementIndex].connectivity[1],
                                         marker);
            }
        }
    }

    printf("Finished writing AFLR3 file\n\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in mesh_writeAFLR3, status = %d\n", status);
        if (fp != NULL) fclose(fp);

        if (filename != NULL) EG_free(filename);

        return status;

}

// Read a mesh into the mesh structure from an AFLR3 format (*.ugrid, *.lb8.ugrid, *.b8.ugrid)
int mesh_readAFLR3(char *fname,
                   meshStruct *mesh,
                   double scaleFactor) // Scale factor for coordinates
{

    int status; // Function return status

    FILE *fp = NULL;
    int i, elementIndex; // Indexing variable
    int marker;
    int asciiFlag;

    int sint = sizeof(int); // Size of an integer
    int sdouble = sizeof(double); // Size of a double

    //int numBytes = 0; // Number bytes written (unformatted option) // Un-comment if writing an unformatted file

    double tempDouble;

    //cfdMeshDataStruct *cfdData;

    if (fname == NULL) return CAPS_NULLVALUE;
    if (mesh  == NULL) return CAPS_NULLVALUE;

    status = destroy_meshStruct(mesh);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Check filename for ugrid extension
    if (strstr(fname, ".ugrid") == NULL) {
        printf("Unrecognized file name, no \".ugrid\" extension found!\n");
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    // Determine is file is binary or ascii
    if (strstr(fname, ".lb8.ugrid") != NULL ||
        strstr(fname, ".b8.ugrid")  != NULL) {
        asciiFlag = 0;

    } else {
        asciiFlag = 1;

        printf("Function mesh_readAFLR3 doesn't currently support reading ASCII meshes!\n");
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    printf("\nReading AFLR3 file ....\n");

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }



    if (asciiFlag == 0) {

        fp = fopen(fname, "r");
        if (fp == NULL) {
            printf("\tUnable to open file: %s\n", fname);
            status = CAPS_IOERR;
            goto cleanup;
        }

        //numBytes = 7 * sint; // Un-comment if writing an unformatted file
        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

        fread(&mesh->numNode,                       sint ,1,fp);
        fread(&mesh->meshQuickRef.numTriangle,      sint ,1,fp);
        fread(&mesh->meshQuickRef.numQuadrilateral, sint ,1,fp);
        fread(&mesh->meshQuickRef.numTetrahedral,   sint ,1,fp);
        fread(&mesh->meshQuickRef.numPyramid,       sint ,1,fp);
        fread(&mesh->meshQuickRef.numPrism,         sint ,1,fp);
        fread(&mesh->meshQuickRef.numHexahedral,    sint ,1,fp);

        mesh->numElement = mesh->meshQuickRef.numTriangle +
                           mesh->meshQuickRef.numQuadrilateral +
                           mesh->meshQuickRef.numTetrahedral +
                           mesh->meshQuickRef.numPyramid +
                           mesh->meshQuickRef.numPrism +
                           mesh->meshQuickRef.numHexahedral;

        mesh->meshQuickRef.useStartIndex = (int) true;

        // What type of mesh is it - no current check for SurfaceMesh2D
        if (mesh->meshQuickRef.numTriangle > 0 ||
            mesh->meshQuickRef.numQuadrilateral ) {
            mesh->meshType = SurfaceMesh;
        }

        if (mesh->meshQuickRef.numTetrahedral > 0 ||
            mesh->meshQuickRef.numPyramid     > 0 ||
            mesh->meshQuickRef.numPrism       > 0 ||
            mesh->meshQuickRef.numHexahedral) {
            mesh->meshType = VolumeMesh;
        }

        // Allocate and initate nodes and elements
        mesh->node = (meshNodeStruct *) EG_alloc(mesh->numNode*sizeof(meshNodeStruct));
        if (mesh->node == NULL) {
            mesh->numNode = 0;
            status = EGADS_MALLOC;
            goto cleanup;
        }

        mesh->element = (meshElementStruct *) EG_alloc(mesh->numElement*sizeof(meshElementStruct));
        if (mesh->element == NULL) {
            mesh->numElement = 0;
            status = EGADS_MALLOC;
            goto cleanup;
        }

        for (i = 0; i < mesh->numNode; i++) {
            status = initiate_meshNodeStruct(&mesh->node[i], UnknownMeshAnalysis);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        for (i = 0; i < mesh->numElement; i++) {
            status = initiate_meshElementStruct(&mesh->element[i], UnknownMeshAnalysis);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

        /*
        numBytes = 3*mesh->numTriFace  * sint +
                   4*mesh->numQuadFace * sint +
                     mesh->numTriFace  * sint +
                     mesh->numQuadFace * sint +
                   4*mesh->numTetra * sint +
                   5*mesh->numPyr   * sint +
                   6*mesh->numPrz   * sint +
                   8*mesh->numHex   * sint +
                   3*mesh->numNodes * sdouble;
         */
        // Un-comment if writing an unformatted file

        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

        // Write nodal coordinates
        for (i = 0; i < mesh->numNode; i++) {

            fread(&tempDouble, sdouble, 1, fp);
            mesh->node[i].xyz[0] = tempDouble*scaleFactor;

            fread(&tempDouble, sdouble, 1, fp);
            mesh->node[i].xyz[1] = tempDouble*scaleFactor;

            fread(&tempDouble, sdouble, 1, fp);
            mesh->node[i].xyz[2] = tempDouble*scaleFactor;

        }

        elementIndex = 0;
        // Write tri-faces
        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {

            if (i == 0) mesh->meshQuickRef.startIndexTriangle = elementIndex;

            mesh->element[elementIndex].elementType = Triangle;

            status = mesh_allocMeshElementConnectivity(&mesh->element[elementIndex]);
            if (status != CAPS_SUCCESS) goto cleanup;

            fread(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);

            elementIndex += 1;
        }

        // Write quad-faces
        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {

            if (i == 0) mesh->meshQuickRef.startIndexQuadrilateral = elementIndex;

            mesh->element[elementIndex].elementType = Quadrilateral;

            status = mesh_allocMeshElementConnectivity(&mesh->element[elementIndex]);
            if (status != CAPS_SUCCESS) goto cleanup;

            fread(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);

            elementIndex += 1;
        }

        // Write tri-face boundaries
        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {

//            if (mesh->element[elementIndex].analysisType == MeshCFD) {
//                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
//                marker = cfdData->bcID;
//            } else {
//                marker = mesh->element[elementIndex].markerID;
//            }

            fread(&marker, sint, 1, fp);

            mesh->element[i + mesh->meshQuickRef.startIndexTriangle].markerID = marker;

        }

        // Write quad-face boundaries
        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {

//            if (mesh->element[elementIndex].analysisType == MeshCFD) {
//                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
//                marker = cfdData->bcID;
//            } else {
//                marker = mesh->element[elementIndex].markerID;
//            }

            fread(&marker, sint, 1, fp);

            mesh->element[i + mesh->meshQuickRef.startIndexQuadrilateral].markerID = marker;

        }

        // Write tetrahedrals connectivity
        for (i = 0; i < mesh->meshQuickRef.numTetrahedral; i++) {

            if (i == 0) mesh->meshQuickRef.startIndexTetrahedral = elementIndex;

            mesh->element[elementIndex].elementType = Tetrahedral;

            status = mesh_allocMeshElementConnectivity(&mesh->element[elementIndex]);
            if (status != CAPS_SUCCESS) goto cleanup;

            fread(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);

            elementIndex += 1;
        }

        // Write pyramid connectivity
        for (i = 0; i < mesh->meshQuickRef.numPyramid; i++) {

            if (i == 0) mesh->meshQuickRef.startIndexPyramid = elementIndex;

            mesh->element[elementIndex].elementType = Pyramid;

            status = mesh_allocMeshElementConnectivity(&mesh->element[elementIndex]);
            if (status != CAPS_SUCCESS) goto cleanup;

            fread(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[4], sint, 1, fp);

            elementIndex += 1;
        }

        // Write prisms connectivity
        for (i = 0; i < mesh->meshQuickRef.numPrism; i++) {

            if (i == 0) mesh->meshQuickRef.startIndexPrism = elementIndex;

            mesh->element[elementIndex].elementType = Prism;

            status = mesh_allocMeshElementConnectivity(&mesh->element[elementIndex]);
            if (status != CAPS_SUCCESS) goto cleanup;

            fread(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[4], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[5], sint, 1, fp);

            elementIndex += 1;
        }

        // Write hex connectivity
        for (i = 0; i < mesh->meshQuickRef.numHexahedral; i++) {

            if (i == 0) mesh->meshQuickRef.startIndexHexahedral = elementIndex;

            mesh->element[elementIndex].elementType = Hexahedral;

            status = mesh_allocMeshElementConnectivity(&mesh->element[elementIndex]);
            if (status != CAPS_SUCCESS) goto cleanup;

            fread(&mesh->element[elementIndex].connectivity[0], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[1], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[2], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[3], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[4], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[5], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[6], sint, 1, fp);
            fread(&mesh->element[elementIndex].connectivity[7], sint, 1, fp);

            elementIndex += 1;
        }

        //fwrite(&numBytes, sint,1,fp); // Un-comment if writing an unformatted file

    } else {
//
//        postFix = ".ugrid";
//        filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(postFix)) *sizeof(char));
//        if (filename == NULL) {
//            status = EGADS_MALLOC;
//            goto cleanup;
//        }
//
//        sprintf(filename, "%s%s", fname, postFix);
//
//        fp = fopen(filename, "w");
//        if (fp == NULL) {
//            printf("\tUnable to open file: %s\n", filename);
//            status = CAPS_IOERR;
//            goto cleanup;
//        }
//
//        //nodes, tri-face, quad-face, numTetra, numPyr, numPrz, numHex
//        fprintf(fp,"%d, %d, %d, %d, %d, %d, %d\n", mesh->numNode,
//                                                   mesh->meshQuickRef.numTriangle,
//                                                   mesh->meshQuickRef.numQuadrilateral,
//                                                   mesh->meshQuickRef.numTetrahedral,
//                                                   mesh->meshQuickRef.numPyramid,
//                                                   mesh->meshQuickRef.numPrism,
//                                                   mesh->meshQuickRef.numHexahedral);
//        // Write nodal coordinates
//        for (i = 0; i < mesh->numNode; i++) {
//            fprintf(fp,"%f %f %f\n", mesh->node[i].xyz[0]*scaleFactor,
//                                     mesh->node[i].xyz[1]*scaleFactor,
//                                     mesh->node[i].xyz[2]*scaleFactor);
//        }
//
//        // Write tri-faces
//        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {
//            if (mesh->meshQuickRef.startIndexTriangle >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexTriangle +i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexTriangle[i];
//            }
//            fprintf(fp,"%d %d %d\n", mesh->element[elementIndex].connectivity[0],
//                                     mesh->element[elementIndex].connectivity[1],
//                                     mesh->element[elementIndex].connectivity[2]);
//        }
//
//        // Write quad-faces
//        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {
//            if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[i];
//            }
//            fprintf(fp,"%d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
//                                        mesh->element[elementIndex].connectivity[1],
//                                        mesh->element[elementIndex].connectivity[2],
//                                        mesh->element[elementIndex].connectivity[3]);
//        }
//
//        // Write tri-face boundaries
//        for (i = 0; i < mesh->meshQuickRef.numTriangle; i++) {
//            if (mesh->meshQuickRef.startIndexTriangle >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexTriangle +i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexTriangle[i];
//            }
//
//            if (mesh->element[elementIndex].analysisType == MeshCFD) {
//                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
//                marker = cfdData->bcID;
//            } else {
//                marker = mesh->element[elementIndex].markerID;
//            }
//
//            fprintf(fp,"%d\n",marker);
//        }
//
//        // Write quad-face boundaries
//        for (i = 0; i < mesh->meshQuickRef.numQuadrilateral; i++) {
//            if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[i];
//            }
//
//            if (mesh->element[elementIndex].analysisType == MeshCFD) {
//                cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
//                marker = cfdData->bcID;
//            } else {
//                marker = mesh->element[elementIndex].markerID;
//            }
//
//            fprintf(fp,"%d\n",marker);
//        }
//
//        // Write tetrahedrals connectivity
//        for (i = 0; i < mesh->meshQuickRef.numTetrahedral; i++) {
//            if (mesh->meshQuickRef.startIndexTetrahedral >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexTetrahedral + i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexTetrahedral[i];
//            }
//            fprintf(fp,"%d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
//                                        mesh->element[elementIndex].connectivity[1],
//                                        mesh->element[elementIndex].connectivity[2],
//                                        mesh->element[elementIndex].connectivity[3]);
//        }
//
//        // Write pyramid connectivity
//        for (i = 0; i < mesh->meshQuickRef.numPyramid; i++) {
//            if (mesh->meshQuickRef.startIndexPyramid >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexPyramid + i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexPyramid[i];
//            }
//            fprintf(fp,"%d %d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
//                                           mesh->element[elementIndex].connectivity[1],
//                                           mesh->element[elementIndex].connectivity[2],
//                                           mesh->element[elementIndex].connectivity[3],
//                                           mesh->element[elementIndex].connectivity[4]);
//        }
//
//        // Write prisms connectivity
//        for (i = 0; i < mesh->meshQuickRef.numPrism; i++) {
//            if (mesh->meshQuickRef.startIndexPrism >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexPrism + i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexPrism[i];
//            }
//            fprintf(fp,"%d %d %d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
//                                              mesh->element[elementIndex].connectivity[1],
//                                              mesh->element[elementIndex].connectivity[2],
//                                              mesh->element[elementIndex].connectivity[3],
//                                              mesh->element[elementIndex].connectivity[4],
//                                              mesh->element[elementIndex].connectivity[5]);
//        }
//
//        // Write hex connectivity
//        for (i = 0; i < mesh->meshQuickRef.numHexahedral; i++) {
//            if (mesh->meshQuickRef.startIndexHexahedral >= 0) {
//                elementIndex = mesh->meshQuickRef.startIndexHexahedral + i;
//            } else {
//                elementIndex = mesh->meshQuickRef.listIndexHexahedral[i];
//            }
//            fprintf(fp,"%d %d %d %d %d %d %d %d\n", mesh->element[elementIndex].connectivity[0],
//                                                    mesh->element[elementIndex].connectivity[1],
//                                                    mesh->element[elementIndex].connectivity[2],
//                                                    mesh->element[elementIndex].connectivity[3],
//                                                    mesh->element[elementIndex].connectivity[4],
//                                                    mesh->element[elementIndex].connectivity[5],
//                                                    mesh->element[elementIndex].connectivity[6],
//                                                    mesh->element[elementIndex].connectivity[7]);
//        }
    }

    printf("Finished writing AFLR3 file\n\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in mesh_readAFLR3, status = %d\n", status);
        if (fp != NULL) fclose(fp);

        return status;

}


// Write a mesh contained in the mesh structure in VTK format (*.vtk)
int mesh_writeVTK(char *fname,
                  int asciiFlag, // 0 for binary, anything else for ascii
                  meshStruct *mesh,
                  double scaleFactor) // Scale factor for coordinates
{

    int status; // Function return status

    FILE *fp = NULL;
    int i, j;
    int numCell, length, tempInteger;
    int sint = sizeof(int);
    int sdouble = sizeof(double);
    double tempDouble;

    char *filename = NULL;

    // NEED to change if indices already start at 0
    int m1 = -1; // VTK indices start at 0 !!!!

    if (mesh == NULL) return CAPS_NULLVALUE;

    if (mesh->meshQuickRef.useStartIndex == (int) false &&
        mesh->meshQuickRef.useListIndex  == (int) false) {

        status = mesh_fillQuickRefList( mesh);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + 4) *sizeof(char));
    sprintf(filename,"%s.vtk",fname);

    printf("\nWriting VTK file: %s....\n", filename);

    if (asciiFlag == 0) {
        fp = fopen(filename, "wb");
    } else {
        fp = fopen(filename, "w");
    }

    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);
        status = CAPS_IOERR;
        goto cleanup;
    }

    fprintf(fp,"# vtk DataFile Version 2.0\n");
    fprintf(fp,"Unstructured Grid\n");

    if (asciiFlag == 0) {
        fprintf(fp,"BINARY\n");
    } else {
        fprintf(fp,"ASCII\n");
    }

    fprintf(fp,"DATASET UNSTRUCTURED_GRID\n");

    fprintf(fp,"POINTS %d double\n", mesh->numNode);

    // Write nodal coordinates
    for (i = 0; i < mesh->numNode; i++) {

        if (asciiFlag == 0) {

            tempDouble = mesh->node[i].xyz[0]*scaleFactor;
            fwrite(&tempDouble, sdouble, 1, fp);

            tempDouble = mesh->node[i].xyz[1]*scaleFactor;
            fwrite(&tempDouble, sdouble, 1, fp);

            tempDouble = mesh->node[i].xyz[2]*scaleFactor;
            fwrite(&tempDouble, sdouble, 1, fp);

        } else {
            fprintf(fp,"%f %f %f\n", mesh->node[i].xyz[0]*scaleFactor,
                                     mesh->node[i].xyz[1]*scaleFactor,
                                     mesh->node[i].xyz[2]*scaleFactor);
        }
    }

    // 2D and surface meshes
    if (mesh->meshType == Surface2DMesh ||
        mesh->meshType == SurfaceMesh) {

        numCell = mesh->meshQuickRef.numLine          +
                  mesh->meshQuickRef.numTriangle      +
                  mesh->meshQuickRef.numTriangle_6    +
                  mesh->meshQuickRef.numQuadrilateral +
                  mesh->meshQuickRef.numQuadrilateral_8;

        length = numCell +
                 mesh->meshQuickRef.numLine           *mesh_numMeshConnectivity(Line) +
                 mesh->meshQuickRef.numTriangle       *mesh_numMeshConnectivity(Triangle) +
                 mesh->meshQuickRef.numTriangle_6     *mesh_numMeshConnectivity(Triangle_6) +
                 mesh->meshQuickRef.numQuadrilateral  *mesh_numMeshConnectivity(Quadrilateral) +
                 mesh->meshQuickRef.numQuadrilateral_8*mesh_numMeshConnectivity(Quadrilateral_8);

    } else { // Default to volume mesh

        numCell = mesh->meshQuickRef.numTetrahedral    +
                  mesh->meshQuickRef.numTetrahedral_10 +
                  mesh->meshQuickRef.numPyramid        +
                  mesh->meshQuickRef.numPrism          +
                  mesh->meshQuickRef.numHexahedral;

        length = numCell +
                 mesh->meshQuickRef.numTetrahedral*mesh_numMeshConnectivity(Tetrahedral)       +
                 mesh->meshQuickRef.numTetrahedral_10*mesh_numMeshConnectivity(Tetrahedral_10) +
                 mesh->meshQuickRef.numPyramid    *mesh_numMeshConnectivity(Pyramid)           +
                 mesh->meshQuickRef.numPrism      *mesh_numMeshConnectivity(Prism)             +
                 mesh->meshQuickRef.numHexahedral *mesh_numMeshConnectivity(Hexahedral);
    }

    fprintf(fp,"CELLS %d %d\n", numCell, length);

    // Loop through elements - Write connectivity
    for (i = 0; i < mesh->numElement; i++) {

        if (mesh->meshType == Surface2DMesh ||
            mesh->meshType == SurfaceMesh) {

            if (mesh->element[i].elementType != Line          &&
                mesh->element[i].elementType != Triangle      &&
                mesh->element[i].elementType != Triangle_6    &&
                mesh->element[i].elementType != Quadrilateral &&
                mesh->element[i].elementType != Quadrilateral_8) {

                continue;
            }
        } else {
            if (mesh->element[i].elementType != Tetrahedral    &&
            	mesh->element[i].elementType != Tetrahedral_10 &&
                mesh->element[i].elementType != Pyramid        &&
                mesh->element[i].elementType != Prism          &&
                mesh->element[i].elementType != Hexahedral) {

                continue;
            }
        }

        length = mesh_numMeshElementConnectivity(&mesh->element[i]);
        if (asciiFlag == 0) {
            fwrite(&length,sint,1,fp);
            for (j = 0; j < length; j++) {
                tempInteger = mesh->element[i].connectivity[j] + m1;
                fwrite(&tempInteger, sint, 1, fp);
            }
        } else {
            fprintf(fp,"%d ", length);
            for (j = 0; j < length; j++) {
                tempInteger = mesh->element[i].connectivity[j] + m1;
                fprintf(fp, "%d ", tempInteger);
            }
            fprintf(fp, "\n");
        }
    }


    fprintf(fp,"CELL_TYPES %d\n", numCell);

    // Loop through elements - Write what type of element type it is
    for (i = 0; i < mesh->numElement; i++) {

        if (mesh->meshType == Surface2DMesh ||
            mesh->meshType == SurfaceMesh) {

            if (mesh->element[i].elementType != Line          &&
                mesh->element[i].elementType != Triangle      &&
                mesh->element[i].elementType != Triangle_6    &&
                mesh->element[i].elementType != Quadrilateral &&
                mesh->element[i].elementType != Quadrilateral_8) {

                continue;
            }
        } else {
            if (mesh->element[i].elementType != Tetrahedral    &&
                mesh->element[i].elementType != Tetrahedral_10 &&
                mesh->element[i].elementType != Pyramid        &&
                mesh->element[i].elementType != Prism          &&
                mesh->element[i].elementType != Hexahedral) {

                continue;
            }
        }

        if ( mesh->element[i].elementType == Line)            length = 3;
        if ( mesh->element[i].elementType == Triangle)        length = 5;
        if ( mesh->element[i].elementType == Quadrilateral)   length = 9;
        if ( mesh->element[i].elementType == Tetrahedral)     length = 10;
        if ( mesh->element[i].elementType == Pyramid)         length = 14;
        if ( mesh->element[i].elementType == Prism)           length = 13;
        if ( mesh->element[i].elementType == Hexahedral)      length = 12;
        if ( mesh->element[i].elementType == Triangle_6)      length = 22;
        if ( mesh->element[i].elementType == Quadrilateral_8) length = 23;
        if ( mesh->element[i].elementType == Tetrahedral_10)  length = 24;

        if (asciiFlag == 0) {
            fwrite(&length, sint, 1, fp);
        } else {
            fprintf(fp,"%d\n",length);
        }
    }

    fprintf(fp, "CELL_DATA %d\n", numCell);
    fprintf(fp, "SCALARS cell_scalars int 1\n");
    fprintf(fp, "LOOKUP_TABLE default\n");
    for (i = 0; i < mesh->numElement; ++i)
    {
      if (mesh->meshType == Surface2DMesh ||
          mesh->meshType == SurfaceMesh) {

        if (mesh->element[i].elementType != Line          &&
            mesh->element[i].elementType != Triangle      &&
            mesh->element[i].elementType != Triangle_6    &&
            mesh->element[i].elementType != Quadrilateral &&
            mesh->element[i].elementType != Quadrilateral_8) {

              continue;
          }
      } else {
          if (mesh->element[i].elementType != Tetrahedral    &&
              mesh->element[i].elementType != Tetrahedral_10 &&
              mesh->element[i].elementType != Pyramid        &&
              mesh->element[i].elementType != Prism          &&
              mesh->element[i].elementType != Hexahedral) {

              continue;
          }
      }

      if (asciiFlag == 0) {
        fwrite(&mesh->element[i].markerID, sint, 1, fp);
      } else {
        fprintf(fp, "%d\n", mesh->element[i].markerID);
      }
    }

    printf("Finished writing VTK file\n\n");

    status = CAPS_SUCCESS;

    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in mesh_writeVTK, status = %d\n", status);

        if (fp != NULL) fclose(fp);
        if (filename != NULL) EG_free(filename);
        return status;
}

// Write a mesh contained in the mesh structure in SU2 format (*.su2)
int mesh_writeSU2(char *fname,
                  int asciiFlag, // 0 for binary, anything else for ascii
                  meshStruct *mesh,
                  int numBnds,
                  int bndID[],
                  double scaleFactor) // Scale factor for coordinates
{

    // Important: SU2 wants elements/index to start at 0 we assume everything coming in starts at 1

    int status; // Function return status

    FILE *fp = NULL;
    int  i, j, m1 = -1, *numMarkerList = NULL;
    int  length, elementType, elementID = 0, elementIndex, markerID;
    char *filename = NULL;
    char fileExt[] = ".su2";

    cfdMeshDataStruct *cfdData;

    if (mesh == NULL) return CAPS_NULLVALUE;

    numMarkerList = (int *) EG_alloc(numBnds*sizeof(int));  // Array to keep track of the number of
    // surface faces with a particular boundary id.
    if (numMarkerList == NULL) {
        status  = EGADS_MALLOC;
        goto cleanup;
    }

    if (mesh->meshQuickRef.useStartIndex == (int) false &&
        mesh->meshQuickRef.useListIndex  == (int) false) {

        status = mesh_fillQuickRefList( mesh);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    printf("\nWriting SU2 file ....\n");

    if (asciiFlag == 0) {
        printf("\tBinary output is not supported by SU2\n");
        printf("\t..... switching to ASCII!\n");
        asciiFlag = 1;
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) {
        status  = EGADS_MALLOC;
        goto cleanup;
    }

    sprintf(filename,"%s%s",fname, fileExt);

    fp = fopen(filename, "w");

    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);

        status  = CAPS_IOERR;
        goto cleanup;
    }


    // Dimensionality
    if (mesh->meshType == Surface2DMesh) {

        printf("\tThe supplied mesh appears to be a 2D mesh!\n");

        fprintf(fp,"NDIME= %d\n",2);

    } else {
        fprintf(fp,"NDIME= %d\n",3);
    }

    if (mesh->meshType == Surface2DMesh){

        // Number of elements
        fprintf(fp,"NELEM= %d\n", mesh->meshQuickRef.numTriangle    +
                                  mesh->meshQuickRef.numQuadrilateral);
    } else {
        // Number of elements
        fprintf(fp,"NELEM= %d\n", mesh->meshQuickRef.numTetrahedral +
                                  mesh->meshQuickRef.numPyramid     +
                                  mesh->meshQuickRef.numPrism       +
                                  mesh->meshQuickRef.numHexahedral);
    }

    // SU2 wants elements/index to start at 0 - assume everything starts at 1
    elementID = 0;
    for (i = 0; i < mesh->numElement; i++) {

        if (mesh->meshType == Surface2DMesh) {

            if (mesh->element[i].elementType != Triangle &&
                mesh->element[i].elementType != Quadrilateral) {

                continue;
            }

        } else {
            if (mesh->element[i].elementType != Tetrahedral &&
                mesh->element[i].elementType != Pyramid     &&
                mesh->element[i].elementType != Prism       &&
                mesh->element[i].elementType != Hexahedral) {

                continue;
            }
        }

        elementType = -1;

        if      ( mesh->element[i].elementType == Triangle)      elementType = 5;
        else if ( mesh->element[i].elementType == Quadrilateral) elementType = 9;
        else if ( mesh->element[i].elementType == Tetrahedral)   elementType = 10;
        else if ( mesh->element[i].elementType == Pyramid)       elementType = 14;
        else if ( mesh->element[i].elementType == Prism)         elementType = 13;
        else if ( mesh->element[i].elementType == Hexahedral)    elementType = 12;
        else {
            printf("Unrecognized elementType %d for SU2!\n", mesh->element[i].elementType);
            status = CAPS_BADVALUE;
            goto cleanup;
        }
        fprintf(fp, "%d ", elementType);

        length = mesh_numMeshElementConnectivity(&mesh->element[i]);
        for (j = 0; j < length; j++ ) {
            fprintf(fp, "%d ", mesh->element[i].connectivity[j] + m1);
        }

        fprintf(fp, "%d\n", elementID);

        elementID += 1;
    }

    // Number of points
    fprintf(fp,"NPOIN= %d\n", mesh->numNode);

    // Write nodal coordinates
    for (i = 0; i < mesh->numNode; i++) {
        fprintf(fp,"%f %f %f %d\n", mesh->node[i].xyz[0]*scaleFactor,
                                    mesh->node[i].xyz[1]*scaleFactor,
                                    mesh->node[i].xyz[2]*scaleFactor,
                                    mesh->node[i].nodeID + m1);  // Connectivity starts at 0
    }

    // Number of boundary ID
    fprintf(fp,"NMARK= %d\n", numBnds);

    // We need the number of surface elements that have a particular boundary ID
    // First initialize numMarkerList components to zero
    for (i = 0; i < numBnds; i++) numMarkerList[i] = 0;

    // Next count elements with a particular id
    for (i = 0; i < numBnds; i++) {

        if (mesh->meshType == Surface2DMesh) {

            for (j = 0; j < mesh->meshQuickRef.numLine; j++) {
                if (mesh->meshQuickRef.startIndexLine >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexLine + j;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexLine[j];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    markerID = cfdData->bcID;
                } else {
                    markerID = mesh->element[elementIndex].markerID;
                }

                if (markerID == bndID[i]) numMarkerList[i] += 1;
            }

        } else {

            for (j = 0; j < mesh->meshQuickRef.numTriangle; j++) {
                if (mesh->meshQuickRef.startIndexTriangle >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexTriangle + j;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexTriangle[j];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    markerID = cfdData->bcID;
                } else {
                    markerID = mesh->element[elementIndex].markerID;
                }

                if (markerID == bndID[i]) numMarkerList[i] += 1;
            }

            for (j = 0; j < mesh->meshQuickRef.numQuadrilateral; j++) {
                if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + j;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[j];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    markerID = cfdData->bcID;
                } else {
                    markerID = mesh->element[elementIndex].markerID;
                }

                if (markerID == bndID[i]) numMarkerList[i] += 1;
            }
        }
    }

    // Finally lets write the connectivity for the boundaries
    for (i = 0; i < numBnds; i++) {

        if (numMarkerList[i] == 0) continue;

        fprintf(fp,"MARKER_TAG= %d\n", bndID[i]); // Probably eventually want to change this to a string tag
        // see note at the beginning of function

        fprintf(fp,"MARKER_ELEMS= %d\n", numMarkerList[i]); //Number of elements with a particular ID

        if (mesh->meshType == Surface2DMesh) {

            for (j = 0; j < mesh->meshQuickRef.numLine; j++) {
                if (mesh->meshQuickRef.startIndexLine >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexLine + j;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexLine[j];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    markerID = cfdData->bcID;
                } else {
                    markerID = mesh->element[elementIndex].markerID;
                }

                //if ( mesh->element[i].elementType == Line)
                elementType = 3;
                if (markerID == bndID[i]) {
                    fprintf(fp, "%d %d %d\n", elementType,
                                              mesh->element[elementIndex].connectivity[0] + m1,
                                              mesh->element[elementIndex].connectivity[1] + m1);
                }
            }

        } else {

            for (j = 0; j < mesh->meshQuickRef.numTriangle; j++) {
                if (mesh->meshQuickRef.startIndexTriangle >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexTriangle + j;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexTriangle[j];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    markerID = cfdData->bcID;
                } else {
                    markerID = mesh->element[elementIndex].markerID;
                }

                //if ( mesh->element[i].elementType == Triangle)
                elementType = 5;
                if (markerID == bndID[i]) {
                    fprintf(fp, "%d %d %d %d\n", elementType,
                                                 mesh->element[elementIndex].connectivity[0] + m1,
                                                 mesh->element[elementIndex].connectivity[1] + m1,
                                                 mesh->element[elementIndex].connectivity[2] + m1);
                }
            }

            for (j = 0; j < mesh->meshQuickRef.numQuadrilateral; j++) {
                if (mesh->meshQuickRef.startIndexQuadrilateral >= 0) {
                    elementIndex = mesh->meshQuickRef.startIndexQuadrilateral + j;
                } else {
                    elementIndex = mesh->meshQuickRef.listIndexQuadrilateral[j];
                }

                if (mesh->element[elementIndex].analysisType == MeshCFD) {
                    cfdData = (cfdMeshDataStruct *) mesh->element[elementIndex].analysisData;
                    markerID = cfdData->bcID;
                } else {
                    markerID = mesh->element[elementIndex].markerID;
                }

                //if ( mesh->element[i].elementType == Quadrilateral)
                elementType = 9;
                if (markerID == bndID[i]) {
                    fprintf(fp, "%d %d %d %d %d\n", elementType,
                                                    mesh->element[elementIndex].connectivity[0] + m1,
                                                    mesh->element[elementIndex].connectivity[1] + m1,
                                                    mesh->element[elementIndex].connectivity[2] + m1,
                                                    mesh->element[elementIndex].connectivity[3] + m1);
                }
            }
        } // End else
    } // End for loop on bndIDs

    printf("Finished writing SU2 file\n\n");

    status = CAPS_SUCCESS;

    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in mesh_writeSU2, status = %d\n", status);

        if (filename != NULL) EG_free(filename);
        if (numMarkerList != NULL) EG_free(numMarkerList);

        if (fp != NULL) fclose(fp);
        return status;
}

// Write a mesh contained in the mesh structure in NASTRAN format
int mesh_writeNASTRAN(char *fname,
                      int asciiFlag, // 0 for binary, anything else for ascii
                      meshStruct *nasMesh,
                      feaFileTypeEnum gridFileType,
                      double scaleFactor) // Scale factor for coordinates
{
    int status; // Function return status
    FILE *fp = NULL;
    int i, j, gridFields;
    int coordID, propertyID;
    char *filename = NULL, *delimiter = NULL, *tempString = NULL;
    //char x[20], y[20], z[20];
    char fileExt[] = ".bdf";

    feaMeshDataStruct *feaData;

    meshElementSubTypeEnum elementSubType;

    if (nasMesh == NULL) return CAPS_NULLVALUE;

    if (gridFileType == LargeField) {
        printf("\nWriting Nastran grid and connectivity file (in large field format) ....\n");

    } else if ( gridFileType == FreeField){
        printf("\nWriting Nastran grid and connectivity file (in free field format) ....\n");

    } else {
        printf("\nWriting Nastran grid and connectivity file (in small field format) ....\n");
    }

    if (asciiFlag == 0) {
        printf("\tBinary output is not currently supported for working with Nastran\n");
        printf("\t..... switching to ASCII!\n");
        asciiFlag = 1;
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    sprintf(filename,"%s%s", fname, fileExt);

    fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);

        status = CAPS_IOERR;
        goto cleanup;
    }

    if (gridFileType == LargeField) {
        fprintf(fp,"$---1A--|-------2-------|-------3-------|-------4-------|-------5-------|-10A--|\n");
        fprintf(fp,"$---1B--|-------6-------|-------7-------|-------8-------|-------9-------|-10B--|\n");
    } else {
        fprintf(fp,"$---1---|---2---|---3---|---4---|---5---|---6---|---7---|---8---|---9---|---10--|\n");
    }

    if (gridFileType == FreeField) {
        delimiter = ",";
        gridFields = 8;
    } else {
        delimiter = " ";
        gridFields = 7;
    }

    // Write nodal coordinates
    if (gridFileType == LargeField) {

        for (i = 0; i < nasMesh->numNode; i++) {

            fprintf(fp,"%-8s %15d", "GRID*", nasMesh->node[i].nodeID);

            // If the coord ID == 0 leave blank
            if (nasMesh->node[i].analysisType == MeshStructure) {
                feaData = (feaMeshDataStruct *) nasMesh->node[i].analysisData;
                coordID = feaData->coordID;

                if (coordID != 0) fprintf(fp," %15d",coordID);
                else              fprintf(fp,"%16s", "");
            } else {
                fprintf(fp,"%16s", "");
            }

            // This function always has 15 characters
//            doubleToString_LargeField(nasMesh->node[i].xyz[0]*scaleFactor, x);
//            doubleToString_LargeField(nasMesh->node[i].xyz[1]*scaleFactor, y);
//            doubleToString_LargeField(nasMesh->node[i].xyz[2]*scaleFactor, z);
//
//            fprintf(fp," %s %s%-8s\n", x, y, "*");
//            fprintf(fp,"%-8s %s\n", "*", z);

            // x
            tempString = convert_doubleToString(nasMesh->node[i].xyz[0]*scaleFactor, 15, 1);
            fprintf(fp," %s", tempString);
            EG_free(tempString); tempString = NULL;

            // y
            tempString = convert_doubleToString(nasMesh->node[i].xyz[1]*scaleFactor, 15, 1);
            fprintf(fp," %s%-8s\n", tempString, "*");
            EG_free(tempString); tempString = NULL;

            // z
            tempString = convert_doubleToString(nasMesh->node[i].xyz[2]*scaleFactor, 15, 1);
            fprintf(fp,"%-8s %s\n", "*", tempString);
            EG_free(tempString); tempString = NULL;
        }

    } else {

        for (i = 0; i < nasMesh->numNode; i++) {

            fprintf(fp,"%-8s", "GRID");

            tempString = convert_integerToString(nasMesh->node[i].nodeID  , 7, 1);
            fprintf(fp, "%s%s", delimiter, tempString);
            EG_free(tempString); tempString = NULL;

            // If the coord ID == 0 leave blank
            if (nasMesh->node[i].analysisType == MeshStructure) {
                feaData = (feaMeshDataStruct *) nasMesh->node[i].analysisData;
                coordID = feaData->coordID;

                if (coordID != 0) fprintf(fp,"%s%7d", delimiter, coordID);
                else              fprintf(fp,"%s%7s", delimiter, "");
            } else {
                fprintf(fp,"%s%7s", delimiter, "");
            }

            for (j = 0; j < 3; j++) {
                tempString = convert_doubleToString(nasMesh->node[i].xyz[j]*scaleFactor, gridFields, 1);
                fprintf(fp,"%s%s", delimiter, tempString);
                EG_free(tempString); tempString = NULL;
            }

            fprintf(fp,"\n");
        }
    }

    fprintf(fp,"$---1---|---2---|---3---|---4---|---5---|---6---|---7---|---8---|---9---|---10--|\n");

    for (i = 0; i < nasMesh->numElement; i++) {

        // If we have a volume mesh skip the surface elements
        if (nasMesh->meshType == VolumeMesh) {
            if (nasMesh->element[i].elementType != Tetrahedral &&
                nasMesh->element[i].elementType != Pyramid     &&
                nasMesh->element[i].elementType != Prism       &&
                nasMesh->element[i].elementType != Hexahedral) continue;
        }

        // Grab Structure specific related data if available
        if (nasMesh->element[i].analysisType == MeshStructure) {
            feaData = (feaMeshDataStruct *) nasMesh->element[i].analysisData;
            propertyID = feaData->propertyID;
            coordID = feaData->coordID;
            elementSubType = feaData->elementSubType;
        } else {
            propertyID = nasMesh->element[i].markerID;
            coordID = 0;
            elementSubType = UnknownMeshSubElement;
        }

        if ( nasMesh->element[i].elementType == Line &&
                elementSubType == UnknownMeshSubElement) { // Non-default subtype handled by nastran_writeSubElementCard
            // in nastranUtils.c because of additional information need that
            // that isn't stored in the mesh structure.

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d\n", "CROD",
                                                     delimiter, nasMesh->element[i].elementID,
                                                     delimiter, propertyID,
                                                     delimiter, nasMesh->element[i].connectivity[0],
                                                     delimiter, nasMesh->element[i].connectivity[1]);
        }

        if ( nasMesh->element[i].elementType == Triangle) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d", "CTRIA3",
                                                        delimiter, nasMesh->element[i].elementID,
                                                        delimiter, propertyID,
                                                        delimiter, nasMesh->element[i].connectivity[0],
                                                        delimiter, nasMesh->element[i].connectivity[1],
                                                        delimiter, nasMesh->element[i].connectivity[2]);

            // Write coordinate id
            if (coordID != 0){
                fprintf(fp, "%s%7d", delimiter, coordID);
            }

            fprintf(fp,"\n");
        }

        if ( nasMesh->element[i].elementType == Triangle_6) {

            // Write coordinate id
            if (coordID != 0){
                fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%-8s\n", "CTRIA6",
                                                                             delimiter, nasMesh->element[i].elementID,
                                                                             delimiter, propertyID,
                                                                             delimiter, nasMesh->element[i].connectivity[0],
                                                                             delimiter, nasMesh->element[i].connectivity[1],
                                                                             delimiter, nasMesh->element[i].connectivity[2],
                                                                             delimiter, nasMesh->element[i].connectivity[3],
                                                                             delimiter, nasMesh->element[i].connectivity[4],
                                                                             delimiter, nasMesh->element[i].connectivity[5],
                                                                             delimiter, "+CT");

                fprintf(fp, "%-8s%s%7d\n", "+CT", delimiter, coordID);

            } else {
                fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d\n", "CTRIA6",
                                                                              delimiter, nasMesh->element[i].elementID,
                                                                              delimiter, propertyID,
                                                                              delimiter, nasMesh->element[i].connectivity[0],
                                                                              delimiter, nasMesh->element[i].connectivity[1],
                                                                              delimiter, nasMesh->element[i].connectivity[2],
                                                                              delimiter, nasMesh->element[i].connectivity[3],
                                                                              delimiter, nasMesh->element[i].connectivity[4],
                                                                              delimiter, nasMesh->element[i].connectivity[5]);
            }
        }


        if ( nasMesh->element[i].elementType == Quadrilateral &&
                elementSubType == UnknownMeshSubElement) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d", "CQUAD4",
                                                             delimiter, nasMesh->element[i].elementID,
                                                             delimiter, propertyID,
                                                             delimiter, nasMesh->element[i].connectivity[0],
                                                             delimiter, nasMesh->element[i].connectivity[1],
                                                             delimiter, nasMesh->element[i].connectivity[2],
                                                             delimiter, nasMesh->element[i].connectivity[3]);

            // Write coordinate id
            if (coordID != 0){
                fprintf(fp, "%s%7d", delimiter, coordID);
            }

            fprintf(fp,"\n");

        }

        if ( nasMesh->element[i].elementType == Quadrilateral &&
                elementSubType == ShearElement) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d", "CSHEAR",
                                                             delimiter, nasMesh->element[i].elementID,
                                                             delimiter, propertyID,
                                                             delimiter, nasMesh->element[i].connectivity[0],
                                                             delimiter, nasMesh->element[i].connectivity[1],
                                                             delimiter, nasMesh->element[i].connectivity[2],
                                                             delimiter, nasMesh->element[i].connectivity[3]);
            fprintf(fp,"\n");
        }

        if ( nasMesh->element[i].elementType == Quadrilateral_8) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%-8s\n", "CQUAD8",
                                                                         delimiter, nasMesh->element[i].elementID,
                                                                         delimiter, propertyID,
                                                                         delimiter, nasMesh->element[i].connectivity[0],
                                                                         delimiter, nasMesh->element[i].connectivity[1],
                                                                         delimiter, nasMesh->element[i].connectivity[2],
                                                                         delimiter, nasMesh->element[i].connectivity[3],
                                                                         delimiter, nasMesh->element[i].connectivity[4],
                                                                         delimiter, nasMesh->element[i].connectivity[5],
                                                                         delimiter, "+CQ");

            fprintf(fp,"%-8s%s%7d%s%7d", "+CQ",
                                         delimiter, nasMesh->element[i].connectivity[6],
                                         delimiter, nasMesh->element[i].connectivity[7]);

            fprintf(fp,"\n");
        }


        if ( nasMesh->element[i].elementType == Tetrahedral)   {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d\n", "CTETRA",
                                                               delimiter, nasMesh->element[i].elementID,
                                                               delimiter, propertyID,
                                                               delimiter, nasMesh->element[i].connectivity[0],
                                                               delimiter, nasMesh->element[i].connectivity[1],
                                                               delimiter, nasMesh->element[i].connectivity[2],
                                                               delimiter, nasMesh->element[i].connectivity[3]);
        }

        if ( nasMesh->element[i].elementType == Tetrahedral_10)   {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%-8s\n", "CTETRA",
                                                                              delimiter, nasMesh->element[i].elementID,
                                                                              delimiter, propertyID,
                                                                              delimiter, nasMesh->element[i].connectivity[0],
                                                                              delimiter, nasMesh->element[i].connectivity[1],
                                                                              delimiter, nasMesh->element[i].connectivity[2],
                                                                              delimiter, nasMesh->element[i].connectivity[3],
                                                                              delimiter, nasMesh->element[i].connectivity[4],
                                                                              delimiter, nasMesh->element[i].connectivity[5],
                                                                              delimiter, "+CT");

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d", "+CT",
                                                     delimiter, nasMesh->element[i].connectivity[6],
                                                     delimiter, nasMesh->element[i].connectivity[7],
                                                     delimiter, nasMesh->element[i].connectivity[8],
                                                     delimiter, nasMesh->element[i].connectivity[9]);

            fprintf(fp,"\n");
        }


        if ( nasMesh->element[i].elementType == Pyramid) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d\n", "CPYRAM",
                                                                    delimiter, nasMesh->element[i].elementID,
                                                                    delimiter, propertyID,
                                                                    delimiter, nasMesh->element[i].connectivity[0],
                                                                    delimiter, nasMesh->element[i].connectivity[1],
                                                                    delimiter, nasMesh->element[i].connectivity[2],
                                                                    delimiter, nasMesh->element[i].connectivity[3],
                                                                    delimiter, nasMesh->element[i].connectivity[4]);
        }

        if ( nasMesh->element[i].elementType == Prism)   {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d\n", "CPENTA",
                                                                         delimiter, nasMesh->element[i].elementID,
                                                                         delimiter, propertyID,
                                                                         delimiter, nasMesh->element[i].connectivity[0],
                                                                         delimiter, nasMesh->element[i].connectivity[1],
                                                                         delimiter, nasMesh->element[i].connectivity[2],
                                                                         delimiter, nasMesh->element[i].connectivity[3],
                                                                         delimiter, nasMesh->element[i].connectivity[4],
                                                                         delimiter, nasMesh->element[i].connectivity[5]);

        }

        if ( nasMesh->element[i].elementType == Hexahedral) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%-8s\n", "CHEXA",
                                                                               delimiter, nasMesh->element[i].elementID,
                                                                               delimiter, propertyID,
                                                                               delimiter, nasMesh->element[i].connectivity[0],
                                                                               delimiter, nasMesh->element[i].connectivity[1],
                                                                               delimiter, nasMesh->element[i].connectivity[2],
                                                                               delimiter, nasMesh->element[i].connectivity[3],
                                                                               delimiter, nasMesh->element[i].connectivity[4],
                                                                               delimiter, nasMesh->element[i].connectivity[5],
                                                                               delimiter, "+CH");
            fprintf(fp,"%-8s%s%7d%s%7d\n", "+CH",
                                           delimiter, nasMesh->element[i].connectivity[6],
                                           delimiter, nasMesh->element[i].connectivity[7]);
        }
    }

    fprintf(fp,"$---1---|---2---|---3---|---4---|---5---|---6---|---7---|---8---|---9---|---10--|\n");

    printf("Finished writing Nastran grid file\n\n");

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in mesh_writeNastran, status = %d\n", status);

        EG_free(tempString);
        EG_free(filename);

        if (fp != NULL) fclose(fp);
        return status;
}

// Write a mesh contained in the mesh structure in Astros format (*.bdf)
int mesh_writeAstros(char *fname,
                     int asciiFlag, // 0 for binary, anything else for ascii
                     meshStruct *mesh,
                     feaFileTypeEnum gridFileType,
                     int numDesignVariable, feaDesignVariableStruct feaDesignVariable[],
                     double scaleFactor) // Scale factor for coordinates
{
    int status; // Function return status
    FILE *fp = NULL;
    int i, j, gridFields;
    int coordID, propertyID;
    char *filename = NULL, *delimiter = NULL, *tempString = NULL;
    //char x[20], y[20], z[20];
    char fileExt[] = ".bdf";

    feaMeshDataStruct *feaData;

    // Design variables
    int foundDesignVar, designIndex;
    double maxDesignVar = 0.0;

    if (mesh == NULL) return CAPS_NULLVALUE;

    printf("\nWriting Astros grid and connectivity file (in large field format) ....\n");

    if (asciiFlag == 0) {
        printf("\tBinary output is not currently supported for working with Astros\n");
        printf("\t..... switching to ASCII!\n");
        asciiFlag = 1;
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    sprintf(filename,"%s%s", fname, fileExt);

    fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);

        status = CAPS_IOERR;
        goto cleanup;
    }

    if (gridFileType == LargeField) {
        fprintf(fp,"$---1A--|-------2-------|-------3-------|-------4-------|-------5-------|-10A--|\n");
        fprintf(fp,"$---1B--|-------6-------|-------7-------|-------8-------|-------9-------|-10B--|\n");
    } else {
        fprintf(fp,"$---1---|---2---|---3---|---4---|---5---|---6---|---7---|---8---|---9---|---10--|\n");
    }

    if (gridFileType == FreeField) {
        delimiter = ",";
        gridFields = 8;
    } else {
        delimiter = " ";
        gridFields = 7;
    }

    // Write nodal coordinates
    if (gridFileType == LargeField) {

        for (i = 0; i < mesh->numNode; i++) {

            fprintf(fp,"%-8s %15d", "GRID*", mesh->node[i].nodeID);

            // If the coord ID == 0 leave blank
            if (mesh->node[i].analysisType == MeshStructure) {
                feaData = (feaMeshDataStruct *) mesh->node[i].analysisData;
                coordID = feaData->coordID;

                if (coordID != 0) fprintf(fp," %15d",coordID);
                else              fprintf(fp,"%16s", "");
            } else {
                fprintf(fp,"%16s", "");
            }

            // This function always has 15 characters
//            doubleToString_LargeField(mesh->node[i].xyz[0]*scaleFactor, x);
//            doubleToString_LargeField(mesh->node[i].xyz[1]*scaleFactor, y);
//            doubleToString_LargeField(mesh->node[i].xyz[2]*scaleFactor, z);

//            fprintf(fp," %s %s%-8s\n", x, y, "*C");
//            fprintf(fp,"%-8s %s\n", "*C", z);

            // x
            tempString = convert_doubleToString(mesh->node[i].xyz[0]*scaleFactor, 15, 1);
            fprintf(fp," %s", tempString);
            EG_free(tempString); tempString = NULL;

            // y
            tempString = convert_doubleToString(mesh->node[i].xyz[1]*scaleFactor, 15, 1);
            fprintf(fp," %s%-8s\n", tempString, "*C");
            EG_free(tempString); tempString = NULL;

            // z
            tempString = convert_doubleToString(mesh->node[i].xyz[2]*scaleFactor, 15, 1);
            fprintf(fp,"%-8s %s\n", "*C", tempString);
            EG_free(tempString); tempString = NULL;

            /*
             * This implementation does not guarantee 15 characters because there is no
             * control over the number of digits in the exponent
            fprintf(fp," %+14.8e %+14.8e%-8s\n", mesh->node[i].xyz[0]*scaleFactor,
                                                 mesh->node[i].xyz[1]*scaleFactor,
                                                 "*C");

            fprintf(fp,"%-8s %+14.8e\n", "*C", mesh->node[i].xyz[2]*scaleFactor);
             */
        }

    } else {

        for (i = 0; i < mesh->numNode; i++) {

            fprintf(fp,"%-8s", "GRID");

            tempString = convert_integerToString(mesh->node[i].nodeID  , 7, 1);
            fprintf(fp, "%s%s", delimiter, tempString);
            EG_free(tempString);

            // If the coord ID == 0 leave blank
            if (mesh->node[i].analysisType == MeshStructure) {
                feaData = (feaMeshDataStruct *) mesh->node[i].analysisData;
                coordID = feaData->coordID;

                if (coordID != 0) fprintf(fp,"%s%7d", delimiter, coordID);
                else              fprintf(fp,"%s%7s", delimiter, "");
            } else {
                fprintf(fp,"%s%7s", delimiter, "");
            }

            for (j = 0; j < 3; j++) {
                tempString = convert_doubleToString(mesh->node[i].xyz[j], gridFields, 1);
                fprintf(fp,"%s%s", delimiter, tempString);
                EG_free(tempString); tempString = NULL;
            }

            fprintf(fp,"\n");
        }
    }

    fprintf(fp,"$---1---|---2---|---3---|---4---|---5---|---6---|---7---|---8---|---9---|---10--|\n");

    for (i = 0; i < mesh->numElement; i++) {

        // If we have a volume mesh skip the surface elements
        if (mesh->meshType == VolumeMesh) {
            if (mesh->element[i].elementType != Tetrahedral &&
                mesh->element[i].elementType != Pyramid     &&
                mesh->element[i].elementType != Prism       &&
                mesh->element[i].elementType != Hexahedral) continue;
        }

        // Grab Structure specific related data if available
        if (mesh->element[i].analysisType == MeshStructure) {
            feaData = (feaMeshDataStruct *) mesh->element[i].analysisData;
            propertyID = feaData->propertyID;
            coordID = feaData->coordID;
        } else {
            propertyID = mesh->element[i].markerID;
            coordID = 0;
        }

        // Check for design minimum area
        foundDesignVar = (int) false;
        for (designIndex = 0; designIndex < numDesignVariable; designIndex++) {
            for (j = 0; j < feaDesignVariable[designIndex].numPropertyID; j++) {

                if (feaDesignVariable[designIndex].propertySetID[j] == propertyID) {
                    foundDesignVar = (int) true;

                    maxDesignVar = feaDesignVariable[designIndex].upperBound;

                    // If 0.0 don't do anything
                    if (maxDesignVar == 0.0) foundDesignVar = (int) false;

                    break;
                }
            }

            if (foundDesignVar == (int) true) break;
        }

        if ( mesh->element[i].elementType == Line) { // Need to add subType for bar and beam .....

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d", "CROD",
                                                   delimiter, mesh->element[i].elementID,
                                                   delimiter, propertyID,
                                                   delimiter, mesh->element[i].connectivity[0],
                                                   delimiter, mesh->element[i].connectivity[1]);

            if (foundDesignVar == (int) true) {

                tempString = convert_doubleToString( maxDesignVar, gridFields, 1);

                fprintf(fp, "%s%s", delimiter, tempString);
                EG_free(tempString); tempString = NULL;
            }

            fprintf(fp, "\n");
        }

        if ( mesh->element[i].elementType == Triangle) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d", "CTRIA3",
                                                        delimiter, mesh->element[i].elementID,
                                                        delimiter, propertyID,
                                                        delimiter, mesh->element[i].connectivity[0],
                                                        delimiter, mesh->element[i].connectivity[1],
                                                        delimiter, mesh->element[i].connectivity[2]);

            // Write coordinate id
            if (coordID != 0){
                fprintf(fp, "%s%7d", delimiter, coordID);
            } else if (foundDesignVar == (int) true) {

                // BLANK FIELD
                if (gridFileType == FreeField) {
                    fprintf(fp, ", ");
                } else {
                    fprintf(fp, " %7s", "");
                }
            }

            /*
            if (foundDesignVar == (int) true) {

                // BLANK FIELD
                if (gridFileType == FreeField) {
                    fprintf(fp, ", ");
                } else {
                    fprintf(fp, " %7s", "");
                }

                // BLANK FIELD
                if (gridFileType == FreeField) {
                    fprintf(fp, ", ");
                } else {
                    fprintf(fp, " %7s", "");
                }

                // CONINUATION LINE
                if (gridFileType == FreeField) {
                    fprintf(fp, ",+C ");
                } else {
                    fprintf(fp, "+C%6s", "");
                }
                fprintf(fp, "\n");

                // CONINUATION LINE
                if (gridFileType == FreeField) {
                    fprintf(fp, "+C ");
                } else {
                    fprintf(fp, "+C%6s", "");
                }

                // BLANK FIELD
                if (gridFileType == FreeField) {
                    fprintf(fp, ", ");
                } else {
                    fprintf(fp, " %7s", "");
                }

                tempString = convert_doubleToString( maxDesignVar, gridFields, 1);

                fprintf(fp, "%s%s", delimiter, tempString);
                EG_free(tempString);
            }
             */

            fprintf(fp, "\n");

        }

        if ( mesh->element[i].elementType == Quadrilateral) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d", "CQUAD4",
                                                             delimiter, mesh->element[i].elementID,
                                                             delimiter, propertyID,
                                                             delimiter, mesh->element[i].connectivity[0],
                                                             delimiter, mesh->element[i].connectivity[1],
                                                             delimiter, mesh->element[i].connectivity[2],
                                                             delimiter, mesh->element[i].connectivity[3]);

            // Write coordinate id
            if (coordID != 0){
                fprintf(fp, "%s%7d", delimiter, coordID);
            } else if (foundDesignVar == (int) true) {

                // BLANK FIELD
                if (gridFileType == FreeField) {
                    fprintf(fp, ", ");
                } else {
                    fprintf(fp, " %7s", "");
                }
            }

            /*
            if (foundDesignVar == (int) true) {

                // BLANK FIELD
                if (gridFileType == FreeField) {
                    fprintf(fp, ", ");
                } else {
                    fprintf(fp, " %7s", "");
                }

                // CONINUATION LINE
                if (gridFileType == FreeField) {
                    fprintf(fp, ",+ ");
                } else {
                    fprintf(fp, "+%7s", "");
                }
                fprintf(fp, "\n");

                // CONINUATION LINE
                if (gridFileType == FreeField) {
                    fprintf(fp, "+C ");
                } else {
                    fprintf(fp, "+C%6s", "");
                }

                // BLANK FIELD
                if (gridFileType == FreeField) {
                    fprintf(fp, ", ");
                } else {
                    fprintf(fp, " %7s", "");
                }

                tempString = convert_doubleToString( maxDesignVar, gridFields, 1);

                fprintf(fp, "%s%s", delimiter, tempString);
                EG_free(tempString);
            }
             */

            fprintf(fp, "\n");
        }

        if ( mesh->element[i].elementType == Tetrahedral)   {

            printf("\tWarning: Astros doesn't support tetrahedral elements - skipping element %d\n", mesh->element[i].elementID);
        }

        if ( mesh->element[i].elementType == Pyramid) {

            printf("\tWarning: Astros doesn't support pyramid elements - skipping element %d\n", mesh->element[i].elementID);
        }

        if ( mesh->element[i].elementType == Prism)   {

            printf("\tWarning: Astros doesn't support prism elements - skipping element %d\n", mesh->element[i].elementID);
        }

        if ( mesh->element[i].elementType == Hexahedral) {

            fprintf(fp,"%-8s%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%7d%s%-8s\n", "CIHEX1",
                                                                               delimiter, mesh->element[i].elementID,
                                                                               delimiter, propertyID,
                                                                               delimiter, mesh->element[i].connectivity[0],
                                                                               delimiter, mesh->element[i].connectivity[1],
                                                                               delimiter, mesh->element[i].connectivity[2],
                                                                               delimiter, mesh->element[i].connectivity[3],
                                                                               delimiter, mesh->element[i].connectivity[4],
                                                                               delimiter, mesh->element[i].connectivity[5],
                                                                               delimiter, "+CH");
            fprintf(fp,"%-8s%s%7d%s%7d\n", "+CH",
                                           delimiter, mesh->element[i].connectivity[6],
                                           delimiter, mesh->element[i].connectivity[7]);
        }
    }

    fprintf(fp,"$---1---|---2---|---3---|---4---|---5---|---6---|---7---|---8---|---9---|---10--|\n");

    printf("Finished writing Astros grid file\n\n");

    status = CAPS_SUCCESS;

    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in mesh_writeAstros, status = %d\n", status);

        if (filename != NULL) EG_free(filename);

        if (fp != NULL) fclose(fp);
        return status;

}

// Write a mesh contained in the mesh structure in STL format (*.stl)
int mesh_writeSTL(char *fname,
                  int asciiFlag, // 0 for binary, anything else for ascii
                  meshStruct *mesh,
                  double scaleFactor) // Scale factor for coordinates
{
    int status;

    int i; // Indexing

    FILE *fp = NULL;

    double norm[3] = {0,0,0}, p1[3] = {0,0,0}, p2[3] = {0,0,0}, p3[3] = {0,0,0};
    float temp;
    char *filename = NULL;
    char fileExt[] = ".stl";
    char header[80] = "CAPS_STL";

    // Binaray
    int schar = sizeof(char); // Size of an character
    int sint = sizeof(unsigned int); // Size of an unsigned integer
    int sshort = sizeof(short); // Size of a short
    int sfloat = sizeof(float); // Size of a float
    unsigned int numTriangle = 0;

    short numAtrr = 0; // Number of attributes

    int numTriangleMesh = 0, numQuadrilateralMesh = 0;

    if (mesh == NULL) {
        status = CAPS_NULLVALUE;
        goto cleanup;
    }

    printf("\nWriting STL file ....\n");

    status = mesh_retrieveNumMeshElements(mesh->numElement,
                                          mesh->element,
                                          Triangle,
                                          &numTriangleMesh);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = mesh_retrieveNumMeshElements(mesh->numElement,
                                          mesh->element,
                                          Quadrilateral,
                                          &numQuadrilateralMesh);
    if (status != CAPS_SUCCESS) goto cleanup;


    //if (asciiFlag == 0) {
    //   printf("\tBinary output is not currently supported when writing STL files\n");
    //   printf("\t..... switching to ASCII!\n");
    //   asciiFlag = 1;
    //}

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    sprintf(filename,"%s%s",fname, fileExt);

    fp = fopen(filename,"w");

    if (fp == NULL) {

        printf("\tUnable to open file: %s\n", filename);
        status = CAPS_IOERR;
        goto cleanup;
    }

    if (asciiFlag == 0) { // Binary

        fwrite(&header, schar*80 , 1, fp);

        numTriangle = numTriangleMesh + 2*numQuadrilateralMesh;
        fwrite(&numTriangle, sint, 1, fp);

        for (i = 0; i < mesh->numElement; i++) {

            if (mesh->element[i].elementType == Triangle) {

                p1[0] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[0]*scaleFactor;
                p1[1] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[1]*scaleFactor;
                p1[2] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[2]*scaleFactor;

                p2[0] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[0]*scaleFactor;
                p2[1] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[1]*scaleFactor;
                p2[2] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[2]*scaleFactor;

                p3[0] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[0]*scaleFactor;
                p3[1] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[1]*scaleFactor;
                p3[2] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[2]*scaleFactor;

                get_Surface_Norm(p1,
                                 p2,
                                 p3,
                                 norm);

                temp = (float) norm[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) norm[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) norm[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p1[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p1[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p1[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p2[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p2[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p2[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p3[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p3[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p3[2];
                fwrite(&temp, sfloat, 1, fp);

                fwrite(&numAtrr, sshort, 1, fp);

            }

            if (mesh->element[i].elementType == Quadrilateral) {
                // First triangle
                p1[0] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[0]*scaleFactor;
                p1[1] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[1]*scaleFactor;
                p1[2] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[2]*scaleFactor;

                p2[0] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[0]*scaleFactor;
                p2[1] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[1]*scaleFactor;
                p2[2] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[2]*scaleFactor;

                p3[0] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[0]*scaleFactor;
                p3[1] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[1]*scaleFactor;
                p3[2] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[2]*scaleFactor;

                get_Surface_Norm(p1,
                                 p2,
                                 p3,
                                 norm);

                temp = (float) norm[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) norm[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) norm[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p1[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p1[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p1[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p2[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p2[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p2[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p3[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p3[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p3[2];
                fwrite(&temp, sfloat, 1, fp);

                fwrite(&numAtrr, sshort, 1, fp);

                // Second triangle
                p1[0] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[0]*scaleFactor;
                p1[1] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[1]*scaleFactor;
                p1[2] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[2]*scaleFactor;

                p2[0] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[0]*scaleFactor;
                p2[1] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[1]*scaleFactor;
                p2[2] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[2]*scaleFactor;

                p3[0] = mesh->node[mesh->element[i].connectivity[3]-1].xyz[0]*scaleFactor;
                p3[1] = mesh->node[mesh->element[i].connectivity[3]-1].xyz[1]*scaleFactor;
                p3[2] = mesh->node[mesh->element[i].connectivity[3]-1].xyz[2]*scaleFactor;

                get_Surface_Norm(p1,
                        p2,
                        p3,
                        norm);

                temp = (float) norm[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) norm[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) norm[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p1[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p1[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p1[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p2[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p2[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p2[2];
                fwrite(&temp, sfloat, 1, fp);

                temp = (float) p3[0];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p3[1];
                fwrite(&temp, sfloat, 1, fp);
                temp = (float) p3[2];
                fwrite(&temp, sfloat, 1, fp);

                fwrite(&numAtrr, sshort, 1, fp);

            }
        }

    } else { // ASCII

        fprintf(fp,"solid %s\n", header);


        for (i = 0; i < mesh->numElement; i++) {

            if (mesh->element[i].elementType == Triangle) {

                p1[0] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[0]*scaleFactor;
                p1[1] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[1]*scaleFactor;
                p1[2] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[2]*scaleFactor;

                p2[0] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[0]*scaleFactor;
                p2[1] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[1]*scaleFactor;
                p2[2] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[2]*scaleFactor;

                p3[0] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[0]*scaleFactor;
                p3[1] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[1]*scaleFactor;
                p3[2] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[2]*scaleFactor;

                get_Surface_Norm(p1,
                                 p2,
                                 p3,
                                 norm);

                fprintf(fp,"\tfacet normal %f %f %f\n",norm[0],norm[1],norm[2]);
                fprintf(fp,"\t\touter loop\n");

                fprintf(fp,"\t\t\tvertex %f %f %f\n",p1[0],p1[1],p1[2]);
                fprintf(fp,"\t\t\tvertex %f %f %f\n",p2[0],p2[1],p2[2]);
                fprintf(fp,"\t\t\tvertex %f %f %f\n",p3[0],p3[1],p3[2]);

                fprintf(fp,"\t\tendloop\n");
                fprintf(fp,"\tendfacet\n");
            }

            if (mesh->element[i].elementType == Quadrilateral) {
                // First triangle
                p1[0] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[0]*scaleFactor;
                p1[1] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[1]*scaleFactor;
                p1[2] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[2]*scaleFactor;

                p2[0] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[0]*scaleFactor;
                p2[1] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[1]*scaleFactor;
                p2[2] = mesh->node[mesh->element[i].connectivity[1]-1].xyz[2]*scaleFactor;

                p3[0] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[0]*scaleFactor;
                p3[1] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[1]*scaleFactor;
                p3[2] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[2]*scaleFactor;

                get_Surface_Norm(p1,
                                 p2,
                                 p3,
                                 norm);

                fprintf(fp,"\tfacet normal %f %f %f\n",norm[0],norm[1],norm[2]);
                fprintf(fp,"\t\touter loop\n");

                fprintf(fp,"\t\t\tvertex %f %f %f\n",p1[0],p1[1],p1[2]);
                fprintf(fp,"\t\t\tvertex %f %f %f\n",p2[0],p2[1],p2[2]);
                fprintf(fp,"\t\t\tvertex %f %f %f\n",p3[0],p3[1],p3[2]);

                fprintf(fp,"\t\tendloop\n");
                fprintf(fp,"\tendfacet\n");

                // Second triangle
                p1[0] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[0]*scaleFactor;
                p1[1] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[1]*scaleFactor;
                p1[2] = mesh->node[mesh->element[i].connectivity[0]-1].xyz[2]*scaleFactor;

                p2[0] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[0]*scaleFactor;
                p2[1] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[1]*scaleFactor;
                p2[2] = mesh->node[mesh->element[i].connectivity[2]-1].xyz[2]*scaleFactor;

                p3[0] = mesh->node[mesh->element[i].connectivity[3]-1].xyz[0]*scaleFactor;
                p3[1] = mesh->node[mesh->element[i].connectivity[3]-1].xyz[1]*scaleFactor;
                p3[2] = mesh->node[mesh->element[i].connectivity[3]-1].xyz[2]*scaleFactor;

                get_Surface_Norm(p1,
                                 p2,
                                 p3,
                                 norm);

                fprintf(fp,"\tfacet normal %f %f %f\n",norm[0],norm[1],norm[2]);
                fprintf(fp,"\t\touter loop\n");

                fprintf(fp,"\t\t\tvertex %f %f %f\n",p1[0],p1[1],p1[2]);
                fprintf(fp,"\t\t\tvertex %f %f %f\n",p2[0],p2[1],p2[2]);
                fprintf(fp,"\t\t\tvertex %f %f %f\n",p3[0],p3[1],p3[2]);

                fprintf(fp,"\t\tendloop\n");
                fprintf(fp,"\tendfacet\n");

            }
        }

        fprintf(fp,"endsolid");
    }

    printf("Done Writing STL\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_writeSTL, status %d\n", status);

        if (fp != NULL) fclose(fp);

        if (filename != NULL) EG_free(filename);

        return status;
}

// Write a mesh contained in the mesh structure in Tecplot format (*.dat)
int mesh_writeTecplot(const char *fname,
                      int asciiFlag,
                      meshStruct *mesh,
                      double scaleFactor) // Scale factor for coordinates
{

    int status; // Function status return
    int i; // Indexing

    FILE *fp = NULL;
    char filename[512];

    printf("\nWriting TECPLOT file: %s.dat ....\n", fname);

    if (asciiFlag == 0) {
        printf("\tBinary output is not currently supported for Tecplot output\n");
        printf("\t..... switching to ASCII!\n");
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    sprintf(filename,"%s.dat",fname);


    if (mesh->meshQuickRef.useStartIndex == (int) false &&
        mesh->meshQuickRef.useListIndex  == (int) false) {
        status = mesh_fillQuickRefList( mesh);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // Write mesh
    fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);
        status = CAPS_IOERR;
        goto cleanup;
    }

    fprintf(fp,"TITLE = \"%s\"\n",fname);
    fprintf(fp,"VARIABLES = \"x\", \"y\", \"z\"\n");

    if (mesh->meshType == VolumeMesh) {
        fprintf(fp,"ZONE N = %d, E = %d, DATAPACKING = POINT, ZONETYPE = FEBRICK\n",
                                                    mesh->numNode, mesh->meshQuickRef.numTetrahedral +
                                                    mesh->meshQuickRef.numPyramid +
                                                    mesh->meshQuickRef.numPrism +
                                                    mesh->meshQuickRef.numHexahedral);
    } else {

        if ( (mesh->meshQuickRef.numTriangle + mesh->meshQuickRef.numQuadrilateral) != 0) {
            fprintf(fp,"ZONE N = %d, E = %d, DATAPACKING = POINT, ZONETYPE = FEQUADRILATERAL\n",
                                                    mesh->numNode, mesh->meshQuickRef.numTriangle +
                                                    mesh->meshQuickRef.numQuadrilateral);

        } else if (mesh->meshQuickRef.numLine != 0){
            fprintf(fp,"ZONE N = %d, E = %d, DATAPACKING = POINT, ZONETYPE = FELINESEG\n",
                                                    mesh->numNode, mesh->meshQuickRef.numLine);
        } else if (mesh->meshQuickRef.numNode != 0){
            fprintf(fp,"ZONE DATAPACKING = POINT\n");
        } else {
            printf("No elements to write out!\n");
            status = CAPS_BADVALUE;
            goto cleanup;
        }

    }

    // Write nodal coordinates
    // X, Y, and Z
    for (i = 0; i < mesh->numNode; i++) {
        fprintf(fp,"%f %f %f\n", mesh->node[i].xyz[0],
                                 mesh->node[i].xyz[1],
                                 mesh->node[i].xyz[2]);
    }

    // Write connectivity
    for (i = 0; i < mesh->numElement; i++) {

        if (mesh->meshType == VolumeMesh) {
            if (mesh->element[i].elementType != Tetrahedral &&
                mesh->element[i].elementType != Pyramid &&
                mesh->element[i].elementType != Prism &&
                mesh->element[i].elementType != Hexahedral) {
                continue;
            }
        }

        if ( (mesh->meshQuickRef.numTriangle + mesh->meshQuickRef.numQuadrilateral) == 0) {

            if (mesh->element[i].elementType == Line) {
                fprintf(fp,"%d %d\n", mesh->element[i].connectivity[0],
                                      mesh->element[i].connectivity[1]);
            }
        }

        if (mesh->element[i].elementType == Triangle) {
            fprintf(fp,"%d %d %d %d\n", mesh->element[i].connectivity[0],
                                        mesh->element[i].connectivity[1],
                                        mesh->element[i].connectivity[2],
                                        mesh->element[i].connectivity[2]);
        }

        if (mesh->element[i].elementType == Quadrilateral) {
            fprintf(fp,"%d %d %d %d\n", mesh->element[i].connectivity[0],
                                        mesh->element[i].connectivity[1],
                                        mesh->element[i].connectivity[2],
                                        mesh->element[i].connectivity[3]);
        }

        if (mesh->element[i].elementType == Tetrahedral) {
            fprintf(fp,"%d %d %d %d %d %d %d %d\n", mesh->element[i].connectivity[0],
                                                    mesh->element[i].connectivity[1],
                                                    mesh->element[i].connectivity[2],
                                                    mesh->element[i].connectivity[2],
                                                    mesh->element[i].connectivity[3],
                                                    mesh->element[i].connectivity[3],
                                                    mesh->element[i].connectivity[3],
                                                    mesh->element[i].connectivity[3]);
        }

        if (mesh->element[i].elementType == Pyramid) {

            fprintf(fp,"%d %d %d %d %d %d %d %d\n", mesh->element[i].connectivity[0],
                                                    mesh->element[i].connectivity[1],
                                                    mesh->element[i].connectivity[2],
                                                    mesh->element[i].connectivity[3],
                                                    mesh->element[i].connectivity[4],
                                                    mesh->element[i].connectivity[4],
                                                    mesh->element[i].connectivity[4],
                                                    mesh->element[i].connectivity[4]);
        }

        if (mesh->element[i].elementType == Prism) {

            fprintf(fp,"%d %d %d %d %d %d %d %d\n", mesh->element[i].connectivity[0],
                                                    mesh->element[i].connectivity[1],
                                                    mesh->element[i].connectivity[2],
                                                    mesh->element[i].connectivity[2],
                                                    mesh->element[i].connectivity[3],
                                                    mesh->element[i].connectivity[4],
                                                    mesh->element[i].connectivity[5],
                                                    mesh->element[i].connectivity[5]);
        }

        if (mesh->element[i].elementType == Hexahedral) {

            fprintf(fp,"%d %d %d %d %d %d %d %d\n", mesh->element[i].connectivity[0],
                                                    mesh->element[i].connectivity[1],
                                                    mesh->element[i].connectivity[2],
                                                    mesh->element[i].connectivity[3],
                                                    mesh->element[i].connectivity[4],
                                                    mesh->element[i].connectivity[5],
                                                    mesh->element[i].connectivity[6],
                                                    mesh->element[i].connectivity[7]);
        }
    }

    printf("Finished writing TECPLOT file\n\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_writeTecplot, status %d\n", status);

        if (fp != NULL) fclose(fp);

        return status;
}

// Write a mesh contained in the mesh structure in Airfoil format (boundary edges only [Lines]) (*.af)
//  "Character Name"
//	x[0] y[0] x y coordinates
//	x[1] y[1]
//	 ...  ...
int mesh_writeAirfoil(char *fname,
                      int asciiFlag,
                      meshStruct *mesh,
                      double scaleFactor) // Scale factor for coordinates
{

    int status; // Function status return

    int i, elementIndex, nodeIndex;

    int firstLineElement = (int) false;

    char *filename = NULL;
    char fileExt[] = ".af";

    FILE *fp = NULL;

    // 2D mesh checks
    int xMeshConstant = (int) true, yMeshConstant = (int) true, zMeshConstant= (int) true;
    int swapZX = (int) false, swapZY = (int) false;
    double x=0, y=0, z=0;

    if (mesh == NULL) return CAPS_NULLVALUE;

    printf("\nWriting Airfoil file ....\n");

    if (asciiFlag == 0) {
        printf("\tBinary output is not currently supported when writing Airfoil files\n");
        printf("\t..... switching to ASCII!\n");
        asciiFlag = 1;
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) return EGADS_MALLOC;

    sprintf(filename,"%s%s",fname, fileExt);

    fp = fopen(filename,"w");

    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);
        status = CAPS_IOERR;
        goto cleanup;
    }

    // Loop through elements and check for planar on line elements
    for (elementIndex = 0; elementIndex < mesh->numElement; elementIndex++) {

        if (mesh->element[elementIndex].elementType != Line) continue;

        // Set first point
        if (firstLineElement == (int) false) {

            firstLineElement = (int) true;

            nodeIndex = mesh->element[elementIndex].connectivity[0];

            x = mesh->node[nodeIndex].xyz[0];
            y = mesh->node[nodeIndex].xyz[1];
            z = mesh->node[nodeIndex].xyz[2];
        }

        // Loop though line connectivity
        for (i = 0; i < mesh_numMeshConnectivity(Line); i++) {
            nodeIndex = mesh->element[elementIndex].connectivity[i];

            // Constant x?
            if ((mesh->node[nodeIndex].xyz[0] - x) > 1E-7) {

                xMeshConstant = (int) false;
            }

            // Constant y?
            if ((mesh->node[nodeIndex].xyz[1] - y) > 1E-7) {

                yMeshConstant = (int) false;
            }

            // Constant z?
            if ((mesh->node[nodeIndex].xyz[2] - z) > 1E-7) {

                zMeshConstant = (int) false;
            }
        }
    }

    // Check arrays
    if (firstLineElement == (int) false) {// No line elements were found
        printf("\tNo edge boundaries saved - cannot write Airfoil file!\n");
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    if (zMeshConstant != (int) true) {
        printf("\tMesh is not in x-y plane... attempting to rotate mesh through node swapping!\n");

        if (xMeshConstant == (int) true && yMeshConstant == (int) false) {

            printf("\tSwapping z and x coordinates!\n");
            swapZX = (int) true;

        } else if(xMeshConstant == (int) false && yMeshConstant == (int) true) {

            printf("\tSwapping z and y coordinates!\n");
            swapZY = (int) true;

        } else {

            printf("\tUnable to rotate mesh!\n");
            status = CAPS_BADVALUE;
            goto cleanup;
        }
    }

    // Write name to file
    fprintf(fp,"%s\n", fname);

    // Write nodal coordinates to file
    for (elementIndex = 0; elementIndex < mesh->numElement; elementIndex++) {

        if (mesh->element[elementIndex].elementType != Line) continue;

        nodeIndex = mesh->element[elementIndex].connectivity[0]-1;

        if (swapZX == (int) true) {
            x = mesh->node[nodeIndex].xyz[2]*scaleFactor;
            y = mesh->node[nodeIndex].xyz[1]*scaleFactor;

        } else if (swapZY == (int) true) {
            x = mesh->node[nodeIndex].xyz[0]*scaleFactor;
            y = mesh->node[nodeIndex].xyz[2]*scaleFactor;

        } else {
            x = mesh->node[nodeIndex].xyz[0]*scaleFactor;
            y = mesh->node[nodeIndex].xyz[1]*scaleFactor;
        }

        fprintf(fp, "%f %f\n", x, y);

        // Get the next node but don't write it
        nodeIndex = mesh->element[elementIndex].connectivity[1]-1;

        if (swapZX == (int) true) {
            x = mesh->node[nodeIndex].xyz[2]*scaleFactor;
            y = mesh->node[nodeIndex].xyz[1]*scaleFactor;

        } else if (swapZY == (int) true) {
            x = mesh->node[nodeIndex].xyz[0]*scaleFactor;
            y = mesh->node[nodeIndex].xyz[2]*scaleFactor;

        } else {
            x = mesh->node[nodeIndex].xyz[0]*scaleFactor;
            y = mesh->node[nodeIndex].xyz[1]*scaleFactor;
        }

    }

    // Write the last node
    fprintf(fp, "%f %f\n", x, y);
    printf("Finished writing Airfoil file\n\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:

        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_writeAirfoil, status %d\n", status);

        if (filename != NULL) EG_free(filename);

        if (fp != NULL) fclose(fp);

        return status;

}

// Write a mesh contained in the mesh structure in FAST mesh format (*.msh)
int mesh_writeFAST(char *fname,
                   int asciiFlag,
                   meshStruct *mesh,
                   double scaleFactor) // Scale factor for coordinates
{
    int status; // Function return

    int i, j; // Indexing

    int marker;

    cfdMeshDataStruct *cfdData;

    char *filename = NULL;
    char fileExt[] = ".msh";

    FILE *fp = NULL;

    if (mesh == NULL) return CAPS_NULLVALUE;

    printf("\nWriting FAST mesh file ....\n");

    if (asciiFlag == 0) {
        printf("\tBinary output is not currently supported when writing FAST mesh files\n");
        printf("\t..... switching to ASCII!\n");
        asciiFlag = 1;
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) return EGADS_MALLOC;

    sprintf(filename,"%s%s",fname, fileExt);

    fp = fopen(filename,"w");

    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);
        status = CAPS_IOERR;
        goto cleanup;
    }

    if (mesh->meshQuickRef.useStartIndex == (int) false &&
        mesh->meshQuickRef.useListIndex  == (int) false) {

        status = mesh_fillQuickRefList(mesh);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // Number of nodes  Number of triangles Number of Tetra = 0
    fprintf(fp, "%d %d %d\n", mesh->numNode, mesh->meshQuickRef.numTriangle, mesh->meshQuickRef.numTetrahedral);

    // Write x coordinates for nodes
    for (i = 0; i < mesh->numNode; i++) {

        fprintf(fp, "%f\n", mesh->node[i].xyz[0]*scaleFactor);
    }

    // Write y coordinates for nodes
    for (i = 0; i < mesh->numNode; i++) {

        fprintf(fp, "%f\n", mesh->node[i].xyz[1]*scaleFactor);
    }

    // Write z coordinates for nodes
    for (i = 0; i < mesh->numNode; i++) {

        fprintf(fp, "%f\n", mesh->node[i].xyz[2]*scaleFactor);
    }

    // Write connectivity for Triangles
    for (i = 0; i < mesh->numElement; i++) {

        if (mesh->element[i].elementType != Triangle) continue;

        for (j = 0; j < mesh_numMeshConnectivity(mesh->element[i].elementType); j++){
            fprintf(fp, "%d ", mesh->element[i].connectivity[j]);
        }

        fprintf(fp, "\n");
    }

    // Write unused flags
    for (i = 0; i < mesh->numElement; i++) {

        if (mesh->element[i].elementType != Triangle) continue;

        if (mesh->element[i].analysisType == MeshCFD) {
            cfdData = (cfdMeshDataStruct *) mesh->element[i].analysisData;
            marker = cfdData->bcID;
        } else {
            marker = mesh->element[i].markerID;
        }

        fprintf(fp, "%d\n", marker);
    }

    // Write connectivity for Tetrahedral
    for (i = 0; i < mesh->numElement; i++) {

        if (mesh->element[i].elementType != Tetrahedral) continue;

        for (j = 0; j < mesh_numMeshConnectivity(mesh->element[i].elementType); j++){
            fprintf(fp, "%d ", mesh->element[i].connectivity[j]);
        }

        fprintf(fp, "\n");
    }

    printf("Finished writing FAST file\n\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:

        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_writeFAST, status %d\n", status);

        if (filename != NULL) EG_free(filename);
        if (fp != NULL) fclose(fp);

        return status;
}

// Write a mesh contained in the mesh structure in Abaqus mesh format (*_Mesh.inp)
int mesh_writeAbaqus(char *fname,
                     int asciiFlag,
                     meshStruct *mesh,
                     mapAttrToIndexStruct *attrMap, // Mapping between element sets and property IDs
                     double scaleFactor) // Scale factor for coordinates
{

    int status = CAPS_SUCCESS; // Function return status

    int i, j, attrIndex; // Indexing

    char *filename = NULL;
    char fileExt[] = "_Mesh.inp";

    FILE *fp = NULL;

    int propertyID, pID; //coordID
    const char *type = NULL, *elemSet = NULL; // Don't free

    meshElementTypeEnum elementType = UnknownMeshElement;
    meshElementSubTypeEnum elementSubType = UnknownMeshSubElement;

    feaMeshDataStruct *feaData;

    if (mesh == NULL) return CAPS_NULLVALUE;

    printf("\nWriting Abaqus grid and connectivity file ....\n");

    if (asciiFlag == 0) {
        printf("\tBinary output is not currently supported when writing Abaqus mesh files\n");
        printf("\t..... switching to ASCII!\n");
        asciiFlag = 1;
    }

    if (scaleFactor <= 0) {
        printf("\tScale factor for mesh must be > 0! Defaulting to 1!\n");
        scaleFactor = 1;
    }

    filename = (char *) EG_alloc((strlen(fname) + 1 + strlen(fileExt)) *sizeof(char));
    if (filename == NULL) return EGADS_MALLOC;

    sprintf(filename,"%s%s",fname, fileExt);

    fp = fopen(filename,"w");

    if (fp == NULL) {
        printf("\tUnable to open file: %s\n", filename);
        status = CAPS_IOERR;
        goto cleanup;
    }

    if (mesh->meshQuickRef.useStartIndex == (int) false &&
        mesh->meshQuickRef.useListIndex  == (int) false) {

        status = mesh_fillQuickRefList(mesh);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // Write nodal coordinates
    fprintf(fp, "*NODE\n");
    for (i = 0; i < mesh->numNode; i++) {
        fprintf(fp,"%d, %.6e, %.6e, %.6e\n", mesh->node[i].nodeID,
                                             mesh->node[i].xyz[0]*scaleFactor,
                                             mesh->node[i].xyz[1]*scaleFactor,
                                             mesh->node[i].xyz[2]*scaleFactor);
    }


    // Search a mapAttrToIndex structure for a given index and return the corresponding keyword


    for (attrIndex = 0; attrIndex < attrMap->numAttribute; attrIndex++) {

        pID = attrMap->attributeIndex[attrIndex];
        status = get_mapAttrToIndexKeyword(attrMap, pID, &elemSet);
        if (status != CAPS_SUCCESS) goto cleanup;

        elementType = UnknownMeshElement;

        for (i = 0; i < mesh->numElement; i++) {

            // Grab Structure specific related data if available
            if (mesh->element[i].analysisType == MeshStructure) {
                feaData = (feaMeshDataStruct *) mesh->element[i].analysisData;
                propertyID = feaData->propertyID;
                //coordID = feaData->coordID;
                elementSubType = feaData->elementSubType;
            } else {
                propertyID = mesh->element[i].markerID;
                //coordID = 0;
                elementSubType = UnknownMeshSubElement;
            }

            if (pID != propertyID) continue;

            if (elementSubType != UnknownMeshSubElement) continue;

            if (elementType == UnknownMeshElement) {

                if ( mesh->element[i].elementType == Line) {
                    type = "B21";
                } else if (mesh->element[i].elementType == Triangle) {
                    type = "S3";
                } else if (mesh->element[i].elementType == Quadrilateral) {
                    type = "S4";
                } else if (mesh->element[i].elementType == Tetrahedral) {
                    type = "C3D4";
                } else if (mesh->element[i].elementType == Hexahedral) {
                    type = "C3D8";
                } else {
                    printf("Unsupported element type!\n");
                    status = CAPS_BADTYPE;
                    goto cleanup;
                }

                fprintf(fp,"*ELEMENT, TYPE=%s, ELSET=%s\n", type, elemSet);

                elementType = mesh->element[i].elementType;
            }

            if (mesh->element[i].elementType != elementType) {
                printf("Element %d belongs to ELSET %s, but it is not of type %s\n", mesh->element[i].elementID,
                        elemSet, type);
                status = CAPS_MISMATCH;
                goto cleanup;

            }

            // Print out mesh connectivity
            fprintf(fp, "%d", mesh->element[i].elementID);

            for (j = 0; j < mesh_numMeshElementConnectivity(&mesh->element[i]); j++) {
                fprintf(fp, ", ");
                fprintf(fp, "%d", mesh->element[i].connectivity[j]);
            }

            fprintf(fp, "\n");

        }
    }

    printf("Finished writing Abaqus grid file\n\n");

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_writeAbaqus, status %d\n", status);

        if (filename != NULL) EG_free(filename);
        if (fp != NULL) fclose(fp);

        return status;
}


// Extrude a surface mesh a single unit the length of extrusionLength - return a
// volume mesh, cell volume and left-handness is not checked
int extrude_SurfaceMesh(double extrusionLength, int extrusionMarker, meshStruct *surfaceMesh, meshStruct *volumeMesh)
{

    int status; // Status return

    int i; // Indexing

    int normalFixed = (int) false;

    double p1[3], p2[3], p3[3], normalVector[3], normalVectorInitial[3]= {0,0,0}; // Surface normal

    double tol = 1E-5;

    int elementIndex, numElement = 0;
    meshAnalysisTypeEnum analysisType;
    cfdMeshDataStruct *cfdData;

    printf("\nCreating a 3D volume mesh from a 2D surface\n");

    analysisType = surfaceMesh->analysisType;

    // Initiate volume mesh
    status = destroy_meshStruct(volumeMesh);
    if (status != CAPS_SUCCESS) return status;

    if (surfaceMesh->meshQuickRef.useStartIndex == (int) false &&
        surfaceMesh->meshQuickRef.useListIndex  == (int) false) {

        status = mesh_fillQuickRefList(surfaceMesh);
        if (status != CAPS_SUCCESS) goto bail;
    }

    // Check arrays
    if (surfaceMesh->meshQuickRef.numLine == 0) {
        printf("No edge boundaries saved - cannot use extrude_SurfaceMesh function!\n");
        status = CAPS_BADVALUE;
        goto bail;
    }

    // Set number of nodes
    volumeMesh->numNode = 2*surfaceMesh->numNode;

    // Nodes - allocate
    volumeMesh->node = (meshNodeStruct *) EG_alloc(volumeMesh->numNode*sizeof(meshNodeStruct));
    if (volumeMesh->node == NULL) {
        status = EGADS_MALLOC;
        goto bail;
    }

    // Initiate nodes
    for (i = 0; i < volumeMesh->numNode; i++) {

        status = initiate_meshNodeStruct(&volumeMesh->node[i], analysisType);
        if (status != CAPS_SUCCESS) goto bail;
    }

    // Set number of elements
    volumeMesh->meshQuickRef.numQuadrilateral =   surfaceMesh->meshQuickRef.numLine +
                                                2*surfaceMesh->meshQuickRef.numQuadrilateral;

    volumeMesh->meshQuickRef.numTriangle = 2*surfaceMesh->meshQuickRef.numTriangle;

    volumeMesh->meshQuickRef.numPrism = surfaceMesh->meshQuickRef.numTriangle;
    volumeMesh->meshQuickRef.numHexahedral = surfaceMesh->meshQuickRef.numQuadrilateral;

    volumeMesh->numElement = volumeMesh->meshQuickRef.numTriangle +
                             volumeMesh->meshQuickRef.numQuadrilateral +
                             volumeMesh->meshQuickRef.numPrism +
                             volumeMesh->meshQuickRef.numHexahedral;

    volumeMesh->element = (meshElementStruct *) EG_alloc(volumeMesh->numElement *sizeof(meshElementStruct));
    if (volumeMesh->element == NULL) {
        status = EGADS_MALLOC;
        goto bail;
    }

    for (i = 0; i < volumeMesh->numElement; i++) {
        status = initiate_meshElementStruct(&volumeMesh->element[i], analysisType);
        if (status != CAPS_SUCCESS) goto bail;
    }

    printf("\tNumber of Nodes =  %d\n", volumeMesh->numNode);
    printf("\tNumber of Prisms =  %d\n", volumeMesh->meshQuickRef.numPrism);
    printf("\tNumber of Hexahedral =  %d\n", volumeMesh->meshQuickRef.numHexahedral);

    // Determine surface normal sure all points are on the same plane

    for (i = 0; i < surfaceMesh->numElement; i++) {

        if (surfaceMesh->element[i].elementType == Triangle ||
            surfaceMesh->element[i].elementType == Quadrilateral) {

            p1[0] = surfaceMesh->node[surfaceMesh->element[i].connectivity[0]-1].xyz[0];
            p1[1] = surfaceMesh->node[surfaceMesh->element[i].connectivity[0]-1].xyz[1];
            p1[2] = surfaceMesh->node[surfaceMesh->element[i].connectivity[0]-1].xyz[2];

            p2[0] = surfaceMesh->node[surfaceMesh->element[i].connectivity[1]-1].xyz[0];
            p2[1] = surfaceMesh->node[surfaceMesh->element[i].connectivity[1]-1].xyz[1];
            p2[2] = surfaceMesh->node[surfaceMesh->element[i].connectivity[1]-1].xyz[2];

            p3[0] = surfaceMesh->node[surfaceMesh->element[i].connectivity[2]-1].xyz[0];
            p3[1] = surfaceMesh->node[surfaceMesh->element[i].connectivity[2]-1].xyz[1];
            p3[2] = surfaceMesh->node[surfaceMesh->element[i].connectivity[2]-1].xyz[2];
            get_Surface_Norm(p1, p2, p3, normalVector);

            if (normalFixed == (int) false) {

                normalVectorInitial[0] = fabs(normalVector[0]);
                normalVectorInitial[1] = fabs(normalVector[1]);
                normalVectorInitial[2] = fabs(normalVector[2]);

                normalFixed = (int) true;

                //printf("Surface normal vector INITIAL = %f %f %f\n", normalVectorInitial[0], normalVectorInitial[1], normalVectorInitial[2]);

            } else {

                if (fabs(normalVectorInitial[0] - fabs(normalVector[0])) > tol &&
                    fabs(normalVectorInitial[1] - fabs(normalVector[1])) > tol &&
                    fabs(normalVectorInitial[2] - fabs(normalVector[2])) > tol) {

                    printf("Warning points are not all on a single plane!!!\n");
                }
            }
        }
    }

    // Copy initial surface mesh
    for (i = 0; i < surfaceMesh->numNode; i++) {

        status = mesh_copyMeshNodeStruct(&surfaceMesh->node[i], 0, &volumeMesh->node[i]);
        if (status != CAPS_SUCCESS) goto bail;
    }

    // Get extrusion plane nodes
    for (i = 0; i < surfaceMesh->numNode; i++) {
        status = mesh_copyMeshNodeStruct(&surfaceMesh->node[i],
                                         surfaceMesh->numNode,
                                         &volumeMesh->node[i + surfaceMesh->numNode]);
        if (status != CAPS_SUCCESS) goto bail;

        volumeMesh->node[i+surfaceMesh->numNode].xyz[0] = surfaceMesh->node[i].xyz[0] + normalVectorInitial[0]*extrusionLength;
        volumeMesh->node[i+surfaceMesh->numNode].xyz[1] = surfaceMesh->node[i].xyz[1] + normalVectorInitial[1]*extrusionLength;
        volumeMesh->node[i+surfaceMesh->numNode].xyz[2] = surfaceMesh->node[i].xyz[2] + normalVectorInitial[2]*extrusionLength;
    }

    // Copy initial triangles and extruded triangles
    numElement = 0;
    for (i = 0; i < surfaceMesh->meshQuickRef.numTriangle; i++) {

        if (i == 0) volumeMesh->meshQuickRef.startIndexTriangle = numElement;

        if (surfaceMesh->meshQuickRef.startIndexTriangle >= 0) {
            elementIndex = surfaceMesh->meshQuickRef.startIndexTriangle +i;
        } else {
            elementIndex = surfaceMesh->meshQuickRef.listIndexTriangle[i];
        }

        status = mesh_copyMeshElementStruct(&surfaceMesh->element[elementIndex],
                                            0, 0,
                                            &volumeMesh->element[numElement]);
        if (status != CAPS_SUCCESS) goto bail;

        // the volume element cannot be connected to the surface anymore
        volumeMesh->element[numElement].topoIndex = -1;

        volumeMesh->element[numElement].elementID = numElement + 1;

        numElement += 1;

        status = mesh_copyMeshElementStruct(&surfaceMesh->element[elementIndex],
                                            0, surfaceMesh->numNode,
                                            &volumeMesh->element[numElement]);
        if (status != CAPS_SUCCESS) goto bail;

        // the volume element cannot be connected to the surface anymore
        volumeMesh->element[numElement].topoIndex = -1;

        volumeMesh->element[numElement].elementID = numElement + 1;
        volumeMesh->element[numElement].markerID = extrusionMarker;

        if (volumeMesh->element[numElement].analysisType == MeshCFD) {
            cfdData = (cfdMeshDataStruct *) volumeMesh->element[numElement].analysisData;
            cfdData->bcID = extrusionMarker;
        }

        numElement += 1;

    }
    // Copy initial quadrilaterals and extruded quadrilaterals
    for (i = 0; i < surfaceMesh->meshQuickRef.numQuadrilateral; i++) {

        if (i == 0) volumeMesh->meshQuickRef.startIndexQuadrilateral = numElement;

        if (surfaceMesh->meshQuickRef.startIndexQuadrilateral >= 0) {
            elementIndex = surfaceMesh->meshQuickRef.startIndexQuadrilateral +i;
        } else {
            elementIndex = surfaceMesh->meshQuickRef.listIndexQuadrilateral[i];
        }

        status = mesh_copyMeshElementStruct(&surfaceMesh->element[elementIndex],
                                            0, 0,
                                            &volumeMesh->element[numElement]);
        if (status != CAPS_SUCCESS) goto bail;

        // the volume element cannot be connected to the surface anymore
        volumeMesh->element[numElement].topoIndex = -1;

        volumeMesh->element[numElement].elementID = numElement + 1;

        numElement += 1;

        status = mesh_copyMeshElementStruct(&surfaceMesh->element[elementIndex],
                                            0, surfaceMesh->numNode,
                                            &volumeMesh->element[numElement]);
        if (status != CAPS_SUCCESS) goto bail;

        // the volume element cannot be connected to the surface anymore
        volumeMesh->element[numElement].topoIndex = -1;

        volumeMesh->element[numElement].elementID = numElement + 1;
        volumeMesh->element[numElement].markerID  = extrusionMarker;

        if (volumeMesh->element[numElement].analysisType == MeshCFD) {
            cfdData = (cfdMeshDataStruct *) volumeMesh->element[numElement].analysisData;
            cfdData->bcID = extrusionMarker;
        }
        numElement += 1;
    }

    // Create quadrilaterals from line elements
    for (i = 0; i < surfaceMesh->meshQuickRef.numLine; i++) {
        if (volumeMesh->meshQuickRef.startIndexQuadrilateral < 0) {
            volumeMesh->meshQuickRef.startIndexQuadrilateral = numElement;
        }

        if (surfaceMesh->meshQuickRef.startIndexLine >= 0) {
            elementIndex = surfaceMesh->meshQuickRef.startIndexLine +i;
        } else {
            elementIndex = surfaceMesh->meshQuickRef.listIndexLine[i];
        }

        volumeMesh->element[numElement].elementType  = Quadrilateral;
        volumeMesh->element[numElement].elementID    = numElement + 1;
        volumeMesh->element[numElement].markerID     = surfaceMesh->element[elementIndex].markerID;
        volumeMesh->element[numElement].analysisType = surfaceMesh->element[elementIndex].analysisType;
        volumeMesh->element[numElement].analysisData = surfaceMesh->element[elementIndex].analysisData;

        status = mesh_allocMeshElementConnectivity(&volumeMesh->element[numElement]);
        if (status != CAPS_SUCCESS) goto bail;

        volumeMesh->element[numElement].connectivity[0] = surfaceMesh->element[elementIndex].connectivity[0];
        volumeMesh->element[numElement].connectivity[1] = surfaceMesh->element[elementIndex].connectivity[1];

        volumeMesh->element[numElement].connectivity[2] = surfaceMesh->element[elementIndex].connectivity[1] + surfaceMesh->numNode;
        volumeMesh->element[numElement].connectivity[3] = surfaceMesh->element[elementIndex].connectivity[0] + surfaceMesh->numNode;
        numElement += 1;
    }

    // Create prisms from triangle elements
    for (i = 0; i < surfaceMesh->meshQuickRef.numTriangle; i++) {

        if (i == 0) volumeMesh->meshQuickRef.startIndexPrism = numElement;

        if (surfaceMesh->meshQuickRef.startIndexTriangle >= 0) {
            elementIndex = surfaceMesh->meshQuickRef.startIndexTriangle + i;
        } else {
            elementIndex = surfaceMesh->meshQuickRef.listIndexTriangle[i];
        }

        volumeMesh->element[numElement].elementType  = Prism;
        volumeMesh->element[numElement].elementID    = numElement + 1;
        volumeMesh->element[numElement].markerID     = 1;
        volumeMesh->element[numElement].analysisType = surfaceMesh->element[elementIndex].analysisType;
        volumeMesh->element[numElement].analysisData = surfaceMesh->element[elementIndex].analysisData;

        status = mesh_allocMeshElementConnectivity(&volumeMesh->element[numElement]);
        if (status != CAPS_SUCCESS) goto bail;

        volumeMesh->element[numElement].connectivity[0] = surfaceMesh->element[elementIndex].connectivity[0];
        volumeMesh->element[numElement].connectivity[1] = surfaceMesh->element[elementIndex].connectivity[1];
        volumeMesh->element[numElement].connectivity[2] = surfaceMesh->element[elementIndex].connectivity[2];

        volumeMesh->element[numElement].connectivity[3] = surfaceMesh->element[elementIndex].connectivity[0] + surfaceMesh->numNode;
        volumeMesh->element[numElement].connectivity[4] = surfaceMesh->element[elementIndex].connectivity[1] + surfaceMesh->numNode;
        volumeMesh->element[numElement].connectivity[5] = surfaceMesh->element[elementIndex].connectivity[2] + surfaceMesh->numNode;
        numElement += 1;
    }


    // Create hexahedral from quadrilateral elements
    for (i = 0; i < surfaceMesh->meshQuickRef.numQuadrilateral; i++) {

        if (i == 0) volumeMesh->meshQuickRef.startIndexHexahedral = numElement;

        if (surfaceMesh->meshQuickRef.startIndexQuadrilateral >= 0) {
            elementIndex = surfaceMesh->meshQuickRef.startIndexQuadrilateral + i;
        } else {
            elementIndex = surfaceMesh->meshQuickRef.listIndexQuadrilateral[i];
        }

        volumeMesh->element[numElement].elementType  = Hexahedral;
        volumeMesh->element[numElement].elementID    = numElement + 1;
        volumeMesh->element[numElement].markerID     = 1;
        volumeMesh->element[numElement].analysisType = surfaceMesh->element[elementIndex].analysisType;
        volumeMesh->element[numElement].analysisData = surfaceMesh->element[elementIndex].analysisData;

        status = mesh_allocMeshElementConnectivity(&volumeMesh->element[numElement]);
        if (status != CAPS_SUCCESS) goto bail;

        volumeMesh->element[numElement].connectivity[0] = surfaceMesh->element[elementIndex].connectivity[0];
        volumeMesh->element[numElement].connectivity[1] = surfaceMesh->element[elementIndex].connectivity[1];
        volumeMesh->element[numElement].connectivity[2] = surfaceMesh->element[elementIndex].connectivity[2];
        volumeMesh->element[numElement].connectivity[3] = surfaceMesh->element[elementIndex].connectivity[3];

        volumeMesh->element[numElement].connectivity[4] = surfaceMesh->element[elementIndex].connectivity[0] + surfaceMesh->numNode;
        volumeMesh->element[numElement].connectivity[5] = surfaceMesh->element[elementIndex].connectivity[1] + surfaceMesh->numNode;
        volumeMesh->element[numElement].connectivity[6] = surfaceMesh->element[elementIndex].connectivity[2] + surfaceMesh->numNode;
        volumeMesh->element[numElement].connectivity[7] = surfaceMesh->element[elementIndex].connectivity[3] + surfaceMesh->numNode;
        numElement += 1;
    }

    return CAPS_SUCCESS;

    bail:
        // Destroy volume mesh
        (void ) destroy_meshStruct(volumeMesh);

        return status;
}

// Retrieve the max valence and valence of each node in the mesh - currently only supports Triangle and Quadrilateral elements
int mesh_retrieveMaxValence(meshStruct *mesh,
                            int *maxValence,     // Max valence
                            int *nodeValence[]) {// Array of valences for each node  (freeable)

    int status; // Function return status

    int i, j, k, m, n; // Indexing

    int found;

    int numConnectivity;

    int numConnectID = 2, connectID[2]; // These are coupled

    int numValence = 0, *valenceList = NULL;

    *maxValence = 0;

    if (*nodeValence != NULL) EG_free(*nodeValence);

    *nodeValence = (int *) EG_alloc(mesh->numNode*sizeof(int));
    if (*nodeValence == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    // Loop through the nodes
    for (i = 0; i < mesh->numNode; i++) {

        numValence = 0;
        if (valenceList != NULL) EG_free(valenceList);
        valenceList = NULL;

        (*nodeValence)[i] = numValence;

        // Loop through the elements
        for (j = 0; j < mesh->numElement; j++) {

            numConnectivity = mesh_numMeshElementConnectivity(&mesh->element[j]);
            if (numConnectivity < 0) {
                status = numConnectivity;
                goto cleanup;
            }

            // Loop through element connectivity
            for (k = 0; k < numConnectivity; k++) {

                // Node ID found in connectivity for element
                if (mesh->node[i].nodeID == mesh->element[j].connectivity[k]) {

                    // Need to add logic in the future to support other element types
                    if (mesh->element[j].elementType == Triangle ||
                            mesh->element[j].elementType == Quadrilateral) {

                        if (k == 0) {
                            connectID[0] = mesh->element[j].connectivity[numConnectivity-1];
                            connectID[1] = mesh->element[j].connectivity[k+1];

                        } else if (k == numConnectivity -1) {
                            connectID[0] = mesh->element[j].connectivity[numConnectivity-2];
                            connectID[1] = mesh->element[j].connectivity[0];

                        } else {
                            connectID[0] = mesh->element[j].connectivity[k-1];
                            connectID[1] = mesh->element[j].connectivity[k+1];
                        }

                    } else {
                        printf("mesh_retrieveMaxValence currently only supports Triangle and Quadrilateral elements!\n");
                        status = CAPS_BADVALUE;
                        goto cleanup;

                    }

                    for (m = 0; m < numConnectID; m++) {

                        found = (int) false;
                        for (n = 0; n < numValence; n++) {
                            if (valenceList[n] == connectID[m]) {
                                found = (int) true;
                                break;
                            }
                        }

                        if (found == (int) false) {
                            numValence += 1;
                            valenceList = (int *) realloc(valenceList,numValence*sizeof(int));
                            if (valenceList == NULL) {
                                status = EGADS_MALLOC;
                                goto cleanup;
                            }

                            valenceList[numValence-1] = connectID[m];
                        }
                    } // End numConnectID loop

                    break; // Don't need to got through the remaining connectivity

                } // End NodeID found if

            } // End element connectivity loop

        } // End element loop

        (*nodeValence)[i] = numValence;

        if (numValence > *maxValence) *maxValence = numValence;

    } // End node loop

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:

        if (status != CAPS_SUCCESS) {
            printf("Error: Premature exit in mesh_retrieveMaxValence, status %d\n", status);
            if (*nodeValence != NULL) EG_free(*nodeValence);
        }

        if (valenceList != NULL) EG_free(valenceList);

        return status;
}

// Look at the nodeID for each node and check to see if it is being used in the element connectivity; if not it is removed
// Note: that the nodeIDs for the nodes and element connectivity isn't changed, as such if using element connectivity to blindly
// access a given node this could lead to seg-faults!. mesh_nodeID2Array must be used to access the node array index.
int mesh_removeUnusedNodes(meshStruct *mesh) {

    int status; // Function status

    int i, j, k; // Indexing

    int inode, numNode = 0;
    int *nodeUsed = NULL; // track which nodes are used

    meshNodeStruct *newNode=NULL;

    printf("Removing unused nodes...\n");

    // Get the maximum nodeID value
    if (mesh->node[mesh->numNode-1].nodeID != mesh->numNode) {
        printf("Error: Mesh has already had nodes removed!\n");
        status = CAPS_BADOBJECT;
        goto cleanup;
    }

    // Allocate the array to track indexes used
    nodeUsed = (int*) EG_alloc(mesh->numNode*sizeof(int));
    if (nodeUsed == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    for (inode = 0; inode < mesh->numNode; inode++) {
        nodeUsed[inode] = (int)false;
    }

    // Mark all the nodes used
    for (j = 0; j < mesh->numElement; j++) {
        for (k = 0; k < mesh_numMeshConnectivity(mesh->element[j].elementType); k++) {
            nodeUsed[mesh->element[j].connectivity[k]-1] = (int) true;
        }
    }

    // Count the number of used nodes
    for (i = 0; i < mesh->numNode; i++) {
        if (nodeUsed[i] == (int)true) numNode++;
    }

    newNode = (meshNodeStruct *) EG_alloc(numNode*sizeof(meshNodeStruct));
    if (newNode == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    inode = 0;
    for (i = 0; i < mesh->numNode; i++) {

        // Copy over the old new if still needed
        if (nodeUsed[i] == (int) true) {
            status = initiate_meshNodeStruct(&newNode[inode], mesh->node[i].analysisType);
            if (status != CAPS_SUCCESS) goto cleanup;

            status = mesh_copyMeshNodeStruct(&mesh->node[i], 0, &newNode[inode]);
            if (status != CAPS_SUCCESS) goto cleanup;

            inode += 1;
        }

        // Remove the old node
        status = destroy_meshNodeStruct(&mesh->node[i]);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    printf("\tRemoved %d (out of %d) unused nodes!\n", mesh->numNode-numNode, mesh->numNode);

    // swap out the count and memory in the mesh
    mesh->numNode = numNode;
    EG_free(mesh->node);
    mesh->node = newNode;

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:

        if (status != CAPS_SUCCESS) {

            if (newNode != NULL) EG_free(newNode);

            printf("Error: Premature exit in mesh_removeUnusedNodes, status %d\n", status);
        }

        EG_free(nodeUsed);
        return status;
}

// Constructs a map that maps from nodeID to the mesh->node array index
int mesh_nodeID2Array(const meshStruct *mesh,
                      int **n2a_out)
{
    int status; // Function status

    int inode, j, k; // Indexing

    int numNodeID = 0;
    int *n2a = NULL;

    if (mesh    == NULL) { status = CAPS_NULLVALUE; goto cleanup; }
    if (n2a_out == NULL) { status = CAPS_NULLVALUE; goto cleanup; }
    *n2a_out = NULL;

    // get the maximum nodeID value from the elements
    for (j = 0; j < mesh->numElement; j++) {
        for (k = 0; k < mesh_numMeshConnectivity(mesh->element[j].elementType); k++) {
            numNodeID = MAX(numNodeID, mesh->element[j].connectivity[k]);
        }
    }
    numNodeID++; // change to a count rather than ID

    // allocate the output array and initialize the map to -1
    n2a = (int*)EG_alloc(numNodeID*sizeof(int));
    for (inode = 0; inode < numNodeID; inode++) {
        n2a[inode] = -1;
    }

    for (inode = 0; inode < mesh->numNode; inode++) {
        if (mesh->node[inode].nodeID < 1) continue;
        // map the nodeID to the array mesh->node array index
        n2a[mesh->node[inode].nodeID] = inode;
    }

    *n2a_out = n2a;
    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) {
            printf("Error: Premature exit in mesh_nodeID2Array, status %d\n", status);

            if (n2a != NULL) EG_free(n2a);
            n2a = NULL;
        }

        return status;
}

// Create a new mesh with topology tagged with capsIgnore being removed, if capsIgnore isn't found the mesh is simply copied.
int mesh_createIgnoreMesh(meshStruct *mesh, meshStruct *meshIgnore) {
    int status;

    int i, j; // Indexing

    int numFace = 0, numEdge = 0, numNode = 0; // Number of egos
    ego *faces = NULL, *edges = NULL, *nodes = NULL; // Geometry
    int *ignoreFace=NULL, *ignoreEdge=NULL, *ignoreNode=NULL;

    ego body;
    int dummy;
    double coord[3];

    const char *string;

    int isNodeBody = (int) false;
    int ignoreFound = (int) false;

    meshElementStruct *temp;

    // Clean up outgoing mesh
    status = destroy_meshStruct(meshIgnore);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Get body from tessellation
    status = EG_statusTessBody(mesh->bodyTessMap.egadsTess, &body, &dummy, &dummy);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Determine the number of nodes
    status = EG_getBodyTopos(body, NULL, NODE, &numNode, &nodes);
    if (status != EGADS_SUCCESS) goto cleanup;

    // Determine the number of edges
    status = EG_getBodyTopos(body, NULL, EDGE, &numEdge, &edges);
    if (status != EGADS_SUCCESS) goto cleanup;

    // Determine the number of faces
    status = EG_getBodyTopos(body, NULL, FACE, &numFace, &faces);
    if (status != EGADS_SUCCESS) goto cleanup;

    if (numNode > 0) {
        ignoreNode = (int *) EG_alloc(numNode*sizeof(int));
        if (ignoreNode == NULL){
            status = EGADS_MALLOC;
            goto cleanup;
        }
    }
    if (numEdge > 0) {
        ignoreEdge = (int *) EG_alloc(numEdge*sizeof(int));
        if (ignoreEdge == NULL){
            status = EGADS_MALLOC;
            goto cleanup;
        }
    }
    if (numFace > 0) {
        ignoreFace = (int *) EG_alloc(numFace*sizeof(int));
        if (ignoreFace == NULL){
            status = EGADS_MALLOC;
            goto cleanup;
        }
    }

    // See if it is node body
    isNodeBody = aim_isNodeBody(body, coord);
    if (isNodeBody < EGADS_SUCCESS) goto cleanup;
    if (isNodeBody == EGADS_SUCCESS) {
      // all attributes are on the body rather than the node for a Node Body
     nodes[0] = body;
     numNode = 1;
    }

    for (i = 0; i < numNode; i++) ignoreNode[i] = (int) false;
    for (i = 0; i < numEdge; i++) ignoreEdge[i] = (int) false;
    for (i = 0; i < numFace; i++) ignoreFace[i] = (int) false;

    // Loop through faces
    for (i = 0; i < numFace; i++) {

        status = retrieve_CAPSIgnoreAttr(faces[i], &string);
        if (status != EGADS_SUCCESS && status != EGADS_NOTFOUND) goto cleanup;

        if (status == EGADS_NOTFOUND) continue;

        ignoreFound = (int) true;

        j = EG_indexBodyTopo(body,faces[i]);
        ignoreFace[j-1] = (int) true;
    }

    // Loop through edges
    for (i = 0; i < numEdge; i++) {
        status = retrieve_CAPSIgnoreAttr(edges[i], &string);
        if (status != EGADS_SUCCESS && status != EGADS_NOTFOUND) goto cleanup;

        if (status == EGADS_NOTFOUND) continue;

        ignoreFound = (int) true;

        j = EG_indexBodyTopo(body,edges[i]);
        ignoreEdge[j-1] = (int) true;
    }

    // Loop through nodes
    for (i = 0; i < numNode; i++) {
        status = retrieve_CAPSIgnoreAttr(nodes[i], &string);
        if (status != EGADS_SUCCESS && status != EGADS_NOTFOUND) goto cleanup;

        if (status == EGADS_NOTFOUND) continue;

        ignoreFound = (int) true;

        j = EG_indexBodyTopo(body,nodes[i]);
        ignoreNode[j-1] = (int) true;
    }


    if (ignoreFound == (int) true) {

        printf("capsIgnore attribute found. Removing unneeded nodes and elements from mesh!\n");

        status = initiate_meshStruct(meshIgnore);
        if (status != CAPS_SUCCESS) goto cleanup;

        meshIgnore->analysisType = mesh->analysisType;
        meshIgnore->meshType = mesh->meshType;

        // shallow copy of egads tessellations
        status = mesh_copyBodyTessMappingStruct(&mesh->bodyTessMap, &meshIgnore->bodyTessMap);
        if (status != CAPS_SUCCESS) goto cleanup;

        // Allocate nodes
        meshIgnore->numNode = mesh->numNode;

        meshIgnore->node = (meshNodeStruct *) EG_alloc(meshIgnore->numNode*sizeof(meshNodeStruct));
        if (meshIgnore->node == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        // Initiate nodes  and set nodes
        for (i = 0; i < meshIgnore->numNode; i++) {
            status = initiate_meshNodeStruct(&meshIgnore->node[i], meshIgnore->analysisType);
            if (status != CAPS_SUCCESS) goto cleanup;

            status = mesh_copyMeshNodeStruct(&mesh->node[i], 0, &meshIgnore->node[i]);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        for (i = 0; i< mesh->numElement; i++) {
            if (mesh->element[i].elementType == Node) {
                if (ignoreNode[ mesh->element[i].topoIndex-1] == (int) true) continue;
            }

            if (mesh->element[i].elementType == Line) {
                if (ignoreEdge[ mesh->element[i].topoIndex-1] == (int) true) continue;
            }

            if (mesh->element[i].elementType == Triangle ||
                mesh->element[i].elementType == Triangle_6 ||
                mesh->element[i].elementType == Quadrilateral ||
                mesh->element[i].elementType == Quadrilateral_8) {

                if (ignoreFace[ mesh->element[i].topoIndex-1] == (int) true) continue;
            }

            meshIgnore->numElement += 1;

            temp = (meshElementStruct *) EG_reall(meshIgnore->element, meshIgnore->numElement*sizeof(meshElementStruct));
            if (temp == NULL) {
                status = EGADS_MALLOC;
                meshIgnore->numElement -= 1;
                goto cleanup;
            }
            meshIgnore->element = temp;

            status = initiate_meshElementStruct(&meshIgnore->element[meshIgnore->numElement-1], meshIgnore->analysisType);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Should we be offsetting here?
            status = mesh_copyMeshElementStruct(&mesh->element[i], 0, 0, &meshIgnore->element[meshIgnore->numElement-1]);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        // Look at the nodeID for each node and check to see if it is being used in the element connectivity; if not it is removed
        // Note: that the nodeIDs for the nodes and element connectivity isn't changed, as such if using element connectivity to blindly
        // access a given node this could lead to seg-faults!. mesh_nodeID2Array must be used to access the node array index.
        status = mesh_removeUnusedNodes( meshIgnore );
        if (status != CAPS_SUCCESS) goto cleanup;

        status = mesh_fillQuickRefList( meshIgnore );
        if (status != CAPS_SUCCESS) goto cleanup;


    } else { // Copy mesh
        status = mesh_copyMeshStruct( mesh, meshIgnore);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_createIgnoreMesh, status %d\n", status);

        EG_free(faces);
        EG_free(edges);
        EG_free(nodes);

        EG_free(ignoreFace);
        EG_free(ignoreEdge);
        EG_free(ignoreNode);

        return status;
}


// Changes the analysisType of a mesh
int mesh_setAnalysisType(meshAnalysisTypeEnum analysisType, meshStruct *mesh) {

    int status;
    int i; //Indexing

    if (mesh  == NULL) return CAPS_NULLVALUE;

    // nothing to do if already the same analysis
    if (mesh->analysisType == analysisType) return CAPS_SUCCESS;

    // Destroy any old data
    for (i = 0; i < mesh->numNode; i++) {
        status = destroy_analysisData(&mesh->node[i].analysisData, mesh->node[i].analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    for (i = 0; i < mesh->numElement; i++){
        status = destroy_analysisData(&mesh->element[i].analysisData, mesh->element[i].analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // Set analysis type
    mesh->analysisType = analysisType;

    // initialize new data
    for (i = 0; i < mesh->numNode; i++) {
        mesh->node[i].analysisType = mesh->analysisType;
        status = initiate_analysisData(&mesh->node[i].analysisData, mesh->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    for (i = 0; i < mesh->numElement; i++){
        mesh->element[i].analysisType = mesh->analysisType;
        status = initiate_analysisData(&mesh->element[i].analysisData, mesh->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in mesh_setAnalysisType, status = %d\n", status);

        return status;
}
