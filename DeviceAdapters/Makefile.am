AUTOMAKE_OPTIONS = foreign

if BUILD_HAMAMATSU
   HAMAMATSU = Hamamatsu
endif
if BUILD_PVCAM
   PVCAM = PVCAM
endif
if BUILD_QCAM
   QCAM = QCam
endif
if BUILD_TE2000
   TE2000 = NikonTE2000
endif
if BUILD_DC1394
   DC1394 = dc1394
endif
if BUILD_NIDAQ
   NIDAQ = NIDAQ
endif
if BUILD_USBMANAGER
   USBMANAGER = USBManager 
endif

SUBDIRS =  AOTF Andor Arduino ASIFW1000 ASIStage DTOpenLayer DemoCamera DemoStreamingCamera SerialManager SerialManagerUNIX Ludl LudlLow PI PI_GCS Prior ParallelPort Pecon Sensicam ZeissCAN ZeissCAN29 Vincent Conix $(HAMAMATSU) $(PVCAM) $(QCAM) NikonTE2000 $(TE2000) K8055 K8061 $(NIDAQ) $(USBMANAGER) Nikon SpectralLMM5 SutterLambda Utilities $(DC1394) Yokogawa

