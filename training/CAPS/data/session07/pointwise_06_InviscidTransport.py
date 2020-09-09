#------------------------------------------------------------------------------#

# Allow print statement to be compatible between Python 2 and 3
from __future__ import print_function

# Import pyCAPS class
from pyCAPS import capsProblem

# Import os and platform
import os
import platform
import time
#------------------------------------------------------------------------------#

def run_pointwise(pointwise):
    # Run AIM pre-analysis
    pointwise.preAnalysis()
    
    ####### Run pointwise #################
    currentDirectory = os.getcwd() # Get current working directory
    os.chdir(pointwise.analysisDir)    # Move into test directory
    
    CAPS_GLYPH = os.environ["CAPS_GLYPH"]
    for i in range(60):
        if "Windows" in platform.system():
            PW_HOME = os.environ["PW_HOME"]
            os.system('"' + PW_HOME + '\\win64\\bin\\tclsh.exe ' + CAPS_GLYPH + '\\GeomToMesh.glf" caps.egads capsUserDefaults.glf')
        else:
            os.system("pointwise -b " + CAPS_GLYPH + "/GeomToMesh.glf caps.egads capsUserDefaults.glf")
    
        time.sleep(1) # let the harddrive breathe
        if os.path.isfile('caps.GeomToMesh.gma') and os.path.isfile('caps.GeomToMesh.ugrid'): break
        time.sleep(20) # wait and try again
    
    os.chdir(currentDirectory)     # Move back to top directory
    #######################################
    
    # Run AIM post-analysis
    pointwise.postAnalysis()

#------------------------------------------------------------------------------#

# Initialize capsProblem object
myProblem = capsProblem()

# Load CSM file
transport = myProblem.loadCAPS(os.path.join("..","EGADS","CFDInviscid_Transport.egads"))

# Load pointwise aim
pointwise = myProblem.loadAIM(aim = "pointwiseAIM",
                              analysisDir = "workDir_6_InviscidTransport")

# Dump VTK files for visualization
pointwise.setAnalysisVal("Proj_Name", "Transport")
pointwise.setAnalysisVal("Mesh_Format", "VTK")

# Connector level
pointwise.setAnalysisVal("Connector_Turn_Angle"    , 10)
pointwise.setAnalysisVal("Connector_Source_Spacing", True)

# Domain level
pointwise.setAnalysisVal("Domain_Algorithm"   , "AdvancingFront")
pointwise.setAnalysisVal("Domain_Max_Layers"  , 15)
pointwise.setAnalysisVal("Domain_TRex_ARLimit", 20.0)
pointwise.setAnalysisVal("Domain_Decay"       , 0.8)

# Block level
pointwise.setAnalysisVal("Block_Boundary_Decay"      , 0.8)
pointwise.setAnalysisVal("Block_Edge_Max_Growth_Rate", 1.5)

# Execute pointwise
run_pointwise(pointwise)

# View the surface tessellation
pointwise.viewGeometry()