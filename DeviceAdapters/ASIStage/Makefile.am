AM_CXXFLAGS = -fpermissive
lib_LTLIBRARIES = libmmgr_dal_ASIStage.la
libmmgr_dal_ASIStage_la_SOURCES = ASIStage.cpp ASIStage.h h
## adding the libtool library gives problems on OS X 10.4, adding the static lib works
libmmgr_dal_ASIStage_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a 
libmmgr_dal_ASIStage_la_LDFLAGS = -module

install:
	cp .libs/libmmgr_dal_ASIStage.so $(DEVICEADAPTERPATH)/libmmgr_dal_ASIStage$(MMSUFFIX)

EXTRA_DIST = ASIStage.vcproj license.txt
