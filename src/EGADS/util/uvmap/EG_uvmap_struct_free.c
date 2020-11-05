#include "UVMAP_LIB.h"

/*
 * UVMAP : TRIA-FACE SURFACE MESH UV MAPPING GENERATOR
 *         DERIVED FROM AFLR4, UG, UG2, and UG3 LIBRARIES
 * $Id: EG_uvmap_struct_free.c,v 1.1 2020/06/15 18:08:07 marcum Exp $
 * Copyright 1994-2020, David L. Marcum
 */

/*

--------------------------------------------------------------------------------
Free UV mapping data structure for all surfaces.
--------------------------------------------------------------------------------

void EG_uvmap_struct_free (void *ptr);


INPUT ARGUMENTS
---------------

ptr	UV mapping data structure.

*/

void EG_uvmap_struct_free (void *ptr)
{
  uvmap_struct_free (ptr);
  return;
}
