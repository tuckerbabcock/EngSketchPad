from __future__ import print_function

# Import pyCAPS class file
from pyCAPS import capsProblem

# Import os module
import os
import argparse

# Setup and read command line options. Please note that this isn't required for pyCAPS
parser = argparse.ArgumentParser(description = 'Nastran Composite Pytest Example',
                                 prog = 'nastran_Composite_PyTest',
                                 formatter_class = argparse.ArgumentDefaultsHelpFormatter)

#Setup the available commandline options
parser.add_argument('-workDir', default = "./", nargs=1, type=str, help = 'Set working/run directory')
parser.add_argument('-noAnalysis', action='store_true', default = False, help = "Don't run analysis code")
parser.add_argument("-verbosity", default = 1, type=int, choices=[0, 1, 2], help="Set output verbosity")
args = parser.parse_args()

# Initialize capsProblem object
myProblem = capsProblem()

# Load CSM file
myProblem.loadCAPS("../csmData/feaCantileverPlate.csm", verbosity=args.verbosity)

# Load nastran aim
myProblem.loadAIM(aim = "nastranAIM",
                  altName = "nastran",
                  analysisDir = os.path.join(str(args.workDir[0]), "NastranCompositeTest"),
                  capsIntent = "STRUCTURE")

# Set project name so a mesh file is generated
projectName = "pyCAPS_nastran_Test"
myProblem.analysis["nastran"].setAnalysisVal("Proj_Name", projectName)

myProblem.analysis["nastran"].setAnalysisVal("Edge_Point_Max", 10)

myProblem.analysis["nastran"].setAnalysisVal("Quad_Mesh", True)

myProblem.analysis["nastran"].setAnalysisVal("File_Format", "Small")

myProblem.analysis["nastran"].setAnalysisVal("Mesh_File_Format", "Large")

# Set analysis
eigen = { "extractionMethod"     : "Lanczos",
          "frequencyRange"       : [0, 10000],
          "numEstEigenvalue"     : 1,
          "numDesiredEigenvalue" : 4,
          "eigenNormaliztion"    : "MASS"}
myProblem.analysis["nastran"].setAnalysisVal("Analysis", ("EigenAnalysis", eigen))

# Set materials
unobtainium  = {"youngModulus" : 2.2E6 ,
                "poissonRatio" : .5,
                "density"      : 7850}

madeupium    = {"materialType" : "isotropic",
                "youngModulus" : 1.2E5 ,
                "poissonRatio" : .5,
                "density"      : 7850}
myProblem.analysis["nastran"].setAnalysisVal("Material", [("Unobtainium", unobtainium),
                                                          ("Madeupium", madeupium)])

# Set property
shell  = {"propertyType"           : "Composite",
          "shearBondAllowable"     : 1.0e6,
          "bendingInertiaRatio"    : 1.0, # Default - not necesssary
          "shearMembraneRatio"     : 0, # Turn off shear - no materialShear
          "membraneThickness"      : 0.2,
          "compositeMaterial"      : ["Unobtainium", "Madeupium", "Madeupium"],
          "compositeFailureTheory" : "STRN",
          "compositeThickness"     : [1.2, 0.5, 2.0],
          "compositeOrientation"   : [30.6, 45, 50.4]}

myProblem.analysis["nastran"].setAnalysisVal("Property", ("plate", shell))

# Set constraints
constraint = {"groupName" : ["plateEdge"],
              "dofConstraint" : 123456}

myProblem.analysis["nastran"].setAnalysisVal("Constraint", ("cantilever", constraint))


# Run AIM pre-analysis
myProblem.analysis["nastran"].preAnalysis()

####### Run Nastran####################
print ("\n\nRunning Nastran......")
currentDirectory = os.getcwd() # Get our current working directory

os.chdir(myProblem.analysis["nastran"].analysisDir) # Move into test directory

if (args.noAnalysis == False):
    os.system("nastran old=no notify=no batch=no scr=yes sdirectory=./ " + projectName +  ".dat"); # Run Nastran via system call

os.chdir(currentDirectory) # Move back to working directory
print ("Done running Nastran!")

# Run AIM post-analysis
myProblem.analysis["nastran"].postAnalysis()

# Close CAPS
myProblem.closeCAPS()
