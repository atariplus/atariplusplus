###################################################################################
##
## Global autoconf/automake Makefile. This makefile with capital 'M'
## builds the makefiles with small 'm' which are then searched first
## for the real program.
##
## $Id: Makefile,v 1.57 2022/12/20 20:00:30 thor Exp $
##
###################################################################################

DIST	=	atari++

.PHONY:		all clean distclean realclean distrib test manual deb atari final

all:		atari

final:		atari

makefile:	makefile.in configure
	./configure
	touch types.h

configure:	configure.in types.h.in
	autoconf

types.h.in:	configure.in
	autoheader
	touch types.h.in

types.h:	types.h.in configure.in configure
	./configure
	touch types.h

realclean:	clean

distclean:
	@rm -rf types.h makefile \
	*.o *.d *.dyn *.il atari++ \
	core autom4te.cache config.* configure.scan atari++.tgz dox/hmtl manual

clean:
	@rm -rf configure types.h types.h.in makefile \
	*.o *.d *.dyn *.il atari++ \
	core autom4te.cache config.* configure.scan atari++.tgz dox/hmtl manual

atari:		types.h makefile
	$(MAKE) -f makefile atari

atari++:	types.h makefile
	$(MAKE) -f makefile atari

debug:		types.h makefile
	$(MAKE) -f makefile debug

efence:		types.h makefile
	$(MAKE) -f makefile efence

profile:	types.h makefile
	$(MAKE) -f makefile atari

config:		types.h configure
	./configure

distrib:	atari++.tgz

manual:		atari++.man
	mkdir -p manual
	rm -f manual/*
	cd manual && man -Thtml ../atari++.man >atari++.html

atari++.tgz:	
	$(MAKE) -f Makefile clean
	$(MAKE) -f Makefile configure
	$(MAKE) -f Makefile manual
	cd .. && tar -czf $(DIST)/atari++.tgz $(DIST)/*.cpp $(DIST)/*.hpp $(DIST)/Makefile $(DIST)/makefile.in \
	$(DIST)/wintypes.h $(DIST)/winmain.cpp $(DIST)/atari++.vcproj $(DIST)/atari++_vc8.vcproj \
	$(DIST)/README.Compile.win \
	$(DIST)/atari++.rc $(DIST)/icon1.ico \
	$(DIST)/types.h.in $(DIST)/configure $(DIST)/configure.in $(DIST)/atari++.man $(DIST)/Makefile.atari \
	$(DIST)/README.licence $(DIST)/README.Compile $(DIST)/README.History $(DIST)/README.LEGAL \
	$(DIST)/ARCHITECTURE $(DIST)/CREDITS $(DIST)/COPYRIGHT $(DIST)/joystick.ps $(DIST)/manual/*

test:		atari++.tgz
	echo "Testing the distribution"
	mkdir -p /tmp/atari++
	cp atari++.tgz /tmp/atari++
	cd /tmp/atari++ && tar -xzf atari++.tgz && cd atari++ && configure && $(MAKE)


deb:	
	$(MAKE) -f Makefile clean
	$(MAKE) -f Makefile configure
	debuild

