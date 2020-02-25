#
IDIR  = $(ESP_ROOT)/include
include $(IDIR)/$(ESP_ARCH)
LDIR  = $(ESP_ROOT)/lib
BDIR  = $(ESP_ROOT)/bin
ifdef ESP_BLOC
ODIR  = $(ESP_BLOC)/obj
else
ODIR  = .
endif

VPATH = $(ODIR)

OBJS  =	meshUtils.o cfdUtils.o miscUtils.o feaUtils.o vlmUtils.o nastranUtils.o tecplotUtils.o
OBJSP =	vlmSpanSpace.o

$(LDIR)/libutils.a:	$(OBJS) $(OBJSP)
	touch $(LDIR)/libutils.a
	rm $(LDIR)/libutils.a
	(cd $(ODIR); ar -rs $(LDIR)/libutils.a $(OBJS) $(OBJSP) )

$(OBJS): %.o:	%.c 
	$(CC) -c $(COPTS) $(DEFINE) -I../include -I$(IDIR) -I. $< -o $(ODIR)/$@

$(OBJSP): %.o:	%.cpp
	$(CXX) -c $(CPPOPT) $(DEFINE) -I../include -I$(IDIR) -I. $< -o $(ODIR)/$@

clean:
	-(cd $(ODIR); rm $(OBJS) $(OBJSP))

cleanall:	clean
	-rm $(LDIR)/libutils.a
