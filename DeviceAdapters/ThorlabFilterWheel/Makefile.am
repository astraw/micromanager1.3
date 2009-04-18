AM_CXXFLAGS = -fpermissive -fweak
lib_LTLIBRARIES = libmmgr_dal_ThorlabFilterWheel.la
libmmgr_dal_ThorlabFilterWheel_la_SOURCES = FilterWheel.cpp FilterWheel.h
libmmgr_dal_ThorlabFilterWheel_la_LDFLAGS = -module 
libmmgr_dal_ThorlabFilterWheel_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a

install:
	cp .libs/libmmgr_dal_ThorlabFilterWheel.so $(DEVICEADAPTERPATH)/libmmgr_dal_ThorlabFilterWheel$(MMSUFFIX)

EXTRA_DIST = ThorlabFilterWheel.vcproj license.txt
