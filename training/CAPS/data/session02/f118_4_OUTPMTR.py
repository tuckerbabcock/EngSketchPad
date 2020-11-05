#------------------------------------------------------------------------------#G

# Allow print statement to be compatible between Python 2 and 3
from __future__ import print_function

# Import capsProblem class from pyCAPS
from pyCAPS import capsProblem

#------------------------------------------------------------------------------#

# Initialize capsProblem object
myProblem = capsProblem()

# Load geometry [.csm] file
filename = "f118-A.csm"
print ('\n==> Loading geometry from file "'+filename+'"...')
f118 = myProblem.loadCAPS(filename)

# Build and print all available output parameters
print ("--> wing:wet      =", f118.getGeometryOutVal("wing:wet"    ) )
print ("--> wing:volume   =", f118.getGeometryOutVal("wing:volume" ) )
print ()
htail_prop = f118.getGeometryOutVal("htail:prop")
print ("--> htail:prop[0] =", htail_prop[0] )
print ("--> htail:prop[1] =", htail_prop[1] )
print ()
# Accessing OUTPMTR that has not been set
print ("--> vtail:prop    =", f118.getGeometryOutVal("vtail:prop"  ) )
print ()
print ("--> fuse:wet      =", f118.getGeometryOutVal("fuse:wet"    ) )
print ("--> fuse:volume   =", f118.getGeometryOutVal("fuse:volume" ) )
print ()
# Attempt to get a SET value not defined as OUTPMTR (causes an error)
print ("--> wing:span     =", f118.getGeometryOutVal("wing:span"   ) )
