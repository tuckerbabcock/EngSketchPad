## [importPrint]
from __future__ import print_function
## [importPrint]

# Import pyCAPS and os module
## [import]
import pyCAPS

import os
## [import]

import argparse
# Setup and read command line options. Please note that this isn't required for pyCAPS
parser = argparse.ArgumentParser(description = 'AVL Pytest Example',
                                 prog = 'avl_PyTest.py',
                                 formatter_class = argparse.ArgumentDefaultsHelpFormatter)

#Setup the available commandline options
parser.add_argument('-workDir', default = "." + os.sep, nargs=1, type=str, help = 'Set working/run directory')
parser.add_argument("-verbosity", default = 1, type=int, choices=[0, 1, 2], help="Set output verbosity")
args = parser.parse_args()

# -----------------------------------------------------------------
# Initialize capsProblem object
# -----------------------------------------------------------------
## [initateProblem]
myProblem = pyCAPS.capsProblem()
## [initateProblem]

# -----------------------------------------------------------------
# Load CSM file and Change a design parameter - area in the geometry 
# Any despmtr from the avlWing.csm file are available inside the pyCAPS script
# They are: thick, camber, area, aspect, taper, sweep, washout, dihedral
# -----------------------------------------------------------------

## [geometry]
myGeometry = myProblem.loadCAPS(os.path.join("..","csmData","avlWings.csm"), verbosity=args.verbosity)
## [geometry]

# Create working directory variable
## [localVariable]
workDir = "AVLAutoSpanAnalysisTest"
## [localVariable]
workDir = os.path.join(str(args.workDir[0]), workDir)

# -----------------------------------------------------------------
# Load desired aim 
# -----------------------------------------------------------------
print ("Loading AIM")
## [loadAIM]
myAnalysis = myProblem.loadAIM(aim = "avlAIM", 
                               analysisDir = workDir)
## [loadAIM]
# -----------------------------------------------------------------
# Also available are all aimInput values
# Set new Mach/Alt parameters
# -----------------------------------------------------------------

## [setInputs]
myAnalysis.setAnalysisVal("Mach", 0.5)
myAnalysis.setAnalysisVal("Alpha", 1.0)
myAnalysis.setAnalysisVal("Beta", 0.0)

AVL_Surface = []
for i in range(1,4):
    wing = {"groupName"    : "Wing"+str(i), # Notice Wing is the value for the capsGroup attribute 
            "numChord"     : 12, 
            "spaceChord"   : 1.0, 
            "numSpanTotal" : 40, 
            "spaceSpan"    : 1.0}
    
    AVL_Surface.append( ("Wing"+str(i), wing) )

myAnalysis.setAnalysisVal("AVL_Surface", AVL_Surface)
## [setInputs]

# -----------------------------------------------------------------
# Run AIM pre-analysis
# -----------------------------------------------------------------
## [preAnalysis]
myAnalysis.preAnalysis()
## [preAnalysis]

# -----------------------------------------------------------------
# Run AVL
# -----------------------------------------------------------------
## [runAVL]
print ("Running AVL")
currentDirectory = os.getcwd() # Get our current working directory 
os.chdir(myAnalysis.analysisDir) # Move into test directory

os.system("avl caps < avlInput.txt > avlOutput.txt");

os.chdir(currentDirectory) # Move back to working directory 
## [runAVL]

# -----------------------------------------------------------------
# Run AIM post-analysis
# -----------------------------------------------------------------
## [postAnalysis]
myAnalysis.postAnalysis()
## [postAnalysis]

# -----------------------------------------------------------------
# Get Output Data from AVL
# These calls access aimOutput data
# -----------------------------------------------------------------
## [output]
print ("CXtot  ", myAnalysis.getAnalysisOutVal("CXtot"))
print ("CYtot  ", myAnalysis.getAnalysisOutVal("CYtot"))
print ("CZtot  ", myAnalysis.getAnalysisOutVal("CZtot"))
print ("Cltot  ", myAnalysis.getAnalysisOutVal("Cltot"))
print ("Cmtot  ", myAnalysis.getAnalysisOutVal("Cmtot"))
print ("Cntot  ", myAnalysis.getAnalysisOutVal("Cntot"))
print ("Cl'tot ", myAnalysis.getAnalysisOutVal("Cl'tot"))
print ("Cn'tot ", myAnalysis.getAnalysisOutVal("Cn'tot"))
print ("CLtot  ", myAnalysis.getAnalysisOutVal("CLtot"))
print ("CDtot  ", myAnalysis.getAnalysisOutVal("CDtot"))
print ("CDvis  ", myAnalysis.getAnalysisOutVal("CDvis"))
print ("CLff   ", myAnalysis.getAnalysisOutVal("CLff"))
print ("CYff   ", myAnalysis.getAnalysisOutVal("CYff"))
print ("CDind  ", myAnalysis.getAnalysisOutVal("CDind"))
print ("CDff   ", myAnalysis.getAnalysisOutVal("CDff"))
print ("e      ", myAnalysis.getAnalysisOutVal("e"))
## [output]

Cl = myAnalysis.getAnalysisOutVal("CLtot")
Cd = myAnalysis.getAnalysisOutVal("CDtot")

## [sensitivity]    
sensitivity = myAnalysis.getSensitivity("Alpha", "CLtot")
## [sensitivity]

# -----------------------------------------------------------------
# Close CAPS 
# -----------------------------------------------------------------
myProblem.closeCAPS()