from __future__ import print_function
import unittest

import os
import shutil

import pyCAPS

class TestAFLR4(unittest.TestCase):


    @classmethod
    def setUpClass(cls):

        # Initialize a global capsProblem object
        cls.myProblem = pyCAPS.capsProblem()

        # Working directory
        cls.workDir = "workDir_aflr4Test"

        # Load CSM file
        cls.myGeometry = cls.myProblem.loadCAPS("../csmData/cornerGeom.csm")

    @classmethod
    def tearDownClass(cls):

        # Close CAPS - Optional
        cls.myProblem.closeCAPS()

        # Remove analysis directories
        if os.path.exists(cls.workDir):
            shutil.rmtree(cls.workDir)

    def test_invalid_Mesh_Lenght_Scale(self):

        file = "../csmData/cfdSingleBody.csm"
        myProblem = pyCAPS.capsProblem()

        myGeometry = myProblem.loadCAPS(file)

        myAnalysis = myProblem.loadAIM(aim = "aflr4AIM",
                                       analysisDir = self.workDir)

        # Set project name so a mesh file is generated
        myAnalysis.setAnalysisVal("Proj_Name", "pyCAPS_AFLR4_Test")

        # Set output grid format since a project name is being supplied - Tecplot  file
        myAnalysis.setAnalysisVal("Mesh_Format", "Tecplot")

        # Set output grid format since a project name is being supplied - Tecplot  file
        myAnalysis.setAnalysisVal("Mesh_Length_Factor", -1)

        with self.assertRaises(pyCAPS.CAPSError) as e:
            myAnalysis.preAnalysis()

        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

    def test_setAnalysisVals(self):

        file = "../csmData/cfdSingleBody.csm"
        myProblem = pyCAPS.capsProblem()

        myGeometry = myProblem.loadCAPS(file)

        myAnalysis = myProblem.loadAIM(aim = "aflr4AIM",
                                       analysisDir = self.workDir)

        myAnalysis.setAnalysisVal("Proj_Name", "pyCAPS_AFLR4_Test")
        myAnalysis.setAnalysisVal("Mesh_Quiet_Flag", True)
        myAnalysis.setAnalysisVal("Mesh_Format", "Tecplot")
        myAnalysis.setAnalysisVal("Mesh_ASCII_Flag", True)
        myAnalysis.setAnalysisVal("Mesh_Gen_Input_String", "ff_nseg 10")
        myAnalysis.setAnalysisVal("ff_nseg", 10)
        myAnalysis.setAnalysisVal("min_ncell", 20)
        myAnalysis.setAnalysisVal("no_prox", True)
        myAnalysis.setAnalysisVal("abs_min_scale", 0.01)
        myAnalysis.setAnalysisVal("BL_thickness", 0.01)
        myAnalysis.setAnalysisVal("curv_factor", 0.4)
        myAnalysis.setAnalysisVal("max_scale", 0.1)
        myAnalysis.setAnalysisVal("min_scale", 0.01)
        myAnalysis.setAnalysisVal("Mesh_Length_Factor", 1.05)
        myAnalysis.setAnalysisVal("erw_all", 0.7)

    def test_SingleBody(self):

        file = "../csmData/cfdSingleBody.csm"
        myProblem = pyCAPS.capsProblem()

        myGeometry = myProblem.loadCAPS(file)

        myAnalysis = myProblem.loadAIM(aim = "aflr4AIM",
                                       analysisDir = self.workDir)

        # Run
        myAnalysis.preAnalysis()
        myAnalysis.postAnalysis()


    def test_MultiBody(self):

        file = "../csmData/cfdMultiBody.csm"
        myProblem = pyCAPS.capsProblem()

        myGeometry = myProblem.loadCAPS(file)

        myAnalysis = myProblem.loadAIM(aim = "aflr4AIM",
                                       analysisDir = self.workDir)

        # Run
        myAnalysis.preAnalysis()
        myAnalysis.postAnalysis()

    def test_reenter(self):

        file = "../csmData/cfdSingleBody.csm"
        myProblem = pyCAPS.capsProblem()

        myGeometry = myProblem.loadCAPS(file)

        myAnalysis = myProblem.loadAIM(aim = "aflr4AIM",
                                       analysisDir = self.workDir)

        myAnalysis.setAnalysisVal("Mesh_Length_Factor", 1)

        # Run 1st time
        myAnalysis.preAnalysis()
        myAnalysis.postAnalysis()

        myAnalysis.setAnalysisVal("Mesh_Length_Factor", 2)

        # Run 2nd time coarser
        myAnalysis.preAnalysis()
        myAnalysis.postAnalysis()


    def test_box(self):

        # Load aflr4 aim
        aflr4 = self.myProblem.loadAIM(aim = "aflr4AIM",
                                           analysisDir = self.workDir + "Box",
                                           capsIntent = ["box", "farfield"])

        aflr4.setAnalysisVal("max_scale", 0.5)
        aflr4.setAnalysisVal("min_scale", 0.1)
        aflr4.setAnalysisVal("ff_nseg", 10)

        # Just make sure it runs without errors...
        aflr4.preAnalysis()
        aflr4.postAnalysis()

    def test_cylinder(self):

        # Load aflr4 aim
        aflr4 = self.myProblem.loadAIM(aim = "aflr4AIM",
                                           analysisDir = self.workDir + "Cylinder",
                                           capsIntent = ["cylinder", "farfield"])

        aflr4.setAnalysisVal("max_scale", 0.5)
        aflr4.setAnalysisVal("min_scale", 0.1)
        aflr4.setAnalysisVal("ff_nseg", 10)

        # Just make sure it runs without errors...
        aflr4.preAnalysis()
        aflr4.postAnalysis()

    def test_cone(self):

        # Load aflr4 aim
        aflr4 = self.myProblem.loadAIM(aim = "aflr4AIM",
                                           analysisDir = self.workDir + "Cone",
                                           capsIntent = ["cone", "farfield"])

        aflr4.setAnalysisVal("max_scale", 0.5)
        aflr4.setAnalysisVal("min_scale", 0.1)
        aflr4.setAnalysisVal("ff_nseg", 10)

        # Just make sure it runs without errors...
        aflr4.preAnalysis()
        aflr4.postAnalysis()

    def test_torus(self):

        # Load aflr4 aim
        aflr4 = self.myProblem.loadAIM(aim = "aflr4AIM",
                                           analysisDir = self.workDir + "Torus",
                                           capsIntent = ["torus", "farfield"])

        aflr4.setAnalysisVal("max_scale", 0.5)
        aflr4.setAnalysisVal("min_scale", 0.1)
        aflr4.setAnalysisVal("ff_nseg", 10)

        # Just make sure it runs without errors...
        aflr4.preAnalysis()
        aflr4.postAnalysis()

        #aflr4.viewGeometry()

    def test_sphere(self):

        # Load aflr4 aim
        aflr4 = self.myProblem.loadAIM(aim = "aflr4AIM",
                                           analysisDir = self.workDir + "Sphere",
                                           capsIntent = ["sphere", "farfield"])

        aflr4.setAnalysisVal("max_scale", 0.5)
        aflr4.setAnalysisVal("min_scale", 0.1)
        aflr4.setAnalysisVal("ff_nseg", 10)

        # Just make sure it runs without errors...
        aflr4.preAnalysis()
        aflr4.postAnalysis()

        #aflr4.viewGeometry()

    def off_test_boxhole(self):

        # Load aflr4 aim
        aflr4 = self.myProblem.loadAIM(aim = "aflr4AIM",
                                           analysisDir = self.workDir + "BoxHole",
                                           capsIntent = ["boxhole", "farfield"])

        aflr4.setAnalysisVal("max_scale", 0.5)
        aflr4.setAnalysisVal("min_scale", 0.1)
        aflr4.setAnalysisVal("ff_nseg", 10)

        # Just make sure it runs without errors...
        aflr4.preAnalysis()
        aflr4.postAnalysis()

        #aflr4.viewGeometry()

    def test_all(self):

        # Load aflr4 aim
        aflr4 = self.myProblem.loadAIM(aim = "aflr4AIM",
                                           analysisDir = self.workDir + "All",
                                           capsIntent = ["box", "cylinder", "cone", "torus", "sphere", "farfield"]) #, "boxhole"

        aflr4.setAnalysisVal("max_scale", 0.5)
        aflr4.setAnalysisVal("min_scale", 0.1)
        aflr4.setAnalysisVal("ff_nseg", 10)

        # Just make sure it runs without errors...
        aflr4.preAnalysis()
        aflr4.postAnalysis()

        #aflr4.viewGeometry()


if __name__ == '__main__':
    unittest.main()
