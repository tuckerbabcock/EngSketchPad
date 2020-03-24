# Import other need modules
from __future__ import print_function

import unittest
import os
import argparse

# Import pyCAPS class file
import pyCAPS
from pyCAPS import capsProblem


class TestavlAIM(unittest.TestCase):

#    @classmethod
#    def setUpClass(self):
    def setUp(self):

        # Initialize capsProblem object
        self.myProblem = capsProblem()

        # Create working directory variable
        self.workDir = "workDir_avlAnalysisTest"
        #workDir = os.path.join(str(args.workDir[0]), workDir)

        # Load CSM file
        self.myGeometry = self.myProblem.loadCAPS("../csmData/avlSections.csm")

        # Load avl aim
        self.avl = self.myProblem.loadAIM(aim = "avlAIM",
                                          analysisDir = self.workDir)

        # Set new Mach/Alt parameters
        self.avl.setAnalysisVal("Mach", 0.5)
        self.avl.setAnalysisVal("Alpha", 1.0)
        self.avl.setAnalysisVal("Beta", 0.0)

        wing = {"groupName"         : "Wing", # Notice Wing is the value for the capsGroup attribute
                "numChord"          : 8,
                "spaceChord"        : 1.0,
                "numSpanPerSection" : 12,
                "spaceSpan"         : 1.0}

        self.avl.setAnalysisVal("AVL_Surface", [("Wing", wing)])

    def run_avl(self):
        # Run AIM pre-analysis
        self.avl.preAnalysis()

        ####### Run avl ####################
        print ("\n\nRunning avl......")
        currentDirectory = os.getcwd() # Get our current working directory

        os.chdir(self.avl.analysisDir) # Move into test directory

        # Run avl via system call
        os.system("avl caps < avlInput.txt > avlOutput.txt");

        os.chdir(currentDirectory) # Move back to top directory

        # Run AIM post-analysis
        self.avl.postAnalysis()

    def test_numSpan(self):

        # Load avl aim
        avl = self.myProblem.loadAIM(aim = "avlAIM",
                                     analysisDir = self.workDir + "tmp")

        # Set new Mach/Alt parameters
        avl.setAnalysisVal("Mach", 0.5)
        avl.setAnalysisVal("Alpha", 1.0)
        avl.setAnalysisVal("Beta", 0.0)

        wing = {"groupName"         : "Wing",
                "numChord"          : 8,
                "spaceChord"        : 1.0,
                "numSpanTotal"      : 24, # Can specify both Total and PerSection
                "numSpanPerSection" : 12,
                "spaceSpan"         : 1.0}

        avl.setAnalysisVal("AVL_Surface", [("Wing", wing)])

        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()

        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

        wing = {"groupName"  : "Wing",
                "numChord"   : 8,
                "spaceChord" : 1.0,
                "numSpan"    : 24, # numSpan is depricated
                "spaceSpan"  : 1.0}

        avl.setAnalysisVal("AVL_Surface", [("Wing", wing)])

        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()

        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

    def test_alpha_custom_increment(self):

        Alpha      = [0.0, 3.0, 9.0]
        CLtotTrue  = [0.27391, 0.44792, 0.78472]
        CDtotTrue  = [0.00805, 0.02151, 0.06657]

        for i in range(0,len(Alpha)):
            # Set custom AoA
            self.avl.setAnalysisVal("Alpha", Alpha[i])

            # run avl
            self.run_avl()

            # Retrieve results
            CLtot = self.avl.getAnalysisOutVal("CLtot")
            CDtot = self.avl.getAnalysisOutVal("CDtot")
            #print("Alpha = ", Alpha[i])
            #print("CLtot = ", CLtot)
            #print("CDtot = ", CDtot)

            self.assertAlmostEqual(CLtotTrue[i], CLtot, 4)
            self.assertAlmostEqual(CDtotTrue[i], CDtot, 4)

    def test_MassProp(self):

       # Load avl aim
        avl = self.myProblem.loadAIM(aim = "avlAIM",
                                     analysisDir = self.workDir + "Mass")

       # Set new Mach/Alt parameters
        avl.setAnalysisVal("Mach", 0.5)
        avl.setAnalysisVal("Alpha", 1.0)
        avl.setAnalysisVal("Beta", 0.0)

        wing = {"groupName"         : "Wing", # Notice Wing is the value for the capsGroup attribute
                "numChord"          : 8,
                "spaceChord"        : 1.0,
                "numSpanPerSection" : 12,
                "spaceSpan"         : 1.0}

        avl.setAnalysisVal("AVL_Surface", [("Wing", wing)])

        mass = 0.1773
        x =  0.02463
        y = 0.
        z = 0.2239
        Ixx = 1.350
        Iyy = 0.7509
        Izz = 2.095

        avl.setAnalysisVal("MassProp", ("Aircraft",{"mass":[mass,"kg"], "CG":[[x,y,z],"m"], "massInertia":[[Ixx, Iyy, Izz], "kg*m^2"]}))

       # check there are errors if information is missing
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

        avl.setAnalysisVal("Lunit", 1, units="ft")
        avl.setAnalysisVal("Gravity", 32.18, units="ft/s^2")
        avl.setAnalysisVal("Density", 0.002378, units="slug/ft^3")
        avl.setAnalysisVal("Velocity", 64.5396, units="ft/s")

       # make sure there are no errsos
        avl.preAnalysis()

        avl.setAnalysisVal("MassProp", [("Aircraft",{"mass":[mass,"kg"], "CG":[[x,y,z],"m"], "massInertia":[[Ixx, Iyy, Izz, 1.0, 2.0, 3.0], "kg*m^2"]}),
                                        ("Engine",  {"mass":[mass,"kg"], "CG":[[x,y,z],"m"], "massInertia":[[Ixx, Iyy, Izz], "kg*m^2"]})])

       # again should not cause errors
        avl.preAnalysis()

       # test error handling of the mass properties parsing

        avl.setAnalysisVal("MassProp", ("Aircraft", "1"))
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

        avl.setAnalysisVal("MassProp", ("Aircraft", "foo"))
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

        avl.setAnalysisVal("MassProp", ("Aircraft", {"mass":[mass,"kg"], "CG":[[x,y,z],"m"]}))
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_NOTFOUND")

        avl.setAnalysisVal("MassProp", ("Aircraft", {"mass":(mass), "CG":[[x,y,z],"m"], "massInertia":[[Ixx, Iyy, Izz], "kg*m^2"]}))
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

        avl.setAnalysisVal("MassProp", ("Aircraft",{"mass":[mass,"kg"], "CG":[x,y,z], "massInertia":[[Ixx, Iyy, Izz], "kg*m^2"]}))
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

        avl.setAnalysisVal("MassProp", ("Aircraft",{"mass":[mass,"kg"], "CG":[[x,y,z],"m"], "massInertia":[Ixx, Iyy, Izz]}))
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

        avl.setAnalysisVal("MassProp", ("Aircraft",{"mass":("mass","kg"), "CG":[[x,y,z],"m"], "massInertia":[[Ixx, Iyy, Izz], "kg*m^2"]}))
        with self.assertRaises(pyCAPS.CAPSError) as e:
            avl.preAnalysis()
        self.assertEqual(e.exception.errorName, "CAPS_BADVALUE")

#    @classmethod
#    def tearDownClass(self):
    def tearDown(self):

        # Close CAPS - Optional
        self.myProblem.closeCAPS()

if __name__ == '__main__':
    unittest.main()
