#include <string.h>
#include <math.h>

#include "aimUtil.h" // Bring in AIM utility functions

#include "miscUtils.h" // Bring in misc. utility functions
#include "meshUtils.h" // Bring in mesh utility functions
#include "vlmUtils.h"  // Bring in vortex lattice method utility functions

#include "feaTypes.h"  // Bring in FEA structures
#include "meshTypes.h" // Bring in mesh structures
#include "capsTypes.h" // Bring in CAPS types

#include "feaUtils.h"

#ifdef WIN32
#define strcasecmp  stricmp
#endif

//#define           MIN(A,B)        (((A) < (B)) ? (A) : (B))
#define           MAX(A,B)        (((A) < (B)) ? (B) : (A))



int fea_createMesh(void *aimInfo,
                   double paramTess[3],                 // (in)  Tessellation parameters
                   int    edgePointMin,                 // (in)  minimum points along any Edge
                   int    edgePointMax,                 // (in)  maximum points along any Edge
                   int    quadMesh,                     // (in)  only do tris-for faces
                   mapAttrToIndexStruct *attrMap,       // (in)  map from CAPSGroup names to indexes
                   mapAttrToIndexStruct *constraintMap, // (in)  map from CAPSConstraint names to indexes
                   mapAttrToIndexStruct *loadMap,       // (in)  map from CAPSLoad names to indexes
                   mapAttrToIndexStruct *transferMap,   // (in)  map from CAPSTransfer names to indexes
                   mapAttrToIndexStruct *connectMap,    // (in)  map from CAPSConnect names to indexes
                   int *numMesh,                        // (out) total number of FEA mesh structures
                   meshStruct **feaMesh,                // (out) FEA mesh structure
                   feaProblemStruct *feaProblem ) {     // (out) FEA problem structure

    int status; // Function return status

    int i, body; // Indexing

    // Bodies
    const char *intents;
    int   numBody; // Number of Bodies
    ego  *bodies;
    const char *discipline;

    // Coordinate system
    mapAttrToIndexStruct coordSystemMap, attrMapTemp1, attrMapTemp2;

    // AIM Data transfer
    int  nrow, ncol, rank;
    void *dataTransfer;
    enum capsvType vtype;
    char *units;

    meshStruct *tempMesh, *feaMeshes = NULL;
    int *feaMeshList = NULL; // List to seperate structural meshes for aero

    ego tempBody;

    // Inherited fea/volume mesh related variables
    int numFEAMesh = 0;
    int feaMeshInherited = (int) false;

    // Destroy feaMeshes
    if ((*feaMesh) != NULL) {

        for (i = 0; i < *numMesh; i++) {
            status = destroy_meshStruct(&(*feaMesh)[i]);
            if (status != CAPS_SUCCESS) printf("Error: Status %d during destroy_meshStruct!\n", status);
        }

        EG_free(*feaMesh);
    }

    (*feaMesh) = NULL;
    *numMesh = 0;

    // Get AIM bodies
    status = aim_getBodies(aimInfo, &intents, &numBody, &bodies);
    if (status != CAPS_SUCCESS) return status;

    if ((numBody <= 0) || (bodies == NULL)) {
        printf(" fea_createMesh No Bodies!\n");

        return CAPS_SOURCEERR;
    }

    // Initiate our maps
    status = initiate_mapAttrToIndexStruct(&coordSystemMap);
    if (status != CAPS_SUCCESS) return status;

    status = initiate_mapAttrToIndexStruct(&attrMapTemp1);
    if (status != CAPS_SUCCESS) return status;

    status = initiate_mapAttrToIndexStruct(&attrMapTemp2);
    if (status != CAPS_SUCCESS) return status;

    // Alloc feaMesh list
    feaMeshList = (int *) EG_alloc(numBody*sizeof(int));
    if (feaMeshList == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    // Set all indexes to true
    for (i = 0; i < numBody; i++ ) feaMeshList[i] = (int) true;

    // Check for capsDiscipline consistency
    for (body = 0; body < numBody; body++) {

        status = retrieve_CAPSDisciplineAttr(bodies[body], &discipline);
        if (status != EGADS_SUCCESS) continue; // Need to add an error code

        if (strcasecmp(discipline, "structure") != 0) feaMeshList[body] = (int) false;
    }

    //for (body = 0; body< numBody; body++) printf("Body %i, FeaMeshList = %i\n", body, feaMeshList[body]);

    // Get CoordSystem attribute to index mapping
    status = create_CoordSystemAttrToIndexMap(numBody,
                                              bodies,
                                              3, //>2 - search the body, faces, edges, and all the nodes
                                              &coordSystemMap);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = fea_getCoordSystem(numBody,
                                bodies,
                                coordSystemMap,
                                &feaProblem->numCoordSystem,
                                &feaProblem->feaCoordSystem);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Get capsConstraint name and index mapping
    status = create_CAPSConstraintAttrToIndexMap(numBody,
                                                 bodies,
                                                 3, //>2 - search the body, faces, edges, and all the nodes
                                                 constraintMap);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Get capsLoad name and index mapping
    status = create_CAPSLoadAttrToIndexMap(numBody,
                                           bodies,
                                           3, //>2 - search the body, faces, edges, and all the nodes
                                           loadMap);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Get transfer to index mapping
    status = create_CAPSBoundAttrToIndexMap(numBody,
                                            bodies,
                                            3, //>2 - search the body, faces, edges, and all the nodes
                                            transferMap);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Get connect to index mapping
    status = create_CAPSConnectAttrToIndexMap(numBody,
                                              bodies,
                                              3, //>2 - search the body, faces, edges, and all the nodes
                                              connectMap);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Get attribute to index mapping
    status = aim_getData(aimInfo, "Attribute_Map", &vtype, &rank, &nrow, &ncol, &dataTransfer, &units);
    if (status == CAPS_SUCCESS) {

        printf("Found link for attrMap (Attribute_Map) from parent\n");

        status = copy_mapAttrToIndexStruct( (mapAttrToIndexStruct *) dataTransfer, attrMap);
        if (status != CAPS_SUCCESS) goto cleanup;

    } else {

        if (status == CAPS_DIRTY) printf("Parent AIMS are dirty\n");

        printf("Didn't find a link to attrMap from parent - getting it ourselves\n");

        // Get capsGroup name and index mapping to make sure all faces have a capsGroup value
        status = create_CAPSGroupAttrToIndexMap(numBody,
                                                bodies,
                                                3, //>2 - search the body, faces, edges, and all the nodes
                                                attrMap);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // See if a FEA mesh is available from parent
    status = aim_getData(aimInfo, "Surface_Mesh", &vtype, &rank, &nrow, &ncol, &dataTransfer, &units);
    if (status == CAPS_SUCCESS) {

        printf("Found link for a Surface mesh from parent\n");

        numFEAMesh = 1;
        if      (nrow == 1 && ncol != 1) numFEAMesh = ncol;
        else if (nrow != 1 && ncol == 1) numFEAMesh = nrow;
        else if (nrow >  1 && ncol >  1) {
            printf("Can not accept 2D matrix of surface meshes\n");
            status = CAPS_BADVALUE;
            goto cleanup;
        }

        if (numFEAMesh != numBody) { // May not be an error if we are doing aero-struct

            // Check for capsDiscipline consistency
            for (body = 0; body < numFEAMesh; body++) {

                tempMesh = &((meshStruct *) dataTransfer)[body];
                                                                                     // Dummy arguments
                status = EG_statusTessBody(tempMesh->bodyTessMap.egadsTess, &tempBody, &nrow, &ncol);
                if (status != EGADS_SUCCESS) goto cleanup;

                status = retrieve_CAPSDisciplineAttr(tempBody, &discipline);
                if (status != EGADS_SUCCESS) {
                    printf("Failed to find a capsDiscipline attribute!\n");
                    printf("Number of inherited surface meshes does not match the number of bodies, this is only allowed if doing aero-struct analysis\n");
                    status= CAPS_SOURCEERR;
                    goto cleanup;
                }

                if (strcasecmp(discipline, "structure") != 0) {
                    printf("Failed to find a capsDiscipline attribute - 'structure'!\n");
                    printf("Number of inherited surface meshes does not match the number of bodies, this is only allowed if doing aero-struct analysis\n");
                    status= CAPS_SOURCEERR;
                    goto cleanup;
                }
            }

            // We need to update our attribute map
             status = copy_mapAttrToIndexStruct( attrMap, &attrMapTemp1);
             if (status != CAPS_SUCCESS) goto cleanup;

             // Get capsGroup name and index mapping to make sure all faces have a capsGroup value
             status = create_CAPSGroupAttrToIndexMap(numBody,
                                                     bodies,
                                                     3, //>2 - search the body, faces, edges, and all the nodes
                                                     &attrMapTemp2);
             if (status != CAPS_SUCCESS) goto cleanup;

             status = merge_mapAttrToIndexStruct(&attrMapTemp1, &attrMapTemp2, attrMap);
             if (status != CAPS_SUCCESS) goto cleanup;

        }

        feaMeshes = (meshStruct *) EG_alloc(numFEAMesh*sizeof(meshStruct));
        if (feaMeshes == NULL) { status = EGADS_MALLOC; goto cleanup; }

        for (body = 0; body < numFEAMesh; body++) (void) initiate_meshStruct(&feaMeshes[body]);

        for (body = 0; body < numFEAMesh; body++) {

            // Create a new mesh with topology tagged with capsIgnore being removed, if capsIgnore isn't found the mesh is simply copied.
            status = mesh_createIgnoreMesh(&((meshStruct *) dataTransfer)[body], &feaMeshes[body]);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Change the analysis type of the mesh
            status = mesh_setAnalysisType(MeshStructure, &feaMeshes[body]);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        for (body = 0; body < numFEAMesh; body++) {

            // Update/change the analysis data in a meshStruct
            status = change_meshAnalysis(&feaMeshes[body], MeshStructure);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Get FEA Problem from EGADs body
            status = fea_setAnalysisData(attrMap,
                                         &coordSystemMap,
                                         constraintMap,
                                         loadMap,
                                         transferMap,
                                         connectMap,
                                         &feaMeshes[body]);
            if (status != CAPS_SUCCESS) goto cleanup;

            printf("\tMesh for body = %d\n", body);
            printf("\tNumber of nodal coordinates = %d\n", feaMeshes[body].numNode);
            printf("\tNumber of elements = %d\n", feaMeshes[body].numElement);
            printf("\tElemental Nodes = %d\n", feaMeshes[body].meshQuickRef.numNode);
            printf("\tElemental Rods  = %d\n", feaMeshes[body].meshQuickRef.numLine);
            printf("\tElemental Tria3 = %d\n", feaMeshes[body].meshQuickRef.numTriangle);
            printf("\tElemental Quad4 = %d\n", feaMeshes[body].meshQuickRef.numQuadrilateral);

            // set the resulting tessellation
            status = aim_setTess(aimInfo, feaMeshes[body].bodyTessMap.egadsTess);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        if (numFEAMesh > 1) {
            printf("Combining multiple FEA meshes!\n");
        }

        status = mesh_combineMeshStruct(numFEAMesh,
                                        feaMeshes,
                                        &feaProblem->feaMesh);
        if (status != CAPS_SUCCESS) goto cleanup;

        if (numFEAMesh > 1) {
            printf("\tCombined Number of nodal coordinates = %d\n", feaProblem->feaMesh.numNode);
            printf("\tCombined Number of elements = %d\n", feaProblem->feaMesh.numElement);
            printf("\tCombined Elemental Nodes = %d\n", feaProblem->feaMesh.meshQuickRef.numNode);
            printf("\tCombined Elemental Rods  = %d\n", feaProblem->feaMesh.meshQuickRef.numLine);
            printf("\tCombined Elemental Tria3 = %d\n", feaProblem->feaMesh.meshQuickRef.numTriangle);
            printf("\tCombined Elemental Quad4 = %d\n", feaProblem->feaMesh.meshQuickRef.numQuadrilateral);
        }

        // Set output meshes
        *numMesh = numFEAMesh;
        *feaMesh = feaMeshes;
        feaMeshes = NULL;

        // Set reference meshes
        feaProblem->feaMesh.numReferenceMesh = *numMesh;
        feaProblem->feaMesh.referenceMesh = (meshStruct *) EG_alloc(*numMesh*sizeof(meshStruct));
        if (feaProblem->feaMesh.referenceMesh == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        for (body = 0; body < *numMesh; body++) {
            feaProblem->feaMesh.referenceMesh[body] = (*feaMesh)[body];
        }

        feaMeshInherited = (int) true;

    } else if (status == CAPS_DIRTY) {

        printf("Link for a Surface mesh (Surface) from parent found, but Parent AIMS are dirty\n");
        goto cleanup;

    } else { // Check to see if a general unstructured volume mesh is available

        printf("Didn't find a Surface mesh (Surface_Mesh) from parent - checking for volume mesh\n");

        status = aim_getData(aimInfo, "Volume_MESH", &vtype, &rank, &nrow, &ncol, &dataTransfer, &units);
        if (status == CAPS_DIRTY) {

            printf("Link for a volume mesh (Volume_Mesh) from parent found, but Parent AIMS are dirty\n");
            goto cleanup;

        } else if (status == CAPS_SUCCESS) {

            printf("Found link for a  volume mesh (Volume_Mesh) from parent\n");

            numFEAMesh = 1;
            if (nrow != 1 && ncol != 1) {

                printf("Can not accept multiple volume meshes\n");
                status = CAPS_BADVALUE;
                goto cleanup;
            }

            if (numFEAMesh != numBody) {
                printf("Number of inherited volume meshes does not match the number of bodies - assuming volume mesh is already combined\n");
            }

            tempMesh = (meshStruct *) dataTransfer;
            status = mesh_copyMeshStruct( tempMesh, &feaProblem->feaMesh);
            if (status != CAPS_SUCCESS) goto cleanup;
//            feaProblem->feaMesh = *(meshStruct *) dataTransfer;

            // Set reference meshes
             feaProblem->feaMesh.numReferenceMesh = tempMesh->numReferenceMesh;
             feaProblem->feaMesh.referenceMesh = (meshStruct *) EG_alloc(tempMesh->numReferenceMesh*sizeof(meshStruct));
             if (feaProblem->feaMesh.referenceMesh == NULL) {
                 status = EGADS_MALLOC;
                 goto cleanup;
             }

             for (body = 0; body < tempMesh->numReferenceMesh; body++) {
                 feaProblem->feaMesh.referenceMesh[body] = tempMesh->referenceMesh[body];
             }

            for (i = 0; i < feaProblem->feaMesh.numReferenceMesh; i++) {

                status = aim_setTess(aimInfo,
                                     feaProblem->feaMesh.referenceMesh[i].bodyTessMap.egadsTess);
                if (status != CAPS_SUCCESS) goto cleanup;
            }

            // Update/change the analysis data in a meshStruct
            status = change_meshAnalysis(&feaProblem->feaMesh, MeshStructure);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Get FEA Problem from EGADs body
            status = fea_setAnalysisData(attrMap,
                                         &coordSystemMap,
                                         constraintMap,
                                         loadMap,
                                         transferMap,
                                         connectMap,
                                         &feaProblem->feaMesh);
            if (status != CAPS_SUCCESS) goto cleanup;

            feaMeshInherited = (int) true;
        } else {
            printf("Didn't find a volume mesh (Volume_Mesh) from parent\n");

            feaMeshInherited = (int) false;
        }
    }


    // If we didn't inherit a FEA mesh we need to get one ourselves
    if (feaMeshInherited == (int) false) {

        if (edgePointMin < 2) {
            printf("The minimum number of allowable edge points is 2 not %d\n", edgePointMin);
            edgePointMin = 2;
        }

        if (edgePointMax < edgePointMin) {
            printf("The maximum number of edge points must be greater than the current minimum (%d)\n", edgePointMin);
            edgePointMax = edgePointMin+1;
        }

        for (body = 0; body < numBody; body++) {
            if (feaMeshList[body] != (int) true) continue;

            *numMesh += 1;
            tempMesh = (meshStruct *) EG_reall((*feaMesh), *numMesh*sizeof(meshStruct));

            if (tempMesh == NULL) {
                status = EGADS_MALLOC;
                *numMesh -= 1;
                goto cleanup;
            }

            (*feaMesh) = tempMesh;

            status = initiate_meshStruct(&tempMesh[*numMesh-1]);
            if (status != CAPS_SUCCESS) goto cleanup;


            // Get FEA Problem from EGADs body
            status = fea_bodyToBEM(bodies[body], // (in)  EGADS Body
                                   paramTess,    // (in)  Tessellation parameters
                                   edgePointMin, // (in)  minimum points along any Edge
                                   edgePointMax, // (in)  maximum points along any Edge
                                   quadMesh,
                                   attrMap,
                                   &coordSystemMap,
                                   constraintMap,
                                   loadMap,
                                   transferMap,
                                   connectMap,
                                   &tempMesh[*numMesh-1]);

            if (status != CAPS_SUCCESS) goto cleanup;
            printf("\tMesh for body = %d\n", body);
            printf("\tNumber of nodal coordinates = %d\n", tempMesh[*numMesh-1].numNode);
            printf("\tNumber of elements = %d\n", tempMesh[*numMesh-1].numElement);
            printf("\tElemental Nodes = %d\n", tempMesh[*numMesh-1].meshQuickRef.numNode);
            printf("\tElemental Rods  = %d\n", tempMesh[*numMesh-1].meshQuickRef.numLine);
            printf("\tElemental Tria3 = %d\n", tempMesh[*numMesh-1].meshQuickRef.numTriangle);
            printf("\tElemental Quad4 = %d\n", tempMesh[*numMesh-1].meshQuickRef.numQuadrilateral);

            // set the resulting tessellation
            status = aim_setTess(aimInfo, tempMesh[*numMesh-1].bodyTessMap.egadsTess);
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        // Only compbine if there are actual meshes
        if (*numMesh > 0) {
            if (*numMesh > 1) printf("Combining multiple FEA meshes!\n");

            // Combine fea meshes into a single mesh for the problem
            status = mesh_combineMeshStruct(*numMesh,
                                            (*feaMesh),
                                            &feaProblem->feaMesh);
            if (status != CAPS_SUCCESS) goto cleanup;

            if (*numMesh > 1) {
                printf("\tCombined Number of nodal coordinates = %d\n", feaProblem->feaMesh.numNode);
                printf("\tCombined Number of elements = %d\n", feaProblem->feaMesh.numElement);
                printf("\tCombined Elemental Nodes = %d\n", feaProblem->feaMesh.meshQuickRef.numNode);
                printf("\tCombined Elemental Rods  = %d\n", feaProblem->feaMesh.meshQuickRef.numLine);
                printf("\tCombined Elemental Tria3 = %d\n", feaProblem->feaMesh.meshQuickRef.numTriangle);
                printf("\tCombined Elemental Quad4 = %d\n", feaProblem->feaMesh.meshQuickRef.numQuadrilateral);
            }

            // Set reference meshes
            feaProblem->feaMesh.numReferenceMesh = *numMesh;
            feaProblem->feaMesh.referenceMesh = (meshStruct *) EG_alloc(*numMesh*sizeof(meshStruct));
            if (feaProblem->feaMesh.referenceMesh == NULL) {
                status = EGADS_MALLOC;
                goto cleanup;
            }

            for (body = 0; body < *numMesh; body++) {
                feaProblem->feaMesh.referenceMesh[body] = (*feaMesh)[body];
            }
        }
    }
    status = CAPS_SUCCESS;

    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in fea_createMesh, status = %d\n", status);

        EG_free(feaMeshes);
        EG_free(feaMeshList);

        (void ) destroy_mapAttrToIndexStruct(&coordSystemMap);

        (void ) destroy_mapAttrToIndexStruct(&attrMapTemp1);
        (void ) destroy_mapAttrToIndexStruct(&attrMapTemp2);

        return status;
}

// Convert an EGADS body to a boundary element model, modified by Ryan Durscher (AFRL)
// from code originally written by John Dannenhoffer @ Syracuse University, patterned after code
// written by Bob Haimes  @ MIT
int fea_bodyToBEM(ego    ebody,                        // (in)  EGADS Body
                  double paramTess[3],                 // (in)  Tessellation parameters
                  int    edgePointMin,                 // (in)  minimum points along any Edge
                  int    edgePointMax,                 // (in)  maximum points along any Edge
                  int    quadMesh,                     // (in)  0 - only do tris-for faces, 1 - mixed quad/tri, 2 - regularized quads
                  mapAttrToIndexStruct *attrMap,       // (in)  map from CAPSGroup names to indexes
                  mapAttrToIndexStruct *coordSystemMap,// (in)  map from CoordSystem names to indexes
                  mapAttrToIndexStruct *constraintMap, // (in)  map from CAPSConstraint names to indexes
                  mapAttrToIndexStruct *loadMap,       // (in)  map from CAPSLoad names to indexes
                  mapAttrToIndexStruct *transferMap,   // (in)  map from CAPSTransfer names to indexes
                  mapAttrToIndexStruct *connectMap,    // (in)  map from CAPSConnect names to indexes
                  meshStruct *feaMesh)                 // (out) FEA mesh structure
{
    int status = 0; // Function return status

    int isNodeBody;
    int i, j, k, face, edge, patch; // Indexing
    double scale;

    // Body entities
    int numNode = 0, numEdge = 0, numFace = 0;
    ego *enodes=NULL, *eedges=NULL, *efaces=NULL;

    int atype, alen; // EGADS return variables
    const int    *ints;
    const double *reals;
    const char *string;

    // Meshing
    int numPoint = 0, numTri = 0;
    const int  *triConn = NULL, *triNeighbor = NULL; // Triangle

    int numPatch = 0; // Patching
    int n1, n2;

    int gID; // Global id

    const double *xyz, *uv;

    const int *ptype = NULL, *pindex = NULL, *pvindex = NULL, *pbounds = NULL;

    int       periodic, nchange, oclass, mtype, nchild, nchild2, *senses;

    // Edge point distributions
    int    *points=NULL, *isouth=NULL, *ieast=NULL, *inorth=NULL, *iwest=NULL;
    double  params[3], bbox[6], size, range[2], arclen, data[4], eval[18], eval2[18];
    double *rpos=NULL;
    ego     eref, *echilds, *echilds2, eloop, tempBody, topObj, prev, next;

    int bodySubType = 0; // Body classification

    int pointType, pointTopoIndex;
    double xyzPoint[3], xyzNode[3];

    // Attributues
    const char *attrName;
    int         attrIndex, coordSystemIndex, loadIndex;

    int numElement = 0; // Number of elements
    int coordID = 0; // Default coordinate id for mesh

    feaMeshDataStruct *feaData;

    meshElementStruct *tempElement = NULL;

    // In case our geometry has ignores
    int ignoreFound = (int) false;

    // ---------------------------------------------------------------

    printf("Creating FEA BEM\n");

    // Check for contradiction where quading is requested but dissabled on the body
    if (quadMesh == (int)true) {
        status = EG_attributeRet(ebody, ".qParams", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && (atype != ATTRREAL || (atype == ATTRREAL && reals[0] <= 0 ))) {
            printf("\tQuading on all faces disabled with .qParams attribute on the body\n");
            quadMesh = (int) false;
        }
    }

    // Get number of Nodes, Edges, and Faces in ebody
    status = EG_getBodyTopos(ebody, NULL, NODE, &numNode, &enodes);
    if (status < EGADS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(ebody, NULL, EDGE, &numEdge, &eedges);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(ebody, NULL, FACE, &numFace, &efaces);
    if (status < EGADS_SUCCESS) goto cleanup;

    // What type of BODY do we have?
    isNodeBody = aim_isNodeBody(ebody, xyzNode);
    if (isNodeBody < EGADS_SUCCESS) goto cleanup;
    if (isNodeBody == EGADS_SUCCESS) {
      // all attributes are on the body rather than the node for a node body
      enodes[0] = ebody;
    }

    // Determine the nominal number of points along each Edge
    points = (int    *) EG_alloc((numEdge+1)     *sizeof(int   ));
    rpos  = (double *) EG_alloc((edgePointMax)*sizeof(double));
    if (points == NULL || rpos == NULL) {
        status = EGADS_MALLOC;
        printf("\tError in fea_bodyToBEM: EG_alloc\n");
        goto cleanup;
    }

    status = EG_getBoundingBox(ebody, bbox);
    if (status < EGADS_SUCCESS) {
        printf("\tError in fea_bodyToBEM: EG_getBoundingBox\n");
        goto cleanup;
    }

    size = sqrt( (bbox[3] - bbox[0]) * (bbox[3] - bbox[0])
                 +(bbox[4] - bbox[1]) * (bbox[4] - bbox[1])
                 +(bbox[5] - bbox[2]) * (bbox[5] - bbox[2]));

    params[0] = paramTess[0] * size;
    params[1] = paramTess[1] * size;
    params[2] = paramTess[2];

    status = EG_attributeAdd(ebody, ".tParam", ATTRREAL, 3, NULL, params, NULL);
    if (status < EGADS_SUCCESS) {
        printf("\tError in fea_bodyToBEM: EG_attributeAdd\n");
        goto cleanup;
    }

    if (isNodeBody == EGADS_SUCCESS)
        params[0] = 1; // does not matter but can't be zero

    if (params[0] <= 0.0) {
        printf("\tError in fea_bodyToBEM: params[0] = %f must be a positive number!\n", params[0]);
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    for (i = 1; i <= numEdge; i++) {
        status = EG_getRange(eedges[i-1], range, &periodic);
        if (status < EGADS_SUCCESS) {
            printf("\tError in fea_bodyToBEM: EG_getRange\n");
            goto cleanup;
        }

        status = EG_arcLength(eedges[i-1], range[0], range[1], &arclen);
        if (status < EGADS_SUCCESS) {
            printf("\tError in fea_bodyToBEM: EG_arcLength\n");
            goto cleanup;
        }

        //points[i] = MIN(MAX(MAX(edgePointMin,2), (int)(1+arclen/params[0])), edgePointMax);
        points[i] = (int) min_DoubleVal(
                                        max_DoubleVal(
                                                max_DoubleVal( (double) edgePointMin, 2.0),
                                                (double) (1+arclen/params[0])),
                                        (double) edgePointMax);
    }


    // make arrays for "opposite" sides of four-sided Faces (with only one loop)
    isouth = (int *) EG_alloc((numFace+1)*sizeof(int));
    ieast  = (int *) EG_alloc((numFace+1)*sizeof(int));
    inorth = (int *) EG_alloc((numFace+1)*sizeof(int));
    iwest  = (int *) EG_alloc((numFace+1)*sizeof(int));

    if (isouth == NULL ||
        ieast == NULL  ||
        inorth == NULL ||
        iwest == NULL   ) {

        status = EGADS_MALLOC;
        printf("\tError in fea_bodyToBEM: EG_alloc\n");
        goto cleanup;
    }

    for (i = 1; i <= numFace; i++) {
        isouth[i] = 0;
        ieast [i] = 0;
        inorth[i] = 0;
        iwest [i] = 0;

        // nothing to check if quading isn't requested
        if (quadMesh == (int) false)
            continue;

        // check if quading is disabled with .qParams
        status = EG_attributeRet(efaces[i-1], ".qParams", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && (atype != ATTRREAL || (atype == ATTRREAL && reals[0] <= 0 ))) {
            printf("\tFace %d quading disabled with attribute .qParams\n", i);
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
            if (quadMesh == (int)true) {
                printf("Face %d has parallel edges - not quading\n", i);
            }
            continue;
        }

        if (status == EGADS_DEGEN) {
            if (quadMesh == (int)true) {
                printf("Face %d has a degenerate edge - not quading\n", i);
            }
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
    nchange = 1;
    for (i = 0; i < 20; i++) {
        nchange = 0;

        for (face = 1; face <= numFace; face++) {
            if (isouth[face] <= 0 || ieast[face] <= 0 ||
                inorth[face] <= 0 || iwest[face] <= 0   ) continue;

            if (points[iwest[face]] < points[ieast[face]]) {
                points[iwest[face]] = points[ieast[face]];
                nchange++;

            } else if (points[ieast[face]] < points[iwest[face]]) {
                       points[ieast[face]] = points[iwest[face]];
                nchange++;
            }

            if (points[isouth[face]] < points[inorth[face]]) {
                points[isouth[face]] = points[inorth[face]];
                nchange++;
            } else if (points[inorth[face]] < points[isouth[face]]) {
                       points[inorth[face]] = points[isouth[face]];
                nchange++;
            }
        }
        if (nchange == 0) break;
    }
    if (nchange > 0) {
        printf("Exceeded number of tries making \"opposite\" sides of four-sided Faces (with only one loop) match\n");
        status = CAPS_MISMATCH;
        goto cleanup;
    }

    // mark the Edges with points[iedge] evenly-spaced points
    for (edge = 1; edge <= numEdge; edge++) {
        for (i = 1; i < points[edge]-1; i++) {
            rpos[i-1] = (double)(i) / (double)(points[edge]-1);
        }

        if (points[edge] == 2) {
            i = 0;
            status = EG_attributeAdd(eedges[edge-1], ".rPos", ATTRINT, 1, &i, NULL, NULL);
            if (status < EGADS_SUCCESS) goto cleanup;

        } else {
            status = EG_attributeAdd(eedges[edge-1], ".rPos", ATTRREAL, points[edge]-2, NULL, rpos, NULL);
            if (status < EGADS_SUCCESS) goto cleanup;
        }
    }

    // Make tessellation
    status = EG_makeTessBody(ebody, params, &feaMesh->bodyTessMap.egadsTess);
    if (status != EGADS_SUCCESS) {
      printf("\tError in fea_bodyToBEM: EG_makeTessBody\n");
      goto cleanup;
    }

    // Make Quads on each four-sided Face
    params[0] = 0;
    params[1] = 0;
    params[2] = 0;

    // If making quads on faces lets setup an array to keep track of which faces have been quaded.
    if (quadMesh == (int)true) {
        feaMesh->bodyTessMap.numTessFace = numFace;
        if (feaMesh->bodyTessMap.tessFaceQuadMap != NULL) EG_free(feaMesh->bodyTessMap.tessFaceQuadMap);

        if( numFace > 0) {
            feaMesh->bodyTessMap.tessFaceQuadMap = (int *) EG_alloc(numFace*sizeof(int));
            if (feaMesh->bodyTessMap.tessFaceQuadMap == NULL) {
                status = EGADS_MALLOC;
                goto cleanup;
            }
        }
        // Set default to 0
        for (face = 0; face < numFace; face++) feaMesh->bodyTessMap.tessFaceQuadMap[face] = 0;
    }

    if (quadMesh == (int)true) {
        for (face = 1; face <= numFace; face++) {
            if (iwest[face] <= 0) continue;

            status = EG_makeQuads(feaMesh->bodyTessMap.egadsTess, params, face);
            if (status < EGADS_SUCCESS) {
                printf("Face = %d, failed to make quads\n", face);
                printf("Edges = %d %d %d %d\n", inorth[face], ieast[face], isouth[face], iwest[face]);
                continue;
                //goto cleanup;
            }
        }
    }

    // Set the mesh type information
    feaMesh->meshType = SurfaceMesh;
    feaMesh->analysisType = MeshStructure;

    // Get number of point in the tessellation
    status = EG_statusTessBody(feaMesh->bodyTessMap.egadsTess, &tempBody, &i, &feaMesh->numNode);
    if (status != EGADS_SUCCESS) goto cleanup;

    feaMesh->node = (meshNodeStruct *) EG_alloc(feaMesh->numNode*sizeof(meshNodeStruct));
    if (feaMesh->node == NULL) {
        feaMesh->numNode = 0;
        status = EGADS_MALLOC;
        goto cleanup;
    }

    for (i = 0; i < feaMesh->numNode; i++) {
        status = initiate_meshNodeStruct(&feaMesh->node[i], feaMesh->analysisType);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // Fill up the Attributes for the nodes
    for (i = 0; i < feaMesh->numNode; i++) {

        status = EG_getGlobal(feaMesh->bodyTessMap.egadsTess, i+1, &pointType, &pointTopoIndex, xyzPoint);
        if (status != EGADS_SUCCESS) goto cleanup;

        feaMesh->node[i].xyz[0] = xyzPoint[0];
        feaMesh->node[i].xyz[1] = xyzPoint[1];
        feaMesh->node[i].xyz[2] = xyzPoint[2];

        feaMesh->node[i].nodeID = i+1;

        feaData = (feaMeshDataStruct *) feaMesh->node[i].analysisData;

        status = fea_setFEADataPoint(efaces, eedges, enodes,
                                     attrMap,
                                     coordSystemMap,
                                     constraintMap,
                                     loadMap,
                                     transferMap,
                                     connectMap,
                                     pointType, pointTopoIndex,
                                     feaData); // Set the feaData structure
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    // Fill element information

    // If body is just a single node
    if (isNodeBody == EGADS_SUCCESS) {

        if (numNode != 1) {
            printf("NodeBody found, but more than one node being reported!\n");
            status = CAPS_BADVALUE;
            goto cleanup;
        }

        numElement = numNode;
        feaMesh->numElement = numElement;

        feaMesh->element = (meshElementStruct *) EG_alloc(feaMesh->numElement*sizeof(meshElementStruct));
        if (feaMesh->element == NULL) {
            status = EGADS_MALLOC;
            feaMesh->numElement = 0;
            goto cleanup;
        }

        i = 0;
        (void) initiate_meshElementStruct(&feaMesh->element[i], feaMesh->analysisType);

        status = retrieve_CAPSGroupAttr(enodes[i], &attrName);
        if (status != CAPS_SUCCESS) {
            printf("\tError: no capsGroup attribute found for node - %d!!\n", i+1);
            printf("Available attributes are:\n");
            print_AllAttr( enodes[i] );
            goto cleanup;
        }

        status = get_mapAttrToIndexIndex(attrMap, attrName, &attrIndex);
        if (status != CAPS_SUCCESS) {
            printf("\tError: capsGroup name %s not found in attribute to index map\n", attrName);
            goto cleanup;
        }

        feaMesh->element[i].elementType = Node;

        feaMesh->element[i].elementID = i+1;

        status = mesh_allocMeshElementConnectivity(&feaMesh->element[i]);
        if (status != CAPS_SUCCESS) goto cleanup;

        feaMesh->element[i].markerID = attrIndex;

        feaMesh->element[i].connectivity[0] = i+1;

        feaData = (feaMeshDataStruct *) feaMesh->element[i].analysisData;

        feaData->propertyID = attrIndex;

        status = get_mapAttrToIndexIndex(coordSystemMap, attrName, &coordSystemIndex);
        if (status == CAPS_SUCCESS) {
            feaData->coordID = coordSystemIndex;
        } else {
            feaData->coordID = coordID;
        }

        feaMesh->meshQuickRef.numNode += 1; // Add count

        feaMesh->meshQuickRef.startIndexNode = 0;
        feaMesh->meshQuickRef.useStartIndex = (int) true;

        status = CAPS_SUCCESS;
        goto cleanup;

    } //NODEBODY IF

    /* Determine of the body type */
    status = EG_getTopology(ebody, &eref, &oclass, &bodySubType, data, &nchild, &echilds, &senses);
    if (status != EGADS_SUCCESS) goto cleanup;

    // Can only have "free" edges in wire bodies - Don't want to count the edges of the faces
    //   as "free" edges
    if (bodySubType == WIREBODY) {
        feaMesh->numElement = numEdge;

        feaMesh->element = (meshElementStruct *) EG_alloc(feaMesh->numElement*sizeof(meshElementStruct));
        if (feaMesh->element == NULL) {
            status = EGADS_MALLOC;
            feaMesh->numElement = 0;
            goto cleanup;
        }

        for (i = 0; i < feaMesh->numElement; i++) {
            (void) initiate_meshElementStruct(&feaMesh->element[i], feaMesh->analysisType);
        }

        for (i = 0; i < numEdge; i++) {

            status = EG_getInfo(eedges[i], &oclass, &mtype, &topObj, &prev, &next);
            if (status != CAPS_SUCCESS) goto cleanup;
            if (mtype == DEGENERATE) continue;

            status = retrieve_CAPSIgnoreAttr(eedges[i], &attrName);
            if (status == CAPS_SUCCESS) {
                printf("\tcapsIgnore attribute found for edge - %d!!\n", i+1);
                ignoreFound = (int) true;
                continue;
            }

            numElement += 1;

            status = retrieve_CAPSGroupAttr(eedges[i], &attrName);
            if (status != CAPS_SUCCESS) {
                printf("\tError: no capsGroup attribute found for edge - %d!!\n", i+1);
                printf("Available attributes are:\n");
                print_AllAttr( eedges[i] );
                goto cleanup;
            }

            status = get_mapAttrToIndexIndex(attrMap, attrName, &attrIndex);
            if (status != CAPS_SUCCESS) {
                printf("\tError: capsGroup name %s not found in attribute to index map\n", attrName);
                goto cleanup;
            }

            feaMesh->element[numElement-1].elementType = Line;

            feaMesh->element[numElement-1].elementID = numElement;

            status = mesh_allocMeshElementConnectivity(&feaMesh->element[numElement-1]);
            if (status != CAPS_SUCCESS) goto cleanup;

            feaMesh->element[numElement-1].markerID = attrIndex;

            status = EG_getTessEdge(feaMesh->bodyTessMap.egadsTess, i+1, &numPoint, &xyz, &uv);
            if (status < EGADS_SUCCESS) goto cleanup;

            status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, -(i+1), 1, &gID);
            if (status != EGADS_SUCCESS) goto cleanup;

            feaMesh->element[numElement-1].connectivity[0] = gID;

            status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, -(i+1), numPoint, &gID);
            if (status != EGADS_SUCCESS) goto cleanup;

            feaMesh->element[numElement-1].connectivity[1] = gID;

            feaData = (feaMeshDataStruct *) feaMesh->element[numElement-1].analysisData;

            feaData->propertyID = attrIndex;

            status = get_mapAttrToIndexIndex(coordSystemMap, attrName, &coordSystemIndex);
            if (status == CAPS_SUCCESS) {
                feaData->coordID = coordSystemIndex;
            } else {
                feaData->coordID = coordID;
            }

            feaMesh->meshQuickRef.numLine += 1; // Add count

        }

        if (feaMesh->meshQuickRef.numLine != numEdge) { // Resize our array in case we have some ignores

            tempElement = (meshElementStruct *) EG_reall(feaMesh->element,
                                                         feaMesh->meshQuickRef.numLine*sizeof(meshElementStruct));

            if (tempElement == NULL) {
                status = EGADS_MALLOC;
                goto cleanup;
            }

            feaMesh->element = tempElement;

            feaMesh->numElement = feaMesh->meshQuickRef.numLine;
        }

        feaMesh->meshQuickRef.startIndexLine = 0;
        feaMesh->meshQuickRef.useStartIndex = (int) true;

        status = CAPS_SUCCESS;
        goto cleanup;

    } //WIREBODY IF

    if (quadMesh == (int) true && numFace > 0) {
        printf("\tGetting quads for BEM!\n");

        // Turn off meshQuick guide if you are getting quads
        feaMesh->meshQuickRef.useStartIndex = (int) false;
    } else {
        feaMesh->meshQuickRef.useStartIndex = (int) true;
        feaMesh->meshQuickRef.startIndexTriangle = numElement;
    }

    // Get Tris and Quads from faces
    for (face = 0; face < numFace; face++) {

        status = retrieve_CAPSIgnoreAttr(efaces[face], &attrName);
        if (status == CAPS_SUCCESS) {
            printf("\tcapsIgnore attribute found for face - %d!!\n", face+1);
            ignoreFound = (int) true;
            continue;
        }

        status = retrieve_CAPSGroupAttr(efaces[face], &attrName);
        if (status != CAPS_SUCCESS) {
            printf("Error: no capsGroup attribute found for face - %d!!\n", face+1);
            printf("Available attributes are:\n");
            print_AllAttr( efaces[face] );
            goto cleanup;
        }

        status = get_mapAttrToIndexIndex(attrMap, attrName, &attrIndex);
        if (status != CAPS_SUCCESS) {
            printf("Error: capsGroup name %s not found in attribute to index map\n", attrName);
            goto cleanup;
        }

        status = get_mapAttrToIndexIndex(coordSystemMap, attrName, &coordSystemIndex);
        if (status != CAPS_SUCCESS) coordSystemIndex = 0;


        loadIndex = CAPSMAGIC;
        status = retrieve_CAPSLoadAttr(efaces[face], &attrName);
        if (status == CAPS_SUCCESS) {

            status = get_mapAttrToIndexIndex(loadMap, attrName, &loadIndex);

            if (status != CAPS_SUCCESS) {
                printf("Error: capsLoad name %s not found in attribute to index map\n", attrName);
                goto cleanup;
            }
        }

        if (quadMesh == (int) true) {
            status = EG_getQuads(feaMesh->bodyTessMap.egadsTess, face+1, &numPoint, &xyz, &uv, &ptype, &pindex, &numPatch);
            if (status < EGADS_SUCCESS) goto cleanup;

        } else numPatch = -1;

        if (numPatch > 0) {

            if (numPatch != 1) {
                status = CAPS_NOTIMPLEMENT;
                printf("feaUtils: EG_localToGlobal accidentally only works for a single quad patch! FIXME!\n");
                goto cleanup;
            }

            feaMesh->bodyTessMap.tessFaceQuadMap[face] = 0;
            for (patch = 1; patch <= numPatch; patch++) {

                status = EG_getPatch(feaMesh->bodyTessMap.egadsTess, face+1, patch, &n1, &n2, &pvindex, &pbounds);
                if (status < EGADS_SUCCESS) goto cleanup;

                for (j = 1; j < n2; j++) {
                    for (i = 1; i < n1; i++) {
                        numElement += 1;

                        feaMesh->meshQuickRef.numQuadrilateral += 1;
                        feaMesh->numElement = numElement;

                        tempElement = (meshElementStruct *) EG_reall(feaMesh->element,
                                                                     feaMesh->numElement*sizeof(meshElementStruct));

                        if (tempElement == NULL) {
                            status = EGADS_MALLOC;
                            feaMesh->numElement -= 1;
                            goto cleanup;
                        }

                        feaMesh->element = tempElement;

                        status = initiate_meshElementStruct(&feaMesh->element[numElement-1], feaMesh->analysisType);
                        if (status != CAPS_SUCCESS) goto cleanup;

                        feaMesh->bodyTessMap.tessFaceQuadMap[face] += 1;

                        feaMesh->element[numElement-1].elementType = Quadrilateral;

                        feaMesh->element[numElement-1].elementID = numElement;

                        status = mesh_allocMeshElementConnectivity(&feaMesh->element[numElement-1]);
                        if (status != CAPS_SUCCESS) goto cleanup;

                        status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, face+1, pvindex[(i-1)+n1*(j-1)], &gID);
                        if (status != EGADS_SUCCESS) goto cleanup;

                        feaMesh->element[numElement-1].connectivity[0] = gID;

                        status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, face+1, pvindex[(i  )+n1*(j-1)], &gID);
                        if (status != EGADS_SUCCESS) goto cleanup;

                        feaMesh->element[numElement-1].connectivity[1] = gID;

                        status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, face+1, pvindex[(i  )+n1*(j  )], &gID);
                        if (status != EGADS_SUCCESS) goto cleanup;

                        feaMesh->element[numElement-1].connectivity[2] = gID;

                        status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, face+1, pvindex[(i-1)+n1*(j  )], &gID);
                        if (status != EGADS_SUCCESS) goto cleanup;

                        feaMesh->element[numElement-1].connectivity[3] = gID;

                        feaMesh->element[numElement-1].markerID = attrIndex;

                        feaData = (feaMeshDataStruct *) feaMesh->element[numElement-1].analysisData;

                        feaData->propertyID = attrIndex;
                        feaData->coordID = coordSystemIndex;
                        feaData->loadIndex = loadIndex;

                    }
                }

            }
        } else {
            status = EG_getTessFace(feaMesh->bodyTessMap.egadsTess, face+1,
                                    &numPoint, &xyz, &uv, &ptype, &pindex,
                                    &numTri, &triConn, &triNeighbor);
            if (status < EGADS_SUCCESS) goto cleanup;

            for (i= 0; i < numTri; i++) {

                numElement += 1;

                feaMesh->meshQuickRef.numTriangle += 1;
                feaMesh->numElement = numElement;


                tempElement = (meshElementStruct *) EG_reall(feaMesh->element,
                                                             feaMesh->numElement*sizeof(meshElementStruct));

                if (tempElement == NULL) {
                    status = EGADS_MALLOC;
                    feaMesh->numElement -= 1;
                    goto cleanup;
                }

                feaMesh->element = tempElement;

                status = initiate_meshElementStruct(&feaMesh->element[numElement-1], feaMesh->analysisType);
                if (status != CAPS_SUCCESS) goto cleanup;

                feaMesh->element[numElement-1].elementType = Triangle;

                feaMesh->element[numElement-1].elementID = numElement;

                status = mesh_allocMeshElementConnectivity(&feaMesh->element[numElement-1]);
                if (status != CAPS_SUCCESS) goto cleanup;

                status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, face+1, triConn[3*i + 0], &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                feaMesh->element[numElement-1].connectivity[0] = gID;

                status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, face+1, triConn[3*i + 1], &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                feaMesh->element[numElement-1].connectivity[1] = gID;

                status = EG_localToGlobal(feaMesh->bodyTessMap.egadsTess, face+1, triConn[3*i + 2], &gID);
                if (status != EGADS_SUCCESS) goto cleanup;

                feaMesh->element[numElement-1].connectivity[2] = gID;

                feaMesh->element[numElement-1].markerID = attrIndex;

                feaData = (feaMeshDataStruct *) feaMesh->element[numElement-1].analysisData;

                feaData->propertyID = attrIndex;
                feaData->coordID = coordSystemIndex;
                feaData->loadIndex = loadIndex;

            }
        }
    }

    if (ignoreFound == (int) true) {

        // Look at the nodeID for each node and check to see if it is being used in the element connectivity; if not it is removed
        // Note: that the nodeIDs for the nodes and element connectivity isn't changed, as such if using element connectivity to blindly
        // access a given node this could lead to seg-faults!. mesh_nodeID2Array must be used to access the node array index.
        status = mesh_removeUnusedNodes(feaMesh);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in fea_bodyToBem, status = %d\n", status);

        if (iwest  != NULL) EG_free(iwest );
        if (inorth != NULL) EG_free(inorth);
        if (ieast  != NULL) EG_free(ieast );
        if (isouth != NULL) EG_free(isouth);
        if (rpos   != NULL) EG_free(rpos  );
        if (points  != NULL) EG_free(points );

        if (enodes != NULL) EG_free(enodes);
        if (eedges != NULL) EG_free(eedges);
        if (efaces != NULL) EG_free(efaces);

        enodes = NULL;
        eedges =NULL;
        efaces =NULL;

        return status;
}

// Set the fea analysis meta data in a mesh
int fea_setAnalysisData( mapAttrToIndexStruct *attrMap,       // (in)  map from CAPSGroup names to indexes
                         mapAttrToIndexStruct *coordSystemMap,// (in)  map from CoordSystem names to indexes
                         mapAttrToIndexStruct *constraintMap, // (in)  map from CAPSConstraint names to indexes
                         mapAttrToIndexStruct *loadMap,       // (in)  map from CAPSLoad names to indexes
                         mapAttrToIndexStruct *transferMap,   // (in)  map from CAPSTransfer names to indexes
                         mapAttrToIndexStruct *connectMap,    // (in)  map from CAPSConnect names to indexes
                         meshStruct *feaMesh)                 // (in/out) FEA mesh structure
{
    int status = 0; // Function return status

    int i, face, body; // Indexing
    int dummy;

    int nodeOffset=0, elementOffset=0;

    // Body entities
    int numNode = 0, numEdge = 0, numFace = 0;
    ego *enodes=NULL, *eedges=NULL, *efaces=NULL;
    ego ebody;

    // Meshing
    int elem;

    int     oclass, mtype, nchild, *senses;

    // Edge point distributions
    int    *points=NULL, *isouth=NULL, *ieast=NULL, *inorth=NULL, *iwest=NULL;
    double  data[4];
    double *rpos=NULL;
    ego     eref, *echilds, topObj, prev, next;

    int isNodeBody, bodySubType = 0; // Body classification

    int pointType, pointTopoIndex;
    double xyzPoint[3];

    // Attributues
    const char *attrName;
    int         attrIndex, coordSystemIndex, loadIndex;

    feaMeshDataStruct *feaData = NULL;

    // ---------------------------------------------------------------

    if (feaMesh->meshType == SurfaceMesh || feaMesh->meshType == Surface2DMesh ) {
        printf("Setting FEA Data\n");

        // Get body from tessellation
        status = EG_statusTessBody(feaMesh->bodyTessMap.egadsTess, &ebody, &dummy, &dummy);
        if (status != CAPS_SUCCESS) goto cleanup;

        // Get number of Nodes, Edges, and Faces in ebody
        status = EG_getBodyTopos(ebody, NULL, NODE, &numNode, &enodes);
        if (status < EGADS_SUCCESS) goto cleanup;

        status = EG_getBodyTopos(ebody, NULL, EDGE, &numEdge, &eedges);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_getBodyTopos(ebody, NULL, FACE, &numFace, &efaces);
        if (status < EGADS_SUCCESS) goto cleanup;

        // What type of BODY do we have?
        isNodeBody = aim_isNodeBody(ebody, xyzPoint);
        if (isNodeBody < EGADS_SUCCESS) goto cleanup;
        if (isNodeBody == EGADS_SUCCESS) {
          // all attributes are on the body rather than the node for a node body
          enodes[0] = ebody;
        }

        // Fill up the Attributes for the nodes
        for (i = 0; i < feaMesh->numNode; i++) {

            status = EG_getGlobal(feaMesh->bodyTessMap.egadsTess, feaMesh->node[i].nodeID, &pointType, &pointTopoIndex, xyzPoint);
            if (status != EGADS_SUCCESS) goto cleanup;

            feaData = (feaMeshDataStruct *) feaMesh->node[i].analysisData;

            status = fea_setFEADataPoint(efaces, eedges, enodes,
                                         attrMap,
                                         coordSystemMap,
                                         constraintMap,
                                         loadMap,
                                         transferMap,
                                         connectMap,
                                         pointType, pointTopoIndex,
                                         feaData); // Set the feaData structure
            if (status != CAPS_SUCCESS) goto cleanup;
        }

        // Fill element information

        // If body is just a single node
        if (numNode == 1) {

            if (feaMesh->numNode != 1) {
                printf("NodeBody found, but more than one node being reported!\n");
                status = CAPS_BADVALUE;
                goto cleanup;
            }

            i = 0;
            status = retrieve_CAPSGroupAttr(enodes[i], &attrName);
            if (status != CAPS_SUCCESS) {
                printf("\tError: no capsGroup attribute found for node - %d!!\n", i+1);
                printf("Available attributes are:\n");
                print_AllAttr( enodes[i] );
                goto cleanup;
            }

            status = get_mapAttrToIndexIndex(attrMap, attrName, &attrIndex);
            if (status != CAPS_SUCCESS) {
                printf("\tError: capsGroup name %s not found in attribute to index map\n", attrName);
                goto cleanup;
            }

            feaData = (feaMeshDataStruct *) feaMesh->element[i].analysisData;

            feaData->propertyID = attrIndex;

            status = get_mapAttrToIndexIndex(coordSystemMap, attrName, &coordSystemIndex);
            if (status != CAPS_SUCCESS) coordSystemIndex = 0;

            feaData->coordID = coordSystemIndex;

            status = CAPS_SUCCESS;
            goto cleanup;

        } //NODEBODY IF

        /* Determine the body type */
        status = EG_getTopology(ebody, &eref, &oclass, &bodySubType, data, &nchild, &echilds, &senses);
        if (status != EGADS_SUCCESS) goto cleanup;

        // Can only have "free" edges in wire bodies - Don't want to count the edges of the faces
        //   as "free" edges
        if (bodySubType == WIREBODY) {

            for (i = 0; i < feaMesh->meshQuickRef.numLine; i++) {

                status = EG_getInfo(eedges[i], &oclass, &mtype, &topObj, &prev, &next);
                if (status != CAPS_SUCCESS) goto cleanup;
                if (mtype == DEGENERATE) continue;

                if (feaMesh->meshQuickRef.useStartIndex == (int)true)
                  elem = i + feaMesh->meshQuickRef.startIndexLine;
                else if (feaMesh->meshQuickRef.useListIndex == (int)true)
                  elem = feaMesh->meshQuickRef.listIndexLine[i];
                else {
                  status = CAPS_BADOBJECT;
                  printf("DEVELOPER ERROR: Both useStartIndex and useListIndex are true!\n");
                  goto cleanup;
                }

                attrIndex = feaMesh->element[elem].markerID;

                // get the capsGroup attribute string value
                status = get_mapAttrToIndexKeyword(attrMap, attrIndex, &attrName);
                if (status != CAPS_SUCCESS) {
                    printf("\tError: capsGroup index '%d' not found in attribute to index map\n", attrIndex);
                    goto cleanup;
                }

                feaData = (feaMeshDataStruct *) feaMesh->element[elem].analysisData;


                status = get_mapAttrToIndexIndex(coordSystemMap, attrName, &coordSystemIndex);
                if (status != CAPS_SUCCESS) coordSystemIndex = 0;

                feaData->propertyID = attrIndex;
                feaData->coordID    = coordSystemIndex;
            }

            status = CAPS_SUCCESS;
            goto cleanup;

        } //WIREBODY IF


        // Set tri and quad analysis data
        for (elem = 0; elem < feaMesh->numElement; elem++) {

          if (feaMesh->element[elem].elementType != Triangle &&
              feaMesh->element[elem].elementType != Triangle_6 &&
              feaMesh->element[elem].elementType != Quadrilateral &&
              feaMesh->element[elem].elementType != Quadrilateral_8) continue;

          attrIndex = feaMesh->element[elem].markerID;

          // get the capsGroup attribute string value
          status = get_mapAttrToIndexKeyword(attrMap, attrIndex, &attrName);
          if (status != CAPS_SUCCESS) {
            printf("\tError: capsGroup index '%d' not found in attribute to index map\n", attrIndex);
            goto cleanup;
          }

          status = get_mapAttrToIndexIndex(coordSystemMap, attrName, &coordSystemIndex);
          if (status != CAPS_SUCCESS) coordSystemIndex = 0;

          // get the face index
          face = feaMesh->element[elem].topoIndex;
          if (face < 1 || face > numFace) {
            printf("Error: Element '%d': Invalid face topological index: %d, [1-%d]\n", elem, face, numFace);
            status = CAPS_BADVALUE;
            goto cleanup;
          }

          loadIndex = CAPSMAGIC;
          status = retrieve_CAPSLoadAttr(efaces[face-1], &attrName);
          if (status == CAPS_SUCCESS) {

            status = get_mapAttrToIndexIndex(loadMap, attrName, &loadIndex);

            if (status != CAPS_SUCCESS) {
              printf("Error: capsLoad name %s not found in attribute to index map\n", attrName);
              goto cleanup;
            }
          }

          feaData = (feaMeshDataStruct *) feaMesh->element[elem].analysisData;

          feaData->propertyID = attrIndex;
          feaData->coordID    = coordSystemIndex;
          feaData->loadIndex  = loadIndex;
        }
    } else if (feaMesh->meshType == VolumeMesh) {

        // Show warning statement
        printf("Warning - surface nodes are assumed to be packed sequentially in the volume\n");

        // Loop through reference meshes
        for (body = 0; body < feaMesh->numReferenceMesh; body++) {
            printf("Setting FEA Data from reference mesh %d (of %d)\n", body+1, feaMesh->numReferenceMesh);

            // Get body from tessellation
            status = EG_statusTessBody(feaMesh->referenceMesh[body].bodyTessMap.egadsTess, &ebody, &dummy, &dummy);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Get number of Nodes, Edges, and Faces in ebody
            status = EG_getBodyTopos(ebody, NULL, NODE, &numNode, &enodes);
            if (status < EGADS_SUCCESS) goto cleanup;

            status = EG_getBodyTopos(ebody, NULL, EDGE, &numEdge, &eedges);
            if (status != EGADS_SUCCESS) goto cleanup;

            status = EG_getBodyTopos(ebody, NULL, FACE, &numFace, &efaces);
            if (status < EGADS_SUCCESS) goto cleanup;

            // What type of BODY do we have?
            isNodeBody = aim_isNodeBody(ebody, xyzPoint);
            if (isNodeBody < EGADS_SUCCESS) goto cleanup;

            // If body is just a single node
              if (isNodeBody == EGADS_SUCCESS) {

                  printf("NodeBody found, not currently supported for VolumeMesh!\n");
                  status = CAPS_BADVALUE;
                  goto cleanup;

              } //NODEBODY IF

            // Fill up the Attributes for the nodes
            for (i = 0; i < feaMesh->referenceMesh[body].numNode; i++) {

                status = EG_getGlobal(feaMesh->referenceMesh[body].bodyTessMap.egadsTess, feaMesh->referenceMesh[body].node[i].nodeID, &pointType, &pointTopoIndex, xyzPoint);
                if (status != EGADS_SUCCESS) goto cleanup;

                feaData = (feaMeshDataStruct *) feaMesh->node[i+nodeOffset].analysisData;

                status = fea_setFEADataPoint(efaces, eedges, enodes,
                                             attrMap,
                                             coordSystemMap,
                                             constraintMap,
                                             loadMap,
                                             transferMap,
                                             connectMap,
                                             pointType, pointTopoIndex,
                                             feaData); // Set the feaData structure
                if (status != CAPS_SUCCESS) goto cleanup;
            }

            // Fill element information //

            // Determine the body type
            status = EG_getTopology(ebody, &eref, &oclass, &bodySubType, data, &nchild, &echilds, &senses);
            if (status != EGADS_SUCCESS) goto cleanup;

            // Can only have "free" edges in wire bodies - Don't want to count the edges of the faces
            //   as "free" edges
            if (bodySubType == WIREBODY) {

                printf("WireBody found, not currently supported for VolumeMesh!\n");
                status = CAPS_BADVALUE;
                goto cleanup;

            } //WIREBODY IF

            // Set tri and quad analysis data
            for (elem = 0; elem < feaMesh->referenceMesh[body].numElement; elem++) {

                if (feaMesh->referenceMesh[body].element[elem].elementType != Triangle &&
                    feaMesh->referenceMesh[body].element[elem].elementType != Triangle_6 &&
                    feaMesh->referenceMesh[body].element[elem].elementType != Quadrilateral &&
                    feaMesh->referenceMesh[body].element[elem].elementType != Quadrilateral_8) continue;

//
//                attrIndex = feaMesh->referenceMesh[body].element[elem].markerID;
//
//                // get the capsGroup attribute string value
//                status = get_mapAttrToIndexKeyword(attrMap, attrIndex, &attrName);
//                if (status != CAPS_SUCCESS) {
//                    printf("\tError: capsGroup index '%d' not found in attribute to index map\n", attrIndex);
//                    goto cleanup;
//                }
//
//                status = get_mapAttrToIndexIndex(coordSystemMap, attrName, &coordSystemIndex);
//                if (status != CAPS_SUCCESS) coordSystemIndex = 0;

                // get the face index
                face = feaMesh->referenceMesh[body].element[elem].topoIndex;
                if (face < 1 || face > numFace) {
                    printf("Error: Element '%d': Invalid face topological index: %d, [1-%d]\n", elem, face, numFace);
                    status = CAPS_BADVALUE;
                    goto cleanup;
                }

                loadIndex = CAPSMAGIC;
                status = retrieve_CAPSLoadAttr(efaces[face-1], &attrName);
                if (status == CAPS_SUCCESS) {

                    status = get_mapAttrToIndexIndex(loadMap, attrName, &loadIndex);

                    if (status != CAPS_SUCCESS) {
                        printf("Error: capsLoad name %s not found in attribute to index map\n", attrName);
                        goto cleanup;
                    }
                }

                feaData = (feaMeshDataStruct *) feaMesh->element[elem+elementOffset].analysisData;

                //feaData->propertyID = attrIndex; DONT THINK WE WANT TO CHANGE THE PROPERTY TYPE
                // feaData->coordID    = coordSystemIndex; DONT THINK WE WANT TO CHANGE THE Coordinate TYPE
                feaData->loadIndex  = loadIndex;
            }

            nodeOffset += feaMesh->referenceMesh[body].numNode;
            elementOffset += feaMesh->referenceMesh[body].numElement;
        }

    } else {
        printf("Unknown meshType!\n");
        status = CAPS_BADTYPE;
        goto cleanup;
    }

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("\tPremature exit in fea_setAnalysisData, status = %d\n", status);

        EG_free(iwest );
        EG_free(inorth);
        EG_free(ieast );
        EG_free(isouth);
        EG_free(rpos  );
        EG_free(points );

        EG_free(enodes);
        EG_free(eedges);
        EG_free(efaces);

        enodes = NULL;
        eedges = NULL;
        efaces = NULL;

        return status;
}

// Set feaData for a given point index and topology index. Ego faces, edges, and nodes must be provided along with attribute maps
int fea_setFEADataPoint(ego *faces, ego *edges, ego *nodes,
                        mapAttrToIndexStruct *attrMap, // Not currently used
                        mapAttrToIndexStruct *coordSystemMap,
                        mapAttrToIndexStruct *constraintMap,
                        mapAttrToIndexStruct *loadMap,
                        mapAttrToIndexStruct *transferMap,
                        mapAttrToIndexStruct *connectMap,
                        int pointType, int pointTopoIndex,
                        feaMeshDataStruct *feaData) { // Set the feaData structure

    int status;

    int coordID = 0; // Default coordinate id for mesh

    // Attributes
    const char *attrName;
    int         constraintIndex, loadIndex, transferIndex, connectIndex, connectLinkIndex; //coordSystemIndex

    feaData->coordID = coordID;

    // Get attribute index on entity
    constraintIndex = CAPSMAGIC;
    loadIndex = CAPSMAGIC;
    transferIndex = CAPSMAGIC;
    connectIndex = CAPSMAGIC;
    connectLinkIndex = CAPSMAGIC;

    if (pointType == 0) { // Node

        status = retrieve_CAPSConstraintAttr(nodes[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(constraintMap, attrName, &constraintIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSLoadAttr(nodes[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(loadMap, attrName, &loadIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSBoundAttr(nodes[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(transferMap, attrName, &transferIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSConnectAttr(nodes[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(connectMap, attrName, &connectIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSConnectLinkAttr(nodes[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(connectMap, attrName, &connectLinkIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

    } else if (pointType > 0 ) { // Edge

        status = retrieve_CAPSConstraintAttr(edges[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(constraintMap, attrName, &constraintIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSLoadAttr(edges[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(loadMap, attrName, &loadIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSBoundAttr(edges[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(transferMap, attrName, &transferIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSConnectAttr(edges[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(connectMap, attrName, &connectIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSConnectLinkAttr(edges[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(connectMap, attrName, &connectLinkIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

    } else { // Face

        status = retrieve_CAPSConstraintAttr(faces[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(constraintMap, attrName, &constraintIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSLoadAttr(faces[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(loadMap, attrName, &loadIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSBoundAttr(faces[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(transferMap, attrName, &transferIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSConnectAttr(faces[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(connectMap, attrName, &connectIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

        status = retrieve_CAPSConnectLinkAttr(faces[pointTopoIndex-1], &attrName);
        if (status == CAPS_SUCCESS) {
            status = get_mapAttrToIndexIndex(connectMap, attrName, &connectLinkIndex);
            if (status != CAPS_SUCCESS && status != CAPS_NOTFOUND && status != CAPS_NULLVALUE) goto cleanup;
        }

    }

    feaData->constraintIndex = constraintIndex;
    feaData->loadIndex = loadIndex;
    feaData->transferIndex = transferIndex;

    feaData->connectIndex = connectIndex;
    feaData->connectLinkIndex = connectLinkIndex;

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:

        if (status != CAPS_SUCCESS) printf("Error: Premature exit in fea_setFEADataPoint, status %d\n", status);

        return status;
}
// Initiate (0 out all values and NULL all pointers) of feaProperty in the feaPropertyStruct structure format
int initiate_feaPropertyStruct(feaPropertyStruct *feaProperty) {

    feaProperty->name = NULL;

    feaProperty->propertyType = UnknownProperty;

    feaProperty->propertyID = 0; // ID number of property

    feaProperty->materialID = 0; // ID number of material
    feaProperty->materialName = NULL; // Name of material associated with material ID

    // Rods
    feaProperty->crossSecArea           = 0.0; // Bar cross-sectional area
    feaProperty->torsionalConst         = 0.0; // Torsional constant
    feaProperty->torsionalStressReCoeff = 0.0; // Torsional stress recovery coefficient
    feaProperty->massPerLength          = 0.0; // Mass per unit length

    // Bar - see rod for additional variables
    feaProperty->zAxisInertia = 0.0; // Section moment of inertia about the z axis
    feaProperty->yAxisInertia = 0.0; // Section moment of inertia about the y axis

    feaProperty->yCoords[0] = 0.0; // Element y, z coordinates, in the bar cross-section, of
    feaProperty->yCoords[1] = 0.0; //    of four points at which to recover stresses
    feaProperty->yCoords[2] = 0.0;
    feaProperty->yCoords[3] = 0.0;

    feaProperty->zCoords[0] = 0.0;
    feaProperty->zCoords[1] = 0.0;
    feaProperty->zCoords[2] = 0.0;
    feaProperty->zCoords[3] = 0.0;

    feaProperty->areaShearFactors[0] = 0.0; // Area factors for shear
    feaProperty->areaShearFactors[1] = 0.0;

    feaProperty->crossProductInertia = 0.0; // Section cross-product of inertia

    // Shear
    feaProperty->shearPanelThickness  = 0.0; // Shear panel thickness
    feaProperty->nonStructMassPerArea = 0.0; // Nonstructural mass per unit area

    // Shell
    feaProperty->membraneThickness = 0.0;     // Membrane thickness
    feaProperty->materialBendingID = 0;     /// ID number of material for bending - if not specified and bendingInertiaRatio > 0 this value defaults to materialID
    feaProperty->bendingInertiaRatio = 1.0; // Ratio of actual bending moment inertia (I) to bending inertia of a solid
                                            //   plate of thickness TM Real  - default 1.0
    feaProperty->materialShearID = 0;       // ID number of material for shear - if not specified and shearMembraneRatio > 0 this value defaults to materialID
    feaProperty->shearMembraneRatio  = 5.0/6.0;     // Ratio of shear to membrane thickness  - default 5/6
    feaProperty->massPerArea = 0.0;           // Mass per unit area
    //feaProperty->neutralPlaneDist[0] = 0;   // Distances from the neutral plane of the plate to locations where
    //feaProperty->neutralPlaneDist[1] = 0;   //   stress is calculate

    feaProperty->compositeShearBondAllowable = 0.0;  // Shear stress limit for bonding between plies
    feaProperty->compositeFailureTheory = NULL;      // HILL, HOFF, TSAI, STRN
    feaProperty->compositeSymmetricLaminate = (int) false;  // 1- SYM only half the plies are specified, for odd number plies 1/2 thickness of center ply is specified
                                                            // the first ply is the bottom ply in the stack, default (0) all plies specified
    feaProperty->numPly = 0;
    feaProperty->compositeMaterialID = NULL;      // Vector of material ID's from bottom to top for all plies
    feaProperty->compositeThickness = NULL;        // Vector of thicknesses from bottom to top for all plies
    feaProperty->compositeOrientation = NULL;      // Vector of orientations from bottom to top for all plies

    // Solid

    // Concentrated Mass
    feaProperty->mass = 0.0; // Mass value
    feaProperty->massOffset[0]  = 0.0; // Offset distance from the grid point to the center of gravity
    feaProperty->massOffset[1]  = 0.0;
    feaProperty->massOffset[2]  = 0.0;
    feaProperty->massInertia[0] = 0.0; // Mass moment of inertia measured at the mass center of gravity
    feaProperty->massInertia[1] = 0.0;
    feaProperty->massInertia[2] = 0.0;
    feaProperty->massInertia[3] = 0.0;
    feaProperty->massInertia[4] = 0.0;
    feaProperty->massInertia[5] = 0.0;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaProperty in the feaPropertyStruct structure format
int destroy_feaPropertyStruct(feaPropertyStruct *feaProperty) {

    if (feaProperty->name != NULL) EG_free(feaProperty->name);

    feaProperty->propertyType = UnknownProperty;

    feaProperty->propertyID = 0; // ID number of property

    feaProperty->materialID = 0; // ID number of material

    if (feaProperty->materialName != NULL) EG_free(feaProperty->materialName); // Name of material associated with material ID
    feaProperty->materialName = NULL;

    // Rods
    feaProperty->crossSecArea           = 0; // Bar cross-sectional area
    feaProperty->torsionalConst         = 0; // Torsional constant
    feaProperty->torsionalStressReCoeff = 0; // Torsional stress recovery coefficient
    feaProperty->massPerLength          = 0; // Mass per unit length

    // Bar - see rod for additional variables
    feaProperty->zAxisInertia = 0; // Section moment of inertia about the z axis
    feaProperty->yAxisInertia = 0; // Section moment of inertia about the y axis

    feaProperty->yCoords[0] = 0; // Element y, z coordinates, in the bar cross-section, of
    feaProperty->yCoords[1] = 0; //    of four points at which to recover stresses
    feaProperty->yCoords[2] = 0;
    feaProperty->yCoords[3] = 0;

    feaProperty->zCoords[0] = 0;
    feaProperty->zCoords[1] = 0;
    feaProperty->zCoords[2] = 0;
    feaProperty->zCoords[3] = 0;

    feaProperty->areaShearFactors[0] = 0; // Area factors for shear
    feaProperty->areaShearFactors[1] = 0;

    feaProperty->crossProductInertia = 0; // Section cross-product of inertia

    // Shear
    feaProperty->shearPanelThickness  = 0; // Shear panel thickness
    feaProperty->nonStructMassPerArea = 0; // Nonstructural mass per unit area

    // Shell
    feaProperty->membraneThickness = 0; // Membrane thickness
    feaProperty->materialBendingID = 0;     // ID number of material for bending
    feaProperty->bendingInertiaRatio = 1.0; // Ratio of actual bending moment inertia (I) to bending inertia of a solid
    //   plate of thickness TM Real  - default 1.0
    feaProperty->materialShearID = 0;       // ID number of material for shear
    feaProperty->shearMembraneRatio  = 5.0/6.0;  // Ratio of shear to membrane thickness  - default 5/6
    feaProperty->massPerArea = 0; // Mass per unit area
    //feaProperty->neutralPlaneDist[0] = 0;   // Distances from the neutral plane of the plate to locations where
    //feaProperty->neutralPlaneDist[1] = 0;   //   stress is calculate

    feaProperty->numPly = 0;
    feaProperty->compositeShearBondAllowable = 0.0;  // Shear stress limit for bonding between plies
    if (feaProperty->compositeFailureTheory != NULL) EG_free(feaProperty->compositeFailureTheory); // HILL, HOFF, TSAI, STRN
    feaProperty->compositeFailureTheory = NULL;
    feaProperty->compositeSymmetricLaminate = (int) false;  // 1- SYM only half the plies are specified, for odd number plies 1/2 thickness of center ply is specified
                                                            // the first ply is the bottom ply in the stack, default (0) all plies specified
    if (feaProperty->compositeMaterialID != NULL) EG_free(feaProperty->compositeMaterialID);
    if (feaProperty->compositeThickness != NULL) EG_free(feaProperty->compositeThickness);
    if (feaProperty->compositeOrientation != NULL) EG_free(feaProperty->compositeOrientation);

    feaProperty->compositeMaterialID = NULL;
    feaProperty->compositeThickness = NULL;
    feaProperty->compositeOrientation = NULL;

    // Solid

    // Concentrated Mass
    feaProperty->mass = 0.0; // Mass value
    feaProperty->massOffset[0]  = 0.0; // Offset distance from the grid point to the center of gravity
    feaProperty->massOffset[1]  = 0.0;
    feaProperty->massOffset[2]  = 0.0;
    feaProperty->massInertia[0] = 0.0; // Mass moment of inertia measured at the mass center of gravity
    feaProperty->massInertia[1] = 0.0;
    feaProperty->massInertia[2] = 0.0;
    feaProperty->massInertia[3] = 0.0;
    feaProperty->massInertia[4] = 0.0;
    feaProperty->massInertia[5] = 0.0;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaMaterial in the feaMaterialStruct structure format
int initiate_feaMaterialStruct(feaMaterialStruct *feaMaterial) {

    feaMaterial->name = NULL;  // Material name

    feaMaterial->materialType = UnknownMaterial; // Set

    feaMaterial->materialID = 0; // ID number of material

    feaMaterial->youngModulus = 0.0; // E - Young's Modulus
    feaMaterial->shearModulus = 0.0; // G - Shear Modulus
    feaMaterial->poissonRatio = 0.0; // Poisson's Ratio
    feaMaterial->density      = 0.0;    // Rho - material mass density
    feaMaterial->thermalExpCoeff = 0.0; //Coefficient of thermal expansion
    feaMaterial->temperatureRef  = 0.0; // Reference temperature
    feaMaterial->dampingCoeff    = 0.0; // Damping coefficient
    feaMaterial->tensionAllow    = 0.0; // Tension allowable for the material
    feaMaterial->compressAllow   = 0.0; // Compression allowable for the material
    feaMaterial->shearAllow      = 0.0; // Shear allowable for the material

    feaMaterial->youngModulusLateral = 0.0; // Young's Modulus in the lateral direction
    feaMaterial->shearModulusTrans1Z = 0.0; // Transverse shear modulus in the 1-Z plane
    feaMaterial->shearModulusTrans2Z = 0.0; // Transverse shear modulus in the 2-Z plane
    feaMaterial->tensionAllowLateral    = 0.0; // Tension allowable for the material
    feaMaterial->compressAllowLateral   = 0.0; // Compression allowable for the material
    feaMaterial->thermalExpCoeffLateral = 0.0; //Coefficient of thermal expansion
    feaMaterial->allowType = 0;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaMaterial in the feaMaterialStruct structure format
int destroy_feaMaterialStruct(feaMaterialStruct *feaMaterial) {

    if (feaMaterial->name != NULL) EG_free(feaMaterial->name);
    feaMaterial->name = NULL;  // Material name

    feaMaterial->materialType = UnknownMaterial; // Material type

    feaMaterial->materialID = 0; // ID number of material

    feaMaterial->youngModulus = 0.0; // E - Young's Modulus
    feaMaterial->shearModulus = 0.0; // G - Shear Modulus
    feaMaterial->poissonRatio = 0.0; // Poisson's Ratio
    feaMaterial->density      = 0.0;    // Rho - material mass density
    feaMaterial->thermalExpCoeff = 0.0; //Coefficient of thermal expansion
    feaMaterial->temperatureRef  = 0.0; // Reference temperature
    feaMaterial->dampingCoeff    = 0.0; // Damping coefficient
    feaMaterial->tensionAllow    = 0.0; // Tension allowable for the material
    feaMaterial->compressAllow   = 0.0; // Compression allowable for the material
    feaMaterial->shearAllow      = 0.0; // Shear allowable for the material

    feaMaterial->youngModulusLateral = 0.0; // Young's Modulus in the lateral direction
    feaMaterial->shearModulusTrans1Z = 0.0; // Transverse shear modulus in the 1-Z plane
    feaMaterial->shearModulusTrans2Z = 0.0; // Transverse shear modulus in the 2-Z plane
    feaMaterial->tensionAllowLateral    = 0.0; // Tension allowable for the material
    feaMaterial->compressAllowLateral   = 0.0; // Compression allowable for the material
    feaMaterial->thermalExpCoeffLateral = 0.0; //Coefficient of thermal expansion
    feaMaterial->allowType = 0;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaConstraint in the feaConstraintStruct structure format
int initiate_feaConstraintStruct(feaConstraintStruct *feaConstraint) {

    feaConstraint->name = NULL;  // Constraint name

    feaConstraint->constraintType = UnknownConstraint; // Constraint type

    feaConstraint->constraintID = 0; // ID number of constraint

    feaConstraint->numGridID = 0; // Component number of grid
    feaConstraint->gridIDSet = NULL; // List of component number of grids to apply constraint to

    feaConstraint->dofConstraint = 0; // Number to indicate DOF constraints
    feaConstraint->gridDisplacement = 0;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaConstraint in the feaConstraintStruct structure format
int destroy_feaConstraintStruct(feaConstraintStruct *feaConstraint) {

    if (feaConstraint->name != NULL) EG_free(feaConstraint->name);
    feaConstraint->name = NULL;  // Constraint name

    feaConstraint->constraintType = UnknownConstraint; // Constraint type

    feaConstraint->constraintID = 0; // ID number of constraint

    feaConstraint->numGridID = 0; // Number of grid IDs in grid ID set

    if (feaConstraint->gridIDSet != NULL) EG_free(feaConstraint->gridIDSet); // List of component number of grids to apply constraint to

    feaConstraint->dofConstraint = 0; // Number to indicate DOF constraints
    feaConstraint->gridDisplacement = 0; // The value for the displacement

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaSupport in the feaSupportStruct structure format
int initiate_feaSupportStruct(feaSupportStruct *feaSupport) {

    feaSupport->name = NULL;  // Support name

    feaSupport->supportID = 0; // ID number of support

    feaSupport->numGridID = 0; // Component number of grid
    feaSupport->gridIDSet = NULL; // List of component number of grids to apply support to

    feaSupport->dofSupport = 0; // Number to indicate DOF supports

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaSupport in the feaSupportStruct structure format
int destroy_feaSupportStruct(feaSupportStruct *feaSupport) {

    if (feaSupport->name != NULL) EG_free(feaSupport->name);
    feaSupport->name = NULL;  // Support name

    feaSupport->supportID = 0; // ID number of support

    feaSupport->numGridID = 0; // Number of grid IDs in grid ID set

    if (feaSupport->gridIDSet != NULL) EG_free(feaSupport->gridIDSet); // List of component number of grids to apply support to

    feaSupport->dofSupport = 0; // Number to indicate DOF supports

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaAnalysis in the feaAnalysisStruct structure format
int initiate_feaAnalysisStruct(feaAnalysisStruct *feaAnalysis) {

    feaAnalysis->name = NULL; // Analysis name

    feaAnalysis->analysisType = UnknownAnalysis; // Type of analysis

    feaAnalysis->analysisID = 0; // ID number of analysis

    // Loads for the analysis
    feaAnalysis->numLoad = 0; 	// Number of loads in the analysis
    feaAnalysis->loadSetID = NULL; // List of the load IDSs

    // Constraints for the analysis
    feaAnalysis->numConstraint = 0;   // Number of constraints in the analysis
    feaAnalysis->constraintSetID = NULL; // List of constraint IDs

    // Supports for the analysis
    feaAnalysis->numSupport = 0;   // Number of supports in the analysis
    feaAnalysis->supportSetID = NULL; // List of support IDs

    // Optimization constraints
    feaAnalysis->numDesignConstraint = 0; // Number of design constraints
    feaAnalysis->designConstraintSetID = NULL; // List of design constraint IDs

    // Eigenvalue
    feaAnalysis->extractionMethod = NULL;

    feaAnalysis->frequencyRange[0] = 0;
    feaAnalysis->frequencyRange[1] = 0;

    feaAnalysis->numEstEigenvalue = 0;
    feaAnalysis->numDesiredEigenvalue = 0;
    feaAnalysis->eigenNormaliztion = NULL;

    feaAnalysis->gridNormaliztion = 0;
    feaAnalysis->componentNormaliztion = 0;

    feaAnalysis->lanczosMode = 2; //Lanczos mode for calculating eigenvalues
    feaAnalysis->lanczosType = NULL; //Lanczos matrix type (DPB, DGB)

    // Trim
    feaAnalysis->numMachNumber = 0;
    feaAnalysis->machNumber = NULL; // Mach number
    feaAnalysis->dynamicPressure = 0.0; // Dynamic pressure
    feaAnalysis->density = 0.0; // Density
    feaAnalysis->aeroSymmetryXY = NULL;
    feaAnalysis->aeroSymmetryXZ = NULL;

    feaAnalysis->numRigidVariable = 0; // Number of rigid trim variables
    feaAnalysis->rigidVariable = NULL; // List of character labels identifying rigid trim variables, size=[numRigidVariables]

    feaAnalysis->numRigidConstraint = 0; // Number of rigid trim constrained variables
    feaAnalysis->rigidConstraint = NULL; // List of character labels identifying rigid constrained trim variables, size=[numRigidConstraint]
    feaAnalysis->magRigidConstraint = NULL; // Magnitude of rigid constrained trim variables, size=[numRigidConstraint]

    feaAnalysis->numControlConstraint = 0; // Number of control surface constrained variables
    feaAnalysis->controlConstraint = NULL; // List of character labels identifying control surfaces to be constrained trim variables, size=[numControlConstraint]
    feaAnalysis->magControlConstraint = NULL; // Magnitude of control surface constrained variables, size=[numControlConstraint]

    // Flutter
    feaAnalysis->numReducedFreq = 0;
    feaAnalysis->reducedFreq = NULL;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaAnalysis in the feaAnalysisStruct structure format
int destroy_feaAnalysisStruct(feaAnalysisStruct *feaAnalysis) {
    int status; // Function return status

    if (feaAnalysis->name != NULL) EG_free(feaAnalysis->name);
    feaAnalysis->name = NULL; // Analysis name

    feaAnalysis->analysisType = UnknownAnalysis; // Type of analysis

    feaAnalysis->analysisID = 0; // ID number of analysis

    // Loads for the analysis
    feaAnalysis->numLoad = 0; 	// Number of loads in the analysis
    if(feaAnalysis->loadSetID != NULL) EG_free(feaAnalysis->loadSetID);
    feaAnalysis->loadSetID = NULL; // List of the load IDSs

    // Constraints for the analysis
    feaAnalysis->numConstraint = 0;   // Number of constraints in the analysis
    if (feaAnalysis->constraintSetID != NULL) EG_free(feaAnalysis->constraintSetID);
    feaAnalysis->constraintSetID = NULL; // List of constraint IDs

    // Supports for the analysis
    feaAnalysis->numSupport = 0;   // Number of supports in the analysis
    if (feaAnalysis->supportSetID != NULL) EG_free(feaAnalysis->supportSetID);
    feaAnalysis->supportSetID = NULL; // List of support IDs

    // Optimization constraints
    feaAnalysis->numDesignConstraint = 0; // Number of design constraints
    if (feaAnalysis->designConstraintSetID != NULL) EG_free(feaAnalysis->designConstraintSetID);
    feaAnalysis->designConstraintSetID = NULL; // List of design constraint IDs

    // Eigenvalue
    if (feaAnalysis->extractionMethod != NULL) EG_free(feaAnalysis->extractionMethod);
    feaAnalysis->extractionMethod = NULL;

    feaAnalysis->frequencyRange[0] = 0;
    feaAnalysis->frequencyRange[1] = 0;

    feaAnalysis->numEstEigenvalue = 0;
    feaAnalysis->numDesiredEigenvalue = 0;

    if (feaAnalysis->eigenNormaliztion != NULL) EG_free(feaAnalysis->eigenNormaliztion);
    feaAnalysis->eigenNormaliztion = NULL;

    feaAnalysis->gridNormaliztion = 0;
    feaAnalysis->componentNormaliztion = 0;

    feaAnalysis->lanczosMode = 2; //Lanczos mode for calculating eigenvalues

    if (feaAnalysis->lanczosType != NULL) EG_free(feaAnalysis->lanczosType);
    feaAnalysis->lanczosType = NULL; //Lanczos matrix type (DPB, DGB)

    // Trim
    feaAnalysis->numMachNumber = 0;

    if (feaAnalysis->machNumber != NULL) EG_free(feaAnalysis->machNumber);
    feaAnalysis->machNumber = NULL; // Mach number

    //feaAnalysis->machNumber = 0.0; // Mach number
    feaAnalysis->dynamicPressure = 0.0; // Dynamic pressure
    feaAnalysis->density = 0.0; // Density
    if (feaAnalysis->aeroSymmetryXY != NULL) EG_free(feaAnalysis->aeroSymmetryXY);
    if (feaAnalysis->aeroSymmetryXZ != NULL) EG_free(feaAnalysis->aeroSymmetryXZ);

    if (feaAnalysis->rigidVariable != NULL) {
        status = string_freeArray(feaAnalysis->numRigidVariable, &feaAnalysis->rigidVariable);
        if (status != CAPS_SUCCESS) printf("Status %d during string_freeArray\n", status);
    }

    feaAnalysis->numRigidVariable = 0; // Number of trim rigid trim variables
    feaAnalysis->rigidVariable = NULL; // List of character labels identifying rigid trim variables, size=[numRigidVariables]

    if (feaAnalysis->rigidConstraint != NULL) {
        status = string_freeArray(feaAnalysis->numRigidConstraint, &feaAnalysis->rigidConstraint);
        if (status != CAPS_SUCCESS) printf("Status %d during string_freeArray\n", status);
    }

    feaAnalysis->numRigidConstraint = 0; // Number of rigid trim constrained variables
    feaAnalysis->rigidConstraint = NULL; // List of character labels identifying rigid constrained trim variables, size=[numRigidConstraint]

    if (feaAnalysis->magRigidConstraint != NULL) EG_free(feaAnalysis->magRigidConstraint);
    feaAnalysis->magRigidConstraint = NULL; // Magnitude of rigid constrained trim variables, size=[numRigidConstraint]

    if (feaAnalysis->controlConstraint != NULL) {
        status = string_freeArray(feaAnalysis->numControlConstraint, &feaAnalysis->controlConstraint);
        if (status != CAPS_SUCCESS) printf("Status %d during string_freeArray\n", status);
    }

    feaAnalysis->numControlConstraint = 0; // Number of control surface constrained variables
    feaAnalysis->controlConstraint = NULL; // List of character labels identifying control surfaces to be constrained trim variables, size=[numControlConstraint]

    if (feaAnalysis->magControlConstraint != NULL) EG_free(feaAnalysis->magControlConstraint);
    feaAnalysis->magControlConstraint = NULL; // Magnitude of control surface constrained variables, size=[numControlConstraint]

    // Flutter
    feaAnalysis->numReducedFreq = 0;
    if (feaAnalysis->reducedFreq != NULL) EG_free(feaAnalysis->reducedFreq);
    feaAnalysis->reducedFreq = NULL; // Mach number

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaLoad in the feaLoadStruct structure format
int initiate_feaLoadStruct(feaLoadStruct *feaLoad) {

    feaLoad->name = NULL;  // Load name

    feaLoad->loadType = UnknownLoad; // Load type

    feaLoad->loadID = 0; // ID number of load

    feaLoad->loadScaleFactor = 1; // Scale factor for when combining loads

    // Concentrated force at a grid point
    feaLoad->numGridID = 0; // Number of grid IDs in grid ID set
    feaLoad->gridIDSet = NULL; // List of grid IDs to apply the constraint to. size = [numGridID]
    feaLoad->coordSystemID= 0; // Component number of coordinate system in which force vector is specified

    feaLoad->forceScaleFactor= 0.0; // Overall scale factor for the force
    feaLoad->directionVector[0] = 0.0;   // [0]-x, [1]-y, [2]-z components of the force vector
    feaLoad->directionVector[1] = 0.0;
    feaLoad->directionVector[2] = 0.0;

    // Concentrated moment at a grid point (also uses coordSystemID and directionVector)
    feaLoad->momentScaleFactor= 0.0; // Overall scale factor for the moment

    // Gravitational load (also uses coordSystemID and directionVector)
    feaLoad->gravityAcceleration= 0.0; // Gravitational acceleration

    // Pressure load
    feaLoad->pressureForce= 0.0; // Pressure value
    feaLoad->pressureDistributeForce[0] = 0.0; // Pressure load at a specified grid location in the element
    feaLoad->pressureDistributeForce[1] = 0.0;
    feaLoad->pressureDistributeForce[2] = 0.0;
    feaLoad->pressureDistributeForce[3] = 0.0;

    feaLoad->pressureMultiDistributeForce = NULL; // Unique pressure load at a specified grid location for
                                                  // each element in elementIDSet size = [numElementID][4] - used in type PressureExternal
                                                  // where the pressure force is being provided by an external source (i.e. data transfer)

    feaLoad->numElementID = 0; // Number of elements IDs in element ID set
    feaLoad->elementIDSet = NULL; // List element IDs in which to apply the load. size = [numElementID]

    // Rotational velocity (also uses coordSystemID and directionVector)
    feaLoad->angularVelScaleFactor = 0.0; // Overall scale factor for the angular velocity
    feaLoad->angularAccScaleFactor = 0.0; // Overall scale factor for the angular acceleration

    // Thermal load - currently the temperature at a grid point - use gridIDSet
    feaLoad->temperature = 0.0; // Temperature value
    feaLoad->temperatureDefault = 0.0; // Default temperature of grid point explicitly not used

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaLoad in the feaLoadStruct structure format
int destroy_feaLoadStruct(feaLoadStruct *feaLoad) {

    if (feaLoad->name != NULL) EG_free(feaLoad->name);
    feaLoad->name = NULL;  // Load name

    feaLoad->loadType = UnknownLoad; // Load type

    feaLoad->loadID = 0; // ID number of load

    feaLoad->loadScaleFactor = 1; // Scale factor for when combining loads

    // Concentrated force at a grid point
    feaLoad->numGridID = 0; // Number of grid IDs in grid ID set
    if (feaLoad->gridIDSet != NULL) EG_free(feaLoad->gridIDSet); // List of grid IDs to apply the constraint to
    feaLoad->gridIDSet = NULL;

    feaLoad->coordSystemID= 0; // Component number of coordinate system in which force vector is specified
    feaLoad->forceScaleFactor= 0; // Overall scale factor for the force
    feaLoad->directionVector[0] = 0;   // [0]-x, [1]-y, [2]-z components of the force vector
    feaLoad->directionVector[1] = 0;
    feaLoad->directionVector[2] = 0;

    // Concentrated moment at a grid pofeaLoad->(also uses coordSystemID and directionVector)
    feaLoad->momentScaleFactor= 0; // Overall scale factor for the moment

    // Gravitational load (also uses coordSystemID and directionVector)
    feaLoad->gravityAcceleration= 0; // Gravitational acceleration

    // Pressure load
    feaLoad->pressureForce= 0; // Pressure value

    feaLoad->pressureDistributeForce[0] = 0; // Pressure load at a specified grid location in the element
    feaLoad->pressureDistributeForce[1] = 0;
    feaLoad->pressureDistributeForce[2] = 0;
    feaLoad->pressureDistributeForce[3] = 0;

    if (feaLoad->pressureMultiDistributeForce != NULL) EG_free(feaLoad->pressureMultiDistributeForce); // Unique pressure load at a specified grid location for
    feaLoad->pressureMultiDistributeForce = NULL; //each element in elementIDSet size = [4*numElementID]- used in type PressureExternal


    feaLoad->numElementID = 0; // Number of elements IDs in element ID set
    if (feaLoad->elementIDSet != NULL) EG_free(feaLoad->elementIDSet); // List element IDs in which to apply the load
    feaLoad->elementIDSet = NULL;

    // Rotational velocity (also uses coordSystemID and directionVector)
    feaLoad->angularVelScaleFactor = 0.0; // Overall scale factor for the angular velocity
    feaLoad->angularAccScaleFactor = 0.0; // Overall scale factor for the angular acceleration

    // Thermal load - currently the temperature at a grid point - use gridIDSet
    feaLoad->temperature = 0.0; // Temperature value
    feaLoad->temperatureDefault = 0.0; // Default temperature of grid point explicitly not used

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaDesignVariable in the feaDesignVariableStruct structure format
int initiate_feaDesignVariableStruct(feaDesignVariableStruct *feaDesignVariable) {

    feaDesignVariable->name = NULL;

    feaDesignVariable->designVariableType = UnknownDesignVar;

    feaDesignVariable->designVariableID = 0; //  ID number of design variable


    feaDesignVariable->initialValue = 0.0; // Initial value of design variable
    feaDesignVariable->lowerBound = 0.0;   // Lower bounds of variable
    feaDesignVariable->upperBound = 0.0;   // Upper bounds of variable
    feaDesignVariable->maxDelta= 0.0;     // Move limit for design variable

    feaDesignVariable->numDiscreteValue = 0; // Number of discrete values that a design variable can assume

    feaDesignVariable->discreteValue = NULL; // List of discrete values that a design variable can assume;

    feaDesignVariable->numMaterialID = 0; // Number of materials to apply the design variable to
    feaDesignVariable->materialSetID = NULL; // List of materials IDs
    feaDesignVariable->materialSetType = NULL; // List of materials types corresponding to the materialSetID

    feaDesignVariable->numPropertyID = 0;   // Number of property ID to apply the design variable to
    feaDesignVariable->propertySetID = NULL; // List of property IDs
    feaDesignVariable->propertySetType = NULL; // List of property types corresponding to the propertySetID

    feaDesignVariable->fieldPosition = 0; //  Position in card to apply design variable to
    feaDesignVariable->fieldName = NULL; // Name of property/material to apply design variable to

    feaDesignVariable->numIndependVariable = 0;  // Number of independent variables this variables depends on
    feaDesignVariable->independVariable = NULL; // List of independent variable names, size[numIndependVariable]
    feaDesignVariable->independVariableID = NULL;// List of independent variable designVariableIDs, size[numIndependVariable]
    feaDesignVariable->independVariableWeight = NULL; // List of independent variable weights, size[numIndependVariable]

    feaDesignVariable->variableWeight[0] = 0.0; // Weight to apply to if variable is dependent
    feaDesignVariable->variableWeight[1] = 0.0;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaDesignVariable in the feaDesignVariableStruct structure format
int destroy_feaDesignVariableStruct(feaDesignVariableStruct *feaDesignVariable) {

    if (feaDesignVariable->name != NULL) EG_free(feaDesignVariable->name);
    feaDesignVariable->name = NULL;


    feaDesignVariable->designVariableType = UnknownDesignVar;

    feaDesignVariable->designVariableID = 0; //  ID number of design variable

    feaDesignVariable->initialValue = 0.0; // Initial value of design variable
    feaDesignVariable->lowerBound = 0.0;   // Lower bounds of variable
    feaDesignVariable->upperBound = 0.0;   // Upper bounds of variable
    feaDesignVariable->maxDelta= 0.0;     // Move limit for design variable

    feaDesignVariable->numDiscreteValue = 0; // Number of discrete values that a design variable can assume;
    if (feaDesignVariable->discreteValue != NULL) EG_free(feaDesignVariable->discreteValue);
    feaDesignVariable->discreteValue = NULL; // List of discrete values that a design variable can assume;

    feaDesignVariable->numMaterialID = 0; // Number of materials to apply the design variable to
    if (feaDesignVariable->materialSetID != NULL) EG_free(feaDesignVariable->materialSetID);
    feaDesignVariable->materialSetID = NULL; // List of materials IDs

    if (feaDesignVariable->materialSetType != NULL) EG_free(feaDesignVariable->materialSetType);
    feaDesignVariable->materialSetType = NULL; // List of materials types corresponding to the materialSetID

    feaDesignVariable->numPropertyID = 0;   // Number of property ID to apply the design variable to
    if (feaDesignVariable->propertySetID != NULL) EG_free(feaDesignVariable->propertySetID);
    feaDesignVariable->propertySetID = NULL; // List of property IDs

    if (feaDesignVariable->propertySetType != NULL) EG_free(feaDesignVariable->propertySetType);
    feaDesignVariable->propertySetType = NULL; // List of property types corresponding to the propertySetID

    feaDesignVariable->fieldPosition = 0; //  Position in card to apply design variable to
    if (feaDesignVariable->fieldName != NULL) EG_free(feaDesignVariable->fieldName);
    feaDesignVariable->fieldName = NULL; // Name of property/material to apply design variable to

    (void) string_freeArray(feaDesignVariable->numIndependVariable, &feaDesignVariable->independVariable);
    feaDesignVariable->independVariable = NULL; // List of independent variable names, size[numIndependVariable]

    feaDesignVariable->numIndependVariable = 0;  // Number of independent variables this variables depends on

    if (feaDesignVariable->independVariableID != NULL) EG_free(feaDesignVariable->independVariableID);
    feaDesignVariable->independVariableID = NULL;// List of independent variable designVariableIDs

    if (feaDesignVariable->independVariableWeight != NULL) EG_free(feaDesignVariable->independVariableWeight);
    feaDesignVariable->independVariableWeight = NULL;  // List of independent variable weights, size[numIndependVariable]

    feaDesignVariable->variableWeight[0] = 0.0; // Weight to apply to if variable is dependent
    feaDesignVariable->variableWeight[1] = 0.0;

    return CAPS_SUCCESS;

}

// Initiate (0 out all values and NULL all pointers) of feaDesignConstraint in the feaDesignConstraintStruct structure format
int initiate_feaDesignConstraintStruct(feaDesignConstraintStruct *feaDesignConstraint) {

    feaDesignConstraint->name = NULL;

    feaDesignConstraint->designConstraintID = 0; //  ID number of design constraint

    feaDesignConstraint->responseType = NULL;  // Response type options for DRESP1 Entry

    feaDesignConstraint->lowerBound = 0.0;   // Lower bounds of design response
    feaDesignConstraint->upperBound = 0.0;   // Upper bounds of design response

    feaDesignConstraint->numPropertyID = 0;   // Number of property ID to apply the design variable to
    feaDesignConstraint->propertySetID = NULL; // List of property IDs
    feaDesignConstraint->propertySetType = NULL; // List of property types corresponding to the propertySetID

    feaDesignConstraint->fieldPosition = 0; //  Position in card to apply design variable to
    feaDesignConstraint->fieldName = NULL; // Name of property/material to apply design variable to

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaDesignConstraint in the feaDesignConstraintStruct structure format
int destroy_feaDesignConstraintStruct(feaDesignConstraintStruct *feaDesignConstraint) {

    if (feaDesignConstraint->name != NULL) EG_free(feaDesignConstraint->name);
    feaDesignConstraint->name = NULL;

    feaDesignConstraint->designConstraintID = 0; //  ID number of design constraint

    if (feaDesignConstraint->responseType != NULL) EG_free(feaDesignConstraint->responseType);
    feaDesignConstraint->responseType = NULL;  // Response type options for DRESP1 Entry

    feaDesignConstraint->lowerBound = 0.0;   // Lower bounds of design response
    feaDesignConstraint->upperBound = 0.0;   // Upper bounds of design response

    feaDesignConstraint->numPropertyID = 0;   // Number of property ID to apply the design variable to

    if (feaDesignConstraint->propertySetID != NULL) EG_free(feaDesignConstraint->propertySetID);
    feaDesignConstraint->propertySetID = NULL; // List of property IDs

    if (feaDesignConstraint->propertySetType != NULL) EG_free(feaDesignConstraint->propertySetType);
    feaDesignConstraint->propertySetType = NULL; // List of property types corresponding to the propertySetID

    feaDesignConstraint->fieldPosition = 0; //  Position in card to apply design variable to
    if (feaDesignConstraint->fieldName != NULL) EG_free(feaDesignConstraint->fieldName);
    feaDesignConstraint->fieldName = NULL; // Name of property/material to apply design variable to

    return CAPS_SUCCESS;

}


// Initiate (0 out all values and NULL all pointers) of feaCoordSystem in the feaCoordSystemStruct structure format
int initiate_feaCoordSystemStruct(feaCoordSystemStruct *feaCoordSystem) {

    int i; // Indexing

    feaCoordSystem->name = NULL; // Coordinate system name

    feaCoordSystem->coordSystemType = UnknownCoordSystem;  // Coordinate system type

    feaCoordSystem->coordSystemID = 0; // ID number of coordinate system
    feaCoordSystem->refCoordSystemID = 0; // ID of reference coordinate system

    for (i = 0; i < 3; i++) {
        feaCoordSystem->origin[i] = 0; // x, y, and z coordinates for the origin

        feaCoordSystem->normal1[i] = 0; // First normal direction
        feaCoordSystem->normal2[i] = 0; // Second normal direction
        feaCoordSystem->normal3[i] = 0; // Third normal direction - found from normal1 x normal2
    }

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaCoordSystem in the feaCoordSystemStruct structure format
int destroy_feaCoordSystemStruct(feaCoordSystemStruct *feaCoordSystem) {

    int i; // Indexing

    if (feaCoordSystem->name != NULL) EG_free(feaCoordSystem->name);
    feaCoordSystem->name = NULL; // Coordinate system name

    feaCoordSystem->coordSystemType = UnknownCoordSystem;  // Coordinate system type

    feaCoordSystem->coordSystemID = 0; // ID number of coordinate system
    feaCoordSystem->refCoordSystemID = 0; // ID of reference coordinate system

    for (i = 0; i < 3; i++) {
        feaCoordSystem->origin[i] = 0; // x, y, and z coordinates for the origin

        feaCoordSystem->normal1[i] = 0; // First normal direction
        feaCoordSystem->normal2[i] = 0; // Second normal direction
        feaCoordSystem->normal3[i] = 0; // Third normal direction - found from normal1 x normal2
    }

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaAero in the feaAeroStruct structure format
int initiate_feaAeroStruct(feaAeroStruct *feaAero) {

    int status; // Function return status

    feaAero->name = NULL; // Coordinate system name

    feaAero->surfaceID = 0; // Surface ID
    feaAero->coordSystemID = 0; // Coordinate system ID

    feaAero->numGridID = 0; // Number of grid IDs in grid ID set for the spline
    feaAero->gridIDSet = NULL; // List of grid IDs to apply spline to. size = [numGridID]

    status = initiate_vlmSurfaceStruct(&feaAero->vlmSurface);
    if (status != CAPS_SUCCESS) printf("Status %d during initiate_vlmSurfaceStruct\n", status);

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaAero in the feaAeroStruct structure format
int destroy_feaAeroStruct(feaAeroStruct *feaAero) {

    int status; // Function return status

    if (feaAero->name != NULL) EG_free(feaAero->name);
    feaAero->name = NULL; // Coordinate system name

    feaAero->surfaceID = 0; // Surface ID
    feaAero->coordSystemID = 0; // Coordinate system ID

    feaAero->numGridID = 0; // Number of grid IDs in grid ID set for the spline

    if (feaAero->gridIDSet != NULL) EG_free(feaAero->gridIDSet);
    feaAero->gridIDSet = NULL; // List of grid IDs to apply spline to. size = [numGridID]

    status = destroy_vlmSurfaceStruct(&feaAero->vlmSurface);
    if (status != CAPS_SUCCESS) printf("Status %d during destroy_vlmSurfaceStruct\n", status);

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaAeroRef in the feaAeroRefStruct structure format
int initiate_feaAeroRefStruct(feaAeroRefStruct *feaAeroRef) {

    feaAeroRef->coordSystemID = 0; // Aerodynamic coordinate sytem id
    feaAeroRef->rigidMotionCoordSystemID = 0; // Reference coordinate system identification for rigid body motions.

    feaAeroRef->refChord = 1.0; // Reference chord length.  	Reference span.  (Real > 0.0)
    feaAeroRef->refSpan = 1.0; // Reference span
    feaAeroRef->refArea = 1.0; // Reference area

    feaAeroRef->symmetryXZ  = 0; // Symmetry key for the aero coordinate x-z plane.  (Integer = +1 for symmetry, 0 for no symmetry,
                                 // and -1 for antisymmetry; Default = 0)
    feaAeroRef->symmetryXY = 0; // The symmetry key for the aero coordinate x-y plane can be used to simulate ground effects.
                                // (Integer = +1 for antisymmetry, 0 for no symmetry, and -1 for symmetry; Default = 0)
    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaAeroRef in the feaAeroRefStruct structure format
int destroy_feaAeroRefStruct(feaAeroRefStruct *feaAeroRef) {

    feaAeroRef->coordSystemID = 0; // Aerodynamic coordinate sytem id
    feaAeroRef->rigidMotionCoordSystemID = 0; // Reference coordinate system identification for rigid body motions.

    feaAeroRef->refChord = 0; // Reference chord length.  	Reference span.  (Real > 0.0)
    feaAeroRef->refSpan = 0; // Reference span
    feaAeroRef->refArea = 0; // Reference area

    feaAeroRef->symmetryXZ  = 0; // Symmetry key for the aero coordinate x-z plane.  (Integer = +1 for symmetry, 0 for no symmetry,
                                 // and -1 for antisymmetry; Default = 0)
    feaAeroRef->symmetryXY = 0; // The symmetry key for the aero coordinate x-y plane can be used to simulate ground effects.
                                // (Integer = +1 for antisymmetry, 0 for no symmetry, and -1 for symmetry; Default = 0)
    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaConnect in the feaConnectionStruct structure format
int initiate_feaConnectionStruct(feaConnectionStruct *feaConnect) {

    feaConnect->name = NULL; // Connection name

    feaConnect->connectionID =0; // Connection ID

    feaConnect->connectionType = UnknownConnection; // Connection type

    feaConnect->elementID = 0;

    // RBE2 - dependent degrees of freedom
    feaConnect->connectivity[0] = 0; // Grid IDs - 0 index = Independent grid ID, 1 index = Dependent grid ID
    feaConnect->connectivity[1] = 0;
    feaConnect->dofDependent = 0;

    // Spring
    feaConnect->stiffnessConst  = 0.0;
    feaConnect->componentNumberStart = 0;
    feaConnect->componentNumberEnd = 0;
    feaConnect->dampingConst  = 0.0;
    feaConnect->stressCoeff = 0.0;

    // Damper - see spring for additional entries

    // Mass (scalar) - see spring for additional entries
    feaConnect->mass = 0.0;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaConnect in the feaConnectionStruct structure format
int destroy_feaConnectionStruct(feaConnectionStruct *feaConnect) {

    if (feaConnect->name != NULL) EG_free(feaConnect->name);
    feaConnect->name = NULL; // Connection name

    feaConnect->connectionID =0; // Connection ID

    feaConnect->connectionType = UnknownConnection; // Connection type

    feaConnect->elementID = 0;

    // RBE2 - dependent degrees of freedom
    feaConnect->connectivity[0] = 0; // Grid IDs - 0 index = Independent grid ID, 1 index = Dependent grid ID
    feaConnect->connectivity[1] = 0;
    feaConnect->dofDependent = 0;

    // Spring
    feaConnect->stiffnessConst = 0.0;
    feaConnect->componentNumberStart = 0;
    feaConnect->componentNumberEnd = 0;
    feaConnect->dampingConst  = 0.0;
    feaConnect->stressCoeff = 0.0;

    // Damper - see spring for additional entries

    // Mass (scalar) - see spring for additional entries
    feaConnect->mass = 0.0;

    return CAPS_SUCCESS;
}

// Get the material properties from a capsTuple
int fea_getMaterial(int numMaterialTuple,
                    capsTuple materialTuple[],
                    int *numMaterial,
                    feaMaterialStruct *feaMaterial[]) {

    /*! \page feaMaterial FEA Material
     * Structure for the material tuple  = ("Material Name", "Value").
     * "Material Name" defines the reference name for the material being specified.
     *	The "Value" can either be a JSON String dictionary (see Section \ref jsonStringMaterial) or a single string keyword
     *	(see Section \ref keyStringMaterial).
     */

    int status; //Function return

    int i; // Indexing

    char *keyValue = NULL;
    char *keyWord = NULL;

    // Destroy our material structures coming in if aren't 0 and NULL already
    if (*feaMaterial != NULL) {
        for (i = 0; i < *numMaterial; i++) {
            status = destroy_feaMaterialStruct(&(*feaMaterial)[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (*feaMaterial != NULL) EG_free(*feaMaterial);
    *feaMaterial = NULL;
    *numMaterial = 0;

    printf("\nGetting FEA materials.......\n");

    *numMaterial = numMaterialTuple;
    printf("\tNumber of materials - %d\n", *numMaterial);

    if (*numMaterial > 0) {
        *feaMaterial = (feaMaterialStruct *) EG_alloc(*numMaterial * sizeof(feaMaterialStruct));
        if (*feaMaterial == NULL) return EGADS_MALLOC;

    } else {
        printf("\tNumber of material values in input tuple is 0\n");
        return CAPS_NOTFOUND;
    }

    for (i = 0; i < *numMaterial; i++) {
        status = initiate_feaMaterialStruct(&(*feaMaterial)[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    for (i = 0; i < *numMaterial; i++) {

        printf("\tMaterial name - %s\n", materialTuple[i].name);

        (*feaMaterial)[i].name = (char *) EG_alloc(((strlen(materialTuple[i].name)) + 1)*sizeof(char));
        if ((*feaMaterial)[i].name == NULL) return EGADS_MALLOC;

        memcpy((*feaMaterial)[i].name, materialTuple[i].name, strlen(materialTuple[i].name)*sizeof(char));
        (*feaMaterial)[i].name[strlen(materialTuple[i].name)] = '\0';

        (*feaMaterial)[i].materialID = i + 1;

        // Do we have a json string?
        if (strncmp(materialTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n", materialTuple[i].value);


            /*! \page feaMaterial
             * \section jsonStringMaterial JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if (MYSTRAN || NASTRAN)
             *  (e.g. "Value" = {"density": 7850, "youngModulus": 120000.0, "poissonRatio": 0.5, "materialType": "isotropic"})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             * <ul>
             * <li> <B>materialType = "Isotropic"</B> </li> <br>
             *      Material property type. Options: Isotropic, Anisothotropic, Orthotropic, or Anisotropic.
             * </ul>
             */

            // Get material Type
            keyWord = "materialType";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{UnknownMaterial, Isotropic, Anisothotropic, Orthotropic, Anisotropic}
                if      (strcasecmp(keyValue, "\"Isotropic\"")      == 0) (*feaMaterial)[i].materialType = Isotropic;
                else if (strcasecmp(keyValue, "\"Anisothotropic\"") == 0) (*feaMaterial)[i].materialType = Anisothotropic;
                else if (strcasecmp(keyValue, "\"Orthotropic\"")    == 0) (*feaMaterial)[i].materialType = Orthotropic;
                else if (strcasecmp(keyValue, "\"Anisotropic\"")    == 0) (*feaMaterial)[i].materialType = Anisotropic;
                else {

                    printf("\tUnrecognized \"%s\" specified (%s) for Material tuple %s, defaulting to \"Isotropic\"\n", keyWord,
                            keyValue,
                            materialTuple[i].name);
                    (*feaMaterial)[i].materialType = Isotropic;
                }

            } else {

                printf("\tNo \"%s\" specified for Material tuple %s, defaulting to \"Isotropic\"\n", keyWord,
                        materialTuple[i].name);
                (*feaMaterial)[i].materialType = Isotropic;
            }

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            //Fill up material properties

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *	<li> <B>youngModulus = 0.0</B> </li> <br>
             *  Also known as the elastic modulus, defines the relationship between stress and strain.
             *  Default if `shearModulus' and `poissonRatio' != 0, youngModulus = 2*(1+poissonRatio)*shearModulus
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>youngModulus = 0.0</B> </li> <br>
             *  Also known as the elastic modulus, defines the relationship between stress and strain.
             *  Default if `shearModulus' and `poissonRatio' != 0, youngModulus = 2*(1+poissonRatio)*shearModulus
             *  </ul>
             * \endif
             *
             */

            keyWord = "youngModulus";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].youngModulus);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>shearModulus = 0.0</B> </li> <br>
             *  Also known as the modulus of rigidity, is defined as the ratio of shear stress to the shear strain.
             *  Default if `youngModulus' and `poissonRatio' != 0, shearModulus = youngModulus/(2*(1+poissonRatio))
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>shearModulus = 0.0</B> </li> <br>
             *  Also known as the modulus of rigidity is defined as the ratio of shear stress to the shear strain.
             *  Default if `youngModulus' and `poissonRatio' != 0, shearModulus = youngModulus/(2*(1+poissonRatio))
             *  </ul>
             * \endif
             */

            keyWord = "shearModulus";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].shearModulus);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>poissonRatio = 0.0</B> </li> <br>
             *  The fraction of expansion divided by the fraction of compression.
             *  Default if `youngModulus' and `shearModulus' != 0, poissonRatio = (2*youngModulus/shearModulus) - 1
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>poissonRatio = 0.0</B> </li> <br>
             *  The fraction of expansion divided by the fraction of compression.
             *  Default if `youngModulus' and `shearModulus' != 0, poissonRatio = (2*youngModulus/shearModulus) - 1
             *  </ul>
             * \endif
             */

            keyWord = "poissonRatio";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].poissonRatio);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            // Check Young's modulus, shear modulus, and Poisson's ratio
            if ( ((*feaMaterial)[i].youngModulus == 0 && (*feaMaterial)[i].poissonRatio == 0) ||
                 ((*feaMaterial)[i].shearModulus == 0 && (*feaMaterial)[i].poissonRatio == 0) ||
                 ((*feaMaterial)[i].youngModulus == 0 && (*feaMaterial)[i].shearModulus == 0) ) {
                // Do nothing

            } else if ((*feaMaterial)[i].youngModulus == 0) {
                (*feaMaterial)[i].youngModulus = 2*(1+(*feaMaterial)[i].poissonRatio)*(*feaMaterial)[i].shearModulus;
            } else if ((*feaMaterial)[i].shearModulus == 0) {
                (*feaMaterial)[i].shearModulus = (*feaMaterial)[i].youngModulus/(2*(1+(*feaMaterial)[i].poissonRatio));
            } else if ((*feaMaterial)[i].poissonRatio == 0) {
                (*feaMaterial)[i].poissonRatio = (*feaMaterial)[i].youngModulus/(2*(*feaMaterial)[i].shearModulus) -1;
            }

            /*! \page feaMaterial
             *
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>density = 0.0</B> </li> <br>
             *  Density of the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>density = 0.0</B> </li> <br>
             *  Density of the material.
             *  </ul>
             * \endif
             */
            keyWord = "density";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].density);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>thermalExpCoeff = 0.0</B> </li> <br>
             *  Thermal expansion coefficient of the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>thermalExpCoeff = 0.0</B> </li> <br>
             *  Thermal expansion coefficient of the material.
             *  </ul>
             * \endif
             */
            keyWord = "thermalExpCoeff";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].thermalExpCoeff);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>thermalExpCoeffLateral = 0.0</B> </li> <br>
             *  Thermal expansion coefficient of the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>thermalExpCoeffLateral = 0.0</B> </li> <br>
             *  Thermal expansion coefficient of the material.
             *  </ul>
             * \endif
             */
            keyWord = "thermalExpCoeffLateral";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].thermalExpCoeffLateral);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>temperatureRef = 0.0</B> </li> <br>
             *  Reference temperature for material properties.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>temperatureRef = 0.0</B> </li> <br>
             *  Reference temperature for material properties.
             *  </ul>
             * \endif
             */
            keyWord = "temperatureRef";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].temperatureRef);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>dampingCoeff = 0.0</B> </li> <br>
             *  Damping coefficient for the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>dampingCoeff = 0.0</B> </li> <br>
             *  Damping coefficient for the material.
             * </ul>
             * \endif
             */
            keyWord = "dampingCoeff";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].dampingCoeff);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             *
             * \if MYSTRAN
             * \elseif NASTRAN
             *  <ul>
             *  <li> <B>yieldAllow = 0.0</B> </li> <br>
             *  Yield strength/allowable for the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>yieldAllow = 0.0</B> </li> <br>
             *  Yield strength/allowable for the material.
             *  </ul>
             * \endif
             */
            keyWord = "yieldAllow";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].yieldAllow);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             *
             * \if MYSTRAN
             * \elseif NASTRAN
             *  <ul>
             *  <li> <B>tensionAllow = 0.0</B> </li> <br>
             *  Tension allowable for the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>tensionAllow = 0.0</B> </li> <br>
             *  Tension allowable for the material.
             *  </ul>
             * \endif
             */
            keyWord = "tensionAllow";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].tensionAllow);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             *
             * \if MYSTRAN
             * \elseif NASTRAN
             *  <ul>
             *  <li> <B>tensionAllowLateral = 0.0</B> </li> <br>
             *  Lateral tension allowable for the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>tensionAllowLateral = 0.0</B> </li> <br>
             *  Lateral tension allowable for the material.
             *  </ul>
             * \endif
             */
            keyWord = "tensionAllowLateral";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].tensionAllowLateral);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if MYSTRAN
             * \elseif NASTRAN
             *  <ul>
             *  <li> <B>compressAllow = 0.0</B> </li> <br>
             *  Compression allowable for the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>compressAllow = 0.0</B> </li> <br>
             *  Compression allowable for the material.
             *  </ul>
             * \endif
             */
            keyWord = "compressAllow";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].compressAllow);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if MYSTRAN
             * \elseif NASTRAN
             *  <ul>
             *  <li> <B>compressAllowLateral = 0.0</B> </li> <br>
             *  Lateral compression allowable for the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>compressAllowLateral = 0.0</B> </li> <br>
             *  Compression allowable for the material.
             *  </ul>
             * \endif
             */
            keyWord = "compressAllowLateral";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].compressAllowLateral);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if MYSTRAN
             * \elseif NASTRAN
             *  <ul>
             *  <li> <B>shearAllow = 0.0</B> </li> <br>
             *  Shear allowable for the material.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>shearAllow = 0.0</B> </li> <br>
             *  Shear allowable for the material.
             *  </ul>
             * \endif
             */
            keyWord = "shearAllow";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].shearAllow);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>allowType = 0 </B> </li> <br>
             *  This flag defines if the above allowables <c>compressAllow</c> etc. are defined in terms of stress (0) or strain (1).  The default is stress (0).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>allowType = 0 </B> </li> <br>
             *  This flag defines if the above allowables such as <c>compressAllow</c> etc. are defined in terms of stress (0) or strain (1).  The default is stress (0).
             * </ul>
             * \endif
             */
            keyWord = "allowType";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toInteger(keyValue, &(*feaMaterial)[i].allowType);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>youngModulusLateral = 0.0</B> </li> <br>
             *  Elastic modulus in lateral direction for an orthotropic material
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>youngModulusLateral = 0.0</B> </li> <br>
             *  Elastic modulus in lateral direction for an orthotropic material
             * </ul>
             * \endif
             */
            keyWord = "youngModulusLateral";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].youngModulusLateral);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>shearModulusTrans1Z = 0.0</B> </li> <br>
             *  Transverse shear modulus in the 1-Z plane for an orthotropic material
             * </ul>
             * \elseif ABAQUS
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>shearModulusTrans1Z = 0.0</B> </li> <br>
             *  Transverse shear modulus in the 1-Z plane for an orthotropic material
             * </ul>
             * \endif
             */
            keyWord = "shearModulusTrans1Z";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].shearModulusTrans1Z);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaMaterial
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>shearModulusTrans2Z = 0.0</B> </li> <br>
             *  Transverse shear modulus in the 2-Z plane for an orthotropic material
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>shearModulusTrans2Z = 0.0</B> </li> <br>
             *  Transverse shear modulus in the 2-Z plane for an orthotropic material
             *  </ul>
             * \endif
             */
            keyWord = "shearModulusTrans2Z";
            status = search_jsonDictionary( materialTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*feaMaterial)[i].shearModulusTrans2Z);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

        } else {

            /*! \page feaMaterial
             * \section keyStringMaterial Single Value String
             *
             * If "Value" is a string, the string value may correspond to an entry in a predefined material lookup
             * table. NOT YET IMPLEMENTED!!!!
             *
             */
            // CALL material look up
            printf("\tError: Material tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;
        }
    }

    if (keyValue != NULL) {
        EG_free(keyValue);
        keyValue = NULL;
    }

    printf("\tDone getting FEA materials\n");
    return CAPS_SUCCESS;
}

// Get the property properties from a capsTuple
int fea_getProperty(int numPropertyTuple,
                    capsTuple propertyTuple[],
                    mapAttrToIndexStruct *attrMap,
                    feaProblemStruct *feaProblem) {

    /*! \page feaProperty FEA Property
     * Structure for the property tuple  = ("Property Name", "Value").
     * "Property Name" defines the reference <c>capsGroup</c> for the property being specified.
     *	The "Value" can either be a JSON String dictionary (see Section \ref jsonStringProperty) or a single string keyword
     *	(see Section \ref keyStringProperty).
     */

    int status; //Function return

    int i, j,  matIndex; // Indexing
    int found; // Bool test function

    int pidIndex; // Property identification index found in attribute map

    int tempInteger;

    char *keyValue = NULL;
    char *keyWord = NULL;
    char *tempString = NULL;
    char **tempStringArray = NULL;

    // Destroy our property structures coming in if aren't 0 and NULL already
    if (feaProblem->feaProperty != NULL) {
        for (i = 0; i < feaProblem->numProperty; i++) {
            status = destroy_feaPropertyStruct(&feaProblem->feaProperty[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (feaProblem->feaProperty != NULL) EG_free(feaProblem->feaProperty);
    feaProblem->feaProperty = NULL;
    feaProblem->numProperty = 0;

    printf("\nGetting FEA properties.......\n");

    feaProblem->numProperty = numPropertyTuple;
    printf("\tNumber of properties - %d\n", feaProblem->numProperty);

    if (feaProblem->numProperty > 0) {

        feaProblem->feaProperty = (feaPropertyStruct *) EG_alloc(feaProblem->numProperty * sizeof(feaPropertyStruct));
        if (feaProblem->feaProperty == NULL) return EGADS_MALLOC;

    } else {
        printf("\tNumber of property values in input tuple is 0\n");
        return CAPS_NOTFOUND;
    }

    for (i = 0; i < feaProblem->numProperty; i++) {
        status = initiate_feaPropertyStruct(&feaProblem->feaProperty[i]);
        if (status != CAPS_SUCCESS) {
            printf("\tUnable to initiate feaProperty structure (number = %d\n",i);
            return status;
        }
    }

    for (i = 0; i < feaProblem->numProperty; i++) {

        printf("\tProperty name - %s\n", propertyTuple[i].name);

        // Set property name from tuple name
        feaProblem->feaProperty[i].name = EG_strdup(propertyTuple[i].name);
        if (feaProblem->feaProperty[i].name == NULL) return EGADS_MALLOC;

        // Get to property ID number from the attribute map
        status = get_mapAttrToIndexIndex(attrMap, feaProblem->feaProperty[i].name, &pidIndex);

        if (status != CAPS_SUCCESS) {

            printf("\tTuple name %s not found in attribute map of PIDS!!!!\n", feaProblem->feaProperty[i].name);

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            return status;

        } else {

            feaProblem->feaProperty[i].propertyID = pidIndex;
        }

        // Do we have a json string?
        if (strncmp(propertyTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n",propertyTuple[i].value);

            /*! \page feaProperty
             * \section jsonStringProperty JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if (MYSTRAN || NASTRAN)
             *  (e.g. "Value" = {"shearMembraneRatio": 0.83, "bendingInertiaRatio": 1.0, "membraneThickness": 0.2, "propertyType": "Shell"})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>propertyType = No Default value</B> </li> <br>
             *  Type of property to apply to a given capsGroup <c>Name</c>. Options: ConcentratedMass, Rod,
             *  Bar, Shear, Shell, Composite, and Solid
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>propertyType = No Default value</B> </li> <br>
             *  Type of property to apply to a give capsGroup <c>Name</c>. Options: ConcentratedMass, Rod,
             *  Bar, Shear, Shell, Composite, and Solid
             * </ul>
             * \endif
             *
             */

            // Get property Type
            keyWord = "propertyType";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{UnknownProperty, ConcentratedMass Rod, Bar, Shear, Shell, Composite, Solid}
                if (strcasecmp(keyValue, "\"ConcentratedMass\"") == 0) feaProblem->feaProperty[i].propertyType = ConcentratedMass;
                else if (strcasecmp(keyValue, "\"Rod\"")      == 0) feaProblem->feaProperty[i].propertyType = Rod;
                else if (strcasecmp(keyValue, "\"Bar\"")      == 0) feaProblem->feaProperty[i].propertyType = Bar;
                else if (strcasecmp(keyValue, "\"Shear\"")    == 0) feaProblem->feaProperty[i].propertyType = Shear;
                else if (strcasecmp(keyValue, "\"Shell\"")    == 0) feaProblem->feaProperty[i].propertyType = Shell;
                else if (strcasecmp(keyValue, "\"Composite\"")== 0) feaProblem->feaProperty[i].propertyType = Composite;
                else if (strcasecmp(keyValue, "\"Solid\"")    == 0) feaProblem->feaProperty[i].propertyType = Solid;
                else {

                    printf("\tUnrecognized \"%s\" specified (%s) for Property tuple %s, current options are "
                            "\"Rod, Bar, Shear, Shell, Composite, and Solid\"\n", keyWord,
                            keyValue,
                            propertyTuple[i].name);
                    if (keyValue != NULL) {
                        EG_free(keyValue);
                        keyValue = NULL;
                    }

                    return CAPS_NOTFOUND;
                }

            } else {

                printf("\tNo \"%s\" specified for Property tuple %s, this mandatory! Current options are "
                        "\"ConcentratedMass, Rod, Bar, Shear, Shell, Composite, and Solid\"\n", keyWord,
                        propertyTuple[i].name);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                return CAPS_NOTFOUND;
            }

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>material = "Material Name" (\ref feaMaterial) </B> </li> <br>
             *  "Material Name" from \ref feaMaterial to use for property. If no material is set the first material
             *   created will be used
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>material = `Material Name' (\ref feaMaterial) </B> </li> <br>
             *  `Material Name' from \ref feaMaterial to use for property. If no material is set the first material
             *   created will be used
             * </ul>
             * \endif
             */
            keyWord = "material";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                found = (int) false;
                for (matIndex = 0; matIndex < feaProblem->numMaterial; matIndex++ ) {

                    if (tempString != NULL) EG_free(tempString);
                    tempString = string_removeQuotation(keyValue);

                    if (strcasecmp(feaProblem->feaMaterial[matIndex].name, tempString) == 0) {
                        feaProblem->feaProperty[i].materialID = feaProblem->feaMaterial[matIndex].materialID;

                        feaProblem->feaProperty[i].materialName = (char *) EG_alloc((strlen(feaProblem->feaMaterial[matIndex].name)+1) *sizeof(char));
                        if (feaProblem->feaProperty[i].name == NULL) return EGADS_MALLOC;

                        memcpy(feaProblem->feaProperty[i].materialName,
                               feaProblem->feaMaterial[matIndex].name,
                               strlen(feaProblem->feaMaterial[matIndex].name)*sizeof(char));

                        feaProblem->feaProperty[i].materialName[strlen(feaProblem->feaMaterial[matIndex].name)] = '\0';

                        found = (int) true;
                        break;
                    }
                }

                if (tempString != NULL) EG_free(tempString);
                tempString = NULL;

                if (found == (int) false) {
                    printf("\tUnrecognized \"%s\" specified (%s) for Property tuple %s. No match in Material tuple\n", keyWord,
                                                                                                                       keyValue,
                                                                                                                       propertyTuple[i].name);
                    if (keyValue != NULL) {
                        EG_free(keyValue);
                        keyValue = NULL;
                    }
                    return CAPS_NOTFOUND;
                }

            } else {

                if (feaProblem->feaProperty[i].propertyType != ConcentratedMass &&
                    feaProblem->feaProperty[i].propertyType != Composite) {
                    printf("\tNo \"%s\" specified for Property tuple %s, defaulting to an index of 1\n", keyWord,
                            propertyTuple[i].name);
                }

                feaProblem->feaProperty[i].materialID = 1;
                if (feaProblem->numMaterial > 0) {

                    feaProblem->feaProperty[i].materialName = (char *) EG_alloc((strlen(feaProblem->feaMaterial[0].name)+1) *sizeof(char));
                    if (feaProblem->feaProperty[i].name == NULL) {
                        if (keyValue != NULL) EG_free(keyValue);
                        return EGADS_MALLOC;
                    }

                    memcpy(feaProblem->feaProperty[i].materialName,
                           feaProblem->feaMaterial[0].name,
                           strlen(feaProblem->feaMaterial[0].name)*sizeof(char));

                    feaProblem->feaProperty[i].materialName[strlen(feaProblem->feaMaterial[0].name)] = '\0';
                }
            }

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            //  Fill up properties ///

            // Rods
            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>crossSecArea = 0.0</B> </li> <br>
             *  Cross sectional area.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>crossSecArea = 0.0</B> </li> <br>
             *  Cross sectional area.
             *  </ul>
             * \endif
             */
            keyWord = "crossSecArea";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].crossSecArea);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>torsionalConst = 0.0</B> </li> <br>
             *  Torsional constant.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>torsionalConst = 0.0</B> </li> <br>
             *  Torsional constant.
             * </ul>
             * \endif
             */
            keyWord = "torsionalConst";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].torsionalConst);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>torsionalStressReCoeff = 0.0</B> </li> <br>
             *  Torsional stress recovery coefficient.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>torsionalStressReCoeff = 0.0</B> </li> <br>
             *  Torsional stress recovery coefficient.
             * </ul>
             * \endif
             */
            keyWord = "torsionalStressReCoeff";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].torsionalStressReCoeff);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>massPerLength = 0.0</B> </li> <br>
             *  Mass per unit length.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             *  <ul>
             *  <li> <B>massPerLength = 0.0</B> </li> <br>
             *  Mass per unit length.
             * </ul>
             * \endif
             */
            keyWord = "massPerLength";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].massPerLength);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            // Bar - see rod for additional variables

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>zAxisInertia = 0.0</B> </li> <br>
             *  Section moment of inertia about the element z-axis.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>zAxisInertia = 0.0</B> </li> <br>
             *  Section moment of inertia about the element z-axis.
             * </ul>
             * \endif
             */
            keyWord = "zAxisInertia";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].zAxisInertia);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             *  <ul>
             *  <li> <B>yAxisInertia = 0.0</B> </li> <br>
             *  Section moment of inertia about the element y-axis.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>yAxisInertia = 0.0</B> </li> <br>
             *  Section moment of inertia about the element y-axis.
             * </ul>
             * \endif
             */
            keyWord = "yAxisInertia";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].yAxisInertia);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>yCoords[4] = [0.0, 0.0, 0.0, 0.0]</B> </li> <br>
             *  Element y-coordinates, in the bar cross-section, of four points at which to recover stresses
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>yCoords[4] = [0.0, 0.0, 0.0, 0.0]</B> </li> <br>
             *  Element y-coordinates, in the bar cross-section, of four points at which to recover stresses
             * </ul>
             * \endif
             */
            keyWord = "yCoords";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDoubleArray(keyValue,
                        (int) sizeof(feaProblem->feaProperty[i].yCoords)/sizeof(double),
                        feaProblem->feaProperty[i].yCoords);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>zCoords[4] = [0.0, 0.0, 0.0, 0.0]</B> </li> <br>
             *  Element z-coordinates, in the bar cross-section, of four points at which to recover stresses
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>zCoords[4] = [0.0, 0.0, 0.0, 0.0]</B> </li> <br>
             *  Element z-coordinates, in the bar cross-section, of four points at which to recover stresses
             * </ul>
             * \endif
             */
            keyWord = "zCoords";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDoubleArray(keyValue,
                        (int) sizeof(feaProblem->feaProperty[i].zCoords)/sizeof(double),
                        feaProblem->feaProperty[i].zCoords);

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>areaShearFactors[2] = [0.0, 0.0]</B> </li> <br>
             *  Area factors for shear.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>areaShearFactors[2] = [0.0, 0.0]</B> </li> <br>
             *  Area factors for shear.
             * </ul>
             * \endif
             */
            keyWord = "areaShearFactors";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDoubleArray(keyValue,
                        (int) sizeof(feaProblem->feaProperty[i].areaShearFactors)/sizeof(double),
                        feaProblem->feaProperty[i].areaShearFactors);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>crossProductInertia = 0.0</B> </li> <br>
             *  Section cross-product of inertia.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>crossProductInertia = 0.0</B> </li> <br>
             *  Section cross-product of inertia.
             * </ul>
             * \endif
             */
            keyWord = "crossProductInertia";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].crossProductInertia);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }


            // Shear

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>shearPanelThickness = 0.0</B> </li> <br>
             *  Shear panel thickness.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>shearPanelThickness = 0.0</B> </li> <br>
             *  Shear panel thickness.
             * </ul>
             * \endif
             */
            keyWord = "shearPanelThickness";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].shearPanelThickness);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>nonStructMassPerArea = 0.0</B> </li> <br>
             *  Nonstructural mass per unit area.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>nonStructMassPerArea = 0.0</B> </li> <br>
             *  Nonstructural mass per unit area.
             * </ul>
             * \endif
             */
            keyWord = "nonStructMassPerArea";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].nonStructMassPerArea);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            // Shell

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>membraneThickness = 0.0</B> </li> <br>
             *  Membrane thickness.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>membraneThickness = 0.0</B> </li> <br>
             *  Membrane thickness.
             * </ul>
             * \endif
             */
            keyWord = "membraneThickness";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].membraneThickness);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }


            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>bendingInertiaRatio = 1.0</B> </li> <br>
             *  Ratio of actual bending moment inertia to the bending inertia of a solid plate of thickness "membraneThickness"
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>bendingInertiaRatio = 1.0</B> </li> <br>
             *  Ratio of actual bending moment inertia to the bending inertia of a solid plate of thickness "membraneThickness"
             * </ul>
             * \endif
             */
            keyWord = "bendingInertiaRatio";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].bendingInertiaRatio);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }


            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>shearMembraneRatio = 5.0/6.0</B> </li> <br>
             *  Ratio shear thickness to membrane thickness.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>shearMembraneRatio = 5.0/6.0</B> </li> <br>
             *  Ratio shear thickness to membrane thickness.
             * </ul>
             * \endif
             */
            keyWord = "shearMembraneRatio";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].shearMembraneRatio);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>materialBending = "Material Name" (\ref feaMaterial)</B> </li> <br>
             *  "Material Name" from \ref feaMaterial to use for property bending. If no material is given and
             *  "bendingInertiaRatio" is greater than 0, the material name provided in "material" is used.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>materialBending = "Material Name" (\ref feaMaterial)</B> </li> <br>
             *  "Material Name" from \ref feaMaterial to use for property bending.
             * </ul>
             * \endif
             */
            keyWord = "materialBending"; // Shell specific materials
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                found = (int) false;
                for (matIndex = 0; matIndex < feaProblem->numMaterial; matIndex++ ) {

                    if (tempString != NULL) EG_free(tempString);
                    tempString = string_removeQuotation(keyValue);

                    if (strcasecmp(feaProblem->feaMaterial[matIndex].name, tempString) == 0) {
                        feaProblem->feaProperty[i].materialBendingID = feaProblem->feaMaterial[matIndex].materialID;
                        found = (int) true;

                        if (keyValue != NULL) {
                            EG_free(keyValue);
                            keyValue = NULL;
                        }
                        break;
                    }

                }

                if (tempString != NULL) EG_free(tempString);
                tempString = NULL;

                if (found == (int) false) {
                    printf("\tUnrecognized \"%s\" specified (%s) for Property tuple %s. No match in Material tuple\n", keyWord,
                            keyValue,
                            propertyTuple[i].name);
                    if (keyValue != NULL) {
                        EG_free(keyValue);
                        keyValue = NULL;
                    }
                    return CAPS_NOTFOUND;
                }

            } else { // Don't default to anything - yet

                /*
                printf("No \"%s\" specified for Property tuple %s which is a shell element, "
                       " no bending material will be specified\n", keyWord,
                                                                   propertyTuple[i].name);

                feaProblem->feaProperty[i].materialBendingID = 0;
                 */

                if (feaProblem->feaProperty[i].bendingInertiaRatio > 0) {

                    feaProblem->feaProperty[i].materialBendingID = feaProblem->feaProperty[i].materialID;
                }
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>materialShear = "Material Name" (\ref feaMaterial)</B> </li> <br>
             *  "Material Name" from \ref feaMaterial to use for property shear. If no material is given and
             *  "shearMembraneRatio" is greater than 0, the material name provided in "material" is used.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>materialShear = "Material Name" (\ref feaMaterial)</B> </li> <br>
             *  "Material Name" from \ref feaMaterial to use for property shear.
             * </ul>
             * \endif
             */
            keyWord = "materialShear"; // Shell specific materials
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                found = (int) false;
                for (matIndex = 0; matIndex < feaProblem->numMaterial; matIndex++ ) {

                    if (tempString != NULL) EG_free(tempString);
                    tempString = string_removeQuotation(keyValue);

                    if (strcasecmp(feaProblem->feaMaterial[matIndex].name, tempString) == 0) {
                        feaProblem->feaProperty[i].materialShearID = feaProblem->feaMaterial[matIndex].materialID;
                        found = (int) true;

                        if (keyValue != NULL) {
                            EG_free(keyValue);
                            keyValue = NULL;
                        }
                        break;
                    }

                }

                if (tempString != NULL) EG_free(tempString);
                tempString = NULL;

                if (found == (int) false) {
                    printf("\tUnrecognized \"%s\" specified (%s) for Property tuple %s. No match in Material tuple\n", keyWord,
                            keyValue,
                            propertyTuple[i].name);
                    if (keyValue != NULL) {
                        EG_free(keyValue);
                        keyValue = NULL;
                    }
                    return CAPS_NOTFOUND;
                }

            } else {
                /*
                printf("No \"%s\" specified for Property tuple %s which is a shell element, "
                       " no shear material will be specified\n", keyWord,
						    	        						 propertyTuple[i].name);

                feaProblem->feaProperty[i].materialShearID = 0;
                 */

                if (feaProblem->feaProperty[i].shearMembraneRatio > 0) {
                    feaProblem->feaProperty[i].materialShearID = feaProblem->feaProperty[i].materialID;
                }
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>massPerArea = 0.0</B> </li> <br>
             *  Mass per unit area.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>massPerArea = 0.0</B> </li> <br>
             *  Mass per unit area.
             * </ul>
             * \endif
             */
            keyWord = "massPerArea";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].massPerArea);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            // Composite

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>compositeMaterial = "no default" </B> </li> <br>
             *  List of "Material Name"s, ["Material Name -1", "Material Name -2", ...], from \ref feaMaterial to use for composites.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>compositeMaterial = "no default" </B> </li> <br>
             *  List of "Material Name"s, ["Material Name -1", "Material Name -2", ...], from \ref feaMaterial to use for composites.
             * </ul>
             * \endif
             */
            keyWord = "compositeMaterial";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &feaProblem->feaProperty[i].numPly, &tempStringArray);
                if (status != CAPS_SUCCESS) return status;

                feaProblem->feaProperty[i].compositeMaterialID = (int *) EG_alloc(feaProblem->feaProperty[i].numPly*sizeof(int));
                if (feaProblem->feaProperty[i].compositeMaterialID == NULL) {
                    (void) string_freeArray(feaProblem->feaProperty[i].numPly, &tempStringArray);
                    if (keyValue != NULL) EG_free(keyValue);
                    return EGADS_MALLOC;
                }

                for (j = 0; j < feaProblem->feaProperty[i].numPly; j++) {
                    found = (int) false;
                    for (matIndex = 0; matIndex < feaProblem->numMaterial; matIndex++ ) {

                        if (tempString != NULL) EG_free(tempString);
                        tempString = string_removeQuotation(tempStringArray[j]);

                        if (strcasecmp(feaProblem->feaMaterial[matIndex].name, tempString) == 0) {
                            feaProblem->feaProperty[i].compositeMaterialID[j] = feaProblem->feaMaterial[matIndex].materialID;
                            found = (int) true;
                            break;
                        }

                    }

                    if (found == (int) false) {
                        printf("\tUnrecognized \"%s\" specified (%s) for Property tuple %s. No match in Material tuple\n", keyValue,
                                keyWord,
                                propertyTuple[i].name);

                        if (keyValue != NULL) EG_free(keyValue);
                        if (tempString != NULL) EG_free(tempString);

                        (void) string_freeArray(feaProblem->feaProperty[i].numPly, &tempStringArray);

                        return CAPS_NOTFOUND;
                    }

                    if (tempString != NULL) EG_free(tempString);
                    tempString = NULL;
                }
            }


            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            status = string_freeArray(feaProblem->feaProperty[i].numPly, &tempStringArray);
            if (status != CAPS_SUCCESS) return status;

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>shearBondAllowable = 0.0 </B> </li> <br>
             *  Allowable interlaminar shear stress.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>shearBondAllowable = 0.0 </B> </li> <br>
             *  Allowable interlaminar shear stress.
             * </ul>
             * \endif
             */
            keyWord = "shearBondAllowable";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].compositeShearBondAllowable);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>symmetricLaminate = False </B> </li> <br>
             *  Symmetric lamination option. True- SYM only half the plies are specified, for odd number plies 1/2 thickness
             *   of center ply is specified with the first ply being the bottom ply in the stack, default (False) all plies specified.
             * </ul>
             * \elseif (ASTROS)
             * <ul>
             *  <li> <B>symmetricLaminate = False </B> </li> <br>
             *  Symmetric lamination option. If "True" only half the plies are specified (the plies will be repeated in
             *  reverse order internally in the PCOMP card). For an odd number of plies, the 1/2 thickness
             *   of the center ply is specified with the first ply being the bottom ply in the stack, default (False) all plies specified.
             * </ul>
             *
             * \else
             * <ul>
             *  <li> <B>symmetricLaminate = False </B> </li> <br>
             *  Symmetric lamination option. True- SYM only half the plies are specified, for odd number plies 1/2 thickness
             *   of center ply is specified with the first ply being the bottom ply in the stack, default (False) all plies specified
             * </ul>
             * \endif
             */
            keyWord = "symmetricLaminate";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toBoolean(keyValue, &feaProblem->feaProperty[i].compositeSymmetricLaminate);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>compositeFailureTheory = "(no default)" </B> </li> <br>
             *  Composite failure theory. Options: "HILL", "HOFF", "TSAI", and "STRN"
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>compositeFailureTheory = "(no default)" </B> </li> <br>
             *  Composite failure theory.
             * </ul>
             * \endif
             */
            keyWord = "compositeFailureTheory";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                feaProblem->feaProperty[i].compositeFailureTheory = string_removeQuotation(keyValue);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>compositeThickness = (no default) </B> </li> <br>
             *  List of composite thickness for each layer (e.g. [1.2, 4.0, 3.0]). If the length of this list doesn't match the length
             *  of the "compositeMaterial" list, the list is either truncated [ >length("compositeMaterial")] or expanded [ <length("compositeMaterial")]
             *  in which case the last thickness provided is repeated.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>compositeThickness = (no default) </B> </li> <br>
             *  List of composite thickness for each layer (e.g. [1.2, 4.0, 3.0]). If the length of this list doesn't match the length
             *  of the "compositeMaterial" list, the list is either truncated [ >length("compositeMaterial")] or expanded [ <length("compositeMaterial")]
             *  in which case the last thickness provided is repeated.
             * </ul>
             * \endif
             */
            keyWord = "compositeThickness";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDoubleDynamicArray(keyValue, &tempInteger,
                        &feaProblem->feaProperty[i].compositeThickness);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;

                if (tempInteger < feaProblem->feaProperty[i].numPly) {

                    printf("\tThe number of thicknesses provided does not match the number of materials for the composite. "
                            "The last thickness will be repeated %d times\n", feaProblem->feaProperty[i].numPly - tempInteger);

                    feaProblem->feaProperty[i].compositeThickness = (double *) EG_reall(feaProblem->feaProperty[i].compositeThickness,
                                                                                        feaProblem->feaProperty[i].numPly*sizeof(double));

                    if (feaProblem->feaProperty[i].compositeThickness == NULL ) return EGADS_MALLOC;

                    for (j = 0; j < feaProblem->feaProperty[i].numPly - tempInteger; j++) {

                        feaProblem->feaProperty[i].compositeThickness[j+tempInteger] = feaProblem->feaProperty[i].compositeThickness[tempInteger-1];
                    }
                }

                if (tempInteger > feaProblem->feaProperty[i].numPly) {

                    printf("\tThe number of thicknesses provided does not match the number of materials for the composite. "
                            "The last %d thicknesses will be not be used\n", tempInteger -feaProblem->feaProperty[i].numPly);

                    feaProblem->feaProperty[i].compositeThickness = (double *) EG_reall(feaProblem->feaProperty[i].compositeThickness,
                                                                                        feaProblem->feaProperty[i].numPly*sizeof(double));

                    if (feaProblem->feaProperty[i].compositeThickness == NULL ) return EGADS_MALLOC;
                }
            } else {

                if (feaProblem->feaProperty[i].numPly != 0 &&
                        feaProblem->feaProperty[i].propertyType == Composite) {

                    printf("\"compositeMaterial\" have been set but no thicknesses (\"compositeThickness\") provided!!!\n");
                    return CAPS_BADVALUE;

                }
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>compositeOrientation = (no default) </B> </li> <br>
             *  List of composite orientations (angle relative element material axis) for each layer (eg. [5.0, 10.0, 30.0]).
             *  If the length of this list doesn't match the length of the "compositeMaterial" list, the list is either
             *  truncated [ >length("compositeMaterial")] or expanded [ <length("compositeMaterial")] in which case
             *  the last orientation provided is repeated.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>compositeOrientation = (no default) </B> </li> <br>
             *  List of composite orientations (angle relative element material axis) for each layer (eg. [5.0, 10.0, 30.0]).
             *  If the length of this list doesn't match the length of the "compositeMaterial" list, the list is either
             *  truncated [ >length("compositeMaterial")] or expanded [ <length("compositeMaterial")] in which case
             *  the last orientation provided is repeated.
             * </ul>
             * \endif
             */
            keyWord = "compositeOrientation";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDoubleDynamicArray(keyValue, &tempInteger,
                        &feaProblem->feaProperty[i].compositeOrientation);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;


                if (tempInteger < feaProblem->feaProperty[i].numPly) {

                    printf("\tThe number of orientations provided does not match the number of materials for the composite. "
                            "The last orientation will be repeated %d times\n", feaProblem->feaProperty[i].numPly - tempInteger);

                    feaProblem->feaProperty[i].compositeOrientation = (double *) EG_reall(feaProblem->feaProperty[i].compositeOrientation,
                                                                                          feaProblem->feaProperty[i].numPly*sizeof(double));

                    if (feaProblem->feaProperty[i].compositeOrientation == NULL ) return EGADS_MALLOC;

                    for (j = 0; j < feaProblem->feaProperty[i].numPly - tempInteger; j++) {

                        feaProblem->feaProperty[i].compositeOrientation[j+tempInteger] = feaProblem->feaProperty[i].compositeOrientation[tempInteger-1];
                    }
                }

                if (tempInteger > feaProblem->feaProperty[i].numPly) {

                    printf("\tThe number of orientations provided does not match the number of materials for the composite. "
                            "The last %d orientation will be not be used\n", tempInteger -feaProblem->feaProperty[i].numPly);

                    feaProblem->feaProperty[i].compositeOrientation = (double *) EG_reall(feaProblem->feaProperty[i].compositeOrientation,
                            feaProblem->feaProperty[i].numPly*sizeof(double));

                    if (feaProblem->feaProperty[i].compositeOrientation == NULL ) return EGADS_MALLOC;
                }

            } else {

                if (feaProblem->feaProperty[i].numPly != 0 &&
                        feaProblem->feaProperty[i].propertyType == Composite) {

                    printf("\t\"compositeMaterial\" have been set but no Orientation  (\"compositeOrientation\") provided!!!\n");
                    return CAPS_BADVALUE;
                }
            }

            // Mass

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>mass = 0.0</B> </li> <br>
             *  Mass value.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>mass = 0.0</B> </li> <br>
             *  Mass value.
             * </ul>
             * \endif
             */
            keyWord = "mass";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDouble(keyValue, &feaProblem->feaProperty[i].mass);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>massOffset = [0.0, 0.0, 0.0]</B> </li> <br>
             *  Offset distance from the grid point to the center of gravity for a concentrated mass.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>massOffset = [0.0, 0.0, 0.0]</B> </li> <br>
             *  Offset distance from the grid point to the center of gravity for a concentrated mass.
             * </ul>
             * \endif
             */
            keyWord = "massOffset";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDoubleArray(keyValue,
                        (int) sizeof(feaProblem->feaProperty[i].massOffset)/sizeof(double),
                        feaProblem->feaProperty[i].massOffset);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaProperty
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>massInertia = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]</B> </li> <br>
             *  Mass moment of inertia measured at the mass center of gravity.
             * </ul>
             * \elseif ABAQUS
             *v
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>massInertia = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]</B> </li> <br>
             *  Mass moment of inertia measured at the mass center of gravity.
             * </ul>
             * \endif
             */
            keyWord = "massInertia";
            status = search_jsonDictionary( propertyTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                status = string_toDoubleArray(keyValue,
                        (int) sizeof(feaProblem->feaProperty[i].massInertia)/sizeof(double),
                        feaProblem->feaProperty[i].massInertia);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

        } else {

            /*! \page feaProperty
             * \section keyStringProperty Single Value String
             *
             * If "Value" is a string, the string value may correspond to an entry in a predefined property lookup
             * table. NOT YET IMPLEMENTED!!!!
             *
             */

            // CALL property look up
            printf("\tError: Property tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;
        }
    }

    printf("\tDone getting FEA properties\n");

    if (keyValue != NULL) {
        EG_free(keyValue);
        keyValue = NULL;
    }

    return CAPS_SUCCESS;

}

// Get the constraint properties from a capsTuple
int fea_getConstraint(int numConstraintTuple,
                      capsTuple constraintTuple[],
                      mapAttrToIndexStruct *attrMap,
                      feaProblemStruct *feaProblem) {

    /*! \page feaConstraint FEA Constraint
     * Structure for the constraint tuple  = ("Constraint Name", "Value").
     * "Constraint Name" defines the reference name for the constraint being specified.
     *	The "Value" can either be a JSON String dictionary (see Section \ref jsonStringConstraint) or a single string keyword
     *	(see Section \ref keyStringConstraint).
     */

    int status; //Function return

    int i, groupIndex, attrIndex, nodeIndex; // Indexing

    char *keyValue = NULL; // Key values from tuple searches
    char *keyWord = NULL; // Key words to find in the tuples

    int numGroupName = 0;
    char **groupName = NULL;

    feaMeshDataStruct *feaData;

    // Destroy our constraint structures coming in if aren't 0 and NULL already
    if (feaProblem->feaConstraint != NULL) {
        for (i = 0; i < feaProblem->numConstraint; i++) {
            status = destroy_feaConstraintStruct(&feaProblem->feaConstraint[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (feaProblem->feaConstraint != NULL) EG_free(feaProblem->feaConstraint);
    feaProblem->feaConstraint = NULL;
    feaProblem->numConstraint = 0;

    printf("\nGetting FEA constraints.......\n");

    feaProblem->numConstraint = numConstraintTuple;

    printf("\tNumber of constraints - %d\n", feaProblem->numConstraint);

    // Allocate constraints
    if (feaProblem->numConstraint > 0) {
        feaProblem->feaConstraint = (feaConstraintStruct *) EG_alloc(feaProblem->numConstraint * sizeof(feaConstraintStruct));

        if (feaProblem->feaConstraint == NULL ) return EGADS_MALLOC;
    }

    // Initiate constraints to default values
    for (i = 0; i < feaProblem->numConstraint; i++) {
        status = initiate_feaConstraintStruct(&feaProblem->feaConstraint[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    // Loop through tuples and fill out the constraint structures
    for (i = 0; i < feaProblem->numConstraint; i++) {

        printf("\tConstraint name - %s\n", constraintTuple[i].name );

        // Set constraint name to tuple attribute name
        feaProblem->feaConstraint[i].name = (char *) EG_alloc((strlen(constraintTuple[i].name) + 1)*sizeof(char));
        if (feaProblem->feaConstraint[i].name == NULL) return EGADS_MALLOC;

        memcpy(feaProblem->feaConstraint[i].name, constraintTuple[i].name, strlen(constraintTuple[i].name)*sizeof(char));
        feaProblem->feaConstraint[i].name[strlen(constraintTuple[i].name)] = '\0';

        // Set constraint id -> 1 bias
        feaProblem->feaConstraint[i].constraintID = i+1;

        // Do we have a json string?
        if (strncmp(constraintTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n", constraintTuple[i].value);

            /*! \page feaConstraint
             * \section jsonStringConstraint JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if (MYSTRAN || NASTRAN)
             *  (eg. "Value" = {"groupName": "plateEdge", "dofConstraint": 123456})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>constraintType = "ZeroDisplacement"</B> </li> <br>
             *  Type of constraint. Options: "Displacement", "ZeroDisplacement".
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>constraintType = "ZeroDisplacement"</B> </li> <br>
             *  Type of constraint. Options: "Displacement", "ZeroDisplacement".
             * </ul>
             * \endif
             *
             */

            // Get constraint Type
            keyWord = "constraintType";
            status = search_jsonDictionary( constraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{UnknownConstraint, Displacement, ZeroDisplacement}
                if      (strcasecmp(keyValue, "\"Displacement\"")      == 0) feaProblem->feaConstraint[i].constraintType = Displacement;
                else if (strcasecmp(keyValue, "\"ZeroDisplacement\"")  == 0) feaProblem->feaConstraint[i].constraintType = ZeroDisplacement;
                else {

                    printf("\tUnrecognized \"%s\" specified (%s) for Constraint tuple %s, defaulting to \"ZeroDisplacement\"\n", keyWord,
                                                                                                                                 keyValue,
                                                                                                                                 constraintTuple[i].name);
                    feaProblem->feaConstraint[i].constraintType = ZeroDisplacement;
                }

            } else {

                printf("\tNo \"%s\" specified for Constraint tuple %s, defaulting to \"ZeroDisplacement\"\n", keyWord,
                        constraintTuple[i].name);
                feaProblem->feaConstraint[i].constraintType = ZeroDisplacement;
            }

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            // Get constraint node set

            /*! \page feaConstraint
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsConstraint</c> names on which to apply the constraint
             *  (e.g. "Name1" or ["Name1","Name2",...]. If not provided, the constraint tuple name will be
             *  used.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsConstraint</c> names on which to apply the constraint
             *  (e.g. ["Name1","Name2",...]. If not provided, the constraint tuple name will be
             *  used.
             * </ul>
             * \endif
             *
             */

            keyWord = "groupName";
            status = search_jsonDictionary( constraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;

                if (status != CAPS_SUCCESS) return status;

            } else {

                printf("\tNo \"%s\" specified for Constraint tuple %s, going to use constraint name\n", keyWord,
                                                                                                        constraintTuple[i].name);

                status = string_toStringDynamicArray(constraintTuple[i].name, &numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;

            }

            // Determine how many point constraints we have
            feaProblem->feaConstraint[i].numGridID = 0;
            for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                status = get_mapAttrToIndexIndex(attrMap, (const char *) groupName[groupIndex], &attrIndex);

                if (status == CAPS_NOTFOUND) {
                    printf("\tName %s not found in attribute map of capsConstraints!!!!\n", groupName[groupIndex]);
                    continue;
                } else if (status != CAPS_SUCCESS) return status;

                // Now lets loop through the grid to see how many grid points have the attrIndex
                for (nodeIndex = 0; nodeIndex < feaProblem->feaMesh.numNode; nodeIndex++ ) {

                    if (feaProblem->feaMesh.node[nodeIndex].analysisType == MeshStructure) {
                        feaData = (feaMeshDataStruct *) feaProblem->feaMesh.node[nodeIndex].analysisData;
                    } else {
                        continue;
                    }

                    if (feaData->constraintIndex == attrIndex) {

                        feaProblem->feaConstraint[i].numGridID += 1;

                        // Allocate/Re-allocate grid ID array
                        if (feaProblem->feaConstraint[i].numGridID == 1) {
                            feaProblem->feaConstraint[i].gridIDSet = (int *) EG_alloc(feaProblem->feaConstraint[i].numGridID*sizeof(int));
                        } else {
                            feaProblem->feaConstraint[i].gridIDSet = (int *) EG_reall(feaProblem->feaConstraint[i].gridIDSet,
                                                                                      feaProblem->feaConstraint[i].numGridID*sizeof(int));
                        }

                        if (feaProblem->feaConstraint[i].gridIDSet == NULL) {
                            status = string_freeArray(numGroupName, &groupName);
                            if (status != CAPS_SUCCESS) printf("Status %d during string_freeArray\n", status);

                            return EGADS_MALLOC;
                        }

                        // Set grid ID value -> 1 bias
                        feaProblem->feaConstraint[i].gridIDSet[feaProblem->feaConstraint[i].numGridID-1] = feaProblem->feaMesh.node[nodeIndex].nodeID;
                    }
                }
            }

            status = string_freeArray(numGroupName, &groupName);
            if (status != CAPS_SUCCESS) return status;
            groupName = NULL;


            //Fill up constraint properties
            /*! \page feaConstraint
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>dofConstraint = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom that will be constrained (123 - zero translation in all three
             *  directions).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>dofConstraint = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom that will be constrained (123 - zero translation in all three
             *  directions).
             * </ul>
             * \endif
             *
             */
            keyWord = "dofConstraint";
            status = search_jsonDictionary( constraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaConstraint[i].dofConstraint);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaConstraint
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>gridDisplacement = 0.0 </B> </li> <br>
             *  Value of displacement for components defined in "dofConstraint".
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>gridDisplacement = 0.0 </B> </li> <br>
             *  Value of displacement for components defined in "dofConstraint".
             * </ul>
             * \endif
             */
            keyWord = "gridDisplacement";
            status = search_jsonDictionary( constraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaConstraint[i].gridDisplacement);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

        } else {

            /*! \page feaConstraint
             * \section keyStringConstraint Single Value String
             *
             * If "Value" is a string, the string value may correspond to an entry in a predefined constraint lookup
             * table. NOT YET IMPLEMENTED!!!!
             *
             */

            // Call some look up table maybe?
            printf("\tError: Constraint tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;
        }
    }

    if (keyValue != NULL) EG_free(keyValue);

    printf("\tDone getting FEA constraints\n");
    return CAPS_SUCCESS;
}

// Get the support properties from a capsTuple
int fea_getSupport(int numSupportTuple,
                   capsTuple supportTuple[],
                   mapAttrToIndexStruct *attrMap,
                   feaProblemStruct *feaProblem) {

    /*! \page feaSupport FEA Support
     * Structure for the support tuple  = ("Support Name", "Value").
     * "Support Name" defines the reference name for the support being specified.
     *	The "Value" can either be a JSON String dictionary (see Section \ref jsonStringSupport) or a single string keyword
     *	(see Section \ref keyStringSupport).
     */

    int status; //Function return

    int i, groupIndex, attrIndex, nodeIndex; // Indexing

    char *keyValue = NULL; // Key values from tuple searches
    char *keyWord = NULL; // Key words to find in the tuples

    int numGroupName = 0;
    char **groupName = NULL;

    feaMeshDataStruct *feaData;

    // Destroy our support structures coming in if aren't 0 and NULL already
    if (feaProblem->feaSupport != NULL) {
        for (i = 0; i < feaProblem->numSupport; i++) {
            status = destroy_feaSupportStruct(&feaProblem->feaSupport[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (feaProblem->feaSupport != NULL) EG_free(feaProblem->feaSupport);
    feaProblem->feaSupport = NULL;
    feaProblem->numSupport = 0;

    printf("\nGetting FEA supports.......\n");

    feaProblem->numSupport = numSupportTuple;

    printf("\tNumber of supports - %d\n", feaProblem->numSupport);

    // Allocate supports
    if (feaProblem->numSupport > 0) {
        feaProblem->feaSupport = (feaSupportStruct *) EG_alloc(feaProblem->numSupport * sizeof(feaSupportStruct));

        if (feaProblem->feaSupport == NULL ) return EGADS_MALLOC;
    }

    // Initiate supports to default values
    for (i = 0; i < feaProblem->numSupport; i++) {
        status = initiate_feaSupportStruct(&feaProblem->feaSupport[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    // Loop through tuples and fill out the support structures
    for (i = 0; i < feaProblem->numSupport; i++) {

        printf("\tSupport name - %s\n", supportTuple[i].name );

        // Set support name to tuple attribute name
        feaProblem->feaSupport[i].name = (char *) EG_alloc((strlen(supportTuple[i].name) + 1)*sizeof(char));
        if (feaProblem->feaSupport[i].name == NULL) return EGADS_MALLOC;

        memcpy(feaProblem->feaSupport[i].name, supportTuple[i].name, strlen(supportTuple[i].name)*sizeof(char));
        feaProblem->feaSupport[i].name[strlen(supportTuple[i].name)] = '\0';

        // Set support id -> 1 bias
        feaProblem->feaSupport[i].supportID = i+1;

        // Do we have a json string?
        if (strncmp(supportTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n", supportTuple[i].value);

            /*! \page feaSupport
             * \section jsonStringSupport JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if (MYSTRAN || NASTRAN)
             *  (eg. "Value" = {"groupName": "plateEdge", "dofSupport": 123456})
             * \endif
             *  the following keywords ( = default values) may be used:
             */

            // Get support node set

            /*! \page feaSupport
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsConstraint</c> names on which to apply the support
             *  (e.g. "Name1" or ["Name1","Name2",...]. If not provided, the constraint tuple name will be
             *  used.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsConstraint</c> names on which to apply the support
             *  (e.g. ["Name1","Name2",...]. If not provided, the constraint tuple name will be
             *  used.
             * </ul>
             * \endif
             *
             */
            keyWord = "groupName";
            status = search_jsonDictionary( supportTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;

                if (status != CAPS_SUCCESS) return status;

            } else {

                printf("\tNo \"%s\" specified for Support tuple %s, going to use support name\n", keyWord,
                        supportTuple[i].name);

                status = string_toStringDynamicArray(supportTuple[i].name, &numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;

            }

            // Determine how many point supports we have
            feaProblem->feaSupport[i].numGridID = 0;
            for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                status = get_mapAttrToIndexIndex(attrMap, (const char *) groupName[groupIndex], &attrIndex);

                if (status == CAPS_NOTFOUND) {
                    printf("\tName %s not found in attribute map of capsConstraints!!!!\n", groupName[groupIndex]);
                    continue;
                } else if (status != CAPS_SUCCESS) return status;

                // Now lets loop through the grid to see how many grid points have the attrIndex
                for (nodeIndex = 0; nodeIndex < feaProblem->feaMesh.numNode; nodeIndex++ ) {

                    if (feaProblem->feaMesh.node[nodeIndex].analysisType == MeshStructure) {
                        feaData = (feaMeshDataStruct *) feaProblem->feaMesh.node[nodeIndex].analysisData;
                    } else {
                        continue;
                    }

                    if (feaData->constraintIndex == attrIndex) {

                        feaProblem->feaSupport[i].numGridID += 1;

                        // Allocate/Re-allocate grid ID array
                        if (feaProblem->feaSupport[i].numGridID == 1) {
                            feaProblem->feaSupport[i].gridIDSet = (int *) EG_alloc(feaProblem->feaSupport[i].numGridID*sizeof(int));
                        } else {
                            feaProblem->feaSupport[i].gridIDSet = (int *) EG_reall(feaProblem->feaSupport[i].gridIDSet,
                                                                                   feaProblem->feaSupport[i].numGridID*sizeof(int));
                        }

                        if (feaProblem->feaSupport[i].gridIDSet == NULL) {
                            status = string_freeArray(numGroupName, &groupName);
                            if (status != CAPS_SUCCESS) printf("Status %d during string_freeArray\n", status);

                            return EGADS_MALLOC;
                        }

                        // Set grid ID value -> 1 bias
                        feaProblem->feaSupport[i].gridIDSet[feaProblem->feaSupport[i].numGridID-1] = feaProblem->feaMesh.node[nodeIndex].nodeID;
                    }
                }
            }

            status = string_freeArray(numGroupName, &groupName);
            if (status != CAPS_SUCCESS) return status;
            groupName = NULL;

            //Fill up support properties
            /*! \page feaSupport
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>dofSupport = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom that will be supported (123 - zero translation in all three
             *  directions).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>dofSupport = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom that will be supported (123 - zero translation in all three
             *  directions).
             * </ul>
             * \endif
             *
             */
            keyWord = "dofSupport";
            status = search_jsonDictionary( supportTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaSupport[i].dofSupport);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

        } else {

            /*! \page feaSupport
             * \section keyStringSupport Single Value String
             *
             * If "Value" is a string, the string value may correspond to an entry in a predefined support lookup
             * table. NOT YET IMPLEMENTED!!!!
             *
             */

            // Call some look up table maybe?
            printf("\tError: Support tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;

        }
    }

    if (keyValue != NULL) {
        EG_free(keyValue);
        keyValue = NULL;
    }

    printf("\tDone getting FEA supports\n");
    return CAPS_SUCCESS;
}

// Get the Connections properties from a capsTuple and create connections based on the mesh
int fea_getConnection(int numConnectionTuple,
                      capsTuple connectionTuple[],
                      mapAttrToIndexStruct *attrMap,
                      feaProblemStruct *feaProblem) {

    /*! \page feaConnection FEA Connection
     * Structure for the connection tuple  = ("Connection Name", "Value").
     * "Connection Name" defines the reference name to the capsConnect being specified and denotes the "source" node
     * for the connection.
     *	The "Value" can either be a JSON String dictionary (see Section \ref jsonStringConnection) or a single string keyword
     *	(see Section \ref keyStringConnection).
     */

    int status; //Function return

    int i, groupIndex, nodeIndex, nodeIndexDest, counter = 0; // Indexing

    char *keyValue = NULL; // Key values from tuple searches
    char *keyWord = NULL; // Key words to find in the tuples

    int numGroupName = 0;
    char **groupName = NULL;
    int attrIndex, attrIndexDest;

    // Values to set
    feaConnectionTypeEnum connectionType;
    int dofDependent = 0, componentNumberStart = 0, componentNumberEnd = 0;
    double stiffnessConst = 0.0, dampingConst  = 0.0, stressCoeff = 0.0, mass = 0.0;

    feaMeshDataStruct *feaData, *feaDataDest;

    // Destroy our support structures coming in if aren't 0 and NULL already
    if (feaProblem->feaConnect != NULL) {
        for (i = 0; i < feaProblem->numConnect; i++) {
            status = destroy_feaConnectionStruct(&feaProblem->feaConnect[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (feaProblem->feaConnect != NULL) EG_free(feaProblem->feaConnect);
    feaProblem->feaConnect = NULL;
    feaProblem->numConnect = 0;

    printf("\nGetting FEA connections.......\n");

    printf("\tNumber of connection tuples - %d\n", numConnectionTuple);

    // Loop through tuples and fill out the support structures
    for (i = 0; i < numConnectionTuple; i++) {

        // Reset defaults
        dofDependent  = componentNumberStart = componentNumberEnd = 0;
        stiffnessConst = dampingConst = stressCoeff = mass = 0.0;

        printf("\tConnection name - %s\n", connectionTuple[i].name );

        // Look for connection name in connection map
        status = get_mapAttrToIndexIndex(attrMap, (const char *) connectionTuple[i].name, &attrIndex);
        if (status == CAPS_NOTFOUND) {
            printf("\tName %s not found in attribute map of capsConnect!!!!\n", connectionTuple[i].name);
            continue;
        } else if (status != CAPS_SUCCESS) return status;

        // Do we have a json string?
        if (strncmp(connectionTuple[i].value, "{", 1) == 0) {

            /*! \page feaConnection
             * \section jsonStringConnection JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if (MYSTRAN || NASTRAN || ASTROS)
             *  (e.g. "Value" = {"dofDependent": 1, "propertyType": "RigidBody"})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>connectionType = RigidBody</B> </li> <br>
             *  Type of connection to apply to a given capsConnect pair defined by "Connection Name" and the "groupName".
             *  Options: Mass (scalar), Spring (scalar), Damper (scalar), RigidBody.
             * </ul>
             * \elseif (ASTROS)
             * <ul>
             *  <li> <B>connectionType = RigidBody</B> </li> <br>
             *  Type of connection to apply to a given capsConnect pair defined by "Connection Name" and the "groupName".
             *  Options: Mass (scalar), Spring (scalar), RigidBody.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             * <li> <B>connectionType = RigidBody</B> </li> <br>
             *  Type of connection to apply to a given capsConnect pair defined by "Connection Name" and the "groupName".
             *  Options: Mass (scalar), Spring (scalar), Damper (scalar), RigidBody.
             *  </ul>
             * \endif
             *
             */

            // Get connection Type
            keyWord = "connectionType";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{UnknownConnection, Mass, Spring, Damper, RigidBody}
                if      (strcasecmp(keyValue, "\"Mass\"")      == 0) connectionType = Mass;
                else if (strcasecmp(keyValue, "\"Spring\"")    == 0) connectionType = Spring;
                else if (strcasecmp(keyValue, "\"Damper\"")    == 0) connectionType = Damper;
                else if (strcasecmp(keyValue, "\"RigidBody\"") == 0) connectionType = RigidBody;
                else {
                    printf("\tUnrecognized \"%s\" specified (%s) for Connection tuple %s, current options are "
                            "\"Mass, Spring, Damper, and RigidBody\"\n", keyWord,
                            keyValue,
                            connectionTuple[i].name);
                    if (keyValue != NULL) {
                        EG_free(keyValue);
                        keyValue = NULL;
                    }

                    return CAPS_NOTFOUND;
                }

            } else {

                printf("\tNo \"%s\" specified for Connection tuple %s, defaulting to RigidBody\n", keyWord,
                                                                                                   connectionTuple[i].name);
                connectionType = RigidBody;
            }

            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>dofDependent = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom of the dependent end of rigid body connections (ex. 123 - translation in all three
             *  directions).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>dofDependent = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom of the dependent end of rigid body connections (ex. 123 - translation in all three
             *  directions).
             * </ul>
             * \endif
             *
             */
            keyWord = "dofDependent";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &dofDependent);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }


            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>componentNumberStart = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom of the starting point of the connection for mass,
             *  spring, and damper elements (scalar) ( 0 <= Integer <= 6).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>componentNumberStart = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom of the starting point of the connection for mass,
             *  spring, and damper elements (scalar) ( 0 <= Integer <= 6).
             * </ul>
             * \endif
             *
             */
            keyWord = "componentNumberStart";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &componentNumberStart);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>componentNumberEnd= 0 </B> </li> <br>
             *  Component numbers / degrees of freedom of the ending point of the connection for mass,
             *  spring, and damper elements (scalar) ( 0 <= Integer <= 6).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>componentNumberEnd = 0 </B> </li> <br>
             *  Component numbers / degrees of freedom of the ending point of the connection for mass,
             *  spring, and damper elements (scalar) ( 0 <= Integer <= 6).
             * </ul>
             * \endif
             *
             */
            keyWord = "componentNumberEnd";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &componentNumberEnd);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>stiffnessConst = 0.0 </B> </li> <br>
             *  Stiffness constant of a spring element (scalar).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>stiffnessConst = 0.00 </B> </li> <br>
             *  Stiffness constant of a spring element (scalar).
             * </ul>
             * \endif
             *
             */
            keyWord = "stiffnessConst";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &stiffnessConst);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>dampingConst = 0.0 </B> </li> <br>
             *  Damping coefficient/constant of a spring or damping element (scalar).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>dampingConst = 0.0 </B> </li> <br>
             *  Damping constant of a spring or damping element (scalar).
             * </ul>
             * \endif
             *
             */
            keyWord = "dampingConst";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &dampingConst);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>stressCoeff = 0.0 </B> </li> <br>
             *  Stress coefficient of a spring element (scalar).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>stressCoeff = 0.0 </B> </li> <br>
             * Stress coefficient of a spring element (scalar).
             * </ul>
             * \endif
             *
             */
            keyWord = "stressCoeff";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &stressCoeff);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>mass = 0.0 </B> </li> <br>
             *  Mass of a mass element (scalar).
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>mass = 0.0 </B> </li> <br>
             * Mass of a mass element (scalar).
             * </ul>
             * \endif
             *
             */
            keyWord = "mass";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &mass);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaConnection
             *
             * \if (MYSTRAN || NASTRAN || ASTROS)
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsConnect</c> names on which to connect the nodes found with the
             *  tuple name ("Connection Name") to. (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             * <li> <B>groupName = "(no default)"</B> </li> <br>
             * Single or list of <c>capsConnect</c> names on which to connect the nodes found with the
             *  tuple name ("Connection Name") to. (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \endif
             *
             */
            numGroupName = 0;
            keyWord = "groupName";
            status = search_jsonDictionary( connectionTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;

                if (status != CAPS_SUCCESS) return status;

                // Lets loop through the group names and create the connections
                for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                    status = get_mapAttrToIndexIndex(attrMap, (const char *) groupName[groupIndex], &attrIndexDest);
                    if (status == CAPS_NOTFOUND) {
                        printf("\tName %s not found in attribute map of capsConnects!!!!\n", groupName[groupIndex]);
                        continue;
                    } else if (status != CAPS_SUCCESS) {
                        (void) string_freeArray(numGroupName, &groupName);
                        return status;
                    }

                    // Find the "source" node in the mesh
                    for (nodeIndex = 0; nodeIndex < feaProblem->feaMesh.numNode; nodeIndex++ ) {

                        feaData = (feaMeshDataStruct *) feaProblem->feaMesh.node[nodeIndex].analysisData;

                        // If "source" doesn't match - continue
                        if (feaData->connectIndex != attrIndex) continue;

                        // Find the "destination" node in the mesh
                        for (nodeIndexDest = 0; nodeIndexDest < feaProblem->feaMesh.numNode; nodeIndexDest++) {

                            feaDataDest = (feaMeshDataStruct *) feaProblem->feaMesh.node[nodeIndexDest].analysisData;

                            // If the "destination" doesn't match - continue
                            if (feaDataDest->connectIndex != attrIndexDest) continue;

                            feaProblem->numConnect += 1;

                            if (feaProblem->numConnect == 1) {
                                feaProblem->feaConnect = (feaConnectionStruct *) EG_alloc(sizeof(feaConnectionStruct));
                            } else {
                                feaProblem->feaConnect = (feaConnectionStruct *) EG_reall(feaProblem->feaConnect ,
                                                                                          feaProblem->numConnect*sizeof(feaConnectionStruct));
                            }

                            if (feaProblem->feaConnect == NULL) {
                                feaProblem->numConnect = 0;
                                (void) string_freeArray(numGroupName, &groupName);
                                return EGADS_MALLOC;
                            }

                            feaProblem->feaConnect[feaProblem->numConnect-1].name = (char *) EG_alloc((strlen(connectionTuple[i].name) + 1)*sizeof(char));
                            if (feaProblem->feaConnect[feaProblem->numConnect-1].name == NULL) {
                                (void) string_freeArray(numGroupName, &groupName);
                                return EGADS_MALLOC;
                            }

                            memcpy(feaProblem->feaConnect[feaProblem->numConnect-1].name,
                                   connectionTuple[i].name,
                                   strlen(connectionTuple[i].name)*sizeof(char));
                            feaProblem->feaConnect[feaProblem->numConnect-1].name[strlen(connectionTuple[i].name)] = '\0';

                            feaProblem->feaConnect[feaProblem->numConnect-1].connectionID = i+1; // ConnectionTuple index

                            feaProblem->feaConnect[feaProblem->numConnect-1].connectionType = connectionType;
                            feaProblem->feaConnect[feaProblem->numConnect-1].elementID = feaProblem->numConnect + feaProblem->feaMesh.numElement;
                            feaProblem->feaConnect[feaProblem->numConnect-1].connectivity[0] = feaProblem->feaMesh.node[nodeIndex].nodeID;
                            feaProblem->feaConnect[feaProblem->numConnect-1].connectivity[1] = feaProblem->feaMesh.node[nodeIndexDest].nodeID;

                            feaProblem->feaConnect[feaProblem->numConnect-1].dofDependent = dofDependent;
                            feaProblem->feaConnect[feaProblem->numConnect-1].stiffnessConst = stiffnessConst;
                            feaProblem->feaConnect[feaProblem->numConnect-1].dampingConst = dampingConst;
                            feaProblem->feaConnect[feaProblem->numConnect-1].stressCoeff = stressCoeff;
                            feaProblem->feaConnect[feaProblem->numConnect-1].mass = mass;
                            feaProblem->feaConnect[feaProblem->numConnect-1].componentNumberStart = componentNumberStart;
                            feaProblem->feaConnect[feaProblem->numConnect-1].componentNumberEnd = componentNumberEnd;
                        } // End destination loop
                    } // End source loop
                } // End group loop

                status =  string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;

            } else {
                printf("\tNo \"%s\" specified for Connection tuple %s!\n", keyWord,
                        connectionTuple[i].name);
            }


            // Create automatic connections from the "capsConnectLink" tag
            printf("\tLooking for automatic connections from the use of capsConnectLink for %s\n", connectionTuple[i].name);

            // Find the "source" node in the mesh
            for (nodeIndex = 0; nodeIndex < feaProblem->feaMesh.numNode; nodeIndex++ ) {

                feaData = (feaMeshDataStruct *) feaProblem->feaMesh.node[nodeIndex].analysisData;

                // If "source" doesn't match - continue
                if (feaData->connectIndex != attrIndex) continue;

                counter = 0;
                // Find the "destination" node in the mesh
                for (nodeIndexDest = 0; nodeIndexDest < feaProblem->feaMesh.numNode; nodeIndexDest++) {

                    feaDataDest = (feaMeshDataStruct *) feaProblem->feaMesh.node[nodeIndexDest].analysisData;

                    // If the "destination" doesn't match - continue
                    if (feaDataDest->connectLinkIndex != attrIndex) continue;

                    counter +=1;

                    feaProblem->numConnect += 1;

                    if (feaProblem->numConnect == 1) {
                        feaProblem->feaConnect = (feaConnectionStruct *) EG_alloc(sizeof(feaConnectionStruct));
                    } else {
                        feaProblem->feaConnect = (feaConnectionStruct *) EG_reall(feaProblem->feaConnect ,
                                                                                  feaProblem->numConnect*sizeof(feaConnectionStruct));
                    }

                    if (feaProblem->feaConnect == NULL) {
                        feaProblem->numConnect = 0;
                        return EGADS_MALLOC;
                    }

                    feaProblem->feaConnect[feaProblem->numConnect-1].name = (char *) EG_alloc((strlen(connectionTuple[i].name) + 1)*sizeof(char));
                    if (feaProblem->feaConnect[feaProblem->numConnect-1].name == NULL) return EGADS_MALLOC;

                    memcpy(feaProblem->feaConnect[feaProblem->numConnect-1].name,
                           connectionTuple[i].name,
                           strlen(connectionTuple[i].name)*sizeof(char));
                    feaProblem->feaConnect[feaProblem->numConnect-1].name[strlen(connectionTuple[i].name)] = '\0';

                    feaProblem->feaConnect[feaProblem->numConnect-1].connectionID = i+1; // ConnectionTuple index

                    feaProblem->feaConnect[feaProblem->numConnect-1].connectionType = connectionType;
                    feaProblem->feaConnect[feaProblem->numConnect-1].elementID = feaProblem->numConnect + feaProblem->feaMesh.numElement;
                    feaProblem->feaConnect[feaProblem->numConnect-1].connectivity[0] = feaProblem->feaMesh.node[nodeIndex].nodeID;
                    feaProblem->feaConnect[feaProblem->numConnect-1].connectivity[1] = feaProblem->feaMesh.node[nodeIndexDest].nodeID;

                    feaProblem->feaConnect[feaProblem->numConnect-1].dofDependent = dofDependent;
                    feaProblem->feaConnect[feaProblem->numConnect-1].stiffnessConst = stiffnessConst;
                    feaProblem->feaConnect[feaProblem->numConnect-1].dampingConst = dampingConst;
                    feaProblem->feaConnect[feaProblem->numConnect-1].stressCoeff = stressCoeff;
                    feaProblem->feaConnect[feaProblem->numConnect-1].componentNumberStart = componentNumberStart;
                    feaProblem->feaConnect[feaProblem->numConnect-1].componentNumberEnd = componentNumberEnd;
                } // End destination loop

                printf("\t%d automatic connections were made for capsConnect %s\n", counter,
                                                                                    connectionTuple[i].name);
            } // End source loop
        } else {

            /*! \page feaConnection
             * \section keyStringConnection Single Value String
             *
             * If "Value" is a string, the string value may correspond to an entry in a predefined connection lookup
             * table. NOT YET IMPLEMENTED!!!!
             *
             */

            // Call some look up table maybe?
            printf("\tError: Connection tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;

        }
    }

    if (keyValue != NULL) {
        EG_free(keyValue);
        keyValue = NULL;
    }

    printf("\tDone getting FEA connections\n");
    return CAPS_SUCCESS;
}

// Get the load properties from a capsTuple
int fea_getLoad(int numLoadTuple,
                capsTuple loadTuple[],
                mapAttrToIndexStruct *attrMap,
                feaProblemStruct *feaProblem) {

    /*! \page feaLoad FEA Load
     * Structure for the load tuple  = ("Load Name", "Value").
     * "Load Name" defines the reference name for the load being specified.
     *	The "Value" can either be a JSON String dictionary (see Section \ref jsonStringLoad) or a single string keyword
     *	(see Section \ref keyStringLoad).
     */

    int status; //Function return

    int i, groupIndex, attrIndex, nodeIndex, elementIndex; // Indexing

    char *keyValue = NULL; // Key values from tuple searches
    char *keyWord = NULL; // Key words to find in the tuples

    int numGroupName = 0;
    char **groupName = NULL;

    char *tempString = NULL; // Temporary string holder

    feaMeshDataStruct *feaData;

    // Destroy our load structures coming in if aren't 0 and NULL already
    if (feaProblem->feaLoad != NULL) {
        for (i = 0; i < feaProblem->numLoad; i++) {
            status = destroy_feaLoadStruct(&feaProblem->feaLoad[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (feaProblem->feaLoad != NULL) EG_free(feaProblem->feaLoad);
    feaProblem->feaLoad = NULL;
    feaProblem->numLoad = 0;

    printf("\nGetting FEA loads.......\n");

    feaProblem->numLoad = numLoadTuple;

    printf("\tNumber of loads - %d\n", feaProblem->numLoad);

    // Allocate loads
    if (feaProblem->numLoad > 0) {
        feaProblem->feaLoad = (feaLoadStruct *) EG_alloc(feaProblem->numLoad * sizeof(feaLoadStruct));

        if (feaProblem->feaLoad == NULL ) return EGADS_MALLOC;
    }

    // Initiate loads to default values
    for (i = 0; i < feaProblem->numLoad; i++) {
        status = initiate_feaLoadStruct(&feaProblem->feaLoad[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    // Loop through tuples and fill out the load structures
    for (i = 0; i < feaProblem->numLoad; i++) {

        printf("\tLoad name - %s\n", loadTuple[i].name );

        // Set load name to tuple attribute name
        feaProblem->feaLoad[i].name = (char *) EG_alloc((strlen(loadTuple[i].name) + 1)*sizeof(char));
        if (feaProblem->feaLoad[i].name == NULL) return EGADS_MALLOC;

        memcpy(feaProblem->feaLoad[i].name, loadTuple[i].name, strlen(loadTuple[i].name)*sizeof(char));
        feaProblem->feaLoad[i].name[strlen(loadTuple[i].name)] = '\0';

        // Set load id -> 1 bias
        feaProblem->feaLoad[i].loadID = i+1;

        // Do we have a json string?
        if (strncmp(loadTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n", constraintTuple[i].value);

            /*! \page feaLoad
             * \section jsonStringLoad JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if (MYSTRAN || NASTRAN || HSM)
             *  (e.g. "Value" = {"groupName": "plate", "loadType": "Pressure", "pressureForce": 2000000.0})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>loadType = "(no default)"</B> </li> <br>
             *  Type of load. Options: "GridForce", "GridMoment", "Rotational", "Thermal",
             *  "Pressure", "PressureDistribute", "PressureExternal", "Gravity".
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \elseif HSM
             *  <ul>
             *  <li> <B>loadType = "(no default)"</B> </li> <br>
             *  Type of load. Options: "GridForce", "GridMoment", "LineForce", "LineMoment", "Rotational",
             *  "Pressure", "PressureDistribute", "PressureExternal", "Gravity".
             * </ul>
             * \else
             * <ul>
             *  <li> <B>loadType = "(no default)"</B> </li> <br>
             *  Type of load. Options: "GridForce", "GridMoment", "Rotational", "Thermal",
             *  "Pressure", "PressureDistribute", "PressureExternal", "Gravity".
             * </ul>
             * \endif
             *
             */

            // Get load Type
            keyWord = "loadType";
            status = search_jsonDictionary(loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{UnknownLoad, GridForce, GridMoment, Gravity, Pressure, Rotational, Thermal}
                if      (strcasecmp(keyValue, "\"GridForce\"")          == 0) feaProblem->feaLoad[i].loadType = GridForce;
                else if (strcasecmp(keyValue, "\"GridMoment\"")         == 0) feaProblem->feaLoad[i].loadType = GridMoment;
                else if (strcasecmp(keyValue, "\"LineForce\"")          == 0) feaProblem->feaLoad[i].loadType = LineForce;
                else if (strcasecmp(keyValue, "\"LineMoment\"")         == 0) feaProblem->feaLoad[i].loadType = LineMoment;
                else if (strcasecmp(keyValue, "\"Rotational\"")         == 0) feaProblem->feaLoad[i].loadType = Rotational;
                else if (strcasecmp(keyValue, "\"Thermal\"")            == 0) feaProblem->feaLoad[i].loadType = Thermal;
                else if (strcasecmp(keyValue, "\"Pressure\"")           == 0) feaProblem->feaLoad[i].loadType = Pressure;
                else if (strcasecmp(keyValue, "\"PressureDistribute\"") == 0) feaProblem->feaLoad[i].loadType = PressureDistribute;
                else if (strcasecmp(keyValue, "\"PressureExternal\"")   == 0) feaProblem->feaLoad[i].loadType = PressureExternal;
                else if (strcasecmp(keyValue, "\"Gravity\"")            == 0) feaProblem->feaLoad[i].loadType = Gravity;
                else {
                    printf("\tUnrecognized \"%s\" specified (%s) for Load tuple %s\n", keyWord,
                            keyValue,
                            loadTuple[i].name);
                    if (keyValue != NULL) EG_free(keyValue);
                    return CAPS_NOTFOUND;
                }

            } else {
                printf("\t\"loadType\" variable not found in tuple %s, this is required input!!\n", loadTuple[i].name);

                if (keyValue != NULL) EG_free(keyValue);
                return status;
            }

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            // Get load node/element set
            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN || HSM)
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsLoad</c> names on which to apply the load
             *  (e.g. "Name1" or ["Name1","Name2",...]. If not provided, the load tuple name will be
             *  used.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsLoad</c> names on which to apply the load
             *  (e.g. "Name1" or ["Name1","Name2",...]. If not provided, the load tuple name will be
             *  used.
             * </ul>
             * \endif
             *
             */
            keyWord = "groupName";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;

                if (status != CAPS_SUCCESS) return status;

            } else {

                printf("\tNo \"%s\" specified for Load tuple %s, going to use load name\n", keyWord,
                        loadTuple[i].name);

                status = string_toStringDynamicArray(loadTuple[i].name, &numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;

            }

            // Determine how many element/point loads we have
            feaProblem->feaLoad[i].numGridID = 0;
            feaProblem->feaLoad[i].numElementID = 0;
            for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                // Do nothing for PressureExternal and Gravity loads
                if (feaProblem->feaLoad[i].loadType == PressureExternal) continue;
                if (feaProblem->feaLoad[i].loadType == Gravity) continue;

                status = get_mapAttrToIndexIndex(attrMap, (const char *) groupName[groupIndex], &attrIndex);

                if (status == CAPS_NOTFOUND) {
                    printf("\tName %s not found in attribute map of capsLoads!!!!\n", groupName[groupIndex]);
                    continue;

                } else if (status != CAPS_SUCCESS) {

                    (void)  string_freeArray(numGroupName, &groupName);
                    return status;
                }

                //{UnknownLoad, GridForce, GridMoment, Gravity, Pressure, Rotational, Thermal}
                if (feaProblem->feaLoad[i].loadType == GridForce  ||
                    feaProblem->feaLoad[i].loadType == GridMoment ||
                    feaProblem->feaLoad[i].loadType == Rotational ||
                    feaProblem->feaLoad[i].loadType == Thermal) {

                    // Now lets loop through the grid to see how many grid points have the attrIndex
                    for (nodeIndex = 0; nodeIndex < feaProblem->feaMesh.numNode; nodeIndex++ ) {

                        if (feaProblem->feaMesh.node[nodeIndex].analysisType == MeshStructure) {
                            feaData = (feaMeshDataStruct *) feaProblem->feaMesh.node[nodeIndex].analysisData;
                        } else {
                            continue;
                        }

                        if (feaData->loadIndex == attrIndex) {

                            feaProblem->feaLoad[i].numGridID += 1;

                            // Allocate/Re-allocate grid ID array
                            if (feaProblem->feaLoad[i].numGridID == 1) {

                                feaProblem->feaLoad[i].gridIDSet = (int *) EG_alloc(feaProblem->feaLoad[i].numGridID*sizeof(int));

                            } else {

                                feaProblem->feaLoad[i].gridIDSet = (int *) EG_reall(feaProblem->feaLoad[i].gridIDSet,
                                                                                    feaProblem->feaLoad[i].numGridID*sizeof(int));
                            }

                            if (feaProblem->feaLoad[i].gridIDSet == NULL) {
                                status = string_freeArray(numGroupName, &groupName);
                                if (status == CAPS_SUCCESS) return status;

                                return EGADS_MALLOC;
                            }

                            // Set grid ID value -> 1 bias
                            feaProblem->feaLoad[i].gridIDSet[feaProblem->feaLoad[i].numGridID-1] = feaProblem->feaMesh.node[nodeIndex].nodeID;

                            //printf("GroupName = %s %d\n", groupName[groupIndex], feaProblem->feaMesh.node[nodeIndex].nodeID);

                        }
                    }

                } else if (feaProblem->feaLoad[i].loadType == LineForce ||
                           feaProblem->feaLoad[i].loadType == LineMoment ) {

                  for (elementIndex = 0; elementIndex < feaProblem->feaMesh.numElement; elementIndex++) {

                      if (feaProblem->feaMesh.element[elementIndex].elementType != Line) {
                          continue;
                      }

                      if (feaProblem->feaMesh.element[elementIndex].analysisType == MeshStructure) {
                          feaData = (feaMeshDataStruct *) feaProblem->feaMesh.element[elementIndex].analysisData;
                      } else {
                          continue;
                      }

                      if (feaData->loadIndex == attrIndex) {

                          feaProblem->feaLoad[i].numElementID += 1;

                          feaProblem->feaLoad[i].elementIDSet = (int *) EG_reall(feaProblem->feaLoad[i].elementIDSet,
                                                                                 feaProblem->feaLoad[i].numElementID*sizeof(int));

                          if (feaProblem->feaLoad[i].elementIDSet == NULL) {
                              status = string_freeArray(numGroupName, &groupName);
                              return EGADS_MALLOC;
                          }

                          // Set element ID value -> 1 bias
                          feaProblem->feaLoad[i].elementIDSet[feaProblem->feaLoad[i].numElementID-1] = feaProblem->feaMesh.element[elementIndex].elementID;
                      }
                  }

                } else if (feaProblem->feaLoad[i].loadType == LineForce ||
                           feaProblem->feaLoad[i].loadType == LineMoment ||
                           feaProblem->feaLoad[i].loadType == Pressure ||
                           feaProblem->feaLoad[i].loadType == PressureDistribute) {

                    // Now lets loop through the elements to see how many elements have the attrIndex

                    // Element types - CTRIA3, CTRIA3K, CQUAD4, CQUAD4K
                    for (elementIndex = 0; elementIndex < feaProblem->feaMesh.numElement; elementIndex++) {

                        if (feaProblem->feaMesh.element[elementIndex].elementType != Triangle &&
                            feaProblem->feaMesh.element[elementIndex].elementType != Triangle_6 &&
                            feaProblem->feaMesh.element[elementIndex].elementType != Quadrilateral &&
                            feaProblem->feaMesh.element[elementIndex].elementType != Quadrilateral_8) {
                            continue;
                        }

                        if (feaProblem->feaMesh.element[elementIndex].analysisType == MeshStructure) {
                            feaData = (feaMeshDataStruct *) feaProblem->feaMesh.element[elementIndex].analysisData;
                        } else {
                            continue;
                        }

                        if (feaData->loadIndex == attrIndex) {

                            feaProblem->feaLoad[i].numElementID += 1;

                            // Allocate/Re-allocate element ID array
                            if (feaProblem->feaLoad[i].numElementID == 1) {

                                feaProblem->feaLoad[i].elementIDSet = (int *) EG_alloc(feaProblem->feaLoad[i].numElementID*sizeof(int));

                            } else {

                                feaProblem->feaLoad[i].elementIDSet = (int *) EG_reall(feaProblem->feaLoad[i].elementIDSet,
                                                                                       feaProblem->feaLoad[i].numElementID*sizeof(int));
                            }

                            if (feaProblem->feaLoad[i].elementIDSet == NULL) {
                                status = string_freeArray(numGroupName, &groupName);
                                return EGADS_MALLOC;
                            }

                            // Set element ID value -> 1 bias
                            feaProblem->feaLoad[i].elementIDSet[feaProblem->feaLoad[i].numElementID-1] = feaProblem->feaMesh.element[elementIndex].elementID;
                        }
                    }

                }
            } // End attr search loop

            status = string_freeArray(numGroupName, &groupName);
            if (status != CAPS_SUCCESS) return status;
            groupName = NULL;

            // Free keyValue (just in case)
            if (keyValue != NULL) EG_free(keyValue);
            keyValue = NULL;

            //Fill up load properties

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>loadScaleFactor = 1.0 </B> </li> <br>
             *  Scale factor to use when combining loads.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \elseif HSM
             *
             * \else
             * <ul>
             *  <li> <B>loadScaleFactor = 1.0 </B> </li> <br>
             *  Scale factor to use when combining loads.
             * </ul>
             * \endif
             *
             */
            keyWord = "loadScaleFactor";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].loadScaleFactor);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN || HSM)
             * <ul>
             *  <li> <B>forceScaleFactor = 0.0 </B> </li> <br>
             *  Overall scale factor for the force for a "GridForce" load.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>forceScaleFactor = 0.0 </B> </li> <br>
             *  Overall scale factor for the force for a "GridForce" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "forceScaleFactor";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].forceScaleFactor);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN || HSM)
             * <ul>
             *  <li> <B>directionVector = [0.0, 0.0, 0.0] </B> </li> <br>
             *  X-, y-, and z- components of the force vector for a "GridForce", "GridMoment", or "Gravity" load.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>directionVector = [0.0, 0.0, 0.0] </B> </li> <br>
             *  X-, y-, and z- components of the force vector for a "GridForce", "GridMoment", "Rotational", or "Gravity" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "directionVector";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleArray(keyValue,
                        (int) sizeof(feaProblem->feaLoad[i].directionVector)/sizeof(double),
                        feaProblem->feaLoad[i].directionVector);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN || HSM)
             * <ul>
             *  <li> <B>momentScaleFactor = 0.0 </B> </li> <br>
             *  Overall scale factor for the moment for a "GridMoment" load.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>momentScaleFactor = 0.0 </B> </li> <br>
             *  Overall scale factor for the moment for a "GridMoment" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "momentScaleFactor";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].momentScaleFactor);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN || HSM)
             * <ul>
             *  <li> <B>gravityAcceleration = 0.0 </B> </li> <br>
             *  Acceleration value for a "Gravity" load.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>gravityAcceleration = 0.0 </B> </li> <br>
             *  Acceleration value for a "Gravity" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "gravityAcceleration";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].gravityAcceleration);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if MYSTRAN
             * <ul>
             *  <li> <B>pressureForce = 0.0 </B> </li> <br>
             *  Uniform pressure force for a "Pressure" load (only applicable to 2D elements).
             * </ul>
             * \elseif (NASTRAN || HSM)
             *  <ul>
             *  <li> <B>pressureForce = 0.0 </B> </li> <br>
             *  Uniform pressure force for a "Pressure" load.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>pressureForce = 0.0 </B> </li> <br>
             *  Uniform pressure force for a "Pressure" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "pressureForce";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].pressureForce);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if MYSTRAN
             * <ul>
             *  <li> <B>pressureDistributeForce = [0.0, 0.0, 0.0, 0.0] </B> </li> <br>
             *  Distributed pressure force for a "PressureDistribute" load (only applicable to 2D elements). The four values
             *  correspond to the 4 (quadrilateral elements) or 3 (triangle elements) node locations.
             * </ul>
             * \elseif (NASTRAN || HSM)
             * <ul>
             *  <li> <B>pressureDistributeForce = [0.0, 0.0, 0.0, 0.0] </B> </li> <br>
             *  Distributed pressure force for a "PressureDistribute" load. The four values
             *  correspond to the 4 (quadrilateral elements) or 3 (triangle elements) node locations.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>pressureDistributeForce = [0.0, 0.0, 0.0, 0.0] </B> </li> <br>
             *  Distributed pressure force for a "PressureDistribute" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "pressureDistributeForce";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleArray(keyValue,
                        (int) sizeof(feaProblem->feaLoad[i].pressureDistributeForce)/sizeof(double),
                        feaProblem->feaLoad[i].pressureDistributeForce);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>angularVelScaleFactor = 0.0 </B> </li> <br>
             *  An overall scale factor for the angular velocity in revolutions per unit time for a "Rotational" load.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \elseif HSM
             * <ul>
             *  <li> <B>angularVelScaleFactor = 0.0 </B> </li> <br>
             *  An overall scale factor for the angular velocity in revolutions per unit time for a "Rotational" load - applied in a global sense.
             * </ul>
             * \else
             * <ul>
             *  <li> <B>angularVelScaleFactor = 0.0 </B> </li> <br>
             *  An overall scale factor for the angular velocity in revolutions per unit time for a "Rotational" load .
             * </ul>
             * \endif
             *
             */
            keyWord = "angularVelScaleFactor";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].angularVelScaleFactor);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>angularAccScaleFactor = 0.0 </B> </li> <br>
             *  An overall scale factor for the angular acceleration in revolutions per unit time squared for a "Rotational" load.
             * </ul>
             * \elseif ABAQUS
             *
             *  Something else ....
             *
             * \elseif HSM
             * <ul>
             *  <li> <B>angularAccScaleFactor = 0.0 </B> </li> <br>
             *  An overall scale factor for the angular acceleration in revolutions per unit time squared for a "Rotational" load - applied in a global sense.
             * </ul>
             *
             * \else
             * <ul>
             *  <li> <B>angularAccScaleFactor = 0.0 </B> </li> <br>
             *  An overall scale factor for the angular acceleration in revolutions per unit time squared for a "Rotational" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "angularAccScaleFactor";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].angularAccScaleFactor);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>coordinateSystem = "(no default)" </B> </li> <br>
             *  Name of coordinate system in which defined force components are in reference to. If no value
             *  is provided the global system is assumed.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \elseif HSM
             *
             * \else
             * <ul>
             *  <li> <B>coordinateSystem = "(no default)" </B> </li> <br>
             *  Name of coordinate system in which defined force components are in reference to. If no value
             *  is provided the global system is assumed.
             * </ul>
             * \endif
             *
             */
            keyWord = "coordinateSystem";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                for (attrIndex = 0; attrIndex < feaProblem->numCoordSystem; attrIndex++) {

                    if (strcasecmp(tempString, feaProblem->feaCoordSystem[attrIndex].name) == 0) {
                        feaProblem->feaLoad[i].coordSystemID  = feaProblem->feaCoordSystem[attrIndex].coordSystemID;
                        break;
                    }
                }

                if (feaProblem->feaLoad[i].coordSystemID == 0) {
                    printf("\tCoordinate system %s not found, defaulting to global system!!", keyValue);
                }

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }


            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>temperature = 0.0 </B> </li> <br>
             *  Temperature at give node for a "Temperature" load.
             * </	ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \elseif HSM
             *
             * \else
             * <ul>
             *  <li> <B>temperature = 0.0 </B> </li> <br>
             *  Temperature at give node for a "Temperature" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "temperature";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].temperature);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaLoad
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>temperatureDefault = 0.0 </B> </li> <br>
             *  Default temperature at a node not explicitly being used for a "Temperature" load.
             * </	ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \elseif HSM
             *
             * \else
             * <ul>
             *  <li> <B>temperatureDefault = 0.0 </B> </li> <br>
             *  Default temperature at a node not explicitly being used for a "Temperature" load.
             * </ul>
             * \endif
             *
             */
            keyWord = "temperatureDefault";
            status = search_jsonDictionary( loadTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaLoad[i].temperatureDefault);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }


        } else { // if not a JSON string search

            /*! \page feaLoad
             * \section keyStringLoad Single Value String
             *
             * If "Value" is a string, the string value may correspond to an entry in a predefined load lookup
             * table. NOT YET IMPLEMENTED!!!!
             *
             */

            // Call some look up table maybe?
            printf("\tError: Load tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;
        }
    }

    /// Free keyValue and tempString (just in case)
    if (keyValue != NULL) EG_free(keyValue);

    if (tempString != NULL) EG_free(tempString);

    printf("\tDone getting FEA loads\n");

    return CAPS_SUCCESS;
}

// Get the analysis properties from a capsTuple
int fea_getAnalysis(int numAnalysisTuple,
                    capsTuple analysisTuple[],
                    feaProblemStruct *feaProblem) {

    /*! \page feaAnalysis FEA Analysis
     * Structure for the analysis tuple  = (`Analysis Name', `Value').
     * 'Analysis Name' defines the reference name for the analysis being specified.
     *	The "Value" can either be a JSON String dictionary (see Section \ref jsonStringAnalysis) or a single string keyword
     *	(see Section \ref keyStringAnalysis).
     */

    int status; //Function return

    int i, j, groupIndex, attrIndex; // Indexing


    char *keyValue = NULL;
    char *keyWord = NULL;

    char *tempString = NULL;

    char **groupName = NULL;
    int  numGroupName = 0;

    int tempInt=0; // Temporary integer

    // Destroy our analysis structures coming in if aren't 0 and NULL already
    if (feaProblem->feaAnalysis != NULL) {
        for (i = 0; i < feaProblem->numAnalysis; i++) {
            status = destroy_feaAnalysisStruct(&feaProblem->feaAnalysis[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }

    if (feaProblem->feaAnalysis != NULL) EG_free(feaProblem->feaAnalysis);
    feaProblem->feaAnalysis = NULL;
    feaProblem->numAnalysis = 0;

    printf("\nGetting FEA analyses.......\n");

    feaProblem->numAnalysis = numAnalysisTuple;

    printf("\tNumber of analyses - %d\n", feaProblem->numAnalysis);

    if (feaProblem->numAnalysis > 0) {
        feaProblem->feaAnalysis = (feaAnalysisStruct *) EG_alloc(feaProblem->numAnalysis * sizeof(feaAnalysisStruct));
    } else {
        printf("\tNumber of analysis values in input tuple is 0\n");
        return CAPS_NOTFOUND;
    }

    for (i = 0; i < feaProblem->numAnalysis; i++) {
        status = initiate_feaAnalysisStruct(&feaProblem->feaAnalysis[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    for (i = 0; i < feaProblem->numAnalysis; i++) {

        printf("\tAnalysis name - %s\n", analysisTuple[i].name);

        feaProblem->feaAnalysis[i].name = (char *) EG_alloc(((strlen(analysisTuple[i].name)) + 1)*sizeof(char));
        if (feaProblem->feaAnalysis[i].name == NULL) return EGADS_MALLOC;

        memcpy(feaProblem->feaAnalysis[i].name, analysisTuple[i].name, strlen(analysisTuple[i].name)*sizeof(char));
        feaProblem->feaAnalysis[i].name[strlen(analysisTuple[i].name)] = '\0';

        feaProblem->feaAnalysis[i].analysisID = i + 1;

        // Do we have a json string?
        if (strncmp(analysisTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n", analysisTuple[i].value);

            /*! \page feaAnalysis
             * \section jsonStringAnalysis JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if (MYSTRAN || NASTRAN)
             *  (e.g. "Value" = {"numDesiredEigenvalue": 10, "eigenNormaliztion": "MASS", "numEstEigenvalue": 1,
             * "extractionMethod": "GIV", "frequencyRange": [0, 10000]})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             * \if MYSTRAN
             * <ul>
             *  <li> <B>analysisType = "Modal"</B> </li> <br>
             *  Type of load. Options: "Modal", "Static".
             * </ul>
             * \elseif NASTRAN
             * <ul>
             *  <li> <B>analysisType = "Modal"</B> </li> <br>
             *  Type of load. Options: "Modal", "Static", "AeroelasticTrim", "AeroelasticFlutter"
             *  Note: "AeroelasticStatic" is still supported but refers to "AeroelasticTrim"
             *  Note: "Optimization" and "StaticOpt" are not valid - Optimization is initialized by the Analysis_Type AIM Input
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>analysisType = "Modal"</B> </li> <br>
             *  Type of load. Options: "Modal", "Static", "AeroelasticTrim", "AeroelasticFlutter"
             *  Note: "StaticOpt" is no longer valid - This will default to Static
             *  Note: "Optimization" and "StaticOpt" are not valid - Optimization is initialized by the Analysis_Type AIM Input
             * </ul>
             * \endif
             *
             */

            // Get analysis Type
            keyWord = "analysisType";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{UnknownAnalysis, Modal, Static}
                if      (strcasecmp(keyValue, "\"Modal\"")  == 0) feaProblem->feaAnalysis[i].analysisType = Modal;
                else if (strcasecmp(keyValue, "\"Static\"") == 0) feaProblem->feaAnalysis[i].analysisType = Static;
                else if (strcasecmp(keyValue, "\"StaticOpt\"") == 0) feaProblem->feaAnalysis[i].analysisType = Optimization;
                else if (strcasecmp(keyValue, "\"Optimization\"") == 0) feaProblem->feaAnalysis[i].analysisType = Optimization;
                else if (strcasecmp(keyValue, "\"AeroelasticTrim\"") == 0) feaProblem->feaAnalysis[i].analysisType = AeroelasticTrim;
                else if (strcasecmp(keyValue, "\"AeroelasticStatic\"") == 0) feaProblem->feaAnalysis[i].analysisType = AeroelasticTrim;
                else if (strcasecmp(keyValue, "\"AeroelasticFlutter\"") == 0) feaProblem->feaAnalysis[i].analysisType = AeroelasticFlutter;
                else {

                    printf("\tUnrecognized \"%s\" specified (%s) for Analysis tuple %s, defaulting to \"Modal\"\n", keyWord,
                                                                                                                    keyValue,
                                                                                                                    analysisTuple[i].name);
                    feaProblem->feaAnalysis[i].analysisType = Modal;
                }

            } else {

                printf("\tNo \"%s\" specified for Analysis tuple %s, defaulting to \"Modal\"\n", keyWord, analysisTuple[i].name);

                feaProblem->feaAnalysis[i].analysisType = Modal;
            }

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            // Get loads to be applied for a given analysis

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>analysisLoad = "(no default)"</B> </li> <br>
             *  Single or list of "Load Name"s defined in \ref feaLoad in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>analysisLoad = "(no default)"</B> </li> <br>
             *  Single or list of "Load Name"s defined in \ref feaLoad in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \endif
             *
             */
            keyWord = "analysisLoad";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) return status;

                feaProblem->feaAnalysis[i].numLoad = 0;
                for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                    for (attrIndex = 0; attrIndex < feaProblem->numLoad; attrIndex++) {

                        if (strcasecmp(feaProblem->feaLoad[attrIndex].name, groupName[groupIndex]) == 0) {

                            if (feaProblem->feaLoad[attrIndex].loadType == Thermal) {
                                printf("Combining Thermal loads in a subcase isn't supported yet!\n");
                                status = string_freeArray(numGroupName, &groupName);
                                if (status != CAPS_SUCCESS) return status;
                                groupName = NULL;
                                return CAPS_BADVALUE;
                            }

                            feaProblem->feaAnalysis[i].numLoad += 1;

                            if (feaProblem->feaAnalysis[i].numLoad == 1)  {

                                feaProblem->feaAnalysis[i].loadSetID = (int *) EG_alloc(feaProblem->feaAnalysis[i].numLoad *sizeof(int));

                            } else {

                                feaProblem->feaAnalysis[i].loadSetID = (int *) EG_reall(feaProblem->feaAnalysis[i].loadSetID,
                                                                               feaProblem->feaAnalysis[i].numLoad *sizeof(int));
                            }

                            if (feaProblem->feaAnalysis[i].loadSetID == NULL) {
                                status = string_freeArray(numGroupName, &groupName);
                                if (status != CAPS_SUCCESS) return status;
                                groupName = NULL;
                                return EGADS_MALLOC;
                            }

                            feaProblem->feaAnalysis[i].loadSetID[feaProblem->feaAnalysis[i].numLoad-1] = feaProblem->feaLoad[attrIndex].loadID;
                            break;
                        }
                    }

                    if (feaProblem->feaAnalysis[i].numLoad != groupIndex+1) {

                        printf("\tWarning: Analysis load name, %s, not found in feaLoad structure\n", groupName[groupIndex]);

                    }
                }

                status = string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;
                groupName = NULL;

            }

            // Get constraints to be applied for a given analysis
            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>analysisConstraint = "(no default)"</B> </li> <br>
             *  Single or list of "Constraint Name"s defined in \ref feaConstraint in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>analysisConstraint = "(no default)"</B> </li> <br>
             *  Single or list of "Constraint Name"s defined in \ref feaConstraint in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \endif
             *
             */
            keyWord = "analysisConstraint";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) return status;

                feaProblem->feaAnalysis[i].numConstraint = 0;
                for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                    for (attrIndex = 0; attrIndex < feaProblem->numConstraint; attrIndex++) {

                        if (strcasecmp(feaProblem->feaConstraint[attrIndex].name, groupName[groupIndex]) == 0) {

                            feaProblem->feaAnalysis[i].numConstraint += 1;

                            if (feaProblem->feaAnalysis[i].numConstraint == 1)  {

                                feaProblem->feaAnalysis[i].constraintSetID = (int *) EG_alloc(feaProblem->feaAnalysis[i].numConstraint *sizeof(int));

                            } else {

                                feaProblem->feaAnalysis[i].constraintSetID = (int *) EG_reall(feaProblem->feaAnalysis[i].constraintSetID,
                                                                                              feaProblem->feaAnalysis[i].numConstraint *sizeof(int));
                            }

                            if (feaProblem->feaAnalysis[i].constraintSetID == NULL) {
                                status = string_freeArray(numGroupName, &groupName);
                                if (status != CAPS_SUCCESS) return status;
                                groupName = NULL;
                                return EGADS_MALLOC;
                            }

                            feaProblem->feaAnalysis[i].constraintSetID[feaProblem->feaAnalysis[i].numConstraint-1] = feaProblem->feaConstraint[attrIndex].constraintID;
                            break;
                        }
                    }

                    if (feaProblem->feaAnalysis[i].numConstraint != groupIndex+1) {

                        printf("\tWarning: Analysis constraint name, %s, not found in feaConstraint structure\n", groupName[groupIndex]);

                    }
                }

                status = string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;
                groupName = NULL;

            }

            // Get supports to be applied for a given analysis
            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>analysisSupport = "(no default)"</B> </li> <br>
             *  Single or list of "Support Name"s defined in \ref feaSupport in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>analysisSupport = "(no default)"</B> </li> <br>
             *  Single or list of "Support Name"s defined in \ref feaSupport in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \endif
             *
             */
            keyWord = "analysisSupport";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) return status;

                feaProblem->feaAnalysis[i].numSupport = 0;
                for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                    for (attrIndex = 0; attrIndex < feaProblem->numSupport; attrIndex++) {

                        if (strcasecmp(feaProblem->feaSupport[attrIndex].name, groupName[groupIndex]) == 0) {

                            feaProblem->feaAnalysis[i].numSupport += 1;

                            if (feaProblem->feaAnalysis[i].numSupport == 1)  {

                                feaProblem->feaAnalysis[i].supportSetID = (int *) EG_alloc(feaProblem->feaAnalysis[i].numSupport *sizeof(int));

                            } else {

                                feaProblem->feaAnalysis[i].supportSetID = (int *) EG_reall(feaProblem->feaAnalysis[i].supportSetID,
                                                                                           feaProblem->feaAnalysis[i].numSupport *sizeof(int));
                            }

                            if (feaProblem->feaAnalysis[i].supportSetID == NULL) {
                                status = string_freeArray(numGroupName, &groupName);
                                if (status != CAPS_SUCCESS) return status;
                                groupName = NULL;
                                return EGADS_MALLOC;
                            }

                            feaProblem->feaAnalysis[i].supportSetID[feaProblem->feaAnalysis[i].numSupport-1] = feaProblem->feaSupport[attrIndex].supportID;
                            break;
                        }
                    }

                    if (feaProblem->feaAnalysis[i].numSupport != groupIndex+1) {

                        printf("\tWarning: Analysis support name, %s, not found in feaSupport structure\n", groupName[groupIndex]);

                    }
                }

                status = string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;
                groupName = NULL;

            }

            // Get design constraints to be applied for a given analysis

            /*! \page feaAnalysis
             *
             * \if MYSTRAN
             * \elseif NASTRAN
             * <ul>
             *  <li> <B>analysisDesignConstraint = "(no default)"</B> </li> <br>
             *  Single or list of "Design Constraint Name"s defined in \ref feaDesignConstraint in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>analysisDesignConstraint = "(no default)"</B> </li> <br>
             *  Single or list of "Design Constraint Name"s defined in \ref feaDesignConstraint in which to use for the analysis (e.g. "Name1" or ["Name1","Name2",...].
             * </ul>
             * \endif
             *
             */
            keyWord = "analysisDesignConstraint";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) return status;

                feaProblem->feaAnalysis[i].numDesignConstraint = 0;
                for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                    for (attrIndex = 0; attrIndex < feaProblem->numDesignConstraint; attrIndex++) {

                        if (strcasecmp(feaProblem->feaDesignConstraint[attrIndex].name, groupName[groupIndex]) == 0) {

                            feaProblem->feaAnalysis[i].numDesignConstraint += 1;

                            if (feaProblem->feaAnalysis[i].numDesignConstraint == 1)  {

                                feaProblem->feaAnalysis[i].designConstraintSetID = (int *) EG_alloc(feaProblem->feaAnalysis[i].numDesignConstraint *sizeof(int));

                            } else {

                                feaProblem->feaAnalysis[i].designConstraintSetID = (int *) EG_reall(feaProblem->feaAnalysis[i].designConstraintSetID,
                                                                                                    feaProblem->feaAnalysis[i].numDesignConstraint *sizeof(int));
                            }

                            if (feaProblem->feaAnalysis[i].designConstraintSetID == NULL) {
                                status = string_freeArray(numGroupName, &groupName);
                                if (status != CAPS_SUCCESS) return status;
                                groupName = NULL;
                                return EGADS_MALLOC;
                            }

                            feaProblem->feaAnalysis[i].designConstraintSetID[feaProblem->feaAnalysis[i].numDesignConstraint-1] = feaProblem->feaDesignConstraint[attrIndex].designConstraintID;
                            break;
                        }
                    }

                    if (feaProblem->feaAnalysis[i].numDesignConstraint != groupIndex+1) {

                        printf("\tWarning: Analysis design constraint name, %s, not found in feaDesignConstraint structure\n", groupName[groupIndex]);

                    }
                }

                status = string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) {
                    if (tempString != NULL) EG_free(tempString);

                    return status;
                }
                groupName = NULL;

            }

            //Fill up analysis properties
            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>extractionMethod = "(no default)"</B> </li> <br>
             *  Extraction method for modal analysis.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>extractionMethod = "(no default)"</B> </li> <br>
             *  Extraction method for modal analysis.
             * </ul>
             * \endif
             *
             */
            keyWord = "extractionMethod";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                feaProblem->feaAnalysis[i].extractionMethod = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaAnalysis[i].extractionMethod == NULL) {
                    if (tempString != NULL) EG_free(tempString);
                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaAnalysis[i].extractionMethod, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaAnalysis[i].extractionMethod[strlen(tempString)] = '\0';

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }


            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>frequencyRange = [0.0, 0.0] </B> </li> <br>
             *  Frequency range of interest for modal analysis.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>frequencyRange = [0.0, 0.0] </B> </li> <br>
             *  Frequency range of interest for modal analysis.
             * </ul>
             * \endif
             *
             */
            keyWord = "frequencyRange";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleArray(keyValue,
                                              (int) sizeof(feaProblem->feaAnalysis[i].frequencyRange)/sizeof(double),
                                              feaProblem->feaAnalysis[i].frequencyRange);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>numEstEigenvalue = 0 </B> </li> <br>
             *  Number of estimated eigenvalues for modal analysis.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>numEstEigenvalue = 0 </B> </li> <br>
             *  Number of estimated eigenvalues for modal analysis.
             * </ul>
             * \endif
             *
             */
            keyWord = "numEstEigenvalue";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaAnalysis[i].numEstEigenvalue);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>numDesiredEigenvalue = 0</B> </li> <br>
             *  Number of desired eigenvalues for modal analysis.
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>numDesiredEigenvalue = 0</B> </li> <br>
             *  Number of desired eigenvalues for modal analysis.
             * </ul>
             * \endif
             *
             */
            keyWord = "numDesiredEigenvalue";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaAnalysis[i].numDesiredEigenvalue);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>eigenNormaliztion = "(no default)"</B> </li> <br>
             *  Method of eigenvector renormilization. Options: "POINT", "MAX", "MASS"
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>eigenNormaliztion = "(no default)"</B> </li> <br>
             *  Method of eigenvector renormilization.
             * </ul>
             * \endif
             *
             */
            keyWord = "eigenNormaliztion";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaAnalysis[i].eigenNormaliztion = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaAnalysis[i].eigenNormaliztion == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaAnalysis[i].eigenNormaliztion, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaAnalysis[i].eigenNormaliztion[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>gridNormaliztion = 0 </B> </li> <br>
             *  Grid point to be used in normalizing eigenvector to 1.0 when using eigenNormaliztion = "POINT"
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>gridNormaliztion = 0</B> </li> <br>
             *  Grid point to be used in normalizing eigenvector to 1.0 when using eigenNormaliztion = "POINT"
             * </ul>
             * \endif
             *
             */
            keyWord = "gridNormaliztion";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaAnalysis[i].gridNormaliztion);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>componentNormaliztion = 0</B> </li> <br>
             *  Degree of freedom about "gridNormalization" to be used in normalizing eigenvector to 1.0
             *  when using eigenNormaliztion = "POINT"
             * </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>componentNormaliztion = 0</B> </li> <br>
             *  Degree of freedom about "gridNormalization" to be used in normalizing eigenvector to 1.0
             *  when using eigenNormaliztion = "POINT"
             * </ul>
             * \endif
             *
             */
            keyWord = "componentNormaliztion";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaAnalysis[i].componentNormaliztion);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>lanczosMode = 2</B> </li> <br>
             *  Mode refers to the Lanczos mode type to be used in the solution. In mode 3 the mass matrix, Maa,must
             *  be nonsingular whereas in mode 2 the matrix K aa - sigma*Maa must be nonsingular
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>lanczosMode = 2</B> </li> <br>
             *  Mode refers to the Lanczos mode type to be used in the solution. In mode 3 the mass matrix, Maa,must
             *  be nonsingular whereas in mode 2 the matrix K aa - sigma*Maa must be nonsingular
             *  </ul>
             * \endif
             */
            keyWord = "lanczosMode";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaAnalysis[i].lanczosMode);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>lanczosType = "(no default)"</B> </li> <br>
             *  Lanczos matrix type. Options: DPB, DGB.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>lanczosType = "(no default)"</B> </li> <br>
             *  Lanczos matrix type. Options: DPB, DGB.
             *  </ul>
             * \endif
             */
            keyWord = "lanczosType";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaAnalysis[i].lanczosType = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaAnalysis[i].lanczosType == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaAnalysis[i].lanczosType, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaAnalysis[i].lanczosType[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN)
             * \elseif (NASTRAN)
             * <ul>
             *  <li> <B>machNumber = 0.0 or [0.0, ..., 0.0]</B> </li> <br>
             *  Mach number used in trim analysis OR Mach numbers used in flutter analysis..
             *  </ul>
             * \elseif (ASTROS)
             * <ul>
             *  <li> <B>machNumber = 0.0 or [0.0, ..., 0.0]</B> </li> <br>
             *  Mach number used in trim analysis OR Mach up to 6 values used in flutter analysis..
             *  </ul>
             * \elseif ABAQUS
             * \else
             * <ul>
             *  <li> <B>machNumber = 0.0</B> </li> <br>
             *  Mach number used in trim analysis.
             *  </ul>
             * \endif
             */
            keyWord = "machNumber";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleDynamicArray(keyValue, &feaProblem->feaAnalysis[i].numMachNumber, &feaProblem->feaAnalysis[i].machNumber);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN)
             * \elseif (NASTRAN)
             * <ul>
             *  <li> <B>dynamicPressure = 0.0</B> </li> <br>
             *  Dynamic pressure used in trim analysis.
             *  </ul>
             * \elseif ABAQUS
             * \else
             * <ul>
             *  <li> <B>dynamicPressure = 0.0</B> </li> <br>
             *  Dynamic pressure used in trim analysis.
             *  </ul>
             * \endif
             */
            keyWord = "dynamicPressure";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaAnalysis[i].dynamicPressure);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN)
             * \elseif (NASTRAN)
             * <ul>
             *  <li> <B>density = 0.0</B> </li> <br>
             *  Density used in trim analysis to determine true velocity, or flutter analysis.
             *  </ul>
             * \elseif ABAQUS
             * \else
             * <ul>
             *  <li> <B>density = 0.0</B> </li> <br>
             *  Density used in trim analysis to determine true velocity, or flutter analysis.
             *  </ul>
             * \endif
             */
            keyWord = "density";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaAnalysis[i].density);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>aeroSymmetryXY = "(no default)"</B> </li> <br>
             *  Aerodynamic symmetry about the XY Plane. Options: SYM, ANTISYM, ASYM.
             *  Aerodynamic symmetry about the XY Plane. Options: SYM, ANTISYM, ASYM. SYMMETRIC Indicates that a half span aerodynamic model is moving in a symmetric manner with respect to the XY plane.
             *  ANTISYMMETRIC Indicates that a half span aerodynamic model is moving in an antisymmetric manner with respect to the XY plane.
             *  ASYMMETRIC Indicates that a full aerodynamic model is provided.
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>aeroSymmetryXY = "(no default)"</B> </li> <br>
             *  Aerodynamic symmetry about the XY Plane. Options: SYM, ANTISYM, ASYM.
             *  </ul>
             * \endif
             */
            keyWord = "aeroSymmetryXY";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaAnalysis[i].aeroSymmetryXY = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaAnalysis[i].aeroSymmetryXY == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaAnalysis[i].aeroSymmetryXY, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaAnalysis[i].aeroSymmetryXY[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            // check for the old option trimSymmetry
            keyWord = "trimSymmetry";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaAnalysis[i].aeroSymmetryXY = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaAnalysis[i].aeroSymmetryXY == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaAnalysis[i].aeroSymmetryXY, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaAnalysis[i].aeroSymmetryXY[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN || NASTRAN)
             * <ul>
             *  <li> <B>aeroSymmetryXZ = "(no default)"</B> </li> <br>
             *  Aerodynamic symmetry about the XZ Plane. Options: SYM, ANTISYM, ASYM. SYMMETRIC Indicates that a half span aerodynamic model is moving in a symmetric manner with respect to the XZ plane.
             *  ANTISYMMETRIC Indicates that a half span aerodynamic model is moving in an antisymmetric manner with respect to the XZ plane.
             *  ASYMMETRIC Indicates that a full aerodynamic model is provided.
             *
             *  </ul>
             * \elseif ABAQUS
             *
             * Something else ....
             *
             * \else
             * <ul>
             *  <li> <B>aeroSymmetryXZ = "(no default)"</B> </li> <br>
             *  Aerodynamic symmetry about the XZ Plane. Options: SYM, ANTISYM, ASYM.
             *  </ul>
             * \endif
             */
            keyWord = "aeroSymmetryXZ";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaAnalysis[i].aeroSymmetryXZ = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaAnalysis[i].aeroSymmetryXZ == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaAnalysis[i].aeroSymmetryXZ, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaAnalysis[i].aeroSymmetryXZ[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN)
             * \elseif (NASTRAN)
             * <ul>
             *  <li> <B>rigidVariable = ["no default"]</B> </li> <br>
             *  List of rigid body motions to be used as trim variables during a trim analysis. Nastran
             *  valid labels are: ANGLEA, SIDES, ROLL, PITCH, YAW, URDD1, URDD2, URDD3, URDD4, URDD5, URDD6
             *  </ul>
             * \elseif ABAQUS
             * \else
             * <ul>
             *  <li> <B>rigidVariable = ["no default"]</B> </li> <br>
             *  List of rigid body motions to be used as trim variables during a trim analysis. Nastran formant
             *  labels are used and will be converted by the AIM automatically.
             *  Expected inputs: ANGLEA, SIDES, ROLL, PITCH, YAW, URDD1, URDD2, URDD3, URDD4, URDD5, URDD6
             *  </ul>
             * \endif
             */
            keyWord = "rigidVariable";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue,
                        &feaProblem->feaAnalysis[i].numRigidVariable,
                        &feaProblem->feaAnalysis[i].rigidVariable);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN)
             * \elseif (NASTRAN)
             * <ul>
             *  <li> <B>rigidConstraint = ["no default"]</B> </li> <br>
             *  List of rigid body motions to be used as trim constraint variables during a trim analysis. Nastran
             *  valid labels are: ANGLEA, SIDES, ROLL, PITCH, YAW, URDD1, URDD2, URDD3, URDD4, URDD5, URDD6
             *  </ul>
             * \elseif ABAQUS
             * \else
             * <ul>
             *  <li> <B>rigidConstraint = ["no default"]</B> </li> <br>
             *  List of rigid body motions to be used as trim constraint variables during a trim analysis.
             *  </ul>
             * \endif
             */
            keyWord = "rigidConstraint";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue,
                        &feaProblem->feaAnalysis[i].numRigidConstraint,
                        &feaProblem->feaAnalysis[i].rigidConstraint);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            if (feaProblem->feaAnalysis[i].numRigidConstraint != 0) {
                /*! \page feaAnalysis
                 *
                 * \if (MYSTRAN)
                 * \elseif (NASTRAN)
                 * <ul>
                 *  <li> <B>magRigidConstraint = [0.0 , 0.0, ...]</B> </li> <br>
                 *  List of magnitudes of trim constraint variables. If none and 'rigidConstraint'(s) are specified
                 *  then 0.0 is assumed for each rigid constraint.
                 *  </ul>
                 * \elseif ABAQUS
                 * \else
                 * <ul>
                 *  <li> <B>magRigidConstraint = [0.0 , 0.0, ...]</B> </li> <br>
                 *  List of magnitudes of trim constraint variables. If none and 'rigidConstraint'(s) are specified
                 *  then 0.0 is assumed for each rigid constraint.
                 *  </ul>
                 * \endif
                 */
                keyWord = "magRigidConstraint";
                status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
                if (status == CAPS_SUCCESS) {

                    status = string_toDoubleDynamicArray(keyValue,
                            &tempInt,
                            &feaProblem->feaAnalysis[i].magRigidConstraint);
                    if (keyValue != NULL) {
                        EG_free(keyValue);
                        keyValue = NULL;
                    }
                    if (status != CAPS_SUCCESS) return status;
                } else {

                    tempInt = feaProblem->feaAnalysis[i].numRigidConstraint;

                    feaProblem->feaAnalysis[i].magRigidConstraint = (double *) EG_alloc(tempInt*sizeof(double));
                    if (feaProblem->feaAnalysis[i].magRigidConstraint == NULL) return EGADS_MALLOC;

                    for (j = 0; j < feaProblem->feaAnalysis[i].numRigidConstraint; j++) {

                        feaProblem->feaAnalysis[i].magRigidConstraint[j] = 0.0;
                    }
                }

                if (tempInt != feaProblem->feaAnalysis[i].numRigidConstraint) {
                    printf("\tDimensional mismatch between 'magRigidConstraint' and 'rigidConstraint'.\n");
                    printf("\t 'magRigidConstraint' will be resized.\n");

                    feaProblem->feaAnalysis[i].magRigidConstraint = (double *) EG_reall(feaProblem->feaAnalysis[i].magRigidConstraint,
                            feaProblem->feaAnalysis[i].numRigidConstraint*sizeof(double));

                    if (feaProblem->feaAnalysis[i].magRigidConstraint == NULL) return EGADS_MALLOC;

                    for (j = tempInt; j < feaProblem->feaAnalysis[i].numRigidConstraint; j++) {
                        feaProblem->feaAnalysis[i].magRigidConstraint[j] = 0.0;
                    }
                }
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN)
             * \elseif (NASTRAN)
             * <ul>
             *  <li> <B>controlConstraint = ["no default"]</B> </li> <br>
             *  List of controls surfaces to be used as trim constraint variables during a trim analysis.
             *  </ul>
             * \elseif ABAQUS
             * \else
             * <ul>
             *  <li> <B>controlConstraint = ["no default"]</B> </li> <br>
             *  List of controls surfaces to be used as trim constraint variables during a trim analysis.
             *  </ul>
             * \endif
             */
            keyWord = "controlConstraint";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue,
                        &feaProblem->feaAnalysis[i].numControlConstraint,
                        &feaProblem->feaAnalysis[i].controlConstraint);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            if (feaProblem->feaAnalysis[i].numControlConstraint != 0) {
                /*! \page feaAnalysis
                 *
                 * \if (MYSTRAN)
                 * \elseif (NASTRAN)
                 * <ul>
                 *  <li> <B>magControlConstraint = [0.0 , 0.0, ...]</B> </li> <br>
                 *  List of magnitudes of trim control surface constraint variables. If none and 'controlConstraint'(s) are specified
                 *   then 0.0 is assumed for each control surface constraint.
                 *  </ul>
                 * \elseif ABAQUS
                 * \else
                 * <ul>
                 *  <li> <B>magControlConstraint = [0.0 , 0.0, ...]</B> </li> <br>
                 *  List of magnitudes of trim control surface constraint variables. If none and 'controlConstraint'(s) are specified
                 *   then 0.0 is assumed for each control surface constraint.
                 *  </ul>
                 * \endif
                 */
                keyWord = "magControlConstraint";
                status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
                if (status == CAPS_SUCCESS) {

                    status = string_toDoubleDynamicArray(keyValue,
                            &tempInt,
                            &feaProblem->feaAnalysis[i].magControlConstraint);
                    if (keyValue != NULL) {
                        EG_free(keyValue);
                        keyValue = NULL;
                    }
                    if (status != CAPS_SUCCESS) return status;
                } else {

                    tempInt = feaProblem->feaAnalysis[i].numControlConstraint;

                    feaProblem->feaAnalysis[i].magControlConstraint = (double *) EG_alloc(tempInt*sizeof(double));
                    if (feaProblem->feaAnalysis[i].magControlConstraint == NULL) return EGADS_MALLOC;

                    for (j = 0; j < feaProblem->feaAnalysis[i].numControlConstraint; j++) {

                        feaProblem->feaAnalysis[i].magControlConstraint[j] = 0.0;
                    }
                }

                if (tempInt != feaProblem->feaAnalysis[i].numControlConstraint) {
                    printf("\tDimensional mismatch between 'magControlConstraint' and 'controlConstraint'.\n");
                    printf("\t 'magControlConstraint' will be resized.\n");

                    feaProblem->feaAnalysis[i].magControlConstraint = (double *) EG_reall(feaProblem->feaAnalysis[i].magControlConstraint,
                            feaProblem->feaAnalysis[i].numControlConstraint*sizeof(double));

                    if (feaProblem->feaAnalysis[i].magControlConstraint == NULL) return EGADS_MALLOC;

                    for (j = tempInt; j < feaProblem->feaAnalysis[i].numControlConstraint; j++) {
                        feaProblem->feaAnalysis[i].magControlConstraint[j] = 0.0;
                    }
                }
            }

            /*! \page feaAnalysis
             *
             * \if (MYSTRAN)
             * \elseif (NASTRAN)
             * <ul>
             *  <li> <B>reducedFreq = [0.1, ..., 20.0], No Default Values are defined.</B> </li> <br>
             *  Reduced Frequencies to be used in Flutter Analysis.  Up to 8 values can be defined.
             *  </ul>
             * \elseif ABAQUS
             * \else
             * <ul>
             *  <li> <B>reducedFreq = [0.1, ..., 20.0], No Default Values are defined.</B> </li> <br>
             *  Reduced Frequencies to be used in Flutter Analysis.  Up to 8 values can be defined.
             *  </ul>
             * \endif
             */
            keyWord = "reducedFreq";
            status = search_jsonDictionary( analysisTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleDynamicArray(keyValue, &feaProblem->feaAnalysis[i].numReducedFreq, &feaProblem->feaAnalysis[i].reducedFreq);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (feaProblem->feaAnalysis[i].numReducedFreq > 8) {
                    printf("\tError: The number of reduced frequencies (reducedFreq) entered in an Analysis AIM Input must be eight or less\n");
                    return CAPS_BADVALUE;
                }
                if (status != CAPS_SUCCESS) return status;
            }

        } else { // Not JSONstring

            /*! \page feaAnalysis
             * \section keyStringAnalysis Single Value String
             *
             * If "Value" is a string, the string value may correspond to an entry in a predefined analysis lookup
             * table. NOT YET IMPLEMENTED!!!!
             *
             *
             */

            // CALL analysis look up
            printf("\tError: Analysis tuple value is expected to be a JSON string\n");
            //printf("\t%s", analysisTuple[i].value);
            return CAPS_BADVALUE;
        }
    }

    if (keyValue != NULL) {
        EG_free(keyValue);
        keyValue = NULL;
    }

    printf("\tDone getting FEA analyses\n");
    return CAPS_SUCCESS;
}

// Get the design variables from a capsTuple
int fea_getDesignVariable(int numDesignVariableTuple,
                          capsTuple designVariableTuple[],
                          feaProblemStruct *feaProblem) {

    /*! \page feaDesignVariable FEA DesignVariable
     * Structure for the design variable tuple  = ("DesignVariable Name", "Value").
     * "DesignVariable Name" defines the reference name for the design variable being specified.
     *  This string will be used in the FEA input directly. The "Value" must be a JSON String dictionary
     *  (see Section \ref jsonStringDesignVariable).
     *  \if NASTRAN
     *  In Nastran the DesignVariable Name will be the LABEL used in the DESVAR input.
     *  For this reason the user should keep the length of this input to a minimum number of characters, ideally 7 or less.
     *
     *  - <c>DESVAR  ID      LABEL   XINIT   XLB     XUB     DELXV   DDVAL</c>
     *  \endif
     *
     */
    int status; //Function return

    int i, j, k, groupIndex, attrIndex; // Indexing
    int found;

    char *keyValue = NULL;
    char *keyWord = NULL;

    char *tempString = NULL;

    int tempInteger = 0;

    char **groupName = NULL;
    int  numGroupName = 0;

    // Destroy our design variables structures coming in if aren't 0 and NULL already
    if (feaProblem->feaDesignVariable != NULL) {
        for (i = 0; i < feaProblem->numDesignVariable; i++) {
            status = destroy_feaDesignVariableStruct(&feaProblem->feaDesignVariable[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (feaProblem->feaDesignVariable != NULL) EG_free(feaProblem->feaDesignVariable);
    feaProblem->feaDesignVariable = NULL;
    feaProblem->numDesignVariable = 0;

    printf("\nGetting FEA design variables.......\n");

    feaProblem->numDesignVariable = numDesignVariableTuple;

    printf("\tNumber of design variables - %d\n", feaProblem->numDesignVariable);

    if (feaProblem->numDesignVariable > 0) {
        feaProblem->feaDesignVariable = (feaDesignVariableStruct *) EG_alloc(feaProblem->numDesignVariable * sizeof(feaDesignVariableStruct));
    } else {
        printf("\tNumber of design variable values in input tuple is 0\n");
        return CAPS_NOTFOUND;
    }

    for (i = 0; i < feaProblem->numDesignVariable; i++) {
        status = initiate_feaDesignVariableStruct(&feaProblem->feaDesignVariable[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    for (i = 0; i < feaProblem->numDesignVariable; i++) {

        printf("\tDesign_Variable name - %s\n", designVariableTuple[i].name);

        feaProblem->feaDesignVariable[i].name = (char *) EG_alloc(((strlen(designVariableTuple[i].name)) + 1)*sizeof(char));
        if (feaProblem->feaDesignVariable[i].name == NULL) return EGADS_MALLOC;

        memcpy(feaProblem->feaDesignVariable[i].name, designVariableTuple[i].name, strlen(designVariableTuple[i].name)*sizeof(char));
        feaProblem->feaDesignVariable[i].name[strlen(designVariableTuple[i].name)] = '\0';

        feaProblem->feaDesignVariable[i].designVariableID = i + 1;

        // Do we have a json string?
        if (strncmp(designVariableTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n", designVariableTuple[i].value);

            /*! \page feaDesignVariable
             * \section jsonStringDesignVariable JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if NASTRAN
             *  (eg. "Value" = {"designVariableType": "Property", "groupName": "plate", "upperBound": 10.0, "fieldName": "TM"})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>designVariableType = "Property"</B> </li> <br>
             *  Type of design variable in an optimization problem. Options: "Material", "Property".
             * </ul>
             * \endif
             *
             */

            // Get designVariable Type
            keyWord = "designVariableType";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                //{UnknownDesignVariable, Modal, Static}
                if      (strcasecmp(keyValue, "\"Material\"")  == 0) feaProblem->feaDesignVariable[i].designVariableType = MaterialDesignVar;
                else if (strcasecmp(keyValue, "\"Property\"")  == 0) feaProblem->feaDesignVariable[i].designVariableType = PropertyDesignVar;
                else {

                    printf("\tUnrecognized \"%s\" specified (%s) for Design_Variable tuple %s, defaulting to \"Property\"\n", keyWord,
                                                                                                                              keyValue,
                                                                                                                              designVariableTuple[i].name);
                    feaProblem->feaDesignVariable[i].designVariableType = PropertyDesignVar;
                }

            } else {

                printf("\tNo \"%s\" specified for Design_Variable tuple %s, defaulting to \"Property\"\n", keyWord,
                                                                                                           designVariableTuple[i].name);

                feaProblem->feaDesignVariable[i].designVariableType = PropertyDesignVar;
            }

            if (keyValue != NULL) {
                EG_free(keyValue);
                keyValue = NULL;
            }

            // Get material/properties that the design variable should be applied to
            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsGroup</c> or FEA Material name(s)
             *  to the design variable (e.g. "Name1" or ["Name1","Name2",...].
             *  - For <c>designVariableType</c> Property a <c>capsGroup</c> Name (or names) is given. The property (see \ref feaProperty) also
             *  assigned to the same <c>capsGroup</c> will be automatically related to this design variable entry.
             *  - For <c>designVariableType</c> Material a \ref feaMaterial name (or names) is given.
             * </ul>
             * \endif
             *
             */
            keyWord = "groupName";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) return status;

                if (feaProblem->feaDesignVariable[i].designVariableType == MaterialDesignVar) {

                    feaProblem->feaDesignVariable[i].numMaterialID = 0;
                    for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                        for (attrIndex = 0; attrIndex < feaProblem->numMaterial; attrIndex++) {

                            if (strcasecmp(feaProblem->feaMaterial[attrIndex].name, groupName[groupIndex]) == 0) {

                                feaProblem->feaDesignVariable[i].numMaterialID += 1;

                                if (feaProblem->feaDesignVariable[i].numMaterialID == 1)  {

                                    feaProblem->feaDesignVariable[i].materialSetID   = (int *) EG_alloc(feaProblem->feaDesignVariable[i].numMaterialID *sizeof(int));
                                    feaProblem->feaDesignVariable[i].materialSetType = (int *) EG_alloc(feaProblem->feaDesignVariable[i].numMaterialID *sizeof(int));

                                } else {

                                    feaProblem->feaDesignVariable[i].materialSetID   = (int *) EG_reall(feaProblem->feaDesignVariable[i].materialSetID,
                                                                                                        feaProblem->feaDesignVariable[i].numMaterialID *sizeof(int));
                                    feaProblem->feaDesignVariable[i].materialSetType = (int *) EG_reall(feaProblem->feaDesignVariable[i].materialSetType,
                                                                                                        feaProblem->feaDesignVariable[i].numMaterialID *sizeof(int));

                                }

                                if (feaProblem->feaDesignVariable[i].materialSetID   == NULL ||
                                    feaProblem->feaDesignVariable[i].materialSetType == NULL   ) {

                                    status = string_freeArray(numGroupName, &groupName);
                                    if (status != CAPS_SUCCESS) return status;
                                    groupName = NULL;
                                    return EGADS_MALLOC;
                                }

                                feaProblem->feaDesignVariable[i].materialSetID[feaProblem->feaDesignVariable[i].numMaterialID-1] = feaProblem->feaMaterial[attrIndex].materialID;
                                feaProblem->feaDesignVariable[i].materialSetType[feaProblem->feaDesignVariable[i].numMaterialID-1] = feaProblem->feaMaterial[attrIndex].materialType;

                                break;
                            }
                        }

                        if (feaProblem->feaDesignVariable[i].numMaterialID != groupIndex+1) {

                            printf("\tWarning: DesignVariable material name, %s, not found in feaMaterial structure\n", groupName[groupIndex]);

                        }
                    }
                } else if (feaProblem->feaDesignVariable[i].designVariableType == PropertyDesignVar) {

                    feaProblem->feaDesignVariable[i].numPropertyID = 0;
                    for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                        for (attrIndex = 0; attrIndex < feaProblem->numProperty; attrIndex++) {

                            if (strcasecmp(feaProblem->feaProperty[attrIndex].name, groupName[groupIndex]) == 0) {

                                feaProblem->feaDesignVariable[i].numPropertyID += 1;

                                if (feaProblem->feaDesignVariable[i].numPropertyID == 1)  {

                                    feaProblem->feaDesignVariable[i].propertySetID   = (int *) EG_alloc(feaProblem->feaDesignVariable[i].numPropertyID *sizeof(int));
                                    feaProblem->feaDesignVariable[i].propertySetType = (int *) EG_alloc(feaProblem->feaDesignVariable[i].numPropertyID *sizeof(int));

                                } else {

                                    feaProblem->feaDesignVariable[i].propertySetID = (int *) EG_reall(feaProblem->feaDesignVariable[i].propertySetID,
                                                                                                      feaProblem->feaDesignVariable[i].numPropertyID *sizeof(int));

                                    feaProblem->feaDesignVariable[i].propertySetType = (int *) EG_reall(feaProblem->feaDesignVariable[i].propertySetType,
                                                                                                        feaProblem->feaDesignVariable[i].numPropertyID *sizeof(int));

                                }

                                if (feaProblem->feaDesignVariable[i].propertySetID   == NULL ||
                                    feaProblem->feaDesignVariable[i].propertySetType == NULL   ) {

                                    status = string_freeArray(numGroupName, &groupName);
                                    if (status != CAPS_SUCCESS) return status;
                                    groupName = NULL;
                                    return EGADS_MALLOC;
                                }

                                feaProblem->feaDesignVariable[i].propertySetID  [feaProblem->feaDesignVariable[i].numPropertyID-1] = feaProblem->feaProperty[attrIndex].propertyID;
                                feaProblem->feaDesignVariable[i].propertySetType[feaProblem->feaDesignVariable[i].numPropertyID-1] = feaProblem->feaProperty[attrIndex].propertyType;
                                break;
                            }
                        }

                        if (feaProblem->feaDesignVariable[i].numPropertyID != groupIndex+1) {

                            printf("\tWarning: DesignVariable property name, %s, not found in feaProperty structure\n", groupName[groupIndex]);

                        }
                    }
                }

                status = string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;
                groupName = NULL;
            }

            //Fill up designVariable properties

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>initialValue = 0.0</B> </li> <br>
             *  Initial value for the design variable.
             * </ul>
             * \endif
             *
             */
            keyWord = "initialValue";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaDesignVariable[i].initialValue);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>lowerBound = 0.0</B> </li> <br>
             *  Lower bound for the design variable.
             * </ul>
             * \endif
             */
            keyWord = "lowerBound";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaDesignVariable[i].lowerBound);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>upperBound = 0.0</B> </li> <br>
             *  Upper bound for the design variable.
             * </ul>
             * \endif
             */
            keyWord = "upperBound";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaDesignVariable[i].upperBound);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>maxDelta = 0.0</B> </li> <br>
             *  Move limit for the design variable.
             * </ul>
             * \endif
             */
            keyWord = "maxDelta";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaDesignVariable[i].maxDelta);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>discreteValue = 0.0</B> </li> <br>
             *  List of discrete values do use for the design variable (e.g. [0.0,1.0,1.5,3.0].
             * </ul>
             * \endif
             */
            keyWord = "discreteValue";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleDynamicArray(keyValue,
                                                     &feaProblem->feaDesignVariable[i].numDiscreteValue,
                                                     &feaProblem->feaDesignVariable[i].discreteValue);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) {
                    feaProblem->feaDesignVariable[i].numDiscreteValue = 0;
                    feaProblem->feaDesignVariable[i].discreteValue = NULL;
                    return status;
                }
            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>fieldName = "(no default)"</B> </li> <br>
             *  Fieldname of variable (e.g. "E" for Young's Modulus). Design Variables can be defined as two types based on the <c>designVariableType</c> value.
             *  These are Material or Property.  This means that an aspect of a material or property input can change in the optimization problem.  This input
             *  specifies what aspect of the Material or Property is changing.
             *  -# <b> Material Types</b> Selected based on the material type (see \ref feaMaterial, materialType) referenced  in the <c>groupName</c> above.
             *  	- <c><b> MAT1</b>, 	materialType = "Isotropic" </c>
             *  		- "E", "G", "NU", "RHO", "A"
             *  	- <c><b>  MAT2</b>, materialType = "Anisothotropic"  </c>
             *  		- "G11", "G12", "G13", "G22", "G23", "G33", "RHO", "A1", "A2", "A3"
             *  	- <c><b>  MAT8</b>, materialType = "Orthotropic"  </c>
             *  	    - "E1", "E2", "NU12", "G12", "G1Z", "G2Z", "RHO", "A1", "A2"
             *  	- <c><b>  MAT9</b>, materialType = "Anisotropic"  </c>
             *  	    - "G11", "G12", "G13", "G14", "G15", "G16"
             *  	    - "G22", "G23", "G24", "G25", "G26"
             *  	    - "G33", "G34", "G35", "G36"
             *  	    - "G44", "G45", "G46"
             *  	    - "G55", "G56", "G66"
             *  	    - "RHO", "A1", "A2", "A3", "A4", "A5", "A6"
             *  -# <b> Property Types</b> (see \ref feaProperty)
             *  	- <c><b> PROD</b> </c> 	<c>propertyType = "Rod"</c>
             *  		- "A", "J"
             *  	- <c><b> PBAR</b> </c> 	<c>propertyType = "Bar"</c>
             *  		- "A", "I1", "I2", "J"
             *  	- <c><b> PSHELL</b> </c><c>propertyType = "Shell"</c>
             *  		- "T"
             *  	- <c><b> PCOMP</b> </c> <c>propertyType = "Composite"</c>
             *  		- "T1", "THETA1", "T2", "THETA2", ... "Ti", "THETAi"
             *  	- <c><b> PSOLID</b> </c><c>propertyType = "Solid"</c>
             *  		- not supported
             * </ul>
             * \endif
             */

            keyWord = "fieldName";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaDesignVariable[i].fieldName = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaDesignVariable[i].fieldName == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaDesignVariable[i].fieldName, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaDesignVariable[i].fieldName[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>fieldPosition = 0</B> </li> <br>
             *  This input is ignored if not defined.  The user may use this field instead of the <c>fieldName</c> input defined above to
             *  relate design variables and property inputs.  This requires knowledge of Nastran bulk data input format for material and
             *   property input cards.
             * </ul>
             * \endif
             */

            keyWord = "fieldPosition";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaDesignVariable[i].fieldPosition);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>independentVariable = "(no default)"</B> </li> <br>
             *  Single or list of "DesignVariable Name"s  (that is the Tuple name) used to create/designate a
             *  dependent design variable.
             *  - independentValue = variableWeight[1] + variableWeight[2] * SUM{independentVariableWeight[i] * independentVariable[i]}
             * </ul>
             * \endif
             */

            keyWord = "independentVariable";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue,
                        &feaProblem->feaDesignVariable[i].numIndependVariable,
                        &feaProblem->feaDesignVariable[i].independVariable);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;

            }

            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>independentVariableWeight = 1.0 or [1.0, 1.0, ...]</B> </li> <br>
             *  Single or list of weighting constants with respect to the variables set for "independentVariable".
             *  If the length of this list doesn't match the length
             *  of the "independentVariable" list, the list is either truncated [ >length("independentVariable")] or expanded [ <length("independentVariable")]
             *  in which case the <b>last weight is repeated</b>.
             * </ul>
             * \endif
             */

            keyWord = "independentVariableWeight";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleDynamicArray(keyValue,
                        &tempInteger,
                        &feaProblem->feaDesignVariable[i].independVariableWeight);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) return status;

                // We have weights, but no variables
                if (feaProblem->feaDesignVariable[i].numIndependVariable == 0) {

                    printf("\tWeighting constants have been provided, but no independent design variables were set!\n");

                    // Less weights than variables
                } else if( tempInteger < feaProblem->feaDesignVariable[i].numIndependVariable) {

                    printf("\tThe number of weighting constants provided does not match the number of independent design variables. "
                            "The last weight will be repeated %d times\n", feaProblem->feaDesignVariable[i].numIndependVariable - tempInteger);

                    feaProblem->feaDesignVariable[i].independVariableWeight = (double *) EG_reall(feaProblem->feaDesignVariable[i].independVariableWeight,
                            feaProblem->feaDesignVariable[i].numIndependVariable*sizeof(double));

                    if (feaProblem->feaDesignVariable[i].independVariableWeight == NULL ) return EGADS_MALLOC;

                    for (j = 0; j < feaProblem->feaDesignVariable[i].numIndependVariable - tempInteger; j++) {

                        feaProblem->feaDesignVariable[i].independVariableWeight[j+tempInteger] = feaProblem->feaDesignVariable[i].independVariableWeight[tempInteger-1];
                    }

                    // More weights than variables
                } else if (tempInteger > feaProblem->feaDesignVariable[i].numIndependVariable) {

                    printf("\tThe number of weighting constants provided does not match the number of independent design variables. "
                            "The last %d weights will be not be used\n", tempInteger -feaProblem->feaDesignVariable[i].numIndependVariable);

                    feaProblem->feaDesignVariable[i].independVariableWeight = (double *) EG_reall(feaProblem->feaDesignVariable[i].independVariableWeight,
                            feaProblem->feaDesignVariable[i].numIndependVariable*sizeof(double));

                    if (feaProblem->feaDesignVariable[i].independVariableWeight == NULL ) return EGADS_MALLOC;
                }

            } else { // No weights provided - set default value of 1.0

                if (feaProblem->feaDesignVariable[i].numIndependVariable != 0) {
                    feaProblem->feaDesignVariable[i].independVariableWeight = (double *) EG_alloc(feaProblem->feaDesignVariable[i].numIndependVariable*sizeof(double));

                    if (feaProblem->feaDesignVariable[i].independVariableWeight == NULL) return EGADS_MALLOC;

                    for (j = 0; j < feaProblem->feaDesignVariable[i].numIndependVariable; j++) {
                        feaProblem->feaDesignVariable[i].independVariableWeight[j] = 1.0;
                    }
                }
            }


            /*! \page feaDesignVariable
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>variableWeight = [1.0, 1.0]</B> </li> <br>
             *  Weighting constants for a dependent variable - used if "independentVariable"(s) have been provided.
             * </ul>
             * \endif
             */
            keyWord = "variableWeight";
            status = search_jsonDictionary( designVariableTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleArray(keyValue,
                        2,
                        feaProblem->feaDesignVariable[i].variableWeight);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) {
                    printf("\tError during retrieving variableWeight - status %d\n", status);
                    return status;
                }
            } else {
                feaProblem->feaDesignVariable[i].variableWeight[0] = 1.0;
                feaProblem->feaDesignVariable[i].variableWeight[1] = 1.0;
            }

        } else {

            printf("\tError: Design_Variable tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;

        }
    }

    if (keyValue != NULL) {
        EG_free(keyValue);
        keyValue = NULL;
    }

    // Now that we are done going through all the tuples we need to populate/create the independVaraiableID array
    // if independentVariable was set for any of them.
    for (i = 0; i < feaProblem->numDesignVariable; i++) {

        if (feaProblem->feaDesignVariable[i].numIndependVariable != 0) {

            feaProblem->feaDesignVariable[i].independVariableID = (int *) EG_alloc(feaProblem->feaDesignVariable[i].numIndependVariable*sizeof(int));
            if (feaProblem->feaDesignVariable[i].independVariableID == NULL) return EGADS_MALLOC;

            // Loop through the independent variable names
            for (j = 0; j < feaProblem->feaDesignVariable[i].numIndependVariable; j++) {

                // Compare the independent variable names with design variable name
                found = (int) false;
                for (k = 0; k < feaProblem->numDesignVariable; k++) {
                    if (strcasecmp(feaProblem->feaDesignVariable[i].independVariable[j], feaProblem->feaDesignVariable[k].name) == 0) {
                        found = (int) true;
                        break;
                    }
                }

                // If NOT found
                if (found != (int) true) {
                    printf("\tDesign variable name, \"%s\", not found when searching for independent design variables for "
                            "variable %s!!!\n", feaProblem->feaDesignVariable[i].independVariable[j],
                                                feaProblem->feaDesignVariable[i].name);
                    return CAPS_NOTFOUND;
                }

                feaProblem->feaDesignVariable[i].independVariableID[j] = feaProblem->feaDesignVariable[k].designVariableID;
            }
        }
    }

    printf("\tDone getting FEA design variables\n");
    return CAPS_SUCCESS;
}

// Get the design constraints from a capsTuple
int fea_getDesignConstraint(int numDesignConstraintTuple,
                            capsTuple designConstraintTuple[],
                            feaProblemStruct *feaProblem) {

    /*! \page feaDesignConstraint FEA DesignConstraint
     * Structure for the design constraint tuple  = (`DesignConstraint Name', `Value').
     * 'DesignConstraint Name' defines the reference name for the design constraint being specified.
     *	The "Value" must be a JSON String dictionary (see Section \ref jsonStringDesignConstraint).
     */

    int status; //Function return

    int i, groupIndex, attrIndex; // Indexing

    char *keyValue = NULL;
    char *keyWord = NULL;

    char *tempString = NULL;

    char **groupName = NULL;
    int  numGroupName = 0;

    // Destroy our design constraints structures coming in if aren't 0 and NULL already
    if (feaProblem->feaDesignConstraint != NULL) {
        for (i = 0; i < feaProblem->numDesignConstraint; i++) {
            status = destroy_feaDesignConstraintStruct(&feaProblem->feaDesignConstraint[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (feaProblem->feaDesignConstraint != NULL) EG_free(feaProblem->feaDesignConstraint);
    feaProblem->feaDesignConstraint = NULL;
    feaProblem->numDesignConstraint = 0;

    printf("\nGetting FEA design constraints.......\n");

    feaProblem->numDesignConstraint = numDesignConstraintTuple;

    printf("\tNumber of design constraints - %d\n", feaProblem->numDesignConstraint);

    if (feaProblem->numDesignConstraint > 0) {
        feaProblem->feaDesignConstraint = (feaDesignConstraintStruct *) EG_alloc(feaProblem->numDesignConstraint * sizeof(feaDesignConstraintStruct));

        if (feaProblem->feaDesignConstraint == NULL) return EGADS_MALLOC;

    } else {
        printf("\tNumber of design constraint values in input tuple is 0\n");
        return CAPS_NOTFOUND;
    }

    for (i = 0; i < feaProblem->numDesignConstraint; i++) {
        status = initiate_feaDesignConstraintStruct(&feaProblem->feaDesignConstraint[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    for (i = 0; i < feaProblem->numDesignConstraint; i++) {

        printf("\tDesign_Constraint name - %s\n", designConstraintTuple[i].name);

        feaProblem->feaDesignConstraint[i].name = (char *) EG_alloc(((strlen(designConstraintTuple[i].name)) + 1)*sizeof(char));
        if (feaProblem->feaDesignConstraint[i].name == NULL) return EGADS_MALLOC;

        memcpy(feaProblem->feaDesignConstraint[i].name, designConstraintTuple[i].name, strlen(designConstraintTuple[i].name)*sizeof(char));
        feaProblem->feaDesignConstraint[i].name[strlen(designConstraintTuple[i].name)] = '\0';

        feaProblem->feaDesignConstraint[i].designConstraintID = i + 1;

        // Do we have a json string?
        if (strncmp(designConstraintTuple[i].value, "{", 1) == 0) {
            //printf("JSON String - %s\n", designConstraintTuple[i].value);

            /*! \page feaDesignConstraint
             * \section jsonStringDesignConstraint JSON String Dictionary
             *
             * If "Value" is JSON string dictionary
             * \if NASTRAN
             *  (eg. "Value" = {"groupName": "plate", "upperBound": 10.0})
             * \endif
             *  the following keywords ( = default values) may be used:
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <c>capsGroup</c> name(s)
             *  to the design variable (e.g. "Name1" or ["Name1","Name2",...].The property (see \ref feaProperty) also
             *  assigned to the same <c>capsGroup</c> will be automatically related to this constraint entry.
             * </ul>
             * \endif
             *
             */

            // Get material/properties that the design constraint should be applied to
            keyWord = "groupName";
            status = search_jsonDictionary( designConstraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (status != CAPS_SUCCESS) return status;

                feaProblem->feaDesignConstraint[i].numPropertyID = 0;
                for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                    for (attrIndex = 0; attrIndex < feaProblem->numProperty; attrIndex++) {

                        if (strcasecmp(feaProblem->feaProperty[attrIndex].name, groupName[groupIndex]) == 0) {

                            feaProblem->feaDesignConstraint[i].numPropertyID += 1;

                            if (feaProblem->feaDesignConstraint[i].numPropertyID == 1)  {

                                feaProblem->feaDesignConstraint[i].propertySetID   = (int *) EG_alloc(feaProblem->feaDesignConstraint[i].numPropertyID *sizeof(int));
                                feaProblem->feaDesignConstraint[i].propertySetType = (int *) EG_alloc(feaProblem->feaDesignConstraint[i].numPropertyID *sizeof(int));

                            } else {

                                feaProblem->feaDesignConstraint[i].propertySetID   = (int *) EG_reall(feaProblem->feaDesignConstraint[i].propertySetID,
                                                                                                      feaProblem->feaDesignConstraint[i].numPropertyID *sizeof(int));
                                feaProblem->feaDesignConstraint[i].propertySetType = (int *) EG_reall(feaProblem->feaDesignConstraint[i].propertySetType,
                                                                                                      feaProblem->feaDesignConstraint[i].numPropertyID *sizeof(int));

                            }

                            if (feaProblem->feaDesignConstraint[i].propertySetID   == NULL ||
                                feaProblem->feaDesignConstraint[i].propertySetType == NULL    ) {

                                status = string_freeArray(numGroupName, &groupName);
                                if (status != CAPS_SUCCESS) return status;
                                groupName = NULL;
                                return EGADS_MALLOC;
                            }

                            feaProblem->feaDesignConstraint[i].propertySetID[feaProblem->feaDesignConstraint[i].numPropertyID-1]   = feaProblem->feaProperty[attrIndex].propertyID;
                            feaProblem->feaDesignConstraint[i].propertySetType[feaProblem->feaDesignConstraint[i].numPropertyID-1] = feaProblem->feaProperty[attrIndex].propertyType;

                            break;
                        }
                    }

                    if (feaProblem->feaDesignConstraint[i].numPropertyID != groupIndex+1) {

                        printf("\tWarning: DesignConstraint property name, %s, not found in feaProperty structure\n", groupName[groupIndex]);

                    }
                }

                status = string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) return status;
                groupName = NULL;
            }

            //Fill up designConstraint properties

            /*! \page feaDesignConstraint
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>lowerBound = 0.0</B> </li> <br>
             *  Lower bound for the design constraint.
             * </ul>
             * \endif
             *
             */
            keyWord = "lowerBound";
            status = search_jsonDictionary( designConstraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaDesignConstraint[i].lowerBound);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaDesignConstraint
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>upperBound = 0.0</B> </li> <br>
             *  Upper bound for the design constraint.
             * </ul>
             * \endif
             *
             */
            keyWord = "upperBound";
            status = search_jsonDictionary( designConstraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &feaProblem->feaDesignConstraint[i].upperBound);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

            /*! \page feaDesignConstraint
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>responseType = "(no default)"</B> </li> <br>
             *  Response type options for DRESP1 Entry (see Nastran manual).
             *  	- Implemented Options
             *  		-# <c>STRESS</c>, for <c>propertyType = "Rod" or "Shell"</c> (see \ref feaProperty)
             *  		-# <c>CFAILURE</c>, for <c>propertyType = "Composite"</c> (see \ref feaProperty)
             * </ul>
             * \endif
             *
             */
            keyWord = "responseType";
            status = search_jsonDictionary( designConstraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaDesignConstraint[i].responseType = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaDesignConstraint[i].responseType == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaDesignConstraint[i].responseType, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaDesignConstraint[i].responseType[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            /*! \page feaDesignConstraint
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>fieldName = "(no default)"</B> </li> <br>
             *  For constraints, this field is only used currently when applying constraints to composites.  This field is used to identify
             *  the specific lamina in a stacking sequence that a constraint is being applied too.  Note if the user has design variables
             *  for both THEATA1 and T1 it is likely that only a single constraint on the first lamina is required.  For this reason, the user
             *  can simply enter LAMINA1 in addition to the possible entries defined in the \ref feaDesignVariable section.
             *  Additionally, the <c>fieldPosition</c> integer entry below can be used.  In this case <c>"LAMINA1" = 1</c>.
             *
             *  *  -# <b> Property Types</b> (see \ref feaProperty)
             *  	- <c><b> PCOMP</b> </c> <c>propertyType = "Composite"</c>
             *  		- "T1", "THETA1", "T2", "THETA2", ... "Ti", "THETAi"
             *  		- "LAMINA1", "LAMINA2", ... "LAMINAi"
             *
             * </ul>
             * \endif
             */

            keyWord = "fieldName";
            status = search_jsonDictionary( designConstraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                tempString = string_removeQuotation(keyValue);

                feaProblem->feaDesignConstraint[i].fieldName = EG_alloc((strlen(tempString) + 1)*sizeof(char));
                if(feaProblem->feaDesignConstraint[i].fieldName == NULL) {
                    if (tempString != NULL) EG_free(tempString);

                    return EGADS_MALLOC;
                }

                memcpy(feaProblem->feaDesignConstraint[i].fieldName, tempString, strlen(tempString)*sizeof(char));
                feaProblem->feaDesignConstraint[i].fieldName[strlen(tempString)] = '\0';

                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }

                if (tempString != NULL) {
                    EG_free(tempString);
                    tempString = NULL;
                }
            }

            /*! \page feaDesignConstraint
             *
             * \if NASTRAN
             * <ul>
             *  <li> <B>fieldPosition = 0</B> </li> <br>
             *  This input is ignored if not defined.  The user may use this field instead of the <c>fieldName</c> input defined above to
             *  identify a specific lamina in a composite stacking sequence where a constraint is applied.  Please read the <c>fieldName</c>
             *  information above for more information.
             * </ul>
             * \endif
             */

            keyWord = "fieldPosition";
            status = search_jsonDictionary( designConstraintTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &feaProblem->feaDesignConstraint[i].fieldPosition);
                if (keyValue != NULL) {
                    EG_free(keyValue);
                    keyValue = NULL;
                }
                if (status != CAPS_SUCCESS) return status;
            }

        } else {

            printf("\tError: Design_Constraint tuple value is expected to be a JSON string\n");
            return CAPS_BADVALUE;
            // CALL designConstraint look up

        }
    }

    if (keyValue != NULL) {
        EG_free(keyValue);
        keyValue = NULL;
    }

    printf("Done getting FEA design constraints\n");
    return CAPS_SUCCESS;
}

// Get the coordinate system information from the bodies and an attribute map (of CoordSystem)
int fea_getCoordSystem(int numBody,
                       ego bodies[],
                       mapAttrToIndexStruct coordSystemMap,
                       int *numCoordSystem,
                       feaCoordSystemStruct *feaCoordSystem[]) {

    int status; // Function return

    int i, body, face, edge, node; // Indexing

    int numFace = 0, numEdge = 0, numNode = 0; // Number of egos
    ego *faces = NULL, *edges = NULL, *nodes = NULL; // Geometry

    int atype, alen; // EGADS return variables
    const int    *ints;
    const double *reals;
    const char *string;

    int found = (int) false;

    // Destroy our CoordSystem structures coming in if aren't 0 and NULL already
    if (*feaCoordSystem != NULL) {
        for (i = 0; i < *numCoordSystem; i++) {
            status = destroy_feaCoordSystemStruct(&(*feaCoordSystem)[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }
    if (*feaCoordSystem != NULL) EG_free(*feaCoordSystem);
    *feaCoordSystem = NULL;
    *numCoordSystem = 0;

    printf("\nGetting FEA coordinate systems.......\n");

    *numCoordSystem = coordSystemMap.numAttribute;
    printf("\tNumber of coordinate systems - %d\n", *numCoordSystem);

    if (*numCoordSystem > 0) {
        *feaCoordSystem = (feaCoordSystemStruct *) EG_alloc((*numCoordSystem)*sizeof(feaCoordSystemStruct));
        if (*feaCoordSystem == NULL) return EGADS_MALLOC;

    } else {
        printf("\tNo coordinate systems found - defaulting to global\n");
        return CAPS_SUCCESS;
    }

    for (i = 0; i < *numCoordSystem; i++) {
        status = initiate_feaCoordSystemStruct(&(*feaCoordSystem)[i]);
        if (status != CAPS_SUCCESS) return status;
    }

    for (i = 0; i < *numCoordSystem; i++) {

        if (faces != NULL) EG_free(faces);
        if (edges != NULL) EG_free(edges);
        if (nodes != NULL) EG_free(nodes);

        faces = NULL;
        edges = NULL;
        nodes = NULL;

        printf("\tCoordinate system name - %s\n", coordSystemMap.attributeName[i]);

        (*feaCoordSystem)[i].name = EG_strdup(coordSystemMap.attributeName[i]);
        if ((*feaCoordSystem)[i].name == NULL) { status = EGADS_MALLOC; goto cleanup; }

        (*feaCoordSystem)[i].coordSystemID = coordSystemMap.attributeIndex[i];

        (*feaCoordSystem)[i].refCoordSystemID = 0;

        (*feaCoordSystem)[i].coordSystemType = RectangularCoordSystem;

        found = (int) false;

        // Search through bodies
        for (body = 0; body < numBody; body++) {

            // Look at the body level
            status = EG_attributeRet(bodies[body], (*feaCoordSystem)[i].name, &atype, &alen, &ints, &reals, &string);
            if (status != EGADS_SUCCESS &&
                    status != EGADS_NOTFOUND) goto cleanup;

            if (atype == ATTRCSYS) {

                // Save the origin
                (*feaCoordSystem)[i].origin[0] = reals[alen+0];
                (*feaCoordSystem)[i].origin[1] = reals[alen+1];
                (*feaCoordSystem)[i].origin[2] = reals[alen+2];

                (*feaCoordSystem)[i].normal1[0] = reals[alen+3];
                (*feaCoordSystem)[i].normal1[1] = reals[alen+4];
                (*feaCoordSystem)[i].normal1[2] = reals[alen+5];

                (*feaCoordSystem)[i].normal2[0] = reals[alen+6];
                (*feaCoordSystem)[i].normal2[1] = reals[alen+7];
                (*feaCoordSystem)[i].normal2[2] = reals[alen+8];

                (*feaCoordSystem)[i].normal3[0] = reals[alen+9];
                (*feaCoordSystem)[i].normal3[1] = reals[alen+10];
                (*feaCoordSystem)[i].normal3[2] = reals[alen+11];

                found = (int) true;
            }

            if (found == (int) true)  break;

            // Determine the number of faces, edges, and nodes
            status = EG_getBodyTopos(bodies[body], NULL, FACE, &numFace, &faces);
            if (status != EGADS_SUCCESS) goto cleanup;

            status = EG_getBodyTopos(bodies[body], NULL, EDGE, &numEdge, &edges);
            if (status != EGADS_SUCCESS) goto cleanup;

            status = EG_getBodyTopos(bodies[body], NULL, NODE, &numNode, &nodes);
            if (status != EGADS_SUCCESS) goto cleanup;

            // Loop through faces
            for (face = 0; face < numFace; face++) {

                status = EG_attributeRet(faces[face], (const char *) (*feaCoordSystem)[i].name, &atype, &alen, &ints, &reals, &string);

                if (status == EGADS_NOTFOUND) continue;
                if (status != EGADS_SUCCESS) goto cleanup;

                if (atype == ATTRCSYS) {

                    // Save the origin
                    (*feaCoordSystem)[i].origin[0] = reals[alen+0];
                    (*feaCoordSystem)[i].origin[1] = reals[alen+1];
                    (*feaCoordSystem)[i].origin[2] = reals[alen+2];

                    (*feaCoordSystem)[i].normal1[0] = reals[alen+3];
                    (*feaCoordSystem)[i].normal1[1] = reals[alen+4];
                    (*feaCoordSystem)[i].normal1[2] = reals[alen+5];

                    (*feaCoordSystem)[i].normal2[0] = reals[alen+6];
                    (*feaCoordSystem)[i].normal2[1] = reals[alen+7];
                    (*feaCoordSystem)[i].normal2[2] = reals[alen+8];

                    (*feaCoordSystem)[i].normal3[0] = reals[alen+9];
                    (*feaCoordSystem)[i].normal3[1] = reals[alen+10];
                    (*feaCoordSystem)[i].normal3[2] = reals[alen+11];

                    found = (int) true;

                    break;
                }
            } // End face loop

            if (found == (int) true)  break;

            // Loop through edges
            for (edge = 0; edge < numEdge; edge++) {

                status = EG_attributeRet(edges[edge], (*feaCoordSystem)[i].name, &atype, &alen, &ints, &reals, &string);

                if (status == EGADS_NOTFOUND) continue;
                if (status != EGADS_SUCCESS) goto cleanup;

                if (atype == ATTRCSYS) {

                    // Save the origin
                    (*feaCoordSystem)[i].origin[0] = reals[alen+0];
                    (*feaCoordSystem)[i].origin[1] = reals[alen+1];
                    (*feaCoordSystem)[i].origin[2] = reals[alen+2];

                    (*feaCoordSystem)[i].normal1[0] = reals[alen+3];
                    (*feaCoordSystem)[i].normal1[1] = reals[alen+4];
                    (*feaCoordSystem)[i].normal1[2] = reals[alen+5];

                    (*feaCoordSystem)[i].normal2[0] = reals[alen+6];
                    (*feaCoordSystem)[i].normal2[1] = reals[alen+7];
                    (*feaCoordSystem)[i].normal2[2] = reals[alen+8];

                    (*feaCoordSystem)[i].normal3[0] = reals[alen+9];
                    (*feaCoordSystem)[i].normal3[1] = reals[alen+10];
                    (*feaCoordSystem)[i].normal3[2] = reals[alen+11];

                    found = (int) true;

                    break;
                }

            } // End edge loop

            if (found == (int) true)  break;

            // Loop through nodes
            for (node = 0; node < numNode; node++) {

                status = EG_attributeRet(nodes[node], (*feaCoordSystem)[i].name, &atype, &alen, &ints, &reals, &string);

                if (status == EGADS_NOTFOUND) continue;
                if (status != EGADS_SUCCESS) goto cleanup;

                if (atype == ATTRCSYS) {

                    // Save the origin
                    (*feaCoordSystem)[i].origin[0] = reals[alen+0];
                    (*feaCoordSystem)[i].origin[1] = reals[alen+1];
                    (*feaCoordSystem)[i].origin[2] = reals[alen+2];

                    (*feaCoordSystem)[i].normal1[0] = reals[alen+3];
                    (*feaCoordSystem)[i].normal1[1] = reals[alen+4];
                    (*feaCoordSystem)[i].normal1[2] = reals[alen+5];

                    (*feaCoordSystem)[i].normal2[0] = reals[alen+6];
                    (*feaCoordSystem)[i].normal2[1] = reals[alen+7];
                    (*feaCoordSystem)[i].normal2[2] = reals[alen+8];

                    (*feaCoordSystem)[i].normal3[0] = reals[alen+9];
                    (*feaCoordSystem)[i].normal3[1] = reals[alen+10];
                    (*feaCoordSystem)[i].normal3[2] = reals[alen+11];

                    found = (int) true;

                    break;
                }
            } // End node loop

            EG_free(faces); faces = NULL;
            EG_free(edges); edges = NULL;
            EG_free(nodes); nodes = NULL;

            if (found == (int) true)  break;

        } // End body loop
    }

    status = CAPS_SUCCESS;

    cleanup:

        if (status != CAPS_SUCCESS) printf("\tError in fea_getCoordSystem = %d\n", status);

        EG_free(faces);
        EG_free(edges);
        EG_free(nodes);

        return status;
}


// Initiate (0 out all values and NULL all pointers) of feaProblem in the feaProblemStruct structure format
int initiate_feaProblemStruct(feaProblemStruct *feaProblem) {

    int status = 0;

    // Problem analysis
    feaProblem->numAnalysis = 0;
    feaProblem->feaAnalysis = NULL;

    // Materials
    feaProblem->numMaterial = 0;
    feaProblem->feaMaterial = NULL;

    // Properties
    feaProblem->numProperty = 0;
    feaProblem->feaProperty = NULL;

    // Constraints
    feaProblem->numConstraint = 0;
    feaProblem->feaConstraint = NULL;

    // Supports
    feaProblem->numSupport = 0;
    feaProblem->feaSupport = NULL;

    // Loads
    feaProblem->numLoad = 0;
    feaProblem->feaLoad = NULL;

    // Connections
    feaProblem->numConnect = 0;
    feaProblem->feaConnect = NULL;

    // Mesh
    status = initiate_meshStruct(&feaProblem->feaMesh);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Output formatting
    status = initiate_feaFileFormatStruct(&feaProblem->feaFileFormat);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Optimization - design variables
    feaProblem->numDesignVariable = 0;
    feaProblem->feaDesignVariable = NULL;

    // Optimization - design constraint
    feaProblem->numDesignConstraint = 0;
    feaProblem->feaDesignConstraint = NULL;

    // Coordinate Systems
    feaProblem->numCoordSystem = 0;
    feaProblem->feaCoordSystem = NULL;

    // Aerodynamics
    feaProblem->numAero = 0;
    feaProblem->feaAero = NULL;
    status = initiate_feaAeroRefStruct(&feaProblem->feaAeroRef);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Status %d during initiate_feaProblemStruct!\n", status);

        return status;
}

// Destroy (0 out all values and NULL all pointers) of feaProblem in the feaProblemStruct structure format
int destroy_feaProblemStruct(feaProblemStruct *feaProblem) {

    int i; // Indexing

    int status = 0; // Status return

    // Analysis
    if (feaProblem->feaAnalysis != NULL) {
        for (i = 0; i < feaProblem->numAnalysis; i++) {
            status = destroy_feaAnalysisStruct(&feaProblem->feaAnalysis[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaAnalysisStruct\n", status);
        }
    }

    if (feaProblem->feaAnalysis != NULL) EG_free(feaProblem->feaAnalysis);
    feaProblem->feaAnalysis = NULL;

    feaProblem->numAnalysis = 0;

    // Materials
    if (feaProblem->feaMaterial != NULL) {
        for (i = 0; i < feaProblem->numMaterial; i++) {
            status = destroy_feaMaterialStruct(&feaProblem->feaMaterial[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaMaterialStruct\n", status);
        }
    }
    if (feaProblem->feaMaterial != NULL) EG_free(feaProblem->feaMaterial);
    feaProblem->feaMaterial = NULL;

    feaProblem->numMaterial = 0;

    // Properties
    if (feaProblem->feaProperty != NULL) {
        for (i = 0; i < feaProblem->numProperty; i++) {
            status = destroy_feaPropertyStruct(&feaProblem->feaProperty[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaPropertyStruct\n", status);
        }
    }

    if (feaProblem->feaProperty != NULL)EG_free(feaProblem->feaProperty);
    feaProblem->feaProperty = NULL;

    feaProblem->numProperty = 0;

    // Constraints
    if (feaProblem->feaConstraint != NULL) {
        for (i = 0; i < feaProblem->numConstraint; i++) {
            status = destroy_feaConstraintStruct(&feaProblem->feaConstraint[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaConstraintStruct\n", status);
        }
    }

    feaProblem->numConstraint = 0;

    if (feaProblem->feaConstraint != NULL) EG_free(feaProblem->feaConstraint);
    feaProblem->feaConstraint = NULL;

    // Supports
    if (feaProblem->feaSupport != NULL) {
        for (i = 0; i < feaProblem->numSupport; i++) {
            status = destroy_feaSupportStruct(&feaProblem->feaSupport[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaSupportStruct\n", status);
        }
    }

    feaProblem->numSupport = 0;

    if (feaProblem->feaSupport != NULL) EG_free(feaProblem->feaSupport);
    feaProblem->feaSupport = NULL;

    // Loads
    if (feaProblem->feaLoad != NULL) {
        for (i = 0; i < feaProblem->numLoad; i++) {
            status = destroy_feaLoadStruct(&feaProblem->feaLoad[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaLoadStruct\n", status);
        }
    }

    feaProblem->numLoad = 0;

    if (feaProblem->feaLoad != NULL) EG_free(feaProblem->feaLoad);
    feaProblem->feaLoad = NULL;

    // Connections
    if (feaProblem->feaConnect != NULL) {
        for (i = 0; i < feaProblem->numConnect; i++) {
            status = destroy_feaConnectionStruct(&feaProblem->feaConnect[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaConnectStruct\n", status);
        }
    }

    feaProblem->numConnect = 0;

    if (feaProblem->feaConnect != NULL) EG_free(feaProblem->feaConnect);
    feaProblem->feaConnect = NULL;

    // Mesh
    status = destroy_meshStruct(&feaProblem->feaMesh);
    if (status != CAPS_SUCCESS) printf("Status %d during destroy_meshStruct\n", status);

    // Output formatting
    status = destroy_feaFileFormatStruct(&feaProblem->feaFileFormat);
    if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaFileFormatStruct\n", status);

    // Optimization - design variables
    if (feaProblem->feaDesignVariable != NULL) {
        for (i = 0; i < feaProblem->numDesignVariable; i++) {
            status = destroy_feaDesignVariableStruct(&feaProblem->feaDesignVariable[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaDesignVariableStruct\n", status);
        }
    }

    feaProblem->numDesignVariable = 0;

    if (feaProblem->feaDesignVariable != NULL) EG_free(feaProblem->feaDesignVariable);
    feaProblem->feaDesignVariable = NULL;

    // Optimization - design constraint
    if (feaProblem->feaDesignConstraint != NULL) {
        for (i = 0; i < feaProblem->numDesignConstraint; i++) {
            status = destroy_feaDesignConstraintStruct(&feaProblem->feaDesignConstraint[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaDesignConstraintStruct\n", status);
        }
    }

    feaProblem->numDesignConstraint = 0;

    if (feaProblem->feaDesignConstraint != NULL) EG_free(feaProblem->feaDesignConstraint);
    feaProblem->feaDesignConstraint = NULL;

    // Coordinate Systems
    if (feaProblem->feaCoordSystem != NULL) {

        for (i = 0; i < feaProblem->numCoordSystem; i++) {
            status = destroy_feaCoordSystemStruct(&feaProblem->feaCoordSystem[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaCoordSystemStruct\n", status);
        }
    }

    if (feaProblem->feaCoordSystem != NULL) EG_free(feaProblem->feaCoordSystem);
    feaProblem->feaCoordSystem = NULL;

    feaProblem->numCoordSystem = 0;

    // Aerodynamics
    if (feaProblem->feaAero != NULL) {

        for (i = 0; i < feaProblem->numAero; i++) {
            status = destroy_feaAeroStruct(&feaProblem->feaAero[i]);
            if (status != CAPS_SUCCESS) printf("Status %d during destroy_feaAeroStruct\n", status);
        }
    }

    if (feaProblem->feaAero != NULL) EG_free(feaProblem->feaAero);
    feaProblem->feaAero = NULL;

    feaProblem->numAero = 0;

    (void) destroy_feaAeroRefStruct(&feaProblem->feaAeroRef);

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of feaFileFormat in the feaFileFormatStruct structure format
int initiate_feaFileFormatStruct(feaFileFormatStruct *feaFileFormat) {

    feaFileFormat->fileType = SmallField;

    feaFileFormat->gridFileType = LargeField;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of feaFileFormat in the feaFileFormatStruct structure format
int destroy_feaFileFormatStruct(feaFileFormatStruct *feaFileFormat) {

    feaFileFormat->fileType = SmallField;

    feaFileFormat->gridFileType = LargeField;

    return CAPS_SUCCESS;
}

// Transfer external pressure from the discrObj into the feaLoad structure
int fea_transferExternalPressure(void *aimInfo, meshStruct *feaMesh, feaLoadStruct *feaLoad) {

    // [in/out] feaLoad
    // [in] feaMesh
    // [in] aimInfo

    int status; // Function status return

    int i, j; // Indexing

    // Variables used in global node mapping
    int *tessNodeMap = NULL, *storage = NULL;
    int globalNodeID;
    int nodeIndex[4], transferIndex[4], elementID, elementIndex, elementCount;

    // Discrete data transfer variables
    capsDiscr *dataTransferDiscreteObj;
    char **transferName = NULL;
    int numTransferName, transferNameIndex;
    enum capsdMethod dataTransferMethod;
    int numDataTransferPoint;
    int numDataTransferElement = 0;
    int dataTransferRank;
    double *dataTransferData;

    printf("Extracting external pressure loads from data transfer....\n");

    feaLoad->numElementID = 0;
    if (feaLoad->pressureMultiDistributeForce != NULL) EG_free(feaLoad->pressureMultiDistributeForce);
    if (feaLoad->elementIDSet                 != NULL) EG_free(feaLoad->elementIDSet);

    //See if we have data transfer information
    status = aim_getBounds(aimInfo, &numTransferName, &transferName);
    if (status != CAPS_SUCCESS) goto cleanup;

    numDataTransferElement = 0;
    elementIndex = 0;
    elementCount = 0;

    for (transferNameIndex = 0; transferNameIndex < numTransferName; transferNameIndex++) {

        status = aim_getDiscr(aimInfo, transferName[transferNameIndex], &dataTransferDiscreteObj);
        if (status != CAPS_SUCCESS) continue;

        status = aim_getDataSet(dataTransferDiscreteObj,
                                "Pressure",
                                &dataTransferMethod,
                                &numDataTransferPoint,
                                &dataTransferRank,
                                &dataTransferData);

        if (status == CAPS_SUCCESS) { // If we do have data ready, how many elements there are?

            if (numDataTransferPoint == 1) {
                printf("*********************************\n");
                printf("\n");
                printf("ERROR: Pressures not initialized!\n");
                printf("\n");
                printf("*********************************\n");
                status = CAPS_BADINIT;
                goto cleanup;
            }

            numDataTransferElement += dataTransferDiscreteObj->nElems;
            printf("\tTransferName = %s\n", transferName[transferNameIndex]);
            printf("\tNumber of Elements = %d (total = %d)\n", dataTransferDiscreteObj->nElems, numDataTransferElement);

        } else {

            continue; // If no elements in this object skip to next transfer name
        }

        if (dataTransferRank != 1) {
            printf("\tPressure transfer data found however rank is %d not 1!!!!\n", dataTransferRank);

            status = CAPS_BADRANK;
            goto cleanup;
        }

        // If the first time we found elements allocate arrays
        if (feaLoad->numElementID == 0) {

            feaLoad->elementIDSet = (int *) EG_alloc(numDataTransferElement *sizeof(int));
            feaLoad->pressureMultiDistributeForce = (double *) EG_alloc(4*numDataTransferElement *sizeof(double));

        } else { // Else reallocate arrays

            feaLoad->elementIDSet = (int *) EG_reall(feaLoad->elementIDSet,
                                                     numDataTransferElement *sizeof(int));
            feaLoad->pressureMultiDistributeForce = (double *) EG_reall(feaLoad->pressureMultiDistributeForce,
                                                                        4*numDataTransferElement *sizeof(double));
        }

        // Check to see if allocation failed
        if (feaLoad->elementIDSet == NULL ||
            feaLoad->pressureMultiDistributeForce == NULL) {

            feaLoad->numElementID = 0;
            status = EGADS_MALLOC;
            goto cleanup;
        }

        //Get extra node information stored in the discrObj
        storage = (int *) dataTransferDiscreteObj->ptrm;
        tessNodeMap = &storage[0]; // Local indexing on the body

        //Now lets loop through our ctria3 mesh and get the node indexes for each element
        for (i = 0; i < feaMesh->numElement; i++) {

            if (feaMesh->element[i].elementType != Triangle) continue;

            elementID = feaMesh->element[i].elementID;
            //printf("Element Id = %d\n", elementID);

            // elementID is 1 bias
            nodeIndex[0] = feaMesh->element[i].connectivity[0];
            nodeIndex[1] = feaMesh->element[i].connectivity[1];
            nodeIndex[2] = feaMesh->element[i].connectivity[2];

            //printf("Node Index = %d %d %d\n", nodeIndex[0], nodeIndex[1], nodeIndex[2]);

            transferIndex[0] = -1;
            transferIndex[1] = -1;
            transferIndex[2] = -1;
            transferIndex[3] = -1;

            // Loop through the nodeMap of the data set getting nodeIDs trying
            //   to match the nodes in the element
            for (j = 0; j < numDataTransferPoint; j++) {

                globalNodeID = tessNodeMap[j];

                if (nodeIndex[0] == globalNodeID) transferIndex[0] = j;
                if (nodeIndex[1] == globalNodeID) transferIndex[1] = j;
                if (nodeIndex[2] == globalNodeID) transferIndex[2] = j;

                // If the nodes completely match the nodes on the element - break
                if (transferIndex[0] >= 0 && transferIndex[1] >= 0 && transferIndex[2] >= 0) {
                    break;
                }
            }

            // If all the nodeIndexes match the transferIndex the element is in the data set
            //  so transfer the pressure forces
            if (transferIndex[0] >= 0 && transferIndex[1] >= 0 && transferIndex[2] >= 0) {

                feaLoad->elementIDSet[elementIndex] = elementID;

                feaLoad->pressureMultiDistributeForce[4*elementIndex+0] = dataTransferData[transferIndex[0]];
                feaLoad->pressureMultiDistributeForce[4*elementIndex+1] = dataTransferData[transferIndex[1]];
                feaLoad->pressureMultiDistributeForce[4*elementIndex+2] = dataTransferData[transferIndex[2]];
                feaLoad->pressureMultiDistributeForce[4*elementIndex+3] = 0.0;

                elementIndex += 1;
                elementCount += 1;
                feaLoad->numElementID  += 1;

            }
        }

        //Now lets loop through our cquad4 mesh and get the node indexes for each element
        for (i = 0; i < feaMesh->numElement; i++) {

            if (feaMesh->element[i].elementType != Quadrilateral) continue;

            elementID = feaMesh->element[i].elementID;

            // elementID is 1 bias
            nodeIndex[0] = feaMesh->element[i].connectivity[0];
            nodeIndex[1] = feaMesh->element[i].connectivity[1];
            nodeIndex[2] = feaMesh->element[i].connectivity[2];
            nodeIndex[3] = feaMesh->element[i].connectivity[3];

            transferIndex[0] = -1;
            transferIndex[1] = -1;
            transferIndex[2] = -1;
            transferIndex[3] = -1;

            // Loop through the nodeMap of the data set getting nodeIDs trying
            //   to match the nodes in the element
            for (j = 0; j < numDataTransferPoint; j++) {

                globalNodeID = tessNodeMap[j];

                if (nodeIndex[0] == globalNodeID) transferIndex[0] = j;
                if (nodeIndex[1] == globalNodeID) transferIndex[1] = j;
                if (nodeIndex[2] == globalNodeID) transferIndex[2] = j;
                if (nodeIndex[3] == globalNodeID) transferIndex[3] = j;

                // If the nodes completely match the nodes on the element - break
                if (transferIndex[0] >= 0 && transferIndex[1] >= 0 &&
                        transferIndex[2] >= 0 && transferIndex[3] >= 0) {
                    break;
                }
            }

            // If all the nodeIndexes match the transferIndex the element is in the data set
            //  so transfer the pressure forces
            if (transferIndex[0] >= 0 && transferIndex[1] >= 0 &&
                    transferIndex[2] >= 0 && transferIndex[3] >= 0) {

                feaLoad->elementIDSet[elementIndex] = elementID;

                feaLoad->pressureMultiDistributeForce[4*elementIndex+0] = dataTransferData[transferIndex[0]];
                feaLoad->pressureMultiDistributeForce[4*elementIndex+1] = dataTransferData[transferIndex[1]];
                feaLoad->pressureMultiDistributeForce[4*elementIndex+2] = dataTransferData[transferIndex[2]];
                feaLoad->pressureMultiDistributeForce[4*elementIndex+3] = dataTransferData[transferIndex[3]];

                elementIndex += 1;
                elementCount += 1;
                feaLoad->numElementID  += 1;

            }
        }

        if (elementCount != numDataTransferElement) {
            printf("\tElement transfer mismatch: number of elements found = %d, number"
                    " of elements in transfer data set %d\n", elementCount, numDataTransferElement);
            if (transferName != NULL) EG_free(transferName);
            transferName = NULL;

            status = CAPS_MISMATCH;
            goto cleanup;
        }

        // Resize
        if (feaLoad->numElementID != numDataTransferElement) {
            feaLoad->pressureMultiDistributeForce = (double *) EG_reall(feaLoad->pressureMultiDistributeForce,
                                                                        4*feaLoad->numElementID *sizeof(double));
            if (feaLoad->pressureMultiDistributeForce == NULL) {
                feaLoad->numElementID = 0;
                status = EGADS_MALLOC;
                goto cleanup;
            }
        }

    } // End data transfer name loop


    status = CAPS_SUCCESS;

cleanup:

    if (status != CAPS_SUCCESS) printf("\tPremature exit in fea_transferExternalPressure, status = %d\n", status);

    if (transferName != NULL) EG_free(transferName);

    return status;
}

// Retrieve aerodynamic reference quantities from bodies
int fea_retrieveAeroRef(int numBody, ego *bodies, feaAeroRefStruct *feaAeroRef) {

    int status; // Function return status
    int body;

    // EGADS return values
    int          atype, alen;
    const int    *ints;
    const char   *string;
    const double *reals;

    // Get reference quantities from the bodies
    for (body = 0; body < numBody; body++) {

        status = EG_attributeRet(bodies[body], "capsReferenceArea", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && atype == Double) {

            feaAeroRef->refArea = (double) reals[0];
        }

        status = EG_attributeRet(bodies[body], "capsReferenceChord", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && atype == Double){

            feaAeroRef->refChord = (double) reals[0];
        }

        status = EG_attributeRet(bodies[body], "capsReferenceSpan", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && atype == Double) {

            feaAeroRef->refSpan = (double) reals[0];
        }

        /*
        status = EG_attributeRet(bodies[body], "capsReferenceX", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && atype == Double) {
            XX = (double) reals[0];
        }

        status = EG_attributeRet(bodies[body], "capsReferenceY", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && atype == Double) {
            XX = (double) reals[0];
        }

        status = EG_attributeRet(bodies[body], "capsReferenceZ", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS && atype == Double) {
            XX = (double) reals[0];
        }
         */
    }

    return CAPS_SUCCESS;
}


// Assign element "subtypes" based on properties set
int fea_assignElementSubType(int numProperty, feaPropertyStruct *feaProperty, meshStruct *feaMesh)
{
    int propertyIndex, i ;

    feaMeshDataStruct *feaData;

    if (numProperty > 0 && feaProperty == NULL) return CAPS_NULLVALUE;
    if (feaMesh == NULL) return CAPS_NULLVALUE;

    printf("Updating mesh element types based on properties input\n");

    for (propertyIndex = 0; propertyIndex < numProperty; propertyIndex++ ) {

        // Types that don't need subtypes - setting this correctly requires knowledge of what mesh_writeNastran
        //  writes by default for each mesh element type
        if (feaProperty[propertyIndex].propertyType == Rod   ||
            feaProperty[propertyIndex].propertyType == Shell ||
            feaProperty[propertyIndex].propertyType == Composite ||
            feaProperty[propertyIndex].propertyType == Solid) continue;

        for (i = 0; i < feaMesh->numElement; i++) {

            if (feaMesh->element[i].markerID != feaProperty[propertyIndex].propertyID) continue;

            // What if this is a volume mesh we inherited ?
            if (feaMesh->element[i].analysisType != MeshStructure) {
                printf("Analysis type not set to MeshStructure for element %d\n", feaMesh->element[i].elementID);
                continue;
            }

            feaData = (feaMeshDataStruct *) feaMesh->element[i].analysisData;

            if (feaData->propertyID != feaProperty[propertyIndex].propertyID) {
                printf("Property ID mismatch between element \"markerID\" and feaData \"propertyID\" for element %d\n", feaMesh->element[i].elementID);
                continue;
            }

            if (feaProperty[propertyIndex].propertyType == ConcentratedMass &&
                feaMesh->element[i].elementType == Node) {

                feaData->elementSubType = ConcentratedMassElement;
            }

            if (feaProperty[propertyIndex].propertyType == Bar &&
                feaMesh->element[i].elementType == Line) {

                feaData->elementSubType = BarElement;
            }

            if (feaProperty[propertyIndex].propertyType == Beam &&
                feaMesh->element[i].elementType == Line) {

                feaData->elementSubType = BeamElement;
            }

            if (feaProperty[propertyIndex].propertyType == Shear &&
                feaMesh->element[i].elementType == Quadrilateral){

                feaData->elementSubType = ShearElement;
            }
        }
    }

    return CAPS_SUCCESS;
}
