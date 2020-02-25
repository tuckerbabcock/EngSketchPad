# *******************************************************************
# *                                                                 *
# * Example3 -- examples for Introduction to Python                 *
# *                                                                 *
# *              Written by John Dannenhoffer @ Syracuse University *
# *                                                                 *
# *******************************************************************

from __future__ import print_function
from   copy     import deepcopy

# ******************************************************************

# global definitions
PI = 3.1415926535897931159979635

# ******************************************************************

def Example3():
    """
    example program for Example 3

    inputs:
        none
    output:
        none

    Written by John Dannenhoffer
    """

    # get an array of NBOX Boxes
    NBOX = 10
    myBoxes = [Box() for count in range(NBOX)]

    # set the size of the Boxes
    i = 0
    for thisBox in myBoxes:
        thisBox.length = i
        thisBox.height = i*i
        thisBox.update()
        i += 1

    # print box info
    for i in range(0, len(myBoxes)):
        print('myBoxes[ ' + format(i, '5d') + ']' +
              '  .length='   + format(myBoxes[i].length,  '8.2f') +
              '  .height='   + format(myBoxes[i].height,  '8.2f') +
              '  .perim='    + format(myBoxes[i].perim(), '8.2f') +
              '  .area='     + format(myBoxes[i].area(),  '8.2f') +
              '  .centroid=' + repr(  myBoxes[i].centroid))
    print('')

    # make another reference to myBoxes
    myBoxes1 = myBoxes

    # make a deep copy of myBoxes
    myBoxes2 = deepcopy(myBoxes)

    # change one of the Boxes
    myBoxes[3].length = 0     # note: this will not update centroid

    # print box info
    for i in range(0, len(myBoxes)):
        print('myBoxes[ ' + format(i, '5d') + ']' +
              '  .length='   + format(myBoxes[i].length,  '8.2f') +
              '  .height='   + format(myBoxes[i].height,  '8.2f') +
              '  .perim='    + format(myBoxes[i].perim(), '8.2f') +
              '  .area='     + format(myBoxes[i].area(),  '8.2f') +
              '  .centroid=' + repr(  myBoxes[i].centroid))
    print('')

    for i in range(0, len(myBoxes1)):
        print('myBoxes1[' + format(i, '5d') + ']' +
              '  .length='   + format(myBoxes1[i].length,  '8.2f') +
              '  .height='   + format(myBoxes1[i].height,  '8.2f') +
              '  .perim='    + format(myBoxes1[i].perim(), '8.2f') +
              '  .area='     + format(myBoxes1[i].area(),  '8.2f') +
              '  .centroid=' + repr(  myBoxes1[i].centroid))
    print('')

    for i in range(0, len(myBoxes2)):
        print('myBoxes2[' + format(i, '5d') + ']' +
              '  .length='   + format(myBoxes2[i].length,  '8.2f') +
              '  .height='   + format(myBoxes2[i].height,  '8.2f') +
              '  .perim='    + format(myBoxes2[i].perim(), '8.2f') +
              '  .area='     + format(myBoxes2[i].area(),  '8.2f') +
              '  .centroid=' + repr(  myBoxes2[i].centroid))
    print('')

    # get an array of NCIRCLE Circles
    NCIRCLE = 5
    myCircles = [Circle(i+4) for i in range(NCIRCLE)]

    # print circle info
    i = 0
    for thisCircle in myCircles:
        print('myCircles[' + format(i, '5d') + ']' +
              '  .radius='   + format(thisCircle.radius,   '8.2f') +
              '  .diam='     + format(thisCircle.diam(),   '8.2f') +
              '  .circum='   + format(thisCircle.circum(), '8.2f') +
              '  .area='     + format(thisCircle.area(),   '8.2f'))
        i += 1

# ******************************************************************

class Box:
    """
    A box anchored at origin
    """

    # length        -> length (in x)
    # height        -> height (in y)
    # perim()       -> perimeter
    # area()        -> area
    # update()      -> None              (updates centroid)
    # centroidc     -> (xcent, ycent)

    def __init__(self, *args):
        if (len(args) < 1):
            self.length = 0.0
        else:
            self.length = float(args[0])
        if (len(args) < 2):
            self.height = 0.0
        else:
            self.height = float(args[1])
        self.centroid = (0.0, 0.0)

    def perim(self):
        return 2 * (self.length + self.height)

    def area(self):
        return self.length * self.height

    def update(self):
        self.centroid = (self.length/2.0, self.height/2.0)

# ******************************************************************

class Circle:
    """
    A circle centered at origin
    """

    # radius        -> radius
    # diam()        -> diameter
    # circum()      -> circumference
    # area()        -> area

    def __init__(self, *args):
        if (len(args) < 1):
            self.radius = 0.0
        else:
            self.radius = float(args[0])

    def diam(self):
        return (2.0      * self.radius)

    def circum(self):
        return (2.0 * PI * self.radius)

    def area(self):
        return (      PI * self.radius * self.radius)

# ******************************************************************

# get help on the functions in this file
help(Example3)
help(Box)
help(Circle)

# run the tests
print('Run Example3:\n')
Example3()

print('\nDone')
