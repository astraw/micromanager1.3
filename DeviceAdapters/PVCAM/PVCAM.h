///////////////////////////////////////////////////////////////////////////////
// FILE:          PVCAM.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   PVCAM camera module
//                
// AUTHOR:        Nico Stuurman, Nenad Amodaj nenad@amodaj.com, 09/13/2005
// COPYRIGHT:     University of California, San Francisco, 2006
//                100X Imaging Inc, 2008
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
//                Micromax compatible adapter is moved to PVCAMPI project, N.A. 10/2007
//
// CVS:           $Id$

#ifndef _PVCAM_H_
#define _PVCAM_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceUtils.h"
#include "../../MMDevice/DeviceThreads.h"

#ifdef WIN32
#include "../../../3rdparty/RoperScientific/Windows/PvCam/SDK/Headers/master.h"
#include "../../../3rdparty/RoperScientific/Windows/PvCam/SDK/Headers/pvcam.h"
#else 
#ifdef __APPLE__
#define __mac_os_x
#include <PVCAM/master.h>
#include <PVCAM/pvcam.h>
#endif
#endif

#include "PVCAMProperty.h"

#if(WIN32 && NDEBUG)
 //NOTE: not clear why is this necessary
 //but otherwise ACE headers won't compile under NDEBUG option 
   WINBASEAPI
   BOOL
   WINAPI
   TryEnterCriticalSection(
      __inout LPCRITICAL_SECTION lpCriticalSection
    );
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
#define ERR_CAMERA_NOT_FOUND        10006

// region of interest
struct ROI {
   uns16 x;
   uns16 y;
   uns16 xSize;
   uns16 ySize;

   ROI::ROI() : x(0), y(0), xSize(0), ySize(0) {}
   ROI::ROI(uns16 _x, uns16 _y, uns16 _xSize, uns16 _ySize )
      : x(_x), y(_y), xSize(_xSize), ySize(_ySize) {}
   ROI::~ROI() {}

   bool ROI::isEmpty() {return x==0 && y==0 && xSize==0 && ySize == 0;}
};

typedef std::map<uns32, uns32> SMap;
typedef std::map<std::string, uns32> TMap;

// helper structure for PVCAM parameters
typedef struct 
{	char * name;
	uns32 id;
   SMap indexMap;
   TMap enumMap;
} SParam;


//////////////////////////////////////////////////////////////////////////////
// Implementation of the MMDevice and MMCamera interfaces
// for all PVCAM cameras
//
class Universal : public CCameraBase<Universal>
{
public:
   
   Universal(short id);
   ~Universal();

   typedef PVCAMAction<Universal> CUniversalPropertyAction;
   
   // MMDevice API
   int Initialize();
   int Shutdown();
   void GetName(char* pszName) const;
   bool Busy();
   bool GetErrorText(int errorCode, char* text) const;
   
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
   int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize); 
   int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize);
   int ClearROI();

   // high-speed interface
   int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
   int StopSequenceAcquisition();
   // temporary debug methods
   int PrepareSequenceAcqusition();

   // action interface
   int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
   //int OnIdentifier(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnChipName(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnReadoutRate(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnMultiplierGain(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnReadoutPort(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnOffset(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTemperature(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTemperatureSetPoint(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnUniversalProperty(MM::PropertyBase* pProp, MM::ActionType eAct, long index);

   // Thread-safe param access:
   rs_bool PlSetParamSafe(int16 hcam, uns32 param_id, void_ptr param_value);
   rs_bool PlGetParamSafe(int16 hcam, uns32 param_id, int16 param_attribute, 
                                    void_ptr param_value);
   bool GetLongParam_PvCam_safe(int16 handle, uns32 pvcam_cmd, long *value);
   bool SetLongParam_PvCam_safe(int16 handle, uns32 pvcam_cmd, long value);
   rs_bool PlGetEnumParamSafe(int16 hcam, uns32 param_id, uns32 index,
                                     int32_ptr value, char_ptr desc,
                                     uns32 length);
   rs_bool PlEnumStrLengthSafe(int16 hcam, uns32 param_id, uns32 index,
                                      uns32_ptr length);

protected:
   int ThreadRun(void);
   void OnThreadExiting() throw();
   int PushImage();

private:
   rgn_type newRegion;

   Universal(Universal&) {}
   int CalculateImageBufferSize(
                                 ROI &newROI, 
                                 unsigned short &newXSize, 
                                 unsigned short &newYSize, 
                                 rgn_type &newRegion
                                 );
   int ResizeImageBufferSingle();
   int ResizeImageBufferContinuous();
   int GetSpeedString(std::string& modeString);
   int GetSpeedTable();
   bool GetEnumParam_PvCam(uns32 pvcam_cmd, uns32 index, std::string& enumString, int32& enumIndex);
   std::string GetPortName(long portId);
   int SetAllowedPixelTypes();
   int SetUniversalAllowedValues(int i, uns16 datatype);
   int SetGainLimits();
   void SuspendSequence();
   int ResumeSequence();
   bool WaitForExposureDone() throw();
   int LaunchSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);

   // Utility logging functions
   int16 LogCamError(int lineNr, std::string message="", bool debug=false) throw();
   int LogMMError(int errCode, int lineNr, std::string message="", bool debug=false) const throw();
   void LogMMMessage(int lineNr, std::string message="", bool debug=true) const throw();

   bool restart_;
   int16 bitDepth_;
   int x_, y_, width_, height_, xBin_, yBin_, bin_;
   ROI roi_;
   bool initialized_;
   bool busy_;
   long numImages_;
   short hPVCAM_; // handle to the driver
   static unsigned refCount_;
   static bool PVCAM_initialized_;
   ImgBuffer img_;
   rs_bool gainAvailable_;
   double exposure_;
   unsigned binSize_;
   bool bufferOK_;
   MM::MMTime startTime_;
   long imageCounter_;

   std::map<std::string, int> portMap_;
   std::map<std::string, int> rateMap_;

   short cameraId_;
   std::string name_;
   std::string chipName_;
   uns32 nrPorts_;
   unsigned short* circBuffer_;
   unsigned long bufferSize_; // circular buffer size
   bool stopOnOverflow_;
   MMThreadLock imgLock_;
   bool snappingSingleFrame_;
   MMThreadLock snappingSingleFrame_Lock_;
   bool singleFrameModeReady_;
   MMThreadLock singleFrameModeReady_Lock_;
   bool use_pl_exp_check_status_;
   bool sequenceModeReady_;

};

#endif //_PVCAM_H_
