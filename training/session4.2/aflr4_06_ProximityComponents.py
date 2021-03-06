#------------------------------------------------------------------------------#

# Allow print statement to be compatible between Python 2 and 3
from __future__ import print_function

# Import pyCAPS class
from pyCAPS import capsProblem

# Import os
import os

#------------------------------------------------------------------------------#

# Initialize capsProblem object
myProblem = capsProblem()

# Load CSM file
filename = os.path.join("..","EGADS","CFDViscous_WingPod.egads")
transport = myProblem.loadCAPS(filename)

# Load AFLR4 AIM
aflr4 = myProblem.loadAIM(aim = "aflr4AIM",
                          analysisDir = "workDir_AFLR4")

# Use 5 segments on farfield face edges
aflr4.setAnalysisVal("ff_nseg", 5)

# Scaling factor to compute AFLR4 'ref_len' parameter via
aflr4.setAnalysisVal("Mesh_Length_Factor", 1)

# Set mesh sizing parmeters
aflr4.setAnalysisVal("Mesh_Sizing", [("Farfield", {"bcType":"Farfield"}),
                                     ("Wing", {"scaleFactor":20}),
                                     ("Pylon", {"scaleFactor":10}),
                                     ("Pod", {"scaleFactor":2})])

# Run AIM pre-/post-analysis
aflr4.preAnalysis()
aflr4.postAnalysis()

# View the surface tessellation
aflr4.viewGeometry()
