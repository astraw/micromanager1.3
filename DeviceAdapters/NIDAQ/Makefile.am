AM_CXXFLAGS = -fpermissive
lib_LTLIBRARIES = libmmgr_dal_NIDAQ.la
libmmgr_dal_NIDAQ_la_SOURCES = NIDAQ.cpp NIDAQ.h \
   ../../MMDevice/MMDevice.h ../../MMDevice/DeviceBase.h
## adding the libtool library gives problems on OS X 10.4, adding the static lib works
libmmgr_dal_NIDAQ_la_LIBADD = ../../MMDevice/.libs/libMMDevice.a
libmmgr_dal_NIDAQ_la_LDFLAGS = -module -framework nidaqmxbase -framework nidaqmxbaselv

install:
	cp .libs/libmmgr_dal_NIDAQ.so $(DEVICEADAPTERPATH)/libmmgr_dal_NIDAQ$(MMSUFFIX)
