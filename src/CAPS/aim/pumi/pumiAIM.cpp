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
#include "aimMesh.h"
#include "meshTypes.h"
#include "miscTypes.h"
#include "meshUtils.h"  // Collection of helper functions for meshing
#include "miscUtils.h"  // Collection of helper functions for miscellaneous use

#include "ugridWriter.h"

/** \brief initialize a gmi_model with filestream and number of regions */
extern "C"
struct gmi_model* gmi_egads_init_model(ego eg_model, int nregions);

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
  Surface_Mesh,
  Volume_Mesh,
  Mesh_Order,
  NUMINPUT = Mesh_Order    /* Total number of inputs */
};

enum aimOutputs
{
  Done = 1,                    /* index is 1-based */
  NUMOUT = Done         /* Total number of outputs */
};

typedef struct {

    // container for inherited mesh
    int numMesh;
    meshStruct *mesh;

    int numPUMIMesh;
    apf::Mesh2 *pumiMesh;

    // Container for mesh input
    meshInputStruct meshInput;

    // Attribute to index map
    mapAttrToIndexStruct attrMap;

    // Mesh references for link
    int numMeshRef;
    aimMeshRef *meshRef;

    aimMesh aimMesh;

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

int countRegions(const std::vector<int> &region_ids)
{
    std::set<int> unique_ids(region_ids.begin(), region_ids.end());
    return unique_ids.size();
}

int getElementRegionIDs(void *aimInfo,
                        aimMeshRef *mesh_ref,
                        std::vector<int> &region_ids)
{
    int nvert = 0;
    int ntri =0;
    int nquad = 0;
    int ntet = 0;
    int npyr = 0;
    int nprism = 0;
    int nhex = 0;

    int status = aim_readBinaryUgridHeader(aimInfo,
                                           mesh_ref,
                                           &nvert,
                                           &ntri,
                                           &nquad,
                                           &ntet,
                                           &npyr,
                                           &nprism,
                                           &nhex);

    std::string ugrid_filename = mesh_ref->fileName;
    ugrid_filename += ".lb8.ugrid";
    auto *fp = fopen(ugrid_filename.c_str(), "rb");
    if (fp == nullptr)
    {
        AIM_ERROR(aimInfo, "Cannot open file: %s\n", ugrid_filename.c_str());
        throw std::runtime_error("failed to open file!\n");
    }

    // skip the header
    status = fseek(fp, 7*sizeof(int), SEEK_CUR);
    // skip the verts
    status += fseek(fp, nvert*3*sizeof(double), SEEK_CUR);
    // skip the surface elements
    status += fseek(fp, (3*ntri+4*nquad)*sizeof(int), SEEK_CUR);
    // skip face BC ID section of the file
    status += fseek(fp, (ntri + nquad)*sizeof(int), SEEK_CUR);
    // skip the volume elements
    status += fseek(fp, (4*ntet)*sizeof(int), SEEK_CUR);
    status += fseek(fp, (5*npyr)*sizeof(int), SEEK_CUR);
    status += fseek(fp, (6*nprism)*sizeof(int), SEEK_CUR);
    status += fseek(fp, (8*nhex)*sizeof(int), SEEK_CUR);
    // // skip BL volume tets
    // status += fseek(fp, sizeof(int), SEEK_CUR);
    if (status != 0)
    {
        throw std::runtime_error("file error!\n");
    }

    int n_vol_elems = ntet+npyr+nprism+nquad;
    region_ids.resize(n_vol_elems);
    status = fread(&region_ids[0], sizeof(int), n_vol_elems, fp);
    if (status != n_vol_elems)
    {
        throw std::runtime_error("file error!\n");
    }

    return countRegions(region_ids);
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

apf::Mesh2 *createPumiMeshFromMeshRef(void *aimInfo,
                                      aimMeshRef *mesh_ref,
                                      ego &model,
                                      ego &tess,
                                      std::vector<std::set<int>> *adj_graph)
{
    std::vector<int> region_ids;
    auto n_model_regions = getElementRegionIDs(aimInfo, mesh_ref, region_ids);

    /// initialize PUMI EGADS model
    gmi_register_egads();
    struct gmi_model *pumi_model = gmi_egads_init_model(model, n_model_regions);

    aimMesh mesh{.meshData = nullptr, .meshRef = mesh_ref};
    int status = aim_readBinaryUgrid(aimInfo, &mesh);
    if (status != CAPS_SUCCESS) { return nullptr; }
    auto *mesh_data = mesh.meshData;

    /// create empty 3D PUMI mesh
    auto *pumi_mesh = apf::makeEmptyMdsMesh(pumi_model, mesh_data->dim, false);


    /// create all mesh vertices without model entities or parameters
    int n_mesh_verts = mesh_data->nVertex;
    std::vector<apf::MeshEntity *> verts(n_mesh_verts);
    for (int i = 0; i < n_mesh_verts; i++) {

        apf::Vector3 vtxCoords(mesh_data->verts[i]);
        if (i == 30)
        {
            std::cout << "";
        }
        apf::Vector3 vtxParam(0.0, 0.0, 0.0);

        verts[i] = pumi_mesh->createVertex(0, vtxCoords, vtxParam);
        PCU_ALWAYS_ASSERT(verts[i]);
    }

    /// classify vertices onto model edges and build mesh edges
    int n_model_edge = pumi_model->n[1];
    for (int i = 0; i < n_model_edge; ++i)
    {
        int edgeID = i + 1;
        int len;
        const double *pxyz = nullptr;
        const double *pt = nullptr;
        status = EG_getTessEdge(tess, edgeID, &len, &pxyz, &pt);
        if (status != EGADS_SUCCESS) { return nullptr; }
        for (int j = 0; j < len; ++j)
        {
            int globalID;
            status = EG_localToGlobal(tess, -edgeID, j+1, &globalID);
            if (status == EGADS_DEGEN)
            {
                break; // skip degenerate edges
            }
            else if (status != EGADS_SUCCESS) 
            {
                return nullptr;
            }
   
            // get the PUMI mesh vertex corresponding to the tess globalID
            // auto *ment = verts[globalID-1];
            auto *ment = verts[mesh_ref->maps->map[globalID-1]-1];
            // {
            //     apf::Vector3 coords;
            //     pumi_mesh->getPoint(ment, 0, coords);
            //     for (int j = 0; j < 3; ++j)
            //     {
            //         if (abs(coords[i] - pxyz[i]) > 1e-12)
            //         {
            //             gmi_fail("bad coordinates for mesh point!\n");
            //         }
            //     }
            // }

            // set the model entity and parametric values
            auto *gent = pumi_mesh->findModelEntity(1, edgeID);
            pumi_mesh->setModelEntity(ment, gent);
            auto param = apf::Vector3(pt[j], 0.0, 0.0);
            pumi_mesh->setParam(ment, param);
        }
    }

    /// classify vertices onto model faces and build surface triangles
    int n_model_face = pumi_model->n[2];
    for (int i = 0; i < n_model_face; ++i)
    {
        int faceID = i + 1;
        int len;
        const double *pxyz = nullptr;
        const double *puv = nullptr;
        const int *ptype;
        const int *pindex;
        int ntri;
        const int *ptris;
        const int *ptric;
        status = EG_getTessFace(tess, faceID, &len, &pxyz, &puv, &ptype,
                                &pindex, &ntri, &ptris, &ptric);
        if (status != EGADS_SUCCESS) { return nullptr; }
        for (int j = 0; j < len; ++j)
        {
            if (ptype[j] > 0) // vertex is classified on a model edge
            {
                continue;
            }

            int globalID;
            status = EG_localToGlobal(tess, faceID, j+1, &globalID);
            if (status != EGADS_SUCCESS) { return nullptr; }

            // get the PUMI mesh vertex corresponding to the tess globalID
            auto *ment = verts[mesh_ref->maps->map[globalID-1]-1];
            // {
            //     apf::Vector3 coords;
            //     pumi_mesh->getPoint(ment, 0, coords);
            //     for (int j = 0; j < 3; ++j)
            //     {
            //         if (abs(coords[i] - pxyz[i]) > 1e-12)
            //         {
            //             gmi_fail("bad coordinates for mesh point!\n");
            //         }
            //     }
            // }

            // set the model entity and parametric values
            apf::ModelEntity *gent;
            apf::Vector3 param;
            if (ptype[j] == 0) // entity should be classified on a vertex
            {
                gent = pumi_mesh->findModelEntity(0, pindex[j]);
                param = apf::Vector3(0.0,0.0,0.0);
            }
            else
            {
                gent = pumi_mesh->findModelEntity(2, faceID);
                param = apf::Vector3(puv[2*j], puv[2*j+1], 0.0);
            }
            pumi_mesh->setModelEntity(ment, gent);
            pumi_mesh->setParam(ment, param);
        }

        // construct triangles
        for (int j = 0; j < ntri; ++j)
        {
            int aGlobalID, bGlobalID, cGlobalID;
            EG_localToGlobal(tess, faceID, ptris[3*j], &aGlobalID);
            EG_localToGlobal(tess, faceID, ptris[3*j+1], &bGlobalID);
            EG_localToGlobal(tess, faceID, ptris[3*j+2], &cGlobalID);

            auto *a = verts[mesh_ref->maps->map[aGlobalID-1]-1];
            auto *b = verts[mesh_ref->maps->map[bGlobalID-1]-1];
            auto *c = verts[mesh_ref->maps->map[cGlobalID-1]-1];

            auto *gent = pumi_mesh->findModelEntity(2, faceID);
            createEdge(pumi_mesh, a, b, gent);
            createEdge(pumi_mesh, b, c, gent);
            createEdge(pumi_mesh, c, a, gent);

            /// construct mesh triangle face
            createFace(pumi_mesh, a, b, c, gent);
        }

    }

    /// build tets
    auto ngroups = mesh_data->nElemGroup;
    auto [group_id, ntet] = [&]() -> std::pair<int, int>
    {
        for (int i = 0; i < ngroups; ++i)
        {
            auto element_topo = mesh_data->elemGroups[i].elementTopo;
            if (element_topo == aimTet)
            {
                return {i, mesh_data->elemGroups[i].nElems};
            }
        }
        throw std::runtime_error("pumiAIM failed!\n");
    }();

    for (int i = 0; i < ntet; ++i) {

        auto *connectivity = mesh_data->elemGroups[group_id].elements;
        auto *a = verts[connectivity[i * 4 + 0]-1];
        auto *b = verts[connectivity[i * 4 + 1]-1];
        auto *c = verts[connectivity[i * 4 + 2]-1];
        auto *d = verts[connectivity[i * 4 + 3]-1];
        apf::MeshEntity *tet_verts[] = {a, b, c, d};

        auto *gent = pumi_mesh->findModelEntity(3, region_ids[i]);
        for (int j = 0; j < 4; ++j) {
            auto *g = pumi_mesh->toModel(tet_verts[j]);
            if (g) // if vtx model ent was already set (boundary vtx), skip
            {
                continue;
            }
            else  // if not already set, set it to be the same as the element
            {
                pumi_mesh->setModelEntity(tet_verts[j], gent);
            }
        }             

        auto *tet = apf::buildElement(pumi_mesh, gent,
                                      apf::Mesh::TET,
                                      tet_verts);
        PCU_ALWAYS_ASSERT(tet);
    }

    pumi_mesh->acceptChanges();

    /// calculate adjacency information and update PUMI gmi_edads model
    int n_model_node = pumi_model->n[0];
    // std::vector<std::set<int>> adj_graph[6];
    int sizes[] = {n_model_regions,
                   n_model_regions,
                   n_model_regions,
                   n_model_node,
                   n_model_edge,
                   n_model_face};

    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < sizes[i]; ++j) {
            adj_graph[i].push_back(std::set<int>{});
        }
    }

    /// iterate over mesh verts
    {
        auto *it = pumi_mesh->begin(0);
        apf::MeshEntity *vtx;
        while ((vtx = pumi_mesh->iterate(it)))
        {
            auto *vtx_model_ent = pumi_mesh->toModel(vtx);
            int vtx_me_dim = pumi_mesh->getModelType(vtx_model_ent);
            int vtx_me_tag = pumi_mesh->getModelTag(vtx_model_ent);
            if (vtx_me_dim == 0)
            {
                apf::Adjacent tets;
                pumi_mesh->getAdjacent(vtx, 3, tets);
                for (int i = 0; i < tets.getSize(); ++i)
                {
                    auto *tet_model_ent = pumi_mesh->toModel(tets[i]);
                    int tet_me_tag = pumi_mesh->getModelTag(tet_model_ent);
                    adj_graph[0][tet_me_tag-1].insert(vtx_me_tag); // 0-3
                    adj_graph[3][vtx_me_tag-1].insert(tet_me_tag); // 3-0
                }
            }
        }
    }

    /// iterate over mesh edges
    {
        auto *it = pumi_mesh->begin(1);
        apf::MeshEntity *edge;
        while ((edge = pumi_mesh->iterate(it)))
        {
            auto *edge_model_ent = pumi_mesh->toModel(edge);
            int edge_me_dim = pumi_mesh->getModelType(edge_model_ent);
            int edge_me_tag = pumi_mesh->getModelTag(edge_model_ent);
            if (edge_me_dim == 1)
            {
                apf::Adjacent tets;
                pumi_mesh->getAdjacent(edge, 3, tets);
                for (int i = 0; i < tets.getSize(); ++i)
                {
                    auto *tet_model_ent = pumi_mesh->toModel(tets[i]);
                    int tet_me_tag = pumi_mesh->getModelTag(tet_model_ent);
                    adj_graph[1][tet_me_tag-1].insert(edge_me_tag); // 1-3
                    adj_graph[4][edge_me_tag-1].insert(tet_me_tag); // 3-1
                }
            }
        }
    }

    /// iterate over mesh faces
    {
        auto *it = pumi_mesh->begin(2);
        apf::MeshEntity *tri;
        while ((tri = pumi_mesh->iterate(it)))
        {
            auto *tri_model_ent = pumi_mesh->toModel(tri);
            int tri_me_dim = pumi_mesh->getModelType(tri_model_ent);
            int tri_me_tag = pumi_mesh->getModelTag(tri_model_ent);
            if (tri_me_dim < 3)
            {
                apf::Up tets;
                pumi_mesh->getUp(tri, tets);
                for (int i = 0; i < tets.n; ++i)
                {
                    auto *tet_model_ent = pumi_mesh->toModel(tets.e[i]);
                    int tet_me_tag = pumi_mesh->getModelTag(tet_model_ent);
                    adj_graph[2][tet_me_tag-1].insert(tri_me_tag); // 2-3
                    adj_graph[5][tri_me_tag-1].insert(tet_me_tag); // 3-2
                }
            }
        }
    }

    /// create C array adjacency graph
    int **c_graph[6];

    for (int i = 0; i < 6; ++i)
    {
        c_graph[i] = (int**)EG_alloc(sizeof(*c_graph[i]) * sizes[i]);
        if (c_graph[i] == nullptr)
        {
            printf("failed to alloc memory for c_graph");
            /// return bad here
        }
        for (int j = 0; j < sizes[i]; ++j)
        {
            c_graph[i][j] = (int*)EG_alloc(sizeof(*c_graph[i][j])
                                           * (adj_graph[i][j].size()+1));
            if (c_graph[i][j] == nullptr)
            {
                printf("failed to alloc memory for c_graph");
                /// return bad here
            }
            c_graph[i][j][0] = adj_graph[i][j].size();
            std::copy(adj_graph[i][j].begin(),
                      adj_graph[i][j].end(),
                      c_graph[i][j]+1);
        }
    }

    gmi_egads_init_adjacency(pumi_model, c_graph);
    printf("Done generating PUMI mesh!\n");

    printf("\nVerifying PUMI mesh.....\n");
    pumi_mesh->verify();
    printf("PUMI mesh verified!\n");

    return pumi_mesh;
}

static int destroy_aimStorage(aimStorage *pumiInstance) {

    int i; // Indexing

    int status; // Function return status

    // Destroy meshInput
    status = destroy_meshInputStruct(&pumiInstance->meshInput);
    if (status != CAPS_SUCCESS)
        printf("Status = %d, pumiAIM meshInput cleanup!!!\n", status);

    // Destroy PUMI mesh
    if (pumiInstance->pumiMesh != NULL) {
        for (i = 0; i < pumiInstance->numPUMIMesh; i++) {
            pumiInstance->pumiMesh->destroyNative();
            apf::destroyMesh(pumiInstance->pumiMesh);
        }
    }

    // Destroy attribute to index map
    status = destroy_mapAttrToIndexStruct(&pumiInstance->attrMap);
    if (status != CAPS_SUCCESS)
        printf("Status = %d, pumi4AIM, attributeMap cleanup!!!\n", status);

    // Free the meshRef
    for (i = 0; i < pumiInstance->numMeshRef; i++)
      aim_freeMeshRef(&pumiInstance->meshRef[i]);
    AIM_FREE(pumiInstance->meshRef);

    return CAPS_SUCCESS;
}

/* ********************** Exposed AIM Functions ***************************** */

extern "C" int
aimInitialize(int inst, /*@null@*/ const char *unitSys, void *aimInfo,
              void **instStore, int *maj, int *min, int *nIn, int *nOut,
              int *nFields, char ***fnames, int **franks, int **fInOut)
{   
    int mpi_flag;
    MPI_Initialized(&mpi_flag);
    if (!mpi_flag) {
        MPI_Init(NULL, NULL);
        PCU_Comm_Init();
    }
    gmi_egads_start();

    int status; // Function status return

#ifdef DEBUG
    printf("\n pumiAIM/aimInitialize   ngIn = %d!\n", ngIn);
#endif

    /* specify the number of analysis input and out "parameters" */
    *nIn = NUMINPUT;
    *nOut = NUMOUT;
    if (inst == -1) return CAPS_SUCCESS;

    /* specify the field variables this analysis can generate */
    *nFields = 0;
    *franks = nullptr;
    *fnames = nullptr;
    *fInOut = nullptr;

    // Allocate pumiInstance
    auto *pumiInstance = (aimStorage *) EG_alloc(sizeof(aimStorage));
    if (pumiInstance == NULL) return EGADS_MALLOC;

    // Set initial values for pumiInstance //

    // container for input mesh
    pumiInstance->numMesh = 0;
    pumiInstance->mesh = nullptr;

    // Mesh reference passed to solver
    pumiInstance->numMeshRef = 0;
    pumiInstance->meshRef = nullptr;

    // Container for mesh
    pumiInstance->numPUMIMesh = 0;
    pumiInstance->pumiMesh = nullptr;

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

    } else if (index == Surface_Mesh) {
        *ainame             = AIM_NAME(Surface_Mesh);
        defval->type        = Pointer;
        defval->dim         = Vector;
        defval->lfixed      = Change;
        defval->sfixed      = Change;
        defval->vals.AIMptr = NULL;
        defval->nullVal     = IsNull;
        AIM_STRDUP(defval->units, "meshStruct", aimInfo, status);

        /*! \page aimInputsPUMI
         * - <B>Surface_Mesh = NULL</B> <br>
         * A Surface_Mesh link for 2D geometries.
         */

    } else if (index == Volume_Mesh) {
        *ainame             = AIM_NAME(Volume_Mesh);
        defval->type        = PointerMesh;
        defval->dim         = Vector;
        defval->lfixed      = Change;
        defval->sfixed      = Fixed;
        defval->vals.AIMptr = NULL;
        defval->nullVal     = IsNull;
        AIM_STRDUP(defval->meshWriter, MESHWRITER, aimInfo, status);

        /*! \page aimInputsPUMI
         * - <B>Volume_Mesh = NULL</B> <br>
         * A Volume_Mesh link for 3D geometries.
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

    } else {
        status = CAPS_BADINDEX;
        AIM_STATUS(aimInfo, status, "Unknown input index %d!", index);
    }

    AIM_NOTNULL(*ainame, aimInfo, status);

cleanup:
    if (status != CAPS_SUCCESS) AIM_FREE(*ainame);
    return status;
}

//// Have to update PUMI AIM to new CAPS structure
//// Modeling it based off of tetgen aim and aflr3 aims, shouldn't be too many changes to make

extern "C" int
aimPreAnalysis(void *instStore, void *aimInfo, capsValue *aimInputs)
{
    int status; // Return status

    // Incoming bodies
    const char *intents;
    ego *bodies = nullptr;
    int numBody = 0;

    printf("\nGenerating PUMI mesh.....\n");

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

    auto *pumiInstance = static_cast<aimStorage *>(instStore);

    // remove previous meshes
    for (int ibody = 0; ibody < pumiInstance->numMeshRef; ibody++) {
        status = aim_deleteMeshes(aimInfo, &pumiInstance->meshRef[ibody]);
        aim_status(aimInfo, status, __FILE__, __LINE__, __func__, GET_ARG_COUNT());
        if (status != CAPS_SUCCESS) return status;
    }

    // Cleanup previous aimStorage for the instance in case this is the second time through preAnalysis for the same instance
    status = destroy_aimStorage(pumiInstance);
    aim_status(aimInfo, status, __FILE__, __LINE__, __func__, GET_ARG_COUNT());
    if (status != CAPS_SUCCESS) return status;

    // Get capsGroup name and index mapping
    status = create_CAPSGroupAttrToIndexMap(numBody,
                                            bodies,
                                            2, // Only search down to the edge level of the EGADS body
                                            &pumiInstance->attrMap);
    if (status != CAPS_SUCCESS) return status;

    // Get input mesh
    bool surface_mesh = !(aimInputs[Surface_Mesh-1].nullVal == IsNull);
    bool volume_mesh = !(aimInputs[Volume_Mesh-1].nullVal == IsNull);
    if (!(surface_mesh || volume_mesh)) {
        AIM_ANALYSISIN_ERROR(aimInfo, Surface_Mesh,
        "'Surface_Mesh' input must be linked to an output 'Surface_Mesh' or "
        "'Volume_Mesh' input must be linked to an output 'Volume_Mesh'");
        return CAPS_BADVALUE;
    }
    if (surface_mesh && volume_mesh) {
        AIM_ANALYSISIN_ERROR(aimInfo, Surface_Mesh,
        "Can only link either 'Surface_Mesh OR Volume_Mesh, not both!\n");
        return CAPS_BADVALUE;
    }

    auto Mesh = surface_mesh ? Surface_Mesh : Volume_Mesh;

    pumiInstance->numMesh = aimInputs[Mesh-1].length;

    if (surface_mesh)
    {
        pumiInstance->mesh = static_cast<meshStruct *>(aimInputs[Mesh-1].vals.AIMptr);
    }
    else
    {
        pumiInstance->meshRef = static_cast<aimMeshRef *>(aimInputs[Mesh-1].vals.AIMptr);
    }

    if (pumiInstance->numMesh != numBody) {
        AIM_ANALYSISIN_ERROR(aimInfo, Mesh, "Number of linked meshes (%d) does not match the number of bodies (%d)!",
                             pumiInstance->numMesh, numBody);
        return CAPS_SOURCEERR;
    }

    // Mesh Format
    pumiInstance->meshInput.outputFormat = EG_strdup(aimInputs[Mesh_Format-1].vals.string);
    if (pumiInstance->meshInput.outputFormat == nullptr) return EGADS_MALLOC;

    // Project Name
    if (aimInputs[Proj_Name-1].nullVal != IsNull)
    {
        pumiInstance->meshInput.outputFileName = EG_strdup(aimInputs[Proj_Name-1].vals.string);
        if (pumiInstance->meshInput.outputFileName == nullptr) return EGADS_MALLOC;
    }

    // Set PUMI specific mesh inputs
    if (aimInputs[Mesh_Order-1].nullVal != IsNull) {
        pumiInstance->meshInput.pumiInput.elementOrder = aimInputs[Mesh_Order-1].vals.integer;
    }

    /// Get EGADS Model
    auto tess = pumiInstance->meshRef->maps->tess;

    ego tessBody;
    int state, npts;
    // get the body from tessellation
    status = EG_statusTessBody(tess, &tessBody, &state, &npts);
    if (status != EGADS_SUCCESS) { return status; }

    /// Copy body from tesselation and create a model to import into PUMI
    ego model;
    ego context;
    status = EG_getContext(tessBody, &context);
    if (status != EGADS_SUCCESS) { return status; }

    ego *body = (ego *) EG_alloc(sizeof(ego));
    if (body == nullptr) { return EGADS_MALLOC; }

    *body = nullptr;
    status = EG_copyObject(tessBody, nullptr, body);
    if (status != EGADS_SUCCESS) { return status; }

    status = EG_makeTopology(context, nullptr, MODEL, 0, nullptr, 
                             numBody, body, nullptr, &model);
    if (status != EGADS_SUCCESS) { return status; }

    if (surface_mesh)
    {
        throw std::runtime_error("pumiAIM doesn't support surface mesh!\n");
    }

    std::vector<std::set<int>> adj_graph[6];
    auto *pumi_mesh = createPumiMeshFromMeshRef(aimInfo,
                                                pumiInstance->meshRef,
                                                model,
                                                tess,
                                                adj_graph);
    
    int sizes[6];
    for (int i = 0; i < 6; ++i)
    {
        sizes[i] = adj_graph[i].size();
    }



//     // auto mesh = pumiInstance->mesh[0];
//     auto tess = [&]() -> ego {
//         if (surface_mesh)
//             return pumiInstance->mesh[0].egadsTess;
//         else if (volume_mesh)
//             return pumiInstance->meshRef->maps->tess;
//         else {
//             return nullptr;
//         }
//     }();
//     if (tess == nullptr)
//     {
//         printf(" Unknown mesh dimension, error!\n");
//         return EGADS_WRITERR;
//     }

//     ego tessBody;
//     int state, npts;
//     // get the body from tessellation
//     status = EG_statusTessBody(tess, &tessBody, &state, &npts);
//     if (status != EGADS_SUCCESS) {
// #ifdef DEBUG
//         printf(" EGADS Warning: Tessellation is Open (EG_saveTess)!\n");
// #endif
//         return EGADS_TESSTATE;
//     }

    // int nregions = 1;
    // if (volume_mesh) {
        
    //     int tetStartIndex;
    //     if (mesh.meshQuickRef.startIndexTetrahedral >= 0) {
    //         tetStartIndex = mesh.meshQuickRef.startIndexTetrahedral;
    //     } else {
    //         tetStartIndex = mesh.meshQuickRef.listIndexTetrahedral[0];
    //     }
    //     int numTets = mesh.meshQuickRef.numTetrahedral;

    //     /// find the number of regions
    //     nregions = countRegions(&(mesh.element[tetStartIndex]), numTets);
    // }

    // /// Copy body from tesselation and create a model to import into PUMI
    // ego model;
    // ego context;
    // status = EG_getContext(tessBody, &context);
    // if (status != EGADS_SUCCESS)
    // {
    //     printf(" PUMI AIM Warning: EG_getContext failed with status: %d!\n", status);
    //     return status;
    // }

    // ego *body = (ego *) EG_alloc(sizeof(ego));
    // if (body == NULL) {
    //     status = EGADS_MALLOC;
    //     if (status != EGADS_SUCCESS)
    //     {
    //         printf(" PUMI AIM Warning: EG_alloc failed with status: %d!\n", status);
    //         return status;
    //     }
    // }

    // *body = NULL;
    // status = EG_copyObject(tessBody, NULL, body);
    // if (status != EGADS_SUCCESS)
    // {
    //     printf(" EG_copyObject failed with status: %d!\n", status);
    //     return status;
    // }

    // status = EG_makeTopology(context, NULL, MODEL, 0, NULL, 
    //                             numBody, body, NULL, &model);
    // if (status != EGADS_SUCCESS)
    // {
    //     printf(" PUMI AIM Warning: EG_makeTopology failed with status: %d!\n", status);
    //     return status;
    // }

    // /// initialize PUMI EGADS model
    // gmi_register_egads();
    // struct gmi_model *pumiModel = gmi_egads_init_model(model, nregions);

    // int nnode = pumiModel->n[0];
    // int nedge = pumiModel->n[1];
    // int nface = pumiModel->n[2];

    // /// create empty PUMI mesh
    // pumiInstance->pumiMesh = apf::makeEmptyMdsMesh(pumiModel, 0, false);

    // // useful shorthand
    // auto *pumiMesh = pumiInstance->pumiMesh;

    // // set PUMI mesh dimension based on mesh type
    // if (mesh.meshType == Surface2DMesh ||
    //         mesh.meshType == SurfaceMesh) {
    //     apf::changeMdsDimension(pumiMesh, 2);
    // } else if (mesh.meshType == VolumeMesh) {
    //     apf::changeMdsDimension(pumiMesh, 3);
    // } else {
    //     printf(" Unknown mesh dimension, error!\n");
    //     return EGADS_WRITERR;
    // }

    // int nMeshVert = mesh.numNode;
    // apf::MeshEntity *verts[nMeshVert];

    // /// create all mesh vertices without model entities or parameters
    // for (int i = 0; i < mesh.numNode; i++) {

    //     apf::Vector3 vtxCoords(mesh.node[i].xyz);
    //     apf::Vector3 vtxParam(0.0, 0.0, 0.0);

    //     verts[i] = pumiMesh->createVertex(0, vtxCoords, vtxParam);
    //     PCU_ALWAYS_ASSERT(verts[i]);
    // }

    // int len, globalID, ntri;
    // const int *ptype=NULL, *pindex=NULL, *ptris=NULL, *ptric=NULL;
    // const double *pxyz = NULL, *puv = NULL, *pt = NULL;

    // apf::ModelEntity *gent;
    // apf::Vector3 param;

    // /// classify vertices onto model edges and build mesh edges
    // for (int i = 0; i < nedge; ++i) {
    //     apf::MeshEntity *ment;
    //     int edgeID = i + 1;
    //     status = EG_getTessEdge(tess, edgeID, &len, &pxyz, &pt);
    //     if (status != EGADS_SUCCESS) return status;
    //     for (int j = 0; j < len; ++j) {
    //         status = EG_localToGlobal(tess, -edgeID, j+1, &globalID);
    //         if (status == EGADS_DEGEN)
    //             break; // skip degenerate edges
    //         else if (status != EGADS_SUCCESS) 
    //             return status;
   
    //         // get the PUMI mesh vertex corresponding to the globalID
    //         ment = verts[globalID-1];
            
    //         // set the model entity and parametric values
    //         gent = pumiMesh->findModelEntity(1, edgeID);
    //         pumiMesh->setModelEntity(ment, gent);
    //         param = apf::Vector3(pt[j], 0.0, 0.0);
    //         pumiMesh->setParam(ment, param);
    //     }
    // }

    // /// classify vertices onto model faces and build surface triangles
    // for (int i = 0; i < nface; ++i) {
    //     int faceID = i + 1;
    //     status = EG_getTessFace(tess, faceID, &len, &pxyz, &puv, &ptype,
    //                             &pindex, &ntri, &ptris, &ptric);
    //     if (status != EGADS_SUCCESS) return status;
    //     for (int j = 0; j < len; ++j) {
    //         if (ptype[j] > 0) // vertex is classified on a model edge
    //             continue;

    //         EG_localToGlobal(tess, faceID, j+1, &globalID);
    //         // get the PUMI mesh vertex corresponding to the globalID
    //         // apf::MeshEntity *ment = apf::getMdsEntity(pumiMesh, 0, globalID);
    //         apf::MeshEntity *ment = verts[globalID-1];

    //         // set the model entity and parametric values
    //         if (ptype[j] == 0) { // entity should be classified on a vertex
    //             gent = pumiMesh->findModelEntity(0, pindex[j]);
    //             param = apf::Vector3(0.0,0.0,0.0);
    //         }
    //         else {
    //             gent = pumiMesh->findModelEntity(2, faceID);
    //             param = apf::Vector3(puv[2*j], puv[2*j+1], 0.0);
    //         }
    //         pumiMesh->setModelEntity(ment, gent);
    //         pumiMesh->setParam(ment, param);
    //     }

    //     apf::MeshEntity *a, *b, *c;
    //     int aGlobalID, bGlobalID, cGlobalID;
    //     gent = pumiMesh->findModelEntity(2, faceID);
    //     // construct triangles
    //     for (int j = 0; j < ntri; ++j) {
    //         EG_localToGlobal(tess, faceID, ptris[3*j], &aGlobalID);
    //         EG_localToGlobal(tess, faceID, ptris[3*j+1], &bGlobalID);
    //         EG_localToGlobal(tess, faceID, ptris[3*j+2], &cGlobalID);

    //         a = verts[aGlobalID-1];
    //         b = verts[bGlobalID-1];
    //         c = verts[cGlobalID-1];

    //         createEdge(pumiMesh, a, b, gent);
    //         createEdge(pumiMesh, b, c, gent);
    //         createEdge(pumiMesh, c, a, gent);

    //         /// construct mesh triangle face
    //         createFace(pumiMesh, a, b, c, gent);
    //     }

    // }
    
    // /// if volume mesh, build tets
    // if (mesh.meshType == VolumeMesh) {
    //     int elementIndex;
    //     apf::MeshEntity *a, *b, *c, *d;
    //     // apf::ModelEntity *gent;
    //     meshElementStruct ment;
    //     const int ntets = mesh.meshQuickRef.numTetrahedral;
    //     for (int i = 0; i < ntets; ++i) {
    //         if (mesh.meshQuickRef.startIndexTetrahedral >= 0) {
    //             elementIndex = mesh.meshQuickRef.startIndexTetrahedral + i;
    //         } else {
    //             elementIndex = mesh.meshQuickRef.listIndexTetrahedral[i];
    //         }
    //         ment = mesh.element[elementIndex];

    //         gent = pumiMesh->findModelEntity(3, ment.markerID);

    //         // a = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[0]);
    //         // b = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[1]);
    //         // c = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[2]);
    //         // d = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[3]);     

    //         a = verts[ment.connectivity[0]-1];
    //         b = verts[ment.connectivity[1]-1];
    //         c = verts[ment.connectivity[2]-1];
    //         d = verts[ment.connectivity[3]-1];
    //         apf::MeshEntity *tet_verts[4] = {a, b, c, d};

    //         apf::ModelEntity *g = NULL;
    //         for (int j = 0; j < 4; ++j) {
    //             g = pumiMesh->toModel(tet_verts[j]);
    //             if (g) { // if vtx model ent was already set (boundary vtx), skip
    //                 continue;
    //             }
    //             else { // if not already set, set it to be the same as the element
    //                 pumiMesh->setModelEntity(tet_verts[j], gent);
    //             }
    //         }             

    //         apf::MeshEntity *tet = apf::buildElement(pumiMesh, gent,
    //                                                  apf::Mesh::TET,
    //                                                  tet_verts);
    //         PCU_ALWAYS_ASSERT(tet);
            
    //     }
    // }

    // pumiMesh->acceptChanges();

    // /// calculate adjacency information and update PUMI gmi_edads model
    // std::vector<std::set<int>> adj_graph[6];
    // int sizes[] = {nregions, nregions, nregions, nnode, nedge, nface};

    // for (int i = 0; i < 6; ++i) {
    //     for (int j = 0; j < sizes[i]; ++j) {
    //         adj_graph[i].push_back(std::set<int>{});
    //     }
    // }

    // /// iterate over mesh verts
    // {
    //     apf::MeshIterator *it = pumiMesh->begin(0);
    //     apf::MeshEntity *vtx;
    //     while ((vtx = pumiMesh->iterate(it)))
    //     {
    //         apf::ModelEntity *vtx_me = pumiMesh->toModel(vtx);
    //         int vtx_me_dim = pumiMesh->getModelType(vtx_me);
    //         int vtx_me_tag = pumiMesh->getModelTag(vtx_me);
    //         if (vtx_me_dim == 0) { // vtx_me_dim == 0?
    //             apf::Adjacent tets;
    //             pumiMesh->getAdjacent(vtx, 3, tets);
    //             for (int i = 0; i < tets.getSize(); ++i)
    //             {
    //                 apf::ModelEntity *tet_me = pumiMesh->toModel(tets[i]);
    //                 int tet_me_tag = pumiMesh->getModelTag(tet_me);
    //                 adj_graph[0][tet_me_tag-1].insert(vtx_me_tag); // 0-3
    //                 adj_graph[3][vtx_me_tag-1].insert(tet_me_tag); // 3-0
    //             }
    //         }
    //     }
    // }

    // {
    //     apf::MeshIterator *it = pumiMesh->begin(1);
    //     apf::MeshEntity *edge;
    //     while ((edge = pumiMesh->iterate(it)))
    //     {
    //         apf::ModelEntity *edge_me = pumiMesh->toModel(edge);
    //         int edge_me_dim = pumiMesh->getModelType(edge_me);
    //         int edge_me_tag = pumiMesh->getModelTag(edge_me);
    //         if (edge_me_dim == 1) { // edge_me_dim == 1?
    //             apf::Adjacent tets;
    //             pumiMesh->getAdjacent(edge, 3, tets);
    //             for (int i = 0; i < tets.getSize(); ++i)
    //             {
    //                 apf::ModelEntity *tet_me = pumiMesh->toModel(tets[i]);
    //                 int tet_me_tag = pumiMesh->getModelTag(tet_me);
    //                 adj_graph[1][tet_me_tag-1].insert(edge_me_tag); // 1-3
    //                 adj_graph[4][edge_me_tag-1].insert(tet_me_tag); // 3-1
    //             }
    //         }
    //     }
    // }

    // {
    //     apf::MeshIterator *it = pumiMesh->begin(2);
    //     apf::MeshEntity *tri;
    //     while ((tri = pumiMesh->iterate(it)))
    //     {
    //         apf::ModelEntity *tri_me = pumiMesh->toModel(tri);
    //         int tri_me_dim = pumiMesh->getModelType(tri_me);
    //         int tri_me_tag = pumiMesh->getModelTag(tri_me);
    //         if (tri_me_dim < 3) { // edge_me_dim == 1?
    //             apf::Up tets;
    //             pumiMesh->getUp(tri, tets);
    //             for (int i = 0; i < tets.n; ++i)
    //             {
    //                 apf::ModelEntity *tet_me = pumiMesh->toModel(tets.e[i]);
    //                 int tet_me_tag = pumiMesh->getModelTag(tet_me);
    //                 adj_graph[2][tet_me_tag-1].insert(tri_me_tag); // 2-3
    //                 adj_graph[5][tri_me_tag-1].insert(tet_me_tag); // 3-2
    //             }
    //         }
    //     }
    // }

    // /// create C array adjacency graph
    // int **c_graph[6];

    // for (int i = 0; i < 6; ++i) {
    //     c_graph[i] = (int**)EG_alloc(sizeof(*c_graph[i]) * sizes[i]);
    //     if (c_graph[i] == NULL) {
    //         printf("failed to alloc memory for c_graph");
    //         /// return bad here
    //     }
    //     for (int j = 0; j < sizes[i]; ++j) {
    //         c_graph[i][j] = (int*)EG_alloc(sizeof(*c_graph[i][j])
    //                                        * (adj_graph[i][j].size()+1));
    //         if (c_graph[i][j] == NULL) {
    //             printf("failed to alloc memory for c_graph");
    //             /// return bad here
    //         }
    //         c_graph[i][j][0] = adj_graph[i][j].size();
    //         std::copy(adj_graph[i][j].begin(),
    //                   adj_graph[i][j].end(),
    //                   c_graph[i][j]+1);
    //     }
    // }

    // gmi_egads_init_adjacency(pumiModel, c_graph);
    // printf("Done generating PUMI mesh!\n");

    // printf("\nVerifying PUMI mesh.....\n");
    // pumiMesh->verify();
    // printf("PUMI mesh verified!\n");

    // int elementOrder = pumiInstance->meshInput.pumiInput.elementOrder;
    // if (elementOrder > 1) {
    //     crv::BezierCurver bc(pumiMesh, elementOrder, 2);
    //     bc.run();
    // }

    if (pumiInstance->meshInput.outputFileName != NULL) {

        char *filename = (char *) EG_alloc((strlen(pumiInstance->meshInput.outputFileName)+2)*sizeof(char));
        if (filename == NULL) return EGADS_MALLOC;

        strcpy(filename, pumiInstance->meshInput.outputFileName);

        if (strcasecmp(pumiInstance->meshInput.outputFormat, "PUMI") == 0)
        {
            /// write .smb mesh file
            std::string smb_filename(filename);
            smb_filename += ".smb";
            printf("\nWriting native PUMI mesh: %s....\n", smb_filename.c_str());
            pumi_mesh->writeNative(smb_filename.c_str());
            printf("Finished writing native PUMI mesh\n");

            /// write native tesselation
            std::string tess_filename(filename);
            tess_filename += ".eto";
            printf("\nWriting native tesselation: %s....\n", tess_filename.c_str());

            /// remove existing tesselation file if it exists
            {
                std::ifstream tess_file(tess_filename.c_str());
                if (tess_file.good())
                    std::remove(tess_filename.c_str());
            }
            status = EG_saveTess(tess, tess_filename.c_str());
            if (status != EGADS_SUCCESS)
                printf(" PUMI AIM Warning: EG_saveTess failed with status: %d!\n", status);
            printf("Finished writing native tesselation\n");

            /// create copy of body in new model for saving
            ego model;
            ego model_children[2];
            ego *body = &model_children[0];
            ego *new_tess = &model_children[1];
            size_t nbytes;
            char *stream;
            {
                ego context;
                status = EG_getContext(tessBody, &context);
                if (status != EGADS_SUCCESS)
                {
                    printf(" PUMI AIM Warning: EG_getContext failed with status: %d!\n", status);
                    return status;
                }

                // body = (ego *) EG_alloc(sizeof(ego));
                // if (body == nullptr) {
                //     status = EGADS_MALLOC;
                //     if (status != EGADS_SUCCESS)
                //     {
                //         printf(" PUMI AIM Warning: EG_alloc failed with status: %d!\n", status);
                //         return status;
                //     }
                // }

                *body = nullptr;
                status = EG_copyObject(tessBody, NULL, body);
                if (status != EGADS_SUCCESS)
                {
                    printf(" EG_copyObject failed with status: %d!\n", status);
                    return status;
                }

                *new_tess = nullptr;
                status = EG_mapTessBody(tess, *body, new_tess);
                if (status != EGADS_SUCCESS)
                {
                    printf(" EG_mapTessBody failed with status: %d!\n", status);
                    return status;
                }

                {
                    ego tess_body = nullptr;
                    int state = 0;
                    int npt = 0;
                    EG_statusTessBody(*new_tess, &tess_body, &state, &npt);
                    std::cout << "saved tess npt: " << npt << "\n";
                }

                // Create a model
                status = EG_makeTopology(context, nullptr, MODEL, 2, nullptr, 
                                         numBody, model_children, nullptr, &model);
                if (status != EGADS_SUCCESS)
                {
                    printf(" PUMI AIM Warning: EG_makeTopology failed with status: %d!\n", status);
                    return status;
                }

                status = EG_exportModel(model, &nbytes, &stream);
                if (status != EGADS_SUCCESS)
                {
                    printf(" PUMI AIM Warning: EG_exportModel failed with status: %d!\n", status);
                    return status;
                }
            }

            /// write EGADS model file
            std::string model_filename(filename);
            model_filename += ".egads";
            printf("\nWriting EGADS model file: %s....\n", model_filename.c_str());

            /// remove existing model file if it exists
            {
                std::ifstream model_file(model_filename.c_str());
                if (model_file.good())
                    std::remove(model_filename.c_str());
            }
            status = EG_saveModel(model, model_filename.c_str());
            if (status != EGADS_SUCCESS)
                printf(" PUMI AIM Warning: EG_saveModel failed with status: %d!\n", status);
            printf("Finished writing EGADS model\n");

            /// write EGADSlite model file
            std::string litemodel_filename(filename);
            litemodel_filename += ".egadslite";
            printf("\nWriting EGADSlite model file: %s....\n", litemodel_filename.c_str());

            /// remove existing lite model file if it exists
            {
                std::ifstream model_file(litemodel_filename.c_str());
                if (model_file.good())
                    std::remove(litemodel_filename.c_str());
            }
            {                
                FILE *fp = fopen(litemodel_filename.c_str(), "wb");
                fwrite(stream, sizeof(char), nbytes, fp);
                fclose(fp);
            }
            EG_free(stream);

            // delete the model
            if (model != NULL) {
                EG_deleteObject(model);
            }
            else {
                if (body != NULL) {
                    if (*body != NULL) {
                        (void) EG_deleteObject(*body);
                    }
                }
            }
            // EG_free(body);
            printf("Finished writing EGADSlite model\n");

            /// write adjacency table (lite)
            {
               std::string adj_filename(filename);
               adj_filename += ".egadslite.sup";
               
               printf("\nWriting supplementary EGADS model file: %s....\n", adj_filename.c_str());

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
            }

            /// write adjacency table
            {
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
            }
            printf("Finished writing supplementary EGADS model\n\n");
        }
        else if (strcasecmp(pumiInstance->meshInput.outputFormat, "VTK") == 0)
        {
            printf("\nWriting VTK file: %s....\n", filename);
            // if (elementOrder > 1) {
            //     // if (mesh.meshType == VolumeMesh)
            //         crv::writeCurvedVtuFiles(pumi_mesh, apf::Mesh::TET, 10, filename);
            //     else
            //         crv::writeCurvedVtuFiles(pumi_mesh, apf::Mesh::TRIANGLE, 10, filename);
            //     crv::writeCurvedWireFrame(pumi_mesh, 10, filename);
            // } else {
                apf::writeVtkFiles(filename, pumi_mesh);
            // }
            printf("Finished writing VTK file\n\n");
        }
        if (filename != NULL) EG_free(filename);
    }

    return status;
}

/* the execution code from above should be moved here */
extern "C" int
aimExecute(/*@unused@*/ void *instStore, /*@unused@*/ void *aimStruc, int *state)
{
  *state = 0;
  return CAPS_SUCCESS;
}

/* no longer optional and needed for restart */
extern "C" int
aimPostAnalysis(/*@unused@*/ void *instStore, /*@unused@*/ void *aimStruc,
                /*@unused@*/ int restart, /*@unused@*/ capsValue *inputs)
{
  return CAPS_SUCCESS;
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

#ifdef DEBUG
    printf(" pumiAIM/aimCalcOutput index = %d!\n", index);
#endif
    auto *pumiInstance = static_cast<aimStorage *>(instStore);

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

    gmi_egads_stop();

    int mpi_flag;
    MPI_Finalized(&mpi_flag);
    if (!mpi_flag) {
        PCU_Comm_Free();
        MPI_Finalize();
    }
}
