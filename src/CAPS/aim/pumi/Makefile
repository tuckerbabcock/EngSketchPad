#
ifndef ESP_ROOT
$(error ESP_ROOT must be set -- Please fix the environment...)
endif
#
ifndef PUMI
$(error PUMI must be set -- Please fix the environment...)
endif
#
IDIR  = $(ESP_ROOT)/include
include $(IDIR)/$(ESP_ARCH)
LDIR  = $(ESP_ROOT)/lib
ifdef ESP_BLOC
ODIR  = $(ESP_BLOC)/obj
else
ODIR  = .
endif

CXX = mpicxx

PUMI_BUILD := pumi_build
PUMI_INSTALL := $(PUMI_BUILD)/install
PUMI_LDIR := $(PUMI_INSTALL)/lib
PUMI_IDIR := $(PUMI_INSTALL)/include

$(LDIR)/pumiAIM.so: $(ODIR)/pumiAIM.o \
			$(LDIR)/libutils.a $(LDIR)/libaimUtil.a $(OBJS) $(OBJSP)
	$(CXX) $(SOFLGS) -o $(LDIR)/pumiAIM.so $(ODIR)/pumiAIM.o \
		$(OBJS) $(OBJSP) -L$(PUMI_LDIR) -L$(LDIR) -lutils \
		-laimUtil -locsm -legads -ludunits2 $(RPATH) $(CPPSLB) -lm \
		-lapf -lapf_zoltan -lcrv -lgmi -lgmi_egads -llion -lma -lmds -lmth \
		-lparma -lpcu -lph -lpumi -lsam -lspr -Wl,-rpath,$(PUMI_LDIR) \
		-lapf -lapf_zoltan -lcrv -lgmi -lgmi_egads -llion -lma -lmds -lmth \
		-lparma -lpcu -lph -lpumi -lsam -lspr -Wl,-rpath,$(PUMI_LDIR) 

$(ODIR)/pumiAIM.o: pumi pumiAIM.cpp $(IDIR)/capsTypes.h ../utils/meshUtils.h \
			 $(PUMI_IDIR)/apf.h $(PUMI_IDIR)/apfMesh.h $(PUMI_IDIR)/apfMesh2.h \
			 $(PUMI_IDIR)/apfMDS.h $(PUMI_IDIR)/crv.h $(PUMI_IDIR)/gmi.h \
			 $(PUMI_IDIR)/gmi_egads.h $(PUMI_IDIR)/pcu_util.h
	$(CXX) -c $(CPPOPT) $(DEFINE) -I$(IDIR) -I$(PUMI_IDIR) -I../utils pumiAIM.cpp \
		-o $(ODIR)/pumiAIM.o

# CAPS compiles PUMI library to use the latest egads.h file
.PHONY: pumi
pumi: 
	./pumi_build.sh $(ESP_ROOT) $(PUMI) $(PUMI_BUILD) $(PUMI_INSTALL)

clean:
	-rm -f $(ODIR)/pumiAIM.o $(OBJS) $(OBJSP)

cleanall:	clean
	-rm -f $(LDIR)/pumiAIM.so

dox:
	(cd doc; doxygen pumiAIM_dox.cfg; cd latex; make; mv refman.pdf pumiAIM.pdf)

doxclean:
	(cd doc; rm -f INPUT; rm -rf html latex; rm -f pumiAIM.tag)
