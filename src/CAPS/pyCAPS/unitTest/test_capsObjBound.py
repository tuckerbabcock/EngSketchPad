import unittest

import os

from sys import version_info as pyVersion
from sys import version_info

from pyCAPS import caps

class TestBound(unittest.TestCase):

    @classmethod
    def setUpClass(cls):

        cls.file = "unitGeom.csm"
        cls.analysisDir = "UnitTest"
        cls.iDir = 1

        cls.boundName = "Upper_Left"

    @classmethod
    def tearDownClass(cls):

        # Remove analysis directories
        if os.path.exists(cls.analysisDir):
            os.rmdir(cls.analysisDir)

        for i in range(1, cls.iDir):
            if os.path.exists(cls.analysisDir+str(i)):
                os.rmdir(cls.analysisDir+str(i))

        # Remove created files
        if os.path.isfile("unitGeom.egads"):
            os.remove("unitGeom.egads")

#=============================================================================-
    # Test bound creation
    def test_boundInit(self):

        problem = caps.open("TestProblem", self.file)
        problem.setOutLevel(0)

        fun3d = problem.makeAnalysis("fun3dAIM", name = None, unitSys = None,
                                     analysisDir = self.analysisDir + str(self.iDir)); self.__class__.iDir += 1

        astros = problem.makeAnalysis("astrosAIM", name = None, unitSys = None,
                                      analysisDir = self.analysisDir + str(self.iDir)); self.__class__.iDir += 1

        bound = problem.makeBound(2, self.boundName)
        
        vset_fun3d  = bound.makeVertexSet(fun3d)
        vset_astros = bound.makeVertexSet(astros)

        vset_fun3d.makeDataSet("Pressure", caps.dMethod.Analysis, 1)
        vset_astros.makeDataSet("Pressure", caps.dMethod.Conserve, 1)

        dset_src = vset_astros.makeDataSet("Displacement", caps.dMethod.Analysis, 3)
        dset_dst = vset_fun3d.makeDataSet("Displacement", caps.dMethod.Conserve, 3)

        bound.completeBound()

        dset_dst.initDataSet([0,0,0])

if __name__ == '__main__':
    unittest.main() 
