from __future__ import print_function

# Import pyCAPS class file
from pyCAPS import capsProblem

# Import os module
import os
import argparse

# Setup and read command line options. Please note that this isn't required for pyCAPS
parser = argparse.ArgumentParser(description = 'Delaundo Pytest Example',
                                 prog = 'delaundo_PyTest.py',
                                 formatter_class = argparse.ArgumentDefaultsHelpFormatter)

#Setup the available commandline options
parser.add_argument('-workDir', default = "./", nargs=1, type=str, help = 'Set working/run directory')
parser.add_argument('-noAnalysis', action='store_true', default = False, help = "Don't run analysis code")
parser.add_argument("-verbosity", default = 1, type=int, choices=[0, 1, 2], help="Set output verbosity")
args = parser.parse_args()

# Initialize capsProblem object
myProblem = capsProblem()

# Create working directory variable
workDir = str(args.workDir[0]) + "/DelaundoAnalysisTest"

# Load CSM file
geometryScript = os.path.join("..","csmData","cfd2D.csm")
myProblem.loadCAPS(geometryScript, verbosity=args.verbosity)

# Set the sharp trailing edge geometry design parameter
myProblem.geometry.setGeometryVal("sharpTE", 0.0)

# Load delaundo aim
myMesh = myProblem.loadAIM(aim = "delaundoAIM", analysisDir = workDir)

# Set airfoil edge parameters
airfoil = {"numEdgePoints" : 100, "edgeDistribution" : "Tanh", "initialNodeSpacing" : [0.001, 0.001]}

# Set meshing parameters
myMesh.setAnalysisVal("Mesh_Sizing", [("Airfoil"   , airfoil),
                                                                 ("TunnelWall", {"numEdgePoints" : 5}),
                                                                 ("InFlow",     {"numEdgePoints" : 5}),
                                                                 ("OutFlow",    {"numEdgePoints" : 5})])

# Thickness of stretched region
myMesh.setAnalysisVal("Delta_Thickness", .1)

# Maximum aspect ratio
myMesh.setAnalysisVal("Max_Aspect", 90.0)

# Set project name and output mesh type
projectName = "delaundoMesh"
myMesh.setAnalysisVal("Proj_Name", projectName )

myMesh.setAnalysisVal("Mesh_Format", "Tecplot")

# Run AIM pre-analysis
myMesh.preAnalysis()

####### Run delaundo ####################
print ("\n\nRunning Delaundo......")
currentDirectory = os.getcwd() # Get our current working directory

os.chdir(myMesh.analysisDir) # Move into test directory

file = open("Input.sh", "w") # Create a simple input shell script with are control file name
file.write(projectName + ".ctr\n")
file.close()

if (args.noAnalysis == False): # Don't run delaundo if noAnalysis is set
    os.system("delaundo < Input.sh > Info.out"); # Run delaundo via system call

os.chdir(currentDirectory) # Move back to top directory

# Run AIM post-analysis
myMesh.postAnalysis()

# Retrieve Surface mesh - and rewrite it in desired format
myMesh.getAnalysisOutVal("Mesh")

# Close CAPS
myProblem.closeCAPS()
