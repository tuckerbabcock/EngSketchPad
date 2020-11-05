#include "UVMAP_LIB.h"

/*
 * UVMAP : TRIA-FACE SURFACE MESH UV MAPPING GENERATOR
 *         DERIVED FROM AFLR4, UG, UG2, and UG3 LIBRARIES
 * $Id: uvmap_version.c,v 1.33 2020/10/21 05:40:41 marcum Exp $
 * Copyright 1994-2020, David L. Marcum
 */

void uvmap_version (
  char Compile_Date[],
  char Compile_OS[],
  char Version_Date[],
  char Version_Number[])
{
  // Put compile date, compile OS, version date, and version number in text
  // string.

strncpy (Compile_Date, "", 40);
strncpy (Compile_OS, "", 40);
strncpy (Version_Date, "10/21/20 @ 01:40AM", 40);
strncpy (Version_Number, "1.5.2", 40);

  strcpy (&(Compile_Date[40]), "\0");
  strcpy (&(Compile_OS[40]), "\0");
  strcpy (&(Version_Date[40]), "\0");
  strcpy (&(Version_Number[40]), "\0");

  return;

}
