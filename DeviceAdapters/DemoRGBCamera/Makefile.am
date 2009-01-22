AM_CXXFLAGS = -fpermissive -fweak
lib_LTLIBRARIES = DemoRGBCamera.la
DemoRGBCamera_la_SOURCES = DemoRGBCamera.cpp DemoRGBCamera.h RGBSignalGenerator.cpp
DemoRGBCamera_la_LDFLAGS = -module 
DemoRGBCamera_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a $(LIBACE)

install:
	cp .libs/DemoRGBCamera.so $(DEVICEADAPTERPATH)/$(MMPREFIX)DemoRGBCamera$(MMSUFFIX)

EXTRA_DIST = DemoRGBCamera.vcproj license.txt
