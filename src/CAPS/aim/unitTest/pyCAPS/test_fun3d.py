import unittest

import os
import shutil

from sys import version_info as pyVersion

# f90nml is used to write fun3d inputs not available in the aim
import f90nml

import pyCAPS

class TestFUN3D(unittest.TestCase):

    @classmethod
    def setUpClass(cls):

        cls.file = os.path.join("..","csmData","cfdMultiBody.csm")
        cls.analysisDir = "workDir_fun3dTest"
        cls.iDir = 1

        cls.configFile = "fun3d.nml"
        cls.myProblem = pyCAPS.capsProblem()

        cls.myGeometry = cls.myProblem.loadCAPS(cls.file)

        aflr4 = cls.myProblem.loadAIM(aim = "aflr4AIM",
                                      analysisDir = cls.analysisDir + str(cls.iDir)); cls.iDir += 1

        aflr4.setAnalysisVal("Mesh_Length_Factor", 60.00)

        aflr4.preAnalysis()
        aflr4.postAnalysis()

        cls.aflr3 = cls.myProblem.loadAIM(aim = "aflr3AIM",
                                          analysisDir = cls.analysisDir + str(cls.iDir),
                                          parents = aflr4); cls.iDir += 1

        cls.aflr3.preAnalysis()
        cls.aflr3.postAnalysis()

        cls.myAnalysis = cls.myProblem.loadAIM(aim = "fun3dAIM",
                                               analysisDir = cls.analysisDir + str(cls.iDir),
                                               parents = cls.aflr3); cls.iDir += 1
        
        cls.myAnalysis.setAnalysisVal("Overwrite_NML", True)

    @classmethod
    def tearDownClass(cls):
 
        # Remove analysis directories
        if os.path.exists(cls.analysisDir):
            shutil.rmtree(cls.analysisDir)
        
        for i in range(1,cls.iDir):
            if os.path.exists(cls.analysisDir + '_' + str(i)):
                shutil.rmtree(cls.analysisDir + '_' + str(i))

#         # Remove created files
#         if os.path.isfile("myGeometry.egads"):
#             os.remove("myGeometry.egads")

        cls.myProblem.closeCAPS()
        
    # Try put an invalid boundary name
    def test_invalidBoundaryName(self):
        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + str(self.iDir),
                                            parents = self.aflr3); self.__class__.iDir += 1
        
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
                                            analysisDir = self.analysisDir + str(self.iDir),
                                            parents = self.aflr3); self.__class__.iDir += 1
        
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
        
        os.remove(os.path.join(self.myAnalysis.analysisDir, self.configFile))
        
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
                                            analysisDir = self.analysisDir + str(self.iDir),
                                            parents = self.aflr3); self.__class__.iDir += 1
        myAnalysis.setAnalysisVal("Overwrite_NML", False)
        
        myAnalysis.setAnalysisVal("Boundary_Condition", [("Wing1", {"bcType" : "Viscous"}),
                                                         ("Wing2", {"bcType" : "Inviscid"}),
                                                         ("Farfield","farfield")])
        
        myAnalysis.preAnalysis() # Don't except a config file because Overwrite_NML = False
    
        self.assertEqual(os.path.isfile(os.path.join(myAnalysis.analysisDir, self.configFile)), False)

    # Create sensitvities
    def test_sensitivity(self):

        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + str(self.iDir),
                                            parents = self.aflr3); self.__class__.iDir += 1
        
        myAnalysis.setAnalysisVal("Boundary_Condition", [("Wing1", {"bcType" : "Viscous"}),
                                                         ("Wing2", {"bcType" : "Inviscid"}),
                                                         ("Farfield","farfield")])
                
        myAnalysis.setAnalysisVal("Design_Variable", ("Alpha", {"upperBound": 10.0}))
        
        myAnalysis.setAnalysisVal("Design_Objective", ("ClCd",{"weight": 1.0, "target": 2.7}))
        
        myAnalysis.setAnalysisVal("Alpha", 1)
        
        myAnalysis.preAnalysis()
        myAnalysis.postAnalysis()
        
    # Test using Cython to write and modify the *.nml file
    def test_cythonNML(self):
        
        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + str(self.iDir),
                                            parents = self.aflr3); self.__class__.iDir += 1
        
        fun3dnml = f90nml.Namelist()
        fun3dnml['boundary_output_variables'] = f90nml.Namelist()
        fun3dnml['boundary_output_variables']['mach'] = True
        fun3dnml['boundary_output_variables']['cp'] = True
        fun3dnml['boundary_output_variables']['average_velocity'] = True

        fun3dnml.write(os.path.join(myAnalysis.analysisDir,self.configFile), force=True)

        myAnalysis.setAnalysisVal("Use_Python_NML", True)
        myAnalysis.setAnalysisVal("Overwrite_NML", False) # append
        
        myAnalysis.setAnalysisVal("Boundary_Condition", [("Wing1", {"bcType" : "Viscous"}),
                                                         ("Wing2", {"bcType" : "Inviscid"}),
                                                         ("Farfield","farfield")])
        
        myAnalysis.preAnalysis()
    
        self.assertEqual(os.path.isfile(os.path.join(myAnalysis.analysisDir, self.configFile)), True)

     # Test using Cython to write and modify the *.nml file with reentrance into the AIM
    def test_cythonNMLReentrance(self):
        
        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + str(self.iDir),
                                            parents = self.aflr3); self.__class__.iDir += 1
        
        myAnalysis.setAnalysisVal("Use_Python_NML", True)
        myAnalysis.setAnalysisVal("Overwrite_NML", False) # append
        
        myAnalysis.setAnalysisVal("Boundary_Condition", [("Wing1", {"bcType" : "Viscous"}),
                                                         ("Wing2", {"bcType" : "Inviscid"}),
                                                         ("Farfield","farfield")])
        
        myAnalysis.preAnalysis()
    
        self.assertEqual(os.path.isfile(os.path.join(myAnalysis.analysisDir, self.configFile)), True)
        
        myAnalysis.postAnalysis()
        
        myAnalysis.setAnalysisVal("Mach", 0.8)
         
        myAnalysis.preAnalysis()
             
        self.assertEqual(os.path.isfile(os.path.join(myAnalysis.analysisDir, self.configFile)), True)
        
        myAnalysis.postAnalysis()
        
    # Test using Cython to write and modify the *.nml file - catch an error
    def test_cythonNMLError(self):
    
        # Create a new instance
        myAnalysis = self.myProblem.loadAIM(aim = "fun3dAIM",
                                            analysisDir = self.analysisDir + str(self.iDir),
                                            parents = self.aflr3); self.__class__.iDir += 1
        
        # Create a bad nml file 
        f = open(os.path.join(myAnalysis.analysisDir,self.configFile), "w")
        f.write("&badNamelist")
        f.close()
        
        myAnalysis.setAnalysisVal("Use_Python_NML", True)
        myAnalysis.setAnalysisVal("Overwrite_NML", False) # append
     
        myAnalysis.setAnalysisVal("Boundary_Condition", ("Wing1", {"bcType" : "Inviscid"}))
        
        with self.assertRaises(pyCAPS.CAPSError) as e:
            myAnalysis.preAnalysis()
            self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")
        
if __name__ == '__main__':
    unittest.main()
