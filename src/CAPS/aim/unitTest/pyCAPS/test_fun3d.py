import unittest

import os
import shutil

from sys import version_info as pyVersion

import pyCAPS

class TestFUN3D(unittest.TestCase):

    @classmethod
    def setUpClass(cls):

        cls.file = "../csmData/cfdMultiBody.csm"
        cls.analysisDir = "fun3d_UnitTest"

        cls.configFile = "fun3d.nml"
        cls.myProblem = pyCAPS.capsProblem()

        cls.myGeometry = cls.myProblem.loadCAPS(cls.file)

        cls.myAnalysis = cls.myProblem.loadAIM(aim = "fun3dAIM",
                                               analysisDir = cls.analysisDir)
        
        cls.myAnalysis.setAnalysisVal("Overwrite_NML", True)

    @classmethod
    def tearDownClass(cls):
 
        # Remove analysis directories
        if os.path.exists(cls.analysisDir):
            shutil.rmtree(cls.analysisDir)
        
        if os.path.exists(cls.analysisDir + '_1'):
            shutil.rmtree(cls.analysisDir + '_1')
            
        if os.path.exists(cls.analysisDir + '_2'):
            shutil.rmtree(cls.analysisDir + '_2')
            
        if os.path.exists(cls.analysisDir + '_3'):
            shutil.rmtree(cls.analysisDir + '_3')

#         # Remove created files
#         if os.path.isfile("myGeometry.egads"):
#             os.remove("myGeometry.egads")

        cls.myProblem.closeCAPS()
        
    # Try put an invalid boundary name
    def test_invalidBoundaryName(self):
        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + "_1")
        
        myAnalysis.setAnalysisVal("Boundary_Condition", [("Wing1", {"bcType" : "Inviscid"}), # No capsGroup 'X'
                                                         ("X", {"bcType" : "Inviscid"}),
                                                         ("Farfield","farfield")])
        with self.assertRaises(pyCAPS.CAPSError) as e:
            myAnalysis.preAnalysis()
 
        self.assertEqual(e.exception.errorName, "CAPS_NOTFOUND")
        
    # Try an invalid boundary type
    def test_invalidBoundary(self):        
        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + "_2")
        
        myAnalysis.setAnalysisVal("Boundary_Condition", ("Wing1", {"bcType" : "X"}))
        
        with self.assertRaises(pyCAPS.CAPSError) as e:
            myAnalysis.preAnalysis()
 
        self.assertEqual(e.exception.errorName, "CAPS_NOTFOUND")
        
    # Test re-enter 
    def test_reenter(self):
        
        self.myAnalysis.setAnalysisVal("Boundary_Condition", [("Wing1", {"bcType" : "Inviscid"}),
                                                              ("Wing2", {"bcType" : "Inviscid"}),
                                                              ("Farfield","farfield")])
        self.myAnalysis.preAnalysis()
        self.myAnalysis.postAnalysis()
        
        self.assertEqual(os.path.isfile(os.path.join(self.myAnalysis.analysisDir, self.configFile)), True)
        
        os.remove(os.path.join(self.analysisDir, self.configFile))
        
        self.myAnalysis.setAnalysisVal("Boundary_Condition", [("Wing1", {"bcType" : "Viscous"}),
                                                              ("Wing2", {"bcType" : "Inviscid"}),
                                                              ("Farfield","farfield")])
        self.myAnalysis.preAnalysis()
        self.myAnalysis.postAnalysis()
        
        self.assertEqual(os.path.isfile(os.path.join(self.myAnalysis.analysisDir, self.configFile)), True)
        
    # Turn off Overwrite_NML
    def test_overwriteNML(self):
        
        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + "_3")
        myAnalysis.setAnalysisVal("Overwrite_NML", False)
        
        myAnalysis.setAnalysisVal("Boundary_Condition", ("Wing1", {"bcType" : "Inviscid"}))
        
        myAnalysis.preAnalysis() # Don't except an config file because Overwrite_NML = False
    
        self.assertEqual(os.path.isfile(os.path.join(myAnalysis.analysisDir, self.configFile)), False)
 
if __name__ == '__main__':
    unittest.main()
