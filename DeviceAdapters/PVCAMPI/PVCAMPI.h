///////////////////////////////////////////////////////////////////////////////
// FILE:          PVCAMPI.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   PVCAM camera module for Prinston Instruments devices
//                
// AUTHOR:        Nico Stuurman, Nenad Amodaj nenad@amodaj.com, 10/19/2007
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
// NOTE:          CPVCAM and Cascade classes are obsolete and remain here only
//                for backward compatibility purposes. For modifications and
//                extensions use Universal class. N.A. 01/17/2007
//
// CVS:           $Id: PVCAM.h 475 2007-09-27 19:44:59Z nenad $

#ifndef _PVCAMPI_H_
#define _PVCAMPI_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceUtils.h"

#ifdef WIN32
#include "../../../3rdparty/RoperScientific/Windows/PvCam_micromax/master.h"
#else 
#ifdef __APPLE__
#define __mac_os_x
#include <PVCAM/master.h>
#include <PVCAM/pvcam.h>
#endif
#endif

#include <string>
#include <map>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_INVALID_BUFFER          10002
#define ERR_INVALID_PARAMETER_VALUE 10003
#define ERR_BUSY_ACQUIRING          10004
#define ERR_STREAM_MODE_NOT_SUPPORTED 10005

// region of interest
struct ROI {
   int x;
   int y;
   int xSize;
   int ySize;

    ROI() : x(0), y(0), xSize(0), ySize(0) {}
    ~ROI() {}

    bool isEmpty() {return x==0 && y==0 && xSize==0 && ySize == 0;}
};


//////////////////////////////////////////////////////////////////////////////
// Implementation of the MMDevice and MMCamera interfaces
// for Micromax camera
//
class CPVCAM : public CCameraBase<CPVCAM>
{
public:
   static CPVCAM* GetInstance();
   ~CPVCAM();
   
   // MMDevice API
   int Initialize();
   int Shutdown();
   void GetName(char* pszName) const;
   bool Busy() {return busy_;}
   bool GetErrorText(int errorCode, char* text) const;
   
   // MMCamera API
   int SnapImage();
   const unsigned char* GetImageBuffer();
   unsigned GetImageWidth() const {return img_.Width();}
   unsigned GetImageHeight() const {return img_.Height();}
   unsigned GetImageBytesPerPixel() const {return img_.Depth();} 
   long GetImageBufferSize() const {return img_.Width() * img_.Height() * GetImageBytesPerPixel();}
   unsigned GetBitDepth() const;
   double GetExposure() const;
   void SetExposure(double dExp);
   int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize); 
   int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize);
   int ClearROI();

   // action interface
   int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnOffset(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTemperature(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   CPVCAM();
   CPVCAM(CPVCAM&) {}

   ROI roi_;

   int ResizeImageBuffer();
   int x_, y_, width_, height_, xBin_, yBin_, bin_;

   static CPVCAM* instance_;
   static unsigned refCount_;
   ImgBuffer img_;
   bool busy_;
   bool initialized_;
   double exposure_;
   unsigned binSize_;
   bool bufferOK_;

   int16 hPVCAM_; // handle to the driver
};



#endif //_PVCAMPI_H_
