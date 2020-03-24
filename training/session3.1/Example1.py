# *******************************************************************
# *                                                                 *
# * Example1 -- examples for Introduction to Python                 *
# *                                                                 *
# *              Written by John Dannenhoffer @ Syracuse University *
# *                                                                 *
# *******************************************************************

from __future__ import print_function
from math import sqrt, floor


# ******************************************************************
# *                                                                *
# *   main - main program                                          *
# *                                                                *
# ******************************************************************

def Example1():
    """
    example program for Example1

    inputs:
        none
    output:
        none

    Written by John Dannenhoffer
    """

    # simple assignments
    i   = 7                        # 7
    j   = 3                        # 3
    ONE = 1                        # 1
    TWO = 2                        # 2
    a = 7.0                        # 7.0
    b = 3.0                        # 3.0
    c = 2.0                        # 2.0

#    order of operations (highest to lowest):
#       (...), [...], {...}    tuple, list, dictionary creation
#       '...'                  string conversion
#       S[i], S[i:j]           indexing and slicing
#       s.attr                 attributes
#       f(...)                 function calls
#       +x, -x, ~x             unary operators
#       x**y                   power (Right-to-left)
#       x*y, x/y, x//y, x%y    multiplication, division, floor, modulo
#       x+y, x-y               addition, subtraction
#       x<<y, x>>y             bit shifting
#       x&y                    bitwise and
#       x^y                    bitwise exclusive or (xor)
#       x|y                    bitwise or
#       x<y, x<=y, x>y, x>=y   comparison, identity, sequence membership test
#       x==y, x!=y
#       x<>y
#       x is y, x is not y
#       x in S, x not in S
#       not x                  logical negation
#       x and y                logical and
#       x or y                 logical or
#       lambda args: expr      anonymous function

    # expressions
    k = i + j                      # k=10
    print('k = ' + repr(k))

    k = i - j                      # k=4
    print('k = ' + repr(k))

    k = i * j                      # k=21
    print('k = ' + repr(k))

    k = i / j                      # k=2     (in python 2)
                                   # k=2.333 (in python 3)
    print('k = ' + repr(k))

    k = floor(i / j)               # k=2.0
    print('k = ' + repr(k))

    k = i // j                     # k=2
    print('k = ' + repr(k))

    k = i % j                      # k=1
    print('k = ' + repr(k))

    k = pow(i, 2)                  # k=49
    print('k = ' + repr(k))

    k = i ** 2                     # k=49
    print('k = ' + repr(k))

    k += 3                         # k=52
    print('k = ' + repr(k))

    c = a + b                      # c=10.0
    print('c = ' + repr(c))

    c = a - b                      # c=4.0
    print('c = ' + repr(c))

    c = a * b                      # c=21.0
    print('c = ' + repr(c))

    c = a / b                      # c=2.3333
    print('c = ' + repr(c))

    c = pow(a, 2)                  # c=49.0
    print('c = ' + repr(c))

    c = a ** 2                     # c=49.0
    print('c = ' + repr(c))

    d = 1/2 * a                    # d=0.0    (in python 2)
                                   # d=3.5    (in python 3)
    print('d = ' + repr(d))

    d = 0.5 * a                    # d=3.5
    print('d = ' + repr(d))

    d = 1/2. * a                   # d=3.5
    print('d = ' + repr(d))

    d = ONE/float(TWO) * a         # d=3.5
    print('d = ' + repr(d))

    # quadratic formula
    a = 4.
    b = 8.
    c = 2.

    NROOT = 8
    roots = [None] * NROOT         # array of length NROOT

    roots[0] = (-b + sqrt(b**2 - 4*a*c)) / (2*a)
    roots[1] = (-b - sqrt(b**2 - 4*a*c)) / (2*a)

    # simple logic block
    if (a > b and c == 2):
        d = 1
    elif (b != 3):
        d = 2                      # d=2.0 (this gets executed)
    else:
        d = 3
    print('d = ' + repr(d))

    # sample loop
    for i in range(0, NROOT):
        roots[i] = float(i + 1)

        if (i == 4):
            continue              # skip to next i and top of loop
        elif (i > 6):
            break                 # exit loop

        # print out:
        #       roots[0] = 1.0
        #       roots[1] = 2.0
        #       roots[2] = 3.0
        #       roots[3] = 4.0
        #       roots[5] = 6.0
        #       roots[6] = 7.0
        #
        print('roots[' + repr(i) + '] = ' + repr(roots[i]))

    return

# *******************************************************************

# get help on the functions in this file
help(Example1)

# run the tests
print('Run Example1:\n')
Example1()

print('\nDone')
