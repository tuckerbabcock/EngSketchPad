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

OBJS  = capsBase.o  capsAnalysis.o capsApprox.o capsAttr.o capsAIM.o \
	capsBound.o capsProblem.o  capsValue.o  conjGrad.o printObject.o

default:	$(IDIR)/caps.h    $(IDIR)/capsErrors.h $(IDIR)/capsTypes.h \
		$(IDIR)/aimUtil.h $(LDIR)/libaimUtil.a $(LDIR)/testingAIM.so \
		$(LDIR)/$(CSHLIB) $(LDIR)/libcapsstatic.a

$(LDIR)/testingAIM.so:	$(ODIR)/testingAIM.o $(LDIR)/libaimUtil.a
	-rm $(LDIR)/testingAIM.so
	$(CC) $(SOFLGS) -o $(LDIR)/testingAIM.so $(ODIR)/testingAIM.o \
		-L$(LDIR) -laimUtil -legads -locsm -ludunits2 $(RPATH) -lm

$(ODIR)/testingAIM.o:	testingAIM.c ../include/capsTypes.h
	$(CC) -c $(COPTS) $(DEFINE) -I../include -I$(IDIR) testingAIM.c \
		-o $(ODIR)/testingAIM.o

$(LDIR)/libaimUtil.a:	$(ODIR)/aimUtil.o $(ODIR)/aimTransferUtil.o
	touch $(LDIR)/libaimUtil.a
	rm $(LDIR)/libaimUtil.a
	ar -rs $(LDIR)/libaimUtil.a $(ODIR)/aimUtil.o $(ODIR)/aimTransferUtil.o

$(ODIR)/aimUtil.o:	aimUtil.c ../include/capsTypes.h $(IDIR)/common.h $(IDIR)/OpenCSM.h
	$(CC) -c $(COPTS) $(DEFINE) -I$(IDIR) -I../include \
		-I/usr/include/udunits2 aimUtil.c -o $(ODIR)/aimUtil.o

$(ODIR)/aimTransferUtil.o:	aimTransferUtil.c ../include/capsTypes.h \
				../include/aimUtil.h
	$(CC) -c $(COPTS) $(DEFINE) -I$(IDIR) -I../include \
		-I/usr/include/udunits2 aimTransferUtil.c \
		-o $(ODIR)/aimTransferUtil.o

$(LDIR)/libcapsstatic.a:	$(OBJS)
	touch $(LDIR)/libcapsstatic.a
	rm $(LDIR)/libcapsstatic.a
	(cd $(ODIR); ar -rs $(LDIR)/libcapsstatic.a $(OBJS) )

$(LDIR)/libcaps.so:	$(OBJS)
	touch $(LDIR)/libcaps.so
	rm $(LDIR)/libcaps.so
	(cd $(ODIR); $(CC) -shared -Wl,-no-undefined -o $(LDIR)/libcaps.so \
			$(OBJS) -L$(LDIR) -ludunits2 -locsm -legads -ldl -lm )

$(LDIR)/libcaps.dylib:	$(OBJS)
	touch $(LDIR)/libcaps.dylib
	rm $(LDIR)/libcaps.dylib
	(cd $(ODIR); $(CC) -dynamiclib -o $(LDIR)/libcaps.dylib $(OBJS) \
			$(RPATH) -L$(LDIR) -ludunits2 -locsm -legads -ldl \
			-undefined error \
			-install_name '@rpath/libcaps.dylib' \
			-compatibility_version $(EGREV) \
			-current_version $(EGREV) )

# CYGWIN
$(LDIR)/libcaps.dll:	$(OBJS)
	touch $(LDIR)/libcaps.dll
	touch $(LDIR)/libcaps.dll.a
	rm $(LDIR)/libcaps.dll
	rm $(LDIR)/libcaps.dll.a
	(cd $(ODIR); $(CC) -shared -o $(LDIR)/libcaps.dll \
			-Wl,--out-implib,$(LDIR)/libcaps.dll.a $(OBJS) \
			-L$(LDIR) -ludunits2 -locsm -legads -ldl )

$(OBJS): %.o: 	%.c ../include/capsErrors.h ../include/capsTypes.h capsBase.h \
		$(IDIR)/OpenCSM.h $(IDIR)/common.h
	$(CC) -c $(COPTS) $(DEFINE) -I../include -I$(IDIR) -I. \
		-I/usr/include/udunits2 $< -o $(ODIR)/$@

$(IDIR)/aimUtil.h:	../include/aimUtil.h
	touch $(IDIR)/aimUtil.h
	rm $(IDIR)/aimUtil.h
	cp -p ../include/aimUtil.h $(IDIR)/aimUtil.h

$(IDIR)/caps.h:	../include/caps.h
	touch $(IDIR)/caps.h
	rm $(IDIR)/caps.h
	cp -p ../include/caps.h $(IDIR)/caps.h

$(IDIR)/capsErrors.h:	../include/capsErrors.h
	touch $(IDIR)/capsErrors.h
	rm $(IDIR)/capsErrors.h
	cp -p ../include/capsErrors.h $(IDIR)/capsErrors.h

$(IDIR)/capsTypes.h:	../include/capsTypes.h
	touch $(IDIR)/capsTypes.h
	rm $(IDIR)/capsTypes.h
	cp -p ../include/capsTypes.h $(IDIR)/capsTypes.h

clean:
	-(cd $(ODIR); rm -f $(OBJS) testingAIM.o aimUtil.o aimTransferUtil.o)

cleanall:	clean
	-rm -f $(LDIR)/libcaps* $(LDIR)/testingAIM.so $(LDIR)/libaimUtil.a
	-rm -f $(IDIR)/caps.h $(IDIR)/capsErrors.h $(IDIR)/capsTypes.h 
	-rm -f $(IDIR)/aimUtil.h
