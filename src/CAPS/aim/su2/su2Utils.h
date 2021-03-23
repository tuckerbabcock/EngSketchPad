
enum aimInputs
{
  Proj_Name = 1,                 /* index is 1-based */
  Mach,
  Re,
  Physical_Problem,
  Equation_Type,
  Alpha,
  Beta,
  Overwrite_CFG,
  Num_Iter,
  CFL_Number,
  Boundary_Condition,
  MultiGrid_Level,
  Residual_Reduction,
  Unit_System,
  Reference_Dimensionalization,
  Freestream_Pressure,
  Freestream_Temperature,
  Freestream_Density,
  Freestream_Velocity,
  Freestream_Viscosity,
  Moment_Center,
  Moment_Length,
  Reference_Area,
  Pressure_Scale_Factor,
  Pressure_Scale_Offset,
  Output_Format,
  Two_Dimensional,
  Convective_Flux,
  SU2_Version,
  Surface_Monitor,
  Surface_Deform,
  Input_String,
  Mesh,
  NUMINPUT = Mesh       /* Total number of inputs */
};

#ifdef __cplusplus
extern "C" {
#endif

// Extract the FEPOINT Tecoplot data from a FUN3D Aero-Loads file (connectivity is ignored) - dataMatrix = [numVariable][numDataPoint]
int su2_readAeroLoad(char *filename, int *numVariable, char **variableName[],
                     int *numDataPoint, double ***dataMatrix);

// Write SU2 surface motion file (connectivity is optional) - dataMatrix = [numVariable][numDataPoint], connectMatrix (optional) = [4*numConnect]
//  the formating of the data may be specified through dataFormat = [numVariable] (use capsTypes Integer and Double)- If NULL default to double
int su2_writeSurfaceMotion(char *filename,
                           int numVariable,
                           int numDataPoint,
                           double **dataMatrix,
                           int *dataFormat,
                           int numConnect,
                           int *connectMatrix);


// Write SU2 data transfer files
int su2_dataTransfer(void *aimInfo,
                     char *projectName,
                     meshStruct volumeMesh);

// Writes out surface indexes for a marker list
int su2_marker(void *aimInfo, const char* iname, capsValue *aimInputs, FILE *fp,
               cfdBoundaryConditionStruct bcProps);

// Write SU2 configuration file for version Cardinal (4.0)
int su2_writeCongfig_Cardinal(void *aimInfo, capsValue *aimInputs,
                              cfdBoundaryConditionStruct bcProps);

// Write SU2 configuration file for version Raven (5.0)
int su2_writeCongfig_Raven(void *aimInfo, capsValue *aimInputs,
                           cfdBoundaryConditionStruct bcProps);

// Write SU2 configuration file for version Falcon (6.1)
int su2_writeCongfig_Falcon(void *aimInfo, capsValue *aimInputs,
                            cfdBoundaryConditionStruct bcProps, int withMotion);

// Write SU2 configuration file for version Falcon (7.0.7)
int su2_writeCongfig_Blackbird(void *aimInfo, capsValue *aimInputs,
                               cfdBoundaryConditionStruct bcProps, int withMotion);

#ifdef __cplusplus
}
#endif
