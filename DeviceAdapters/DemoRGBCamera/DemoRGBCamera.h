///////////////////////////////////////////////////////////////////////////////
// FILE:          DemoRGBCamera.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   RGB camera simulator.
//                
// COPYRIGHT:     University of California, San Francisco, 2007
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

#ifndef _DEMO_RGBCAMERA_H_
#define _DEMO_RGBCAMERA_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include <string>
#include <map>


//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_MODE            102
#define ERR_BUSY_ACQIRING           105
#define ERR_UNSUPPORTED_IMAGE_TYPE  106
#define ERR_DEVICE_NOT_AVAILABLE    107

//////////////////////////////////////////////////////////////////////////////
// DemoRGBCamera class
// Simulation of the fast streaming Camera device
//////////////////////////////////////////////////////////////////////////////
class DemoRGBCamera : public CCameraBase<DemoRGBCamera>  
{
public:
   DemoRGBCamera();
   ~DemoRGBCamera();
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();
  
   void GetName(char* name) const;      
   bool Busy();
   
   // MMCamera API
   // ------------
   int SnapImage();
   const unsigned char* GetImageBuffer();
   const unsigned int* GetImageBufferAsRGB32();
   unsigned GetNumberOfChannels() const;
   int GetChannelName(unsigned int channel, char* name);
   unsigned GetImageWidth() const;
   unsigned GetImageHeight() const;
   unsigned GetImageBytesPerPixel() const;
   unsigned GetBitDepth() const;
   long GetImageBufferSize() const;
   double GetExposure() const;
   void SetExposure(double exp);
   int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize); 
   int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize); 
   int ClearROI();
   int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
   double GetNominalPixelSizeUm() const {return nominalPixelSizeUm_;}
   double GetPixelSizeUm() const {return nominalPixelSizeUm_ * GetBinning();}
   int GetBinning() const;
   int SetBinning(int binSize);

private:

   // action interface
   // ----------------
   int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnReadoutTime(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnColorMode(MM::PropertyBase* pProp, MM::ActionType eAct);
   int SetPixelTypesValues();

   //Do necessary for capturing
   //Is called from the thread function
   //Overrides ones defined in the CCameraBase class 
   int ThreadRun();
   int PushImage();

   static const double nominalPixelSizeUm_;
   static const int imageSize_;

   ImgBuffer img_[3];
   bool initialized_;
   bool busy_;
   long readoutUs_;
   MM::MMTime readoutStartTime_;
   bool color_;
   unsigned char* rawBuffer_;
   bool stopOnOverflow_;

   void GenerateSyntheticImage(ImgBuffer& img, double exp);

   int ResizeImageBuffer(  int imageSizeW = imageSize_, 
                           int imageSizeH = imageSize_);
   int ResizeImageBuffer(
                           int imageSizeW, 
                           int imageSizeH, 
                           int byteDepth, 
                           int binSize = 1);
};

//////////////////////////////////////////////////////////////////////////////
// DemoNoiseProcessor class
// Demonstration of the image processor module.
//////////////////////////////////////////////////////////////////////////////
class DemoNoiseProcessor : public CImageProcessorBase<DemoNoiseProcessor>  
{
public:
   DemoNoiseProcessor();
   ~DemoNoiseProcessor();
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();
  
   void GetName(char* name) const;      
   bool Busy();
   
   // ImageProcessor API
   // ------------------
   int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth);

   // action interface
   // ----------------
   int OnStdDev(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   bool initialized_;
   double stdDev_;
};

//////////////////////////////////////////////////////////////////////////////
// DemoNoiseProcessor class
// Demonstration of the image processor module.
//////////////////////////////////////////////////////////////////////////////
class RGBSignalGenerator : public CImageProcessorBase<RGBSignalGenerator>  
{
public:
   RGBSignalGenerator();
   ~RGBSignalGenerator();
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();
  
   void GetName(char* name) const;      
   bool Busy();
   
   // ImageProcessor API
   // ------------------
   int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth);

   // action interface
   // ----------------
   int OnOutputDevice(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   bool initialized_;
   std::string outputDevice_;
   MM::State* pStateDevice_;
};

/**
 * Acquisition thread
 *
class AcqSequenceThread : public MMDeviceThreadBase
{
public:
   AcqSequenceThread(DemoRGBCamera* pCam) : 
      intervalMs_(100.0), numImages_(1), busy_(false), stop_(false), suspend_(false), suspended_(false)
      {camera_ = pCam;}
   ~AcqSequenceThread() {}
   int svc(void);

   void SetInterval(double intervalMs) {intervalMs_ = intervalMs;}
   void SetLength(long images) {numImages_ = images;}
   void Stop();
   void Start();
   void Suspend() {suspend_ = true;}
   bool IsSuspended() {return suspended_;}
   void Resume() {suspend_ = false;}

private:
   DemoRGBCamera* camera_;
   double intervalMs_;
   long numImages_;
   bool busy_;
   bool stop_;
   bool suspend_;
   bool suspended_;
};
*/

#endif //_DEMO_RGBAMERA_H_
