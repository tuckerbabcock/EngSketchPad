import unittest

import os
import math

from pyCAPS import caps

class TestUnit(unittest.TestCase):

#=============================================================================-
    def test_convert(self):
        
        m1k = caps.Units(1000, "m")
        km  = m1k.convert("km")

        self.assertEqual(km._value, 1)
        self.assertEqual(km._units, "km")

#=============================================================================-
    def test_compare(self):

        m1k = caps.Units(1000, "m")
        km  = caps.Units(1   , "km")
        kg  = caps.Units(1   , "kg")

        self.assertEqual(m1k, km)
        self.assertNotEqual(kg, km)

#=============================================================================-
    def test_binaryOp(self):
        
        s   = caps.unit("s")
        s2  = caps.unit("s^2")
        m   = caps.unit("m")
        km  = caps.unit("km")
        kg  = caps.unit("kg")
        kgm = caps.unit("kg.m")
        N   = caps.unit("N")

        self.assertEqual(kgm, kg*m)
        self.assertEqual(s2, s**2)

        self.assertEqual(N, kg*m/s**2)

        m2  = caps.Units(2, "m")
        m4  = caps.Units(4, "m")
        self.assertEqual(m2, m+m)
        self.assertEqual(m, m2-m)
        self.assertEqual(m2, 2*m)
        self.assertEqual(m, m2/2)

        with self.assertRaises(caps.CAPSError) as e:
            bad = m+s
        self.assertEqual(e.exception.errorName, "CAPS_UNITERR")

        with self.assertRaises(caps.CAPSError) as e:
            bad = m-s
        self.assertEqual(e.exception.errorName, "CAPS_UNITERR")

        self.assertEqual(1, m/m)
        self.assertEqual(1000, km/m)
        self.assertEqual(1, m*m**-1)
        self.assertEqual(1000, km*m**-1)

#=============================================================================-
    def test_unaryOp(self):
        m   = caps.unit("m")
        nm  = caps.Units(-1, "m")

        self.assertEqual(nm, -m)
        self.assertEqual( m, +m)

#=============================================================================-
    def test_inpaceOp(self):
        m  = caps.unit("m")
        s  = caps.unit("s")

        unit = caps.unit("m")

        unit += m
        self.assertEqual(2*m, unit)
        unit -= m
        self.assertEqual(  m, unit)
        unit /= 2*s
        self.assertEqual(0.5*m/s, unit)
        unit **= 2
        self.assertEqual((0.5*m/s)**2, unit)

#=============================================================================-
    def test_angle(self):
        rad = caps.unit("rad")
        deg = caps.unit("degree")
        
        self.assertAlmostEqual(deg/rad, math.pi/180., 5)
        self.assertAlmostEqual(math.sin(5*deg), math.sin(5*deg/rad), 5)

#=============================================================================-
    def test_list(self):
        m  = caps.unit("m")
        s  = caps.unit("s")

        lengths = [3, 4]*m
        
        self.assertEqual(lengths[0], 3*m)
        self.assertEqual(lengths[1], 4*m)

        speeds = lengths/(2.*s)

        self.assertEqual(speeds[0], 1.5*m/s)
        self.assertEqual(speeds[1], 2.0*m/s)

if __name__ == '__main__':
    unittest.main() 
