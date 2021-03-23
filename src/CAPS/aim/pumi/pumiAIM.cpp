/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             PUMI AIM
 *
 *      Written by Tucker Babcock RPI
 *      With advisement from Dr. Marshall Galbraith MIT
 *      Patterned after aflr4AIM and tetgenAIM written by Dr. Ryan Durscher AFRL/RQVC
 *
 */

/*!\mainpage Introduction
 *
 * \section overviewPUMI PUMI AIM Overview
 * A module in the Computational Aircraft Prototype Syntheses (CAPS) has been developed to interact with the
 * open-source Parallel Unstructured Mesh Infrastructure (PUMI) \cite ibanez2016pumi. PUMI is an unstructured,
 * distributed mesh data management system that is capable of handling general non-manifold models and effectively
 * supporting automated adaptive analysis.
 *
 * The PUMI AIM provides the CAPS users with the ability to save an unstructured grid in PUMI's native format.
 *
 * An outline of the AIM's inputs, outputs and attributes are provided in \ref aimInputsPUMI and
 * \ref aimOutputsPUMI and \ref attributePUMI, respectively.
 * The PUMI source code is available on <a href="https://github.com/SCOREC/core">github</a>, and the user's guide
 * is available at <a href="https://scorec.rpi.edu/~seol/PUMI.pdf">SCOREC</a>.
 *
 * Details of the AIM's sharable data structures are outlined in \ref sharableDataPUMI if connecting this AIM to other AIMs in a
 * parent-child like manner.
 */

/// Not sure where this belongs
// @article{ibanez2016pumi,
//   title={PUMI: Parallel unstructured mesh infrastructure},
//   author={Ibanez, Daniel A and Seol, E Seegyoung and Smith, Cameron W and Shephard, Mark S},
//   journal={ACM Transactions on Mathematical Software (TOMS)},
//   volume={42},
//   number={3},
//   pages={1--28},
//   year={2016},
//   publisher={ACM New York, NY, USA}
// }

#include <string.h>
#include <math.h>
#include <iostream>
#include <fstream>

#include <mpi.h>

#include "apf.h"
#include "apfMesh.h"
#include "apfMesh2.h"
#include "apfMDS.h"
#include "crv.h"
#include "gmi.h"
#include "gmi_egads.h"
#include "pcu_util.h"
#include "PCU.h"

#include "egads.h"
#include "capsTypes.h"
#include "aimUtil.h"
#include "meshTypes.h"
#include "miscTypes.h"
#include "meshUtils.h"  // Collection of helper functions for meshing
#include "miscUtils.h"  // Collection of helper functions for miscellaneous use


/** \brief initialize a gmi_model with an EGADS body and number of regions */
extern "C"
struct gmi_model* gmi_egads_init(struct egObject *body, int numRegions);

/** \brief initialize the model adjacency table for 3D regions */
extern "C" 
void gmi_egads_init_adjacency(struct gmi_model* m, int ***adjacency);

extern "C" int
EG_saveTess(ego tess, const char *name); // super secret experimental EGADS tessellation format

extern "C" int
EG_loadTess(ego body, const char *name, ego *tess);

//#define DEBUG

enum aimInputs
{
  Proj_Name = 1,                 /* index is 1-based */
  Mesh_Format,
  Mesh,
  Mesh_Order,
  NUMINPUT = Mesh_Order    /* Total number of inputs */
};

enum aimOutputs
{
  Done = 1,                    /* index is 1-based */
  NUMOUT = Done         /* Total number of outputs */
};

typedef struct {

    int meshInherited;

    // Container for inherited mesh (can be surface or volume)
    int numMesh;
    meshStruct *mesh;

    int numPUMIMesh;
    apf::Mesh2 *pumiMesh;

    // Container for mesh input
    meshInputStruct meshInput;

    // Attribute to index map
    mapAttrToIndexStruct attrMap;

} aimStorage;

/// \brief count the number of unique 3D regions
/// \note this is O(n^2), could be implemented faster
int countRegions(meshElementStruct *elems, int nTets) 
{ 
    int count = 1; 
  
    // Go through each element one by one 
    for (int i = 1; i < nTets; i++) { 
        int j = 0;
        for (j = 0; j < i; j++) {
            if (elems[i].markerID == elems[j].markerID) {
                break; 
            }
        }
  
        // If not found before, account for it now 
        if (i == j) 
            count++; 
    } 
    return count; 
}

/// \brief constructs a mesh triangle from three vertices and a model entity
void createFace(apf::Mesh2* m, apf::MeshEntity* a, apf::MeshEntity* b,
                apf::MeshEntity* c, apf::ModelEntity* gent) {
    PCU_ALWAYS_ASSERT(gent);
    apf::MeshEntity* faceVerts[3] = {a,b,c};
    apf::buildElement(m, gent, apf::Mesh::TRIANGLE, faceVerts);
 }

/// \brief constructs a mesh edge from two mesh verticies. Uses the vertex
///        classifications to determine which model entity the mesh edge should
///        be classified on
void createEdge(apf::Mesh2* m, apf::MeshEntity* a, apf::MeshEntity* b,
                apf::ModelEntity* f_gent) {
    apf::ModelEntity* a_gent = m->toModel(a);
    apf::ModelEntity* b_gent = m->toModel(b);
    PCU_ALWAYS_ASSERT(a_gent);
    PCU_ALWAYS_ASSERT(b_gent);

    int a_gent_dim = m->getModelType(a_gent);
    int b_gent_dim = m->getModelType(b_gent);
    int a_gent_tag = m->getModelTag(a_gent);
    int b_gent_tag = m->getModelTag(b_gent);

    apf::ModelEntity* gent;
    /// \note this classification will fail if a mesh edge's adjacent verticies
    ///       are both on model vertices (this would only occur for a tiny edge)
    ///       *consider a better approach*
    if (a_gent_dim == b_gent_dim) {
        /// if two mesh verts belong to a mesh edge, and they're classified on
        /// model entities with the same dimension, that entity must be the
        /// same.
        std::cout << "gent dim: " << a_gent_dim << "\n";
        std::cout << a_gent_tag << ", " << b_gent_tag << "\n";
        if (a_gent_tag != b_gent_tag)
            gent = f_gent;
        else
            gent = a_gent;
    } else if (a_gent_dim > b_gent_dim) {
        gent = a_gent;
    } else {
        gent = b_gent;
    }
    apf::MeshEntity* edgeVerts[2] = {a,b};
    apf::buildElement(m, gent, 1, // apf::Mesh::EDGE
                      edgeVerts);
}

static int destroy_aimStorage(aimStorage *pumiInstance) {

    int i; // Indexing

    int status; // Function return status

    // Destroy meshInput
    status = destroy_meshInputStruct(&pumiInstance->meshInput);
    if (status != CAPS_SUCCESS)
        printf("Status = %d, pumiAIM meshInput cleanup!!!\n", status);

    // Destroy mesh allocated arrays
    if (pumiInstance->meshInherited == (int) false) { // Did the parent aim already destroy the meshes?
        if (pumiInstance->mesh != NULL) {
            for (i = 0; i < pumiInstance->numMesh; i++) {
                status = destroy_meshStruct(&pumiInstance->mesh[i]);
                if (status != CAPS_SUCCESS)
                    printf("Error during destroy_meshStruct, status = %d, pumiAIM\n", status);
            }
            EG_free(pumiInstance->mesh);
        }
    }

    // Destroy PUMI mesh
    if (pumiInstance->pumiMesh != NULL) {
        for (i = 0; i < pumiInstance->numPUMIMesh; i++) {
            pumiInstance->pumiMesh->destroyNative();
            apf::destroyMesh(pumiInstance->pumiMesh);
        }
    }

    pumiInstance->numMesh = 0;
    pumiInstance->meshInherited = (int) false;
    pumiInstance->mesh = NULL;

    // Destroy attribute to index map
    status = destroy_mapAttrToIndexStruct(&pumiInstance->attrMap);
    if (status != CAPS_SUCCESS)
        printf("Status = %d, pumi4AIM, attributeMap cleanup!!!\n", status);

    return CAPS_SUCCESS;
}

/* ********************** Exposed AIM Functions ***************************** */

extern "C" int
aimInitialize(int inst, /*@unused@*/ const char *unitSys,
              void **instStore, int *maj, int *min, int *nIn, int *nOut,
              int *nFields, char ***fnames, int **ranks)
{   
    int mpi_flag;
    MPI_Initialized(&mpi_flag);
    if (!mpi_flag) {
        MPI_Init(NULL, NULL);
        PCU_Comm_Init();
    }

    int status; // Function status return

    aimStorage *pumiInstance;

#ifdef DEBUG
    printf("\n pumiAIM/aimInitialize   ngIn = %d!\n", ngIn);
#endif

    /* specify the number of analysis input and out "parameters" */
    *nIn     = NUMINPUT;
    *nOut    = NUMOUT;
    if (inst == -1) return CAPS_SUCCESS;

    /* specify the field variables this analysis can generate */
    *nFields = 0;
    *ranks   = NULL;
    *fnames  = NULL;

    // Allocate pumiInstance
    pumiInstance = (aimStorage *) EG_alloc(sizeof(aimStorage));
    if (pumiInstance == NULL) return EGADS_MALLOC;

    // Set initial values for pumiInstance //

    // Container for mesh
    pumiInstance->meshInherited = (int) false;

    pumiInstance->numMesh = 0;
    pumiInstance->mesh = NULL;

    pumiInstance->numPUMIMesh = 0;
    pumiInstance->pumiMesh = NULL;

    // Container for attribute to index map
    status = initiate_mapAttrToIndexStruct(&pumiInstance->attrMap);
    if (status != CAPS_SUCCESS) {
        EG_free(pumiInstance);
        return status;
    }

    // Container for mesh input
    status = initiate_meshInputStruct(&pumiInstance->meshInput);
    if (status != CAPS_SUCCESS) {
        (void) destroy_mapAttrToIndexStruct(&pumiInstance->attrMap);
        EG_free(pumiInstance);
        return status;
    }

    *instStore = pumiInstance;
    return CAPS_SUCCESS;
}


extern "C" int
aimInputs(/*@unused@*/ void *instStore, void *aimInfo, int index, char **ainame,
           capsValue *defval)
{
    /*! \page aimInputsPUMI AIM Inputs
     * The following list outlines the PUMI options along with their default value available
     * through the AIM interface.
     */
    int status = CAPS_SUCCESS;

#ifdef DEBUG
    printf(" pumiAIM/aimInputs index = %d!\n", index);
#endif

    // Meshing Inputs
    if (index == Proj_Name) {
        *ainame              = EG_strdup("Proj_Name"); // If NULL a mesh won't be written by the AIM
        defval->type         = String;
        defval->nullVal      = IsNull;
        defval->vals.string  = NULL;
        //defval->vals.string  = EG_strdup("CAPS");
        defval->lfixed       = Change;

        /*! \page aimInputsPUMI
         * - <B> Proj_Name = NULL</B> <br>
         * This corresponds to the output name of the mesh. If left NULL, the mesh is not written to a file.
         */

    } else if (index == Mesh_Format) {
        *ainame               = EG_strdup("Mesh_Format");
        defval->type          = String;
        defval->vals.string   = EG_strdup("PUMI"); // PUMI, VTK
        defval->lfixed        = Change;

        /*! \page aimInputsPUMI
         * - <B> Mesh_Format = "PUMI"</B> <br>
         * Mesh output format. Available format names include: "PUMI", "VTK".
         */

    } else if (index == Mesh) {
        *ainame             = AIM_NAME(Mesh);
        defval->type        = Pointer;
        defval->nrow        = 1;
        defval->lfixed      = Fixed;
        defval->vals.AIMptr = NULL;
        defval->nullVal     = IsNull;
        AIM_STRDUP(defval->units, "meshStruct", aimInfo, status);

        /*! \page aimInputsPUMI
         * - <B>Mesh = NULL</B> <br>
         * A Surface_Mesh or Volume_Mesh link for 2D and 3D geometries respectively.
         */

    } else if (index == Mesh_Order) {
        *ainame               = EG_strdup("Mesh_Order");
        defval->type          = Integer;
        defval->dim           = Scalar;
        defval->vals.integer  = 1;

        /*! \page aimInputsPUMI
         * - <B> Mesh_Order = "1"</B> <br>
         * Mesh element order. PUMI supports curved simplex elements using Bezier curves or surfaces (currently) up to sixth order.<br>
         * See SCOREC <a href="https://www.scorec.rpi.edu/pumi/doxygen/crv.html">documentation</a> for more information.
         */

    }

    AIM_NOTNULL(*ainame, aimInfo, status);

cleanup:
    if (status != CAPS_SUCCESS) AIM_FREE(*ainame);
    return status;
}

// extern "C" int
// aimData(int iIndex, const char *name, enum capsvType *vtype, int *rank, int *nrow,
//         int *ncol, void **data, char **units)
// {

//     /*! \page sharableDataPUMI AIM Shareable Data
//      * The PUMI AIM has the following shareable data types/values with its children AIMs if they are so inclined.
//      * - <B> Mesh</B> <br>
//      * The mesh in meshStruct (see meshTypes.h) format.
//      * - <B> PUMI_Mesh</B> <br>
//      * The returned mesh after curving is complete in apf::Mesh2 (see <a href="https://www.scorec.rpi.edu/pumi/doxygen/apfMDS_8h.html">apfMDS.h</a>) format.
//      * - <B> Attribute_Map</B> <br>
//      * An index mapping between capsGroups found on the geometry in mapAttrToIndexStruct (see miscTypes.h) format.
//      */

// #ifdef DEBUG
//     printf(" pumiAIM/aimData instance = %d  name = %s!\n", iIndex, name);
// #endif

//     // The mesh input
//     if (strcasecmp(name, "Mesh") == 0){
//         *vtype = Value;
//         *rank  = *ncol = 1;
//         *nrow = pumiInstance->numMesh;
//         if (pumiInstance->numMesh == 1) {
//             *data  = &pumiInstance->mesh[0];
//         } else {
//             *data  = pumiInstance->mesh;
//         }

//         *units = NULL;

//         return CAPS_SUCCESS;
//     }

//     // The returned mesh from PUMI
//     if (strcasecmp(name, "PUMI_Mesh") == 0){
//         *vtype = Value;
//         *rank  = *ncol = 1;
//         *nrow = pumiInstance->numPUMIMesh;
//         if (pumiInstance->numPUMIMesh == 1) {
//             *data  = &(pumiInstance->pumiMesh[0]);
//         } else {
//             *data  = pumiInstance->pumiMesh;
//         }

//         *units = NULL;

//         return CAPS_SUCCESS;
//     }

//     // Share the attribute map
//     if (strcasecmp(name, "Attribute_Map") == 0){
//         *vtype = Value;
//         *rank  = *nrow = *ncol = 1;
//         *data  = &pumiInstance->attrMap;
//         *units = NULL;

//         return CAPS_SUCCESS;
//     }

//     return CAPS_NOTFOUND;
// }

extern "C" int
aimPreAnalysis(void *instStore, void *aimInfo, capsValue *aimInputs)
{
    int status; // Return status

    aimStorage *pumiInstance;

    // Incoming bodies
    const char *intents;
    ego *bodies = nullptr;
    int numBody = 0;

    // Get AIM bodies
    status = aim_getBodies(aimInfo, &intents, &numBody, &bodies);
    if (status != CAPS_SUCCESS) return status;

#ifdef DEBUG
    printf(" pumiAIM/aimPreAnalysis numBody = %d!\n", numBody);
#endif

    if (1 != numBody) {
#ifdef DEBUG
        printf(" pumiAIM/aimPreAnalysis only supports one body!\n");
#endif
        return CAPS_SOURCEERR;
    }

    pumiInstance = (aimStorage *) instStore;

    // Cleanup previous aimStorage for the instance in case this is the second time through preAnalysis for the same instance
    status = destroy_aimStorage(pumiInstance);
    if (status != CAPS_SUCCESS) {
        printf("Status = %d, pumiAIM aimStorage cleanup!!!\n", status);
        return status;
    }

    // Get capsGroup name and index mapping
    status = create_CAPSGroupAttrToIndexMap(numBody,
                                            bodies,
                                            2, // Only search down to the edge level of the EGADS body
                                            &pumiInstance->attrMap);
    if (status != CAPS_SUCCESS) return status;

    // Get mesh
    pumiInstance->numMesh = aimInputs[Mesh-1].length;
    pumiInstance->mesh = (meshStruct *)aimInputs[Mesh-1].vals.AIMptr;
    pumiInstance->meshInherited = (int) true;
    if (pumiInstance->mesh == NULL) {
        status = CAPS_NULLVALUE;
        return status;
    }

    // status = aim_getData(aimInfo, "Surface_Mesh", &vtype, &rank, &nrow, &ncol, &dataTransfer, &units);
    // if (status == CAPS_SUCCESS) {

    //     printf("Found link for a surface mesh (Surface_Mesh) from parent\n");

    //     int numMesh = 0;
    //     if      (nrow == 1 && ncol != 1) numMesh = ncol;
    //     else if (nrow != 1 && ncol == 1) numMesh = nrow;
    //     else if (nrow == 1 && ncol == 1) numMesh = nrow;
    //     else {

    //         printf("Can not except 2D matrix of surface meshes\n");
    //         return  CAPS_BADVALUE;
    //     }

    //     if (numMesh != numBody) {
    //         printf("Number of inherited surface meshes does not match the number of bodies\n");
    //         return CAPS_SOURCEERR;
    //     }
    //     pumiInstance->numMesh =  numMesh;
    //     pumiInstance->mesh = (meshStruct * ) dataTransfer;
    //     pumiInstance->meshInherited = (int) true;

    // } else {
    //     status = aim_getData(aimInfo, "Volume_Mesh", &vtype, &rank, &nrow, &ncol, &dataTransfer, &units);
    //     if (status == CAPS_SUCCESS) {

    //         printf("Found link for a volume mesh (Volume_Mesh) from parent\n");

    //         int numMesh = 0;
    //         if      (nrow == 1 && ncol != 1) numMesh = ncol;
    //         else if (nrow != 1 && ncol == 1) numMesh = nrow;
    //         else if (nrow == 1 && ncol == 1) numMesh = nrow;
    //         else {

    //             printf("Can not except 2D matrix of surface meshes\n");
    //             return  CAPS_BADVALUE;
    //         }

    //         if (numMesh != numBody) {
    //             printf("Number of inherited surface meshes does not match the number of bodies\n");
    //             return CAPS_SOURCEERR;
    //         }
    //         pumiInstance->numMesh =  numMesh;
    //         pumiInstance->mesh = (meshStruct * ) dataTransfer;
    //         pumiInstance->meshInherited = (int) true;
    //     } else {
    //         return status;
    //     }
    // }



    // if (0 != pumiInstance->numMesh) {
    //     printf(" PUMI AIM currently only supports one mesh!!\n");
    //     /// TODO: better return?
    //     return EGADS_INDEXERR;
    // }

    // Mesh Format
    pumiInstance->meshInput.outputFormat = EG_strdup(aimInputs[Mesh_Format-1].vals.string);
    if (pumiInstance->meshInput.outputFormat == NULL) return EGADS_MALLOC;

    // Project Name
    if (aimInputs[Proj_Name-1].nullVal != IsNull) {

        pumiInstance->meshInput.outputFileName = EG_strdup(aimInputs[Proj_Name-1].vals.string);
        if (pumiInstance->meshInput.outputFileName == NULL) return EGADS_MALLOC;
    }

    // // Output directory
    // pumiInstance->meshInput.outputDirectory = EG_strdup(analysisPath);
    // if (pumiInstance->meshInput.outputDirectory == NULL) return EGADS_MALLOC;

    // Set PUMI specific mesh inputs -1 because aim_getIndex is 1 bias
    if (aimInputs[Mesh_Order-1].nullVal != IsNull) {
        pumiInstance->meshInput.pumiInput.elementOrder = aimInputs[Mesh_Order-1].vals.integer;
    }
    printf("got inputs\n");

    // useful shorthands
    meshStruct *mesh = pumiInstance->mesh;
    printf("got mesh ptr\n");
    apf::Mesh2 *pumiMesh = pumiInstance->pumiMesh;
    printf("got pumi mesh ptr\n");
    
    ego tess;
    if (mesh->meshType == SurfaceMesh)
        tess = pumiInstance->mesh[0].bodyTessMap.egadsTess;
    else if (mesh->meshType == VolumeMesh)
        tess = pumiInstance->mesh[0].referenceMesh[0].bodyTessMap.egadsTess;
    else {
        printf(" Unknown mesh dimension, error!\n");
        return EGADS_WRITERR;
    }

    printf("got tess ptr\n");
    ego body;
    int state, npts;
    // get the body from tessellation
    printf("getting status tess body... ");
    status = EG_statusTessBody(tess, &body, &state, &npts);
    printf("got status tess body\n");
    if (status != EGADS_SUCCESS) {
#ifdef DEBUG
        printf(" EGADS Warning: Tessellation is Open (EG_saveTess)!\n");
#endif
        return EGADS_TESSTATE;
    }

    int nregions = 1;
    if (mesh->meshType == VolumeMesh) {
        
        int tetStartIndex;
        if (mesh->meshQuickRef.startIndexTetrahedral >= 0) {
            tetStartIndex = mesh->meshQuickRef.startIndexTetrahedral;
        } else {
            tetStartIndex = mesh->meshQuickRef.listIndexTetrahedral[0];
        }
        int numTets = mesh->meshQuickRef.numTetrahedral;

        /// find the number of regions
        nregions = countRegions(&(mesh->element[tetStartIndex]), numTets);
    }

    /// initialize PUMI EGADS model
    printf("loading gmi model... ");
    gmi_register_egads();
    struct gmi_model *pumiModel = gmi_egads_init(body, nregions);
    printf("loaded gmi_model\n");

    int nnode = pumiModel->n[0];
    int nedge = pumiModel->n[1];
    int nface = pumiModel->n[2];

    /// create empty PUMI mesh
    pumiMesh = apf::makeEmptyMdsMesh(pumiModel, 0, false);
    printf("empty mds mesh\n");

    // set PUMI mesh dimension based on mesh type
    if (mesh->meshType == Surface2DMesh ||
            mesh->meshType == SurfaceMesh) {
        apf::changeMdsDimension(pumiMesh, 2);
    } else if (mesh->meshType == VolumeMesh) {
        apf::changeMdsDimension(pumiMesh, 3);
    } else {
        printf(" Unknown mesh dimension, error!\n");
        return EGADS_WRITERR;
    }

    int nMeshVert = mesh->numNode;
    apf::MeshEntity *verts[nMeshVert];

    printf("create all verts\n");
    /// create all mesh vertices without model entities or parameters
    for (int i = 0; i < mesh->numNode; i++) {

        apf::Vector3 vtxCoords(mesh->node[i].xyz);
        apf::Vector3 vtxParam(0.0, 0.0, 0.0);

        verts[i] = pumiMesh->createVertex(0, vtxCoords, vtxParam);
        PCU_ALWAYS_ASSERT(verts[i]);
    }

    // apf::reorderMdsMesh(pumiMesh);

    int len, globalID, ntri;
    const int *ptype=NULL, *pindex=NULL, *ptris=NULL, *ptric=NULL;
    const double *pxyz = NULL, *puv = NULL, *pt = NULL;

    // apf::MeshEntity *ment;//, *oldMent;
    apf::ModelEntity *gent;
    apf::Vector3 param;

    printf("classify edge verts\n");
    /// classify vertices onto model edges and build mesh edges
    for (int i = 0; i < nedge; ++i) {
        apf::MeshEntity *ment;
        int edgeID = i + 1;
        status = EG_getTessEdge(tess, edgeID, &len, &pxyz, &pt);
        if (status != EGADS_SUCCESS) return status;
        for (int j = 0; j < len; ++j) {
            EG_localToGlobal(tess, -edgeID, j+1, &globalID);
            printf("global id: %d\n", globalID);
            // get the PUMI mesh vertex corresponding to the globalID
            // ment = apf::getMdsEntity(pumiMesh, 0, globalID);
            ment = verts[globalID-1];
            
            // set the model entity and parametric values
            gent = pumiMesh->findModelEntity(1, edgeID);
            pumiMesh->setModelEntity(ment, gent);
            param = apf::Vector3(pt[j], 0.0, 0.0);
            pumiMesh->setParam(ment, param);
        }
    }

    printf("classify face verts\n");
    /// classify vertices onto model faces and build surface triangles
    for (int i = 0; i < nface; ++i) {
        int faceID = i + 1;
        status = EG_getTessFace(tess, faceID, &len, &pxyz, &puv, &ptype,
                                &pindex, &ntri, &ptris, &ptric);
        if (status != EGADS_SUCCESS) return status;
        for (int j = 0; j < len; ++j) {
            if (ptype[j] > 0) // vertex is classified on a model edge
                continue;

            EG_localToGlobal(tess, faceID, j+1, &globalID);
            // get the PUMI mesh vertex corresponding to the globalID
            // apf::MeshEntity *ment = apf::getMdsEntity(pumiMesh, 0, globalID);
            apf::MeshEntity *ment = verts[globalID-1];

            // set the model entity and parametric values
            if (ptype[j] == 0) { // entity should be classified on a vertex
                gent = pumiMesh->findModelEntity(0, pindex[j]);
                param = apf::Vector3(0.0,0.0,0.0);
            }
            else {
                gent = pumiMesh->findModelEntity(2, faceID);
                param = apf::Vector3(puv[2*j], puv[2*j+1], 0.0);
            }
            pumiMesh->setModelEntity(ment, gent);
            pumiMesh->setParam(ment, param);
        }

        apf::MeshEntity *a, *b, *c;
        int aGlobalID, bGlobalID, cGlobalID;
        gent = pumiMesh->findModelEntity(2, faceID);
        // construct triangles
        for (int j = 0; j < ntri; ++j) {
            std::cout << "j: " << j << "\n";
            EG_localToGlobal(tess, faceID, ptris[3*j], &aGlobalID);
            EG_localToGlobal(tess, faceID, ptris[3*j+1], &bGlobalID);
            EG_localToGlobal(tess, faceID, ptris[3*j+2], &cGlobalID);

            std::cout << "tris: " << ptris[3*j] << ", " << ptris[3*j+1] << ", " << ptris[3*j+2] << "\n";
            // a = apf::getMdsEntity(pumiMesh, 0, aGlobalID);
            // b = apf::getMdsEntity(pumiMesh, 0, bGlobalID);
            // c = apf::getMdsEntity(pumiMesh, 0, cGlobalID);

            a = verts[aGlobalID-1];
            b = verts[bGlobalID-1];
            c = verts[cGlobalID-1];

            createEdge(pumiMesh, a, b, gent);
            createEdge(pumiMesh, b, c, gent);
            createEdge(pumiMesh, c, a, gent);

            /// construct mesh triangle face
            createFace(pumiMesh, a, b, c, gent);
        }

    }

    /// TODO: figure out how to build adjacency based on mesh and update the gmi_model
    
    /// if volume mesh, build tets
    if (mesh->meshType == VolumeMesh) {
        int elementIndex;
        apf::MeshEntity *a, *b, *c, *d;
        // apf::ModelEntity *gent;
        meshElementStruct ment;
        const int ntets = mesh->meshQuickRef.numTetrahedral;
        for (int i = 0; i < ntets; ++i) {
            if (mesh->meshQuickRef.startIndexTetrahedral >= 0) {
                elementIndex = mesh->meshQuickRef.startIndexTetrahedral + i;
            } else {
                elementIndex = mesh->meshQuickRef.listIndexTetrahedral[i];
            }
            ment = mesh->element[elementIndex];

            gent = pumiMesh->findModelEntity(3, ment.markerID);

            // a = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[0]);
            // b = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[1]);
            // c = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[2]);
            // d = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[3]);     

            a = verts[ment.connectivity[0]-1];
            b = verts[ment.connectivity[1]-1];
            c = verts[ment.connectivity[2]-1];
            d = verts[ment.connectivity[3]-1];
            apf::MeshEntity *tet_verts[4] = {a, b, c, d};

            apf::ModelEntity *g = NULL;
            for (int j = 0; j < 4; ++j) {
                g = pumiMesh->toModel(tet_verts[j]);
                if (g) { // if vtx model ent was already set (boundary vtx), skip
                    continue;
                }
                else { // if not already set, set it to be the same as the element
                    pumiMesh->setModelEntity(tet_verts[j], gent);
                }
            }             

            // std::cout << "i: " << i << std::endl;
            apf::MeshEntity *tet = apf::buildElement(pumiMesh, gent,
                                                     apf::Mesh::TET,
                                                     tet_verts);
            PCU_ALWAYS_ASSERT(tet);
            
        }
    }

    pumiMesh->acceptChanges();
    // apf::writeVtkFiles("filename", pumiMesh);

    /// calculate adjacency information and update PUMI gmi_edads model
    std::vector<std::set<int>> adj_graph[6];
    int sizes[] = {nregions, nregions, nregions, nnode, nedge, nface};

    for (int i = 0; i < 6; ++i) {
        std::set<int> empty;
        for (int j = 0; j < sizes[i]; ++j) {
            adj_graph[i].push_back(empty);
        }
    }

    /// iterate over mesh verts
    {
        apf::MeshIterator *it = pumiMesh->begin(0);
        apf::MeshEntity *vtx;
        while ((vtx = pumiMesh->iterate(it)))
        {
            apf::ModelEntity *vtx_me = pumiMesh->toModel(vtx);
            int vtx_me_dim = pumiMesh->getModelType(vtx_me);
            int vtx_me_tag = pumiMesh->getModelTag(vtx_me);
            if (vtx_me_dim == 0) { // vtx_me_dim == 0?
                apf::Adjacent tets;
                pumiMesh->getAdjacent(vtx, 3, tets);
                for (int i = 0; i < tets.getSize(); ++i)
                {
                    apf::ModelEntity *tet_me = pumiMesh->toModel(tets[i]);
                    int tet_me_tag = pumiMesh->getModelTag(tet_me);
                    std::cout << tet_me_tag << std::endl;
                    adj_graph[0][tet_me_tag-1].insert(vtx_me_tag); // 0-3
                    adj_graph[3][vtx_me_tag-1].insert(tet_me_tag); // 3-0
                }
            }
        }
    }

    {
        apf::MeshIterator *it = pumiMesh->begin(1);
        apf::MeshEntity *edge;
        while ((edge = pumiMesh->iterate(it)))
        {
            apf::ModelEntity *edge_me = pumiMesh->toModel(edge);
            int edge_me_dim = pumiMesh->getModelType(edge_me);
            int edge_me_tag = pumiMesh->getModelTag(edge_me);
            if (edge_me_dim == 1) { // edge_me_dim == 1?
                apf::Adjacent tets;
                pumiMesh->getAdjacent(edge, 3, tets);
                for (int i = 0; i < tets.getSize(); ++i)
                {
                    apf::ModelEntity *tet_me = pumiMesh->toModel(tets[i]);
                    int tet_me_tag = pumiMesh->getModelTag(tet_me);
                    adj_graph[1][tet_me_tag-1].insert(edge_me_tag); // 1-3
                    adj_graph[4][edge_me_tag-1].insert(tet_me_tag); // 3-1
                }
            }
        }
    }

    {
        apf::MeshIterator *it = pumiMesh->begin(2);
        apf::MeshEntity *tri;
        while ((tri = pumiMesh->iterate(it)))
        {
            apf::ModelEntity *tri_me = pumiMesh->toModel(tri);
            int tri_me_dim = pumiMesh->getModelType(tri_me);
            int tri_me_tag = pumiMesh->getModelTag(tri_me);
            if (tri_me_dim < 3) { // edge_me_dim == 1?
                apf::Up tets;
                pumiMesh->getUp(tri, tets);
                for (int i = 0; i < tets.n; ++i)
                {
                    apf::ModelEntity *tet_me = pumiMesh->toModel(tets.e[i]);
                    int tet_me_tag = pumiMesh->getModelTag(tet_me);
                    adj_graph[2][tet_me_tag-1].insert(tri_me_tag); // 2-3
                    adj_graph[5][tri_me_tag-1].insert(tet_me_tag); // 3-2
                }
            }
        }
    }

    /// create C array adjacency graph
    int **c_graph[6];

    for (int i = 0; i < 6; ++i) {
        c_graph[i] = (int**)EG_alloc(sizeof(*c_graph[i]) * sizes[i]);
        if (c_graph[i] == NULL) {
            printf("failed to alloc memory for c_graph");
            /// return bad here
        }
        for (int j = 0; j < sizes[i]; ++j) {
            c_graph[i][j] = (int*)EG_alloc(sizeof(*c_graph[i][j])
                                           * (adj_graph[i][j].size()+1));
            if (c_graph[i][j] == NULL) {
                printf("failed to alloc memory for c_graph");
                /// return bad here
            }
            c_graph[i][j][0] = adj_graph[i][j].size();
            std::copy(adj_graph[i][j].begin(),
                      adj_graph[i][j].end(),
                      c_graph[i][j]+1);
        }
    }

    gmi_egads_init_adjacency(pumiModel, c_graph);

    pumiMesh->verify();

    int elementOrder = pumiInstance->meshInput.pumiInput.elementOrder;
    if (elementOrder > 1) {
        crv::BezierCurver bc(pumiMesh, elementOrder, 2);
        bc.run();
    }

    if (pumiInstance->meshInput.outputFileName != NULL) {

        char *filename = (char *) EG_alloc((strlen(pumiInstance->meshInput.outputFileName)+2)*sizeof(char));
        if (filename == NULL) return EGADS_MALLOC;

        strcpy(filename, pumiInstance->meshInput.outputFileName);

        if (strcasecmp(pumiInstance->meshInput.outputFormat, "PUMI") == 0) {
            /// write .smb mesh file
            std::string smb_filename(filename);
            smb_filename += ".smb";
            pumiMesh->writeNative(smb_filename.c_str());

            /// write native tesselation
            std::string tess_filename(filename);
            tess_filename += ".eto";

            /// remove existing tesselation file if it exists
            {
                std::ifstream tess_file(tess_filename.c_str());
                if (tess_file.good())
                    std::remove(tess_filename.c_str());
            }
            status = EG_saveTess(tess, tess_filename.c_str());
            if (status != EGADS_SUCCESS)
                printf(" PUMI AIM Warning: EG_saveTess failed with status: %d!\n", status);

            /// write EGADS model file
            std::string model_filename(filename);
            model_filename += ".egads";

            // Get context
            ego context;
            status = EG_getContext(body, &context);
            if (status != EGADS_SUCCESS)
                printf(" PUMI AIM Warning: EG_getContext failed with status: %d!\n", status);

            ego *bodyCopy = (ego *) EG_alloc(sizeof(ego));
            if (bodyCopy == NULL) {
                status = EGADS_MALLOC;
                if (status != EGADS_SUCCESS)
                    printf(" PUMI AIM Warning: EG_alloc failed with status: %d!\n", status);
            }

            *bodyCopy = NULL;

            status = EG_copyObject(body, NULL, bodyCopy);
            // if (status != EGADS_SUCCESS) goto cleanup;

            // Create a model
            ego model;
            status = EG_makeTopology(context, NULL, MODEL, 0, NULL, numBody, bodyCopy, NULL, &model);
            if (status != EGADS_SUCCESS)
                printf(" PUMI AIM Warning: EG_makeTopology failed with status: %d!\n", status);

            /// remove existing model file if it exists
            {
                std::ifstream model_file(model_filename.c_str());
                if (model_file.good())
                    std::remove(model_filename.c_str());
            }
            status = EG_saveModel(model, model_filename.c_str());
            if (status != EGADS_SUCCESS)
                printf(" PUMI AIM Warning: EG_saveModel failed with status: %d!\n", status);

            // delete the model
            if (model != NULL) {
            EG_deleteObject(model);
            } else {
            if (bodyCopy != NULL) {
                if (*bodyCopy != NULL)  {
                    (void) EG_deleteObject(*bodyCopy);
                }
            }
            }
            EG_free(bodyCopy);

            // /// try to load the tess here
            // {
            //     ego load_tess;
            //     status = EG_loadTess(body, tess_filename.c_str(), &load_tess);
            //     if (status != EGADS_SUCCESS)
            //         printf(" PUMI AIM Warning: EG_saveTess failed with status: %d!\n", status);
            //     else
            //         printf("Loaded tess!\n");
            // }

            // /// try to load the tess and model
            // {
            //     ego load_model;
            //     status = EG_loadModel(context, 0, model_filename.c_str(), &load_model);
            //     if (status != EGADS_SUCCESS)
            //     {
            //         printf("EGADS failed to load model with error code: %d", status);
            //     }

            //     int oclass, mtype, nbody, *senses;
            //     ego geom, *eg_bodies;
            //     status = EG_getTopology(load_model, &geom, &oclass, &mtype, NULL, &nbody,
            //                             &eg_bodies, &senses);
            //     if (status != EGADS_SUCCESS)
            //     {
            //         printf("EGADS failed to get bodies with error code: %d", status);
            //     }
            //     else if (nbody > 1)
            //     {
            //         printf("EGADS model should only have one body");
            //         return -1;
            //     }                

            //     ego load_body = eg_bodies[0];

            //     ego load_tess;
            //     status = EG_loadTess(load_body, tess_filename.c_str(), &load_tess);
            //     if (status != EGADS_SUCCESS)
            //         printf(" PUMI AIM Warning: EG_saveTess failed with status: %d!\n", status);
            //     else
            //         printf("Loaded tess and model!\n");
            // }

            /// write adjacency table
            std::string adj_filename(filename);
            adj_filename += ".egads.sup";

            /// Binary
            std::ofstream adj_file(adj_filename.c_str(),
                                   std::ios::out | std::ios::binary);

            /// ASCII
            // std::ofstream adj_file(adj_filename.c_str());
            PCU_ALWAYS_ASSERT(adj_file.is_open());

            /// write header
            /// Binary
            adj_file.write(reinterpret_cast<const char *>(sizes),
                           sizeof(sizes[0])*6);

            /// ASCII
            // adj_file << nregions << "," << nregions "," << nregions << ",";
            // adj_file << nnode << "," << nedge << "," << nface  << std::endl;

            /// write adjacency table
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < sizes[i]; ++j) {
                    /// Binary file
                    auto n = (int)adj_graph[i][j].size();
                    adj_file.write(reinterpret_cast<char*>(&n),
                                   sizeof(n));
                    for (auto elem : adj_graph[i][j]) {
                        adj_file.write(reinterpret_cast<char*>(&elem),
                                       sizeof(elem));
                    }

                    /// ASCII file
                    // adj_file << adj_graph[i][j].size();
                    // for (auto elem : adj_graph[i][j]) {
                    //     adj_file << "," << elem;
                    // }
                    // adj_file << std::endl;
                }
            }
            adj_file.close();


        } else if (strcasecmp(pumiInstance->meshInput.outputFormat, "VTK") == 0) {
            if (elementOrder > 1) {
                if (mesh->meshType == VolumeMesh)
                    crv::writeCurvedVtuFiles(pumiMesh, apf::Mesh::TET, 10, filename);
                else
                    crv::writeCurvedVtuFiles(pumiMesh, apf::Mesh::TRIANGLE, 10, filename);
                crv::writeCurvedWireFrame(pumiMesh, 10, filename);
            } else {
                apf::writeVtkFiles(filename, pumiMesh);
            }
        }
        if (filename != NULL) EG_free(filename);
    }


    return status;
}

extern "C" int
aimOutputs(/*@unused@*/ void *instStore, /*@unused@*/ void *aimInfo, int index,
           char **aoname, capsValue *form)
{
    /*! \page aimOutputsPUMI AIM Outputs
     * The following list outlines the PUMI AIM outputs available through the AIM interface.
     *
     * - <B>Done</B> = True if a PUMI mesh(es) was created, False if not.
     */
    int status = CAPS_SUCCESS;

#ifdef DEBUG
    printf(" pumiAIM/aimOutputs index = %d!\n", index);
#endif

    if (index == Done) {

        *aoname = EG_strdup("Done");
        form->type = Boolean;
        form->vals.integer = (int) false;

    } else {
        status = CAPS_BADINDEX;
        AIM_STATUS(aimInfo, status, "Unknown output index %d!", index);
    }

cleanup:
    if (status != CAPS_SUCCESS) AIM_FREE(*aoname);
    return status;
}

extern "C" int
aimCalcOutput(void *instStore, void *aimInfo, int index, capsValue *val)
{
    int status = CAPS_SUCCESS;
    aimStorage *pumiInstance;

#ifdef DEBUG
    printf(" pumiAIM/aimCalcOutput index = %d!\n", index);
#endif
    pumiInstance = (aimStorage *) instStore;

    if (Done == index) {
        val->vals.integer = (int) false;

        // Check to see if a PUMI mesh was generated - maybe a file was written, maybe not
        for (int i = 0; i < pumiInstance->numPUMIMesh; i++) {
            // Check to see if the mesh was generated with the same number of entities
            if (pumiInstance->pumiMesh[i].count(3) == pumiInstance->mesh->meshQuickRef.numTetrahedral)
                if (pumiInstance->pumiMesh[i].count(2) == pumiInstance->mesh->meshQuickRef.numTriangle)
                    if (pumiInstance->pumiMesh[i].count(1) == pumiInstance->mesh->meshQuickRef.numLine)
                        if (pumiInstance->pumiMesh[i].count(0) == pumiInstance->mesh->meshQuickRef.numNode) {
                            val->vals.integer = (int) true;
                        }
                        else {
                            val->vals.integer = (int) false;
                            if (pumiInstance->numPUMIMesh > 1) {
                                printf("Inconsistent vertex elements were constructed for PUMI mesh %d\n", i+1);
                            } else {
                                printf("Inconsistent vertex elements were constructed for PUMI mesh\n");
                            }
                            return CAPS_SUCCESS;
                        }
                    else {
                        val->vals.integer = (int) false;
                        if (pumiInstance->numPUMIMesh > 1) {
                            printf("Inconsistent edge elements were constructed for PUMI mesh %d\n", i+1);
                        } else {
                            printf("Inconsistent edge elements were constructed for PUMI mesh\n");
                        }
                        return CAPS_SUCCESS;
                    }
                else {
                    val->vals.integer = (int) false;
                    if (pumiInstance->numPUMIMesh > 1) {
                        printf("Inconsistent triangle elements were constructed for PUMI mesh %d\n", i+1);
                    } else {
                        printf("Inconsistent triangle elements were constructed for PUMI mesh\n");
                    }
                    return CAPS_SUCCESS;
                }
            else {
                val->vals.integer = (int) false;
                if (pumiInstance->numPUMIMesh > 1) {
                    printf("Inconsistent tet elements were constructed for PUMI mesh %d\n", i+1);
                } else {
                    printf("Inconsistent tet elements were constructed for PUMI mesh\n");
                }
                return CAPS_SUCCESS;
            }
        }
    } else {

        status = CAPS_BADINDEX;
        AIM_STATUS(aimInfo, status, "Unknown output index %d!", index);

    }

cleanup:

    return status;
}

extern "C" void
aimCleanup( void *instStore )
{
    int status; // Function return status
    aimStorage *pumiInstance;
#ifdef DEBUG
    printf(" pumiAIM/aimCleanup!\n");
#endif
    pumiInstance = (aimStorage *) instStore;

    // Clean up pumiInstance data
    status = destroy_aimStorage(pumiInstance);
    if (status != CAPS_SUCCESS)
        printf("Status = %d, pumiAIM aimStorage cleanup!!!\n", status);

    if (pumiInstance != NULL) EG_free(pumiInstance);
    pumiInstance = NULL;

    int mpi_flag;
    MPI_Finalized(&mpi_flag);
    if (!mpi_flag) {
        PCU_Comm_Free();
        MPI_Finalize();
    }
}
