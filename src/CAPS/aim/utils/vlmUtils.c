// Vortex lattice analysis related utility functions - Written by Dr. Ryan Durscher AFRL/RQVC

#include <string.h>
#include <math.h>
#include "capsTypes.h"  // Bring in CAPS types
#include "vlmTypes.h"   // Bring in Vortex Lattice Method structures
#include "vlmUtils.h"
#include "miscUtils.h"  // Bring in miscellaneous utilities

#ifdef WIN32
#define strcasecmp  stricmp
#endif

#define NINT(A)         (((A) < 0)   ? (int)(A-0.5) : (int)(A+0.5))

// Fill vlmSurface in a vlmSurfaceStruct format with vortex lattice information
// from an incoming surfaceTuple
int get_vlmSurface(int numTuple,
                   capsTuple surfaceTuple[],
                   mapAttrToIndexStruct *attrMap,
                   int *numVLMSurface,
                   vlmSurfaceStruct *vlmSurface[]) {

    /*! \page vlmSurface Vortex Lattice Surface
     * Structure for the Vortex Lattice Surface tuple  = ("Name of Surface", "Value").
     * "Name of surface defines the name of the surface in which the data should be applied.
     *  The "Value" can either be a JSON String dictionary (see Section \ref jsonStringVLMSurface)
     *  or a single string keyword string (see Section \ref keyStringVLMSurface).
     */

    int status; //Function return

    int i, groupIndex, attrIndex; // Indexing

    char *keyValue = NULL; // Key values from tuple searches
    char *keyWord = NULL; // Key words to find in the tuples

    int numGroupName = 0;
    char **groupName = NULL;

    int stringLength = 0;

    // Clear out vlmSurface is it has already been allocated
    if (*vlmSurface != NULL) {
        for (i = 0; i < (*numVLMSurface); i++) {
            status = destroy_vlmSurfaceStruct(&(*vlmSurface)[i]);
            if (status != CAPS_SUCCESS) printf("destroy_vlmSurfaceStruct status = %d\n", status);
        }
    }

    printf("Getting vortex lattice surface data\n");

    if (numTuple <= 0){
        printf("\tNumber of VLM Surface tuples is %d\n", numTuple);
        return CAPS_NOTFOUND;
    }

    *numVLMSurface = numTuple;
    *vlmSurface = (vlmSurfaceStruct *) EG_alloc((*numVLMSurface) * sizeof(vlmSurfaceStruct));
    if (*vlmSurface == NULL) {
        *numVLMSurface = 0;
        return EGADS_MALLOC;
    }

    // Initiate vlmSurfaces
    for (i = 0; i < (*numVLMSurface); i++) {
        status = initiate_vlmSurfaceStruct(&(*vlmSurface)[i]);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    for (i = 0; i < (*numVLMSurface); i++) {

        printf("\tVLM surface name - %s\n", surfaceTuple[i].name);

        // Copy surface name
        stringLength = strlen(surfaceTuple[i].name);
        (*vlmSurface)[i].name = (char *) EG_alloc((stringLength + 1)*sizeof(char));
        if ((*vlmSurface)[i].name == NULL) return EGADS_MALLOC;

        memcpy((*vlmSurface)[i].name,
                surfaceTuple[i].name,
                stringLength);
        (*vlmSurface)[i].name[stringLength] = '\0';

        // Do we have a json string?
        if (strncmp(surfaceTuple[i].value, "{", 1) == 0) {

            //printf("JSON String - %s\n",surfaceTuple[i].value);

            /*! \page vlmSurface
             * \section jsonStringVLMSurface JSON String Dictionary
             *
             * If "Value" is a JSON string dictionary (eg. "Value" = {"numChord": 5, "spaceChord": 1.0, "numSpan": 10, "spaceSpan": 0.5})
             * the following keywords ( = default values) may be used:
             *
             * \if (AVL)
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <em>capsGroup</em> names used to define the surface (e.g. "Name1" or ["Name1","Name2",...].
             *  If no groupName variable is provided an attempted will be made to use the tuple name instead;
             * </ul>
             * \else
             * <ul>
             *  <li> <B>groupName = "(no default)"</B> </li> <br>
             *  Single or list of <em>capsGroup</em> names used to define the surface (e.g. "Name1" or ["Name1","Name2",...].
             *  If no groupName variable is provided an attempted will be made to use the tuple name instead;
             * </ul>
             * \endif
             *
             */

            // Get surface variables
            keyWord = "groupName";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toStringDynamicArray(keyValue, &numGroupName, &groupName);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;

                // Determine how many capsGroups go into making the surface
                for (groupIndex = 0; groupIndex < numGroupName; groupIndex++) {

                    status = get_mapAttrToIndexIndex(attrMap, (const char *) groupName[groupIndex], &attrIndex);

                    if (status == CAPS_NOTFOUND) {
                        printf("\tgroupName name %s not found in attribute map of capsGroups!!!!\n", groupName[groupIndex]);
                        continue;

                    } else if (status != CAPS_SUCCESS) goto cleanup;

                    (*vlmSurface)[i].numAttr += 1;
                    if ((*vlmSurface)[i].numAttr == 1) {
                        (*vlmSurface)[i].attrIndex = (int *) EG_alloc(1*sizeof(int));

                    } else{
                        (*vlmSurface)[i].attrIndex = (int *) EG_reall((*vlmSurface)[i].attrIndex,
                                (*vlmSurface)[i].numAttr*sizeof(int));
                    }

                    if ((*vlmSurface)[i].attrIndex == NULL) {
                        status = EGADS_MALLOC;
                        goto cleanup;
                    }

                    (*vlmSurface)[i].attrIndex[(*vlmSurface)[i].numAttr-1] = attrIndex;
                }

                status = string_freeArray(numGroupName, &groupName);
                if (status != CAPS_SUCCESS) goto cleanup;
                groupName = NULL;
                numGroupName = 0;

            } else {
                printf("\tNo \"groupName\" variable provided or no matches found, going to use tuple name\n");
            }

            if ((*vlmSurface)[i].numAttr == 0) {

                status = get_mapAttrToIndexIndex(attrMap, (const char *) (*vlmSurface)[i].name, &attrIndex);
                if (status == CAPS_NOTFOUND) {
                    printf("\tTuple name %s not found in attribute map of capsGroups!!!!\n", (*vlmSurface)[i].name);
                    goto cleanup;
                }

                (*vlmSurface)[i].numAttr += 1;
                if ((*vlmSurface)[i].numAttr == 1) {
                    (*vlmSurface)[i].attrIndex = (int *) EG_alloc(1*sizeof(int));
                } else{
                    (*vlmSurface)[i].attrIndex = (int *) EG_reall((*vlmSurface)[i].attrIndex,
                    (*vlmSurface)[i].numAttr*sizeof(int));
                }

                if ((*vlmSurface)[i].attrIndex == NULL) {
                    status = EGADS_MALLOC;
                    goto cleanup;
                }

                (*vlmSurface)[i].attrIndex[(*vlmSurface)[i].numAttr-1] = attrIndex;
            }

            EG_free(keyValue); keyValue = NULL;

            /*! \page vlmSurface
             * \if (AVL)
             * <ul>
             * <li> <B>noKeyword = "(no default)"</B> </li> <br>
             *      "No" type. Options: NOWAKE, NOALBE, NOLOAD.
             * </ul>
             * \endif
             */

            keyWord = "noKeyword";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                if      (strcasecmp(keyValue, "\"NOWAKE\"") == 0) (*vlmSurface)[i].nowake = (int) true;
                else if (strcasecmp(keyValue, "\"NOALBE\"") == 0) (*vlmSurface)[i].noalbe = (int) true;
                else if (strcasecmp(keyValue, "\"NOLOAD\"") == 0) (*vlmSurface)[i].noload = (int) true;
                else {

                    printf("\tUnrecognized \"%s\" specified (%s) for VLM Section tuple %s, current options are "
                            "\" NOWAKE, NOALBE, or  NOLOAD\"\n", keyWord, keyValue, surfaceTuple[i].name);
                }

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }

            /*! \page vlmSurface
             * <ul>
             * <li> <B>numChord = 10</B> </li> <br>
             *  The number of chordwise horseshoe vortices placed on the surface.
             * </ul>
             *
             */
            keyWord = "numChord";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &(*vlmSurface)[i].Nchord);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }

            /*! \page vlmSurface
             * <ul>
             * <li> <B>spaceChord = 0.0</B> </li> <br>
             *  The chordwise vortex spacing parameter.
             * </ul>
             *
             */
            keyWord = "spaceChord";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*vlmSurface)[i].Cspace);
                if (status != CAPS_SUCCESS) goto cleanup;

                EG_free(keyValue); keyValue = NULL;
            }

            /* Check for lingering numSpan in old scripts */
            keyWord = "numSpan";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {
                EG_free(keyValue); keyValue = NULL;

                printf("************************************************************\n");
                printf("Error: numSpan is depricated.\n");
                printf("       Please use numSpanTotal or numSpanPerSection instead.\n");
                printf("************************************************************\n");
                status = CAPS_BADVALUE;
                goto cleanup;
            }

            /*! \page vlmSurface
             * <ul>
             * <li> <B>numSpanTotal = 0</B> </li> <br>
             *  Total number of spanwise horseshoe vortices placed on the surface.
             *  The vorticies are 'evenly' distributed across sections to minimize jumps in spacings.
             *  numpSpanPerSection must be zero if this is set.
             * </ul>
             *
             */
            keyWord = "numSpanTotal";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &(*vlmSurface)[i].NspanTotal);
                if (status != CAPS_SUCCESS) goto cleanup;

                EG_free(keyValue); keyValue = NULL;
            }

            /*! \page vlmSurface
             * <ul>
             * <li> <B>numSpanPerSection = 0</B> </li> <br>
             *  The number of spanwise horseshoe vortices placed on each section the surface.
             *  The total number of spanwise vorticies are (numSection-1)*numSpanPerSection.
             *  The vorticies are 'evenly' distributed across sections to minimize jumps in spacings.
             *  numSpanTotal must be zero if this is set.
             * </ul>
             *
             */
            keyWord = "numSpanPerSection";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &(*vlmSurface)[i].NspanSection);
                if (status != CAPS_SUCCESS) goto cleanup;

                EG_free(keyValue); keyValue = NULL;
            }

            if ((*vlmSurface)[i].NspanTotal != 0 && (*vlmSurface)[i].NspanSection != 0) {
                printf("Error: Only one of numSpanTotal and numSpanPerSection can be non-zero!\n");
                printf("       numSpanTotal      = %d\n", (*vlmSurface)[i].NspanTotal);
                printf("       numSpanPerSection = %d\n", (*vlmSurface)[i].NspanSection);
                status = CAPS_BADVALUE;
                goto cleanup;
            }

            /*! \page vlmSurface
             * <ul>
             * <li> <B>spaceSpan = 0.0</B> </li> <br>
             *  The spanwise vortex spacing parameter.
             * </ul>
             *
             */
            keyWord = "spaceSpan";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*vlmSurface)[i].Sspace);
                if (status != CAPS_SUCCESS) goto cleanup;

                EG_free(keyValue); keyValue = NULL;
            }

            /*! \page vlmSurface
             * <ul>
             * <li> <B>yMirror = False</B> </li> <br>
             *  Mirror surface about the y-direction.
             * </ul>
             *
             */
            keyWord = "yMirror";
            status = search_jsonDictionary( surfaceTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toBoolean(keyValue, &(*vlmSurface)[i].iYdup);
                if (status != CAPS_SUCCESS) goto cleanup;

                EG_free(keyValue); keyValue = NULL;
            }
        } else {

            /*! \page vlmSurface
             * \section keyStringVLMSurface Single Value String
             *
             * If "Value" is a single string the following options maybe used:
             * - (NONE Currently)
             *
             */
            printf("\tNo current defaults for get_vlmSurface, tuple value must be a JSON string\n");
            status = CAPS_BADVALUE;
            goto cleanup;
        }
    }

    printf("\tDone getting vortex lattice surface data\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in get_vlmSurface, status = %d\n", status);

        EG_free(keyValue); keyValue = NULL;

        if (numGroupName != 0 && groupName != NULL){
            (void) string_freeArray(numGroupName, &groupName);
        }
        return status;
}


// Fill vlmControl in a vlmControlStruct format with vortex lattice information
// from an incoming controlTuple
int get_vlmControl(int numTuple,
                   capsTuple controlTuple[],
                   int *numVLMControl,
                   vlmControlStruct *vlmControl[]) {

    /*! \page vlmControl Vortex Lattice Control Surface
     * Structure for the Vortex Lattice Control Surface tuple  = ("Name of Control Surface", "Value").
     * "Name of control surface defines the name of the control surface in which the data should be applied.
     *  The "Value" must be a JSON String dictionary (see Section \ref jsonStringVLMSection).
     */

    int status; //Function return

    int i; // Indexing

    char *keyValue = NULL; // Key values from tuple searches
    char *keyWord = NULL; // Key words to find in the tuples

    int stringLength = 0;

    // Clear out vlmSurface is it has already been allocated
    if (*vlmControl != NULL) {
        for (i = 0; i < (*numVLMControl); i++) {
            status = destroy_vlmControlStruct(&(*vlmControl)[i]);
            if (status != CAPS_SUCCESS) printf("destroy_vlmControlStruct status = %d\n", status);
        }
    }

    printf("Getting vortex lattice control surface data\n");

    if (numTuple <= 0){
        printf("\tNumber of VLM Surface tuples is %d\n", numTuple);
        return CAPS_NOTFOUND;
    }

    *numVLMControl = numTuple;
    *vlmControl = (vlmControlStruct *) EG_alloc((*numVLMControl) * sizeof(vlmControlStruct));
    if (*vlmControl == NULL) {
        *numVLMControl = 0;
        return EGADS_MALLOC;
    }

    // Initiate vlmSurfaces
    for (i = 0; i < (*numVLMControl); i++) {
        status = initiate_vlmControlStruct(&(*vlmControl)[i]);
        if (status != CAPS_SUCCESS) goto cleanup;
    }

    for (i = 0; i < (*numVLMControl); i++) {

        printf("\tVLM control surface name - %s\n", controlTuple[i].name);

        // Copy surface name
        stringLength = strlen(controlTuple[i].name);
        (*vlmControl)[i].name = (char *) EG_alloc((stringLength + 1)*sizeof(char));
        if ((*vlmControl)[i].name == NULL) return EGADS_MALLOC;

        memcpy((*vlmControl)[i].name,
                controlTuple[i].name,
                stringLength);
        (*vlmControl)[i].name[stringLength] = '\0';

        // Do we have a json string?
        if (strncmp(controlTuple[i].value, "{", 1) == 0) {

            //printf("JSON String - %s\n",surfaceTuple[i].value);

            /*! \page vlmControl
             * \section jsonStringVLMSection JSON String Dictionary
             *
             * If "Value" is a JSON string dictionary (e.g. "Value" = {"deflectionAngle": 10.0}) the following keywords ( = default values) may be used:
             *
             */

            /*! \page vlmControl
             * \if (AVL)
             * <ul>
             * <li> <B>deflectionAngle = 0.0</B> </li> <br>
             *      Deflection angle of the control surface.
             * </ul>
             * \endif
             */
            keyWord = "deflectionAngle";
            status = search_jsonDictionary( controlTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*vlmControl)[i].deflectionAngle);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }

            /*
            ! \page vlmControl
             * \if (AVL)
             * <ul>
             * <li> <B>percentChord = 0.0</B> </li> <br>
             *      Percentage along the airfoil chord the control surface's hinge line resides.
             * </ul>
             * \endif

            keyWord = "percentChord";
            status = search_jsonDictionary( controlTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*vlmControl)[i].percentChord);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }
             */

            /*! \page vlmControl
             * \if (AVL)
             * <ul>
             * <li> <B>leOrTe = (no default) </B> </li> <br>
             *      Is the control surface a leading ( = 0) or trailing (> 0) edge effector? Overrides
             *      the assumed default value set by the geometry: If the percentage along
             *      the airfoil chord is < 50% a leading edge flap is assumed, while >= 50% indicates a
             *      trailing edge flap.
             * </ul>
             * \endif
             */
            keyWord = "leOrTe";
            status = search_jsonDictionary( controlTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                (*vlmControl)[i].leOrTeOverride = (int) true;
                status = string_toInteger(keyValue, &(*vlmControl)[i].leOrTe);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }

            /*! \page vlmControl
             * \if (AVL)
             * <ul>
             * <li> <B>controlGain = 1.0</B> </li> <br>
             *      Control deflection gain, units:  degrees deflection / control variable
             * </ul>
             * \endif
             */
            keyWord = "controlGain";
            status = search_jsonDictionary( controlTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDouble(keyValue, &(*vlmControl)[i].controlGain);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }

            /*! \page vlmControl
             * \if (AVL)
             * <ul>
             * <li> <B>hingeLine = [0.0 0.0 0.0]</B> </li> <br>
             *      Alternative vector giving hinge axis about which surface rotates
             * </ul>
             * \endif
             */
            keyWord = "hingeLine";
            status = search_jsonDictionary( controlTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toDoubleArray(keyValue, 3, (*vlmControl)[i].xyzHingeVec);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }

            /*! \page vlmControl
             * \if (AVL)
             * <ul>
             * <li> <B>deflectionDup = 0 </B> </li> <br>
             *      Sign of deflection for duplicated surface
             * </ul>
             * \endif
             */
            keyWord = "deflectionDup";
            status = search_jsonDictionary( controlTuple[i].value, keyWord, &keyValue);
            if (status == CAPS_SUCCESS) {

                status = string_toInteger(keyValue, &(*vlmControl)[i].deflectionDup);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (keyValue != NULL) EG_free(keyValue);
                keyValue = NULL;
            }



        } else {

            /*! \page vlmControl
             * \section keyStringVLMControl Single Value String
             *
             * If "Value" is a single string, the following options maybe used:
             * - (NONE Currently)
             *
             */
            printf("\tNo current defaults for get_vlmControl, tuple value must be a JSON string\n");
            status = CAPS_BADVALUE;
            goto cleanup;
        }
    }

    printf("\tDone getting vortex lattice control surface data\n");

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in get_vlmControl, status = %d\n", status);

        if (keyValue != NULL) EG_free(keyValue);

        return status;
}


// Initiate (0 out all values and NULL all pointers) of a control in the vlmcontrol structure format
int initiate_vlmControlStruct(vlmControlStruct *control) {

    control->name = NULL; // Control surface name

    control->deflectionAngle = 0.0; // Deflection angle of the control surface
    control->controlGain = 1.0; //Control deflection gain, units:  degrees deflection / control variable

    control->percentChord = 0.0; // Percentage along chord

    control->xyzHinge[0] = 0.0; // xyz location of the hinge
    control->xyzHinge[1] = 0.0;
    control->xyzHinge[2] = 0.0;

    control->xyzHingeVec[0] = 0.0; // Vector of hinge line at xyzHinge
    control->xyzHingeVec[1] = 0.0;
    control->xyzHingeVec[2] = 0.0;


    control->leOrTeOverride = (int) false; // Does the user want to override the geometry set value?
    control->leOrTe = 0; // Leading = 0 or trailing > 0 edge control surface

    control->deflectionDup = 0; // Sign of deflection for duplicated surface

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of a control in the vlmcontrol structure format
int destroy_vlmControlStruct(vlmControlStruct *control) {

    if (control->name != NULL) EG_free(control->name);
    control->name = NULL; // Control surface name

    control->deflectionAngle = 0.0; // Deflection angle of the control surface

    control->controlGain = 1.0; //Control deflection gain, units:  degrees deflection / control variable

    control->percentChord = 0.0; // Percentage along chord

    control->xyzHinge[0] = 0.0; // xyz location of the hinge
    control->xyzHinge[1] = 0.0;
    control->xyzHinge[2] = 0.0;

    control->xyzHingeVec[0] = 0.0; // Vector of hinge line at xyzHinge
    control->xyzHingeVec[1] = 0.0;
    control->xyzHingeVec[2] = 0.0;

    control->leOrTeOverride = (int) false; // Does the user want to override the geometry set value?
    control->leOrTe = 0; // Leading = 0 or trailing > 0 edge control surface

    control->deflectionDup = 0; // Sign of deflection for duplicated surface

    return CAPS_SUCCESS;
}


// Initiate (0 out all values and NULL all pointers) of a section in the vlmSection structure format
int initiate_vlmSectionStruct(vlmSectionStruct *section) {

    section->name = NULL;

    section->bodyIndex = 0;
    section->sectionIndex = 0;

    section->xyzLE[0] = 0.0;
    section->xyzLE[1] = 0.0;
    section->xyzLE[2] = 0.0;
    section->nodeIndexLE = 0;

    section->xyzTE[0] = 0.0;
    section->xyzTE[1] = 0.0;
    section->xyzTE[2] = 0.0;
    section->nodeIndexTE = 0;

    section->chord = 0.0;

    section->Nspan = 0;
    section->Sspace = 0.0;

    section->numControl = 0;
    section->vlmControl = NULL;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of a section in the vlmSection structure format
int destroy_vlmSectionStruct(vlmSectionStruct *section) {

    int status; // Function return status
    int i; // Indexing

    if (section->name != NULL) EG_free(section->name);
    section->name = NULL;

    section->bodyIndex = 0;
    section->sectionIndex = 0;

    section->xyzLE[0] = 0.0;
    section->xyzLE[1] = 0.0;
    section->xyzLE[2] = 0.0;
    section->nodeIndexLE = 0;

    section->xyzTE[0] = 0.0;
    section->xyzTE[1] = 0.0;
    section->xyzTE[2] = 0.0;
    section->nodeIndexTE = 0;

    section->chord = 0.0;

    if (section->vlmControl != NULL) {

        for (i = 0; i < section->numControl; i++) {
            status = destroy_vlmControlStruct(&section->vlmControl[i]);
            if (status != CAPS_SUCCESS) printf("destroy_vlmControlStruct %d\n", status);
        }

        EG_free(section->vlmControl);
    }

    section->vlmControl = NULL;
    section->numControl = 0;

    return CAPS_SUCCESS;
}

// Initiate (0 out all values and NULL all pointers) of a surface in the vlmSurface structure format
int initiate_vlmSurfaceStruct(vlmSurfaceStruct  *surface) {

    // Surface information
    surface->name   = NULL;

    surface->numAttr = 0;    // Number of capsGroup/attributes used to define a given surface
    surface->attrIndex = NULL; // List of attribute map integers that correspond given capsGroups

    surface->Nchord = 10;
    surface->Cspace = 0.0;

    surface->NspanTotal = 0;
    surface->NspanSection = 0;
    surface->Sspace = 0.0;

    surface->nowake = (int) false;
    surface->noalbe = (int) false;;
    surface->noload = (int) false;;
    surface->compon = 0;
    surface->iYdup  = (int) false;

    // Section storage
    surface->numSection = 0;
    surface->vlmSection = NULL;

    return CAPS_SUCCESS;
}

// Destroy (0 out all values and NULL all pointers) of a surface in the vlmSurface structure format
int destroy_vlmSurfaceStruct(vlmSurfaceStruct  *surface) {

    int status; // Function return status
    int i; // Indexing

    // Surface information
    if (surface->name != NULL) EG_free(surface->name);
    surface->name = NULL;

    surface->numAttr = 0;    // Number of capsGroup/attributes used to define a given surface
    if (surface->attrIndex != NULL) EG_free(surface->attrIndex);
    surface->attrIndex = NULL; // List of attribute map integers that correspond given capsGroups


    surface->Nchord = 0;
    surface->Cspace = 0.0;

    surface->NspanTotal = 0;
    surface->NspanSection = 0;
    surface->Sspace = 0.0;

    surface->nowake = (int) false;
    surface->noalbe = (int) false;;
    surface->noload = (int) false;;
    surface->compon = 0;
    surface->iYdup  = (int) false;

    // Section storage
    if (surface->vlmSection != NULL) {
        for (i = 0; i < surface->numSection; i++ ) {
            status = destroy_vlmSectionStruct(&surface->vlmSection[i]);
            if (status != CAPS_SUCCESS) printf("destroy_vlmSectionStruct status = %d", status);
        }

        if (surface->vlmSection != NULL) EG_free(surface->vlmSection);
    }

    surface->vlmSection = NULL;
    surface->numSection = 0;

    return CAPS_SUCCESS;
}

// Populate vlmSurface-section control surfaces from geometry attributes, modify control properties based on
// incoming vlmControl structures
int get_ControlSurface(ego bodies[],
                       int numControl,
                       vlmControlStruct vlmControl[],
                       vlmSurfaceStruct *vlmSurface) {

    int status = 0; // Function status return

    int section, control, attr, index; // Indexing

    int atype, alen; // EGADS return variables
    const int    *ints;
    const double *reals;
    const char *string;

    const char *attributeKey = "vlmControl", *aName = NULL;
    const char *attrName = NULL;

    int numAttr = 0; // Number of attributes

    // Control related variables
    double chordPercent, chordVec[3]; //chordLength

    for (section = 0; section < vlmSurface->numSection; section++) {

        vlmSurface->vlmSection[section].numControl = 0;

        status = EG_attributeNum(bodies[vlmSurface->vlmSection[section].bodyIndex], &numAttr);
        if (status != EGADS_SUCCESS)  return status;

        // Control attributes
        for (attr = 0; attr < numAttr; attr++) {

            status = EG_attributeGet(bodies[vlmSurface->vlmSection[section].bodyIndex],
                                     attr+1,
                                     &aName,
                                     &atype,
                                     &alen,
                                     &ints,
                                     &reals, &string);


            if (status != EGADS_SUCCESS) continue;
            if (atype  != ATTRREAL)      continue;

            if (strncmp(aName, attributeKey, strlen(attributeKey)) != 0) continue;

            if (alen == 0) {
                printf( "Warning: %s should be followed by a single value corresponding to the flap location "
                        "as a function of the chord. 0 - 1 (fraction - %% / 100), 1-100 (%%)\n", aName);
                continue;
            }

            if (reals[0] > 100) {
                printf( "Warning: %s value (%f) must be less than 100\n", aName, reals[0]);
                continue;
            }

            //printf("Attribute name = %s\n", aName);

            if (aName[strlen(attributeKey)] == '\0')

                attrName = "Flap";

            else if (aName[strlen(attributeKey)] == '_') {

                attrName = &aName[strlen(attributeKey)+1];

            } else {

                attrName = &aName[strlen(attributeKey)];
            }

            //printf("AttrName = %s\n", attrName);

            vlmSurface->vlmSection[section].numControl += 1;

            index = vlmSurface->vlmSection[section].numControl-1; // Make copy to shorten the following lines of code

            if (vlmSurface->vlmSection[section].numControl == 1) {
                vlmSurface->vlmSection[section].vlmControl = (vlmControlStruct *) EG_alloc(vlmSurface->vlmSection[section].numControl*sizeof(vlmControlStruct));

            } else {

                vlmSurface->vlmSection[section].vlmControl = (vlmControlStruct *) EG_reall(vlmSurface->vlmSection[section].vlmControl,
                        vlmSurface->vlmSection[section].numControl*sizeof(vlmControlStruct));
            }

            if (vlmSurface->vlmSection[section].vlmControl == NULL) {
              status = EGADS_MALLOC;
              goto cleanup;
            }

            status = initiate_vlmControlStruct(&vlmSurface->vlmSection[section].vlmControl[index]);
            if (status != CAPS_SUCCESS) return status;

            // Get name of control surface
            if (vlmSurface->vlmSection[section].vlmControl[index].name != NULL) EG_free(vlmSurface->vlmSection[section].vlmControl[index].name);

            vlmSurface->vlmSection[section].vlmControl[index].name = EG_strdup(attrName);
            if (vlmSurface->vlmSection[section].vlmControl[index].name == NULL) {
              status = EGADS_MALLOC;
              goto cleanup;
            }

            // Loop through control surfaces from input Tuple and see if defaults have be augmented
            for (control = 0; control < numControl; control++) {

                if (strcasecmp(vlmControl[control].name, attrName) != 0) continue;

                status = copy_vlmControlStruct(&vlmControl[control],
                        &vlmSurface->vlmSection[section].vlmControl[index]);
                if (status != CAPS_SUCCESS) return status;

                break;
            }

            if (control == numControl) {
                printf("Warning: Control %s not found in controls tuple! Only defaults will be used.\n", attrName);
            }

            // Get percent of chord from attribute
            if (reals[0] < 0.0) {
                printf("Warning: Percent chord must < 0, converting to a positive number.\n");
                vlmSurface->vlmSection[section].vlmControl[index].percentChord = -1.0* reals[0];

            } else {

                vlmSurface->vlmSection[section].vlmControl[index].percentChord = reals[0];
            }

            // Was value given as a percentage or fraction
            if (vlmSurface->vlmSection[section].vlmControl[index].percentChord  >= 1.0) {
                vlmSurface->vlmSection[section].vlmControl[index].percentChord  = vlmSurface->vlmSection[section].vlmControl[index].percentChord / 100;
            }

            if (vlmSurface->vlmSection[section].vlmControl[index].leOrTeOverride == (int) false) {

                if (vlmSurface->vlmSection[section].vlmControl[index].percentChord  < 0.5) {
                    vlmSurface->vlmSection[section].vlmControl[index].leOrTe = 0;
                } else {
                    vlmSurface->vlmSection[section].vlmControl[index].leOrTe = 1;
                }
            }

            // Get xyz of hinge location
            chordVec[0] = vlmSurface->vlmSection[section].xyzTE[0] - vlmSurface->vlmSection[section].xyzLE[0];
            chordVec[1] = vlmSurface->vlmSection[section].xyzTE[1] - vlmSurface->vlmSection[section].xyzLE[1];
            chordVec[2] = vlmSurface->vlmSection[section].xyzTE[2] - vlmSurface->vlmSection[section].xyzLE[2];

            chordPercent = vlmSurface->vlmSection[section].vlmControl[index].percentChord;

            vlmSurface->vlmSection[section].vlmControl[index].xyzHinge[0] = chordPercent*chordVec[0] +
                                                                            vlmSurface->vlmSection[section].xyzLE[0];

            vlmSurface->vlmSection[section].vlmControl[index].xyzHinge[1] = chordPercent*chordVec[1] +
                                                                            vlmSurface->vlmSection[section].xyzLE[1];

            vlmSurface->vlmSection[section].vlmControl[index].xyzHinge[2] = chordPercent*chordVec[2] +
                                                                            vlmSurface->vlmSection[section].xyzLE[2];

            /*
            printf("\nCheck hinge line \n");

            printf("ChordLength = %f\n", chordLength);
            printf("ChordPercent = %f\n", chordPercent);
            printf("LeadingEdge = %f %f %f\n", vlmSurface->vlmSection[section].xyzLE[0],
                                               vlmSurface->vlmSection[section].xyzLE[1],
                                               vlmSurface->vlmSection[section].xyzLE[2]);

            printf("TrailingEdge = %f %f %f\n", vlmSurface->vlmSection[section].xyzTE[0],
                                                vlmSurface->vlmSection[section].xyzTE[1],
                                                vlmSurface->vlmSection[section].xyzTE[2]);

            printf("Hinge location = %f %f %f\n", vlmSurface->vlmSection[section].vlmControl[index].xyzHinge[0],
                                                  vlmSurface->vlmSection[section].vlmControl[index].xyzHinge[1],
                                                  vlmSurface->vlmSection[section].vlmControl[index].xyzHinge[2]);
             */

            /*// Set the hinge vector to the plane normal - TOBE ADDED
            vlmSurface->vlmSection[section].vlmControl[index].xyzHingeVec[0] =
            vlmSurface->vlmSection[section].vlmControl[index].xyzHingeVec[1] =
            vlmSurface->vlmSection[section].vlmControl[index].xyzHingeVec[2] =
             */
        }
    }

cleanup:

    return status;
}


// Make a copy of vlmControlStruct (it is assumed controlOut has already been initialized)
int copy_vlmControlStruct(vlmControlStruct *controlIn, vlmControlStruct *controlOut) {

    int stringLength = 0;

    if (controlIn == NULL) return CAPS_NULLVALUE;
    if (controlOut == NULL) return CAPS_NULLVALUE;

    if (controlIn->name != NULL) {
        stringLength = strlen(controlIn->name);

        if (controlOut->name != NULL) EG_free(controlOut->name);

        controlOut->name = (char *) EG_alloc((stringLength+1)*sizeof(char));
        if (controlOut->name == NULL) return EGADS_MALLOC;

        memcpy(controlOut->name,
                controlIn->name,
                stringLength*sizeof(char));

        controlOut->name[stringLength] = '\0';
    }

    controlOut->deflectionAngle = controlIn->deflectionAngle; // Deflection angle of the control surface
    controlOut->controlGain = controlIn->controlGain; //Control deflection gain, units:  degrees deflection / control variable


    controlOut->percentChord = controlIn->percentChord; // Percentage along chord

    controlOut->xyzHinge[0] = controlIn->xyzHinge[0]; // xyz location of the hinge
    controlOut->xyzHinge[1] = controlIn->xyzHinge[1];
    controlOut->xyzHinge[2] = controlIn->xyzHinge[2];

    controlOut->xyzHingeVec[0] = controlIn->xyzHingeVec[0]; // xyz location of the hinge
    controlOut->xyzHingeVec[1] = controlIn->xyzHingeVec[1];
    controlOut->xyzHingeVec[2] = controlIn->xyzHingeVec[2];


    controlOut->leOrTeOverride = controlIn->leOrTeOverride; // Does the user want to override the geometry set value?
    controlOut->leOrTe = controlIn->leOrTe; // Leading = 0 or trailing > 0 edge control surface

    controlOut->deflectionDup = controlIn->deflectionDup; // Sign of deflection for duplicated surface

    return CAPS_SUCCESS;
}

// Make a copy of vlmSectionStruct (it is assumed sectionOut has already been initialized)
int copy_vlmSectionStruct(vlmSectionStruct *sectionIn, vlmSectionStruct *sectionOut) {

    int status; // Function return status

    int i; // Indexing

    int stringLength = 0;

    if (sectionIn == NULL) return CAPS_NULLVALUE;
    if (sectionOut == NULL) return CAPS_NULLVALUE;

    if (sectionIn->name != NULL) {
        stringLength = strlen(sectionIn->name);

        if (sectionOut->name != NULL) EG_free(sectionOut->name);

        sectionOut->name = (char *) EG_alloc((stringLength+1)*sizeof(char));
        if (sectionOut->name == NULL) return EGADS_MALLOC;

        memcpy(sectionOut->name,
                sectionIn->name,
                stringLength*sizeof(char));

        sectionOut->name[stringLength] = '\0';
    }

    sectionOut->bodyIndex = sectionIn->bodyIndex;  // Body index - 0 bias
    sectionOut->sectionIndex = sectionIn->sectionIndex; // Section index - 0 bias


    sectionOut->xyzLE[0] = sectionIn->xyzLE[0]; // xyz coordinates for leading edge
    sectionOut->xyzLE[1] = sectionIn->xyzLE[1];
    sectionOut->xyzLE[2] = sectionIn->xyzLE[2];

    sectionOut->nodeIndexLE = sectionIn->nodeIndexLE; // Leading edge node index with reference to xyzLE

    sectionOut->xyzTE[0] = sectionIn->xyzTE[0]; // xyz location of the hinge
    sectionOut->xyzTE[1] = sectionIn->xyzTE[1];
    sectionOut->xyzTE[2] = sectionIn->xyzTE[2];

    sectionOut->nodeIndexTE = sectionIn->nodeIndexTE; // Trailing edge node index with reference to xyzTE

    sectionOut->chord = sectionIn->chord; // Chord

    sectionOut->Nspan = sectionIn->Nspan;   // number of spanwise vortices (elements)
    sectionOut->Sspace = sectionIn->Sspace; // spanwise point distribution

    sectionOut->numControl = sectionIn->numControl;

    if (sectionOut->numControl != 0) {

        sectionOut->vlmControl = (vlmControlStruct *) EG_alloc(sectionOut->numControl*sizeof(vlmControlStruct));
        if (sectionOut->vlmControl == NULL) return EGADS_MALLOC;

        for (i = 0; i < sectionOut->numControl; i++) {

            status = initiate_vlmControlStruct(&sectionOut->vlmControl[i]);
            if (status != CAPS_SUCCESS) return status;

            status = copy_vlmControlStruct(&sectionIn->vlmControl[i], &sectionOut->vlmControl[i]);
            if (status != CAPS_SUCCESS) return status;
        }
    }

    return CAPS_SUCCESS;
}

// Make a copy of vlmSurfaceStruct (it is assumed surfaceOut has already been initialized)
// Also the section in vlmSurface are reordered based on a vlm_orderSections() function call
int copy_vlmSurfaceStruct(vlmSurfaceStruct *surfaceIn, vlmSurfaceStruct *surfaceOut) {

    int status; // Function return status

    int i, sectionIndex; // Indexing

    int stringLength;

    if (surfaceIn == NULL) return CAPS_NULLVALUE;
    if (surfaceOut == NULL) return CAPS_NULLVALUE;


    if (surfaceIn->name != NULL) {
        stringLength = strlen(surfaceIn->name);

        if (surfaceOut->name != NULL) EG_free(surfaceOut->name);

        surfaceOut->name = (char *) EG_alloc((stringLength+1)*sizeof(char));
        if (surfaceOut->name == NULL) return EGADS_MALLOC;

        memcpy(surfaceOut->name,
                surfaceIn->name,
                stringLength*sizeof(char));

        surfaceOut->name[stringLength] = '\0';
    }

    surfaceOut->numAttr = surfaceIn->numAttr;

    if (surfaceIn->attrIndex != NULL) {

        surfaceOut->attrIndex = (int *) EG_alloc(surfaceOut->numAttr *sizeof(int));
        if (surfaceOut->attrIndex == NULL) return EGADS_MALLOC;

        memcpy(surfaceOut->attrIndex,
                surfaceIn->attrIndex,
                surfaceOut->numAttr*sizeof(int));
    }

    surfaceOut->Nchord = surfaceIn->Nchord;
    surfaceOut->Cspace = surfaceIn->Cspace;

    surfaceOut->NspanTotal   = surfaceIn->NspanTotal;
    surfaceOut->NspanSection = surfaceIn->NspanSection;
    surfaceOut->Sspace       = surfaceIn->Sspace;

    surfaceOut->nowake = surfaceIn->nowake;
    surfaceOut->noalbe = surfaceIn->noalbe;
    surfaceOut->noload = surfaceIn->noload;

    surfaceOut->compon = surfaceIn->compon;
    surfaceOut->iYdup = surfaceIn->iYdup;

    surfaceOut->numSection = surfaceIn->numSection;


    if (surfaceIn->vlmSection != NULL) {

        surfaceOut->vlmSection = (vlmSectionStruct *) EG_alloc(surfaceOut->numSection*sizeof(vlmSectionStruct));

        if (surfaceOut->vlmSection == NULL) return EGADS_MALLOC;

        status = vlm_orderSections(surfaceIn->numSection, surfaceIn->vlmSection);
        if (status != CAPS_SUCCESS) return status;

        for (i = 0; i < surfaceOut->numSection; i++) {

            status = initiate_vlmSectionStruct(&surfaceOut->vlmSection[i]);
            if (status != CAPS_SUCCESS) return status;

            // Sections aren't necessarily stored in order coming out of vlm_GetSection, however sectionIndex is (after a
            // call to vlm_orderSection()) !

            sectionIndex = surfaceIn->vlmSection[i].sectionIndex;

            status = copy_vlmSectionStruct(&surfaceIn->vlmSection[sectionIndex], &surfaceOut->vlmSection[i]);
            if (status != CAPS_SUCCESS) return status;

            // Reset the sectionIndex that is keeping track of the section order.
            surfaceOut->vlmSection[i].sectionIndex = i;
        }
    }

    return CAPS_SUCCESS;
}


static int findLeadingEdge(ego body, int *nodeIndexLE, double *xyzLE )
{
    int status; // Function return
    int  i; // Indexing

    // Node variables
    ego    *nodes = NULL;
    int    numNode;
    double xmin=1E6, xyz[3]={0.0, 0.0, 0.0};

    //EGADS returns (unused)
    int oclass, mtype, *sens = NULL, numChildren;
    ego ref, *children = NULL;

    *nodeIndexLE = 0;

    // Assume the LE position is the most forward Node in X
    status = EG_getBodyTopos(body, NULL, NODE, &numNode, &nodes);
    if (status != EGADS_SUCCESS) goto cleanup;


    for (i = 0; i < numNode; i++) {
        status = EG_getTopology(nodes[i], &ref, &oclass, &mtype, xyz,
                                &numChildren, &children, &sens);
        if (status != EGADS_SUCCESS) goto cleanup;

        if (*nodeIndexLE == 0) {
            *nodeIndexLE = i+1;
            xmin = xyz[0];
        } else {

            if (xyz[0] < xmin) {
                *nodeIndexLE = i+1;
                xmin = xyz[0];
            }
        }
    }

    if (*nodeIndexLE == 0) {
        printf(" findLeadingEdge: Body has no LE!\n");
        status = CAPS_NOTFOUND;
        goto cleanup;
    }

    status = EG_getTopology(nodes[*nodeIndexLE-1], &ref, &oclass, &mtype, xyzLE, &numChildren, &children, &sens);

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error in findLeadingEdge - status %d\n", status);
        if (nodes != NULL) EG_free(nodes);

        return status;
}

static int findTrailingEdge(ego body, int *nodeIndexTE, double *xyzTE )
{
    int status; // Function return
    int  i; // Indexing

    // Node variables
    ego    *nodes = NULL;
    int    numNode;
    double xmax=-1E6, xyz[3]={0.0, 0.0, 0.0};

    //EGADS returns (unused)
    int oclass, mtype, *sens = NULL, numChildren;
    ego ref, *children = NULL;

    *nodeIndexTE = 0;

    // Assume the TE position is the most rear Node in X
    status = EG_getBodyTopos(body, NULL, NODE, &numNode, &nodes);
    if (status != EGADS_SUCCESS) goto cleanup;


    for (i = 0; i < numNode; i++) {
        status = EG_getTopology(nodes[i], &ref, &oclass, &mtype, xyz,
                &numChildren, &children, &sens);
        if (status != EGADS_SUCCESS) goto cleanup;

        if (*nodeIndexTE == 0) {
            *nodeIndexTE = i+1;
            xmax = xyz[0];
        } else {

            if (xyz[0] > xmax) {
                *nodeIndexTE = i+1;
                xmax = xyz[0];
            }
        }
    }

    if (*nodeIndexTE == 0) {
        printf(" findTrailingEdge: Body has no TE!\n");
        status = CAPS_NOTFOUND;
        goto cleanup;
    }
    status = EG_getTopology(nodes[*nodeIndexTE-1], &ref, &oclass, &mtype, xyzTE, &numChildren, &children, &sens);

    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error in findTrailingEdge - status %d\n", status);
        if (nodes != NULL) EG_free(nodes);

        return status;
}

static int determineSectionPlane(int numEdge, ego *edges, int *swapYZ) {

    int status;
    int i, j;

    double  t, trange[2], result[9];

    //EGADS returns
    int oclass, mtype, *sens = NULL, numChildren;
    ego ref, *children = NULL;

    int numPoint = 0;
    double *points =NULL;

    int xMeshConstant = (int) true;
    int yMeshConstant = (int) true;
    int zMeshConstant = (int) true;

    points = (double *) EG_alloc(numEdge*3*11*sizeof(double));
    if (points == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    // Loop through edges
    for (i = 0; i < numEdge; i++) {

        // Get t- range for edge
        status = EG_getTopology(edges[i], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
        if (mtype == DEGENERATE) continue;
        if (status != EGADS_SUCCESS) goto cleanup;

        // Write out in points along edge
         for (j = 1; j < 10; j++) {
              t = trange[0] + j*(trange[1]-trange[0])/10;

              status = EG_evaluate(edges[i], &t, result);
              if (status != EGADS_SUCCESS) goto cleanup;

              points[3*numPoint + 0] = result[0];
              points[3*numPoint + 1] = result[1];
              points[3*numPoint + 2] = result[2];

              numPoint += 1;
         }
    }

    // See what plane our body is on

    // Constant x?
    for (i = 0; i < numPoint; i++) {
      if (fabs(points[3*i + 0] - points[3*0 + 0]) > 1E-7) {
          xMeshConstant = (int) false;
          break;
      }
    }

    // Constant y?
    for (i = 0; i < numPoint; i++) {
      if (fabs(points[3*i + 1] - points[3*0 + 1] ) > 1E-7) {
          yMeshConstant = (int) false;
          break;
      }
    }

    // Constant z?
    for (i = 0; i < numPoint; i++) {
      if (fabs(points[3*i + 2]-points[3*0 + 2]) > 1E-7) {
          zMeshConstant = (int) false;
          break;
      }
    }

    if (xMeshConstant == (int) true) {
        printf("Error: Airfoils sections can NOT be the x-plane!\n");
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    if (yMeshConstant == (int) true && zMeshConstant == (int) false) {
        *swapYZ = (int) false;

    } else if (yMeshConstant == (int) false && zMeshConstant == (int) true) {
        *swapYZ = (int) true;

    } else {
        printf("Error: Unable to determine Airfoil sections plane! Section is neither purely in the y- or the z-plane.\n");
        status = CAPS_BADVALUE;
        goto cleanup;
    }

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in determineSectionPlane, status = %d\n", status);

        if (points != NULL) EG_free(points);
        return status;

}

// Find the chord of an body and inclination angle
int vlm_getChordandAinc(ego body, double *chord, double *ainc) {

    int status; // Function return status

    double xdot[3];

    int numNode, numEdge;
    ego *nodes = NULL, *edges = NULL;

    double xyzLE[3], xyz[3], xmin, trange[2], result[9];

    //EGADS returns
    int oclass, mtype, *sens = NULL, numChildren;
    ego ref, *children = NULL;

    int nodeIndexLE, nodeIndexTE;

    ego teObj = NULL;

    double PI =3.1415926535897931159979635;

    int swapYZ = (int) false;

    *chord = 0;
    *ainc = 0;

    status = vlm_findTEObj(body, &teObj);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = findLeadingEdge(body, &nodeIndexLE, xyzLE);
    if (status != EGADS_SUCCESS) {
        printf("\tError in findLeadingEdge, getBodyTopos Nodes = %d\n", status);
        goto cleanup;
    }
    status = findTrailingEdge(body, &nodeIndexTE, xyz);
    if (status != EGADS_SUCCESS) {
        printf("\tError in findTrailingEdge, getBodyTopos Nodes = %d\n", status);
        goto cleanup;
    }

    status = EG_getBodyTopos(body, NULL, NODE, &numNode, &nodes);
    if (status != EGADS_SUCCESS) {
        printf("\tError in vlm_getChordandAinc, getBodyTopos Edges = %d\n", status);
        goto cleanup;
    }

    status = EG_getBodyTopos(body, NULL, EDGE, &numEdge, &edges);
    if (status != EGADS_SUCCESS) {
        printf("\tError in vlm_getChordandAinc, getBodyTopos Nodes = %d\n", status);
        goto cleanup;
    }

    // Determine plane
    status = determineSectionPlane(numEdge, edges, &swapYZ);
    if (status != CAPS_SUCCESS) goto cleanup;

    if (teObj == NULL) {

        status = EG_getTopology(nodes[nodeIndexTE-1], &ref, &oclass, &mtype, result, &numChildren, &children, &sens);
        if (status != EGADS_SUCCESS) goto cleanup;

    } else {

        status = EG_getTopology(teObj, &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
        if (status != EGADS_SUCCESS) goto cleanup;

        xmin = 0.5*(trange[0]+trange[1]);

        status = EG_evaluate(teObj, &xmin, result);
        if (status != EGADS_SUCCESS) {
            printf("\tError in vlm_getChordandAinc, evaluate = %d!\n", status);
            goto cleanup;
        }
    }

    xdot[0]  =  result[0] - xyzLE[0];
    xdot[1]  =  result[1] - xyzLE[1];
    xdot[2]  =  result[2] - xyzLE[2];

    *chord    =  sqrt(xdot[0]*xdot[0] + xdot[1]*xdot[1] + xdot[2]*xdot[2]);

    xdot[0] /=  *chord;
    xdot[1] /=  *chord;
    xdot[2] /=  *chord;

    if (swapYZ == (int) true ) {
        *ainc = -atan2(xdot[1], xdot[0])*180./PI;
    } else {
        *ainc = -atan2(xdot[2], xdot[0])*180./PI;
    }

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in vlm_getChordandAinc, status = %d\n", status);

        if (nodes != NULL) EG_free(nodes);
        if (edges != NULL) EG_free(edges);
        return status;

}

// Find the EGO object pertaining the to trailing edge
int vlm_findTEObj(ego body, ego *teObj) {

    int status; // Function return status

    int i; // Indexing

    int numNode, numEdge;
    ego *nodes = NULL, *edges = NULL;

    double xyz[3], xmin, trange[2];

    //EGADS returns
    int oclass, mtype, *sens = NULL, numChildren;
    ego ref, *children = NULL, *temp = NULL;

    int nodeIndexLE, nodeIndexTE;

    *teObj = NULL;

    status = findLeadingEdge(body, &nodeIndexLE, xyz);
    if (status != EGADS_SUCCESS) {
        printf("\tError in findLeadingEdge, getBodyTopos Nodes = %d\n", status);
        goto cleanup;
    }

    status = findTrailingEdge(body, &nodeIndexTE, xyz);
    if (status != EGADS_SUCCESS) {
        printf("\tError in findTrailingEdge, getBodyTopos Nodes = %d\n", status);
        goto cleanup;
    }

    status = EG_getBodyTopos(body, NULL, NODE, &numNode, &nodes);
    if (status != EGADS_SUCCESS) {
        printf("\tError in vlm_findTEObj, getBodyTopos Nodes = %d\n", status);
        goto cleanup;
    }

    status = EG_getBodyTopos(body, NULL, EDGE, &numEdge, &edges);
    if (status != EGADS_SUCCESS) {
        printf("\tError in vlm_findTEObj, getBodyTopos Edges = %d\n", status);
        goto cleanup;
    }

    for (i = 0; i < numEdge; i++) {

        status = EG_getTopology(edges[i], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);

        if (mtype == DEGENERATE) continue;

        if (status != EGADS_SUCCESS) {
            printf("\tError in vlm_findTEObj, Edge %d getTopology = %d!\n", i, status);
            goto cleanup;
        }

        if (numChildren != 2) {
            printf("\tError in vlm_findTEObj, Edge %d has %d nodes!\n", i, numChildren);
            status = CAPS_BADVALUE;
            goto cleanup;
        }

        if (numEdge <= 3) {
            if ( (children[0] != nodes[nodeIndexLE-1]) &&
                 (children[1] != nodes[nodeIndexLE-1])) {

                *teObj = edges[i];
                // printf(" TE Edge = %d\n", i);
                break;
            }

        } else {

            // If the edge doesn't at least contain the TE node pass it by
            if (children[0] != nodes[nodeIndexTE-1] && children[1] != nodes[nodeIndexTE-1]) continue;

            // Lets compare the coordinates
            status = EG_getTopology(children[0], &ref, &oclass, &mtype, xyz, &numChildren, &temp, &sens);
            if (status != EGADS_SUCCESS) goto cleanup;

            xmin = xyz[0];

            status = EG_getTopology(children[1], &ref, &oclass, &mtype, xyz, &numChildren, &temp, &sens);
            if (status != EGADS_SUCCESS) goto cleanup;

            if (fabs(xyz[0] - xmin) < 10E-4) { // If we are at the same x-location

                *teObj = edges[i];
                break;
            }
        }
    }

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in vlm_findTEObj, status = %d\n", status);

        if (nodes != NULL) EG_free(nodes);
        if (edges != NULL) EG_free(edges);

        return status;

}


// Accumulate VLM section data from a set of bodies. If disciplineFilter is not NULL
// bodies not found with disciplineFilter (case insensitive) for a capsDiscipline attribute
// will be ignored.
int vlm_getSection(int numBody,
                   ego bodies[],
                   const char *disciplineFilter,
                   mapAttrToIndexStruct attrMap,
                   int numSurface,
                   vlmSurfaceStruct *vlmSurface[]) {

    int status; // Function return status

    int i, body, surf, section; // Indexing
    int attrIndex;
    int Nspan;
    double Sspace;

    int found = (int) false; // Boolean tester

    const char *groupName = NULL;
    const char *discipline = NULL;

    double angle;

    // Loop through bodies
    for (body = 0; body < numBody; body++) {

        if (disciplineFilter != NULL) {

            status = retrieve_CAPSDisciplineAttr(bodies[body], &discipline);
            if (status == CAPS_SUCCESS) {
                //printf("capsDiscipline = %s, Body, %i\n", discipline, body);
                if (strcasecmp(discipline, disciplineFilter) != 0) continue;
            }
        }

        // Loop through surfaces
        for (surf = 0; surf < numSurface; surf++) {

            status = retrieve_CAPSGroupAttr(bodies[body], &groupName);
            if (status != CAPS_SUCCESS) {
                printf("vlm_getSection: No capsGroup value found on body %d, body will not be used\n", body+1);
                continue;
            }

            status = get_mapAttrToIndexIndex(&attrMap, groupName, &attrIndex);
            if (status == CAPS_NOTFOUND) {
                printf("VLM Surface name \"%s\" not found in attrMap\n", groupName);
                goto cleanup;
            }

            found = (int) false;

            // See if attrIndex is in the attrIndex array for the surface
            for (i = 0; i < (*vlmSurface)[surf].numAttr; i++) {

                if (attrIndex == (*vlmSurface)[surf].attrIndex[i]) {
                    found = (int) true;
                    break;
                }
            }

            // If attrIndex isn't in the array the; body doesn't belong in this surface
            if (found == (int) false) continue;

            // Increment the number of sections
            (*vlmSurface)[surf].numSection += 1;

            // Get section index
            section = (*vlmSurface)[surf].numSection -1;

            if ((*vlmSurface)[surf].numSection == 1) {
                (*vlmSurface)[surf].vlmSection = (vlmSectionStruct *) EG_alloc(1*sizeof(vlmSectionStruct));
            } else {

                (*vlmSurface)[surf].vlmSection = (vlmSectionStruct *) EG_reall((*vlmSurface)[surf].vlmSection,
                                                                               (*vlmSurface)[surf].numSection*
                                                                               sizeof(vlmSectionStruct));
            }
            if ((*vlmSurface)[surf].vlmSection == NULL) {
                status = EGADS_MALLOC;
                goto cleanup;
            }

            status = initiate_vlmSectionStruct(&(*vlmSurface)[surf].vlmSection[section]);
            if (status != CAPS_SUCCESS) goto cleanup;

            // Store the section index
            (*vlmSurface)[surf].vlmSection[section].sectionIndex = section;

            // Store the body index that the section is coming from
            (*vlmSurface)[surf].vlmSection[section].bodyIndex = body;

            // get the specified number of span points from the body
            Nspan = 0;
            status = retrieve_intAttrOptional(bodies[body], "vlmNumSpan", &Nspan);
            if (status == EGADS_ATTRERR) goto cleanup;
            (*vlmSurface)[surf].vlmSection[section].Nspan = Nspan;

            // get the specified span points distribution from the body
            Sspace = (*vlmSurface)[surf].Sspace;
            status = retrieve_doubleAttrOptional(bodies[body], "vlmSspace", &Sspace);
            if (status == EGADS_ATTRERR) goto cleanup;
            (*vlmSurface)[surf].vlmSection[section].Sspace = Sspace;

            // Get leading edge coordinates
            status = findLeadingEdge(bodies[body],
                                     &(*vlmSurface)[surf].vlmSection[section].nodeIndexLE,
                                      (*vlmSurface)[surf].vlmSection[section].xyzLE);
            if (status != CAPS_SUCCESS) {

                if ((*vlmSurface)[surf].vlmSection[section].nodeIndexLE == 0) {
                    printf(" vlm_getSection: Body %d has no LE!\n", body +1);
                    status = CAPS_NOTFOUND;
                }

                goto cleanup;
            }

            // Get trailing edge coordinates
            status = findTrailingEdge(bodies[body],
                                      &(*vlmSurface)[surf].vlmSection[section].nodeIndexTE,
                                       (*vlmSurface)[surf].vlmSection[section].xyzTE);
            if (status != CAPS_SUCCESS) {

                if ((*vlmSurface)[surf].vlmSection[section].nodeIndexTE == 0) {
                    printf(" vlm_getSection: Body %d has no TE!\n", body +1);
                    status = CAPS_NOTFOUND;
                }
                goto cleanup;
            }

            // Get chord
            status = vlm_getChordandAinc(bodies[body],
                                          &(*vlmSurface)[surf].vlmSection[section].chord,
                                          &angle);
            if (status != CAPS_SUCCESS) {
                goto cleanup;
            }
        }
    }

    status = CAPS_SUCCESS;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Status %d during vlm_GetSection", status);
        return status;
}

// Order VLM sections increasing order
int vlm_orderSections(int numSection, vlmSectionStruct vlmSection[])
{
    int    i1, i2, j, k, hit;
    double box[6], lScale;

    if (numSection <= 0) {
        printf("In valid number of sections - %d!\n", numSection);
        return CAPS_BADVALUE;
    }

    j = vlmSection[0].sectionIndex;

    box[0] = box[3] = vlmSection[j].xyzLE[0];
    box[1] = box[4] = vlmSection[j].xyzLE[1];
    box[2] = box[5] = vlmSection[j].xyzLE[2];

    for (k = 0; k < numSection; k++) {
        j = vlmSection[k].sectionIndex;
        if (vlmSection[j].xyzLE[0] < box[0]) box[0] = vlmSection[j].xyzLE[0];
        if (vlmSection[j].xyzLE[0] > box[3]) box[3] = vlmSection[j].xyzLE[0];
        if (vlmSection[j].xyzLE[1] < box[1]) box[1] = vlmSection[j].xyzLE[1];
        if (vlmSection[j].xyzLE[1] > box[4]) box[4] = vlmSection[j].xyzLE[1];
        if (vlmSection[j].xyzLE[2] < box[2]) box[2] = vlmSection[j].xyzLE[2];
        if (vlmSection[j].xyzLE[2] > box[5]) box[5] = vlmSection[j].xyzLE[2];
    }

    lScale = box[3] - box[0];
    if (box[4] - box[1] > lScale) lScale = box[4] - box[1];
    if (box[5] - box[2] > lScale) lScale = box[5] - box[2];

    if ((box[4]-box[1])/lScale > 1.e-5) {
        /* Y ordering */
        do {
            for (hit = k = 0; k < numSection-1; k++) {
                i1 = vlmSection[k  ].sectionIndex;
                i2 = vlmSection[k+1].sectionIndex;

                if (vlmSection[i1].xyzLE[1] <= vlmSection[i2].xyzLE[1]) continue;

                vlmSection[k  ].sectionIndex = i2;
                vlmSection[k+1].sectionIndex = i1;
                hit++;
            }
        } while (hit != 0);

    } else if ((box[5]-box[2])/lScale > 1.e-5) {
        /* assume Vertical Tail and do Z ordering */
        do {
            for (hit = k = 0; k < numSection-1; k++) {
                i1 = vlmSection[k  ].sectionIndex;
                i2 = vlmSection[k+1].sectionIndex;

                if (vlmSection[i1].xyzLE[2] <= vlmSection[i2].xyzLE[2]) continue;

                vlmSection[k  ].sectionIndex = i2;
                vlmSection[k+1].sectionIndex = i1;
                hit++;
            }
        } while (hit != 0);

    } else {
        printf(" AIM Warning: No Y or Z Variation in Surface LE Bounding Box!\n");
        printf("              %lf %lf   %lf %lf   %lf %lf  - lScale = %lf\n", box[0],
                                                                              box[3],
                                                                              box[1],
                                                                              box[4],
                                                                              box[2],
                                                                              box[5],
                                                                              lScale);
    }


    return CAPS_SUCCESS;
    /*
     printf(" Sections =");
     for (j = 0; j < surfaces[i].nsec; j++) printf(" %d", surfaces[j].sectionIndex);
     printf("\n");
     */
}


// Compute spanwise panel spacing with close to equal spacing on each panel
int vlm_equalSpaceSpanPanels(int NspanTotal, int numSection, vlmSectionStruct vlmSection[])
{
    int status = CAPS_SUCCESS;
    int    i, j, sectionIndex1, sectionIndex2;
    double distLE, distLETotal = 0, *b = NULL;
    int Nspan;
    int NspanMax, imax;
    int NspanMin, imin;

    int numSeg = numSection-1;

    // special case for just one segment (2 sections)
    if (numSeg == 1) {
        sectionIndex1 = vlmSection[0].sectionIndex;

        // use any specified counts
        if (vlmSection[sectionIndex1].Nspan >= 2)
          return CAPS_SUCCESS;

        // just set the total
        vlmSection[sectionIndex1].Nspan = NspanTotal;
        return CAPS_SUCCESS;
    }

    // length of each span section
    b = (double*) EG_alloc(numSeg*sizeof(double));

    // go over all but the last section
    for (i = 0; i < numSection-1; i++) {

        // get the section indices
        sectionIndex1 = vlmSection[i  ].sectionIndex;
        sectionIndex2 = vlmSection[i+1].sectionIndex;

        // skip sections explicitly specified
        if (vlmSection[sectionIndex1].Nspan > 1) continue;

        // use the y-z distance between leading edge points to scale the number of spanwise points
        distLE = 0;
        for (j = 1; j < 3; j++) {
            distLE += pow(vlmSection[sectionIndex2].xyzLE[j] - vlmSection[sectionIndex1].xyzLE[j], 2);
        }
        distLE = sqrt(distLE);

        b[i] = distLE;
        distLETotal += distLE;
    }

    // set the number of spanwise points
    for (i = 0; i < numSection-1; i++) {

        // get the section indices
        sectionIndex1 = vlmSection[i].sectionIndex;

        // skip sections explicitly specified
        if (vlmSection[sectionIndex1].Nspan > 1) continue;

        b[i] /= distLETotal;
        Nspan = NINT(b[i]*abs(NspanTotal));

        vlmSection[sectionIndex1].Nspan = Nspan > 1 ? Nspan : 1;
    }

    // make sure the total adds up
    do {

        Nspan = 0;
        NspanMax = 0;
        NspanMin = NspanTotal;
        imax = 0;
        imin = 0;
        for (i = 0; i < numSection-1; i++) {
            // get the section indices
            sectionIndex1 = vlmSection[i].sectionIndex;

            if ( vlmSection[sectionIndex1].Nspan > NspanMax ) {
                NspanMax = vlmSection[sectionIndex1].Nspan;
                imax = sectionIndex1;
            }
            if ( vlmSection[sectionIndex1].Nspan < NspanMin ) {
                NspanMin = vlmSection[sectionIndex1].Nspan;
                imin = sectionIndex1;
            }

            Nspan += vlmSection[sectionIndex1].Nspan;
        }

        if (Nspan > NspanTotal) {
            vlmSection[imax].Nspan--;

            if (vlmSection[imax].Nspan == 0) {
                printf("Error: Insufficient spanwise sections! Increase numSpanTotal or numSpanPerSection!\n");
                status = CAPS_BADVALUE;
                goto cleanup;
            }
        }
        if (Nspan < NspanTotal) {
            vlmSection[imin].Nspan++;
        }

    } while (Nspan != NspanTotal);

    status = CAPS_SUCCESS;
cleanup:
    EG_free(b); b = NULL;

    return status;
}


// Write out the airfoil cross-section given an ego body
int vlm_writeSection(FILE *fp,
                     ego body,
                     int normalize, // Normalize by chord (true/false)
                     int maxNumPoint) // Max number of points in airfoil
{
    int status; // Function return status

    int i, j; // Indexing

    int edgeIndex, *edgeOrder=NULL, *edgeLoopOrder=NULL;

    int *numPoint = NULL;

    double       chord;//, ainc;
    double       xdot[3], ydot[3];

    int numEdge, numNode, numLoop, numEdgeMinusDegenrate;
    ego *nodes = NULL, *edges = NULL, *loops = NULL;

    int sense = 0, itemp = 0, nodeIndexTE2[2];
    double t, trange[2], result[9], arcLen = 0, totLen = 0;
    double norm = 0, normX = 0, normY = 0;

    //EGADS returns
    int oclass, mtype, *sens = NULL, *edgeLoopSense = NULL, numChildren;
    ego ref, prev, next, *children = NULL, *temp = NULL, nodeTE = NULL;


    ego teObj = NULL;

    int swapYZ = (int) false;
    double swapTemp;

    int nodeIndexLE;
    int nodeIndexTE;

    double xyzLE[3];
    double xyzTE[3];

//    double PI =3.1415926535897931159979635;

//#define DUMP_EGADS_SECTIONS
#ifdef DUMP_EGADS_SECTIONS
    static int ID = 0;
    char filename[256];
    ego newModel, bodyCopy,context;
    EG_getContext(body, &context);
    EG_copyObject(body, NULL, &bodyCopy);

    status = EG_attributeRet(body, "_name", &atype, &alen, &ints, &reals, &string);
    if (status == EGADS_SUCCESS) {
      sprintf(filename, "section_%d_%s.egads", ID, string);
    } else {
      sprintf(filename, "section%d.egads", ID);
    }
    /* make a model and write it out */
    EG_makeTopology(context, NULL, MODEL, 0,
                    NULL, 1, &bodyCopy, NULL, &newModel);
    remove(filename);
    printf(" EG_saveModel(%s) = %d\n", filename, EG_saveModel(newModel, filename));
    EG_deleteObject(newModel);
    ID++;
#endif

    status = findLeadingEdge(body, &nodeIndexLE, xyzLE);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = findTrailingEdge( body, &nodeIndexTE, xyzTE);
    if (status != CAPS_SUCCESS) goto cleanup;

    status = EG_getBodyTopos(body, NULL, NODE, &numNode, &nodes);
    if (status != EGADS_SUCCESS) {
        printf("\tError in vlm_writeSection, getBodyTopos Nodes = %d\n", status);
        goto cleanup;
    }

    status = EG_getBodyTopos(body, NULL, EDGE, &numEdge, &edges);
    if (status != EGADS_SUCCESS) {
        printf("\tError in vlm_writeSection, getBodyTopos Edges = %d\n", status);
        goto cleanup;
    }

    status = EG_getBodyTopos(body, NULL, LOOP, &numLoop, &loops);
    if (status != EGADS_SUCCESS) {
        printf("\tError in vlm_writeSection, getBodyTopos Loops = %d\n", status);
        goto cleanup;
    }

    numEdgeMinusDegenrate = 0;
    for (i = 0; i < numEdge; i++) {
        status = EG_getInfo(edges[i], &oclass, &mtype, &ref, &prev, &next);
        if (mtype == DEGENERATE) continue;
        numEdgeMinusDegenrate += 1;
    }

    // There must be at least 2 nodes and 2 edges
    if ((numEdgeMinusDegenrate != numNode) || (numNode < 2) || (numLoop != 1)) {
        printf("\tError in vlm_writeSection, body has %d nodes, %d edges and %d loops!\n", numNode, numEdge, numLoop);
        printf("\t\tThere must be at least one leading and one trailing edge node!\n");
        status = CAPS_SOURCEERR;
        goto cleanup;
    }

    // Determine plane
    status = determineSectionPlane(numEdge, edges, &swapYZ);
    if (status != CAPS_SUCCESS) goto cleanup;

    // Find TE Edge object, if any
    nodeIndexTE2[0] = nodeIndexTE;
    nodeIndexTE2[1] = nodeIndexTE;
    teObj = NULL;
    for (i = 0; i < numEdge; i++) {

        status = EG_getTopology(edges[i], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);

        if (mtype == DEGENERATE) continue;

        if (status != EGADS_SUCCESS) {
            printf("\tError in vlm_writeSection, Edge %d getTopology = %d!\n", i, status);
            goto cleanup;
        }

        if (numChildren != 2) {
            printf("\tError in vlm_writeSection, Edge %d has %d nodes!\n", i, numChildren);
            goto cleanup;
        }

        // If the edge doesn't at least contain the TE node pass it by
        if (children[0] != nodes[nodeIndexTE-1] && children[1] != nodes[nodeIndexTE-1]) continue;

        // evaluate at the edge mid point
        t = 0.5*(trange[0]+trange[1]);
        status = EG_evaluate(edges[i], &t, result);
        if (status != EGADS_SUCCESS) goto cleanup;

        // get the tangent vector
        xdot[0] = result[3+0];
        xdot[1] = result[3+1];
        xdot[2] = result[3+2];

        norm = sqrt(xdot[0]*xdot[0] + xdot[1]*xdot[1] + xdot[2]*xdot[2]);

        // cross it to get the 'normal' to the edge
        if (swapYZ == (int) true ) {
            normX = fabs(  xdot[1]/norm );
            normY = fabs( -xdot[0]/norm );
        } else {
            normX = fabs(  xdot[2]/norm );
            normY = fabs( -xdot[0]/norm );
        }

        // get the tangent vectors at the end points and make sure the dot product is near 1

        // get the tangent vector at t0
        t = trange[0];
        status = EG_evaluate(edges[i], &t, result);
        if (status != EGADS_SUCCESS) goto cleanup;

        xdot[0] = result[3+0];
        xdot[1] = result[3+1];
        xdot[2] = result[3+2];

        norm = sqrt(xdot[0]*xdot[0] + xdot[1]*xdot[1] + xdot[2]*xdot[2]);

        xdot[0] /= norm;
        xdot[1] /= norm;
        xdot[2] /= norm;

        // get the tangent vector at t1
        t = trange[1];
        status = EG_evaluate(edges[i], &t, result);
        if (status != EGADS_SUCCESS) goto cleanup;

        ydot[0] = result[3+0];
        ydot[1] = result[3+1];
        ydot[2] = result[3+2];

        norm = sqrt(ydot[0]*ydot[0] + ydot[1]*ydot[1] + ydot[2]*ydot[2]);

        ydot[0] /= norm;
        ydot[1] /= norm;
        ydot[2] /= norm;

        // compute the dot between the two tangents
        norm = fabs( xdot[0]*ydot[0] + xdot[1]*ydot[1] + xdot[2]*xdot[2] );

        // if the x-component of the normal is larger, assume the edge is pointing in the streamwise direction
        // the tangent at the end points must also be pointing in the same direction
        if (normX > normY && (1 - norm) < 1e-3) {

          if (teObj != NULL) {
              printf("\tError in vlm_writeSection: Found multiple trailing edges!!\n");
              status = CAPS_SOURCEERR;
              goto cleanup;
          }

          teObj = edges[i];
          nodeIndexTE2[0] = EG_indexBodyTopo(body, children[0]);
          nodeIndexTE2[1] = EG_indexBodyTopo(body, children[1]);
          break;
        }
    }


    // weight the number of points on each edge based on the arcLength
    numPoint = (int *) EG_alloc(numEdge*sizeof(int));

    for (i = 0; i < numEdge; i++) {
        numPoint[i] = 0;

        if ( teObj == edges[i] ) continue; // don't count the trailing edge

        status = EG_getTopology(edges[i], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
        if (mtype == DEGENERATE) continue;
        if (status != EGADS_SUCCESS) goto cleanup;
        status = EG_arcLength( ref, trange[0], trange[1], &arcLen );
        if (status != EGADS_SUCCESS) goto cleanup;

        totLen += arcLen;
    }

    for (i = 0; i < numEdge; i++) {

        if ( teObj == edges[i] ) continue; // don't count the trailing edge

        status = EG_getTopology(edges[i], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
        if (mtype == DEGENERATE) continue;
        if (status != EGADS_SUCCESS) goto cleanup;
        status = EG_arcLength( ref, trange[0], trange[1], &arcLen );
        if (status != EGADS_SUCCESS) goto cleanup;

        numPoint[i] = maxNumPoint*arcLen/totLen;
    }

    // get the trailing edge point to use to compute the chord lenth
    if (teObj == NULL) {

        status = EG_getTopology(nodes[nodeIndexTE-1], &ref, &oclass, &mtype, result, &numChildren, &children, &sens);
        if (status != EGADS_SUCCESS) goto cleanup;

    } else {

        status = EG_getTopology(teObj, &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
        if (status != EGADS_SUCCESS) goto cleanup;

        t = 0.5*(trange[0]+trange[1]);

        status = EG_evaluate(teObj, &t, result);
        if (status != EGADS_SUCCESS) {
            printf("\tError in vlm_writeSection, evaluate = %d!\n", status);
            goto cleanup;
        }
    }

    xdot[0]  =  result[0] - xyzLE[0];
    xdot[1]  =  result[1] - xyzLE[1];
    xdot[2]  =  result[2] - xyzLE[2];

    chord    =  sqrt(xdot[0]*xdot[0] + xdot[1]*xdot[1] + xdot[2]*xdot[2]);
    if (normalize == (int) false) chord = 1;

    xdot[0] /=  chord;
    xdot[1] /=  chord;
    xdot[2] /=  chord;

//    if (swapYZ == (int) true ) {
//        ainc = -atan2(xdot[1], xdot[0])*180./PI;
//    } else {
//        ainc = -atan2(xdot[2], xdot[0])*180./PI;
//    }

    ydot[0]  = -xdot[2];
    ydot[1]  =  xdot[1];
    ydot[2]  =  xdot[0];

    // Determine edge order
    edgeLoopOrder = (int *) EG_alloc(numEdge*sizeof(int));
    edgeLoopSense = (int *) EG_alloc(numEdge*sizeof(int));
    edgeOrder  = (int *) EG_alloc(numEdge*sizeof(int));
    if ( (edgeLoopOrder == NULL) || (edgeLoopSense == NULL) || (edgeOrder == NULL) ) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    for (i = 0; i < numEdge; i++) edgeLoopOrder[i] = CAPSMAGIC;

    status = EG_getTopology(loops[0], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
    if (status != EGADS_SUCCESS) goto cleanup;

    // Get the edge ordering in the loop
    // The first edge may not start at the trailing edge
    for (i = 0; i < numChildren; i++) {

        //printf("edgeIndex = %d\n", edgeIndex);
        edgeIndex = EG_indexBodyTopo(body, children[i]);

        if (edgeIndex < EGADS_SUCCESS) {
            status = CAPS_BADINDEX;
            goto cleanup;
        }

        edgeOrder[i]  = edgeIndex;
        edgeLoopOrder[i] = edgeIndex;
        edgeLoopSense[i] = sens[i];
    }

    // Reorder edge indexing such that a trailing edge node is the first node in the loop
    while ( (int) true ) {

        // the first edge cannot be the TE edge
        if (edges[edgeOrder[0]-1] != teObj) {

            status = EG_getTopology(edges[edgeOrder[0]-1], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
            if (status != EGADS_SUCCESS) goto cleanup;

            if (mtype == DEGENERATE) continue;

            // Get the sense of the edge from the loop
            sense = edgeLoopSense[0];

            // check if the starting child node is one of the TE nodes
            if (sense == 1) {
                if ( children[0] == nodes[nodeIndexTE2[0]-1] || children[0] == nodes[nodeIndexTE2[1]-1] ) {
                    nodeTE = children[0];
                    break;
                }
            } else {
                if ( children[1] == nodes[nodeIndexTE2[0]-1] || children[1] == nodes[nodeIndexTE2[1]-1] ) {
                    nodeTE = children[1];
                    break;
                }
            }
        }

        // rotate the avl order and the edge sense to the left by one
        itemp = edgeOrder[0];
        for (i = 0; i < numEdge - 1; i++)
            edgeOrder[i] = edgeOrder[i + 1];
        edgeOrder[i] = itemp;

        itemp = edgeLoopSense[0];
        for (i = 0; i < numEdge - 1; i++)
          edgeLoopSense[i] = edgeLoopSense[i + 1];
        edgeLoopSense[i] = itemp;
    }

    if (teObj != NULL && teObj != edges[edgeOrder[numEdge-1]-1]) {
        printf("Developer ERROR: Found trailing edge but it's not the last edge in the AVL loop!!!!\n");
        status = CAPS_SOURCEERR;
        goto cleanup;
    }


    // Write out points

    // get the coordinate of the starting trailing edge node
    status = EG_getTopology(nodeTE, &ref, &oclass, &mtype, result, &numChildren, &temp, &sens);
    if (status != EGADS_SUCCESS) goto cleanup;

    result[0] -= xyzLE[0];
    result[1] -= xyzLE[1];
    result[2] -= xyzLE[2];

    if ( swapYZ == (int) true ) {
        swapTemp = result[1];
        result[1] = result[2];
        result[2] = swapTemp;
    }

    fprintf(fp, "%lf %lf\n", (xdot[0]*result[0]+xdot[1]*result[1]+xdot[2]*result[2])/chord,
                             (ydot[0]*result[0]+ydot[1]*result[1]+ydot[2]*result[2])/chord);

    // Loop through edges based on order
    for (i = 0; i < numEdge; i++) {
        //printf("Edge order %d\n", edgeOrder[i]);

        edgeIndex = edgeOrder[i] - 1; // -1 indexing

        if (edges[edgeIndex] == teObj) continue;

        // Get t- range for edge
        status = EG_getTopology(edges[edgeIndex], &ref, &oclass, &mtype, trange, &numChildren, &children, &sens);
        if (mtype == DEGENERATE) continue;
        if (status != EGADS_SUCCESS) goto cleanup;

        // Get the sense of the edge from the loop
        sense = edgeLoopSense[i];

        // Write out in points along edge
        for (j = 1; j < numPoint[edgeIndex]; j++) {
            if (sense == 1) {
                t = trange[0] + j*(trange[1]-trange[0])/numPoint[edgeIndex];
            } else {
                t = trange[1] - j*(trange[1]-trange[0])/numPoint[edgeIndex];
            }

            status = EG_evaluate(edges[edgeIndex], &t, result);
            if (status != EGADS_SUCCESS) goto cleanup;

            result[0] -= xyzLE[0];
            result[1] -= xyzLE[1];
            result[2] -= xyzLE[2];

            if ( swapYZ == (int) true ) {
                swapTemp = result[1];
                result[1] = result[2];
                result[2] = swapTemp;
            }

            fprintf(fp, "%lf %lf\n", (xdot[0]*result[0]+xdot[1]*result[1]+xdot[2]*result[2])/chord,
                                     (ydot[0]*result[0]+ydot[1]*result[1]+ydot[2]*result[2])/chord);
        }

        // Write the last node of the edge
        if (sense == 1) {

            status = EG_getTopology(children[1], &ref, &oclass, &mtype, result, &numChildren, &temp, &sens);
            if (status != EGADS_SUCCESS) goto cleanup;

        } else {

            status = EG_getTopology(children[0], &ref, &oclass, &mtype, result, &numChildren, &temp, &sens);
            if (status != EGADS_SUCCESS) goto cleanup;

        }

        result[0] -= xyzLE[0];
        result[1] -= xyzLE[1];
        result[2] -= xyzLE[2];

        if ( swapYZ == (int) true ) {
            swapTemp = result[1];
            result[1] = result[2];
            result[2] = swapTemp;
        }

        fprintf(fp, "%lf %lf\n", (xdot[0]*result[0]+xdot[1]*result[1]+xdot[2]*result[2])/chord,
                                 (ydot[0]*result[0]+ydot[1]*result[1]+ydot[2]*result[2])/chord);
    }

    fprintf(fp, "\n");

    status = CAPS_SUCCESS;

    goto cleanup;

    cleanup:
        if (status != CAPS_SUCCESS) printf("Error: Premature exit in writeSection, status = %d\n", status);

        EG_free(numPoint);

        EG_free(nodes);
        EG_free(edges);
        EG_free(loops);

        EG_free(edgeLoopOrder);
        EG_free(edgeLoopSense);
        EG_free(edgeOrder);

        return status;
}
