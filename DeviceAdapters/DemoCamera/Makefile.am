AM_CXXFLAGS = -fpermissive -fweak
lib_LTLIBRARIES = libmmgr_dal_DemoCamera.la
libmmgr_dal_DemoCamera_la_SOURCES = DemoCamera.cpp DemoCamera.h
libmmgr_dal_DemoCamera_la_LDFLAGS = -module 
libmmgr_dal_DemoCamera_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a

install:
	cp .libs/libmmgr_dal_DemoCamera.so $(DEVICEADAPTERPATH)/libmmgr_dal_DemoCamera$(MMSUFFIX)

EXTRA_DIST = DemoCamera.vcproj license.txt
