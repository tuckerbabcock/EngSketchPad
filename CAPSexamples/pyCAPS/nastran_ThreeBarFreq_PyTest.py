## [importPrint]
from __future__ import print_function
## [importPrint]

## [import]
# Import pyCAPS class file
from pyCAPS import capsProblem

# Import os module
import os
import argparse
## [import]

# Setup and read command line options. Please note that this isn't required for pyCAPS
parser = argparse.ArgumentParser(description = 'Nastran Three Bar Frequency Pytest Example',
                                 prog = 'nastran_ThreeBarFreq_PyTest',
                                 formatter_class = argparse.ArgumentDefaultsHelpFormatter)

#Setup the available commandline options
parser.add_argument('-workDir', default = "./", nargs=1, type=str, help = 'Set working/run directory')
parser.add_argument('-noAnalysis', action='store_true', default = False, help = "Don't run analysis code")
parser.add_argument("-verbosity", default = 1, type=int, choices=[0, 1, 2], help="Set output verbosity")
args = parser.parse_args()

## [initateProblem]
# Initialize capsProblem object
myProblem = capsProblem()
## [initateProblem]

## [geometry]
# Load CSM file
myProblem.loadCAPS("../csmData/feaThreeBar.csm", verbosity=args.verbosity)
## [geometry]

## [loadAIM]
# Load nastran aim
nastranAIM = myProblem.loadAIM(aim = "nastranAIM",
                               altName = "nastran",
                               analysisDir= os.path.join(str(args.workDir[0]), "NastranThreeBarFreq"),
                               capsIntent = "STRUCTURE")
## [loadAIM]

## [setInputs]
# Set project name so a mesh file is generated
projectName = "threebar_nastran_Test"
nastranAIM.setAnalysisVal("Proj_Name", projectName)
nastranAIM.setAnalysisVal("File_Format", "Free")
nastranAIM.setAnalysisVal("Mesh_File_Format", "Large")
nastranAIM.setAnalysisVal("Edge_Point_Max", 2);
nastranAIM.setAnalysisVal("Edge_Point_Min", 2);
nastranAIM.setAnalysisVal("Analysis_Type", "Modal");
## [setInputs]

## [defineMaterials]
madeupium    = {"materialType" : "isotropic",
                "youngModulus" : 1.0E7 ,
                "poissonRatio" : .33,
                "density"      : 0.1}

nastranAIM.setAnalysisVal("Material", [("Madeupium", madeupium)])
## [defineMaterials]

## [defineProperties]
rod  =   {"propertyType"      : "Rod",
          "material"          : "Madeupium",
          "crossSecArea"      : 1.0}

rod2  =   {"propertyType"      : "Rod",
          "material"          : "Madeupium",
          "crossSecArea"      : 2.0}

nastranAIM.setAnalysisVal("Property", [("bar1", rod),
                                       ("bar2", rod2),
                                       ("bar3", rod)])
## [defineProperties]

# Set constraints
## [defineConstraints]
constraint = {"groupName" : ["boundary"],
              "dofConstraint" : 123456}

nastranAIM.setAnalysisVal("Constraint", ("BoundaryCondition", constraint))
## [defineConstraints]

## [defineAnalysis]
eigen = { "extractionMethod"     : "Lanczos",
          "frequencyRange"       : [0, 10000],
          "numEstEigenvalue"     : 1,
          "numDesiredEigenvalue" : 10,
          "eigenNormaliztion"    : "MASS"}

nastranAIM.setAnalysisVal("Analysis", ("EigenAnalysis", eigen))
## [defineAnalysis]

# Run AIM pre-analysis
## [preAnalysis]
nastranAIM.preAnalysis()
## [preAnalysis]

## [run]
print ("\n\nRunning Nastran......")
currentDirectory = os.getcwd() # Get our current working directory
os.chdir(nastranAIM.analysisDir) # Move into test directory
if (args.noAnalysis == False): os.system("nastran old=no notify=no batch=no scr=yes sdirectory=./ " + projectName +  ".dat"); # Run Nastran via system call
os.chdir(currentDirectory) # Move back to working directory
print ("Done running Nastran!")
## [run]

## [postAnalysis]
nastranAIM.postAnalysis()
## [postAnalysis]

# Close CAPS
## [close]
myProblem.closeCAPS()
## [close]
