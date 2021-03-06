from __future__ import print_function

# Import pyCAPS class file
from pyCAPS import capsProblem

# Import os module   
import os
import argparse

# Setup and read command line options. Please note that this isn't required for pyCAPS
parser = argparse.ArgumentParser(description = 'Nastran Aeroelastic Pytest Example',
                                 prog = 'nastran_Aeroelastic_PyTest',
                                 formatter_class = argparse.ArgumentDefaultsHelpFormatter)

#Setup the available commandline options
parser.add_argument('-workDir', default = "./", nargs=1, type=str, help = 'Set working/run directory')
parser.add_argument('-noAnalysis', action='store_true', default = False, help = "Don't run analysis code")
parser.add_argument("-verbosity", default = 1, type=int, choices=[0, 1, 2], help="Set output verbosity")
args = parser.parse_args()

# Initialize capsProblem object
myProblem = capsProblem()

# Load CSM file 
myProblem.loadCAPS("../csmData/feaSimplePlate.csm", verbosity=args.verbosity)

# Create project name 
projectName = "NastranSingleLoadPlate"

# Load Nastran aim 
myProblem.loadAIM(aim = "nastranAIM", 
                  altName = "nastran", 
                  analysisDir = os.path.join(str(args.workDir[0]), projectName), 
                  capsIntent = "STRUCTURE")

# Set project name so a mesh file is generated 
myProblem.analysis["nastran"].setAnalysisVal("Proj_Name", projectName) 

# Set meshing parameters
myProblem.analysis["nastran"].setAnalysisVal("Edge_Point_Max", 4)

myProblem.analysis["nastran"].setAnalysisVal("Quad_Mesh", True)

myProblem.analysis["nastran"].setAnalysisVal("Tess_Params", [.25,.01,15])

# Set analysis type
myProblem.analysis["nastran"].setAnalysisVal("Analysis_Type", "Static");

# Set materials 
madeupium    = {"materialType" : "isotropic", 
                "youngModulus" : 72.0E9 , 
                "poissonRatio": 0.33, 
                "density" : 2.8E3}

myProblem.analysis["nastran"].setAnalysisVal("Material", ("Madeupium", madeupium))

# Set properties
shell  = {"propertyType" : "Shell", 
          "membraneThickness" : 0.006, 
          "material"        : "madeupium", 
          "bendingInertiaRatio" : 1.0, # Default           
          "shearMembraneRatio"  : 5.0/6.0} # Default 

myProblem.analysis["nastran"].setAnalysisVal("Property", ("plate", shell))

# Set constraints
constraint = {"groupName" : "plateEdge", 
              "dofConstraint" : 123456}

myProblem.analysis["nastran"].setAnalysisVal("Constraint", ("edgeConstraint", constraint))

# Set load
load = {"groupName" : "plate", 
        "loadType" : "Pressure", 
        "pressureForce" : 2.e6}

# Set loads    
myProblem.analysis["nastran"].setAnalysisVal("Load", ("appliedPressure", load ))

# Set analysis  
# No analysis case information needs to be set for a single static load case

# Run AIM pre-analysis
myProblem.analysis["nastran"].preAnalysis()

####### Run Nastran ####################
print ("\n\nRunning Nastran......")
currentDirectory = os.getcwd() # Get our current working directory 

os.chdir(myProblem.analysis["nastran"].analysisDir) # Move into test directory

if (args.noAnalysis == False):
    os.system("nastran old=no notify=no batch=no scr=yes sdirectory=./ " + projectName +  ".dat"); # Run Nastran via system call

os.chdir(currentDirectory) # Move back to working directory 
print ("Done running Nastran!")
######################################

# Run AIM post-analysis
myProblem.analysis["nastran"].postAnalysis()

# Close CAPS 
myProblem.closeCAPS()
