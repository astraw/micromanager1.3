AM_CXXFLAGS = -fpermissive
lib_LTLIBRARIES = libmmgr_dal_Prior.la
libmmgr_dal_Prior_la_SOURCES = Prior.cpp prior.h
## adding the libtool library gives problems on OS X 10.4, adding the static lib works
libmmgr_dal_Prior_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a 
libmmgr_dal_Prior_la_LDFLAGS = -module

install:
	cp .libs/libmmgr_dal_Prior.so $(DEVICEADAPTERPATH)/libmmgr_dal_Prior$(MMSUFFIX)

EXTRA_DIST = Prior.vcproj license.txt
