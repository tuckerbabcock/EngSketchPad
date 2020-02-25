from __future__ import print_function

# Import pyCAPS class file
from pyCAPS import capsProblem

# Import argparse module
import os
import argparse
import platform

# Setup and read command line options. Please note that this isn't required for pyCAPS
parser = argparse.ArgumentParser(description = 'Pointwise Pytest Example',
                                 prog = 'pointwise_PyTest',
                                 formatter_class = argparse.ArgumentDefaultsHelpFormatter)

#Setup the available commandline options
parser.add_argument('-workDir', default = "./", nargs=1, type=str, help = 'Set working/run directory')
parser.add_argument("-verbosity", default = 1, type=int, choices=[0, 1, 2], help="Set output verbosity")
args = parser.parse_args()

# Working directory
workDir = os.path.join(str(args.workDir[0]), "PointwiseAnalysisTest")

# Initialize capsProblem object
myProblem = capsProblem()

# Load CSM file
myProblem.loadCAPS("../csmData/cfdMultiBody.csm", verbosity=args.verbosity)

# Load AFLR4 aim
pointwise = myProblem.loadAIM(aim = "pointwiseAIM",
                              analysisDir = workDir)

# Set project name so a mesh file is generated
# pointwise.setAnalysisVal("Proj_Name", "pyCAPS_Pointwise_Test")
#
# pointwise.setAnalysisVal("Mesh_ASCII_Flag", True)
#
# pointwise.setAnalysisVal("Mesh_Format", "AFLR3")

# useGlobal = False
#
# if useGlobal:
#     pointwise.setAnalysisVal("BL_Initial_Spacing", 0.1)
#     pointwise.setAnalysisVal("BL_Thickness", 1.0)
#
# else:
#     inviscidBC = {"boundaryLayerThickness" : 0.0, "boundaryLayerSpacing" : 0.0}
#     viscousBC  = {"boundaryLayerThickness" : 5.0, "boundaryLayerSpacing" : 0.05}
#
#     # Set mesh sizing parmeters
#     pointwise.setAnalysisVal("Mesh_Sizing", [("Wing1", inviscidBC),
#                                                                ("Wing2", viscousBC)])


# Run AIM pre-analysis
pointwise.preAnalysis()

# Pointwise batch execution notes (PW_HOME referes to the Pointwise installation directory):
# Windows (DOS prompt) -
#     PW_HOME\win64\bin\tclsh %CAPS_GLYPH%\GeomToMesh.glf caps.egads capsUserDefaults.glf
# Linux/macOS (terminal) -
#     PW_HOME/pointwise -b $CAPS_GLYPH/GeomToMesh.glf caps.egads capsUserDefaults.glf

# Run pointwise
currentDir = os.getcwd()
os.chdir(pointwise.analysisDir)

CAPS_GLYPH = os.environ["CAPS_GLYPH"]
if platform.system() == "Windows":
    PW_HOME = os.environ["PW_HOME"]
    os.system(PW_HOME + "\win64\bin\tclsh.exe " + CAPS_GLYPH + "\\GeomToMesh.glf caps.egads capsUserDefaults.glf")
elif "CYGWIN" in platform.system():
    PW_HOME = os.environ["PW_HOME"]
    os.system(PW_HOME + "/win64/bin/tclsh.exe " + CAPS_GLYPH + "/GeomToMesh.glf caps.egads capsUserDefaults.glf")
else:
    os.system("pointwise -b " + CAPS_GLYPH + "/GeomToMesh.glf caps.egads capsUserDefaults.glf")

os.chdir(currentDir)

# Run AIM post-analysis
pointwise.postAnalysis()

#pointwise.viewGeometry()

# Close CAPS
myProblem.closeCAPS()
