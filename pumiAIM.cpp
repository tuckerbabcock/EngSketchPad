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

#include "apf.h"
#include "apfMesh.h"
#include "apfMesh2.h"
#include "apfMDS.h"
#include "crv.h"
#include "gmi.h"
#include "gmi_egads.h"
#include "pcu_util.h"

#include "capsTypes.h"
#include "aimUtil.h"
#include "meshTypes.h"
#include "miscTypes.h"
#include "meshUtils.h"  // Collection of helper functions for meshing
#include "miscUtils.h"  // Collection of helper functions for miscellaneous use

#define NUMINPUT   3  // number of mesh inputs
#define NUMOUT     1  // number of outputs // placeholder for now

//#define DEBUG

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

static aimStorage *pumiInstance = NULL;
static int         numInstance  = 0;

static int destroy_aimStorage(int iIndex) {

    int i; // Indexing

    int status; // Function return status

    // Destroy meshInput
    status = destroy_meshInputStruct(&pumiInstance[iIndex].meshInput);
    if (status != CAPS_SUCCESS) printf("Status = %d, pumiAIM instance %d, meshInput cleanup!!!\n", status, iIndex);

    // Destroy mesh allocated arrays
    if (pumiInstance[iIndex].meshInherited == (int) false) { // Did the parent aim already destroy the meshes?
        if (pumiInstance[iIndex].mesh != NULL) {
            for (i = 0; i < pumiInstance[iIndex].numMesh; i++) {
                status = destroy_meshStruct(&pumiInstance[iIndex].mesh[i]);
                if (status != CAPS_SUCCESS) printf("Error during destroy_meshStruct, status = %d, pumiAIM instance %d\n", status, iIndex);
            }
            EG_free(pumiInstance[iIndex].mesh);
        }
    }

    // Destroy PUMI mesh
    if (pumiInstance[iIndex].pumiMesh != NULL) {
        for (i = 0; i < pumiInstance[iIndex].numPUMIMesh; i++) {
            pumiInstance[iIndex].pumiMesh->destroyNative();
            apf::destroyMesh(pumiInstance[iIndex].pumiMesh);
        }
    }

    pumiInstance[iIndex].numMesh = 0;
    pumiInstance[iIndex].meshInherited = (int) false;
    pumiInstance[iIndex].mesh = NULL;

    // Destroy attribute to index map
    status = destroy_mapAttrToIndexStruct(&pumiInstance[iIndex].attrMap);
    if (status != CAPS_SUCCESS) printf("Status = %d, pumi4AIM instance %d, attributeMap cleanup!!!\n", status, iIndex);

    return CAPS_SUCCESS;
}

/* ********************** Exposed AIM Functions ***************************** */

extern "C" int
aimInitialize(int ngIn, capsValue *gIn, int *qeFlag, const char *unitSys,
              int *nIn, int *nOut, int *nFields, char ***fnames, int **ranks)
{
    int status; // Function status return
    int flag;   // query/execute flag

    aimStorage *tmp;

#ifdef DEBUG
    printf("\n pumiAIM/aimInitialize   ngIn = %d!\n", ngIn);
#endif
    flag    = *qeFlag;
    // Set that the AIM executes itself
    *qeFlag = 1;

    /* specify the number of analysis input and out "parameters" */
    *nIn     = NUMINPUT;
    *nOut    = NUMOUT;
    if (flag == 1) return CAPS_SUCCESS;

    /* specify the field variables this analysis can generate */
    *nFields = 0;
    *ranks   = NULL;
    *fnames  = NULL;

    // Allocate pumiInstance
    if (pumiInstance == NULL) {
        pumiInstance = (aimStorage *) EG_alloc(sizeof(aimStorage));
        if (pumiInstance == NULL) return EGADS_MALLOC;
    } else {
        tmp = (aimStorage *) EG_reall(pumiInstance, (numInstance+1)*sizeof(aimStorage));
        if (tmp == NULL) return EGADS_MALLOC;
        pumiInstance = tmp;
    }

    // Set initial values for pumiInstance //

    // Container for mesh
    pumiInstance[numInstance].meshInherited = (int) false;

    pumiInstance[numInstance].numMesh = 0;
    pumiInstance[numInstance].mesh = NULL;

    pumiInstance[numInstance].numPUMIMesh = 0;
    pumiInstance[numInstance].pumiMesh = NULL;

    // Container for attribute to index map
    status = initiate_mapAttrToIndexStruct(&pumiInstance[numInstance].attrMap);
    if (status != CAPS_SUCCESS) return status;

    // Container for mesh input
    status = initiate_meshInputStruct(&pumiInstance[numInstance].meshInput);

    // Increment number of instances
    numInstance += 1;

    return (numInstance-1);
}


extern "C" int
aimInputs(int iIndex, void *aimInfo, int index, char **ainame, capsValue *defval)
{
    /*! \page aimInputsPUMI AIM Inputs
     * The following list outlines the PUMI options along with their default value available
     * through the AIM interface.
     */

#ifdef DEBUG
    printf(" pumiAIM/aimInputs instance = %d  index = %d!\n", iIndex, index);
#endif

    // Meshing Inputs
    if (index == 1) {
        *ainame              = EG_strdup("Proj_Name"); // If NULL a volume grid won't be written by the AIM
        defval->type         = String;
        defval->nullVal      = IsNull;
        defval->vals.string  = NULL;
        //defval->vals.string  = EG_strdup("CAPS");
        defval->lfixed       = Change;

        /*! \page aimInputsPUMI
         * - <B> Proj_Name = NULL</B> <br>
         * This corresponds to the output name of the mesh. If left NULL, the mesh is not written to a file.
         */

    } else if (index == 2) {
        *ainame               = EG_strdup("Mesh_Format");
        defval->type          = String;
        defval->vals.string   = EG_strdup("PUMI"); // PUMI, VTK
        defval->lfixed        = Change;

        /*! \page aimInputsPUMI
         * - <B> Mesh_Format = "PUMI"</B> <br>
         * Mesh output format. Available format names include: "PUMI", "VTK".
         */

    } else if (index == 3) {
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

    return CAPS_SUCCESS;
}

extern "C" int
aimData(int iIndex, const char *name, enum capsvType *vtype, int *rank, int *nrow,
        int *ncol, void **data, char **units)
{

    /*! \page sharableDataPUMI AIM Shareable Data
     * The PUMI AIM has the following shareable data types/values with its children AIMs if they are so inclined.
     * - <B> Mesh</B> <br>
     * The mesh in meshStruct (see meshTypes.h) format.
     * - <B> PUMI_Mesh</B> <br>
     * The returned mesh after curving is complete in apf::Mesh2 (see <a href="https://www.scorec.rpi.edu/pumi/doxygen/apfMDS_8h.html">apfMDS.h</a>) format.
     * - <B> Attribute_Map</B> <br>
     * An index mapping between capsGroups found on the geometry in mapAttrToIndexStruct (see miscTypes.h) format.
     */

#ifdef DEBUG
    printf(" pumiAIM/aimData instance = %d  name = %s!\n", iIndex, name);
#endif

    // The mesh input
    if (strcasecmp(name, "Mesh") == 0){
        *vtype = Value;
        *rank  = *ncol = 1;
        *nrow = pumiInstance[iIndex].numMesh;
        if (pumiInstance[iIndex].numMesh == 1) {
            *data  = &pumiInstance[iIndex].mesh[0];
        } else {
            *data  = pumiInstance[iIndex].mesh;
        }

        *units = NULL;

        return CAPS_SUCCESS;
    }

    // The returned mesh from PUMI
    if (strcasecmp(name, "PUMI_Mesh") == 0){
        *vtype = Value;
        *rank  = *ncol = 1;
        *nrow = pumiInstance[iIndex].numPUMIMesh;
        if (pumiInstance[iIndex].numPUMIMesh == 1) {
            *data  = &(pumiInstance[iIndex].pumiMesh[0]);
        } else {
            *data  = pumiInstance[iIndex].pumiMesh;
        }

        *units = NULL;

        return CAPS_SUCCESS;
    }

    // Share the attribute map
    if (strcasecmp(name, "Attribute_Map") == 0){
        *vtype = Value;
        *rank  = *nrow = *ncol = 1;
        *data  = &pumiInstance[iIndex].attrMap;
        *units = NULL;

        return CAPS_SUCCESS;
    }

    return CAPS_NOTFOUND;
}

extern "C" int
aimPreAnalysis(int iIndex, void *aimInfo, const char *analysisPath, capsValue *aimInputs,
        capsErrs **errs)
{
    int status;
    int nattr=0; // unused so far
    egAttrs *attrs; // unused so far

    if (0 != pumiInstance[iIndex].numMesh) {
        printf(" PUMI AIM currently only supports one mesh!!\n");
        /// TODO: better return?
        return EGADS_INDEXERR;
    }

    // Mesh Format
    pumiInstance[iIndex].meshInput.outputFormat = EG_strdup(aimInputs[aim_getIndex(aimInfo, "Mesh_Format", ANALYSISIN)-1].vals.string);
    if (pumiInstance[iIndex].meshInput.outputFormat == NULL) return EGADS_MALLOC;

    // Project Name
    if (aimInputs[aim_getIndex(aimInfo, "Proj_Name", ANALYSISIN)-1].nullVal != IsNull) {

        pumiInstance[iIndex].meshInput.outputFileName = EG_strdup(aimInputs[aim_getIndex(aimInfo, "Proj_Name", ANALYSISIN)-1].vals.string);
        if (pumiInstance[iIndex].meshInput.outputFileName == NULL) return EGADS_MALLOC;
    }

    // Output directory
    pumiInstance[iIndex].meshInput.outputDirectory = EG_strdup(analysisPath);
    if (pumiInstance[iIndex].meshInput.outputDirectory == NULL) return EGADS_MALLOC;

    // Set PUMI specific mesh inputs -1 because aim_getIndex is 1 bias
    if (aimInputs[aim_getIndex(aimInfo, "Mesh_Order", ANALYSISIN)-1].nullVal != IsNull) {
        pumiInstance[iIndex].meshInput.pumiInput.elementOrder = aimInputs[aim_getIndex(aimInfo, "Mesh_Order", ANALYSISIN)-1].vals.integer;
    }

    // useful shorthands
    meshStruct *mesh = pumiInstance[iIndex].mesh;
    apf::Mesh2 *pumiMesh = pumiInstance[iIndex].pumiMesh;
    
    ego tess = pumiInstance[iIndex].mesh[0].bodyTessMap.egadsTess;
    ego body;
    int state, npts;
    // get the body from tessellation
    status = EG_statusTessBody(tess, &body, &state, &npts);
    if (status != EGADS_SUCCESS) {
#ifdef DEBUG
        printf(" EGADS Warning: Tessellation is Open (EG_saveTess)!\n");
#endif
        return EGADS_TESSTATE;
    }

    /// initialize PUMI EGADS model
    struct gmi_model *pumiModel = gmi_egads_init(body);

    int nnode = pumiModel->n[0];
    int nedge = pumiModel->n[1];
    int nface = pumiModel->n[2];

    /// create empty PUMI mesh
    pumiMesh = apf::makeEmptyMdsMesh(pumiModel, 0, false);

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

    /// TODO: set model ent if n > npts
    /// create all mesh vertices without model entities or parameters
    double scaleFactor = 1.0; // for now
    for (int i = 0; i < mesh->numNode; i++) {

        apf::Vector3 vtxCoords(mesh->node[i].xyz);
        apf::Vector3 vtxParam(0.0, 0.0, 0.0);

        apf::MeshEntity* vtx = pumiMesh->createVertex(0, vtxCoords, vtxParam);
        PCU_ALWAYS_ASSERT(vtx);
    }

    apf::reorderMdsMesh(pumiMesh);

    int len, globalID, ntri;
    const int *ptype=NULL, *pindex=NULL, *ptris=NULL, *ptric=NULL;
    const double *pxyz = NULL, *puv = NULL, *pt = NULL;

    apf::MeshEntity *ment, *oldMent;
    apf::ModelEntity *gent;
    apf::Vector3 param;

    /// classify vertices onto model edges and build mesh edges
    for (int i = 0; i < nedge; ++i) {
        int edgeID = i + 1;
        status = EG_getTessEdge(tess, edgeID, &len, &pxyz, &pt);
        if (status != EGADS_SUCCESS) return status;
        for (int j = 0; j < len; ++j) {
            EG_localToGlobal(tess, -edgeID, j, &globalID);
            // get the PUMI mesh vertex corresponding to the globalID
            ment = apf::getMdsEntity(pumiMesh, 0, globalID);
            
            // set the model entity and parametric values
            gent = pumiMesh->findModelEntity(1, edgeID);
            pumiMesh->setModelEntity(ment, gent);
            param = apf::Vector3(pt[j], 0.0, 0.0);
            pumiMesh->setParam(ment, param);

            // construct mesh edge on same model edge as vertices
            if (j > 0) {    
                apf::MeshEntity *verts[2] = {oldMent, ment};
                apf::MeshEntity *edge = apf::buildElement(pumiMesh, gent, 
                                                          1, // apf::Mesh::EDGE,
                                                          verts);
                PCU_ALWAYS_ASSERT(edge);
            }
            oldMent = ment;
        }
    }

    /// classify vertices onto model faces and build surface triangles
    for (int i = 0; i < nface; ++i) {
        int faceID = i + 1;
        status = EG_getTessFace(tess, faceID, &len, &pxyz, &puv, &ptype,
                                &pindex, &ntri, &ptris, &ptric);
        if (status != EGADS_SUCCESS) return status;
        for (int j = 0; j < len; ++j) {
            if (ptype[j] > 0) // vertex is classified on a model edge
                continue;

            EG_localToGlobal(tess, faceID, j, &globalID);
            // get the PUMI mesh vertex corresponding to the globalID
            apf::MeshEntity *ment = apf::getMdsEntity(pumiMesh, 0, globalID);

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
        for (int j = 0; j < len; j += 3) {
            EG_localToGlobal(tess, faceID, ptris[j], &aGlobalID);
            EG_localToGlobal(tess, faceID, ptris[j+1], &bGlobalID);
            EG_localToGlobal(tess, faceID, ptris[j+2], &cGlobalID);
            a = apf::getMdsEntity(pumiMesh, 0, aGlobalID);
            b = apf::getMdsEntity(pumiMesh, 0, bGlobalID);
            c = apf::getMdsEntity(pumiMesh, 0, cGlobalID);
            apf::MeshEntity *verts[3] = {a, b, c};
            apf::MeshEntity *tri = apf::buildElement(pumiMesh, gent,
                                                     apf::Mesh::TRIANGLE,
                                                     verts);
            PCU_ALWAYS_ASSERT(tri);
        }

    }

    // /Users/tuckerbabcock/Developer/playground/EngSketchPad/src/CAPS/aim/pumi/pumiAIM.cpp

    /// TODO: figure out how to build adjacency based on mesh and update the gmi_model
    
    /// if volume mesh, build tets
    if (mesh->meshType == VolumeMesh) {
        int elementIndex;
        apf::MeshEntity *a, *b, *c, *d;
        apf::ModelEntity *gent;
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

            a = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[0]);
            b = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[1]);
            c = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[2]);
            d = apf::getMdsEntity(pumiMesh, 0, ment.connectivity[3]);                        
            apf::MeshEntity *verts[4] = {a, b, c, d};
            apf::MeshEntity *tet = apf::buildElement(pumiMesh, gent,
                                                     apf::Mesh::TET,
                                                     verts);
            PCU_ALWAYS_ASSERT(tet);
            
        }
    }

    pumiMesh->verify();

    int elementOrder = pumiInstance[iIndex].meshInput.pumiInput.elementOrder;
    if (elementOrder > 1) {
        crv::BezierCurver bc(pumiMesh, elementOrder, 2);
        bc.run();
    }

    if (pumiInstance[iIndex].meshInput.outputFileName != NULL) {

        char *filename = (char *) EG_alloc((strlen(pumiInstance[iIndex].meshInput.outputFileName) +
                                            strlen(pumiInstance[iIndex].meshInput.outputDirectory)+2)*sizeof(char));
        
        strcpy(filename, pumiInstance[iIndex].meshInput.outputDirectory);

        // I'm pretty sure PUMI doesn't work on Windows
        #ifdef WIN32
            strcat(filename, "\\");
        #else
            strcat(filename, "/");
        #endif
        strcat(filename, pumiInstance[iIndex].meshInput.outputFileName);

        if (strcasecmp(pumiInstance[iIndex].meshInput.outputFormat, "PUMI") == 0) {
            pumiMesh->writeNative(filename);
        } else if (strcasecmp(pumiInstance[iIndex].meshInput.outputFormat, "VTK") == 0) {
            if (elementOrder > 1) {
                if (mesh->meshType == VolumeMesh)
                    crv::writeCurvedVtuFiles(pumiMesh, apf::Mesh::TET, 10, "filename");
                else
                    crv::writeCurvedVtuFiles(pumiMesh, apf::Mesh::TRIANGLE, 10, "filename");
                crv::writeCurvedWireFrame(pumiMesh, 10, "curvedvtk");
            } else {
                apf::writeVtkFiles("filename", pumiMesh);
            }
        }
        if (filename != NULL) EG_free(filename);
    }


    return status;
}

extern "C" int
aimOutputs(/*@unused@*/ int iIndex, void *aimInfo,  int index, char **aoname, capsValue *form)
{
    /*! \page aimOutputsPUMI AIM Outputs
     * The following list outlines the PUMI AIM outputs available through the AIM interface.
     *
     * - <B>Done</B> = True if a PUMI mesh(es) was created, False if not.
     */

#ifdef DEBUG
    printf(" pumiAIM/aimOutputs instance = %d  index = %d!\n", iIndex, index);
#endif

    *aoname = EG_strdup("Done");
    form->type = Boolean;
    form->vals.integer = (int) false;

    return CAPS_SUCCESS;
}

extern "C" void
aimCleanup()
{
    int iIndex; //Indexing

    int status; // Function return status

#ifdef DEBUG
    printf(" pumiAIM/aimCleanup!\n");
#endif

    // Clean up pumiInstance data
    for ( iIndex = 0; iIndex < numInstance; iIndex++) {
        printf(" Cleaning up pumiInstance - %d\n", iIndex);

        status = destroy_aimStorage(iIndex);
        if (status != CAPS_SUCCESS) printf("Status = %d, pumiAIM instance %d, aimStorage cleanup!!!\n", status, iIndex);
    }

    if (pumiInstance != NULL) EG_free(pumiInstance);
    pumiInstance = NULL;
    numInstance = 0;
}

/************************************************************************/