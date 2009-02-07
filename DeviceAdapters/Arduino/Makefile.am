AM_CXXFLAGS = -fpermissive
lib_LTLIBRARIES = Arduino.la
Arduino_la_SOURCES = Arduino.cpp Arduino.h \
   ../../MMDevice/MMDevice.h ../../MMDevice/DeviceBase.h
## adding the libtool library gives problems on OS X 10.4, adding the static lib works
Arduino_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a 
Arduino_la_LDFLAGS = -module

install:
	cp .libs/Arduino.so  $(DEVICEADAPTERPATH)/$(MMPREFIX)Arduino$(MMSUFFIX)
