AM_CXXFLAGS = -fpermissive
lib_LTLIBRARIES = libmmgr_dal_Pecon.la
libmmgr_dal_Pecon_la_SOURCES = Pecon.cpp Pecon.h h
## adding the libtool library gives problems on OS X 10.4, adding the static lib works
libmmgr_dal_Pecon_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a 
libmmgr_dal_Pecon_la_LDFLAGS = -module

install:
	cp .libs/libmmgr_dal_Pecon.so $(DEVICEADAPTERPATH)/libmmgr_dal_Pecon$(MMSUFFIX)

EXTRA_DIST = Pecon.vcproj license.txt
