AM_CXXFLAGS = -fpermissive
lib_LTLIBRARIES = Utilities.la
Utilities_la_SOURCES = Utilities.cpp Utilities.h
Utilities_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a 
Utilities_la_LDFLAGS = -module

install:
	cp .libs/Utilities.so $(DEVICEADAPTERPATH)/$(MMPREFIX)Utlities$(MMSUFFIX)

EXTRA_DIST = DAZStage.vcproj license.txt

