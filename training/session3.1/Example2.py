# *******************************************************************
# *                                                                 *
# * Example2 -- examples for Introduction to Python                 *
# *                                                                 *
# *              Written by John Dannenhoffer @ Syracuse University *
# *                                                                 *
# *******************************************************************

from __future__ import print_function
from math import sqrt

# ******************************************************************

def Example2():
    """
    example program for Example 2

    inputs:
        none
    output:
        none

    Written by John Dannenhoffer
    """

    # functions are called by value
    leg1 = 3
    leg2 = 4
    hyp  = hypot2(leg1, leg2)
    print('hyp = ' + repr(hyp))

    val1 = 3
    val2 = 4
    val1, val2 = swapInt(val1, val2)
    print('val1 = ' + repr(val1) + '   val2 = ' + repr(val2))

    # quadratic formulae
    ans = quadform( 2, 2, -12)
    print('ans = ' + repr(ans))

    ans = quadform( 2, 2,  12)
    print('ans = ' + repr(ans))

    ans = quadform( 4, -12, -12)
    print('ans = ' + repr(ans))

    ans = quadform( 2, 0, -18)
    print('ans = ' + repr(ans))

    ans = quadform( 2, 0, 0)
    print('ans = ' + repr(ans))

    ans = quadform( 0, 2, -6)
    print('ans = ' + repr(ans))

    ans = quadform( 0, 2, 0)
    print('ans = ' + repr(ans))

    ans = quadform(0, 0, 3)
    print('ans = ' + repr(ans))

    # tuples, lists, and dictionaries
    tuple1 = (1, 2, 3)               # tuples are immutable
    print('tuple1='+repr(tuple1))    # outputs (1, 2, 3)
    print(' ')

    tuple2 = appendToSeq(tuple1)
    print('tuple1='+repr(tuple1))    # outputs (1, 2, 3)
    print('tuple2='+repr(tuple2))    # outputs (1, 2, 3, 9, 9, 9)
    print(' ')

    try:
        tuple3 = changeFirst(tuple1) # causes TypeError since tuple is immutable
    except TypeError:
        print('error in changeFirst(tuple1)')
    print(' ')

    list1  = [1, 2, 3]               # lists  are   mutable
    print('list1 ='+repr(list1))     # outputs [1, 2, 3]
    print(' ')

    list2  = appendToSeq(list1)      # list1 and list2 point to same memory
    print('list1 ='+repr(list1))     # outputs [1, 2, 3, 9, 9, 9]
    print('list2 ='+repr(list2))     # outputs [1, 2, 3, 9, 9, 9]
    print(' ')

    list3  = changeFirst(list1)      # list1, list2, and list3 all point to same memory
    print('list1 ='+repr(list1))     # outputs [0, 2, 3, 9, 9, 9]
    print('list2 ='+repr(list2))     # outputs [0, 2, 3, 9, 9, 9]
    print('list3 ='+repr(list3))     # outputs [0, 2, 3, 9, 9, 9]
    print(' ')

    list4  = list1[:]                # list4 is a separate list
    print('list1 ='+repr(list1))     # outputs [0, 2, 3, 9, 9, 9]
    print('list2 ='+repr(list2))     # outputs [0, 2, 3, 9, 9, 9]
    print('list3 ='+repr(list3))     # outputs [0, 2, 3, 9, 9, 9]
    print('list4 ='+repr(list4))     # outputs [0, 2, 3, 9, 9, 9]
    print(' ')

    list4[2] = 22                    # only list4 changes
    print('list1 ='+repr(list1))     # outputs [0, 2, 3, 9, 9, 9]
    print('list2 ='+repr(list2))     # outputs [0, 2, 3, 9, 9, 9]
    print('list3 ='+repr(list3))     # outputs [0, 2, 3, 9, 9, 9]
    print('list4 ='+repr(list4))     # outputs [0, 2, 22, 9, 9, 9]
    print(' ')

    dict1  = {'Name': 'John', 'Age': 21, 'Favs': (1, 17, 47)}
    print('dict1 ='+repr(dict1))     # output {'Age': 21, 'Favs': (1, 17, 47), 'Name': 'John'}

    dict1['Age'] = 22                # updates Age
    dict1['School'] = 'Syracuse'     # appends School
    print('dict1 ='+repr(dict1))     # output {'Age': 22, 'Favs': (1, 17, 47), 'Name': 'John', 'School': 'Syracuse'}

    del dict1['Favs']                # removes entry with key Favs
    dict1.clear()                    # removes all entries in dict
    del dict1                        # deletes entire dictionary

    dict2  = {'Name': 'John', 'Age': 21, 'Favs': (1, 17, 47)}
    print('len ='+repr(len(dict2)))  # outputs 3
    print('str ='+str(dict2))        # outputs {'Age': 21, 'Favs': (1, 17, 47), 'Name': 'John'}

    # pseudo-2D array (stored across the rows)
    NROW = 3
    NCOL = 4

    mat1 = [None] * (NROW*NCOL)

    for i in range(0, NROW):
        for j in range(0, NCOL):
            mat1[(i)*NCOL+(j)] = i * 10 + j

    # square the diagonal entries
    squareDiag(mat1, NROW, NCOL)

    # print out matrix
    k = 0
    for i in range(0, NROW):
        for j in range(0, NCOL):
            print('mat1('+repr(i)+','+repr(j)+') = '+repr(mat1[k]))
            k += 1
    print(' ')

    # variable scope
    one = 1
    two = 2
    print('in Example2: one='+repr(one)+'   two='+repr(two))

    varscope()

    print('in Example2: one='+repr(one)+'   two='+repr(two))

    return

# ******************************************************************

def hypot2(x,
           y):
    """
    hyponentuse of right triangle

    inputs:
        leg1
        leg2
    output:
        hypeneuse
    """

    return sqrt(x * x + y * y)

# ******************************************************************

def swapInt(a,
            b):
    """
    swap two numbers

    inputs:
        num1
        num2
    output:
        (num2, num1)
    """

    return (b, a)

# ******************************************************************

def quadform(a,
             b,
             c):
    """
    quadratic formula: a*x^2+b*x+c=0

    inputs:
        a
        b
        c
    output:
        (-3)          no roots
        (-2)          complex roots - not returned
        (-1, x1)      linear equation
        ( 0, x1, x2)  roots of quadratic
    """

    # quadratic
    if (abs(a) > 1e-16):
        det = b * b - 4 * a * c
        if (det < 0):
            return -2

        x1 = (-b + sqrt(det)) / (2 * a)
        x2 = (-b - sqrt(det)) / (2 * a)

        return (0, x1, x2)

    # linear
    elif (abs(b) > 1e-16):
        x1 = -c / b
        return (-1, x1)

    # no solution
    else:
        return -3

# ******************************************************************

def appendToSeq(myseq):
    """
    append 9,9,9 to sequence

    inputs:
        myseq
    output:
        myseq
    """

    myseq += (9, 9, 9)
    return myseq

# ******************************************************************

def changeFirst(myseq):
    """
    change first element to 0

    inputs:
        myseq
    output:
        myseq
    """

    myseq[0] = 0
    return myseq

# ******************************************************************

def squareDiag(A,
               nrow,
               ncol):
    """
    square diagonal elements

    inputs:
        A
        nrow
        ncol
    output:
        none
    """

    for i in range(0, min(nrow,ncol)):
        A[(i)*ncol+(i)] = pow(A[(i)*ncol+(i)], 2)

# ******************************************************************

def varscope():
    """"
    demonstrate variable scope

    inputs:
        none
    output:
        none
    """

    one = -1
    two = -2

    print('in varscope: one='+repr(one)+'   two='+repr(two))

    def funscope():
        one = 11
        two = 22

        print('in funscope: one='+repr(one)+'   two='+repr(two))

    funscope()

    print('in varscope: one='+repr(one)+'   two='+repr(two))

# *******************************************************************

# get help on the functions in this file
help(Example2)

# run the tests
print('Run Example2:\n')
Example2()

print('\nDone')
