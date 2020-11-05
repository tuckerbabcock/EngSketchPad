// AFLR3 interface functions - Modified from functions provided with
//    AFLR3_LIB source (aflr3.c) written by David L. Marcum

int aflr3_Volume_Mesh (void *aimInfo, capsValue *aimInputs,
                       meshInputStruct meshInput,
                       int createBL,
                       int blFlag[],
                       double blSpacing[],
                       double blThickness[],
                       double capsMeshLength,
                       int numMeshProp,
                       meshSizingStruct *meshProp,
                       meshStruct *surfaceMesh,
                       meshStruct *volumeMesh);
