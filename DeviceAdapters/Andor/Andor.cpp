///////////////////////////////////////////////////////////////////////////////
// FILE:          Andor.cpp
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
//				  April 3 & 4, 2008, Nico Stuurman, UCSF
//				  Changed Sleep statement in AcqSequenceThread to be 20% of the actualInterval instead of 5 ms
//            Added property limits to the Gain (EMGain) property
//            Added property limits to the Temperature Setpoint property and delete TempMin and TempMax properties
//
// FUTURE DEVELOPMENT: From September 1 2007, the development of this adaptor is taken over by Andor Technology plc. Daigang Wen (d.wen@andor.com) is the main contact. Changes made by him will not be labeled.
//
// CVS:           $Id$

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../MMDevice/ModuleInterface.h"
#include "atmcd32d.h"
#include "Andor.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <math.h>



#include <iostream>
using namespace std;

#pragma warning(disable : 4996) // disable warning for deperecated CRT functions on Windows 

using namespace std;

// global constants
const char* g_AndorName = "Andor";
const char* g_IxonName = "Ixon";

const char* g_PixelType_8bit = "8bit";
const char* g_PixelType_16bit = "16bit";

const char* g_ShutterMode = "ShutterMode";
const char* g_ShutterMode_Auto = "Auto";
const char* g_ShutterMode_Open = "Open";
const char* g_ShutterMode_Closed = "Closed";

const char* g_FanMode_Full = "Full";
const char* g_FanMode_Low = "Low";
const char* g_FanMode_Off = "Off";

const char* g_CoolerMode_FanOffAtShutdown = "Fan off at shutdown";
const char* g_CoolerMode_FanOnAtShutdown = "Fan on at shutdown";

const char* g_FrameTransferProp = "FrameTransfer";
const char* g_FrameTransferOn = "On";
const char* g_FrameTransferOff = "Off";
const char* g_OutputAmplifier = "Output_Amplifier";
const char* g_OutputAmplifier_EM = "Standard EMCCD gain register";
const char* g_OutputAmplifier_Conventional = "Conventional CCD register";

const char* g_ADChannel = "AD_Converter";
const char* g_ADChannel_14Bit = "14bit";
const char* g_ADChannel_16Bit = "16bit";


// singleton instance
AndorCamera* AndorCamera::instance_ = 0;
unsigned AndorCamera::refCount_ = 0;

// Windows dll entry routine
BOOL APIENTRY DllMain( HANDLE /*hModule*/, 
                       DWORD  ul_reason_for_call, 
                       LPVOID /*lpReserved*/
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
   break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
	break;
	}
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
   AddAvailableDeviceName(g_AndorName, "Generic Andor Camera Adapter");

}

char deviceName[64]; // jizhen 05.16.2007

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   // jizhen 05.16.2007
   //char* deviceName = new char[128]; // will crash the stack if put the variable here! 
   pDevice->GetName( deviceName);
   if ( strcmp(deviceName, g_AndorName) == 0) 
   {
	   AndorCamera::ReleaseInstance((AndorCamera*) pDevice);
   } 
   else 
   // eof jizhen
	   delete pDevice;
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   string strName(deviceName);
   
   if (strcmp(deviceName, g_AndorName) == 0)
      return AndorCamera::GetInstance();
   else if (strcmp(deviceName, g_IxonName) == 0)
      return AndorCamera::GetInstance();

   
   return 0;
}

void AndorCamera::ReleaseInstance(AndorCamera * andorCam) {

	unsigned int refC = andorCam->DeReference();
	if ( refC <=0 ) 
	{
		delete andorCam;
		andorCam = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////
// AndorCamera constructor/destructor

AndorCamera::AndorCamera() :
   initialized_(false),
   busy_(false),
   snapInProgress_(false),
   binSize_(1),
   expMs_(0.0),
   driverDir_(""),
   fullFrameBuffer_(0),
   fullFrameX_(0),
   fullFrameY_(0),
   EmCCDGainLow_(0),
   EmCCDGainHigh_(0),
   EMSwitch_(true),
   minTemp_(0),
   ThermoSteady_(0),
   lSnapImageCnt_(0),
   currentGain_(-1),
   ReadoutTime_(50),
   ADChannelIndex_(0),
   acquiring_(false),
   imageCounter_(0),
   sequenceLength_(0),
   OutputAmplifierIndex_(0),
   HSSpeedIdx_(0),
   bSoftwareTriggerSupported(0),
   maxTemp_(0),
   CurrentCameraID_(-1),
   pImgBuffer_(0),
   currentExpMS_(0.0),
   bFrameTransfer_(0),
   stopOnOverflow_(true),
   iCurrentTriggerMode(INTERNAL),
   strCurrentTriggerMode("")
{
   InitializeDefaultErrorMessages();



   // add custom messages
   SetErrorText(ERR_BUSY_ACQUIRING, "Camera Busy.  Stop camera activity first.");

   seqThread_ = new AcqSequenceThread(this);

   // Pre-initialization properties
   // -----------------------------

   // Driver location property removed.  atmcd32d.dll should be in the working directory

   hAndorDll = 0;
   fpGetKeepCleanTime = 0;
   fpGetReadOutTime = 0;
   if(hAndorDll == 0)
      hAndorDll = ::GetModuleHandle("atmcd32d.dll");
   if(hAndorDll!=NULL)
   {
      fpGetKeepCleanTime = (FPGetKeepCleanTime)GetProcAddress(hAndorDll, "GetKeepCleanTime");
      fpGetReadOutTime = (FPGetReadOutTime)GetProcAddress(hAndorDll, "GetReadOutTime");
   }

}

AndorCamera::~AndorCamera()
{
   delete seqThread_;
   
   refCount_--;
   if (refCount_ == 0)
   {
      // release resources
	if (initialized_)
	{
    	SetToIdle();
        int ShutterMode = 2;  //0: auto, 1: open, 2: close
        SetShutter(1, ShutterMode, 20,20);//0, 0);
	}

	
	if (initialized_)
        CoolerOFF();  //Daigang 24-may-2007 turn off the cooler at shutdown

      if (initialized_)
         Shutdown();
      // clear the instance pointer
      instance_ = 0;
   }
}

AndorCamera* AndorCamera::GetInstance()
{
   if (!instance_)
      instance_ = new AndorCamera();

   refCount_++;
   return instance_;
}

// jizhen 05.16.2007
unsigned AndorCamera::DeReference()
{
   refCount_--;
   return refCount_;
}
// eof jizhen

///////////////////////////////////////////////////////////////////////////////
// API methods
// ~~~~~~~~~~~

/**
* Get list of all available cameras
*/
int AndorCamera::GetListOfAvailableCameras()
{
   unsigned ret;

   NumberOfAvailableCameras_ = 0;
   ret = GetAvailableCameras(&NumberOfAvailableCameras_);
   if (ret != DRV_SUCCESS)
      return ret;
   if(NumberOfAvailableCameras_ == 0)
	   return ERR_CAMERA_DOES_NOT_EXIST;

   long CameraID;
   int UnknownCameraIndex = 0;
   NumberOfWorkableCameras_ = 0;
   cameraName_.clear();
   cameraID_.clear();
   for(int i=0;i<NumberOfAvailableCameras_; i++)
   {
     ret = GetCameraHandle(i, &CameraID);
     if( ret ==DRV_SUCCESS )
     {
       ret = SetCurrentCamera(CameraID);
       if( ret ==DRV_SUCCESS )
	   {
		   ret=::Initialize(const_cast<char*>(driverDir_.c_str()));
         if( ret!=DRV_SUCCESS && ret != DRV_ERROR_FILELOAD )
         {
           ret = ShutDown();
		 }
		 if( ret == DRV_SUCCESS)
		 {
           NumberOfWorkableCameras_++;
           std::string anStr;
           char chars[255];
           ret = GetHeadModel(chars);
           if( ret!=DRV_SUCCESS )
           {
             anStr = "UnknownModel";
           }
           else
           {
             anStr = chars;
           }
           int id;
           ret = GetCameraSerialNumber(&id);
           if( ret!=DRV_SUCCESS )
           {
             UnknownCameraIndex ++;
             id = UnknownCameraIndex;
           }
           sprintf(chars, "%d", id);

		   anStr = anStr + " " + chars;
		   cameraName_.push_back(anStr);
		   cameraID_.push_back((int)CameraID);
          
         
           //if there is only one camera, don't shutdown
           if( NumberOfAvailableCameras_ > 1 )
           {
             ret = ShutDown();
           }
		   else
		   {
			 CurrentCameraID_ = CameraID;
		   }
		 }
	   }
     }
   }

   if(NumberOfWorkableCameras_>=1)
   {
       //camera property for multiple camera support
       /*  //removed because list boxes in Property Browser of MM are unable to update their values after switching camera
       CPropertyAction *pAct = new CPropertyAction (this, &AndorCamera::OnCamera);
       int nRet = CreateProperty("Camera", cameraName_[NumberOfWorkableCameras_-1].c_str(), MM::String, false, pAct);
       assert(nRet == DEVICE_OK);
       nRet = SetAllowedValues("Camera", cameraName_);
	   */
	   return DRV_SUCCESS;
   }
   else
	   return ERR_CAMERA_DOES_NOT_EXIST;

}

/**
 * Set camera
 */
int AndorCamera::OnCamera(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
	  //added to use RTA
	  SetToIdle();

      string CameraName;
      pProp->Get(CameraName);
      for (unsigned i=0; i<cameraName_.size(); ++i)
         if (cameraName_[i].compare(CameraName) == 0)
         {
            int ret = ShutDown(); //shut down the used camera
			initialized_ = false;
			CurrentCameraID_ = cameraID_[i];
			ret = Initialize();
            if (DEVICE_OK != ret)
               return ret;
            else
               return DEVICE_OK;
         }
      assert(!"Unrecognized Camera");
   }
   else if (eAct == MM::BeforeGet)
   {
   }
   return DEVICE_OK;
}
/**
 * Camera Name
 */
int AndorCamera::OnCameraName(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(CameraName_.c_str());
   }
   return DEVICE_OK;
}

/**
 * iCam Features
 */
int AndorCamera::OniCamFeatures(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(iCamFeatures_.c_str());
   }
   return DEVICE_OK;
}

/**
 * Temperature Range Min
 */
int AndorCamera::OnTemperatureRangeMin(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(TemperatureRangeMin_.c_str());
   }
   return DEVICE_OK;
}

/**
 * Temperature Range Min
 */
int AndorCamera::OnTemperatureRangeMax(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(TemperatureRangeMax_.c_str());
   }
   return DEVICE_OK;
}



/**
 * Initialize the camera.
 */
int AndorCamera::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   unsigned ret;
   int nRet;
   CPropertyAction *pAct;

   if(CurrentCameraID_ == -1)
   {
      ret = GetListOfAvailableCameras();
      if (ret != DRV_SUCCESS)
         return ret;
			for(int i = 0; i < NumberOfWorkableCameras_; ++i) {
         CurrentCameraID_ = cameraID_[i];
         ret = SetCurrentCamera(CurrentCameraID_);
         if (ret == DRV_SUCCESS)
            ret = ::Initialize(const_cast<char*>(driverDir_.c_str()));
         /*
			if (ret == DRV_SUCCESS) {
				if(HasProperty("Camera")) {
            int placeholder = 0;
					}
					break;
				}
            */
			}
         
			if(ret != DRV_SUCCESS) {
				return ret;
			}
			/*
	   if(NumberOfAvailableCameras_>1 && NumberOfWorkableCameras_>=1)
	   {
			 
        CurrentCameraID_=cameraID_[0];
        ret = SetCurrentCamera(CurrentCameraID_);
        if (ret != DRV_SUCCESS)
           return ret;
        ret = ::Initialize(const_cast<char*>(driverDir_.c_str()));
        if (ret != DRV_SUCCESS)
           return ret;
	   }
		 */
   }
   else
   {
      ret = SetCurrentCamera(CurrentCameraID_);
      if (ret != DRV_SUCCESS)
         return ret;
      ret = ::Initialize(const_cast<char*>(driverDir_.c_str()));
      if (ret != DRV_SUCCESS)
         return ret;
   }

   // Name
   int currentCameraIdx = 0;
   if(cameraID_.size()>1)
   {
      for(unsigned int i=0;i<cameraID_.size();i++)
      {
	      if(cameraID_[i] == CurrentCameraID_)
	      {
		      currentCameraIdx = i;
		      break;
	      }
      }
   }
   CameraName_ = cameraName_[currentCameraIdx];
   bool isLuca = false;
   if (CameraName_.substr(0,3).compare("Luc")==0)
      isLuca = true;

   if(HasProperty(MM::g_Keyword_Name))
   {
      nRet = SetProperty(MM::g_Keyword_Name,cameraName_[currentCameraIdx].c_str());   
   }
   else
   {
      CPropertyAction *pAct = new CPropertyAction (this, &AndorCamera::OnCameraName);
      nRet = CreateProperty(MM::g_Keyword_Name, cameraName_[currentCameraIdx].c_str(), MM::String, true, pAct);
   }
   assert(nRet == DEVICE_OK);

   // Description
   if (!HasProperty(MM::g_Keyword_Description))
   {
      nRet = CreateProperty(MM::g_Keyword_Description, "Andor camera adapter", MM::String, true);
   }
   assert(nRet == DEVICE_OK);

   // Camera serial number
   int serialNumber;
   ret = GetCameraSerialNumber(&serialNumber);
   if (ret == DRV_SUCCESS) {
      std::ostringstream sN;
      sN << serialNumber;
      nRet = CreateProperty("Serial Number", sN.str().c_str(), MM::String, true);
      std::ostringstream msg;
      msg << "Camera Serial Number: " << serialNumber;
      LogMessage(msg.str().c_str(), false);
   }

   // Get various version numbers
   unsigned int eprom, cofFile, vxdRev, vxdVer, dllRev, dllVer;
   ret = GetSoftwareVersion(&eprom, &cofFile, &vxdRev, &vxdVer, &dllRev, &dllVer);
   if (ret == DRV_SUCCESS) {
      std::ostringstream verInfo;
      verInfo << "Camera version info: " << std::endl;
      verInfo << "EPROM: " << eprom << std::endl;
      verInfo << "COF File: " << cofFile << std::endl;
      verInfo << "Driver: " << vxdVer << "." << vxdRev << std::endl;
      verInfo << "DLL: " << dllVer << "." << dllRev << std::endl;
      LogMessage(verInfo.str().c_str(), false);
   }

   // capabilities
   AndorCapabilities caps;
   caps.ulSize = sizeof(AndorCapabilities);
   ret = GetCapabilities(&caps);
   if (ret != DRV_SUCCESS)
      return ret;

   //check iCam feature
   vTriggerModes.clear();  
   if(caps.ulTriggerModes & AC_TRIGGERMODE_CONTINUOUS)
   {
	   if(iCurrentTriggerMode == SOFTWARE) {
      ret = SetTriggerMode(10);  //set software trigger. mode 0:internal, 1: ext, 6:ext start, 7:bulb, 10:software
      if (ret != DRV_SUCCESS)
	   {
         ShutDown();
         return ret;
	   }
		 strCurrentTriggerMode = "Software";
	   }
	   vTriggerModes.push_back("Software");
	   bSoftwareTriggerSupported = true;
   }
   if(caps.ulTriggerModes & AC_TRIGGERMODE_EXTERNAL) {
	   if(iCurrentTriggerMode == EXTERNAL) {
         ret = SetTriggerMode(1);  //set software trigger. mode 0:internal, 1: ext, 6:ext start, 7:bulb, 10:software
      if (ret != DRV_SUCCESS)
	   {
         ShutDown();
         return ret;
	   }
		 strCurrentTriggerMode = "External";
   }
	   vTriggerModes.push_back("External");
   }
   if(caps.ulTriggerModes & AC_TRIGGERMODE_INTERNAL) {
	   if(iCurrentTriggerMode == INTERNAL) {
         ret = SetTriggerMode(0);  //set software trigger. mode 0:internal, 1: ext, 6:ext start, 7:bulb, 10:software
         if (ret != DRV_SUCCESS)
   {
           ShutDown();
           return ret;
   }
		 strCurrentTriggerMode = "Internal";
	   }
	   vTriggerModes.push_back("Internal");
   }
   if(!HasProperty("Trigger"))
      {
     CPropertyAction *pAct = new CPropertyAction (this, &AndorCamera::OnSelectTrigger);
     nRet = CreateProperty("Trigger", "Trigger Mode", MM::String, false, pAct);
         assert(nRet == DEVICE_OK);
      }
   nRet = SetAllowedValues("Trigger", vTriggerModes);
      assert(nRet == DEVICE_OK);
   nRet = SetProperty("Trigger", strCurrentTriggerMode.c_str());
      assert(nRet == DEVICE_OK);

   //Set EM Gain mode
   if(caps.ulEMGainCapability&AC_EMGAIN_REAL12)
   {
      ret = SetEMAdvanced(1);
      if (ret != DRV_SUCCESS)
         return ret;
      if (ret != DRV_SUCCESS)
         return ret;
   } 
   else if(caps.ulEMGainCapability&AC_EMGAIN_LINEAR12)
   { 
      ret = SetEMAdvanced(1);
      if (ret != DRV_SUCCESS)
         return ret;
      ret = SetEMGainMode(2);  //mode 0: 0-255; 1: 0-4095; 2: Linear; 3: real
      if (ret != DRV_SUCCESS)
         return ret;
   }
   else if(caps.ulEMGainCapability&AC_EMGAIN_12BIT)
   {
      ret = SetEMAdvanced(1);
      if (ret != DRV_SUCCESS)
         return ret;
      ret = SetEMGainMode(1);  //mode 0: 0-255; 1: 0-4095; 2: Linear; 3: real
      if (ret != DRV_SUCCESS)
         return ret;
   }
   else if(caps.ulEMGainCapability&AC_EMGAIN_8BIT)
   {
      ret = SetEMGainMode(0);  //mode 0: 0-255; 1: 0-4095; 2: Linear; 3: real
      if (ret != DRV_SUCCESS)
         return ret;
   }

   //Output amplifier
   int numAmplifiers;
   ret = GetNumberAmp(&numAmplifiers);
   if (ret != DRV_SUCCESS)
      return ret;
   if(numAmplifiers > 1)
   {
      if(!HasProperty(g_OutputAmplifier))
      {
         CPropertyAction *pAct = new CPropertyAction (this, &AndorCamera::OnOutputAmplifier);
         nRet = CreateProperty(g_OutputAmplifier, g_OutputAmplifier_EM, MM::String, false, pAct);
	   }
      vector<string> OutputAmplifierValues;
      OutputAmplifierValues.push_back(g_OutputAmplifier_EM);
      OutputAmplifierValues.push_back(g_OutputAmplifier_Conventional);
      nRet = SetAllowedValues(g_OutputAmplifier, OutputAmplifierValues);
      assert(nRet == DEVICE_OK);
	   nRet = SetProperty(g_OutputAmplifier,  OutputAmplifierValues[0].c_str());   
      assert(nRet == DEVICE_OK);
      if (nRet != DEVICE_OK)
         return nRet;
   }

   //AD channel (pixel bitdepth)
   int numADChannels;
   ret = GetNumberADChannels(&numADChannels);
   if (ret != DRV_SUCCESS)
      return ret;
   if(numADChannels > 1)
   {
      if(!HasProperty(g_ADChannel))
      {
         CPropertyAction *pAct = new CPropertyAction (this, &AndorCamera::OnADChannel);
         nRet = CreateProperty(g_ADChannel, g_ADChannel_14Bit, MM::String, false, pAct);
         assert(nRet == DEVICE_OK);
	   }
      vector<string> ADChannelValues;
      ADChannelValues.push_back(g_ADChannel_14Bit);
      ADChannelValues.push_back(g_ADChannel_16Bit);
      nRet = SetAllowedValues(g_ADChannel, ADChannelValues);
      assert(nRet == DEVICE_OK);
      if (nRet != DEVICE_OK)
         return nRet;
	   nRet = SetProperty(g_ADChannel,  ADChannelValues[0].c_str());   
      if (nRet != DEVICE_OK)
         return nRet;
   }

   ret = SetADChannel(0);  //0:14bit, 1:16bit(if supported)
   if (ret != DRV_SUCCESS)
      return ret;
   ret = SetOutputAmplifier(0);  //0:EM port, 1:Conventional port
   if (ret != DRV_SUCCESS)
      return ret;

   // head model
   char model[32];
   ret = GetHeadModel(model);
   if (ret != DRV_SUCCESS)
      return ret;

   // Get detector information
   ret = GetDetector(&fullFrameX_, &fullFrameY_);
   if (ret != DRV_SUCCESS)
      return ret;

   roi_.x = 0;
   roi_.y = 0;
   roi_.xSize = fullFrameX_;
   roi_.ySize = fullFrameY_;
   binSize_ = 1;
   fullFrameBuffer_ = new short[fullFrameX_ * fullFrameY_];

   // setup image parameters
   // ----------------------
   if(iCurrentTriggerMode == SOFTWARE)
     ret = SetAcquisitionMode(5);// 1: single scan mode, 5: RTA
   else
     ret = SetAcquisitionMode(1);// 1: single scan mode, 5: RTA

   if (ret != DRV_SUCCESS)
      return ret;

   ret = SetReadMode(4); // image mode
   if (ret != DRV_SUCCESS)
      return ret;

   // binning
   if(!HasProperty(MM::g_Keyword_Binning))
   {
      CPropertyAction *pAct = new CPropertyAction (this, &AndorCamera::OnBinning);
      nRet = CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false, pAct);
      assert(nRet == DEVICE_OK);
   }
   else
   {
      nRet = SetProperty(MM::g_Keyword_Binning,  "1");   
      if (nRet != DEVICE_OK)
         return nRet;
   }

   vector<string> binValues;
   binValues.push_back("1");
   binValues.push_back("2");
   binValues.push_back("4");
   binValues.push_back("8");
   nRet = SetAllowedValues(MM::g_Keyword_Binning, binValues);
   if (nRet != DEVICE_OK)
      return nRet;

   // pixel type
   if(!HasProperty(MM::g_Keyword_PixelType))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnPixelType);
      nRet = CreateProperty(MM::g_Keyword_PixelType, g_PixelType_16bit, MM::String, false, pAct);
      assert(nRet == DEVICE_OK);
   }

   vector<string> pixelTypeValues;
   pixelTypeValues.push_back(g_PixelType_16bit);
   nRet = SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
   if (nRet != DEVICE_OK)
      return nRet;
   nRet = SetProperty(MM::g_Keyword_PixelType, pixelTypeValues[0].c_str());
   if (nRet != DEVICE_OK)
      return nRet;

   // exposure
   if(!HasProperty(MM::g_Keyword_Exposure))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnExposure);
      nRet = CreateProperty(MM::g_Keyword_Exposure, "10.0", MM::Float, false, pAct);
      assert(nRet == DEVICE_OK);
   }
   else
   {
	   nRet = SetProperty(MM::g_Keyword_Exposure,"10.0");
      assert(nRet == DEVICE_OK);
   }

   int InternalShutter;
   ret = IsInternalMechanicalShutter(&InternalShutter);
   if(InternalShutter == 0)
	   bShuterIntegrated = false;
   else
   {
	   bShuterIntegrated = true;
	   if(!HasProperty("InternalShutter"))
	   {
         pAct = new CPropertyAction (this, &AndorCamera::OnInternalShutter);
         nRet = CreateProperty("InternalShutter", g_ShutterMode_Open, MM::String, false, pAct);
         assert(nRet == DEVICE_OK);
	   }

      vector<string> shutterValues;
      shutterValues.push_back(g_ShutterMode_Open);
      shutterValues.push_back(g_ShutterMode_Closed);
      nRet = SetAllowedValues("InternalShutter", shutterValues);
      if (nRet != DEVICE_OK)
         return nRet;
	   nRet = SetProperty("InternalShutter", shutterValues[0].c_str());
      if (nRet != DEVICE_OK)
         return nRet;
   }
   int ShutterMode = 1;  //0: auto, 1: open, 2: close
   ret = SetShutter(1, ShutterMode, 20,20);//Opened any way because some old AndorCamera has no flag for IsInternalMechanicalShutter


   // camera gain
   if(!HasProperty(MM::g_Keyword_Gain))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnGain);
      nRet = CreateProperty(MM::g_Keyword_Gain, "0", MM::Integer, false, pAct);
      assert(nRet == DEVICE_OK);
   }
   else
   {
	   nRet = SetProperty(MM::g_Keyword_Gain, "0");
      assert(nRet == DEVICE_OK);
   }

   if (isLuca) {
      pAct = new CPropertyAction(this, &AndorCamera::OnEMSwitch);
      nRet = CreateProperty("EMSwitch", "On", MM::String, false, pAct);
      assert (nRet == DEVICE_OK);
      AddAllowedValue("EMSwitch", "On");      
      AddAllowedValue("EMSwitch", "Off");
   }

   // readout mode
   int numSpeeds;
   ret = GetNumberHSSpeeds(0, 0, &numSpeeds);
   if (ret != DRV_SUCCESS)
      return ret;

   char speedBuf[100];
   readoutModes_.clear();
   for (int i=0; i<numSpeeds; i++)
   {
      float sp;
      ret = GetHSSpeed(0, 0, i, &sp); 
      if (ret != DRV_SUCCESS)
         return ret;
      sprintf(speedBuf, "%.1f MHz", sp);
      readoutModes_.push_back(speedBuf);
   }
   if (readoutModes_.empty())
      return ERR_INVALID_READOUT_MODE_SETUP;

   if(!HasProperty(MM::g_Keyword_ReadoutMode))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnReadoutMode);
	   if(numSpeeds>1)
         nRet = CreateProperty(MM::g_Keyword_ReadoutMode, readoutModes_[0].c_str(), MM::String, false, pAct);
	   else
         nRet = CreateProperty(MM::g_Keyword_ReadoutMode, readoutModes_[0].c_str(), MM::String, true, pAct);
      assert(nRet == DEVICE_OK);
   }
   nRet = SetAllowedValues(MM::g_Keyword_ReadoutMode, readoutModes_);
   nRet = SetProperty(MM::g_Keyword_ReadoutMode,readoutModes_[0].c_str());
   HSSpeedIdx_ = 0;


   //Daigang 24-may-2007
   // Pre-Amp-Gain
   int numPreAmpGain;
   ret = GetNumberPreAmpGains(&numPreAmpGain);
   if (ret != DRV_SUCCESS)
      return ret;
   char PreAmpGainBuf[10];
   PreAmpGains_.clear();
   for (int i=0; i<numPreAmpGain; i++)
   {
      float pag;
      ret = GetPreAmpGain(i, &pag); 
      if (ret != DRV_SUCCESS)
         return ret;
      sprintf(PreAmpGainBuf, "%.2f", pag);
      PreAmpGains_.push_back(PreAmpGainBuf);
   }
   if (PreAmpGains_.empty())
      return ERR_INVALID_PREAMPGAIN;

   if(!HasProperty("Pre-Amp-Gain"))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnPreAmpGain);
	   if(numPreAmpGain>1)
         nRet = CreateProperty("Pre-Amp-Gain", PreAmpGains_[numPreAmpGain-1].c_str(), MM::String, false, pAct);
	   else
         nRet = CreateProperty("Pre-Amp-Gain", PreAmpGains_[numPreAmpGain-1].c_str(), MM::String, true, pAct);
      assert(nRet == DEVICE_OK);
   }
   nRet = SetAllowedValues("Pre-Amp-Gain", PreAmpGains_);
   nRet = SetProperty("Pre-Amp-Gain", PreAmpGains_[PreAmpGains_.size()-1].c_str());
   PreAmpGain_ = PreAmpGains_[numPreAmpGain-1];
   if(numPreAmpGain > 1)
   {
      ret = SetPreAmpGain(numPreAmpGain-1);
      if (ret != DRV_SUCCESS)
         return ret;
   }
   //eof Daigang


   // Vertical Shift Speed
   int numVSpeed;
   ret = GetNumberVSSpeeds(&numVSpeed);
   if (ret != DRV_SUCCESS)
      return ret;

   char VSpeedBuf[10];
   VSpeeds_.clear();
   for (int i=0; i<numVSpeed; i++)
   {
      float vsp;
      ret = GetVSSpeed(i, &vsp); 
      if (ret != DRV_SUCCESS)
         return ret;
      sprintf(VSpeedBuf, "%.2f", vsp);
      VSpeeds_.push_back(VSpeedBuf);
   }
   if (VSpeeds_.empty())
      return ERR_INVALID_VSPEED;

   if(!HasProperty("VerticalSpeed"))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnVSpeed);
	   if(numVSpeed>1)
         nRet = CreateProperty("VerticalSpeed", VSpeeds_[numVSpeed-1].c_str(), MM::String, false, pAct);
      else
         nRet = CreateProperty("VerticalSpeed", VSpeeds_[numVSpeed-1].c_str(), MM::String, true, pAct);
      assert(nRet == DEVICE_OK);
   }
   nRet = SetAllowedValues("VerticalSpeed", VSpeeds_);
   assert(nRet == DEVICE_OK);
   nRet = SetProperty("VerticalSpeed", VSpeeds_[VSpeeds_.size()-1].c_str());
   VSpeed_ = VSpeeds_[numVSpeed-1];
   assert(nRet == DEVICE_OK);


   // Vertical Clock Voltage 
   int numVCVoltages;
   ret = GetNumberVSAmplitudes(&numVCVoltages);
   if (ret != DRV_SUCCESS) {
      numVCVoltages = 0;
      ostringstream eMsg;
      eMsg << "Andor driver returned error code: " << ret << " to GetNumberVSAmplitudes";
      LogMessage(eMsg.str().c_str(), true);
   }
   VCVoltages_.clear();
   if(numVCVoltages>5)
	   numVCVoltages = 5;
   switch(numVCVoltages)
   {
	   case 1:
		   VCVoltages_.push_back("Normal");
		   break;
	   case 2:
		   VCVoltages_.push_back("Normal");
		   VCVoltages_.push_back("+1");
		   break;
	   case 3:
		   VCVoltages_.push_back("Normal");
		   VCVoltages_.push_back("+1");
		   VCVoltages_.push_back("+2");
		   break;
	   case 4:
		   VCVoltages_.push_back("Normal");
		   VCVoltages_.push_back("+1");
		   VCVoltages_.push_back("+2");
		   VCVoltages_.push_back("+3");
		   break;
	   case 5:
		   VCVoltages_.push_back("Normal");
		   VCVoltages_.push_back("+1");
		   VCVoltages_.push_back("+2");
		   VCVoltages_.push_back("+3");
		   VCVoltages_.push_back("+4");
		   break;
	   default:
		   VCVoltages_.push_back("Normal");
   }
   if (numVCVoltages>=1)
   {
      if(!HasProperty("VerticalClockVoltage"))
      {
         pAct = new CPropertyAction (this, &AndorCamera::OnVCVoltage);
         if(numVCVoltages>1)
            nRet = CreateProperty("VerticalClockVoltage", VCVoltages_[0].c_str(), MM::String, false, pAct);
	      else
            nRet = CreateProperty("VerticalClockVoltage", VCVoltages_[0].c_str(), MM::String, true, pAct);
         assert(nRet == DEVICE_OK);
      }
      nRet = SetAllowedValues("VerticalClockVoltage", VCVoltages_);
      assert(nRet == DEVICE_OK);
      nRet = SetProperty("VerticalClockVoltage", VCVoltages_[0].c_str());
      VCVoltage_ = VCVoltages_[0];
      assert(nRet == DEVICE_OK);
   }


   // camera temperature
   // jizhen 05.08.2007
   // temperature range
   int minTemp, maxTemp;
   ret = GetTemperatureRange(&minTemp, &maxTemp);
   if (ret != DRV_SUCCESS)
      return ret;
   minTemp_ = minTemp;
   maxTemp_ = maxTemp;
   ostringstream tMin; 
   ostringstream tMax; 
   tMin << minTemp;
   tMax << maxTemp;


   //added to show some tips
   string strTips = "Wait for temperature to stabilize before acquisition.";
   if(!HasProperty(" Tip1"))
   {
      nRet = CreateProperty(" Tip1", strTips.c_str(), MM::String, true);
   }
   else
   {
	   nRet = SetProperty(" Tip1", strTips.c_str());
   }
   assert(nRet == DEVICE_OK);

   if(iCurrentTriggerMode == SOFTWARE)
   {
      strTips = "To maximize frame rate, do not change camera parameters except Exposure in Configuration Presets.";
      if(!HasProperty(" Tip2"))
      {
         nRet = CreateProperty(" Tip2", strTips.c_str(), MM::String, true);
      }
      else
      {
	      nRet = SetProperty(" Tip2", strTips.c_str());
      }
      assert(nRet == DEVICE_OK);
   }

   int temp;
   ret = GetTemperature(&temp);
   ostringstream strTemp;
   strTemp<<temp;
   if(!HasProperty(MM::g_Keyword_CCDTemperature))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnTemperature);
	   nRet = CreateProperty(MM::g_Keyword_CCDTemperature, strTemp.str().c_str(), MM::Integer, true, pAct);//Daigang 23-may-2007 changed back to read temperature only
   }
   else
   {
	   nRet = SetProperty(MM::g_Keyword_CCDTemperature,  strTemp.str().c_str());
   }
   assert(nRet == DEVICE_OK);


   // Temperature Set Point
   std::string strTempSetPoint;
   if (!isLuca) {      
      if(minTemp<-70)
	      strTempSetPoint = "-70";
      else
	      strTempSetPoint = TemperatureRangeMin_; 
      if(!HasProperty(MM::g_Keyword_CCDTemperatureSetPoint))
      {
         pAct = new CPropertyAction (this, &AndorCamera::OnTemperatureSetPoint);
         nRet = CreateProperty(MM::g_Keyword_CCDTemperatureSetPoint, strTempSetPoint.c_str(), MM::Integer, false, pAct);
         ret = SetPropertyLimits(MM::g_Keyword_CCDTemperatureSetPoint, minTemp_, maxTemp_);
      }
      else
      {
	      nRet = SetProperty(MM::g_Keyword_CCDTemperatureSetPoint, strTempSetPoint.c_str());
      }
      assert(nRet == DEVICE_OK);
   }

   // Cooler  
   if (!isLuca) {
      if(!HasProperty("CoolerMode"))
      {
         pAct = new CPropertyAction (this, &AndorCamera::OnCooler);
         nRet = CreateProperty("CoolerMode", g_CoolerMode_FanOffAtShutdown, MM::String, false, pAct); 
      }
     assert(nRet == DEVICE_OK);
      AddAllowedValue(/*Daigang 24-may-2007 "Cooler" */"CoolerMode", g_CoolerMode_FanOffAtShutdown);//"0");  //Daigang 24-may-2007
      AddAllowedValue(/*Daigang 24-may-2007 "Cooler" */"CoolerMode", g_CoolerMode_FanOnAtShutdown);//"1");  //Daigang 24-may-2007
      nRet = SetProperty("CoolerMode", g_CoolerMode_FanOffAtShutdown);
      assert(nRet == DEVICE_OK);
   }

   //jizhen 05.16.2007
   // Fan
   if(!HasProperty("FanMode"))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnFanMode);
      nRet = CreateProperty("FanMode", /*Daigang 24-may-2007 "0" */g_FanMode_Full, /*Daigang 24-may-2007 MM::Integer */MM::String, false, pAct); 
   }
   assert(nRet == DEVICE_OK);
   AddAllowedValue("FanMode", g_FanMode_Full);// "0"); // high  //Daigang 24-may-2007
   AddAllowedValue("FanMode", g_FanMode_Low);//"1"); // low  //Daigang 24-may-2007
   AddAllowedValue("FanMode", g_FanMode_Off);//"2"); // off  //Daigang 24-may-2007
   nRet = SetProperty("FanMode", g_FanMode_Full);
   assert(nRet == DEVICE_OK);
   // eof jizhen

   // frame transfer mode
   if(!HasProperty(g_FrameTransferProp))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnFrameTransfer);
      nRet = CreateProperty(g_FrameTransferProp, g_FrameTransferOff, MM::String, false, pAct); 
   }
   assert(nRet == DEVICE_OK);
   AddAllowedValue(g_FrameTransferProp, g_FrameTransferOff);
   AddAllowedValue(g_FrameTransferProp, g_FrameTransferOn);
   nRet = SetProperty(g_FrameTransferProp, g_FrameTransferOff);
   assert(nRet == DEVICE_OK);

   // actual interval
   // used by the application to get information on the actual camera interval
   if(!HasProperty(MM::g_Keyword_ActualInterval_ms))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnActualIntervalMS);
      nRet = CreateProperty(MM::g_Keyword_ActualInterval_ms, "0.0", MM::Float, false, pAct);
   }
   else
   {
      nRet = SetProperty(MM::g_Keyword_ActualInterval_ms, "0.0");
   }
   assert(nRet == DEVICE_OK);


   if(!HasProperty(MM::g_Keyword_ReadoutTime))
   {
      pAct = new CPropertyAction (this, &AndorCamera::OnReadoutTime);
      nRet = CreateProperty(MM::g_Keyword_ReadoutTime, "1", MM::Integer, true, pAct);
   }
   else
   {
      nRet = SetProperty(MM::g_Keyword_ReadoutTime, "1");
   }
   assert(nRet == DEVICE_OK);

   //baseline clmap
   if(caps.ulSetFunctions&AC_SETFUNCTION_BASELINEOFFSET) //some camera such as Luca might not support this
   {
      if(!HasProperty("BaselineClamp"))
      {
         pAct = new CPropertyAction (this, &AndorCamera::OnBaselineClamp);
         nRet = CreateProperty("BaselineClamp", "Enabled", MM::String, false, pAct);
         assert(nRet == DEVICE_OK);
      }
      BaselineClampValues_.clear();
      BaselineClampValues_.push_back("Enabled");
      BaselineClampValues_.push_back("Disabled");
      nRet = SetAllowedValues("BaselineClamp", BaselineClampValues_);
      assert(nRet == DEVICE_OK);
      nRet = SetProperty("BaselineClamp", BaselineClampValues_[0].c_str());
      BaselineClampValue_ = BaselineClampValues_[0];
      assert(nRet == DEVICE_OK);
  }

  //DMA parameters
   //if(caps.ulSetFunctions & AC_SETFUNCTION_DMAPARAMETERS)
   {
     int NumFramesPerDMA = 1;
     float SecondsPerDMA = 0.001f;
     ret = SetDMAParameters(NumFramesPerDMA, SecondsPerDMA);
     if (DRV_SUCCESS != ret)
       return (int)ret;
   }


   // synchronize all properties
   // --------------------------
  /*
   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;
      */

   // setup the buffer
   // ----------------
   nRet = ResizeImageBuffer();
   if (nRet != DEVICE_OK)
      return nRet;

   // explicitely set properties which are not readable from the camera
   nRet = SetProperty(MM::g_Keyword_Binning, "1");
   if (nRet != DEVICE_OK)
      return nRet;

   if(bShuterIntegrated)
   {
      nRet = SetProperty("InternalShutter", g_ShutterMode_Open);
      if (nRet != DEVICE_OK)
         return nRet;
   }

   if (!isLuca) {
     nRet = SetProperty(MM::g_Keyword_CCDTemperatureSetPoint, strTempSetPoint.c_str());
     if (nRet != DEVICE_OK)
         return nRet;
   }

   // EM gain
   // jizhen 05.08.2007
   // EMCCDGain range
   int EmCCDGainLow, EmCCDGainHigh;
   ret = GetEMGainRange(&EmCCDGainLow, &EmCCDGainHigh);
   if (ret != DRV_SUCCESS)
      return ret;
   EmCCDGainLow_ = EmCCDGainLow;
   EmCCDGainHigh_ = EmCCDGainHigh;


   nRet = SetProperty(MM::g_Keyword_Exposure, "10.0");
   if (nRet != DEVICE_OK)
      return nRet;

   nRet = SetProperty(MM::g_Keyword_ReadoutMode, readoutModes_[0].c_str());
   if (nRet != DEVICE_OK)
      return nRet;

   nRet = SetProperty("FanMode", g_FanMode_Full);
   if (nRet != DEVICE_OK)
      return nRet;

   nRet = SetProperty(g_FrameTransferProp, g_FrameTransferOff);
   if (nRet != DEVICE_OK)
      return nRet;

   if (!isLuca) {
     nRet = SetProperty("CoolerMode", g_CoolerMode_FanOffAtShutdown);
     if (nRet != DEVICE_OK)
        return nRet;
   }

   ret = CoolerON();  //turn on the cooler at startup
   if (DRV_SUCCESS != ret)
      return (int)ret;

   if(EmCCDGainHigh_>=300)
   {
      ret = SetEMAdvanced(1);  //Enable extended range of EMGain
      if (DRV_SUCCESS != ret)
         return (int)ret;
   }

   UpdateEMGainRange();
   GetReadoutTime();

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;
   return DEVICE_OK;
}

void AndorCamera::GetName(char* name) const 
{
   CDeviceUtils::CopyLimitedString(name, g_AndorName);
}

/**
 * Deactivate the camera, reverse the initialization process.
 */
int AndorCamera::Shutdown()
{
   if (initialized_)
   {
      SetToIdle();
      int ShutterMode = 2;  //0: auto, 1: open, 2: close
      SetShutter(1, ShutterMode, 20,20);//0, 0);
      CoolerOFF();  //Daigang 24-may-2007 turn off the cooler at shutdown
      ShutDown();
   }

   initialized_ = false;
   return DEVICE_OK;
}


double AndorCamera::GetExposure() const
{
   char Buf[MM::MaxStrLength];
   Buf[0] = '\0';
   GetProperty(MM::g_Keyword_Exposure, Buf);
   return atof(Buf);
}

void AndorCamera::SetExposure(double dExp)
{
   SetProperty(MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(dExp));
}


//added to use RTA
/**
 * Acquires a single frame.
 * Micro-Manager expects that this function blocks the calling thread until the exposure phase is over.
 */
int AndorCamera::SnapImage()
{
   bool acquiring = acquiring_;
   if (acquiring)
      StopSequenceAcquisition();

   if (acquiring_)
      return ERR_BUSY_ACQUIRING;

   unsigned ret;
   if(!IsAcquiring())
   {
	   if(bFrameTransfer_ && (iCurrentTriggerMode == SOFTWARE))
		   ret = SetFrameTransferMode(0);  //Software trigger mode can not be used in FT mode

      GetReadoutTime();

	   ret = ::StartAcquisition();
      if (ret != DRV_SUCCESS)
         return ret;
   }
   if(iCurrentTriggerMode == SOFTWARE) // Change to turned on
   {
      SetExposure_();
      ret = SendSoftwareTrigger();
      if (ret != DRV_SUCCESS)
         return ret;
   }
   else
   {
     ret = WaitForAcquisition();
	 if(ret != DRV_SUCCESS)
		 return ret;
   }
   if(iCurrentTriggerMode != SOFTWARE) 
   {
     pImgBuffer_ = GetAcquiredImage(); 
   }
   else
   {
   pImgBuffer_ = GetImageBuffer_();
   }
   if(iCurrentTriggerMode == SOFTWARE)
      CDeviceUtils::SleepMs(KeepCleanTime_);

   if (acquiring)
      StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);


   return DEVICE_OK;
}


//dded to use RTA
/**
 * Returns the raw image buffer.
 */ 
unsigned char* AndorCamera::GetImageBuffer_()
{
   if(!IsAcquiring())
   {
      unsigned ret = ::StartAcquisition();
      if (ret != DRV_SUCCESS)
         return 0;
   }

   long startT = GetTickCount();
   long Timeout = (long)((expMs_ + ReadoutTime_) * 3);
   long delta = 0;

   int TimeoutCnt = 0;

   assert(fullFrameBuffer_ != 0);
   unsigned int ret = GetNewData16((WORD*)fullFrameBuffer_, roi_.xSize/binSize_ * roi_.ySize/binSize_);
   while (ret == DRV_NO_NEW_DATA)
   {
      if(!IsAcquiring())
      {
         unsigned ret = ::StartAcquisition();
         if (ret != DRV_SUCCESS)
            return 0;
      }

      delta = GetTickCount() - startT;
      if(delta > Timeout)
	   {
		   TimeoutCnt++;
		   if(TimeoutCnt>10)
		   {
		      unsigned char* rawBuffer = const_cast<unsigned char*> (img_.GetPixels());
			   memset(rawBuffer,0,img_.Width() * img_.Height() * img_.Depth());
			   return rawBuffer;
		   }
		   else
		   {
		      startT = GetTickCount();
		   }
         if(iCurrentTriggerMode == SOFTWARE)
		   {
            //unsigned ret1 = SendSoftwareTrigger();
            //if (ret1 != DRV_SUCCESS)
		      {
			      ;//for debug
		      }
		   }
	   }
      ret = GetNewData16((WORD*)fullFrameBuffer_, roi_.xSize/binSize_ * roi_.ySize/binSize_);
   }
   assert(img_.Depth() == 2);
   unsigned char* rawBuffer = const_cast<unsigned char*> (img_.GetPixels());
   memcpy(rawBuffer, fullFrameBuffer_, img_.Width() * img_.Height() * img_.Depth());

   // capture complete
   return (unsigned char*)rawBuffer;
}


const unsigned char* AndorCamera::GetImageBuffer()
{
   assert(img_.Depth() == 2);
   assert(pImgBuffer_!=0);
   unsigned char* rawBuffer = pImgBuffer_;
   // process image
   MM::ImageProcessor* ip = GetCoreCallback()->GetImageProcessor(this);
   if (ip)
   {
      int ret = ip->Process((unsigned char*) rawBuffer, img_.Width(), img_.Height(), img_.Depth());
      if (ret != DEVICE_OK)
         return 0;
   }
   return (unsigned char*)rawBuffer;
}
unsigned char* AndorCamera::GetAcquiredImage() {

   assert(fullFrameBuffer_ != 0);
   unsigned int ret = GetNewData16((WORD*)fullFrameBuffer_, roi_.xSize/binSize_ * roi_.ySize/binSize_);
   if(ret != DRV_SUCCESS) {
	   return 0;
   }

   return (unsigned char*)fullFrameBuffer_;
}


/**
 * Readout time
 */ 
long AndorCamera::GetReadoutTime()
{
   long ReadoutTime;
   float fReadoutTime;
   if(fpGetReadOutTime!=0 && (iCurrentTriggerMode == SOFTWARE))
   {
	   fpGetReadOutTime(&fReadoutTime);
      ReadoutTime = long(fReadoutTime * 1000);
   }
   else
   {
      unsigned ret = SetExposureTime(0.0);
      if (DRV_SUCCESS != ret)
         return (int)ret;
      float fExposure, fAccumTime, fKineticTime;
      GetAcquisitionTimings(&fExposure,&fAccumTime,&fKineticTime);
	   ReadoutTime = long(fKineticTime * 1000.0);
      ret = SetExposureTime((float)(expMs_ / 1000.0));
      if (DRV_SUCCESS != ret)
         return (int)ret;
   }
   if(ReadoutTime<=0)
	   ReadoutTime=35;
   ReadoutTime_ = ReadoutTime;

   float fExposure, fAccumTime, fKineticTime;
   GetAcquisitionTimings(&fExposure,&fAccumTime,&fKineticTime);
   ActualInterval_ms_ = fKineticTime * 1000.0f;
   SetProperty(MM::g_Keyword_ActualInterval_ms, CDeviceUtils::ConvertToString(ActualInterval_ms_)); 

//whenever readout needs update, keepcleantime also needs update
   long KeepCleanTime;
   float fKeepCleanTime;
   if(fpGetKeepCleanTime!=0 && (iCurrentTriggerMode == SOFTWARE))
   {
	   fpGetKeepCleanTime(&fKeepCleanTime);
      KeepCleanTime = long(fKeepCleanTime * 1000);
   }
   else
	   KeepCleanTime=10;
   if(KeepCleanTime<=0)
	   KeepCleanTime=10;
   KeepCleanTime_ = KeepCleanTime;


   return ReadoutTime_;
}


/**
 * Sets the image Region of Interest (ROI).
 * The internal representation of the ROI uses the full frame coordinates
 * in combination with binning factor.
 */
int AndorCamera::SetROI(unsigned uX, unsigned uY, unsigned uXSize, unsigned uYSize)
{
   if (Busy())
      return ERR_BUSY_ACQUIRING;

	//added to use RTA
	SetToIdle();

   ROI oldRoi = roi_;

   roi_.x = uX * binSize_;
   roi_.y = uY * binSize_;
   roi_.xSize = uXSize * binSize_;
   roi_.ySize = uYSize * binSize_;

   if (roi_.x + roi_.xSize > fullFrameX_ || roi_.y + roi_.ySize > fullFrameY_)
   {
      roi_ = oldRoi;
      return ERR_INVALID_ROI;
   }

   // adjust image extent to conform to the bin size
   roi_.xSize -= roi_.xSize % binSize_;
   roi_.ySize -= roi_.ySize % binSize_;

   unsigned uret = SetImage(binSize_, binSize_, roi_.x+1, roi_.x+roi_.xSize,
                                       roi_.y+1, roi_.y+roi_.ySize);
   if (uret != DRV_SUCCESS)
   {
      roi_ = oldRoi;
      return uret;
   }

   GetReadoutTime();

   int ret = ResizeImageBuffer();
   if (ret != DEVICE_OK)
   {
      roi_ = oldRoi;
      return ret;
   }

   return DEVICE_OK;
}

unsigned AndorCamera::GetBitDepth() const
{
   int depth;
   // TODO: channel 0 hardwired ???
   unsigned ret = ::GetBitDepth(ADChannelIndex_, &depth);
   if (ret != DRV_SUCCESS)
      depth = 0;
   return depth;
}

int AndorCamera::GetBinning () const
{
   return binSize_;
}

int AndorCamera::SetBinning (int binSize) 
{
   ostringstream os;                                                         
   os << binSize;
   return SetProperty(MM::g_Keyword_Binning, os.str().c_str());                                                                                     
} 

int AndorCamera::GetROI(unsigned& uX, unsigned& uY, unsigned& uXSize, unsigned& uYSize)
{
   uX = roi_.x / binSize_;
   uY = roi_.y / binSize_;
   uXSize = roi_.xSize / binSize_;
   uYSize = roi_.ySize / binSize_;

   return DEVICE_OK;
}

int AndorCamera::ClearROI()
{
   if (acquiring_)
      return ERR_BUSY_ACQUIRING;

	//added to use RTA
	SetToIdle();

   roi_.x = 0;
   roi_.y = 0;
   roi_.xSize = fullFrameX_;
   roi_.ySize = fullFrameY_;

   // adjust image extent to conform to the bin size
   roi_.xSize -= roi_.xSize % binSize_;
   roi_.ySize -= roi_.ySize % binSize_;
   unsigned uret = SetImage(binSize_, binSize_, roi_.x+1, roi_.x+roi_.xSize,
                                       roi_.y+1, roi_.y+roi_.ySize);
   if (uret != DRV_SUCCESS)
      return uret;


   GetReadoutTime();

   int ret = ResizeImageBuffer();
   if (ret != DEVICE_OK)
      return ret;

   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Action handlers
// ~~~~~~~~~~~~~~~

/**
 * Set the directory for the Andor native driver dll.
 */
/*int AndorCamera::OnDriverDir(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(driverDir_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(driverDir_);
   }
   return DEVICE_OK;
}
*/

/**
 * Set binning.
 */
int AndorCamera::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

	   //added to use RTA
	   SetToIdle();

      long bin;
      pProp->Get(bin);
      if (bin <= 0)
         return DEVICE_INVALID_PROPERTY_VALUE;

      // adjust roi to accomodate the new bin size
      ROI oldRoi = roi_;
      roi_.xSize = fullFrameX_;
      roi_.ySize = fullFrameY_;
      roi_.x = 0;
      roi_.y = 0;

      // adjust image extent to conform to the bin size
      roi_.xSize -= roi_.xSize % bin;
      roi_.ySize -= roi_.ySize % bin;

      // setting the binning factor will reset the image to full frame
      unsigned aret = SetImage(bin, bin, roi_.x+1, roi_.x+roi_.xSize,
                                         roi_.y+1, roi_.y+roi_.ySize);
      if (aret != DRV_SUCCESS)
      {
         roi_ = oldRoi;
         return aret;
      }

      GetReadoutTime();


      // apply new settings
      binSize_ = (int)bin;
      int ret = ResizeImageBuffer();
      if (ret != DEVICE_OK)
      {
         roi_ = oldRoi;
         return ret;
      }
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)binSize_);
   }
   return DEVICE_OK;
}

/**
 * Set camera exposure (milliseconds).
 */
int AndorCamera::OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   // exposure property is stored in milliseconds,
   // while the driver returns the value in seconds
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(currentExpMS_);
   }
   else if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      double exp;
      pProp->Get(exp);

      if(fabs(exp-currentExpMS_)<0.001)
         return DEVICE_OK;
	   currentExpMS_ = exp;

      if(!(iCurrentTriggerMode == SOFTWARE))
	   {
	      SetToIdle();
         unsigned ret = SetExposureTime((float)(exp / 1000.0));
         if (DRV_SUCCESS != ret)
            return (int)ret;
         expMs_ = exp;
	   } else
          SetExposure_();

      if (acquiring)
         StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
   }

   return DEVICE_OK;
}

/**
 * Set camera exposure (milliseconds).
 */
int AndorCamera::SetExposure_()
{
   if (acquiring_)
      return ERR_BUSY_ACQUIRING;

   if(!(iCurrentTriggerMode == SOFTWARE))
	  return DEVICE_OK;

   if(fabs(expMs_-currentExpMS_)<0.001)
	  return DEVICE_OK;

   CDeviceUtils::SleepMs(KeepCleanTime_);
   unsigned ret = SetExposureTime((float)(currentExpMS_ / 1000.0));
   if (DRV_SUCCESS != ret)
         return (int)ret;
   expMs_ = currentExpMS_;

   return DEVICE_OK;
}


/**
 * Set camera pixel type. 
 * We support only 16-bit mode here.
 */
int AndorCamera::OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
      pProp->Set(g_PixelType_16bit);
   return DEVICE_OK;
}

/**
 * Set readout mode.
 */
int AndorCamera::OnReadoutMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

	   //added to use RTA
	   SetToIdle();

      string mode;
      pProp->Get(mode);
      for (unsigned i=0; i<readoutModes_.size(); ++i)
         if (readoutModes_[i].compare(mode) == 0)
         {
            unsigned ret = SetHSSpeed(OutputAmplifierIndex_, i);
            if (DRV_SUCCESS != ret)
               return (int)ret;
            else
			{
               HSSpeedIdx_ = i;
               GetReadoutTime();
               if (acquiring)
                  StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
               return DEVICE_OK;
			}
         }
      assert(!"Unrecognized readout mode");
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(readoutModes_[HSSpeedIdx_].c_str());
   }
   return DEVICE_OK;
}

/**
 * Provides information on readout time.
 * TODO: Not implemented
 */
int AndorCamera::OnReadoutTime(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(ReadoutTime_);
   }

   return DEVICE_OK;
}


/**
 * Set camera "regular" gain.
 */
int AndorCamera::OnGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long gain;
      pProp->Get(gain);
      if (!EMSwitch_) {
         currentGain_ = gain;
         return DEVICE_OK;
      }
      if(gain == currentGain_)
		   return DEVICE_OK;

      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

	   if (gain!=0 && gain < (long) EmCCDGainLow_ ) 
         gain = (long)EmCCDGainLow_;
      if (gain > (long) EmCCDGainHigh_ ) 
         gain = (long)EmCCDGainHigh_;
	   pProp->Set(gain);

	   //added to use RTA
      if(!(iCurrentTriggerMode == SOFTWARE))
         SetToIdle();

      unsigned ret = SetEMCCDGain((int)gain);
      if (DRV_SUCCESS != ret)
         return (int)ret;
	   currentGain_ = gain;

      if (acquiring)
         StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(currentGain_);
   }
   return DEVICE_OK;
}

/**
 * Set camera "regular" gain.
 */
int AndorCamera::OnEMSwitch(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      std::string EMSwitch;
      pProp->Get(EMSwitch);
      if (EMSwitch == "Off" && !EMSwitch_)
         return DEVICE_OK;
      if (EMSwitch == "On" && EMSwitch_)
         return DEVICE_OK;

      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      //pProp->Get(currentGain_);

	   //added to use RTA
      if(!(iCurrentTriggerMode == SOFTWARE))
    	   SetToIdle();

      unsigned ret = DRV_SUCCESS;
      if (EMSwitch == "On") {
         ret = SetEMCCDGain((int)currentGain_);
         EMSwitch_ = true;
      } else {
         ret = SetEMCCDGain(0);
         EMSwitch_ = false;
      }
      if (DRV_SUCCESS != ret)
         return (int)ret;

      if (acquiring)
         StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);

   }
   else if (eAct == MM::BeforeGet)
   {
      if (EMSwitch_)
	      pProp->Set("On");
      else
         pProp->Set("Off");
   }
   return DEVICE_OK;
}


/**
 * Enable or Disable Software Trigger.
 */
int AndorCamera::OnSelectTrigger(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      std::string trigger;
      pProp->Get(trigger);
      if(trigger == strCurrentTriggerMode)
         return DEVICE_OK;

      strCurrentTriggerMode = trigger;

      SetToIdle();

      int ret;


      if(trigger == "Software")
      {
         iCurrentTriggerMode = SOFTWARE;
         ret = SetTriggerMode(10);//software trigger mode
         if (ret != DRV_SUCCESS)
            return ret;
         ret = SetAcquisitionMode(5);//RTA
         if (ret != DRV_SUCCESS)
            return ret;
	  }
	  else if(trigger == "External")
	  {
         iCurrentTriggerMode = EXTERNAL;
		  ret = SetAcquisitionMode(1);//SingleScan
        if (ret != DRV_SUCCESS)
           return ret;
		  ret = SetTriggerMode(1);//internal trigger mode
        if (ret != DRV_SUCCESS)
            return ret;
	  }
	  else
	  {
          iCurrentTriggerMode = INTERNAL;
		  ret = SetAcquisitionMode(1);//SingleScan
        if (ret != DRV_SUCCESS)
           return ret;
		  ret = SetTriggerMode(0);//internal trigger mode
        if (ret != DRV_SUCCESS)
            return ret;
	  }
     if (acquiring)
        StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);

   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(strCurrentTriggerMode.c_str());
   }
   return DEVICE_OK;
}

//Daigang 24-may-2007
/**
 * Set camera pre-amp-gain.
 */
int AndorCamera::OnPreAmpGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;
   	
      //added to use RTA
	   SetToIdle();

      string PreAmpGain;
      pProp->Get(PreAmpGain);
      for (unsigned i=0; i<PreAmpGains_.size(); ++i)
      {
         if (PreAmpGains_[i].compare(PreAmpGain) == 0)
         {
            unsigned ret = SetPreAmpGain(i);
            if (DRV_SUCCESS != ret)
               return (int)ret;
            else
			   {
               if (acquiring)
                  StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
               PreAmpGain_=PreAmpGain;
               return DEVICE_OK;
            }
         }
      }
      assert(!"Unrecognized Pre-Amp-Gain");
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(PreAmpGain_.c_str());
   }
   return DEVICE_OK;
}
//eof Daigang

/**
 * Set camera Vertical Clock Voltage
 */
int AndorCamera::OnVCVoltage(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      //added to use RTA
	   SetToIdle();

      string VCVoltage;
      pProp->Get(VCVoltage);
      for (unsigned i=0; i<VCVoltages_.size(); ++i)
      {
         if (VCVoltages_[i].compare(VCVoltage) == 0)
         {
            unsigned ret = SetVSAmplitude(i);
            if (DRV_SUCCESS != ret)
               return (int)ret;
            else
            {
               if (acquiring)
                  StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
               VCVoltage_=VCVoltage;
               return DEVICE_OK;
            }
         }
      }
      assert(!"Unrecognized Vertical Clock Voltage");
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(VCVoltage_.c_str());
   }
   return DEVICE_OK;
}

/**
 * Set camera Baseline Clamp.
 */
int AndorCamera::OnBaselineClamp(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;
	   
      //added to use RTA
	   SetToIdle();

      string BaselineClampValue;
      pProp->Get(BaselineClampValue);
      for (unsigned i=0; i<BaselineClampValues_.size(); ++i)
      {
         if (BaselineClampValues_[i].compare(BaselineClampValue) == 0)
         {
            int iState = 1; 
		      if(i==0)
				   iState = 1;  //Enabled
			   if(i==1)
			   	iState = 0;  //Disabled
            unsigned ret = SetBaselineClamp(iState);
            if (DRV_SUCCESS != ret)
               return (int)ret;
            else
            {
               if (acquiring)
                  StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
               BaselineClampValue_=BaselineClampValue;
               return DEVICE_OK;
            }
         }
      }
      assert(!"Unrecognized BaselineClamp");
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(BaselineClampValue_.c_str());
   }
   return DEVICE_OK;
}


/**
 * Set camera vertical shift speed.
 */
int AndorCamera::OnVSpeed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

	   //added to use RTA
	   SetToIdle();

      string VSpeed;
      pProp->Get(VSpeed);
      for (unsigned i=0; i<VSpeeds_.size(); ++i)
      {
         if (VSpeeds_[i].compare(VSpeed) == 0)
         {
            unsigned ret = SetVSSpeed(i);
            if (DRV_SUCCESS != ret)
               return (int)ret;
            else
            {
               GetReadoutTime();
               if (acquiring)
                  StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
               VSpeed_ = VSpeed;
               return DEVICE_OK;
			   }
         }
      }
      assert(!"Unrecognized Vertical Speed");
   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(VSpeed_.c_str());
   }
   return DEVICE_OK;
}


/**
 * Obtain temperature in Celsius.
 */
int AndorCamera::OnTemperature(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {

	  /* //Daigang 23-may-2007 removed for readonly
      //jizhen 05.10.2007
      long temp;
      pProp->Get(temp);
	  if (temp < (long) minTemp_ ) temp = (long)minTemp_;
      if (temp > (long) maxTemp_ ) temp = (long)maxTemp_;
      unsigned ret = SetTemperature((int)temp);
      if (DRV_SUCCESS != ret)
         return (int)ret;
      ret = CoolerON();
      if (DRV_SUCCESS != ret)
         return (int)ret;
	  // eof jizhen
	  */
   }
   else if (eAct == MM::BeforeGet)
   {
      int temp;
	  //Daigang 24-may-2007
	  //GetTemperature(&temp);
	  unsigned ret = GetTemperature(&temp);
	  if(ret == DRV_TEMP_STABILIZED)
		  ThermoSteady_ = true;
	  else if(ret == DRV_TEMP_OFF || ret == DRV_TEMP_NOT_REACHED)
		  ThermoSteady_ = false;
	  //eof Daigang

      pProp->Set((long)temp);
   }
   return DEVICE_OK;
}

//Daigang 23-May-2007
/**
 * Set temperature setpoint in Celsius.
 */
int AndorCamera::OnTemperatureSetPoint(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;
   	
      //added to use RTA
	   SetToIdle();

      long temp;
      pProp->Get(temp);
      if (temp < (long) minTemp_ )
         temp = (long)minTemp_;
      if (temp > (long) maxTemp_ ) 
         temp = (long)maxTemp_;
      unsigned ret = SetTemperature((int)temp);
      if (DRV_SUCCESS != ret)
         return (int)ret;
      ostringstream strTempSetPoint;
      strTempSetPoint<<temp;
      TemperatureSetPoint_ = strTempSetPoint.str();

      UpdateEMGainRange(); 

      if (acquiring)
          StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);


   }
   else if (eAct == MM::BeforeGet)
   {
	   pProp->Set(TemperatureSetPoint_.c_str());
   }
   return DEVICE_OK;
}



//jizhen 05.11.2007
/**
 * Set cooler on/off.
 */
int AndorCamera::OnCooler(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      //added to use RTA
	   SetToIdle();

      string mode;
      pProp->Get(mode);
      int modeIdx = 0;
      if (mode.compare(g_CoolerMode_FanOffAtShutdown) == 0)
         modeIdx = 0;
      else if (mode.compare(g_CoolerMode_FanOnAtShutdown) == 0)
         modeIdx = 1;
      else
         return DEVICE_INVALID_PROPERTY_VALUE;

      // wait for camera to finish acquiring
      int status = DRV_IDLE;
      unsigned ret = GetStatus(&status);
      while (status == DRV_ACQUIRING && ret == DRV_SUCCESS)
         ret = GetStatus(&status);

      ret = SetCoolerMode(modeIdx);
      if (ret != DRV_SUCCESS)
         return (int)ret;

      if (acquiring)
          StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
   }
   else if (eAct == MM::BeforeGet)
   {

   }
   return DEVICE_OK;
}
// eof jizhen

//jizhen 05.16.2007
/**
 * Set fan mode.
 */
int AndorCamera::OnFanMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      //added to use RTA
   	SetToIdle();

      string mode;
      pProp->Get(mode);
      int modeIdx = 0;
      if (mode.compare(g_FanMode_Full) == 0)
         modeIdx = 0;
      else if (mode.compare(g_FanMode_Low) == 0)
         modeIdx = 1;
      else if (mode.compare(g_FanMode_Off) == 0)
         modeIdx = 2;
      else
         return DEVICE_INVALID_PROPERTY_VALUE;

      // wait for camera to finish acquiring
      int status = DRV_IDLE;
      unsigned ret = GetStatus(&status);
      while (status == DRV_ACQUIRING && ret == DRV_SUCCESS)
         ret = GetStatus(&status);

      ret = SetFanMode(modeIdx);
      if (ret != DRV_SUCCESS)
         return (int)ret;

      if (acquiring)
          StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);
   }
   else if (eAct == MM::BeforeGet)
   {
   }
   return DEVICE_OK;
}
// eof jizhen

int AndorCamera::OnInternalShutter(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;
   
      //added to use RTA
      SetToIdle();

      string mode;
      pProp->Get(mode);
      int modeIdx = 0;
      if (mode.compare(g_ShutterMode_Auto) == 0)
         modeIdx = 0;
      else if (mode.compare(g_ShutterMode_Open) == 0)
         modeIdx = 1;
      else if (mode.compare(g_ShutterMode_Closed) == 0)
         modeIdx = 2;
      else
         return DEVICE_INVALID_PROPERTY_VALUE;

      // wait for camera to finish acquiring
      int status = DRV_IDLE;
      unsigned ret = GetStatus(&status);
      while (status == DRV_ACQUIRING && ret == DRV_SUCCESS)
         ret = GetStatus(&status);

      // the first parameter in SetShutter, must be "1" in order for
      // the shutter logic to work as described in the documentation
      ret = SetShutter(1, modeIdx, 20,20);//0, 0);
      if (ret != DRV_SUCCESS)
         return (int)ret;

      if (acquiring)
          StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);

   }
   else if (eAct == MM::BeforeGet)
   {
   }
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Utility methods
///////////////////////////////////////////////////////////////////////////////

int AndorCamera::ResizeImageBuffer()
{
   // resize internal buffers
   // NOTE: we are assuming 16-bit pixel type
   const int bytesPerPixel = 2;
   img_.Resize(roi_.xSize / binSize_, roi_.ySize / binSize_, bytesPerPixel);
   return DEVICE_OK;
}


//daigang 24-may-2007
void AndorCamera::UpdateEMGainRange()
{
	//added to use RTA
	SetToIdle();

   int EmCCDGainLow, EmCCDGainHigh;
   unsigned ret = GetEMGainRange(&EmCCDGainLow, &EmCCDGainHigh);
   if (ret != DRV_SUCCESS)
      return;
   EmCCDGainLow_ = EmCCDGainLow;
   EmCCDGainHigh_ = EmCCDGainHigh;
   ostringstream emgLow; 
   ostringstream emgHigh; 
   emgLow << EmCCDGainLow;
   emgHigh << EmCCDGainHigh;
   /*
   int nRet = SetProperty("EMGainRangeMin", emgLow.str().c_str());
   if (nRet != DEVICE_OK)
      return;
   nRet = SetProperty("EMGainRangeMax", emgHigh.str().c_str());
   if (nRet != DEVICE_OK)
      return;
   */
   ret = SetPropertyLimits(MM::g_Keyword_Gain, EmCCDGainLow, EmCCDGainHigh);
   if (ret != DEVICE_OK)
      return;

}
//eof Daigang

//daigang 24-may-2007
/**
 * EMGain Range Max
 */
int AndorCamera::OnEMGainRangeMax(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)EmCCDGainHigh_);
   }
   return DEVICE_OK;
}
//eof Daigang

/**
 * ActualInterval_ms
 */
int AndorCamera::OnActualIntervalMS(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      double ActualInvertal_ms;
      pProp->Get(ActualInvertal_ms);
      if(ActualInvertal_ms == ActualInterval_ms_)
         return DEVICE_OK;
	   pProp->Set(ActualInvertal_ms);
	   ActualInterval_ms_ = (float)ActualInvertal_ms;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(CDeviceUtils::ConvertToString(ActualInterval_ms_));
   }
   return DEVICE_OK;
}

//daigang 24-may-2007
/**
 * EMGain Range Max
 */
int AndorCamera::OnEMGainRangeMin(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)EmCCDGainLow_);
   }
   return DEVICE_OK;
}
//eof Daigang




/**
 * Frame transfer mode ON or OFF.
 */
int AndorCamera::OnFrameTransfer(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      SetToIdle();

      string mode;
      pProp->Get(mode);
      int modeIdx = 0;
      if (mode.compare(g_FrameTransferOn) == 0)
	  {
         modeIdx = 1;
		 bFrameTransfer_ = true;
	  }
      else if (mode.compare(g_FrameTransferOff) == 0)
	  {
         modeIdx = 0;
		 bFrameTransfer_ = false;
	  }

      else
         return DEVICE_INVALID_PROPERTY_VALUE;

      // wait for camera to finish acquiring
      int status = DRV_IDLE;
      unsigned ret = GetStatus(&status);
      while (status == DRV_ACQUIRING && ret == DRV_SUCCESS)
         ret = GetStatus(&status);

      ret = SetFrameTransferMode(modeIdx);
      if (ret != DRV_SUCCESS)
         return ret;

      if (acquiring)
          StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);

   }
   else if (eAct == MM::BeforeGet)
   {
      // use cached value
   }
   return DEVICE_OK;
}



/**
 * Set caemra to idle
 */
void AndorCamera::SetToIdle()
{
   if(!initialized_ || !IsAcquiring())
      return;
   unsigned ret = AbortAcquisition();
   if (ret != DRV_SUCCESS)
      CheckError(ret);
}


/**
 * check if camera is acquiring
 */
bool AndorCamera::IsAcquiring()
{
   if(!initialized_)
      return 0;

   int status = DRV_IDLE;
   GetStatus(&status);
   if (status == DRV_ACQUIRING)
      return true;
   else
      return false;

}



/**
 * check if camera is thermosteady
 */
bool AndorCamera::IsThermoSteady()
{
	return ThermoSteady_;
}

void AndorCamera::CheckError(unsigned int /*errorVal*/)
{
}

/**
 * Set output amplifier.
 */
int AndorCamera::OnOutputAmplifier(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      SetToIdle();

      string strAmp;
      pProp->Get(strAmp);
      int AmpIdx = 0;
      if (strAmp.compare(g_OutputAmplifier_EM) == 0)
         AmpIdx = 0;
      else if (strAmp.compare(g_OutputAmplifier_Conventional) == 0)
         AmpIdx = 1;
      else
         return DEVICE_INVALID_PROPERTY_VALUE;

	   OutputAmplifierIndex_ = AmpIdx;

     	UpdateHSSpeeds();

      unsigned ret = SetOutputAmplifier(AmpIdx);
      if (ret != DRV_SUCCESS)
         return (int)ret;

      if (acquiring)
          StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);

      if (initialized_)
         return OnPropertiesChanged();
   }
   else if (eAct == MM::BeforeGet)
   {
   }
   return DEVICE_OK;
}



/**
 * Set output amplifier.
 */
int AndorCamera::OnADChannel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool acquiring = acquiring_;
      if (acquiring)
         StopSequenceAcquisition();

      if (acquiring_)
         return ERR_BUSY_ACQUIRING;

      SetToIdle();

	   string strADChannel;
      pProp->Get(strADChannel);
      int ADChannelIdx = 0;
      if (strADChannel.compare(g_ADChannel_14Bit) == 0)
         ADChannelIdx = 0;
      else if (strADChannel.compare(g_ADChannel_16Bit) == 0)
         ADChannelIdx = 1;
      else
         return DEVICE_INVALID_PROPERTY_VALUE;

      ADChannelIndex_ = ADChannelIdx;

      unsigned int ret = SetADChannel(ADChannelIdx);
      if (ret != DRV_SUCCESS)
         return (int)ret;

	   UpdateHSSpeeds();

      if (acquiring)
          StartSequenceAcquisition(sequenceLength_ - imageCounter_, intervalMs_, stopOnOverflow_);


     if (initialized_)
        return OnPropertiesChanged();
   }
   else if (eAct == MM::BeforeGet)
   {
   }
   return DEVICE_OK;
}

//daigang 24-may-2007
void AndorCamera::UpdateHSSpeeds()
{
	//Daigang 28-may-2007 added to use RTA
	SetToIdle();

   int numSpeeds;
   unsigned ret = GetNumberHSSpeeds(ADChannelIndex_, OutputAmplifierIndex_, &numSpeeds);
   if (ret != DRV_SUCCESS)
      return;

   char speedBuf[100];
   readoutModes_.clear();
   for (int i=0; i<numSpeeds; i++)
   {
      float sp;
      ret = GetHSSpeed(ADChannelIndex_, OutputAmplifierIndex_, i, &sp); 
      if (ret != DRV_SUCCESS)
         return;
      sprintf(speedBuf, "%.1f MHz", sp);
      readoutModes_.push_back(speedBuf);
   }
   SetAllowedValues(MM::g_Keyword_ReadoutMode, readoutModes_);

   if(HSSpeedIdx_ >= (int)readoutModes_.size())
   {
	   HSSpeedIdx_ = 0;
   }
   ret = SetHSSpeed(OutputAmplifierIndex_, HSSpeedIdx_);
   if (ret == DRV_SUCCESS)
     SetProperty(MM::g_Keyword_ReadoutMode,readoutModes_[HSSpeedIdx_].c_str());

   GetReadoutTime();


}



///////////////////////////////////////////////////////////////////////////////
// Continuous acquisition
//

/**
 * Continuous acquisition thread service routine.
 * Starts acquisition on the AndorCamera and repeatedly calls PushImage()
 * to transfer any new images to the MMCore circularr buffer.
 */
int AcqSequenceThread::svc(void)
{
   long acc;
   long series(0);
   long seriesInit;
   unsigned ret;

   printf("Starting Andor svc\n");
   DWORD timePrev = GetTickCount();
   ret = GetAcquisitionProgress(&acc, &seriesInit);
   std::ostringstream os;
   os << "GetAcquisitionProgress returned: " << acc << " and: " << seriesInit;
   printf ("%s\n", os.str().c_str());
   os.str("");

   if (ret != DRV_SUCCESS)
   {
      camera_->StopCameraAcquisition();
      os << "Error in GetAcquisitionProgress: " << ret;
      printf("%s\n", os.str().c_str());
      //core_->LogMessage(camera_, os.str().c_str(), true);
      return ret;
   }

   /*
   float fExposure, fAccumTime, fKineticTime;
   printf ("Before GetAcquisition timings\n");
   ret = GetAcquisitionTimings(&fExposure,&fAccumTime,&fKineticTime);
   if (ret != DRV_SUCCESS)
      printf ("Error in GetAcquisition Timings\n");
   os << "Exposure: " << fExposure << " AcummTime: " << fAccumTime << " KineticTime: " << fKineticTime;
   printf ("%s\n", os.str().c_str());
   os.str("")
   float ActualInterval_ms = fKineticTime * 1000.0f;
   waitTime = (long) (ActualInterval_ms / 5);

   os << "WaitTime: " << waitTime;
   //core_->LogMessage(camera_, os.str().c_str(), true);
   printf("%s\n", os.str().c_str());
   os.str("");
   */

   // wait for frames to start coming in
   do
   {
      ret = GetAcquisitionProgress(&acc, &series);
      if (ret != DRV_SUCCESS)
      {
         camera_->StopCameraAcquisition();
         os << "Error in GetAcquisitionProgress: " << ret;
         printf("%s\n", os.str().c_str());
         os.str("");
         return ret;
      }
      Sleep(waitTime_);
   } while (series == seriesInit && !stop_);
   os << "Images appearing";
   printf("%s\n", os.str().c_str());
   os.str("");
   long seriesPrev = 0;
   long frmcnt = 0;
   do
   {
      //GetStatus(&status);
      ret = GetAcquisitionProgress(&acc, &series);
      if (ret == DRV_SUCCESS)
      {
         if (series > seriesPrev)
         {
            // new frame arrived
            int retCode = camera_->PushImage();
            if (retCode != DEVICE_OK)
            {
               os << "PushImage failed with error code " << retCode;
               printf("%s\n", os.str().c_str());
               os.str("");
               //camera_->StopSequenceAcquisition();
               //return ret;
            }

            // report time elapsed since previous frame
            //printf("Frame %d captured at %ld ms!\n", ++frameCounter, GetTickCount() - timePrev);
            seriesPrev = series;
            frmcnt++;
            timePrev = GetTickCount();
         }
         Sleep(waitTime_);
      }
   }
   //while (ret == DRV_SUCCESS && series < numImages_ && !stop_);
   while (ret == DRV_SUCCESS && frmcnt < numImages_ && !stop_);

   if (ret != DRV_SUCCESS && series != 0)
   {
      camera_->StopCameraAcquisition();
      os << "Error: " << ret;
      printf("%s\n", os.str().c_str());
      os.str("");
      return ret;
   }

   if (stop_)
   {
      printf ("Acquisition interrupted by the user!\n");
      return 0;
   }
  
   if ((series-seriesInit) == numImages_)
   {
      printf("Did not get the intended number of images\n");
      camera_->StopCameraAcquisition();
      return 0;
   }
   
   os << "series: " << series << " seriesInit: " << seriesInit << " numImages: "<< numImages_;
   printf("%s\n", os.str().c_str());
   camera_->StopCameraAcquisition();
   return 3; // we can get here if we are not fast enough.  Report?  
}

/**
 * Starts continuous acquisition.
 */
int AndorCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
   if (acquiring_)
      return ERR_BUSY_ACQUIRING;

   stopOnOverflow_ = stopOnOverflow;
   sequenceLength_ = numImages;
   intervalMs_ = interval_ms;

   if(IsAcquiring())
   {
     SetToIdle();
   }
   LogMessage("Setting Trigger Mode", true);
   int ret0 = SetTriggerMode(0);  //set software trigger. mode 0:internal, 1: ext, 6:ext start, 7:bulb, 10:software
   if (ret0 != DRV_SUCCESS)
      return ret0;

   LogMessage("Setting Frame Transfer mode on", true);
   if(bFrameTransfer_ && (iCurrentTriggerMode == SOFTWARE))
     ret0 = SetFrameTransferMode(1);  //FT mode might be turned off in SnapImage when Software trigger mode is used. Resume it here

   ostringstream os;
   os << "Started sequence acquisition: " << numImages << "images  at " << interval_ms << " ms" << endl;
   LogMessage(os.str().c_str());

   // prepare the camera
   int ret = SetAcquisitionMode(5); // run till abort
   if (ret != DRV_SUCCESS)
      return ret;
   LogMessage("Set acquisition mode to 5", true);

   ret = SetReadMode(4); // image mode
   if (ret != DRV_SUCCESS)
      return ret;
   LogMessage("Set Read Mode to 4", true);

   // set AD-channel to 14-bit
//   ret = SetADChannel(0);
   if (ret != DRV_SUCCESS)
      return ret;

   if(iCurrentTriggerMode == SOFTWARE) // Change to turned on
   SetExposure_();
   SetExposureTime((float) (expMs_/1000.0));

   LogMessage ("Set Exposure time", true);
   ret = SetNumberAccumulations(1);
   if (ret != DRV_SUCCESS)
   {
      SetAcquisitionMode(1);
      return ret;
   }
   LogMessage("Set Number of accumulations to 1", true);

   ret = SetKineticCycleTime((float)(interval_ms / 1000.0));
   if (ret != DRV_SUCCESS)
   {
      SetAcquisitionMode(1);
      return ret;
   }
   LogMessage("Set Kinetic cycle time", true);

   long size;
   ret = GetSizeOfCircularBuffer(&size);
   if (ret != DRV_SUCCESS)
   {
      SetAcquisitionMode(1);
      return ret;
   }
   LogMessage("Get Size of circular Buffer", true);

   // re-apply the frame transfer mode setting
   char ftMode[MM::MaxStrLength];
   ret = GetProperty(g_FrameTransferProp, ftMode);
   assert(ret == DEVICE_OK);
   int modeIdx = 0;
   if (strcmp(g_FrameTransferOn, ftMode) == 0)
      modeIdx = 1;
   else if (strcmp(g_FrameTransferOff, ftMode) == 0)
      modeIdx = 0;
   else
      return DEVICE_INVALID_PROPERTY_VALUE;
   os.str("");
   os << "Set Frame transfer mode to " << modeIdx;
   LogMessage(os.str().c_str(), true);

   ret = SetFrameTransferMode(modeIdx);
   if (ret != DRV_SUCCESS)
   {
      SetAcquisitionMode(1);
      return ret;
   }

   // start thread
   imageCounter_ = 0;

   os.str("");
   os << "Setting thread length to " << numImages << " Images";
   LogMessage(os.str().c_str(), true);
   seqThread_->SetLength(numImages);

   float fExposure, fAccumTime, fKineticTime;
   GetAcquisitionTimings(&fExposure,&fAccumTime,&fKineticTime);
   SetProperty(MM::g_Keyword_ActualInterval_ms, CDeviceUtils::ConvertToString((double)fKineticTime * 1000.0)); 
   ActualInterval_ms_ = fKineticTime * 1000.0f;
   os.str("");
   os << "Exposure: " << fExposure << " AcummTime: " << fAccumTime << " KineticTime: " << fKineticTime;
   LogMessage(os.str().c_str());
   float ActualInterval_ms = fKineticTime * 1000.0f;
   seqThread_->SetWaitTime((long) (ActualInterval_ms / 5));

   // prepare the core
   ret = GetCoreCallback()->PrepareForAcq(this);
   if (ret != DEVICE_OK)
   {
      SetAcquisitionMode(1);
      return ret;
   }

   LogMessage("Starting acquisition in the camera", true);
   ret = ::StartAcquisition();
   seqThread_->Start();

   acquiring_ = true;

   if (ret != DRV_SUCCESS)
   {
      SetAcquisitionMode(1);
      return ret;
   }

   return DEVICE_OK;
}

/**
 * Stop Seq sequence acquisition
 * This is the function for internal use and can/should be called from the thread
 */
int AndorCamera::StopCameraAcquisition()
{
   if (!acquiring_)
      return DEVICE_OK;

   LogMessage("Stopped sequence acquisition");
   AbortAcquisition();
   acquiring_ = false;

   int ret;
   if(iCurrentTriggerMode == SOFTWARE)
   {
     ret = SetTriggerMode(10);  //set software trigger. mode 0:internal, 1: ext, 6:ext start, 7:bulb, 10:software
     //if (ret != DRV_SUCCESS) //not check to allow call of AcqFinished
     //  return ret;
     ret = SetAcquisitionMode(5);//set RTA non-iCam camera
     //if (ret != DRV_SUCCESS)
     //  return ret;
   }
   else if(iCurrentTriggerMode == EXTERNAL)
   {
     ret = SetTriggerMode(1);  //set software trigger. mode 0:internal, 1: ext, 6:ext start, 7:bulb, 10:software
     //if (ret != DRV_SUCCESS)
     //  return ret;
     ret = SetAcquisitionMode(1);//set SingleScan non-iCam camera
     //if (ret != DRV_SUCCESS)
     //  return ret;
   }
   else
   {
     ret = SetTriggerMode(0);  //set software trigger. mode 0:internal, 1: ext, 6:ext start, 7:bulb, 10:software
     //if (ret != DRV_SUCCESS)
     //  return ret;
     ret = SetAcquisitionMode(1);//set SingleScan non-iCam camera
     //if (ret != DRV_SUCCESS)
     //  return ret;
   }
   MM::Core* cb = GetCoreCallback();
   if (cb)
      return cb->AcqFinished(this, 0);
   else
      return DEVICE_OK;
}

/**
 * Stops Squence acquisition
 * This is for external use only (if called from the sequence acquisition thread, deadlock will ensue!
 */
int AndorCamera::StopSequenceAcquisition()
{
   StopCameraAcquisition();
   seqThread_->Stop();
   seqThread_->wait();
   
   return DEVICE_OK;
}

/**
 * Waits for new image and inserts it in the circular buffer.
 * This method is called by the acquisition thread AcqSequenceThread::svc()
 * in an infinite loop.
 *
 * In case of error or if the sequecne is finished StopSequenceAcquisition()
 * is called, which will raise the stop_ flag and cause the thread to exit.
 */
int AndorCamera::PushImage()
{
   // get the top most image from the driver
   unsigned ret = GetNewData16((WORD*)fullFrameBuffer_, roi_.xSize/binSize_ * roi_.ySize/binSize_);
   if (ret != DRV_SUCCESS)
      return (int)ret;

   // process image
   MM::ImageProcessor* ip = GetCoreCallback()->GetImageProcessor(this);
   if (ip)
   {
      int ret = ip->Process((unsigned char*) fullFrameBuffer_, GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());
      if (ret != DEVICE_OK)
         return ret;
   }

   // This method inserts new image in the circular buffer (residing in MMCore)
   int retCode = GetCoreCallback()->InsertImage(this, (unsigned char*) fullFrameBuffer_,
                                           GetImageWidth(),
                                           GetImageHeight(),
                                           GetImageBytesPerPixel());

   if (!stopOnOverflow_ && retCode == DEVICE_BUFFER_OVERFLOW)
   {
      // do not stop on overflow - just reset the buffer
      GetCoreCallback()->ClearImageBuffer(this);
      return GetCoreCallback()->InsertImage(this, (unsigned char*) fullFrameBuffer_,
                                           GetImageWidth(),
                                           GetImageHeight(),
                                           GetImageBytesPerPixel());
   } else
      return DEVICE_OK;
}
