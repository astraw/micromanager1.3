///////////////////////////////////////////////////////////////////////////////
// FILE:          QICamera.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Micro-Manager plugin for QImaging cameras using the QCam API.
//                
// AUTHOR:        QImaging
//
// COPYRIGHT:     Copyright (C) 2007 Quantitative Imaging Corporation (QImaging).
//
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// CVS:           $Id: QICamera.h,v 1.15 2007/05/30 20:07:18 maustin Exp $
//

#ifndef _QICAMERA_H_
#define _QICAMERA_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/ModuleInterface.h"
#include <string>
#include <map>

#include <QCam/QCamAPI.h>
#include <QCam/QCamImgfnc.h>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_NO_CAMERAS_FOUND				100

#define	g_Keyword_Cooler					"Cooler"
#define	g_Keyword_CCDTemperature_Min		"CCDTemperature Min"
#define	g_Keyword_CCDTemperature_Max		"CCDTemperature Max"
#define g_Keyword_Gain_Min					"Gain Min"
#define g_Keyword_Gain_Max					"Gain Max"
#define g_Keyword_Offset_Min				"Offset Min"
#define g_Keyword_Offset_Max				"Offset Max"
#define g_Keyword_EMGain_Min				"EMGain Min"
#define g_Keyword_EMGain_Max				"EMGain Max"
#define g_Keyword_ITGain					"Intensifier Gain"
#define g_Keyword_ITGain_Min				"Intensifier Gain Min"
#define g_Keyword_ITGain_Max				"Intensifier Gain Max"


#define ERR_BUFFER_ALLOCATION_FAILED     1001
#define ERR_INCOMPLETE_SNAP_IMAGE_CYCLE  1002
#define ERR_BUSY_ACQUIRING               1003
#define ERR_INTERNAL_BUFFER_FULL         1004
#define ERR_NO_CAMERA_FOUND              1005

#ifndef QCAM_CHECK_ERROR
#define CHECK_ERROR(inErr) \
{ \
   if (CheckForError(inErr) )\
      return DEVICE_ERR;\
}
#else
#define CHECK_ERROR(inErr) \
{ \
   if (inErr != qerrSuccess) \
   { \
   printf("QCam error %d occured\n", inErr); \
   QCam_ReleaseDriver(); \
   \
   return DEVICE_ERR; \
   } \
}
#endif //QCAM_CHECK_ERROR

// has to be a C function
void QCAMAPI PreviewCallback
(
 void*				userPtr,			// User defined
 unsigned long		userData,			// User defined
 QCam_Err			errcode,			// Error code
 unsigned long		flags				// Combination of flags (see QCam_qcCallbackFlags)
 );

class QICamSequenceThread;

//////////////////////////////////////////////////////////////////////////////
// QICamera class
//////////////////////////////////////////////////////////////////////////////
class QICamera : public CCameraBase<QICamera>  
{
public:
   QICamera();
   ~QICamera();
   friend class QICamSequenceThread;

   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();

   void GetName(char* name) const;      
   bool Busy();

   // helper
   void ConvertReadoutSpeedToString(QCam_qcReadoutSpeed inSpeed, char *outString);
   void ConvertReadoutSpeedToEnum(const char *inSpeed, QCam_qcReadoutSpeed *outSpeed);

   // setup
   int SetupExposure();
   int SetupBinning();
   int SetupGain();
   int SetupOffset();
   int SetupReadoutSpeed();
   int SetupBitDepth();
   int SetupCooler();
   int SetupRegulatedCooling();
   int SetupEMGain();
   int SetupITGain();
   int SetupFrames();

   // MMCamera API
   // ------------
   int SnapImage();
   const unsigned char* GetImageBuffer();
   unsigned GetImageWidth() const;
   unsigned GetImageHeight() const;
   unsigned GetImageBytesPerPixel() const;
   unsigned GetBitDepth() const;
   int GetBinning() const;
   int SetBinning(int binSize);
   long GetImageBufferSize() const;
   double GetExposure() const;
   void SetExposure(double exp);
   int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize); 
   int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize); 
   int ClearROI();

   // action interface
   // ----------------
   int OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnOffset(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnReadoutSpeed(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnCooler(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnRegulatedCooling(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnEMGain(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnITGain(MM::PropertyBase* pProp, MM::ActionType eAct);

   // Sequence acquisition interface
   // ----------------
   int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
   int StopSequenceAcquisition();
   void SetCurrentFrameNumber(unsigned long inFrameNumber);

private:

   // streaming
   int StartStreamingImages();
   int ResizeImageBuffer();
   int FinishSequenceMode();
   void PushSequenceImage();
   int WaitForFrameCaptured();
public:
   void ExposureDone();

private:
   // error 
   int ReportError(std::string message, int err);
   int CheckForError(int err) const;


private:
   ImgBuffer			m_snappedImageBuffer;
   bool					m_isInitialized;
   bool					m_isBusy;
   QCam_Handle			m_camera;
   QCam_Settings		m_settings;
   QCam_Frame			*m_frame1;
   QCam_Frame        *m_frame2;
   QCam_Frame		   *m_currentFrame;
   long              m_numImages;
   bool              m_stopOnOverflow;
   QICamSequenceThread *m_seqThread;

#ifdef WIN32
   HANDLE				m_waitCondition;
#elif __APPLE_CC__
   pthread_mutex_t		m_waitMutex;
   pthread_cond_t		m_waitCondition;
#endif

};
/*
* Acquisition thread (the BaseSequenceThread inherited from CCameraBase is not used)
*/
class QICamSequenceThread : public MMDeviceThreadBase
{
   enum { default_numImages=1, default_intervalMS = 100 };
public:
   QICamSequenceThread(QICamera* camera) 
      :intervalMs_(default_intervalMS)
      ,numImages_(default_numImages)
      ,stop_(true)
      ,camera_(camera)
   {}
   ~QICamSequenceThread() {}
   void Stop() 
   {
      stop_ = true; 
      wait();
   }; 
   int Start(double intervalMs, long numImages) // intervalMS is never used 
   {
      intervalMs_ = intervalMs;
      numImages_ = numImages;
      stop_ = false; 
      return activate();
   }
   bool IsRunning(){return !stop_;};
   int svc();
private:
   double intervalMs_;
   long numImages_;
   bool stop_;
   QICamera* camera_;
};

#endif //_QICAMERA_H_
