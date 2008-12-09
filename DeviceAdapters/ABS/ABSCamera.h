/////////////////////////////////////////////////////////////////////////////
// Name:        ABSCamera.h
// Purpose:     Definition der Kameraklasse als Adapter für µManager
// Author:      Michael Himmelreich
// Created:     31. Juli 2007
// Copyright:   (c) Michael Himmelreich
// Project:     ConfoVis
/////////////////////////////////////////////////////////////////////////////



#ifndef _ABSCAMERA_H_
#define _ABSCAMERA_H_

#include "MMDevice.h"
#include "DeviceBase.h"
#include "ImgBuffer.h"
#include "camusb_api.h"

#include <string>
#include <map>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_MODE         102
#define ERR_UNKNOWN_POSITION     103


class ABSCamera : public CCameraBase<ABSCamera>  
{
public:
   ABSCamera(int iDeviceNumber, const char* szDeviceName);
   ~ABSCamera();
  
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
   unsigned int GetNumberOfChannels() const;
   const unsigned int* GetImageBufferAsRGB32();
      
   unsigned GetImageWidth() const;
   unsigned GetImageHeight() const;
   unsigned GetImageBytesPerPixel() const;
   unsigned GetBitDepth() const;
   long GetImageBufferSize() const;
   double GetExposure() const;
   void SetExposure(double exp);
   int SetROI( unsigned x, unsigned y, unsigned xSize, unsigned ySize ); 
   int GetROI( unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize ); 
   int ClearROI();
   int GetBinning() const;
   int SetBinning(int binSize);

   int StartSequenceAcquisition(long /*numImages*/, double /*interval_ms*/);
   
   // action interface
   // ----------------
   int OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnGain(MM::PropertyBase* pProp, MM::ActionType eAct);

   int OnReadoutTime(MM::PropertyBase* /* pProp */, MM::ActionType /* eAct */) { return DEVICE_OK; };

private:
   std::string m_szDeviceName;

   int UpdateExposureLimits(void);
   int createExposure();
   int createBinning();
   int createPixelType();
   int createGain();

   ImgBuffer imageBuffer;
   bool initialized;
   bool busy;
   unsigned int numberOfChannels;
   unsigned char deviceNumber;
   unsigned __int64 cameraFunctionMask;

   S_CAMERA_VERSION     camVersion;
   S_RESOLUTION_CAPS*   resolutionCap;
   S_PIXELTYPE_CAPS*    pixelTypeCap;
   S_GAIN_CAPS*         gainCap;
   S_EXPOSURE_CAPS*     exposureCap;
   S_AUTOEXPOSURE_CAPS* autoExposureCap;
   
   unsigned char *imageBufRead;

   void GenerateSyntheticImage(ImgBuffer& img, double exp);
   int ResizeImageBuffer();
   void ShowError( unsigned int errorNumber ) const;
   void ShowError() const;
   void* GetCameraCap( unsigned __int64 CamFuncID )  const;   
   int GetCameraFunction( unsigned __int64 CamFuncID, void* functionPara, unsigned long size, void* functionParamOut = NULL, unsigned long sizeOut = 0 )  const;   
   int SetCameraFunction( unsigned __int64 CamFuncID, void* functionPara, unsigned long size ) const;
  
};

#endif //_ABSCAMERA_H_
