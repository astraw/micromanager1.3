AM_CXXFLAGS = -fpermissive
lib_LTLIBRARIES = libmmgr_dal_SerialManager.la
libmmgr_dal_SerialManager_la_SOURCES = SerialManager.cpp SerialManager.h \
         SerialPort.cpp SerialPort.h
## adding the libtool library gives problems on OS X 10.4, adding the static lib works
libmmgr_dal_SerialManager_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a 
libmmgr_dal_SerialManager_la_LDFLAGS = -module $(SERIALFRAMEWORKS)

install:
	cp .libs/libmmgr_dal_SerialManager.so $(DEVICEADAPTERPATH)/libmmgr_dal_SerialManager$(MMSUFFIX)


EXTRA_DIST = license.txt
