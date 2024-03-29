/*
 *      CAPS: Computational Aircraft Prototype Syntheses
 *
 *             FRICTION AIM
 *
 *      Copyright 2014-2020, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <string.h>
#include <math.h>

#include "aimUtil.h"
#include "miscUtils.h"

#ifdef WIN32
#define getcwd   _getcwd
#define PATH_MAX _MAX_PATH
#else
#include <unistd.h>
#include <limits.h>
#endif

#define NUMINPUT  3
#define NUMOUT    3
#define PI        3.1415926535897931159979635
#define NINT(A)         (((A) < 0)   ? (int)(A-0.5) : (int)(A+0.5))

//#define DEBUG

/*!\mainpage Introduction
 *
 * \tableofcontents
 *
 * \section overviewFRICTION FRICTION AIM Overview
 * FRICTION provides an estimate of laminar and turbulent skin friction and form
 * drag suitable for use in aircraft preliminary design \cite Friction. Taken from the
 * FRICTION manual:
 * "The program has its roots in a program by Ron Hendrickson at Grumman. It runs on any computer. The input
 * requires geometric information and either the Mach and altitude combination, or
 * the Mach and Reynolds number at which the results are desired. It uses standard
 * flat plate skin friction formulas. The compressibility effects on skin friction
 * are found using the Eckert Reference Temperature method for laminar flow and the
 * van Driest II formula for turbulent flow. The basic formulas are valid from
 * subsonic to hypersonic speeds, but the implementation makes assumptions that limit
 * the validity to moderate supersonic speeds (about Mach 3). The key assumption is
 * that the vehicle surface is at the adiabatic wall temperature (the user can easily
 * modify this assumption). Form factors are used to estimate the effect of thickness
 * on drag, and a composite formula is used to include the effect of a partial run of
 * laminar flow."
 *
 * An outline of the AIM's inputs, outputs and attributes are provided in \ref aimInputsFRICTION and
 * \ref aimOutputsFRICTION and \ref attributeFRICTION, respectively.
 *
 *
 * Upon running preAnalysis the AIM generates a single file, "frictionInput.txt" which contains the input
 * information and control sequence for FRICTION to execute.
 * To populate output data the AIM expects a file, "frictionOutput.txt", to exist after running FRICTION.
 * An example execution for FRICTION looks like (Linux and OSX executable being used - see \ref frictionModification):
 *
 * \code{.sh}
 * friction frictionInput.txt frictionOutput.txt
 * \endcode
 *
 * \section frictionModification FRICTION Modifications
 * While FRICTION is available from,
 * <a href="http://www.dept.aoe.vt.edu/~mason/Mason_f/MRsoft.html">FRICTION download</a>,
 * the AIM assumes that a modified version of FRICTION is being used. The modified version allows for longer input
 * and output file name lengths, as well as other I/O modifications. This modified version of
 * FRICTION, friction_eja_mod.f, is supplied and built with the AIM. During the compilation the source code is
 * compiled into an executable with the name \a friction (Linux and OSX) or \a friction.exe (Windows).
 *
 * \section exampleFRICTION Examples
 * An example problem using the FRICTION AIM may be found at
 * \ref frictionExample.
 *
 */

/*! \page attributeFRICTION AIM Attributes
 * The following list of attributes drives the FRICTION geometric definition. Aircraft components are defined as cross sections
 * in the low fidelity geometry definition. To be able to logically group the cross sections into wings, tails, fuselage, etc
 * they must be given a grouping attribute. This attribute defines a logical group along with identifying a set of cross sections
 * as a lifting surface or a body of revolution. The format is as follows.
 *
 *  - <b> capsType</b> This string attribute labels the <em> FaceBody</em> as to which type the section
 *  is assigned. This information is also used to logically group sections together by type to create wings, tails, stores, etc. Because
 *  AWAVE is relatively rigid, the <b> capsType </b> attributes must use the following names:
 *
 *    <em> Lifting Surfaces: </em> Wing, Tail, HTail, VTail, Horizontal_Tail, Vertical_Tail, Canard
 *
 *    <em> Body of Revolution: </em> Fuselage, Fuse, Store
 *
 *  - <b> capsReferenceArea</b>  [Optional: Default 1.0] This attribute may exist on any <em> Body</em>.  Its
 * value will be used as the SREF entry in the FRICTION input.
 *
 *  - <b> capsLength</b> This attribute defines the length units that the *.csm file is generated in.  Friction input MUST be in
 *  units of feet.  The AIM handles all unit conversion internally based on this input.
 *
 */


typedef struct {
    const char *attribute;
    int type; // 0 lifting surface, 1 body of revolution
    double chordLength;
    double arcLength;
    double thickOverChord;
    double xyzLE[3]; // leading edge
    double xyzTE[3]; // trailing edge
} aimSurface;

typedef struct {
    const char *name;
    double swet;
    double refLength;
    double thickOverChord; // thickness to chord ratio
    double type; // (0.0) planar surface (1.0) body
    double turbTrans; // 0.0 fully turbulent (default) 1.0 laminar
} frictionSec;

static int        nInstance  = 0;



/* ****************** FRICTION AIM Helper Functions ************************ */
static void initiate_aimSurface(aimSurface *surface) {

    surface->attribute = NULL;
    surface->type = 0;
    surface->chordLength = 0.0;
    surface->arcLength   = 0.0;
    surface->thickOverChord = 0.0;
    surface->xyzLE[0] = surface->xyzLE[1] = surface->xyzLE[2] = 0.0;
    surface->xyzTE[0] = surface->xyzTE[1] = surface->xyzTE[2] = 0.0;
}

static void initiate_frictionSection(frictionSec *section) {

    section->name = NULL;
    section->swet = 0.0;
    section->refLength = 0.0;
    section->thickOverChord = 0.0; // thickness to chord ratio
    section->type = 0; // (0.0) planar surface (1.0) body
    section->turbTrans = 0.0; // 0.0 fully turbulent (default) 1.0 laminar
}
// ********************** AIM Function Break *****************************
static void
cross(double *A, double *B, double *C)
{

    C[0] = A[1]*B[2] - A[2]*B[1];
    C[1] = A[2]*B[0] - A[0]*B[2];
    C[2] = A[0]*B[1] - A[1]*B[0];

}
// ********************** AIM Function Break *****************************
static void
calculate_distance(double *x1, double *x2, double *x0, double* D)
{

    // (x1)-------------(x2)
    //             |D
    //             (x0)

    double a[3], b[3], tmp[3], length;

    length = sqrt((x1[0]-x2[0])*(x1[0]-x2[0]) + (x1[1]-x2[1])*(x1[1]-x2[1]) + (x1[2]-x2[2])*(x1[2]-x2[2]));
    if (length < 1.0e-8) {
        x1[0] = x1[0] - 1.0;
    }

    a[0] = x2[0] - x1[0];
    b[0] = x1[0] - x0[0];
    a[1] = x2[1] - x1[1];
    b[1] = x1[1] - x0[1];
    a[2] = x2[2] - x1[2];
    b[2] = x1[2] - x0[2];
    cross(a,b,tmp);
    *D = sqrt(tmp[0]*tmp[0] + tmp[1]*tmp[1] + tmp[2]*tmp[2]) / sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);

}

// ********************** AIM Function Break *****************************
static int findSectionData(ego body,
                           int *nle, double *xle,
                           int *nte, double *xyzTE,
                           double *chordLength, double *arcLength, double *thickOverChord)
{
    //Input to this function is an egads body object
    //Find leading edge location x,y,z
    //Find trailing edge location x,y,z
    //Find chord length of section (distance between two points),
    //      both should have same y value if an egads body representing
    //      a cross section || to the yplane is entered
    //Find arc length around the body

    int    i, n, stat, nNode, nEdge, oclass, mtype, *sens,index;
    double xmin=1E6, xyzLE[3], xmax=-1E6, box[6], data[4];
    double massData[14];
    // double xyzLEm1[3], xyzLE0[3];
    double thickness;
    ego    ref, *nodes, *objs, *edges;

    *nle = 0;
    *nte = 0;
    *arcLength = 0.0;

    // check body type, looking for a NODE
    stat = aim_isNodeBody(body, data);
    if (stat < EGADS_SUCCESS) {
        printf(" FRICTION AIM Warning: EG_getTopology Failure in findSectionData Code :: %d\n", stat);
        return CAPS_IOERR;
    }

    /*
	printf(" EGADS Body EG_getTopology Class :: %d :: ",oclass);
	switch (oclass) {
		case NODE:
			printf("NODE\n");
			break;
		case EDGE:
			printf("EDGE\n");
			break;
		case LOOP:
			printf("LOOP\n");
			break;
		case FACE:
			printf("FACE\n");
			break;
		case SHELL:
			printf("SHELL\n");
			break;
		case BODY:
			printf("BODY\n");
			break;
		default:
			break;
	}
     */

    if (stat == EGADS_SUCCESS) {
        *nle = 0;
        *nte = 0;
        *chordLength = 0.0;
        *arcLength = 0.0;
        *thickOverChord = 0.0;
        xle[0] = xyzTE[0] = data[0];
        xle[1] = xyzTE[1] = data[1];
        xle[2] = xyzTE[2] = data[2];
        return CAPS_SUCCESS;
    }

    // assume the LE position is the most forward Node in X
    // assume the TE position is the most rearward Node in X

    // Get Nodes from EGADS body, input to this function
    stat = EG_getBodyTopos(body, NULL, NODE, &nNode, &nodes);
    if (stat != EGADS_SUCCESS) {
        printf(" FRICTION AIM Warning: LE getBodyTopos Nodes = %d\n", stat);
        return CAPS_IOERR;
    }

    stat = EG_getBoundingBox(body, box);
    if (stat != EGADS_SUCCESS) {
        printf(" FRICTION AIM Warning: LE getBoundingBox Nodes = %d\n", stat);
        EG_free(nodes);
        return CAPS_IOERR;
    }
    // Estimate thickness value
    thickness = sqrt((box[1]-box[4])*(box[1]-box[4]) + (box[2]-box[5])*(box[2]-box[5]));
    // Sort throught the list of nodes to determine the LE and TE nodes
    //printf("Nodes %d\n",nNode);
    index = 0;

    for (i = 0; i < nNode; i++) {
        stat = EG_getTopology(nodes[i], &ref, &oclass, &mtype, xyzLE,
                &n, &objs, &sens);
        if (stat != EGADS_SUCCESS) continue;

        // LE search
        if (*nle == 0) {
            *nle = i+1;
            xmin = xyzLE[index];
        } else {
            if (xyzLE[index] < xmin) {
                *nle = i+1;
                xmin = xyzLE[index];
            }
        }
        // TE search
        if (*nte == 0) {
            *nte = i+1;
            xmax = xyzLE[index];
        } else {
            if (xyzLE[index] > xmax) {
                *nte = i+1;
                xmax = xyzLE[index];
            }
        }
    }

    if (*nle == 0) {
        EG_free(nodes);
        return CAPS_IOERR;
    }
    if (*nte == 0) {
        EG_free(nodes);
        return CAPS_IOERR;
    }

    // assign xyzLE location variables xle and xyzTE
    EG_getTopology(nodes[*nle-1], &ref, &oclass, &mtype, xle, &n, &objs, &sens);
    EG_getTopology(nodes[*nte-1], &ref, &oclass, &mtype, xyzTE, &n, &objs, &sens);
    // determine distance between LT and TE points
    *chordLength = sqrt((xle[0]-xyzTE[0])*(xle[0]-xyzTE[0]) +
                        (xle[1]-xyzTE[1])*(xle[1]-xyzTE[1]) +
                        (xle[2]-xyzTE[2])*(xle[2]-xyzTE[2]));

    if ( fabs(*chordLength) < 1.0e-8 ) {
        *chordLength = box[3] - box[0];
    }

    *thickOverChord = thickness / *chordLength;
    EG_free(nodes);

    // determine the arc length around the body
    stat = EG_getBodyTopos(body, NULL, EDGE, &nEdge, &edges);
    if (stat != EGADS_SUCCESS) {
        printf(" FRICTION AIM Warning: LE getBodyTopos Edges = %d\n", stat);
        return CAPS_IOERR;
    }

    for (i = 0; i < nEdge; i++) {

        stat = EG_getMassProperties(edges[i], massData);

        if (stat != EGADS_SUCCESS) continue;
        // massData array population
        // volume, surface area (length), cg(3), iniria(9)
        *arcLength = *arcLength + massData[1];
        // printf("EDGE %d :: %8.6f Length\n",i,massData[1]);

    }
    EG_free(edges);

    return CAPS_SUCCESS;

}

/* ********************** Exposed AIM Functions ***************************** */


// ********************** AIM Function Break *****************************
int
aimInitialize(/*@unused@*/ int ngIn,  /*@unused@*/ /*@null@*/ capsValue *gIn,
              int *qeFlag, /*@null@*/ const char *unitSys, int *nIn, int *nOut,
              int *nFields, char ***fnames, int **ranks)
{
    int flag, ret;

#ifdef DEBUG
    printf("\n frictionAIM/aimInitialize   ngIn = %d!\n", ngIn);
#endif
    flag     = *qeFlag;
    *qeFlag  = 0;

    // specify the number of analysis input and out "parameters"
    *nIn     = NUMINPUT;    // Mach, Altitude
    *nOut    = NUMOUT;      // CDtotal, CDform, CDfric
    if (flag == 1) return CAPS_SUCCESS;

    // specify the field variables this analysis can generate
    *nFields = 0;
    *ranks   = NULL;
    *fnames  = NULL;

    /* return an index for the instance generated */
    ret = nInstance;
    nInstance++;
    return ret;
}

// ********************** AIM Function Break *****************************
int aimInputs(/*@unused@*/ int inst, /*@unused@*/ void *aimInfo, int index,
              char **ainame, capsValue *defval)
{

    /*! \page aimInputsFRICTION AIM Inputs
     * The following list outlines the FRICTION inputs along with their default values available
     * through the AIM interface. All inputs to the FRICTION AIM are variable length arrays.
     * <B> All inputs must be the same length </B>.
     */


#ifdef DEBUG
    printf(" frictionAIM/aimInputs instance = %d  index = %d!\n", inst, index);
#endif

    if (index == 1) {
        *ainame               = EG_strdup("Mach");
        defval->type          = Double;
        defval->lfixed        = Change;
        defval->sfixed        = Change;
        defval->nullVal       = IsNull;
        defval->dim           = Vector;

        /*! \page aimInputsFRICTION
         * - <B> Mach = double </B> <br> OR
         * - <B> Mach = [double, ... , double] </B> <br>
         *  Mach number.
         */

    } else if (index == 2) {
        *ainame               = EG_strdup("Altitude");
        defval->type          = Double;
        defval->lfixed        = Change;
        defval->sfixed        = Change;
        defval->nullVal       = IsNull;
        defval->dim           = Vector;
        defval->units         = EG_strdup("kft");

        /*! \page aimInputsFRICTION
         * - <B> Altitude = double </B> <br> OR
         * - <B> Altitude = [double, ... , double] </B> <br>
         *  Altitude in units of kft number.
         */

    } else if (index == 3) {
        *ainame               = EG_strdup("BL_Transition");
        defval->type          = Double;
        defval->dim           = Vector;
        defval->length        = 1;
        defval->nrow          = 1;
        defval->ncol          = 1;
        defval->units         = NULL;
        defval->lfixed        = Fixed;
        defval->vals.real 	  = 0.10;
        defval->limits.dlims[0] = 0.0; // Limit of accepted values
        defval->limits.dlims[1] = 1.0;

        /*! \page aimInputsFRICTION
         * - <B> BL_Transition = double [0.1 default] </B> <br>
         * Boundary layer laminar to turbulent transition percentage [0.0 turbulent to 1.0 laminar] location for all sections.
         */

    }

    return CAPS_SUCCESS;
}

// ********************** AIM Function Break *****************************
int aimPreAnalysis(/*@unused@*/ int inst, void *aimInfo,
                   const char *apath, /*@null@*/ capsValue *inputs, capsErrs **errs)
{

    int status; // Function status return

    int i; //Indexing

    // Bodies
    int numBody;
    ego *bodies = NULL;

    int         nsecrevfound;

    int         tmp, nle, nte, j, nsec, nsecrev;

    int         *nsecrevid = NULL, *nsecrevidreal = NULL, *nsecrevNewSec= NULL;

    double      SREF, dist, refLength, refArea;

    char        tmpInt[12];

    double      massData[14];
    int         firstEntry;

    // Default input parameters
    double Sref = 1.0;

    // Defined types
    aimSurface  *surfaces = NULL; // aimSurface - structure defined global
    frictionSec *secLift = NULL, *secBody = NULL; // structure defined global

    // File
    FILE        *fp = NULL;

    char *tempString = NULL; // Temp. string holde

    /*@-unrecog@*/
    char         cpath[PATH_MAX];
    /*@+unrecog@*/


    // EGADS function returns
    int atype, alen;
    const int       *ints;
    const double    *reals;
    const char      *intents, *lengthUnitsIn, *string;

#ifdef DEBUG
    printf(" frictionAIM/aimPreAnalysis\n");
#endif

    *errs = NULL;

    // Get EGADS bodies
    status = aim_getBodies(aimInfo, &intents, &numBody, &bodies);
    if (status != CAPS_SUCCESS) {
        printf(" frictionAIM/aimPreAnalysis getBodies = %d!\n", status);
        return status;
    }

    if (inputs == NULL) {
#ifdef DEBUG
        printf(" frictionAIM/aimPreAnalysis inputs == NULL!\n");
#endif
        return CAPS_NULLVALUE;
    }

    if ((numBody <= 0) || (bodies == NULL)) {
#ifdef DEBUG
        printf(" frictionAIM/aimPreAnalysis No Bodies!\n");
#endif

        return CAPS_SOURCEERR;
    }

    // Get where we are and set the path to our input
    (void) getcwd(cpath, PATH_MAX);

    printf("\nCWD :: %s\n",cpath);
    printf("APATH :: %s\n",apath);

    if (chdir(apath) != 0) {
        return CAPS_DIRERR;
    }

    // Check inputs
    if (inputs == NULL) {
#ifdef DEBUG
        printf(" frictionAIM/aimPreAnalysis inputs == NULL!\n");
#endif

        return CAPS_NULLVALUE;
    }

    if (inputs[aim_getIndex(aimInfo, "Mach", ANALYSISIN)-1].nullVal == IsNull ||
            inputs[aim_getIndex(aimInfo, "Altitude", ANALYSISIN)-1].nullVal == IsNull) {

        printf("Either input Mach or Altitude has not been set!\n");
        status = CAPS_NULLVALUE;
        goto cleanup;
    }

    if (inputs[aim_getIndex(aimInfo, "Mach", ANALYSISIN)-1].length !=
            inputs[aim_getIndex(aimInfo, "Altitude", ANALYSISIN)-1].length) {

        printf("Inputs Mach and Altitude must be the same length\n");
        status = CAPS_MISMATCH;
        goto cleanup;
    }

    // Allocate memory to surfaces structure
    surfaces = (aimSurface *) EG_alloc(numBody*sizeof(aimSurface));

    if (surfaces == NULL) {
#ifdef DEBUG
        printf(" frictionAIM/aimPreAnalysis Cannot allocate %d surfaces!\n", numBody);
#endif
        status =  EGADS_MALLOC;
        goto cleanup;
    }

    // Define default inputs for each structure created
    for (i = 0; i < numBody; i++) {
        (void) initiate_aimSurface(&surfaces[i]);
    }

    // Get length units
    status = check_CAPSLength(numBody, bodies, &lengthUnitsIn);
    if (status == CAPS_NOTFOUND) {
        printf(" *** WARNING: frictionAIM: No units assigned *** capsLength is not set in *.csm file!\n");
        lengthUnitsIn = "ft";
    } else if (status != CAPS_SUCCESS) {
        goto cleanup;
    }

    // Populate the surfaces structure with real information - accumulate the Surface data
    // Determine number of friction sections
    nsec = 0;
    for (i = 0; i < numBody; i++) {

        // search for the "capsReferenceArea" attribute
        status = EG_attributeRet(bodies[i], "capsReferenceArea", &atype, &alen, &ints, &reals, &string);
        if (status == EGADS_SUCCESS) {

            if (atype == ATTRREAL) {

                Sref = (double) reals[0];

                status = aim_convert(aimInfo, lengthUnitsIn, Sref, "ft", &Sref); // Convert twice for area
                if (status != CAPS_SUCCESS) goto cleanup;

                status = aim_convert(aimInfo, lengthUnitsIn, Sref, "ft", &Sref);
                if (status != CAPS_SUCCESS) goto cleanup;


            } else {

                printf("capsReferenceArea should be followed by a single real value!\n");
                status = EGADS_ATTRERR;
                goto cleanup;
            }
        }

        // Determine type of body - look for "capsType" attribute
        status = EG_attributeRet(bodies[i], "capsType", &atype, &alen, &ints, &reals, &surfaces[i].attribute);
        if (status != EGADS_SUCCESS) {

            printf(" *** WARNING frictionAIM: capsType not found on body %d - defaulting to 'Wing'!\n", i+1);
            surfaces[i].attribute = "Wing";

        } else {

            if (atype != ATTRSTRING) {

                printf("capsType should be followed by a single string!\n");
                status = EGADS_ATTRERR;
                goto cleanup;
            }
        }

        // Wing, Tail, VTail, HTail, Cannard are all lifting surfaces
        // Fuse, Fuselage, Store are all bodies of revolution

        if ((strcmp(surfaces[i].attribute,"Wing")            == 0) ||
                (strcmp(surfaces[i].attribute,"Tail")   		 == 0) ||
                (strcmp(surfaces[i].attribute,"HTail")   		 == 0) ||
                (strcmp(surfaces[i].attribute,"VTail")   		 == 0) ||
                (strcmp(surfaces[i].attribute,"Horizontal_Tail") == 0) ||
                (strcmp(surfaces[i].attribute,"Vertical_Tail")   == 0) ||
                (strcmp(surfaces[i].attribute,"Cannard") == 0)           ) {

            surfaces[i].type = 0;

            status = findSectionData(bodies[i],
                    &nle, surfaces[i].xyzLE,
                    &nte, surfaces[i].xyzTE,
                    &surfaces[i].chordLength,
                    &surfaces[i].arcLength,
                    &surfaces[i].thickOverChord);
            if (status != CAPS_SUCCESS) {
                printf(" *** WARNING: frictionAIM: findSectionData = %d for body %d!\n", status, i+1);
                goto cleanup;

            }

            printf("Lifting Surface: Body = %d, units %s\n", i+1, lengthUnitsIn);
            printf("\tXLE:   %8.6f %8.6f %8.6f\n", surfaces[i].xyzLE[0], surfaces[i].xyzLE[1], surfaces[i].xyzLE[2]);
            printf("\tXTE:   %8.6f %8.6f %8.6f\n", surfaces[i].xyzTE[0], surfaces[i].xyzTE[1], surfaces[i].xyzTE[2]);
            printf("\tChord: %8.6f\n", surfaces[i].chordLength);
            printf("\tArc:   %8.6f\n", surfaces[i].arcLength);
            printf("\tT/C:   %8.6f\n", surfaces[i].thickOverChord);
            printf("\tType: %s\n", surfaces[i].attribute);

        } else if ((strcmp(surfaces[i].attribute,"Fuse")     == 0) ||
                (strcmp(surfaces[i].attribute,"Fuselage") == 0) ||
                (strcmp(surfaces[i].attribute,"Store")    == 0)) {

            surfaces[i].type = 1;

            status = findSectionData(bodies[i],
                    &nle, surfaces[i].xyzLE,
                    &nte, surfaces[i].xyzTE,
                    &surfaces[i].chordLength,
                    &surfaces[i].arcLength,
                    &surfaces[i].thickOverChord);
            if (status != CAPS_SUCCESS) {
                printf(" *** WARNING: frictionAIM: findSectionData = %d for body %d!\n", status, i+1);
                goto cleanup;
            }

            // volume, surface area (length), cg(3), iniria(9)
            status = EG_getMassProperties(bodies[i], massData);
            if (status != EGADS_SUCCESS) {
                printf("Skipping - Body of Revolution: Body = %d, units %s, NODE type\n", i+1, lengthUnitsIn);
                continue; // Probably have a node
            }

            surfaces[i].chordLength = 2.0 * sqrt(massData[1] / PI); // ~diameter of cross section

            // printf("MASSDATA:  %8.6f, %8.6f\n",massData[0], massData[1]);
            printf("Body of Revolution: Body = %d, units %s\n", i+1, lengthUnitsIn);
            printf("\tArc: %8.6f\n", surfaces[i].arcLength);
            printf("\tDiameter: %8.6f\n", surfaces[i].chordLength);
            printf("\tType: %s\n", surfaces[i].attribute);

        } else {

            printf(" *** WARNING: frictionAIM: capsType attribute not recognized for body %d\n", i+1);
            printf("\tOptions: Wing, Tail, VTail, HTail, Cannard, Vertical_Tail, Horizontal_Tail are all lifting surfaces\n");
            printf("\t Fuse, Fuselage, Store are all bodies of revolution\n");
        }

        if ((i > 0) && (surfaces[i].type == 0)) {
            if (!strcmp(surfaces[i].attribute,surfaces[i-1].attribute)) nsec++;
        }
    }

    if (nsec == 0) {
        printf(" *** WARNING: frictionAIM: numSection= 0!\n");
        status = EGADS_NOTFOUND;
        goto cleanup;
    }

    // alocate memory to surfaces structure
    secLift = (frictionSec *) EG_alloc(nsec*sizeof(frictionSec));
    if (secLift == NULL) {
#ifdef DEBUG
        printf(" frictionAIM/aimPreAnalysis Cannot allocate %d sections!\n", nsec);
#endif
        status = EGADS_MALLOC;
        goto cleanup;
    }

    /// Initiate sections
    for (i = 0; i < nsec; i++) {
        (void) initiate_frictionSection(&secLift[i]);

        // Set turbulent transition based on input value
        secLift[i].turbTrans = inputs[aim_getIndex(aimInfo, "BL_Transition", ANALYSISIN)-1].vals.real;
    }

    nsec      = 0;
    nsecrev   = 0;
    nsecrevfound = 1;

    nsecrevid = (int *) EG_alloc(numBody*sizeof(int));
    if (nsecrevid == NULL) {
#ifdef DEBUG
        printf(" frictionAIM/aimPreAnalysis Cannot allocate %d secrev!\n", numBody);
#endif
        status = EGADS_MALLOC;
        goto cleanup;
    }

    // Bodies of revolution may have multiple bodies in a single section.  This method only puts two bodies together into a section
    // The counter nsecrev follows the revolution sections put together
    for (i = 0; i < numBody; i++) {

        nsecrevid[i] = 0;

        if (i > 0) {

            if (!strcmp(surfaces[i].attribute,surfaces[i-1].attribute)) {

                // LIFTING SURFACES
                if (surfaces[i].type == 0) {

                    secLift[nsec].name = surfaces[i].attribute;
                    secLift[nsec].thickOverChord = (surfaces[i].thickOverChord + surfaces[i-1].thickOverChord) / 2.0;

                    calculate_distance(surfaces[i-1].xyzLE, surfaces[i-1].xyzTE, surfaces[i].xyzLE, &dist);

                    status = aim_convert(aimInfo, lengthUnitsIn, dist, "ft", &dist);
                    if (status != CAPS_SUCCESS) goto cleanup;

                    refLength = (surfaces[i].chordLength + surfaces[i-1].chordLength) / 2.0;
                    refArea   = (dist * (surfaces[i].arcLength + surfaces[i-1].arcLength)) / 2.0;

                    status = aim_convert(aimInfo, lengthUnitsIn, refLength, "ft", &secLift[nsec].refLength);
                    if (status != CAPS_SUCCESS) goto cleanup;

                    status = aim_convert(aimInfo, lengthUnitsIn, refArea, "ft", &secLift[nsec].swet);
                    if (status != CAPS_SUCCESS) goto cleanup;

                    nsec++;

                    // BODY OF REVOLUTION
                } else if (surfaces[i].type == 1) {
                    // count the number of surfaces and note their location
                    nsecrev++;
                    nsecrevfound = 0;
                    nsecrevid[i] = 1;
                }
            }
        }
    }

    // determine location of body of revolution sections in the body list
    if (nsecrev > 0) {
        nsecrevidreal = (int *) EG_alloc(nsecrev*sizeof(int));
        nsecrevNewSec = (int *) EG_alloc(nsecrev*sizeof(int));

        if ((nsecrevidreal == NULL) || (nsecrevNewSec == NULL)) {
            status =  EGADS_MALLOC;
            goto cleanup;
        }

        nsecrev = 1;
        tmp = 0;
        firstEntry = 1;
        for (i = 0; i < numBody; i++) {
            if (nsecrevid[i] == 1) {
                nsecrevidreal[tmp] = i;
                nsecrevNewSec[tmp] = 0;
                if (firstEntry==0 && strcmp(surfaces[i].attribute,surfaces[i-1].attribute) != 0) {
                    // if the sections don't have the same attribute then another body has been found
                    nsecrev++;
                    nsecrevNewSec[tmp] = 1;
                }
                tmp++;
                firstEntry = 0;
            } else {
                firstEntry = 1;
            }
        }
        nsecrevNewSec[0] = 1;

        // allocate memory for revolution bodies
        secBody = (frictionSec *) EG_alloc(nsecrev*sizeof(frictionSec));
        if (secBody == NULL) {

            status = EGADS_MALLOC;
            goto cleanup;
        }

        for (i = 0; i < nsecrev; i++) {
            (void) initiate_frictionSection(&secBody[i]);

            secBody[i].type = 1; // Change type from default of 0
            // Set turbulent transition based on input value
            secBody[i].turbTrans = inputs[aim_getIndex(aimInfo, "BL_Transition", ANALYSISIN)-1].vals.real;
        }

        // tmp is the total number of surfaces that make up ALL of the body sections
        nsecrev = -1;
        for (i = 0; i < tmp; i++) {

            firstEntry = 0;

            if (nsecrevNewSec[i] == 1) {
                if (i > 0) { // last body section is finished, onto the next one

                    status = aim_convert(aimInfo, lengthUnitsIn, SREF, "ft", &SREF);
                    if (status != CAPS_SUCCESS) goto cleanup;

                    secBody[nsecrev].thickOverChord = secBody[nsecrev].refLength / SREF;
                }

                nsecrev++;

                secBody[nsecrev].name = surfaces[nsecrevidreal[i]].attribute;
                SREF = surfaces[nsecrevidreal[i]].chordLength; // used as a placehold variable for max diameter

                firstEntry = 1;
            }

            if (firstEntry == 0) {

                dist = fabs(surfaces[nsecrevidreal[i]].xyzLE[0] - surfaces[nsecrevidreal[i-1]].xyzLE[0]); // aligned with flow direction X - global axis

                status = aim_convert(aimInfo, lengthUnitsIn, dist, "ft", &dist);
                if (status != CAPS_SUCCESS) goto cleanup;

                secBody[nsecrev].refLength = secBody[nsecrev].refLength + dist; // keep adding on each length component to the body ref
                refArea   = (surfaces[nsecrevidreal[i]].arcLength + surfaces[nsecrevidreal[i-1]].arcLength);

                status = aim_convert(aimInfo, lengthUnitsIn, refArea, "ft", &refArea);
                if (status != CAPS_SUCCESS) goto cleanup;

                secBody[nsecrev].swet = secBody[nsecrev].swet + (dist * refArea) / 2.0;
                refLength = (surfaces[nsecrevidreal[i]].chordLength + surfaces[nsecrevidreal[i-1]].chordLength) / 2.0; // ref diameter

                status = aim_convert(aimInfo, lengthUnitsIn, refLength, "ft", &refLength);
                if (status != CAPS_SUCCESS) goto cleanup;

                if (surfaces[nsecrevidreal[i]].chordLength > SREF) {
                    SREF = surfaces[nsecrevidreal[i]].chordLength;
                }
            }

            if (i==tmp-1) {

                status = aim_convert(aimInfo, lengthUnitsIn, SREF, "ft", &SREF);
                if (status != CAPS_SUCCESS) goto cleanup;

                secBody[nsecrev].thickOverChord = SREF / secBody[nsecrev].refLength;
            }
        }
    }

    printf("Number of sections %d, number of revolution sections %d\n", nsec, nsecrev+1-nsecrevfound);

    // Create char for number of sections in the friction input (lifting + bodyOfRev)
    /*@-bufferoverflowhigh@*/
    if (nsec+nsecrev+1-nsecrevfound >= 10) {
        sprintf(tmpInt,"%d.       ",nsec+nsecrev+1-nsecrevfound);
    } else {
        sprintf(tmpInt,"%d.        ",nsec+nsecrev+1-nsecrevfound);
    }
    /*@+bufferoverflowhigh@*/

    // Create input file for friction
    fp = fopen("frictionInput.txt","w");
    if (fp == NULL) {
        status =  CAPS_IOERR;
        goto cleanup;
    }

    fprintf(fp,"CAPS Generated Friction Input File\n");

    tempString = convert_doubleToString(Sref,8,0); // 8 spaces, left justified (0)
    fprintf(fp,"%s  1.0       %s0.0\n",tempString,tmpInt);
    if(tempString != NULL) EG_free(tempString);

    //fprintf(fp,"1234567890123456789012345678901234567890123456789012345678901234567890\n");

    // LIFTING SURFACES
    for (i = 0; i < nsec; i++) {

        // Component Name columns 1-16 w/ 4 spaces to 20
        fprintf(fp,"%s",secLift[i].name);

        for (j = 0; j < 20-strlen(secLift[i].name); j++) {
            fprintf(fp," ");
        }

        // SWET spaces 21-30
        tempString = convert_doubleToString(secLift[i].swet,8,0); // 8 spaces, left justified (0)
        fprintf(fp,"%s  ",tempString);
        if(tempString != NULL) EG_free(tempString);

        // RefL spaces 31-40
        tempString = convert_doubleToString(secLift[i].refLength,8,0);
        fprintf(fp,"%s  ",tempString);
        if(tempString != NULL) EG_free(tempString);

        // ToC spaces 41-50
        tempString = convert_doubleToString(secLift[i].thickOverChord,8,0);
        fprintf(fp,"%s  ",tempString);
        if(tempString != NULL) EG_free(tempString);

        // Component type 51-60
        tempString = convert_doubleToString(secLift[i].type,8,0);
        fprintf(fp,"%s  ",tempString);
        if(tempString != NULL) EG_free(tempString);

        // FTrans 61-70
        tempString = convert_doubleToString(secLift[i].turbTrans,8,0);
        fprintf(fp,"%s  ",tempString);
        if(tempString != NULL) EG_free(tempString);

        //
        fprintf(fp,"\n");
    }

    // BODIES OF REVOLUTION
    if (secBody != NULL)
        for (i = 0; i <= nsecrev; i++) {
            // Component Name columns 1-16 w/ 4 spaces to 20
            fprintf(fp,"%s",secBody[i].name);

            for (j = 0; j < 20-strlen(secBody[i].name); j++) {
                fprintf(fp," ");
            }

            // SWET spaces 21-30
            tempString = convert_doubleToString(secBody[i].swet,8,0);
            fprintf(fp,"%s  ",tempString);
            if(tempString != NULL) EG_free(tempString);

            // RefL spaces 31-40
            tempString = convert_doubleToString(secBody[i].refLength,8,0);
            fprintf(fp,"%s  ",tempString);
            if(tempString != NULL) EG_free(tempString);

            // ToC spaces 41-50
            tempString = convert_doubleToString(secBody[i].thickOverChord,8,0);
            fprintf(fp,"%s  ",tempString);
            if(tempString != NULL) EG_free(tempString);

            // Component type 51-60
            tempString = convert_doubleToString(secBody[i].type,8,0);
            fprintf(fp,"%s  ",tempString);
            if(tempString != NULL) EG_free(tempString);

            // FTrans 61-70
            tempString = convert_doubleToString(secBody[i].turbTrans,8,0);
            fprintf(fp,"%s  ",tempString);
            if(tempString != NULL) EG_free(tempString);

            //
            fprintf(fp,"\n");
        }


    printf("Number of Mach-Altitude cases = %d\n", inputs[aim_getIndex(aimInfo, "Mach", ANALYSISIN)-1].length);

    if (inputs[0].length == 1) {
        // MACH
        tempString = convert_doubleToString(inputs[aim_getIndex(aimInfo, "Mach", ANALYSISIN)-1].vals.real, 8, 0);
        fprintf(fp,"%s  ",tempString);
        if(tempString != NULL) EG_free(tempString);

        // ALTITUDE
        tempString = convert_doubleToString(inputs[aim_getIndex(aimInfo, "Altitude", ANALYSISIN)-1].vals.real, 8, 0);
        fprintf(fp,"%s\n",tempString);
        if(tempString != NULL) EG_free(tempString);

    } else {

        for (i = 0; i < inputs[aim_getIndex(aimInfo, "Mach", ANALYSISIN)-1].length; i++) { // Multiple Mach, Altitude pairs

            // MACH
            tempString = convert_doubleToString(inputs[aim_getIndex(aimInfo, "Mach", ANALYSISIN)-1].vals.reals[i], 8, 0);
            fprintf(fp,"%s  ",tempString);
            if(tempString != NULL) EG_free(tempString);

            // ALTITUDE
            tempString = convert_doubleToString(inputs[aim_getIndex(aimInfo, "Altitude", ANALYSISIN)-1].vals.reals[i], 8, 0);
            fprintf(fp,"%s\n",tempString);
            if(tempString != NULL) EG_free(tempString);
        }
    }

    fprintf(fp,"0.00      0.00\n");


    status = CAPS_SUCCESS;

    goto cleanup;

    cleanup:

    if (status != CAPS_SUCCESS) printf("Premature exit in frictionAIM preAnalysis status = %d\n", status);

    chdir(cpath);

    if (fp != NULL) fclose(fp);

    if (nsecrevidreal != NULL) EG_free(nsecrevidreal);
    if (nsecrevNewSec != NULL) EG_free(nsecrevNewSec);
    if (nsecrevid     != NULL) EG_free(nsecrevid);

    if (secLift   != NULL) EG_free(secLift);
    if (secBody   != NULL) EG_free(secBody);

    if (surfaces  != NULL) EG_free(surfaces);

    return status;
}

// ********************** AIM Function Break *****************************
int aimOutputs( int inst, void *aimInfo, int index, char **aoname, capsValue *form)
{
#ifdef DEBUG
    printf(" frictionAIM/aimOutputs instance = %d  index = %d!\n", inst, index);
#endif

    /*! \page aimOutputsFRICTION AIM Outputs
     * Total, Form, and Friction drag components:
     */

    if (index == 1) {

        *aoname             = EG_strdup("CDtotal");

        /*! \page aimOutputsFRICTION
         * - <B> CDtotal = </B> Drag Coefficient [CDform + CDfric].
         */

    } else if (index == 2) {

        *aoname             = EG_strdup("CDform");

        /*! \page aimOutputsFRICTION
         * - <B> CDform = </B> Form Drag Coefficient.
         */

    } else if (index == 3) {

        *aoname             = EG_strdup("CDfric");

        /*! \page aimOutputsFRICTION
         * - <B> CDfric = </B> Friction Drag Coefficient.
         */
    }

    form->type   = Double;
    form->lfixed = Change;
    form->sfixed = Fixed;
    form->dim    = Vector;
    form->length = 1;
    form->nrow   = 1;
    form->ncol   = 1;
    form->vals.real = 0.0;
    form->vals.reals = NULL;

    return CAPS_SUCCESS;
}

// ********************** AIM Function Break *****************************
int aimCalcOutput(int inst, /*@unused@*/ void *aimInfo, const char *apath, int index,
                  capsValue *val, capsErrs **errors)
{

    int status; // Function return status

    int i; // Indexing

    int valueCount = 0;

    size_t linecap = 0;
    char   cpath[PATH_MAX], *valstr = NULL, *line = NULL;

    double tmp, Fric[100], Prof[100], Tot[100];

    FILE   *fp = NULL;

#ifdef DEBUG
    printf(" frictionAIM/aimCalcOutput instance = %d  index = %d!\n", inst, index);
#endif

    *errors        = NULL;

    if (val->length > 1) {
        if (val->vals.reals != NULL) EG_free(val->vals.reals);
        val->vals.reals = NULL;
    } else {
        val->vals.real = 0.0;
    }

    val->nrow = 1;
    val->ncol = 1;
    val->length = val->nrow*val->ncol;

    //  Get where we are and set the path to our output */
    (void) getcwd(cpath, PATH_MAX);
    if (chdir(apath) != 0) {
#ifdef DEBUG
        printf(" frictionAIM/aimCalcOutput Cannot chdir to %s!\n", apath);
#endif

        status = CAPS_DIRERR;
        goto cleanup;
    }

    // Open the friction output file
    fp = fopen("frictionOutput.txt", "r");
    if (fp == NULL) {
#ifdef DEBUG
        printf(" frictionAIM/aimCalcOutput Cannot open Output file!\n");
#endif

        status = CAPS_IOERR;
        goto cleanup;
    }

    /* scan the file for the string */
    valstr = NULL;

    while (getline(&line, &linecap, fp) >= 0) {

        if (line == NULL) continue;

        valstr = strstr(line, "SUMMARY");
        if (valstr != NULL) {

            /* found it -- get the value */
            //printf("\n *** FRICTION SUMMARY FOUND ***\n");

            // Move down three lines
            getline(&line, &linecap, fp);
            getline(&line, &linecap, fp);

            //printf("HEAD :: %s\n",line);

            while (getline(&line, &linecap, fp) >= 0) {
                status = sscanf(line,"%*d %*lf %*lf %*lf %*lf %*lf %lf", &tmp);
                if (status == -1) break; // using the break to exit the while loop, not a read error

                Tot[valueCount] = tmp;
                status = sscanf(line,"%*d %*lf %*lf %*lf %*lf %lf %*lf",&tmp);

                Prof[valueCount] = tmp;

                status = sscanf(line,"%*d %*lf %*lf %*lf %lf %*lf %*lf",&tmp);

                Fric[valueCount] = tmp;

                valueCount += 1;

                //printf("DATA :: %s\n",line);
            }

            break;
        }
    }

    if (valueCount < 1) {
#ifdef DEBUG
        printf(" frictionAIM/aimCalcOutput Cannot find %s in Output file!\n", str);
#endif

        status = CAPS_NOTFOUND;
        goto cleanup;
    }

    val->nrow = valueCount;
    val->ncol = 1;
    val->length = val->nrow*val->ncol;

    if (valueCount == 1) {
        if (index == 1) val->vals.real = Tot[0];
        if (index == 2) val->vals.real = Prof[0];
        if (index == 3) val->vals.real = Fric[0];

    } else if (valueCount > 1) {

        val->vals.reals = (double *) EG_alloc(val->length*sizeof(double));

        if (val->vals.reals == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        for (i = 0; i < valueCount; i++) {

            if (index == 1) val->vals.reals[i] = Tot[i];
            if (index == 2) val->vals.reals[i] = Prof[i];
            if (index == 3) val->vals.reals[i] = Fric[i];

        }
    }

    status = CAPS_SUCCESS;
    goto cleanup;

    cleanup:
    if (status != CAPS_SUCCESS) printf("Premature exit in frictionAIM aimCalcOutput status = %d\n", status);

    chdir(cpath);

    if (fp != NULL) fclose(fp);

    if (line != NULL) EG_free(line);

    return status;
}


// ********************** AIM Function Break *****************************
void
aimCleanup()
{
#ifdef DEBUG
    printf(" frictionAIM/aimCleanup!\n");
#endif

}
