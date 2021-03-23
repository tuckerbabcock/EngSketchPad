/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             AFLR2 AIM
 *
 *      Modified from code written by Dr. Ryan Durscher AFRL/RQVC
 *
 */

/*!\mainpage Introduction
 *
 * \section overviewAFLR2 AFLR2 AIM Overview
 * A module in the Computational Aircraft Prototype Syntheses (CAPS) has been developed to interact with the
 * unstructured, surface grid generator AFLR2 \cite Marcum1995 \cite Marcum1998.
 *
 * The AFLR2 AIM provides the CAPS users with the ability to generate "unstructured, 2D grids" using an
 * "Advancing-Front/Local-Reconnection (AFLR) procedure." Both triangular and quadrilateral elements may be generated.
 *
 * An outline of the AIM's inputs, outputs and attributes are provided in \ref aimInputsAFLR2 and
 * \ref aimOutputsAFLR2 and \ref attributeAFLR2, respectively.
 *
 * The complete AFLR documentation is available at the <a href="http://www.simcenter.msstate.edu/software/downloads/doc/system/index.html">SimCenter</a>.
 *
 */

/*! \page attributeAFLR2 AIM attributes
 * The following list of attributes are required for the AFLR2 AIM inside the geometry input.
 *
 * - <b> capsGroup</b> This is a name assigned to any geometric entity to denote a "boundary" for further analysis. Recall that a
 *  string in ESP starts with a $.  For example, attribute <c>capsGroup $Wing</c>.
 *
 * - <b> capsMesh</b> This is a name assigned to any geometric entity in order to control meshing related parameters. Recall that a
 *  string in ESP starts with a $.  For example, attribute <c>capsMesh $Wing</c>.
 *
 */

#include <string.h>
#include <math.h>
#include "capsTypes.h"
#include "aimUtil.h"

#include "meshUtils.h"       // Collection of helper functions for meshing
#include "miscUtils.h"
#include "deprecateUtils.h"
#include "aflr2_Interface.h" // Bring in AFLR2 'interface' functions

#ifdef WIN32
#define strcasecmp stricmp
#endif

#define MXCHAR  255

//#define DEBUG


enum aimInputs
{
  Proj_Name = 1,               /* index is 1-based */
  Tess_Params,
  Mesh_Quiet_Flag,
  Mesh_Format,
  Mesh_ASCII_Flag,
  Mesh_Gen_Input_String,
  Edge_Point_Min,
  Edge_Point_Max,
  Mesh_Sizing,
  NUMINPUT = Mesh_Sizing       /* Total number of inputs */
};

enum aimOutputs
{
  Done = 1,                    /* index is 1-based */
  Surface_Mesh,
  NUMOUT = Surface_Mesh        /* Total number of outputs */
};


typedef struct {

    // Container for surface mesh
    int numSurface;
    meshStruct *surfaceMesh;

    // Container for mesh input
    meshInputStruct meshInput;

    // Attribute to index map for capsGroup
    mapAttrToIndexStruct groupMap;

    // Attribute to index map for capsMesh
    mapAttrToIndexStruct meshMap;

} aimStorage;


static int destroy_aimStorage(aimStorage *aflr2Instance)
{

    int i; // Indexing

    int status; // Function return status

    // Destroy meshInput
    status = destroy_meshInputStruct(&aflr2Instance->meshInput);
    if (status != CAPS_SUCCESS)
      printf("Status = %d, aflr2AIM meshInput cleanup!!!\n", status);

    // Destroy surface mesh allocated arrays
    for (i = 0; i < aflr2Instance->numSurface; i++) {

        status = destroy_meshStruct(&aflr2Instance->surfaceMesh[i]);
        if (status != CAPS_SUCCESS)
          printf("Status = %d, aflr2AIM surfaceMesh cleanup!!!\n", status);

    }
    aflr2Instance->numSurface = 0;

    if (aflr2Instance->surfaceMesh != NULL) EG_free(aflr2Instance->surfaceMesh);
    aflr2Instance->surfaceMesh = NULL;

    // Destroy attribute to index map
    status = destroy_mapAttrToIndexStruct(&aflr2Instance->groupMap);
    if (status != CAPS_SUCCESS)
      printf("Status = %d, aflr2AIM attributeMap cleanup!!!\n", status);

    status = destroy_mapAttrToIndexStruct(&aflr2Instance->meshMap);
    if (status != CAPS_SUCCESS)
      printf("Status = %d, aflr2AIM attributeMap cleanup!!!\n", status);


    return CAPS_SUCCESS;
}


#ifdef WIN32
    #if  _MSC_VER >= 1900
    // FILE _iob[] = {*stdin, *stdout, *stderr};
    FILE _iob[3];

    extern FILE * __cdecl __iob_func(void)
    {
        return _iob;
    }
    #endif
#endif


/* ********************** Exposed AIM Functions ***************************** */

int aimInitialize(int inst, /*@null@*/ /*@unused@*/ const char *unitSys,
                  void **instStore, /*@unused@*/ int *maj, /*@unused@*/ int *min,
                  int *nIn, int *nOut, int *nFields, char ***fnames, int **ranks)
{
    aimStorage *aflr2Instance;

    #ifdef WIN32
        #if  _MSC_VER >= 1900
            _iob[0] = *stdin;
            _iob[1] = *stdout;
            _iob[2] = *stderr;
        #endif
    #endif

    int status; // Function return

#ifdef DEBUG
    printf("\n aflr2AIM/aimInitialize   inst = %d!\n", inst);
#endif

    /* specify the number of analysis input and out "parameters" */
    *nIn     = NUMINPUT;
    *nOut    = NUMOUT;
    if (inst == -1) return CAPS_SUCCESS;

    /* specify the field variables this analysis can generate */
    *nFields = 0;
    *ranks   = NULL;
    *fnames  = NULL;

    // Allocate aflrInstance
    aflr2Instance = (aimStorage *) EG_alloc(sizeof(aimStorage));
    if (aflr2Instance == NULL) return EGADS_MALLOC;

    // Set initial values for aflrInstance //

    // Container for surface meshes
    aflr2Instance->surfaceMesh = NULL;
    aflr2Instance->numSurface  = 0;

    // Container for attribute to index map
    status = initiate_mapAttrToIndexStruct(&aflr2Instance->meshMap);
    if (status != CAPS_SUCCESS) {
        EG_free(aflr2Instance);
        return status;
    }

    // Container for attribute to index map
    status = initiate_mapAttrToIndexStruct(&aflr2Instance->groupMap);
    if (status != CAPS_SUCCESS) {
        destroy_mapAttrToIndexStruct(&aflr2Instance->meshMap);
        EG_free(aflr2Instance);
        return status;
    }

    // Container for mesh input
    status = initiate_meshInputStruct(&aflr2Instance->meshInput);
    if (status != CAPS_SUCCESS) {
        destroy_mapAttrToIndexStruct(&aflr2Instance->groupMap);
        destroy_mapAttrToIndexStruct(&aflr2Instance->meshMap);
        EG_free(aflr2Instance);
        return status;
    }
  
    *instStore = aflr2Instance;

    return CAPS_SUCCESS;
}


int aimInputs(/*@unused@*/ void *instStore, /*@unused@*/ void *aimInfo,
              int index, char **ainame, capsValue *defval)
{

    /*! \page aimInputsAFLR2 AIM Inputs
     * The following list outlines the AFLR2 meshing options along with their default value available
     * through the AIM interface.
     */

#ifdef DEBUG
    printf(" aflr2AIM/aimInputs instance = %d  index = %d!\n", index);
#endif

    // Meshing Inputs
    if (index == Proj_Name) {
        *ainame              = EG_strdup("Proj_Name"); // If NULL a volume grid won't be written by the AIM
        defval->type         = String;
        defval->nullVal      = IsNull;
        defval->vals.string  = NULL;
        //defval->vals.string  = EG_strdup("CAPS");
        defval->lfixed       = Change;

        /*! \page aimInputsAFLR2
         * - <B> Proj_Name = NULL</B> <br>
         * This corresponds to the output name of the mesh. If left NULL, the mesh is not written to a file.
         */

    } else if (index == Tess_Params) {
        *ainame               = EG_strdup("Tess_Params");
        defval->type          = Double;
        defval->dim           = Vector;
        defval->length        = 3;
        defval->nrow          = 3;
        defval->ncol          = 1;
        defval->units         = NULL;
        defval->lfixed        = Fixed;
        defval->vals.reals    = (double *) EG_alloc(defval->length*sizeof(double));
        if (defval->vals.reals != NULL) {
            defval->vals.reals[0] = 0.025;
            defval->vals.reals[1] = 0.001;
            defval->vals.reals[2] = 15.00;
        } else return EGADS_MALLOC;

        /*! \page aimInputsAFLR2
         * - <B> Tess_Params = [0.025, 0.001, 15.0]</B> <br>
         * Body tessellation parameters. Tess_Params[0] and Tess_Params[1] get scaled by the bounding
         * box of the body. (From the EGADS manual) A set of 3 parameters that drive the EDGE discretization
         * and the FACE triangulation. The first is the maximum length of an EDGE segment or triangle side
         * (in physical space). A zero is flag that allows for any length. The second is a curvature-based
         * value that looks locally at the deviation between the centroid of the discrete object and the
         * underlying geometry. Any deviation larger than the input value will cause the tessellation to
         * be enhanced in those regions. The third is the maximum interior dihedral angle (in degrees)
         * between triangle facets (or Edge segment tangents for a WIREBODY tessellation), note that a
         * zero ignores this phase.
         */

    } else if (index == Mesh_Quiet_Flag) {
        *ainame               = EG_strdup("Mesh_Quiet_Flag");
        defval->type          = Boolean;
        defval->vals.integer  = false;

        /*! \page aimInputsAFLR2
         * - <B> Mesh_Quiet_Flag = False</B> <br>
         * Complete suppression of mesh generator (not including errors)
         */

    } else if (index == Mesh_Format) {
        *ainame               = EG_strdup("Mesh_Format");
        defval->type          = String;
        defval->vals.string   = EG_strdup("AFLR3"); // TECPLOT, VTK, AFLR3, STL, FAST
        defval->lfixed        = Change;

        /*! \page aimInputsAFLR2
         * - <B> Mesh_Format = "AFLR3"</B> <br>
         * Mesh output format. Available format names include: "AFLR3", "VTK", "TECPLOT", "STL" (quadrilaterals will be
         * split into triangles), "FAST".
         */

    } else if (index == Mesh_ASCII_Flag) {
        *ainame               = EG_strdup("Mesh_ASCII_Flag");
        defval->type          = Boolean;
        defval->vals.integer  = true;

        /*! \page aimInputsAFLR2
         * - <B> Mesh_ASCII_Flag = True</B> <br>
         * Output mesh in ASCII format, otherwise write a binary file if applicable.
         */

    } else if (index == Mesh_Gen_Input_String) {
        *ainame               = EG_strdup("Mesh_Gen_Input_String");
        defval->type          = String;
        defval->nullVal       = IsNull;
        defval->vals.string   = NULL;

        /*! \page aimInputsAFLR2
         * - <B> Mesh_Gen_Input_String = NULL</B> <br>
         * Meshing program command line string (as if called in bash mode). Use this to specify more
         * complicated options/use features of the mesher not currently exposed through other AIM input
         * variables. Note that this is the exact string that will be provided to the volume mesher; no
         * modifications will be made. If left NULL an input string will be created based on default values
         * of the relevant AIM input variables.
         */

    } else if (index == Edge_Point_Min) {
        *ainame               = EG_strdup("Edge_Point_Min");
        defval->type          = Integer;
        defval->vals.integer  = 0;
        defval->lfixed        = Fixed;
        defval->nrow          = 1;
        defval->ncol          = 1;
        defval->nullVal       = IsNull;

        /*! \page aimInputsAFLR2
         * - <B> Edge_Point_Min = NULL</B> <br>
         * Minimum number of points on an edge including end points to use when creating a surface mesh (min 2).
         */

    } else if (index == Edge_Point_Max) {
        *ainame               = EG_strdup("Edge_Point_Max");
        defval->type          = Integer;
        defval->vals.integer  = 0;
        defval->length        = 1;
        defval->lfixed        = Fixed;
        defval->nrow          = 1;
        defval->ncol          = 1;
        defval->nullVal       = IsNull;

        /*! \page aimInputsAFLR2
         * - <B> Edge_Point_Max = NULL</B> <br>
         * Maximum number of points on an edge including end points to use when creating a surface mesh (min 2).
         */

    } else if (index == Mesh_Sizing) {
        *ainame              = EG_strdup("Mesh_Sizing");
        defval->type         = Tuple;
        defval->nullVal      = IsNull;
        //defval->units        = NULL;
        defval->dim          = Vector;
        defval->lfixed       = Change;
        defval->vals.tuple   = NULL;

        /*! \page aimInputsAFLR2
         * - <B>Mesh_Sizing = NULL </B> <br>
         * See \ref meshSizingProp for additional details.
         */
    }

    return CAPS_SUCCESS;
}


int aimPreAnalysis(void *instStore, void *aimInfo, capsValue *aimInputs)
{
    int status; // Status return

    int i, bodyIndex; // Indexing

    int Message_Flag;
    aimStorage *aflr2Instance;

    // Body parameters
    const char *intents;
    int numBody = 0; // Number of bodies
    ego *bodies = NULL; // EGADS body objects
    double refLen = -1.0;

    // Global settings
    int minEdgePoint = -1, maxEdgePoint = -1;

    // Mesh attribute parameters
    int numMeshProp = 0;
    meshSizingStruct *meshProp = NULL;

    // File output
    char *filename = NULL;
    char bodyNumber[11];

    // Get AIM bodies
    status = aim_getBodies(aimInfo, &intents, &numBody, &bodies);
    if (status != CAPS_SUCCESS) return status;

#ifdef DEBUG
    printf(" aflr2AIM/aimPreAnalysis numBody = %d!\n", numBody);
#endif

    if (numBody <= 0 || bodies == NULL) {
#ifdef DEBUG
        printf(" aflr2AIM/aimPreAnalysis No Bodies!\n");
#endif
        return CAPS_SOURCEERR;
    }
    if (aimInputs == NULL) return CAPS_NULLOBJ;
  
    aflr2Instance = (aimStorage *) instStore;

    // Cleanup previous aimStorage for the instance in case this is the
    //         second time through preAnalysis for the same instance
    status = destroy_aimStorage(aflr2Instance);
    if (status != CAPS_SUCCESS) {
        printf("Status = %d, aflr2AIM aimStorage cleanup!!!\n", status);
        return status;
    }

    // Get capsGroup name and index mapping to make sure all faces have a capsGroup value
    status = create_CAPSMeshAttrToIndexMap(numBody,
                                           bodies,
                                           3, // Node level
                                           &aflr2Instance->meshMap);
    if (status != CAPS_SUCCESS) return status;

    status = create_CAPSGroupAttrToIndexMap(numBody,
                                            bodies,
                                            3, // Node level
                                            &aflr2Instance->groupMap);
    if (status != CAPS_SUCCESS) return status;

    // Allocate surfaceMesh from number of bodies
    aflr2Instance->numSurface = numBody;
    aflr2Instance->surfaceMesh = (meshStruct *) EG_alloc(aflr2Instance->numSurface*sizeof(meshStruct));
    if (aflr2Instance->surfaceMesh == NULL) {
        aflr2Instance->numSurface = 0;
        return EGADS_MALLOC;
    }

    // Initiate surface meshes
    for (bodyIndex = 0; bodyIndex < numBody; bodyIndex++){
        status = initiate_meshStruct(&aflr2Instance->surfaceMesh[bodyIndex]);
        if (status != CAPS_SUCCESS) return status;
    }

    // Setup meshing input structure

    // Get Tessellation parameters -Tess_Params
    aflr2Instance->meshInput.paramTess[0] = aimInputs[Tess_Params-1].vals.reals[0]; // Gets multiplied by bounding box size
    aflr2Instance->meshInput.paramTess[1] = aimInputs[Tess_Params-1].vals.reals[1]; // Gets multiplied by bounding box size
    aflr2Instance->meshInput.paramTess[2] = aimInputs[Tess_Params-1].vals.reals[2];

    aflr2Instance->meshInput.quiet            = aimInputs[Mesh_Quiet_Flag-1].vals.integer;
    aflr2Instance->meshInput.outputASCIIFlag  = aimInputs[Mesh_ASCII_Flag-1].vals.integer;

    // Mesh Format
    aflr2Instance->meshInput.outputFormat = EG_strdup(aimInputs[Mesh_Format-1].vals.string);
    if (aflr2Instance->meshInput.outputFormat == NULL) return EGADS_MALLOC;

    // Project Name
    if (aimInputs[Proj_Name-1].nullVal != IsNull) {

        aflr2Instance->meshInput.outputFileName = EG_strdup(aimInputs[Proj_Name-1].vals.string);
        if (aflr2Instance->meshInput.outputFileName == NULL) return EGADS_MALLOC;
    }

    // Set aflr2 specific mesh inputs 
    if (aimInputs[Mesh_Gen_Input_String-1].nullVal != IsNull) {

        aflr2Instance->meshInput.aflr4Input.meshInputString =
                      EG_strdup(aimInputs[Mesh_Gen_Input_String-1].vals.string);
        if (aflr2Instance->meshInput.aflr4Input.meshInputString == NULL)
          return EGADS_MALLOC;
    }

    // Max and min number of points
    if (aimInputs[Edge_Point_Min-1].nullVal != IsNull) {
        minEdgePoint = aimInputs[Edge_Point_Min-1].vals.integer;
        if (minEdgePoint < 2) {
          printf("**********************************************************\n");
          printf("Edge_Point_Min = %d must be greater or equal to 2\n", minEdgePoint);
          printf("**********************************************************\n");
          status = CAPS_BADVALUE;
          goto cleanup;
        }
    }

    if (aimInputs[Edge_Point_Max-1].nullVal != IsNull) {
        maxEdgePoint = aimInputs[Edge_Point_Max-1].vals.integer;
        if (maxEdgePoint < 2) {
          printf("**********************************************************\n");
          printf("Edge_Point_Max = %d must be greater or equal to 2\n", maxEdgePoint);
          printf("**********************************************************\n");
          status = CAPS_BADVALUE;
          goto cleanup;
        }
    }

    if (maxEdgePoint >= 2 && minEdgePoint >= 2 && minEdgePoint > maxEdgePoint) {
      printf("**********************************************************\n");
      printf("Edge_Point_Max must be greater or equal Edge_Point_Min\n");
      printf("Edge_Point_Max = %d, Edge_Point_Min = %d\n",maxEdgePoint,minEdgePoint);
      printf("**********************************************************\n");
      status = CAPS_BADVALUE;
      goto cleanup;
    }

    // Get mesh sizing parameters
    if (aimInputs[Mesh_Sizing-1].nullVal != IsNull) {

        status = deprecate_SizingAttr(aimInputs[Mesh_Sizing-1].length,
                                      aimInputs[Mesh_Sizing-1].vals.tuple,
                                      &aflr2Instance->meshMap,
                                      &aflr2Instance->groupMap);
        if (status != CAPS_SUCCESS) return status;

        status = mesh_getSizingProp(aimInputs[Mesh_Sizing-1].length,
                                    aimInputs[Mesh_Sizing-1].vals.tuple,
                                    &aflr2Instance->meshMap,
                                    &numMeshProp,
                                    &meshProp);
        if (status != CAPS_SUCCESS) return status;
    }

    // Modify the EGADS body tessellation based on given inputs
/*@-nullpass@*/
    status =  mesh_modifyBodyTess(numMeshProp,
                                  meshProp,
                                  minEdgePoint,
                                  maxEdgePoint,
                                  (int) false, // quadMesh
                                  &refLen,
                                  aflr2Instance->meshInput.paramTess,
                                  aflr2Instance->meshMap,
                                  numBody,
                                  bodies);
/*@+nullpass@*/
    if (status != CAPS_SUCCESS) goto cleanup;

    // Run AFLR2 for each body
    for (bodyIndex = 0; bodyIndex < numBody; bodyIndex++) {

        printf("Getting 2D mesh for body %d (of %d)\n", bodyIndex+1, numBody);

        Message_Flag = aflr2Instance->meshInput.quiet == (int)true ? 0 : 1;
/*@-nullpass@*/
        status = aflr2_Surface_Mesh(Message_Flag, bodies[bodyIndex],
                                    aflr2Instance->meshInput,
                                    aflr2Instance->groupMap,
                                    aflr2Instance->meshMap,
                                    numMeshProp, meshProp,
                                    &aflr2Instance->surfaceMesh[bodyIndex]);
/*@+nullpass@*/
        if (status != CAPS_SUCCESS) {
            printf("Problem during surface meshing of body %d\n", bodyIndex+1);
            goto cleanup;
        }

        printf("Number of nodes = %d\n", aflr2Instance->surfaceMesh[bodyIndex].numNode);
        printf("Number of elements = %d\n", aflr2Instance->surfaceMesh[bodyIndex].numElement);
        if (aflr2Instance->surfaceMesh[bodyIndex].meshQuickRef.useStartIndex == (int) true ||
            aflr2Instance->surfaceMesh[bodyIndex].meshQuickRef.useListIndex == (int) true) {
            printf("Number of tris = %d\n", aflr2Instance->surfaceMesh[bodyIndex].meshQuickRef.numTriangle);
            printf("Number of quad = %d\n", aflr2Instance->surfaceMesh[bodyIndex].meshQuickRef.numQuadrilateral);
        }

    }

    // Clean up meshProps
    if (meshProp != NULL) {

        for (i = 0; i < numMeshProp; i++) {

            (void) destroy_meshSizingStruct(&meshProp[i]);
        }
        EG_free(meshProp);

        meshProp = NULL;
    }

    if (aflr2Instance->meshInput.outputFileName != NULL) {

        for (bodyIndex = 0; bodyIndex < aflr2Instance->numSurface; bodyIndex++) {

            if (aflr2Instance->numSurface > 1) {
                sprintf(bodyNumber, "%d", bodyIndex);
                filename = (char *) EG_alloc((strlen(aflr2Instance->meshInput.outputFileName)  +
                                              strlen("_2D_") + 2 +
                                              strlen(bodyNumber))*sizeof(char));
            } else {
                filename = (char *) EG_alloc((strlen(aflr2Instance->meshInput.outputFileName) +
                                              2)*sizeof(char));

            }

            if (filename == NULL) {
                status = EGADS_MALLOC;
                goto cleanup;
            }

            strcpy(filename, aflr2Instance->meshInput.outputFileName);

            if (aflr2Instance->numSurface > 1) {
                strcat(filename,"_2D_");
                strcat(filename, bodyNumber);
            }

            if (strcasecmp(aflr2Instance->meshInput.outputFormat, "AFLR3") == 0) {

                status = mesh_writeAFLR3(filename,
                                         aflr2Instance->meshInput.outputASCIIFlag,
                                         &aflr2Instance->surfaceMesh[bodyIndex],
                                         1.0);

            } else if (strcasecmp(aflr2Instance->meshInput.outputFormat, "VTK") == 0) {

                status = mesh_writeVTK(filename,
                                       aflr2Instance->meshInput.outputASCIIFlag,
                                       &aflr2Instance->surfaceMesh[bodyIndex],
                                       1.0);

            } else if (strcasecmp(aflr2Instance->meshInput.outputFormat, "Tecplot") == 0) {

                status = mesh_writeTecplot(filename,
                                           aflr2Instance->meshInput.outputASCIIFlag,
                                           &aflr2Instance->surfaceMesh[bodyIndex],
                                           1.0);

            } else if (strcasecmp(aflr2Instance->meshInput.outputFormat, "STL") == 0) {

                status = mesh_writeSTL(filename,
                                       aflr2Instance->meshInput.outputASCIIFlag,
                                       &aflr2Instance->surfaceMesh[bodyIndex],
                                       1.0);

            } else if (strcasecmp(aflr2Instance->meshInput.outputFormat, "FAST") == 0) {

                status = mesh_writeFAST(filename,
                                        aflr2Instance->meshInput.outputASCIIFlag,
                                        &aflr2Instance->surfaceMesh[bodyIndex],
                                        1.0);

            } else {
                printf("Unrecognized mesh format, \"%s\", the volume mesh will not be written out\n",
                       aflr2Instance->meshInput.outputFormat);
            }

            if (filename != NULL) EG_free(filename);
            filename = NULL;

            if (status != CAPS_SUCCESS) goto cleanup;
        }
    }


    status = CAPS_SUCCESS;
    goto cleanup;

cleanup:

    if (status != CAPS_SUCCESS)
        printf("Error: aflr2AIM status %d\n", status);

    if (meshProp != NULL) {

        for (i = 0; i < numMeshProp; i++) {
            (void) destroy_meshSizingStruct(&meshProp[i]);
        }

        EG_free(meshProp);
    }

    if (filename != NULL) EG_free(filename);
    return status;
}


/* the execution code from above should be moved here */
int aimExecute(/*@unused@*/ void *instStore, /*@unused@*/ void *aimStruc,
               int *state)
{
  *state = 0;
  return CAPS_SUCCESS;
}


/* no longer optional and needed for restart */
int aimPostAnalysis(/*@unused@*/ void *instStore, /*@unused@*/ void *aimStruc,
                    /*@unused@*/ int restart, /*@unused@*/ capsValue *inputs)
{
  return CAPS_SUCCESS;
}


int aimOutputs(/*@unused@*/ void *instStore, /*@unused@*/ void *aimStruc,
               /*@unused@*/ int index, char **aoname, capsValue *form)
{
    /*! \page aimOutputsAFLR2 AIM Outputs
     * The following list outlines the AFLR2 AIM outputs available through the AIM interface.
     *
     * - <B>Done</B> = True if a surface mesh was created on all surfaces, False if not.
     */

    int status = CAPS_SUCCESS;

#ifdef DEBUG
    printf(" aflr2AIM/aimOutputs index = %d!\n", index);
#endif

    if (index == Done) {
        *aoname            = AIM_NAME(Done);
        form->type         = Boolean;
        form->vals.integer = (int) false;

    } else if (index == Surface_Mesh) {
        *aoname           = AIM_NAME(Surface_Mesh);
        form->type        = Pointer;
        form->vals.AIMptr = NULL;
        AIM_STRDUP(form->units, "meshStruct", aimStruc, status);

        /*! \page aimOutputsAFLR2
         * - <B> Surface_Mesh </B> <br>
         * The surface mesh
         */
    }

    AIM_NOTNULL(*aoname, aimStruc, status);

cleanup:
    if (status != CAPS_SUCCESS) AIM_FREE(*aoname);
    return status;
}


// See if a surface mesh was generated for each body
int aimCalcOutput(void *instStore, /*@unused@*/ void *aimInfo,
                  /*@unused@*/ int index, capsValue *val)
{
    int        surf, status = CAPS_SUCCESS;
    aimStorage *aflr2Instance;

#ifdef DEBUG
    printf(" aflr2AIM/aimCalcOutput index = %d!\n",  index);
#endif
    aflr2Instance = (aimStorage *) instStore;

    if (Done == index) {

      val->vals.integer = (int) false;

      // Check to see if surface meshes was generated
      for (surf = 0; surf < aflr2Instance->numSurface; surf++ ) {

          if (aflr2Instance->surfaceMesh[surf].numElement != 0) {

              val->vals.integer = (int) true;

          } else {

              val->vals.integer = (int) false;
              printf("No surface Tris and/or Quads were generated for surface - %d\n",
                     surf);
              return CAPS_SUCCESS;
          }
      }
    } else if (Surface_Mesh == index) {

        // Return the surface meshes
        val->length = aflr2Instance->numSurface;
        val->vals.AIMptr = aflr2Instance->surfaceMesh;

    } else {

        status = CAPS_BADINDEX;
        AIM_STATUS(aimInfo, status, "Unknown output index %d!", index);

    }

cleanup:

    return status;
}

/************************************************************************/

/*
 * since this AIM does not support field variables or CAPS bounds, the
 * following functions are not required to be filled in except for aimDiscr
 * which just stores away our bodies and aimFreeDiscr that does some cleanup
 * when CAPS terminates
 */


void aimCleanup(void *instStore)
{
    int status; // Returning status
    aimStorage *aflr2Instance;

#ifdef DEBUG
    printf(" aflr2AIM/aimCleanup!\n");
#endif
    aflr2Instance = (aimStorage *) instStore;

    // Clean up aflr2Instance data
    status = destroy_aimStorage(aflr2Instance);
    if (status != CAPS_SUCCESS)
          printf("Status = %d, aflr2AIM aimStorage cleanup!!!\n", status);

    EG_free(aflr2Instance);
}
