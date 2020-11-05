#include "UVMAP_LIB.h"

/*
 * UVMAP : TRIA-FACE SURFACE MESH UV MAPPING GENERATOR
 *         DERIVED FROM AFLR4, UG, UG2, and UG3 LIBRARIES
 * $Id: uvmap_message.c,v 1.3 2020/07/04 20:52:05 marcum Exp $
 * Copyright 1994-2020, David L. Marcum
 */

// Write a message to standard output and a file if specified.

void uvmap_message (char *Text) {
  extern FILE * UG_Output_File;
  fprintf (stdout, "%s\n", Text);
  fflush (stdout);
  if (UG_Output_File) {
    fprintf (UG_Output_File, "%s\n", Text);
    fflush (UG_Output_File);
  }
  return;
}

// Write a message to standard error and a file if specified.

void uvmap_error_message (char *Text) {
  extern FILE * UG_Output_File;
  fprintf (stderr, "%s\n", Text);
  fflush (stderr);
  if (UG_Output_File) {
    fprintf (UG_Output_File, "%s\n", Text);
    fflush (UG_Output_File);
  }
  return;
}
