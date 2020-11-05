#include "../UVMAP_LIB.h"

int EG_uvmap_read (
  char Case_Name[],
  int *ntria,
  int *nvert,
  int **idmap,
  int **tria,
  double **xyz);

int EG_uvmap_test (char Case_Name[], int verbosity);

int EG_uvmap_write (
  char Case_Name[],
  int ntria,
  int nvert,
  int *idmap,
  int *tria,
  double *uv,
  double *xyz);

INT_ uvmap_read (
  char Case_Name[],
  INT_ *nbface,
  INT_ *nnode,
  INT_ **idibf,
  INT_3D **inibf,
  DOUBLE_3D **x);

INT_ uvmap_test (char Case_Name[], INT_ verbosity);

INT_ uvmap_write (
  char Case_Name[],
  INT_ nbface,
  INT_ nnode,
  INT_ *idibf,
  INT_3D *inibf,
  DOUBLE_2D *u,
  DOUBLE_3D *x);

FILE *UG_Output_File = NULL;

int main (int argc, char *argv[]) {

  char Case_Name[256], File_Name[512],
       Compile_Date[41], Compile_OS[41], Version_Date[41], Version_Number[41];

  int i;
  int ver_info = 0;
  int help_info = 0;

  int api = 0;
  int log_file = 0;
  int status = 0;
  int verbosity = 1;

  // get program arguments

  if (argc < 2)
    help_info = 1;

  // set input parameters

  for (i = 1; i < argc; i++) {

    if (strcmp (argv[i], "-aflr_api") == 0)
      api = 0;
    else if (strcmp (argv[i], "-egads_api") == 0)
      api = 1;
    else if (strcmp (argv[i], "-log") == 0)
      log_file = 1;
    else if (strcmp (argv[i], "-cpu") == 0)
      verbosity = 2;
    else if (strcmp (argv[i], "-ver") == 0 || strcmp (argv[i], "--ver") == 0 ||
             strcmp (argv[i], "-build") == 0 || strcmp (argv[i], "--build") == 0)
      ver_info = 1;
    else if (strcmp (argv[i], "-version") == 0 || strcmp (argv[i], "--version") == 0)
      ver_info = 2;
    else if (strcmp (argv[i], "-h") == 0 || strcmp (argv[i], "-help") == 0)
      help_info = 1;
    else if (strncmp (argv[i], "-", 1) == 0)
      help_info = 1;
    else
      strcpy (Case_Name, argv[i]);
  }

  // output usage information

  if (help_info) {
    printf ("\n\
uvmap case_name [options]\n\
\n\
Parameter Name        Description\n\
___________________   _____________________________________________________\n\
\n\
-aflr_api           : Use API with AFLR style arrays (default).\n\
-egads_api          : Use API with EGADS style arrays.\n\
-log                : Generate a log file with all output messages.\n\
-cpu                : Generate full CPU usage output messages.\n\
-h, -help           : Output summary of options.\n\
-ver, --ver         : Output version number.\n\
-version, --version : Output version number information.\n\
-build, --build     : Output build number.\n\
case_name           : Case Name for input file case_name.surf, output file\n\
                      case_name_uv.surf, and log file (if -log is used)\n\
                      case_name.uvmap.log.\n\
");
    exit (0);
  }

  // output version number

  if (ver_info == 1) {
    uvmap_version (Compile_Date, Compile_OS, Version_Date, Version_Number);
    uvmap_message (Version_Number);
    exit (0);
  }

  // output version information

  if (ver_info == 2) {
    uvmap_version (Compile_Date, Compile_OS, Version_Date, Version_Number);
    printf ("UVMAP    : Version Number %s\n", Version_Number);
    printf ("UVMAP    : Version Date   %s\n", Version_Date);
    printf ("\n");
    printf ("Lib Info : Library Name   uvmap\n");
    printf ("Lib Info : Version Number %s\n", Version_Number);
    printf ("Lib Info : Version Date   %s\n", Version_Date);
    printf ("Lib Info : Compile OS     %s\n", Compile_OS);
    printf ("Lib Info : Compile Date   %s\n", Compile_Date);
    exit (0);
  }

  // open output log file

  if (log_file) {
    sprintf (File_Name, "%s.uvmap.log", Case_Name);
    UG_Output_File = fopen (File_Name, "w");
  }

  // test uvmap with EGADS API

  if (api)
    status = EG_uvmap_test (Case_Name, verbosity);

  // test uvmap with AFLR API

  else
    status = (int) uvmap_test (Case_Name, (INT_) verbosity);

  // close output log file

  if (log_file)
    fclose (UG_Output_File);

  exit (status);
}

/*

--------------------------------------------------------------------------------
Read and allocate data from a SURF type surface mesh file.
EGADS style data are used in this API.
--------------------------------------------------------------------------------

int EG_uvmap_read(
  char Case_Name[],
  int *ntria,
  int *nvert,
  int **idmap,
  int **tria,
  double **xyz);


INPUT ARGUMENTS
---------------

Case_Name	Case name for SURF type surface mesh file, Case_Name.surf.


RETURN VALUE
------------

EGADS_SUCCESS	Normal completion without errors.
EGADS_MALLOC	Unable to allocate required memory.
EGADS_READERR	An error occurred.


OUTPUT ARGUMENTS
----------------

ntria		Number of tria-faces.

nvert		Number of nodes/vertices.

idmap		Surface ID label for each tria-face (ntria in length).

tria		Tria-face connectivity (3*ntria in length).

xyz		XYZ coordinates (3*nvert in length).

*/

int EG_uvmap_read (
  char Case_Name[],
  int *ntria,
  int *nvert,
  int **idmap,
  int **tria,
  double **xyz)
{
  // Read and allocate data from a SURF type surface mesh file.

  INT_ *idibf = NULL;
  INT_3D *inibf = NULL;

  double *uv = NULL;
  DOUBLE_3D *x = NULL;

  INT_ nbface = 0;
  INT_ nnode = 0;
  INT_ status = 0;

  *ntria = 0;
  *nvert = 0;
  *idmap = NULL;
  *tria = NULL;
  *xyz = NULL;

  status = uvmap_read (Case_Name, &nbface, &nnode, &idibf, &inibf, &x);

  if (status == 0)
    status = uvmap_to_egads (nbface, nnode, idibf, inibf, NULL, x,
                             ntria, nvert, idmap, tria, &uv, xyz);

  uvmap_free (idibf);
  uvmap_free (inibf);
  uvmap_free (x);

  // set return value

  if (status > 100000)
    status = EGADS_MALLOC;
  else if (status)
    status = EGADS_READERR;
  else
    status = EGADS_SUCCESS;

  return (int) status;
}

/*

--------------------------------------------------------------------------------
Test uvmap with EGADS APIs.
--------------------------------------------------------------------------------

int EG_uvmap_test (char Case_Name[], int verbosity);


INPUT ARGUMENTS
---------------

Case_Name	Case name for input and output SURF type surface mesh files.

verbosity	Message flag.
		If verbosity=0 then do not output progress information.
		If verbosity=1 then output progress information to stdout.
		If verbosity=2 then output progress and additional CPU usage
		information to stdout.


RETURN VALUE
------------

EGADS_SUCCESS	Normal completion without errors.
EGADS_MALLOC	Unable to allocate required memory.
EGADS_UVMAP	An error occurred.

*/

int EG_uvmap_test (char Case_Name[], int verbosity)
{
  // Test uvmap with EGADS APIs.

  void *ptr = NULL;

  int *local_idef = NULL;
  int *tria = NULL;
  double *uv = NULL;
  double *xyz = NULL;

  int idef = 1;
  int ntria = 0;
  int nvert = 0;
  int set_struct = 1;
  int status = 0;

  // interpolation testing variables

  char Text[512];

  int ivert[3];
  int i, j, k, itriai, local_idefi;
  int location_found = 0;
  int correct_shape_function = 0;
  int correct_tria = 0;
  int correct_vertex = 0;
  int correct_vertices = 0;

  double uvi[2], s[3];
  double tol = 1.0e-6;

  // read xyz surface mesh

  status = EG_uvmap_read (Case_Name, &ntria, &nvert, &local_idef, &tria, &xyz);

  // generate uv mapping

  if (status == 0)
    status = EG_uvmap_gen (idef, ntria, nvert, set_struct, verbosity,
                           local_idef, tria, xyz, &uv, &ptr);

  // check interpolation

  if (status == 0) {

    uvmap_message ("");
    uvmap_message ("UVMAP    : CHECKING INTERPOLATION");
    uvmap_message ("");

    // loop over tria-faces and check interpolation at tria-face centroids

    i = 0;

    while (i < ntria && (status == EGADS_SUCCESS || status == EGADS_NOTFOUND)) {

      // set UV coordinates of interpolation location equal to the centroid of
      // tria-face

      for (j = 0; j < 2; j++) {
        uvi[j] = (uv[2*(tria[3*i  ]-1)+j]
                + uv[2*(tria[3*i+1]-1)+j]
                + uv[2*(tria[3*i+2]-1)+j]) / 3.0;
      }

      // find interpolation location

      status = EG_uvmap_find_uv (idef, uvi, ptr, &local_idefi, &itriai, ivert, s); 

      // if location was found then check interpolation results

      if (status == EGADS_SUCCESS) {

        location_found++;

        // check if tria-face index is correct

        if (itriai == i+1)
          correct_tria++;

        // check if nodes/vertices are correct
        // note that all orientations are checked as the tria-face connectivity
        // within the UV mapping structure may be reordered for ordering
        // consistency

        correct_vertex = 0;
        for (k = 0; k < 3; k++) {
          for (j = 0; j < 3; j++) {
            if (ivert[k] == tria[3*i+j]) correct_vertex++;
          }
        }
        if (correct_vertex == 3)
          correct_vertices++;

        // check if shape functions are correct

        if (sqrt ((s[0]-1.0/3.0)*(s[0]-1.0/3.0)
                + (s[1]-1.0/3.0)*(s[1]-1.0/3.0)
                + (s[2]-1.0/3.0)*(s[2]-1.0/3.0)) <= tol)
          correct_shape_function++;
      }

      i++;
    }

    // output interpolation check results

    if (location_found == ntria)
      snprintf (Text, 512, "UVMAP    : All %d locations were found", ntria);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations were found", location_found, ntria);
    uvmap_message (Text);

    if (correct_tria == ntria)
      snprintf (Text, 512, "UVMAP    : All %d locations have correct tria-face index", ntria);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations have correct tria-face index", correct_tria, ntria);
    uvmap_message (Text);

    if (correct_vertices == ntria)
      snprintf (Text, 512, "UVMAP    : All %d locations have correct nodes/vertices", ntria);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations have correct nodes/vertices", correct_vertices, ntria);
    uvmap_message (Text);

    if (correct_shape_function == ntria)
      snprintf (Text, 512, "UVMAP    : All %d locations have correct shape functions", ntria);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations have correct shape functions", correct_shape_function, ntria);
    uvmap_message (Text);
  }

  // write uv surface mesh

  if (status == 0)
    status = EG_uvmap_write (Case_Name, ntria, nvert, local_idef, tria, uv, NULL);

  // free UV mapping data structure

  uvmap_struct_free (ptr);

  // free starting data arrays

  uvmap_free (local_idef);
  uvmap_free (tria);
  uvmap_free (uv);
  uvmap_free (xyz);

  return status;
}

/*

--------------------------------------------------------------------------------
Write data to a SURF type surface mesh file.
EGADS style data are used in this API.
--------------------------------------------------------------------------------

int EG_uvmap_write (
  char Case_Name[],
  int ntria,
  int nvert,
  int *idmap,
  int *tria,
  double *uv,
  double *xyz);


INPUT ARGUMENTS
---------------

Case_Name	Case name for SURF type surface mesh file, Case_Name_uv.surf or
		Case_Name_xyz.surf.

ntria		Number of tria-faces.

nvert		Number of nodes/vertices.

idmap		Surface ID label for each tria-face (ntria in length).

tria		Tria-face connectivity (3*ntria in length).

uv		UV coordinates (2*nvert in length).
		If uv is allocated then a Case_Name_uv.surf file will be
		written with UV coordinates for the XY coordinates within the
		output file and zero for the Z.

xyz		XYZ coordinates (3*nvert in length).
		If xyz is allocated then a Case_Name_xyz.surf file will be
		written with XYZ coordinates.


RETURN VALUE
------------

EGADS_SUCCESS	Normal completion without errors.
EGADS_MALLOC	Unable to allocate required memory.
EGADS_WRITERR	An error occurred.


OUTPUT ARGUMENTS
----------------

*/

int EG_uvmap_write (
  char Case_Name[],
  int ntria,
  int nvert,
  int *idmap,
  int *tria,
  double *uv,
  double *xyz)
{
  // Write data to a SURF type surface mesh file.

  INT_ *idibf = NULL;
  INT_3D *inibf = NULL;

  DOUBLE_2D *u = NULL;
  DOUBLE_3D *x = NULL;

  INT_ nbface = 0;
  INT_ nnode = 0;
  INT_ status = 0;

  status = uvmap_from_egads (ntria, nvert, idmap, tria, uv, xyz,
                             &nbface, &nnode, &idibf, &inibf, &u, &x);

  if (status == 0)
    status = uvmap_write (Case_Name, nbface, nnode, idibf, inibf, u, x);

  uvmap_free (idibf);
  uvmap_free (inibf);
  uvmap_free (u);
  uvmap_free (x);

  // set return value

  if (status > 100000)
    status = EGADS_MALLOC;
  else if (status)
    status = EGADS_WRITERR;
  else
    status = EGADS_SUCCESS;

  return (int) status;
}

/*

--------------------------------------------------------------------------------
Read and allocate data from a SURF type surface mesh file.
--------------------------------------------------------------------------------

INT_ uvmap_read (
  char Case_Name[],
  INT_ *nbface,
  INT_ *nnode,
  INT_ **idibf,
  INT_3D **inibf,
  DOUBLE_3D **x);


INPUT ARGUMENTS
---------------

Case_Name	Case name for SURF type surface mesh file, Case_Name.surf.


RETURN VALUE
------------

0		Normal completion without errors.
>0		An error occurred.


OUTPUT ARGUMENTS
----------------

nbface		Number of tria-faces.

nnode		Number of nodes/vertices.

idibf		Surface ID label for each tria-face (nbface+1 in length).

inibf		Tria-face connectivity (nbface+1 in length).

x		XYZ coordinates (nnode+1 in length).

*/

INT_ uvmap_read (
  char Case_Name[],
  INT_ *nbface,
  INT_ *nnode,
  INT_ **idibf,
  INT_3D **inibf,
  DOUBLE_3D **x)
{
  // Read and allocate data from a SURF type surface mesh file.

  FILE *Grid_File;

  char File_Name[500], Text_Line[512], Text[512];
  char *Read_Label;

  int i1, i2, i3, i4, i5, i6;
  INT_ i, n;
  INT_ status = 0;

  double d1, d2;

  *nbface = 0;
  *nnode = 0;
  *idibf = NULL;
  *inibf = NULL;
  *x = NULL;

  strcpy (File_Name, Case_Name);
  strcat (File_Name, ".surf");

  uvmap_message ("");
  snprintf (Text, 512, "UVMAP    : Reading Surf File = %s", File_Name);
  uvmap_message (Text);

  Grid_File = fopen (File_Name, "r");

  if (Grid_File == NULL)
  {
    uvmap_error_message ("*** ERROR 3509 : error opening surface mesh file ***");
    return 3509;
  }

  n = fscanf (Grid_File, "%d %d %d", &i1, &i2, &i3);

  if (n == EOF) {
    uvmap_error_message ("*** ERROR 3510 : error reading SURF surface mesh file ***");
    status = 3510;
  }

  *nbface = (INT_) i1;
  *nnode  = (INT_) i3;

  if (status == 0) {
    *idibf = (INT_ *) uvmap_malloc (&status, ((*nbface)+1)*sizeof(INT_));
    *inibf = (INT_3D *) uvmap_malloc (&status, ((*nbface)+1)*sizeof(INT_3D));
    *x = (DOUBLE_3D *) uvmap_malloc (&status, ((*nnode)+1)*sizeof(DOUBLE_3D));

    if (status) {
      uvmap_error_message ("*** ERROR 103515 : unable to allocate required memory ***");
      status = 103515;
    }
  }

  if (status == 0) {
    Read_Label = fgets (Text_Line, 512, Grid_File);

    if (Read_Label)
      Read_Label = fgets (Text_Line, 512, Grid_File);

    if (Read_Label == NULL) {
      uvmap_error_message ("*** ERROR 3511 : error reading SURF surface mesh file ***");
      status = 3511;
    }
  }

  if (status == 0) {
    i = 1;
    n = sscanf (Text_Line, "%lf %lf %lf %lf %lf",
                &((*x)[i][0]), &((*x)[i][1]), &((*x)[i][2]), &d1, &d2);

    if (n == 3) {
      for (i = 2; i <= *nnode; i++) {
        n = fscanf (Grid_File, "%lf %lf %lf",
                    &((*x)[i][0]), &((*x)[i][1]), &((*x)[i][2]));
      }
    }

    else if (n == 4) {
      for (i = 2; i <= *nnode; i++) {
        n = fscanf (Grid_File, "%lf %lf %lf %lf",
                    &((*x)[i][0]), &((*x)[i][1]), &((*x)[i][2]), &d1);
      }
    }

    else if (n == 5) {
      for (i = 2; i <= *nnode; i++) {
        n = fscanf (Grid_File, "%lf %lf %lf %lf %lf",
                    &((*x)[i][0]), &((*x)[i][1]), &((*x)[i][2]), &d1, &d2);
      }
    }

    else
      n = EOF;

    if (n == EOF) {
      uvmap_error_message ("*** ERROR 3512 : error reading SURF surface mesh file ***");
      status = 3512;
    }
  }

  if (status == 0) {
    for (i = 1; i <= *nbface; i++) {
      n = fscanf (Grid_File, "%d %d %d %d %d %d", &i1, &i2, &i3, &i4, &i5, &i6);

      (*inibf)[i][0] = (INT_) i1;
      (*inibf)[i][1] = (INT_) i2;
      (*inibf)[i][2] = (INT_) i3;
      (*idibf)[i]    = (INT_) i4;
    }

    if (n == EOF) {
      uvmap_error_message ("*** ERROR 3513 : error reading SURF surface mesh file ***");
      status = 3513;
    }
  }

  fclose (Grid_File);

  if (status == 0) {
    uvmap_message ("");
    snprintf (Text, 512, "UVMAP    : Tria Surface Faces=%10d", (int) *nbface);
    uvmap_message (Text);
    snprintf (Text, 512, "UVMAP    : Nodes             =%10d", (int) *nnode);
    uvmap_message (Text);
    uvmap_message ("");
  }

  return 0;
}

/*

--------------------------------------------------------------------------------
Test uvmap.
--------------------------------------------------------------------------------

INT_ uvmap_test (char Case_Name[], INT_ verbosity);


INPUT ARGUMENTS
---------------

Case_Name	Case name for input and output SURF type surface mesh files.

verbosity	Message flag.
		If verbosity=0 then do not output progress information.
		If verbosity=1 then output progress information to stdout.
		If verbosity=2 then output progress and additional CPU usage
		information to stdout.


RETURN VALUE
------------

0		Normal completion without errors.
>0		An error occurred.

*/

INT_ uvmap_test (char Case_Name[], INT_ verbosity)
{
  // Test uvmap with EGADS APIs.

  void *ptr = NULL;

  INT_ *idibf = NULL;
  INT_3D *inibf = NULL;
  DOUBLE_2D *u = NULL;
  DOUBLE_3D *x = NULL;

  INT_ idef = 1;
  INT_ nbface = 0;
  INT_ nnode = 0;
  INT_ set_struct = 1;
  INT_ status = 0;

  // interpolation testing variables

  char Text[512];

  INT_ inode_[3];
  INT_ ibface, ibface_, j, local_idefi;
  INT_ location_found = 0;
  INT_ correct_shape_function = 0;
  INT_ correct_tria = 0;
  INT_ correct_vertex = 0;
  INT_ correct_vertices = 0;

  double u_[2], s[3];
  double tol = 1.0e-6;

  // read xyz surface mesh

  status = uvmap_read (Case_Name, &nbface, &nnode, &idibf, &inibf, &x);

  // generate uv mapping

  if (status == 0)
    status = uvmap_gen (idef, nbface, nnode, set_struct, verbosity,
                        idibf, &inibf, &x, &u, &ptr);

  // check interpolation

  if (status == 0) {

    uvmap_message ("");
    uvmap_message ("UVMAP    : CHECKING INTERPOLATION");
    uvmap_message ("");

    // loop over tria-faces and check interpolation at tria-face centroids

    ibface = 1;

    while (ibface <= nbface && status <= 0) {

      // set UV coordinates of interpolation location equal to the centroid of
      // tria-face

      for (j = 0; j < 2; j++) {
        u_[j] = (u[inibf[ibface][0]][j]
               + u[inibf[ibface][1]][j]
               + u[inibf[ibface][2]][j]) / 3.0;
      }

      // find interpolation location

      status = uvmap_find_uv (idef, u_, ptr, &local_idefi, &ibface_, inode_, s); 
      // if location was found then check interpolation results

      if (status == EGADS_SUCCESS) {

        location_found++;

        // check if tria-face index is correct

        if (ibface_ == ibface)
          correct_tria++;

        // check if nodes/vertices are correct
        // note that all orientations are checked as ordering may have changed

        correct_vertex = 0;
        for (j = 0; j < 3; j++) {
          if (inode_[j] == inibf[ibface][j]) correct_vertex++;
        }
        if (correct_vertex == 3)
          correct_vertices++;

        // check if shape functions are correct

        if (sqrt ((s[0]-1.0/3.0)*(s[0]-1.0/3.0)
                + (s[1]-1.0/3.0)*(s[1]-1.0/3.0)
                + (s[2]-1.0/3.0)*(s[2]-1.0/3.0)) <= tol)
          correct_shape_function++;
      }

      ibface++;
    }

    // output interpolation check results

    if (location_found == nbface)
      snprintf (Text, 512, "UVMAP    : All %d locations were found", (int) nbface);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations were found", (int) location_found, (int) nbface);
    uvmap_message (Text);

    if (correct_tria == nbface)
      snprintf (Text, 512, "UVMAP    : All %d locations have correct tria-face index", (int) nbface);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations have correct tria-face index", (int) correct_tria, (int) nbface);
    uvmap_message (Text);

    if (correct_vertices == nbface)
      snprintf (Text, 512, "UVMAP    : All %d locations have correct nodes/vertices", (int) nbface);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations have correct nodes/vertices", (int) correct_vertices, (int) nbface);
    uvmap_message (Text);

    if (correct_shape_function == nbface)
      snprintf (Text, 512, "UVMAP    : All %d locations have correct shape functions", (int) nbface);
    else
      snprintf (Text, 512, "UVMAP    : Only %d of %d locations have correct shape functions", (int) correct_shape_function, (int) nbface);
    uvmap_message (Text);
  }

  // write uv surface mesh

  if (status == 0)
    status = uvmap_write (Case_Name, nbface, nnode, idibf, inibf, u, NULL);

  // free UV mapping data structure

  uvmap_struct_free (ptr);

  // free starting data arrays

  uvmap_free (idibf);
  uvmap_free (inibf);
  uvmap_free (u);
  uvmap_free (x);

  return status;
}

/*

--------------------------------------------------------------------------------
Write data to a SURF type surface mesh file.
--------------------------------------------------------------------------------

INT_ uvmap_write (
  char Case_Name[],
  INT_ nbface,
  INT_ nnode,
  INT_ *idibf,
  INT_3D *inibf,
  DOUBLE_2D *u,
  DOUBLE_3D *x);


INPUT ARGUMENTS
---------------

Case_Name	Case name for SURF type surface mesh file, Case_Name_uv.surf or
		Case_Name_xyz.surf.

nbface		Number of tria-faces.

nnode		Number of nodes/vertices.

idibf		Surface ID label for each tria-face (nbface+1 in length).

inibf		Tria-face connectivity (nbface+1 in length).

u		UV coordinates (nnode+1 in length).
		If u is allocated then a Case_Name_uv.surf file will be
		written with UV coordinates for the XY coordinates within the
		output file and zero for the Z.

x		XYZ coordinates (nnode+1 in length).
		If x is allocated then a Case_Name_xyz.surf file will be
		written with XYZ coordinates.


RETURN VALUE
------------

0		Normal completion without errors.
>0		An error occurred.


*/

INT_ uvmap_write (
  char Case_Name[],
  INT_ nbface,
  INT_ nnode,
  INT_ *idibf,
  INT_3D *inibf,
  DOUBLE_2D *u,
  DOUBLE_3D *x)
{
  // Write surface mesh file.

  FILE *Grid_File;

  char File_Name[512], Text[512];

  INT_ i, n, type;

  for (type = 1; type <= 2; type++) {

    if ((type == 1 && u) || (type == 2 && x)) {

      strcpy (File_Name, Case_Name);
      if (type == 1)
        strcat (File_Name, "_uv.surf");
      else
        strcat (File_Name, "_xyz.surf");

      uvmap_message ("");
      snprintf (Text, 512, "UVMAP    : Writing Surf File = %s", File_Name);
      uvmap_message (Text);
      uvmap_message ("");
      snprintf (Text, 512, "UVMAP    : Tria Surface Faces=%10d", (int) nbface);
      uvmap_message (Text);
      snprintf (Text, 512, "UVMAP    : Nodes             =%10d", (int) nnode);
      uvmap_message (Text);
      uvmap_message ("");

      Grid_File = fopen (File_Name, "w");

      if (Grid_File == NULL)
      {
        uvmap_error_message ("*** ERROR 3514 : error opening surface mesh file ***");
        return 3514;
      }

      n = fprintf (Grid_File, "%d 0 %d\n", (int) nbface, (int) nnode);

      if (type == 1) {
        for (i = 1; i <= nnode; i++) {
          n = fprintf (Grid_File, "%lf %lf 0.0\n", u[i][0], u[i][1]);
        }
      }
      else {
        for (i = 1; i <= nnode; i++) {
          n = fprintf (Grid_File, "%lf %lf %lf\n", x[i][0], x[i][1], x[i][2]);
        }
      }

      if (idibf) {
        for (i = 1; i <= nbface; i++) {
          n = fprintf (Grid_File, "%d %d %d %d 0 0\n", (int) inibf[i][0], (int) inibf[i][1], (int) inibf[i][2], (int) idibf[i]);
        }
      }
      else {
        for (i = 1; i <= nbface; i++) {
          n = fprintf (Grid_File, "%d %d %d 1 0 0\n", (int) inibf[i][0], (int) inibf[i][1], (int) inibf[i][2]);
        }
      }

      if (n < 0) {
        uvmap_error_message ("*** ERROR 3515 : error writing SURF surface mesh file ***");
        fclose (Grid_File);
        return 3515;
      }

      fclose (Grid_File);
    }
  }

  return 0;
}
