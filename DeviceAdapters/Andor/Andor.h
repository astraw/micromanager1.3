///////////////////////////////////////////////////////////////////////////////
// FILE:          Andor.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Andor camera module 
//                
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 06/30/2006
// COPYRIGHT:     University of California, San Francisco, 2006
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
// REVISIONS:     May 21, 2007, Jizhen Zhao, Andor Technologies
//                Temerature control and other additional related properties added,
//                gain bug fixed, refernce counting fixed for shutter adapter.
//
//				  May 23 & 24, 2007, Daigang Wen, Andor Technology plc added/modified:
//				  Cooler is turned on at startup and turned off at shutdown
//				  Cooler control is changed to cooler mode control
//				  Pre-Amp-Gain property is added
//				  Temperature Setpoint property is added
//				  Temperature is resumed as readonly
//				  EMGainRangeMax and EMGainRangeMin are added
//
// FUTURE DEVELOPMENT: From September 1 2007, the development of this adaptor is taken over by Andor Technology plc. Daigang Wen (d.wen@andor.com) is the main contact. Changes made by him will not be labeled.
//
// CVS:           $Id$
//
#ifndef _ANDOR_H_
#define _ANDOR_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceUtils.h"
#include "../../MMDevice/DeviceThreads.h"
#include <string>
#include <map>

// error codes
#define ERR_BUFFER_ALLOCATION_FAILED 101
#define ERR_INCOMPLETE_SNAP_IMAGE_CYCLE 102
#define ERR_INVALID_ROI 103
#define ERR_INVALID_READOUT_MODE_SETUP 104
#define ERR_CAMERA_DOES_NOT_EXIST 105
#define ERR_BUSY_ACQUIRING 106
#define ERR_INVALID_PREAMPGAIN 107
#define ERR_INVALID_VSPEED 108
#define ERR_SOFTWARE_TRIGGER_NOT_SUPPORTED 109
#define ERR_OPEN_OR_CLOSE_SHUTTER_IN_ACQUISITION_NOT_ALLOWEDD 110

class AcqSequenceThread;

//////////////////////////////////////////////////////////////////////////////
// Implementation of the MMDevice and MMCamera interfaces
//
class Ixon : public CCameraBase<Ixon>
{
public:
   static Ixon* GetInstance();
   unsigned DeReference(); // jizhen 05.16.2007
   static void ReleaseInstance(Ixon*); // jizhen 05.16.2007

   ~Ixon();
   
   // MMDevice API
   int Initialize();
   int Shutdown();
   
   void GetName(char* pszName) const;
   bool Busy() {return acquiring_;}
   
   // MMCamera API
   int SnapImage();
   const unsigned char* GetImageBuffer();
   unsigned GetImageWidth() const {return img_.Width();}
   unsigned GetImageHeight() const {return img_.Height();}
   unsigned GetImageBytesPerPixel() const {return img_.Depth();} 
   long GetImageBufferSize() const {return img_.Width() * img_.Height() * GetImageBytesPerPixel();}
   unsigned GetBitDepth() const;
   int GetBinning() const;
   int SetBinning(int binSize);
   double GetExposure() const;
   void SetExposure(double dExp);
   int SetROI(unsigned uX, unsigned uY, unsigned uXSize, unsigned uYSize); 
   int GetROI(unsigned& uX, unsigned& uY, unsigned& uXSize, unsigned& uYSize);
   int ClearROI();

   // high-speed interface
   int StartSequenceAcquisition(long numImages, double interval_ms);
   int StopSequenceAcquisition();

   // action interface for the camera
   int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnEMGain(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnReadoutMode(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnReadoutTime(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnOffset(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTemperature(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDriverDir(MM::PropertyBase* pProp, MM::ActionType eAct);
   //int OnShutterMode(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnCooler(MM::PropertyBase* pProp, MM::ActionType eAct);// jizhen 05.11.2007
   int OnFanMode(MM::PropertyBase* pProp, MM::ActionType eAct);// jizhen 05.16.2007
   int OnTemperatureSetPoint(MM::PropertyBase* pProp, MM::ActionType eAct);// Daigang 23-May-2007  
   int OnEMGainRangeMax(MM::PropertyBase* pProp, MM::ActionType eAct);// Daigang 24-May-2007  
   int OnEMGainRangeMin(MM::PropertyBase* pProp, MM::ActionType eAct);// Daigang 24-May-2007  
   int OnPreAmpGain(MM::PropertyBase* pProp, MM::ActionType eAct);// Daigang 24-May-2007  
   int OnFrameTransfer(MM::PropertyBase* pProp, MM::ActionType eAct); 
   int OnVSpeed(MM::PropertyBase* pProp, MM::ActionType eAct);  
   int OnInternalShutter(MM::PropertyBase* pProp, MM::ActionType eAct);  
   int OnOutputAmplifier(MM::PropertyBase* pProp, MM::ActionType eAct);  
   int OnADChannel(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnCamera(MM::PropertyBase* pProp, MM::ActionType eAct);//for multiple camera support
   int OnCameraName(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OniCamFeatures(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTemperatureRangeMin(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTemperatureRangeMax(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnVCVoltage(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBaselineClamp(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnActualIntervalMS(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnUseSoftwareTrigger(MM::PropertyBase* pProp, MM::ActionType eAct);


   // custom interface for the thread
   int PushImage();

   //static void ReleaseInstance(Ixon * ixon);

private:
   Ixon();
   int ResizeImageBuffer();
   static Ixon* instance_;
   static unsigned refCount_;
   ImgBuffer img_;
   bool initialized_;
   bool snapInProgress_;
   bool acquiring_;
   long imageCounter_;
   long sequenceLength_;

   //daigang 24-may-2007
   void UpdateEMGainRange();
   long lSnapImageCnt_;
   std::vector<std::string> PreAmpGains_;
   long currentGain_;

   std::vector<std::string> VSpeeds_;
   void SetToIdle();
   bool IsAcquiring();
   bool IsThermoSteady();
   void CheckError(unsigned int errorVal);
   double currentExpMS_;

   long ReadoutTime_, KeepCleanTime_;
   long GetReadoutTime();
   HMODULE hAndorDll;
   typedef unsigned int (CALLBACK *FPGetReadOutTime)(float *_fReadoutTime);
   FPGetReadOutTime fpGetReadOutTime;
   typedef unsigned int (CALLBACK *FPGetKeepCleanTime)(float *_ftime);
   FPGetKeepCleanTime fpGetKeepCleanTime;
   //typedef unsigned int (CALLBACK *FPSendSoftwareTrigger)();
   //FPSendSoftwareTrigger fpSendSoftwareTrigger;

   bool busy_;

   struct ROI {
      int x;
      int y;
      int xSize;
      int ySize;

      ROI() : x(0), y(0), xSize(0), ySize(0) {}
      ~ROI() {}

      bool isEmpty() {return x==0 && y==0 && xSize==0 && ySize == 0;}
   };

   ROI roi_;
   int binSize_;
   double expMs_; //value used by camera
   std::string driverDir_;
   int fullFrameX_;
   int fullFrameY_;
   short* fullFrameBuffer_;
   std::vector<std::string> readoutModes_;

   int EmCCDGainLow_, EmCCDGainHigh_;
   int minTemp_, maxTemp_;
   //Daigang 24-may-2007
   bool ThermoSteady_;

   AcqSequenceThread* seqThread_;

   bool bShuterIntegrated;
   int ADChannelIndex_, OutputAmplifierIndex_;
   void UpdateHSSpeeds();

   int HSSpeedIdx_;

   bool bSoftwareTriggerSupported;

   long CurrentCameraID_;
   long NumberOfAvailableCameras_;
   long NumberOfWorkableCameras_;
   std::vector<std::string> cameraName_;
   std::vector<std::string> cameraSN_;
   std::vector<int> cameraID_;
   int GetListOfAvailableCameras();
   std::string CameraName_;
   std::string iCamFeatures_;
   std::string TemperatureRangeMin_;
   std::string TemperatureRangeMax_;
   std::string PreAmpGain_;
   std::string VSpeed_;
   std::string TemperatureSetPoint_;
   std::vector<std::string> VCVoltages_;
   std::string VCVoltage_;
   std::vector<std::string> BaselineClampValues_;
   std::string BaselineClampValue_;
   float ActualInterval_ms_;

   std::string UseSoftwareTrigger_;
   std::vector<std::string> vUseSoftwareTrigger_;

   bool bFrameTransfer_;

   unsigned char* GetImageBuffer_();
   unsigned char* pImgBuffer_;
   int SetExposure_();



};


/**
 * Acquisition thread
 */
class AcqSequenceThread : public MMDeviceThreadBase
{
public:
   AcqSequenceThread(Ixon* pCam) : 
      intervalMs_(100.0), numImages_(1), busy_(false), stop_(false) {camera_ = pCam;}
   ~AcqSequenceThread() {}
   int svc(void);

   void SetInterval(double intervalMs) {intervalMs_ = intervalMs;}
   void SetLength(long images) {numImages_ = images;}
   void Stop() {stop_ = true;}
   void Start() {stop_ = false; activate();}

private:
   Ixon* camera_;
   double intervalMs_;
   long numImages_;
   bool busy_;
   bool stop_;
};

#endif //_ANDOR_H_
