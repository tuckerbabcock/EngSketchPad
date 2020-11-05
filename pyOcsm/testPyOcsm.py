###################################################################
#                                                                 #
# testPyOcsm --- test program to exercise pyOcsm                  #
#                                                                 #
#              Written by John Dannenhoffer @ Syracuse University #
#                                                                 #
###################################################################

import os
from pyOcsm import Ocsm, OcsmError

# get help on Ocsm and OcsmError
help(Ocsm)

# first MODL

# create MODL structure
modl1 = Ocsm()

print("\ncalling modl1.Version()")
(imajor, iminor) = modl1.Version()
print("    imajor :", imajor)
print("    iminor :", iminor)

print("\ncalling modl1.Load(sample.csm)")
modl1.Load("sample.csm")

print("\ncalling modl1.LoadDict(sample.dict)")
modl1.LoadDict("sample.dict")

print("\ncalling modl1.Check()")
modl1.Check()

print("\ncalling modl1.GetFilelist()")
filelist = modl1.GetFilelist()
print("    filelist:", filelist)

print("\ncalling modl1.Build(0, 0)")
(builtTo, nbody, bodys) = modl1.Build(0, 0)
print("    builtTo:", builtTo)
print("    nbody  :", nbody  )
print("    bodys  :", bodys  )
assert (nbody == 0)

print("\ncalling modl1.Info()")
(nbrch, npmtr, nbody) = modl1.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)
assert (nbody == 3)

print("\ncalling modl1.PrintBodys()")
modl1.PrintBodys("")

print("\ncalling modl1.Copy()")
modl2 = modl1.Copy()
print("    modl2  :", modl2)

if (os.path.exists("sample.out")):
    os.remove("sample.out")

print("\ncalling modl2.PrintPmtrs(sample.out)")
f = open("sample.out", "w")
f.write("\ncalling modl2.PrintPmtrs\n")
f.close()
modl2.PrintPmtrs("sample.out")

print("\ncalling modl2.PrintAttrs(sample.out)")
f = open("sample.out", "a")
f.write("\ncalling modl2.PrintAttrs\n")
f.close()
modl2.PrintAttrs("sample.out")

print("\ncalling modl2.PrintBrchs(sample.out)")
f = open("sample.out", "a")
f.write("\ncalling modl2.PrintBrchs\n")
f.close()
modl2.PrintBrchs("sample.out")

print("\ncalling modl2.FindPmtr(rad)")
irad = modl2.FindPmtr("rad", 0, 0, 0)
print("    irad  :", irad)

print("\ncalling modl2.SetValuD(irad, 1, 1, 0.5)")
modl2.SetValuD(irad, 1, 1, 0.5)

print("\ncalling modl2.Build(0, 20)")
modl2.SetOutLevel(0)
(builtTo, nbody, bodys) = modl2.Build(0, 20)
modl2.SetOutLevel(1)
print("    builtTo:", builtTo)
print("    nbody  :", nbody)
print("    bodys  :", bodys)
assert (nbody == 2)

print("\ncalling modl2.Info()")
(nbrch, npmtr, nbody) = modl2.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)
assert (nbody == 4)

print("\ncalling modl2.SaveDespmtrs(sample.despmtrs)")
modl2.SaveDespmtrs("sample.despmtrs")

print("\ncalling modl2.Free()")
modl2.Free()

# this should cause an error
print("\ncalling modl2.Info() --- after modl2.Free")
try:
    (nbrch, npmtr, nbody) = modl2.Info()
except OcsmError as error:
    print("    OcsmError rasied (as expected)")
else:
    raise Exception

print("\ncalling modl1.Info()")
(nbrch, npmtr, nbody) = modl1.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)
assert (nbody == 3)

print("\ncalling modl1.UpdateDespmtrs(sample.despmtrs)")
modl1.UpdateDespmtrs("sample.despmtrs")

print("\ncalling modl1.Build(0, 0)")
modl1.SetOutLevel(0)
(builtTo, nbody, bodys) = modl1.Build(0, 0)
modl1.SetOutLevel(1)
print("    builtTo:", builtTo)
print("    nbody  :", nbody  )
print("    bodys  :", bodys  )
assert (nbody == 0)

print("\ncalling modl1.Info()")
(nbrch, npmtr, nbody) = modl1.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)
assert (nbody == 4)

print("\ncalling modl1.FindPmtr(oper)")
ioper = modl1.FindPmtr("oper", 0, 0, 0)
print("    ioper  :", ioper)

print("\ncalling modl1.SetValu(ioper, 1, 1, 5-4)")
modl1.SetValu(ioper, 1, 1, "5-4")

print("\ncalling modl1.Build(0, 20)")
modl1.SetOutLevel(0)
(builtTo, nbody, bodys) = modl1.Build(0, 20)
modl1.SetOutLevel(1)
print("    builtTo:", builtTo)
print("    nbody  :", nbody  )
print("    bodys  :", bodys  )
assert (nbody == 1)

print("\ncalling modl1.Info()")
(nbrch, npmtr, nbody) = modl1.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)
assert (nbody == 5)

print("\ncalling modl1.EvalExpr(dx*dy*dz)")
(value, dot, str) = modl1.EvalExpr("dx*dy*dz")
print("    value  :", value)
print("    dot    :", dot  )
print("    str    :", str  )
assert (value == 24)
assert (dot   == 0 )
assert (str   == "")

print("\ncalling modl1.EvalExpr(val2str(3.1415926,4))")
(value, dot, str) = modl1.EvalExpr("val2str(3.1415926,4)")
print("    value  :", value)
print("    dot    :", dot  )
print("    str    :", str  )
assert (str == "3.1416")

print("\ncalling modl1.Save(sample2.cpc)")
modl1.Save("sample2.cpc")

print("\ncalling modl2.Load(sample2.cpc)")
modl2 = Ocsm()
modl2.Load("sample2.cpc")

print("\ncalling modl2.LoadDict(sample.dict)")
modl2.LoadDict("sample.dict")

print("\ncalling modl2.Check()")
modl2.Check()

print("\ncalling modl2.Build(0, 0)")
(builtTo, nbody, bodys) = modl2.Build(0, 0)
print("    builtTo:", builtTo)
print("    nbody  :", nbody  )
print("    bodys  :", bodys  )
assert (nbody == 0)

print("\ncalling modl2.Info()")
(nbrch, npmtr, nbody) = modl2.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)
assert (nbody == 5)

print("\ncalling modl2.EvalExpr(@stack.size)")
(value, dot, str) = modl2.EvalExpr("@stack[1]")
print("    value  :", value)
print("    dot    :", dot  )
print("    str    :", str  )
assert (value == 5)

print("\ncalling modl2.PrintBrep(int(value), )")
modl2.PrintBrep(int(value), "")

print("\ncalling modl2.GetBrch(1)")
(type, bclass, actv, ichld, ileft, irite, narg, nattr) = modl2.GetBrch(1)
print("    type   :", type  )
print("    bclass :", bclass)
print("    actv   :", actv  )
print("    ichld  :", ichld )
print("    ileft  :", ileft )
print("    irite  :", irite )
print("    narg   :", narg  )
print("    nattr  :", nattr )

print("\nconverting cylinder to sphere")
for ibrch in range(1, nbrch+1):
    (type, bclass, actv, ichld, ileft, irite, narg, nattr) = modl2.GetBrch(ibrch)

    if (modl2.GetText(type) == "cylinder"):
        print("    cylinder is ibrch", ibrch)

        print("    calling modl2.NewBrch(", ibrch, ", sphere x0+dx/2 y0+dy/2 z0+dz")
        modl2.NewBrch(ibrch, modl2.GetCode("sphere"), "<none>", 0,
                      "x0+dx/2", "y0+dy/2", "z0+dz", "rad", "", "", "", "", "")

        print("    calling modl2.SetName(", ibrch+1, ", sphere)")
        modl2.SetName(ibrch+1, "sphere")

        print("    calling modl2.SetAttr(", ibrch+1, ", _color $red)")
        modl2.SetAttr(ibrch+1, "_color", "$red")

        print("    calling modl2.SetAttr(", ibrch+1, ", _name $the_sphere)")
        modl2.SetAttr(ibrch+1, "_name",  "$the_sphere")

        print("    call modl2.GetAttr(", ibrch+1, ", _color)")
        avalue = modl2.GetAttr(ibrch+1, "_color")
        print("        attr  :", avalue)

        print("    getting all Attributes for ibrch=", ibrch+1)
        for iattr in range(1, 3):
            (aname, avalue) = modl2.RetAttr(ibrch+1, iattr)
            print("        ", aname, ":", avalue)

        print("    calling modl2.DelBrch(", ibrch, ")")
        modl2.DelBrch(ibrch)

print("\ncalling modl2.PrintBrchs()")
modl2.PrintBrchs("")

print("\ncalling modl2.Build(0, 0)")
modl2.SetOutLevel(0)
(builtTo, nbody, bodys) = modl2.Build(0, 0)
modl2.SetOutLevel(1)
print("    builtTo:", builtTo)
print("    nbody  :", nbody  )
print("    bodys  :", bodys  )
assert (nbody == 0)

print("\ncalling modl2.NewPmtr(Rad, DESPMTR, 1, 1)")
modl2.NewPmtr("Rad", Ocsm.DESPMTR, 1, 1)
iRad = modl2.FindPmtr("Rad", 0, 0, 0)
print("    iRad   :", iRad)

print("\ncalling modl2.SetValuD(iRad, 1, 1, 0.75)")
modl2.SetValuD(iRad, 1, 1, 0.75)

print("\ncalling modl2.SetBnds(iRad, 1, 1, -1, +2)")
modl2.SetBnds(iRad, 1, 1, -1.0, +2.0)

print("\ncalling modl2.SetValu(iRad, 1, 1, 2/3)")
modl2.SetValu(iRad, 1, 1, "2/3")

print("\ncalling modl2.GetBnds(iRad, 1, 1)")
(lbound, ubound) = modl2.GetBnds(iRad, 1, 1)
print("    lbound :", lbound)
print("    ubound :", ubound)

print("\ncalling modl2.PrintPmtrs()")
modl2.PrintPmtrs("")

print("\ncalling modl2.Info()")
(nbrch, npmtr, nbody) = modl2.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)

print("\nconverting 4th arg of sphere to Rad")
for ibrch in range(1, nbrch+1):
    if (modl2.GetName(ibrch) == "sphere"):

        print("    calling modl2.GetArg(", ibrch, ", 4)")
        (defn, value, dot) = modl2.GetArg(ibrch, 4)
        print("        defn :", defn )
        print("        value:", value)
        print("        dot  :", dot  )

        print("    calling modl2.SetArg(", ibrch, ", 4, Rad")
        modl2.SetArg(ibrch, 4, "Rad")

        print("    calling modl2.GetArg(", ibrch, ", 4)")
        (defn, value, dot) = modl2.GetArg(ibrch, 4)
        print("        defn :", defn )
        print("        value:", value)
        print("        dot  :", dot  )

print("\ncalling modl2.NewBrch(nbrch, point 5  5  5")
modl2.NewBrch(nbrch, modl2.GetCode("point"), "<none>", 0,
              "5", "5", "5", "", "", "", "", "", "")

print("\ncalling modl2.Build(0, 0)")
modl2.SetOutLevel(0)
(builtTo, nbody, bodys) = modl2.Build(0, 20)
modl2.SetOutLevel(1)
print("    builtTo:", builtTo)
print("    nbody  :", nbody  )
print("    bodys  :", bodys  )

print("\ncalling modl2.GetBody(bodys[0])")
(type, ichld, ileft, irite, vals, nnode, nedge, nface) = modl2.GetBody(bodys[0])
print("    type   :", type )
print("    ichld  :", ichld)
print("    ileft  :", ileft)
print("    irite  :", irite)
print("    vals   :", vals )
print("    nnode  :", nnode)
print("    nedge  :", nedge)
print("    nface  :", nface)
assert (nnode == 1)
assert (nedge == 0)
assert (nface == 0)

modl2.PrintBrchs("")

print("\ncalling modl2.SetBrch(", nbrch+1, ", SUPPRESSED)")
modl2.SetBrch(nbrch+1, modl2.SUPPRESSED)

print("\ncalling modl2.Build(0, 0)")
modl2.SetOutLevel(0)
(builtTo, nbody, bodys) = modl2.Build(0, 20)
modl2.SetOutLevel(1)
print("    builtTo:", builtTo)
print("    nbody  :", nbody  )
print("    bodys  :", bodys  )

print("\ncalling modl2.GetBody(bodys[0])")
(type, ichld, ileft, irite, vals, nnode, nedge, nface) = modl2.GetBody(bodys[0])
print("    type   :", type )
print("    ichld  :", ichld)
print("    ileft  :", ileft)
print("    irite  :", irite)
print("    vals   :", vals )
print("    nnode  :", nnode)
print("    nedge  :", nedge)
print("    nface  :", nface)
assert (nnode == 11)
assert (nedge == 18)
assert (nface ==  8)

print("\ncalling modl2.DelBrch(", nbrch+1, ")")
modl2.DelBrch(nbrch+1)

print("\ncalling modl2.FindPmtr(foo, DESPMTR, 1, 1)")
modl2.FindPmtr("foo", modl2.DESPMTR, 1, 1)

print("\ncalling modl2.Info()")
(nbrch, npmtr, nbody) = modl2.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)

print("\ncalling modl2.DelPmtr(npmtr)")
modl2.DelPmtr(npmtr)

print("\ncalling modl2.Info()")
(nbrch, npmtr, nbody) = modl2.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)

print("\nmaking list of all DESPMTRs")
for ipmtr in range(1, npmtr+1):
    (type, nrow, ncol, name) = modl2.GetPmtr(ipmtr)

    if (type == modl2.DESPMTR):
        print("   ", name, " is a", nrow, "*", ncol, "DESPMTR")

print("\nmaking list of all OUTPMTRs")
for ipmtr in range(1, npmtr+1):
    (type, nrow, ncol, name) = modl2.GetPmtr(ipmtr)

    if (type == modl2.OUTPMTR):
        print("  ", name, " is a", nrow, "*", ncol, "OUTPMTR")

print("\ncalling modl2.GetPmtr(iMyCG)")
iMyCG = modl2.FindPmtr("myCG", 0, 0, 0)
(type, nrow, ncol, name) = modl2.GetPmtr(iMyCG)

for irow in range(1, nrow+1):
    for icol in range(1, ncol+1):
        (value, dot) = modl2.GetValu(iMyCG, irow, icol)
        print("   ", name, "[", irow, ",", icol, "]: ", value, dot)

print("\ncalling modl2.GetValuS(ititle)")
ititle = modl2.FindPmtr("title", 0, 0, 0)
title  = modl2.GetValuS(ititle)
print("    title  :", title)

print("\ncalling modl2.SetCsys(1, boxCsys, csysName)")
modl2.SetCsys(1, "boxCsys", "csysName")

print("\ncalling modl2.GetCsys(1, boxCsys)")
cvalue = modl2.GetCsys(1, "boxCsys")
print("    cvalue :", cvalue)

print("\ncalling modl2.RetCsys(1, 1)")
(cname, cvalue) = modl2.RetCsys(1, 1)
print("    cname  :", cname )
print("    cvalue :", cvalue)

print("\ncalling modl2.SetCsys(1, boxCsys, )")
modl2.SetCsys(1, "boxCsys", "")

print("\ncalling modl2.GetEnt(nbody, modl2.FACE, [1,6,2])")
ient = modl2.GetEnt(nbody, modl2.FACE, [1,6,1])
print("    ient   :", ient)

print("\ngetting coordinates on Face ient")
(xx, dot, str) = modl2.EvalExpr("x0+0.1")
print("    xx     :", xx)
(yy, dot, str) = modl2.EvalExpr("y0+dy/2")
print("    yy     :", yy)
(zz, dot, str) = modl2.EvalExpr("z0+dz")
print("    zz     :", zz)

print("\ncalling modl2.GetUV(nbody, modl2.FACE, ient, 1, [xx,yy,zz])")
(uu, vv) = modl2.GetUV(nbody, modl2.FACE, ient, 1, [xx, yy, zz])
print("    uu     :", uu)
print("    vv     :", vv)

print("\ncalling modl2.GetXYZ(nbody, modl2.FACE, ient, 2, [uu,vv,uu,vv])")
(xx, yy, zz, xxx, yyy, zzz) = modl2.GetXYZ(nbody, modl2.FACE, ient, 2, [uu, vv, uu, vv])
print("    xx     :", xx )
print("    yy     :", yy )
print("    zz     :", zz )
print("    xxx    :", xxx)
print("    yyy    :", yyy)
print("    zzz    :", zzz)

print("\ncalling modl2.GetNorm(nbody, ient, 1, [uu,vv])")
(normx, normy, normz) = modl2.GetNorm(nbody, ient, 1, [uu, vv])
print("    normx  :", normx)
print("    normy  :", normy)
print("    normz  :", normz)

print("\ncalling modl2.FindPmtr(dx, 0, 0, 0)")
iz0 = modl2.FindPmtr("z0", 0, 0, 0)
print("    iz0    :", iz0)

print("\ncalling modl2.SetVel(iz0, 1, 1, 1/2)")
modl2.SetVel(iz0, 1, 1, "1/2")

print("\ncalling modl2.Build(0, 0)")
modl2.SetOutLevel(0)
modl2.Build(0, 0)
modl2.SetOutLevel(1)

print("\ncalling modl2.FindPmtr(myBbox, 0, 0, 0)")
iMyBbox = modl2.FindPmtr("myBbox", 0, 0, 0)
print("    iMyBbox:", iMyBbox)

(type, nrow, ncol, name) = modl2.GetPmtr(iMyBbox)
for irow in range(1, nrow+1):
    for icol in range(1, ncol+1):
        (value, dot) = modl2.GetValu(iMyBbox, irow, icol)
        print("   ", name, "[", irow, ",", icol, "]=", value, dot)

print("\ncalling modl2.SetVelD(0, 0, 0, 0)")
modl2.SetVelD(0, 0, 0, 0)

print("\ncalling modl2.SetVel(iz0, 1, 1, 1/2)")
modl2.SetVelD(iz0, 1, 1, 1)

print("\ncalling modl2.Build(0, 0)")
modl2.SetOutLevel(0)
modl2.Build(0, 0)
modl2.SetOutLevel(1)

print("\ncalling modl2.FindPmtr(myBbox, 0, 0, 0)")
iMyBbox = modl2.FindPmtr("myBbox", 0, 0, 0)
print("    iMyBbox:", iMyBbox)

(type, nrow, ncol, name) = modl2.GetPmtr(iMyBbox)
for irow in range(1, nrow+1):
    for icol in range(1, ncol+1):
        (value, dot) = modl2.GetValu(iMyBbox, irow, icol)
        print("   ", name, "[", irow, ",", icol, "]=", value, dot)

print("\ncalling modl1.Info()")
(nbrch, npmtr, nbody) = modl1.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)

print("\ncalling modl2.GetBody(nbody)")
(type, ichld, ileft, irite, vals, nnode, nedge, nface) = modl2.GetBody(nbody)
print("    type   :", type )
print("    ichld  :", ichld)
print("    ileft  :", ileft)
print("    irite  :", irite)
print("    vals   :", vals )
print("    nnode  :", nnode)
print("    nedge  :", nedge)
print("    nface  :", nface)

print("\ngetting modl2.GetVel(nbody, modl2.NODE, inode, 1, None)")
for inode in range(1, nnode+1):
    dxyz = modl2.GetVel(nbody, modl2.NODE, inode, 1, None)
    print("    dxyz(inode=", inode, "): ", dxyz[0], dxyz[1], dxyz[2])

print("\ncalling modl2.SetDtime(0.1)")
modl2.SetDtime(0.1)

print("\ncalling modl2.Build(0, 0)")
modl2.SetOutLevel(0)
modl2.Build(0, 0)
modl2.SetOutLevel(1)

print("\ncalling modl2.SetDtime(0.)")
modl2.SetDtime(0.)

print("\ncalling modl2.FindPmtr(myBbox, 0, 0, 0)")
iMyBbox = modl2.FindPmtr("myBbox", 0, 0, 0)
print("    iMyBbox:", iMyBbox)

(type, nrow, ncol, name) = modl2.GetPmtr(iMyBbox)
for irow in range(1, nrow+1):
    for icol in range(1, ncol+1):
        (value, dot) = modl2.GetValu(iMyBbox, irow, icol)
        print("   ", name, "[", irow, ",", icol, "]=", value, dot)

print("\ncalling modl2.SetVelD(0, 0, 0, 0)")
modl2.SetVelD(0, 0, 0, 0)

ix0 = modl2.FindPmtr("x0", 0, 0, 0)
iy0 = modl2.FindPmtr("y0", 0, 0, 0)
iz0 = modl2.FindPmtr("z0", 0, 0, 0)

print("\ncalling modl2.Perturb(3, [ix0, iy0, iz0], [1, 1, 1], [1, 1, 1], [1, 2, 3])")
modl2.Perturb(3, [ix0, iy0, iz0], [1, 1, 1], [1, 1, 1], [11, 22, 33])

print("\ncalling modl2.Perturb(0, 0, 0, 0, 0)")
modl2.Perturb(0, 0, 0, 0, 0)

print("\ncalling modl2.Info()")
(nbrch, npmtr, nbody) = modl2.Info()
print("    nbrch  :", nbrch)
print("    npmtr  :", npmtr)
print("    nbody  :", nbody)

print("\ncalling modl2.GetBody(nbody)")
(type, ichld, ileft, irite, vals, nnode, nedge, nface) = modl2.GetBody(nbody)
print("    type   :", type )
print("    ichld  :", ichld)
print("    ileft  :", ileft)
print("    irite  :", irite)
print("    vals   :", vals )
print("    nnode  :", nnode)
print("    nedge  :", nedge)
print("    nface  :", nface)

print("\ngetting npnt from tessellations")
for iedge in range(1, nedge+1):
    npnt = modl2.GetTessNpnt(nbody, modl2.EDGE, iedge)
    print("    edge", iedge, "has", npnt, "tessellation points")
for iface in range(1, nface+1):
    npnt = modl2.GetTessNpnt(nbody, modl2.FACE, iface)
    print("    face", iface, "has", npnt, "tessellation points")

print("\ncalling modl2.NewBrch(nbrch, dump, <none, 0, $sample.tess, 0, 0, ...)")
modl2.NewBrch(nbrch, modl2.GetCode("dump"), "<none>", 0,
              "$sample.tess", "0", "0", "", "", "", "", "", "")

print("\ncalling modl2.Build(0, 0)")
modl2.Build(0, 0)

print("\ncalling modl2.DelBrch(mbrch+1)")
modl2.DelBrch(nbrch+1)

print("\ncalling modl2.FindPmtr(tfact, 0, 0, 0)")
itfact = modl2.FindPmtr("tfact", 0, 0, 0)
print("    itfact :", itfact)

print("\ncalling modl2.SetValuD(itfact, 1, 1, 1)")
modl2.SetValuD(itfact, 1, 1, 1.0)

print("\ncalling modl2.Build(-1, 0)   to ensure rebuild")
modl2.Build(-1, 0)

print("\ngetting npnt from tessellations")
for iedge in range(1, nedge+1):
    npnt = modl2.GetTessNpnt(nbody, modl2.EDGE, iedge)
    print("    edge", iedge, "has", npnt, "tessellation points")
for iface in range(1, nface+1):
    npnt = modl2.GetTessNpnt(nbody, modl2.FACE, iface)
    print("    face", iface, "has", npnt, "tessellation points")

print("\ncalling modl2.Updatetess(nbody, sample.tess)")
modl2.UpdateTess(nbody, "sample.tess")

print("\ngetting npnt from tessellations")
for iedge in range(1, nedge+1):
    npnt = modl2.GetTessNpnt(nbody, modl2.EDGE, iedge)
    print("    edge", iedge, "has", npnt, "tessellation points")
for iface in range(1, nface+1):
    npnt = modl2.GetTessNpnt(nbody, modl2.FACE, iface)
    print("    face", iface, "has", npnt, "tessellation points")

print("\ncalling modl2.Free()")
modl2.Free()

print("\nremoving temp files")
os.remove("sample2.cpc")
os.remove("sample.despmtrs")
os.remove("sample.out")
os.remove("sample.tess")

print("\nNOTE:")
print("    GetTessVel   is not tested")
print("    RegMesgCB    is not tested")
print("    RegSizeCB    is not tested")
print("    GetSketch    is not tested")
print("    SolveSketch  is not tested")
print("    UpdateSketch is not tested")
print("    SetEgg       is not tested")

print("\nDone")
