///////////////////////////////////////////////////////////////////////////////
// FILE:          Universal.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   PVCAM universal camera module
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
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 06/30/2006
//
// CVS:           $Id$

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning(disable : 4996) // disable warning for deperecated CRT functions on Windows 
#endif

#include "../../MMDevice/ModuleInterface.h"
#include "PVCAM.h"
#include "PVCAMUtils.h"
#include "PVCAMProperty.h"


#ifdef WIN32
#include "../../../3rdparty/RoperScientific/Windows/PvCam/SDK/Headers/pvcam.h"
#include <stdint.h>
#else
#ifdef __APPLE__
#define __mac_os_x
#include <PVCAM/master.h>
#include <PVCAM/pvcam.h>
#endif
#endif

#include <string>
#include <sstream>
#include <iomanip>

using namespace std;
unsigned Universal::refCount_ = 0;
bool Universal::PVCAM_initialized_ = false;

const int BUFSIZE = 60;
#if WIN32
#define snprintf _snprintf
#endif

// global constants
extern const char* g_PixelType_8bit;
extern const char* g_PixelType_10bit;
extern const char* g_PixelType_12bit;
extern const char* g_PixelType_14bit;
extern const char* g_PixelType_16bit;

extern const char* g_ReadoutRate;
extern const char* g_ReadoutPort;
extern const char* g_ReadoutPort_Normal;
extern const char* g_ReadoutPort_Multiplier;
extern const char* g_ReadoutPort_LowNoise;
extern const char* g_ReadoutPort_HighCap;

SParam param_set[] = {
   //clear
   //{"ClearMode", PARAM_CLEAR_MODE},
   {"ClearCycles", PARAM_CLEAR_CYCLES},
   {"ContineousClears", PARAM_CONT_CLEARS},
   {"MinBlock", PARAM_MIN_BLOCK},
   {"NumBlock", PARAM_NUM_MIN_BLOCK},
   {"NumStripsPerClean", PARAM_NUM_OF_STRIPS_PER_CLR},
   // readout
   {"PMode", PARAM_PMODE},
   //{"ADCOffset", PARAM_ADC_OFFSET},
   {"FTCapable", PARAM_FRAME_CAPABLE},
   {"FullWellCapacity", PARAM_FWELL_CAPACITY},
   //{"FTDummies", PARAM_FTSCAN},
   {"ClearMode", PARAM_CLEAR_MODE},
   {"PreampDelay", PARAM_PREAMP_DELAY},
   {"PreampOffLimit", PARAM_PREAMP_OFF_CONTROL}, // preamp is off during exposure if exposure time is less than this
   {"MaskLines", PARAM_PREMASK},
   {"PrescanPixels", PARAM_PRESCAN},
   {"PostscanPixels", PARAM_POSTSCAN},
   {"X-dimension", PARAM_SER_SIZE},
   {"Y-dimension", PARAM_PAR_SIZE},
   {"ShutterMode",PARAM_SHTR_OPEN_MODE},
   {"ExposureMode", PARAM_EXPOSURE_MODE},
   {"LogicOutput", PARAM_LOGIC_OUTPUT}
};
int n_param = sizeof(param_set)/sizeof(SParam);

std::string IntToString(int i)throw()
{
   std::string s;
   try{ ostringstream os; os<<i; s=os.str();}catch(...){};
   return s;
};

#define LOG_MESSAGE(strMsg) \
{\
   strMsg += std::string(__FILE__) + std::string(" Line ") + IntToString(__LINE__);\
   LogMessage(strMsg);\
}

#define LOCATION \
   std::string(gErrorMsg) + std::string(__FILE__)\
   +std::string(" Line ") + IntToString(__LINE__)

#define LOG_CAM_ERROR \
{\
   std::string strMsg("");\
   strMsg += std::string(__FILE__) + std::string(" Line ") + IntToString(__LINE__);\
   LogCamError(" ", strMsg);\
}

#define LOG_IF_CAM_ERROR(bResult)\
   if(!bResult){LOG_CAM_ERROR}

#define RETURN_IF_CAM_ERROR(bResult) \
   if(!bResult){LOG_CAM_ERROR; return;}

#define RETURN_CAM_ERROR_IF_CAM_ERROR(bResult) \
   if(!bResult){LOG_CAM_ERROR; return pl_error_code();}

#define RETURN_BOOL_IF_CAM_ERROR(bResult) \
   if(!bResult){LOG_CAM_ERROR; return bResult?true:false;}

#define RETURN_DEVICE_ERR_IF_CAM_ERROR(bResult) \
   if(!bResult){LOG_CAM_ERROR; return DEVICE_ERR;}

#define LOG_MM_ERROR(nRet)\
{ \
   std::string strMsg("");\
   strMsg += std::string(__FILE__) + std::string(" Line ") + IntToString(__LINE__);\
   LogMMError(nRet, " ", strMsg);\
} 
#define LOG_IF_MM_ERROR(nRet)\
   if(DEVICE_OK != nRet){LOG_MM_ERROR(nRet)}

#define RETURN_IF_MM_ERROR(nRet)\
   if(DEVICE_OK != nRet){LOG_MM_ERROR(nRet); return nRet;}


///////////////////////////////////////////////////////////////////////////////
// &Universal constructor/destructor
Universal::Universal(short cameraId) :
CCameraBase<Universal> (),
initialized_(false),
busy_(false),
hPVCAM_(0),
exposure_(0),
binSize_(1),
bufferOK_(false),
cameraId_(cameraId),
name_("Undefined"),
nrPorts_ (1),
circBuffer_(0),
init_seqStarted_(false),
stopOnOverflow_(true),
noSupportForStreaming_(true),
restart_(false),
snappingSingleFrame_(false),
singleFrameModeReady_(false)

{
   // ACE::init();
   InitializeDefaultErrorMessages();

   // add custom messages
   SetErrorText(ERR_CAMERA_NOT_FOUND, "No Camera Found. Is it connected and switched on?");
   SetErrorText(ERR_BUSY_ACQUIRING, "Acquisition already in progress.");
}


Universal::~Universal()
{   
   refCount_--;
   if (refCount_ == 0)
   {
      // release resources
      if (initialized_)
         Shutdown();
      delete[] circBuffer_;
   }
}

int Universal::GetBinning () const 
{
   return binSize_;
}

int Universal::SetBinning (int binSize) 
{
   ostringstream os;
   os << binSize;
   return SetProperty(MM::g_Keyword_Binning, os.str().c_str());
}


///////////////////////////////////////////////////////////////////////////////
// Action handlers
// ~~~~~~~~~~~~~~~

// Binning
int Universal::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      bool capturing = IsCapturing();
      if (capturing)
         suspend();

      long bin;
      pProp->Get(bin);
      binSize_ = bin;
      ClearROI(); // reset region of interest
      int nRet;
      if (capturing)      
         nRet = resume(); 
      else
         nRet = ResizeImageBufferContinuous();

      if (nRet != DEVICE_OK)
         return nRet;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)binSize_);
   }
   return DEVICE_OK;
}

// Chip Name
int Universal::OnChipName(MM::PropertyBase* pProp, MM::ActionType eAct)
{

   // Read-Only
   if (eAct == MM::BeforeGet)
   {
      char chipName[CCD_NAME_LEN];
      RETURN_DEVICE_ERR_IF_CAM_ERROR(pl_get_param_safe(hPVCAM_, PARAM_CHIP_NAME, ATTR_CURRENT, chipName));

      pProp->Set(chipName);
      chipName_  = chipName;
   }


   return DEVICE_OK;

}

int Universal::OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   // exposure property is stored in milliseconds,
   // while the driver returns the value in seconds
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(exposure_);
   }
   else if (eAct == MM::AfterSet)
   {

      suspend();
      double exp;
      pProp->Get(exp);
      exposure_ = exp;
      resume();
      return DEVICE_OK;

   }
   return DEVICE_OK;
}

int Universal::OnPixelType(MM::PropertyBase* pProp, MM::ActionType /*eAct*/)
{  


   int16 bitDepth;
   RETURN_DEVICE_ERR_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_BIT_DEPTH, ATTR_CURRENT, &bitDepth)
      );
   switch (bitDepth) {
      case (8) : pProp->Set(g_PixelType_8bit); return DEVICE_OK;
      case (10) : pProp->Set(g_PixelType_10bit); return DEVICE_OK;
      case (12) : pProp->Set(g_PixelType_12bit); return DEVICE_OK;
      case (14) : pProp->Set(g_PixelType_14bit); return DEVICE_OK;
      case (16) : pProp->Set(g_PixelType_16bit); return DEVICE_OK;
   }


   return DEVICE_OK;
}

// Camera Speed
int Universal::OnReadoutRate(MM::PropertyBase* pProp, MM::ActionType eAct)
{


   if (eAct == MM::AfterSet)
   {
      long gain;
      GetLongParam_PvCam_safe(hPVCAM_, PARAM_GAIN_INDEX, &gain);

      string par;
      pProp->Get(par);
      long index;
      std::map<std::string, int>::iterator iter = rateMap_.find(par);
      if (iter != rateMap_.end())
         index = iter->second;
      else
         return ERR_INVALID_PARAMETER_VALUE;

      if (!SetLongParam_PvCam_safe(hPVCAM_, PARAM_SPDTAB_INDEX, index))
         return pl_error_code();

      // Try setting the gain to original value, don't make a fuss when it fails
      SetLongParam_PvCam_safe(hPVCAM_, PARAM_GAIN_INDEX, gain);
      if (!SetGainLimits())
         return pl_error_code();
      if (!SetAllowedPixelTypes())
         return pl_error_code();
      // update GUI to reflect these changes
      int ret = OnPropertiesChanged();
      if (ret != DEVICE_OK)
         return ret;
   }
   else if (eAct == MM::BeforeGet)
   {
      long index;
      if (!GetLongParam_PvCam_safe(hPVCAM_, PARAM_SPDTAB_INDEX, &index))
         return pl_error_code();
      string mode;
      int nRet = GetSpeedString(mode);
      if (nRet != DEVICE_OK)
         return nRet;
      pProp->Set(mode.c_str());
   }


   return DEVICE_OK;
}

// Readout Port
int Universal::OnReadoutPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{


   if (eAct == MM::AfterSet)
   {
      string par;
      pProp->Get(par);
      int port;
      std::map<std::string, int>::iterator iter = portMap_.find(par);
      if (iter != portMap_.end())
         port = iter->second;
      else
         return ERR_INVALID_PARAMETER_VALUE;

      ostringstream tmp;
      tmp << "Set port to: " << par << " ID: " << port;
      LogMessage(tmp.str().c_str(), true);

      if (!SetLongParam_PvCam_safe(hPVCAM_, PARAM_READOUT_PORT, port))
         return pl_error_code();

      // Update elements that might have changes because of port change
      int ret = GetSpeedTable();
      if (ret != DEVICE_OK)
         return ret;
      if (!SetGainLimits())
         return pl_error_code();
      if (!SetAllowedPixelTypes())
         return pl_error_code();

      // update GUI to reflect these changes
      ret = OnPropertiesChanged();
      if (ret != DEVICE_OK)
         return ret;
   }
   else if (eAct == MM::BeforeGet)
   {
      long port;
      if (!GetLongParam_PvCam_safe(hPVCAM_, PARAM_READOUT_PORT, &port))
         return pl_error_code();
      std::string portName = GetPortName(port);

      ostringstream tmp;
      tmp << "Get port  " << portName << " ID: " << port;
      LogMessage(tmp.str().c_str(), true);

      pProp->Set(portName.c_str());
   }


   return DEVICE_OK;
}

// Gain
int Universal::OnGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{

   if (eAct == MM::AfterSet)
   {
      long gain;
      pProp->Get(gain);
      if (!SetLongParam_PvCam_safe(hPVCAM_, PARAM_GAIN_INDEX, gain))
         return pl_error_code();
   }
   else if (eAct == MM::BeforeGet)
   {
      long gain;
      if (!GetLongParam_PvCam_safe(hPVCAM_, PARAM_GAIN_INDEX, &gain))
         return pl_error_code();
      pProp->Set(gain);
   }


   return DEVICE_OK;


}


void Universal::suspend()
{

   //CaptureRestartHelper restart(this);
   if(IsCapturing())
   {
      restart_ = true;
      //thd_->Suspend();
      //while (! thd_->IsSuspended());
      thd_->Stop();
      thd_->wait();
      LOG_IF_CAM_ERROR(pl_exp_stop_cont(hPVCAM_, CCS_HALT)); //Circular buffer only
      LOG_IF_CAM_ERROR(pl_exp_finish_seq(hPVCAM_, circBuffer_, 0));
   } 
}

int Universal::resume()
{
   CaptureRestartHelper cameraAlreadyRunning(this);
   if(restart_) 
   {
      int ret = ResizeImageBufferContinuous();
      RETURN_IF_MM_ERROR(ret);

      RETURN_DEVICE_ERR_IF_CAM_ERROR(
         pl_exp_start_cont(hPVCAM_, circBuffer_, bufferSize_)) //Circular buffer only

      long imageCount = thd_->GetImageCounter();
      double intervalMs = thd_->GetIntervalMs();
      //thd_->Resume();
      thd_->Start(numImages_ - imageCount, intervalMs);
      restart_ = false;
      
   }

   return DEVICE_OK;
}


// EM Gain
int Universal::OnMultiplierGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{


   if (eAct == MM::AfterSet)
   {
      long gain;
      pProp->Get(gain);


      if (!SetLongParam_PvCam_safe(hPVCAM_, PARAM_GAIN_MULT_FACTOR, gain))
         return pl_error_code();

   }
   else if (eAct == MM::BeforeGet)
   {
      long gain;


      if (!GetLongParam_PvCam_safe(hPVCAM_, PARAM_GAIN_MULT_FACTOR, &gain))
         return pl_error_code();


      pProp->Set(gain);
   }


   return DEVICE_OK;

}

// Offset
int Universal::OnOffset(MM::PropertyBase* /*pProp*/, MM::ActionType /*eAct*/)
{
   return DEVICE_OK;
}

// Temperature
int Universal::OnTemperature(MM::PropertyBase* pProp, MM::ActionType eAct)
{


   if (eAct == MM::AfterSet)
   {
   }
   else if (eAct == MM::BeforeGet)
   {
      long temp;

      if (!GetLongParam_PvCam_safe(hPVCAM_, PARAM_TEMP, &temp))
         return pl_error_code();

      pProp->Set(temp/100);
   }

   return DEVICE_OK;
}

int Universal::OnTemperatureSetPoint(MM::PropertyBase* pProp, MM::ActionType eAct)
{


   if (eAct == MM::AfterSet)
   {
      long temp;
      pProp->Get(temp);
      temp = temp * 100;

      if (!SetLongParam_PvCam_safe(hPVCAM_, PARAM_TEMP_SETPOINT, temp))
         return pl_error_code();

   }
   else if (eAct == MM::BeforeGet)
   {
      long temp;

      if (!GetLongParam_PvCam_safe(hPVCAM_, PARAM_TEMP_SETPOINT, &temp))
         return pl_error_code();

      pProp->Set(temp/100);
   }


   return DEVICE_OK;
}

int Universal::OnUniversalProperty(MM::PropertyBase* pProp, MM::ActionType eAct, long index)
{


   if (eAct == MM::AfterSet)
   {
      uns16 dataType;
      if (!pl_get_param_safe(hPVCAM_, param_set[index].id, ATTR_TYPE, &dataType) || (dataType != TYPE_ENUM)) 
      {
         long ldata;
         pProp->Get(ldata);

         RETURN_CAM_ERROR_IF_CAM_ERROR(
            SetLongParam_PvCam_safe(hPVCAM_, (long)param_set[index].id, (uns32) ldata));

      } else {
         std::string mnemonic;
         pProp->Get(mnemonic);
         uns32 ldata = param_set[index].enumMap[mnemonic];

         RETURN_CAM_ERROR_IF_CAM_ERROR(
            SetLongParam_PvCam_safe(hPVCAM_, (long)param_set[index].id, (uns32) ldata));
      }
   }
   else if (eAct == MM::BeforeGet)
   {
      long ldata;

      if (!GetLongParam_PvCam_safe(hPVCAM_, param_set[index].id, &ldata))
         return pl_error_code();

      uns16 dataType;
      if (!pl_get_param_safe(hPVCAM_, param_set[index].id, ATTR_TYPE, &dataType) || (dataType != TYPE_ENUM)) 
      {
         pProp->Set(ldata);
      } else {
         char enumStr[100];
         int32 enumValue;
         // It is absurd, but we seem to need this param_set[index].indexMap[ldata] instead of straight ldata??!!!
         if (pl_get_enum_param_safe(hPVCAM_, param_set[index].id, param_set[index].indexMap[ldata], &enumValue, enumStr, 100)) 
         {
            pProp->Set(enumStr);
         } else {
            LOG_CAM_ERROR;
            LogMessage ("Error in pl_get_enum_param_safe\n");
            return pl_error_code();
         }
      }
   }


   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// API methods
// ~~~~~~~~~~~

void Universal::GetName(char* name) const 
{
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

///////////////////////////////////////////////////////////////////////////////
// Function name   : &Universal::Initialize
// Description     : Initialize the camera
// Return type     : bool 

int Universal::Initialize()
{
   // setup the camera
   // ----------------

   // Description
   int nRet = CreateProperty(MM::g_Keyword_Description, "PVCAM API device adapter", MM::String, true);
   assert(nRet == DEVICE_OK);

   // gather information about the equipment
   // ------------------------------------------

   // Camera name
   char name[CAM_NAME_LEN] = "Undef";

   if (!PVCAM_initialized_)
   {
      if (!pl_pvcam_init())
      {
         LOG_CAM_ERROR;
         // Try once more:
         pl_pvcam_uninit();
         RETURN_CAM_ERROR_IF_CAM_ERROR(pl_pvcam_init());
      }
      PVCAM_initialized_ = true;
   }

   // Get PVCAM version
   uns16 version;
   if (!pl_pvcam_get_ver(&version))
      return pl_error_code();
   uns16 major, minor, trivial;
   major = version; major = major >> 8; major = major & 0xFF;
   minor = version; minor = minor >> 4; minor = minor & 0xF;
   trivial = version; trivial = trivial & 0xF;
   stringstream ver;
   ver << major << "." << minor << "." << trivial;
   nRet = CreateProperty("PVCAM Version", ver.str().c_str(),  MM::String, true);
   assert(nRet == DEVICE_OK);

   // find camera
   if (!pl_cam_get_name(cameraId_, name))
   {
      LOG_CAM_ERROR;
      return ERR_CAMERA_NOT_FOUND;
   }
   //return pl_error_code();

   // Get a handle to the camera
   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_cam_open(name, &hPVCAM_, OPEN_EXCLUSIVE ))

   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_cam_get_diags(hPVCAM_));

   refCount_++;

   name_ = name;   // Name
   nRet = CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);
   assert(nRet == DEVICE_OK);


   // Chip Name
   CPropertyAction *pAct = new CPropertyAction (this, &Universal::OnChipName);
   nRet = CreateProperty("ChipName", "", MM::String, true, pAct);

   // Bit depth
      RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_get_param_safe(hPVCAM_, PARAM_BIT_DEPTH, ATTR_CURRENT, &bitDepth_));

   // setup image parameters
   // ----------------------

   // exposure
   pAct = new CPropertyAction (this, &Universal::OnExposure);
   nRet = CreateProperty(MM::g_Keyword_Exposure, "10.0", MM::Float, false, pAct);
   assert(nRet == DEVICE_OK);

   // binning
   pAct = new CPropertyAction (this, &Universal::OnBinning);
   pAct = new CPropertyAction (this, &Universal::OnBinning);
   nRet = CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false, pAct);
   assert(nRet == DEVICE_OK);
   vector<string> binValues;
   binValues.push_back("1");
   binValues.push_back("2");
   binValues.push_back("4");
   nRet = SetAllowedValues(MM::g_Keyword_Binning, binValues);
   if (nRet != DEVICE_OK)
      return nRet;

   // Pixel type.  
   // Note that this can change depending on the readoutport and speed.  SettAllowedPixeltypes should be called after changes in any of these
   pAct = new CPropertyAction (this, &Universal::OnPixelType);
   nRet = CreateProperty(MM::g_Keyword_PixelType, g_PixelType_16bit, MM::String, false, pAct);
   assert(nRet == DEVICE_OK);
   SetAllowedPixelTypes();


   // Gain
   // Check the allowed gain settings.  Note that this can change depending on output port, and readout rate. SetGainLimits() should be called after changing those parameters. 
   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_GAIN_INDEX, ATTR_AVAIL, &gainAvailable_));
   if (gainAvailable_) {
      pAct = new CPropertyAction (this, &Universal::OnGain);
      nRet = CreateProperty(MM::g_Keyword_Gain, "1", MM::Integer, false, pAct);
      if (nRet != DEVICE_OK)
         return nRet;
      nRet = SetGainLimits();
      if (nRet != DEVICE_OK)
         return nRet;
   }

   // ReadoutPorts
   rs_bool readoutPortAvailable;
   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_READOUT_PORT, ATTR_AVAIL, &readoutPortAvailable));
   if (readoutPortAvailable) {
      uns32 minPort, maxPort;
      //should it return?
      LOG_IF_CAM_ERROR(
         pl_get_param_safe(hPVCAM_, PARAM_READOUT_PORT, ATTR_COUNT, &nrPorts_));
      LOG_IF_CAM_ERROR(
         pl_get_param_safe(hPVCAM_, PARAM_READOUT_PORT, ATTR_MIN, &minPort));
      LOG_IF_CAM_ERROR(
         pl_get_param_safe(hPVCAM_, PARAM_READOUT_PORT, ATTR_MAX, &maxPort));
      long currentPort;

      if (!GetLongParam_PvCam_safe(hPVCAM_, PARAM_READOUT_PORT, &currentPort))
         return pl_error_code();

      ostringstream tmp;
      tmp << "Readout Ports, Nr: " << nrPorts_ << " min: " << minPort << " max: " << maxPort << "Current: " << currentPort;
      LogMessage(tmp.str().c_str(), true);

      pAct = new CPropertyAction (this, &Universal::OnReadoutPort);
      if (nrPorts_ <= 1)
         nRet = CreateProperty(g_ReadoutPort, g_ReadoutPort_Normal, MM::String, true, pAct);
      else
         nRet = CreateProperty(g_ReadoutPort, g_ReadoutPort_Normal, MM::String, false, pAct);
      assert(nRet == DEVICE_OK);
      // Found out what ports we have
      vector<string> portValues;
      for (uns32 i=minPort; i<nrPorts_; i++) {
         portValues.push_back(GetPortName(i));
         portMap_[GetPortName(i)] = i;
      }
      nRet = SetAllowedValues(g_ReadoutPort, portValues);
      if (nRet != DEVICE_OK)
         return nRet;
   }

   // Multiplier Gain
   rs_bool emGainAvailable;
   // The HQ2 has 'visual gain', which shows up as EM Gain.  
   // Detect whether this is an interline chip and do not expose EM Gain if it is.
   char chipName[CCD_NAME_LEN];
   if (!pl_get_param_safe(hPVCAM_, PARAM_CHIP_NAME, ATTR_CURRENT, chipName))
      return pl_error_code();
   pl_get_param_safe( hPVCAM_, PARAM_GAIN_MULT_FACTOR, ATTR_AVAIL, &emGainAvailable);
   LogMessage(chipName);
   if (emGainAvailable && (strstr(chipName, "ICX-285") == 0) && (strstr(chipName, "ICX285") == 0) ) {
      LogMessage("This Camera has Em Gain");
      pAct = new CPropertyAction (this, &Universal::OnMultiplierGain);
      nRet = CreateProperty("MultiplierGain", "1", MM::Integer, false, pAct);
      assert(nRet == DEVICE_OK);
      int16 emGainMax;
      // Apparently, the minimum EM gain is always 1
      pl_get_param_safe( hPVCAM_, PARAM_GAIN_MULT_FACTOR, ATTR_MAX, &emGainMax);
      ostringstream s;
      s << "EMGain " << " " << emGainMax;
      LogMessage(s.str().c_str(), true);
      nRet = SetPropertyLimits("MultiplierGain", 1, emGainMax);
      if (nRet != DEVICE_OK)
         return nRet;
   } else
      LogMessage("This Camera does not have EM Gain");

   // Speed Table
   // Deduce available modes from the speed table and make all options available
   // The Speed table will change depending on the Readout Port
   pAct = new CPropertyAction (this, &Universal::OnReadoutRate);
   nRet = CreateProperty(g_ReadoutRate, "0", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   nRet = GetSpeedTable();
   if (nRet != DEVICE_OK)
      return nRet;

   // camera temperature
   pAct = new CPropertyAction (this, &Universal::OnTemperature);
   nRet = CreateProperty(MM::g_Keyword_CCDTemperature, "25", MM::Integer, true, pAct);
   assert(nRet == DEVICE_OK);

   pAct = new CPropertyAction (this, &Universal::OnTemperatureSetPoint);
   nRet = CreateProperty(MM::g_Keyword_CCDTemperatureSetPoint, "-50", MM::Integer, false, pAct);
   assert(nRet == DEVICE_OK);

   // other ('Universal') parameters
   for (int i = 0 ; i < n_param; i++) 
   {
      long ldata;
      char buf[BUFSIZE];
      uns16 AccessType;
      rs_bool bAvail;
      bool versionTest = true;

      // Version cutoff is semi-arbitrary.  PI cameras with PVCAM 0x0271 need this
      if (strcmp(param_set[i].name, "PMode") == 0  && version < 0x0275)
         versionTest= false;


      bool getLongSuccess = GetLongParam_PvCam_safe(hPVCAM_, param_set[i].id, &ldata);


      //should it return?
      LOG_IF_CAM_ERROR(
         pl_get_param_safe( hPVCAM_, param_set[i].id, ATTR_ACCESS, &AccessType));
      LOG_IF_CAM_ERROR(
         pl_get_param_safe( hPVCAM_, param_set[i].id, ATTR_AVAIL, &bAvail));
      if ( (AccessType != ACC_ERROR) && bAvail && getLongSuccess && versionTest) 
      {
         snprintf(buf, BUFSIZE, "%ld", ldata);
         CPropertyActionEx *pAct = new CPropertyActionEx(this, &Universal::OnUniversalProperty, (long)i);
         uint16_t dataType;
         rs_bool plResult = pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_TYPE, &dataType);
         if (!plResult || (dataType != TYPE_ENUM)) {
            LOG_CAM_ERROR;
            nRet = CreateProperty(param_set[i].name, buf, MM::Integer, AccessType == ACC_READ_ONLY, pAct);
            RETURN_IF_MM_ERROR(nRet);

            // get allowed values for non-enum types 
            if (plResult) {
               nRet = SetUniversalAllowedValues(i, dataType);
               RETURN_IF_MM_ERROR(nRet);
            }
            else {
               ostringstream os;
               os << "problems getting type info from parameter: " << param_set[i].name << " " << AccessType;
               LogMessage(os.str().c_str());
            }

         } else  // enum type, get the associated strings, store in a map and make accesible to the user interface
         {
            nRet = CreateProperty(param_set[i].name, buf, MM::String, AccessType == ACC_READ_ONLY, pAct);
            uns32 count, index;
            int32 enumValue;
            char enumStr[100];
            if (pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_COUNT, (void *) &count)) {
               for (index = 0; index < count; index++) {
                  if (pl_get_enum_param_safe(hPVCAM_, param_set[i].id, index, &enumValue, enumStr, 100)) {
                     AddAllowedValue(param_set[i].name, enumStr);
                     std::string tmp = enumStr;
                     param_set[i].indexMap[enumValue] = index;
                     param_set[i].enumMap[tmp] = enumValue;
                  }
                  else
                     LogMessage ("Error in pl_get_enum_param_safe");
               }
            }
         }

         assert(nRet == DEVICE_OK);
      }
   }

   // actual interval in streaming mode
   nRet = CreateProperty(MM::g_Keyword_ActualInterval_ms, "0.0", MM::Float, false);
   assert(nRet == DEVICE_OK);

   // synchronize all properties
   // --------------------------
   nRet = UpdateStatus();
   RETURN_IF_MM_ERROR(nRet);

   // setup imaging
   RETURN_CAM_ERROR_IF_CAM_ERROR(pl_exp_init_seq());

   // check for circular buffer support
   rs_bool availFlag;
   noSupportForStreaming_ = 
      (!pl_get_param_safe(hPVCAM_, PARAM_CIRC_BUFFER, ATTR_AVAIL, &availFlag) || !availFlag);

   // setup the buffer
   // ----------------
   nRet = ResizeImageBufferContinuous();
   RETURN_IF_MM_ERROR(nRet);

   initialized_ = true;
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Function name   : &Universal::Shutdown
// Description     : Deactivate the camera, reverse the initialization process
// Return type     : bool 


int Universal::Shutdown()
{
   if (initialized_)
   {
      rs_bool ret = pl_exp_uninit_seq();
      LOG_IF_CAM_ERROR(ret);
      assert(ret);
      ret = pl_cam_close(hPVCAM_);
      LOG_IF_CAM_ERROR(ret);
      assert(ret);	  
      refCount_--;      
      if (PVCAM_initialized_ && refCount_ == 0)      
      {         
         LOG_IF_CAM_ERROR(pl_pvcam_uninit());
         PVCAM_initialized_ = false;
      }      
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool Universal::Busy()
{
   return false;//acquiring_;
}


///////////////////////////////////////////////////////////////////////////////
// Function name   : &Universal::SnapImage
// Description     : Acquires a single frame and stores it in the internal
//                   buffer.
//                   This command blocks the calling thread
//                   until the image is fully captured.
// Return type     : int 
int Universal::SnapImage()
{
   int16 status;
   uns32 not_needed;
   rs_bool ret;
   int nRet = DEVICE_ERR;

   if(snappingSingleFrame_)
   {
      LOG_MESSAGE(std::string("Warning: Entering SnapImage while GetImage has not been done for previous frame"));
   };

   if (!bufferOK_)
      return ERR_INVALID_BUFFER;

   if(!singleFrameModeReady_)
   {

      LOG_IF_CAM_ERROR(pl_exp_stop_cont(hPVCAM_, CCS_HALT));
      LOG_IF_CAM_ERROR(pl_exp_finish_seq(hPVCAM_, circBuffer_, 0));

      nRet = ResizeImageBufferSingle();
      RETURN_IF_MM_ERROR(nRet);
   }

   void* pixBuffer = const_cast<unsigned char*> (img_.GetPixels());
   RETURN_DEVICE_ERR_IF_CAM_ERROR(pl_exp_start_seq(hPVCAM_, pixBuffer));

   //ToDo: multithreading
   snappingSingleFrame_=true;


   // For MicroMax cameras, wait untill exposure is finished, for others, use the pvcam check status function
   // Check for MicroMax camera in the chipname, this might not be the best method
   if (chipName_.substr(0,3).compare("PID") == 0)
   {
      MM::MMTime startTime = GetCurrentMMTime();
      do {
         CDeviceUtils::SleepMs(2);
      } while ( (GetCurrentMMTime() - startTime) < ( (exposure_ + 50) * 1000.0) );
      ostringstream db;
      db << chipName_.substr(0,3) << " exposure: " << exposure_;
      LogMessage (db.str().c_str());
   } else { // All modern cameras
      // block until exposure is finished
      do {
         ret = pl_exp_check_status(hPVCAM_, &status, &not_needed);
         LOG_IF_CAM_ERROR(ret);
      } while (ret && (status == EXPOSURE_IN_PROGRESS));
      if (!ret)
      {
         snappingSingleFrame_=true;
         return pl_error_code();   
      }
   }

   return DEVICE_OK;
}




const unsigned char* Universal::GetImageBuffer()
{  
   int16 status;
   uns32 not_needed;

   if(!snappingSingleFrame_)
   {
      LOG_MESSAGE(std::string("Warning: GetImageBuffer called before SnapImage()"));
   }

   // wait for data or error
   void* pixBuffer = const_cast<unsigned char*> (img_.GetPixels());

   while(pl_exp_check_status(hPVCAM_, &status, &not_needed) && 
      (status != READOUT_COMPLETE && status != READOUT_FAILED) );

   snappingSingleFrame_=false;

   // Check Error Codes
   if(status == READOUT_FAILED)
   {
      LOG_MESSAGE(std::string("PVCAM: status == READOUT_FAILED"));
      // return pl_error_code();
      return 0;
   }

   if (!pl_exp_finish_seq(hPVCAM_, pixBuffer, 0))
   {
      LOG_CAM_ERROR;
      // return pl_error_code();
      return 0;
   }

   return (unsigned char*) pixBuffer;
}


double Universal::GetExposure() const
{
   char buf[MM::MaxStrLength];
   buf[0] = '\0';
   LOG_IF_MM_ERROR(
      GetProperty(MM::g_Keyword_Exposure, buf));
   return atof(buf);
}

void Universal::SetExposure(double exp)
{
   LOG_IF_MM_ERROR(
      SetProperty(MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(exp)));
}

/**
* Returns the raw image buffer.
*/

unsigned Universal::GetBitDepth() const
{

   return (unsigned) bitDepth_;
}


int Universal::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
   if (IsCapturing())
      return ERR_BUSY_ACQUIRING;

   // ROI internal dimensions are in no-binning mode, so we need to convert

   roi_.x = (uns16) (x * binSize_);
   roi_.y = (uns16) (y * binSize_);
   roi_.xSize = (uns16) (xSize * binSize_);
   roi_.ySize = (uns16) (ySize * binSize_);

   return ResizeImageBufferContinuous();
}

int Universal::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{
   // ROI internal dimensions are in no-binning mode, so we need to convert

   x = roi_.x / binSize_;
   y = roi_.y / binSize_;
   xSize = roi_.xSize / binSize_;
   ySize = roi_.ySize / binSize_;

   return DEVICE_OK;
}

int Universal::ClearROI()
{
   if (IsCapturing())
      return ERR_BUSY_ACQUIRING;

   roi_.x = 0;
   roi_.y = 0;
   roi_.xSize = 0;
   roi_.ySize = 0;

   return ResizeImageBufferContinuous();
}

bool Universal::GetErrorText(int errorCode, char* text) const
{
   if (CCameraBase<Universal>::GetErrorText(errorCode, text))
      return true; // base message

   char buf[ERROR_MSG_LEN];
   if (pl_error_message ((int16)errorCode, buf))
   {
      CDeviceUtils::CopyLimitedString(text, buf);
      return true;
   }
   else
      return false;
}

///////////////////////////////////////////////////////////////////////////////
// Utility methods
///////////////////////////////////////////////////////////////////////////////

int Universal::SetAllowedPixelTypes() 
{
   int16 bitDepth, minBitDepth, maxBitDepth, bitDepthIncrement;
   LOG_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_BIT_DEPTH, ATTR_CURRENT, &bitDepth));
   LOG_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_BIT_DEPTH, ATTR_INCREMENT, &bitDepthIncrement));
   LOG_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_BIT_DEPTH, ATTR_MAX, &maxBitDepth));
   LOG_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_BIT_DEPTH, ATTR_MIN, &minBitDepth));

   ostringstream os;
   os << "Pixel Type: " << bitDepth << " " << bitDepthIncrement << " " << minBitDepth << " " << maxBitDepth;
   LogMessage(os.str().c_str(), true);

   vector<string> pixelTypeValues;
   // These PVCAM folks can give some weird answers now and then:
   if (maxBitDepth < minBitDepth)
      maxBitDepth = minBitDepth;
   if (bitDepthIncrement == 0)
      bitDepthIncrement = 2;
   for (int i = minBitDepth; i <= maxBitDepth; i += bitDepthIncrement) {
      switch (i) {
         case (8) : pixelTypeValues.push_back(g_PixelType_8bit);
            break;
         case (10) : pixelTypeValues.push_back(g_PixelType_10bit);
            break;
         case (12) : pixelTypeValues.push_back(g_PixelType_12bit);
            break;
         case (14) : pixelTypeValues.push_back(g_PixelType_14bit);
            break;
         case (16) : pixelTypeValues.push_back(g_PixelType_16bit);
            break;
      }
   }
   int nRet = SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
      LOG_IF_MM_ERROR(nRet);
   return nRet;
}

int Universal::SetGainLimits()
{
   int nRet = DEVICE_OK;
   if (!gainAvailable_)
      return DEVICE_OK;
   int16 gainMin, gainMax;
   LOG_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_GAIN_INDEX, ATTR_MIN, &gainMin));
   LOG_IF_CAM_ERROR(
      pl_get_param_safe( hPVCAM_, PARAM_GAIN_INDEX, ATTR_MAX, &gainMax));
   ostringstream s;
   s << "Gain " << " " << gainMin << " " << gainMax;
   LogMessage(s.str().c_str(), true);
   if ((gainMax - gainMin) < 10) {
      vector<std::string> values;
      for (int16 index = gainMin; index <= gainMax; index++) {
         ostringstream os;
         os << index;
         values.push_back(os.str());
      }
      nRet = SetAllowedValues(MM::g_Keyword_Gain, values);
   } else
      nRet = SetPropertyLimits(MM::g_Keyword_Gain, (int) gainMin, (int) gainMax);

   LOG_IF_MM_ERROR(nRet);

   return nRet;
}

int Universal::SetUniversalAllowedValues(int i, uns16 dataType)
{
   switch (dataType) { 
      case TYPE_INT8 : {
         int8 min, max, index;
         if (pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MIN, (void *) &min) && pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MAX, (void*) &max ) )  
         {
            if ((max - min) > 10) {
               SetPropertyLimits(param_set[i].name, (double) min, (double) max);
            } else {
               vector<std::string> values;
               for (index = min; index <= max; index++) {
                  ostringstream os;
                  os << index;
                  values.push_back(os.str());
               }
               SetAllowedValues(param_set[i].name, values);
            }
         }else
         {
            LOG_CAM_ERROR;
         }
         break;
                       }
      case TYPE_UNS8 : {
         uns8 min, max, index;
         if (pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MIN, (void *) &min) && pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MAX, (void*) &max ) )  {
            if ((max - min) > 10) {
               SetPropertyLimits(param_set[i].name, (double) min, (double) max);
            } else {
               vector<std::string> values;
               for (index = min; index <= max; index++) {
                  ostringstream os;
                  os << index;
                  values.push_back(os.str());
               }
               SetAllowedValues(param_set[i].name, values);
            }
         }else
         {
            LOG_CAM_ERROR;
         }
         break;
                       }
      case TYPE_INT16 : {
         int16 min, max, index;
         if (pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MIN, (void *) &min) && pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MAX, (void*) &max ) )  {
            if ((max - min) > 10) {
               SetPropertyLimits(param_set[i].name, (double) min, (double) max);
            } else if ((max-min) < 1000000) {
               vector<std::string> values;
               for (index = min; index <= max; index++) {
                  ostringstream os;
                  os << index;
                  values.push_back(os.str());
               }
               SetAllowedValues(param_set[i].name, values);
            }
         }else
         {
            LOG_CAM_ERROR;
         }
         break;
                        }
      case TYPE_UNS16 : {
         uns16 min, max, index;
         if (pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MIN, (void *) &min) && pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MAX, (void*) &max ) )  {
            if ((max - min) > 10) {
               SetPropertyLimits(param_set[i].name, (double) min, (double) max);
            } else if ((max-min) < 1000000) {
               vector<std::string> values;
               for (index = min; index <= max; index++) {
                  ostringstream os;
                  os << index;
                  values.push_back(os.str());
               }
               SetAllowedValues(param_set[i].name, values);
            }
         }else
         {
            LOG_CAM_ERROR;
         }
         break;
                        }
      case TYPE_INT32 : {
         int32 min, max, index;
         if (pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MIN, (void *) &min) && pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MAX, (void*) &max ) )  {
            if ((max - min) > 10) {
               SetPropertyLimits(param_set[i].name, (double) min, (double) max);
            } else if ((max-min) < 1000000) {
               vector<std::string> values;
               for (index = min; index <= max; index++) {
                  ostringstream os;
                  os << index;
                  values.push_back(os.str());
               }
               SetAllowedValues(param_set[i].name, values);
            }
         }else
         {
            LOG_CAM_ERROR;
         }
         break;
                        }
      case TYPE_UNS32 : {
         uns32 min, max, index;
         if (pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MIN, (void *) &min) && pl_get_param_safe(hPVCAM_, param_set[i].id, ATTR_MAX, (void*) &max ) )  {
            if ((max - min) > 10) {
               SetPropertyLimits(param_set[i].name, (double) min, (double) max);
            } else if ((max-min) < 1000000) {
               vector<std::string> values;
               for (index = min; index <= max; index++) {
                  ostringstream os;
                  os << index;
                  values.push_back(os.str());
               }
               SetAllowedValues(param_set[i].name, values);
            }
         }else
         {
            LOG_CAM_ERROR;
         }
         break;
                        }
   }
   return DEVICE_OK;
}

int Universal::GetSpeedString(std::string& modeString)
{
   stringstream tmp;

   // read speed table setting from camera:
   int16 spdTableIndex;
   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_get_param_safe(hPVCAM_, PARAM_SPDTAB_INDEX, ATTR_CURRENT, &spdTableIndex));

   // read pixel time:
   long pixelTime;

   RETURN_CAM_ERROR_IF_CAM_ERROR(
      GetLongParam_PvCam_safe(hPVCAM_, PARAM_PIX_TIME, &pixelTime));

   double rate = 1000/pixelTime; // in MHz

   // read bit depth
   long bitDepth;
   RETURN_CAM_ERROR_IF_CAM_ERROR(
      GetLongParam_PvCam_safe(hPVCAM_, PARAM_BIT_DEPTH, &bitDepth));

   tmp << rate << "MHz " << bitDepth << "bit";
   modeString = tmp.str();
   return DEVICE_OK;
}

/*
* Sets allowed values for entry "Speed"
* Also sets the Map rateMap_
*/
int Universal::GetSpeedTable()
{
   int nRet=DEVICE_ERR;
   int16 spdTableCount;
   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_get_param_safe(hPVCAM_, PARAM_SPDTAB_INDEX, ATTR_MAX, &spdTableCount));

   //Speed Table Index is 0 based.  We got the max number but want the total count
   spdTableCount +=1;
   ostringstream os;
   os << "SpeedTableCountd: " << spdTableCount << "\n";

   // log the current settings, so that we can revert to it after cycling through the options
   int16 spdTableIndex;
   LOG_IF_CAM_ERROR(
      pl_get_param_safe(hPVCAM_, PARAM_SPDTAB_INDEX, ATTR_CURRENT, &spdTableIndex));
   // cycle through the speed table entries and record the associated settings
   vector<string> speedValues;
   rateMap_.clear();
   string speedString;
   for (int16 i=0; i<spdTableCount; i++) 
   {
      stringstream tmp;
      LOG_IF_CAM_ERROR(
         pl_set_param_safe(hPVCAM_, PARAM_SPDTAB_INDEX, &i));
      nRet = GetSpeedString(speedString);
      RETURN_IF_MM_ERROR(nRet);

      os << "Speed: " << speedString.c_str() << "\n";
      speedValues.push_back(speedString);
      rateMap_[speedString] = i;
   }
   LogMessage (os.str().c_str(), true);
   // switch back to original setting
   LOG_IF_CAM_ERROR(
      pl_set_param_safe(hPVCAM_, PARAM_SPDTAB_INDEX, &spdTableIndex));

   RETURN_IF_MM_ERROR(
      SetAllowedValues(g_ReadoutRate, speedValues));

   return nRet;
}

std::string Universal::GetPortName(long portId)
{
   // Work around bug in PVCAM:
   if (nrPorts_ == 1) 
      return "Normal";

   string portName;
   int32 enumIndex;
   if (!GetEnumParam_PvCam((uns32)PARAM_READOUT_PORT, (uns32)portId, portName, enumIndex))
   {
      LOG_CAM_ERROR;
      LogMessage("Error in GetEnumParam in GetPortName");
   }
   switch (enumIndex)
   {
   case 0: portName = "EM"; break;
   case 1: portName = "Normal"; break;
   case 2: portName = "LowNoise"; break;
   case 3: portName = "HighCap"; break;
   default: portName = "Normal";
   }
   return portName;
}

bool Universal::GetEnumParam_PvCam(uns32 pvcam_cmd, uns32 index, std::string& enumString, int32& enumIndex)
{
   // First check this is an enumerated type:
   uint16_t dataType;
   RETURN_BOOL_IF_CAM_ERROR(
      pl_get_param_safe(hPVCAM_, pvcam_cmd, ATTR_TYPE, &dataType));

   if (dataType != TYPE_ENUM)
      return false;

   long unsigned int strLength;
   RETURN_BOOL_IF_CAM_ERROR(
      pl_enum_str_length_safe(hPVCAM_, pvcam_cmd, index, &strLength));

   char* strTmp;
   strTmp = (char*) malloc(strLength);
   int32 tmp;

   RETURN_BOOL_IF_CAM_ERROR(
      pl_get_enum_param_safe(hPVCAM_, pvcam_cmd, index, &tmp, strTmp, strLength));

   enumIndex = tmp;
   enumString = strTmp;
   free (strTmp);

   return true;
}

int Universal::CalculateImageBufferSize(ROI &newROI, unsigned short &newXSize, unsigned short &newYSize, rgn_type &newRegion)
{
   unsigned short xdim, ydim;

   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_ccd_get_par_size(hPVCAM_, &ydim));

   RETURN_CAM_ERROR_IF_CAM_ERROR(
      pl_ccd_get_ser_size(hPVCAM_, &xdim));

   if (newROI.isEmpty())
   {
      // set to full frame
      newROI.xSize = xdim;
      newROI.ySize = ydim;
   }

   // NOTE: roi dimensions are expressed in no-binning mode
   // format the image buffer
   // (assuming 2-byte pixels)
   newXSize = (unsigned short) (newROI.xSize/binSize_);
   newYSize = (unsigned short) (newROI.ySize/binSize_);

   // make an attempt to adjust the image dimensions
   while ((newXSize * newYSize * 2) % 4 != 0 && newXSize > 4 && newYSize > 4)
   {
      newROI.xSize--;
      newXSize = (unsigned short) (newROI.xSize/binSize_);
      if ((newXSize * newYSize * 2) % 4 != 0)
      {
         newROI.ySize--;
         newYSize = (unsigned short) (newROI.ySize/binSize_);
      }
   }

   newRegion.s1 = newROI.x;
   newRegion.s2 = newROI.x + newROI.xSize-1;
   newRegion.sbin = (uns16) binSize_;
   newRegion.p1 = newROI.y;
   newRegion.p2 = newROI.y + newROI.ySize-1;
   newRegion.pbin = (uns16) binSize_;

   return DEVICE_OK;
}


int Universal::ResizeImageBufferContinuous()
{
   //ToDo: use semaphore
   bufferOK_ = false;
   int nRet = DEVICE_ERR;

   ROI            newROI = roi_;
   unsigned short newXSize;
   unsigned short newYSize;
   rgn_type       newRegion;

   try
   {
      nRet = CalculateImageBufferSize(newROI, newXSize, newYSize, newRegion);
      RETURN_IF_MM_ERROR(nRet);

      img_.Resize(newXSize, newYSize, 2);

      uns32 frameSize;

      RETURN_CAM_ERROR_IF_CAM_ERROR(
         pl_exp_setup_cont(hPVCAM_, 1, &newRegion, TIMED_MODE, (uns32)exposure_, &frameSize, CIRC_OVERWRITE)); //Circular buffer only

      RETURN_CAM_ERROR_IF_CAM_ERROR(
         pl_exp_set_cont_mode(hPVCAM_, CIRC_OVERWRITE ));

      if (img_.Height() * img_.Width() * img_.Depth() != frameSize)
      {
         return DEVICE_INTERNAL_INCONSISTENCY; // buffer sizes don't match ???
      }

      // set up a circular buffer for 3 frames
      bufferSize_ = frameSize * 3;
      delete[] circBuffer_;
      circBuffer_ = new unsigned short[bufferSize_];
      roi_=newROI;
      nRet = DEVICE_OK;
   }
   catch(...)
   {
   }
   //ToDo: use semaphore
   bufferOK_ = true;
   singleFrameModeReady_=false;

   return nRet;
}

int Universal::ResizeImageBufferSingle()
{
   //ToDo: use semaphore
   bufferOK_ = false;
   int nRet = DEVICE_ERR;

   ROI            newROI = roi_;
   unsigned short newXSize;
   unsigned short newYSize;
   rgn_type       newRegion;

   try
   {
      nRet = CalculateImageBufferSize(newROI, newXSize, newYSize, newRegion);
      RETURN_IF_MM_ERROR(nRet);

      img_.Resize(newXSize, newYSize, 2);

      uns32 frameSize;

      RETURN_CAM_ERROR_IF_CAM_ERROR(
         pl_exp_setup_seq(hPVCAM_, 1, 1, &newRegion, TIMED_MODE, (uns32)exposure_, &frameSize ));

      if (img_.Height() * img_.Width() * img_.Depth() != frameSize)
      {
         return DEVICE_INTERNAL_INCONSISTENCY; // buffer sizes don't match ???
      }

      roi_=newROI;
      nRet = DEVICE_OK;
      singleFrameModeReady_=true;
   }
   catch(...)
   {
   }
   //ToDo: use semaphore
   bufferOK_ = true;
   return nRet;
}


///////////////////////////////////////////////////////////////////////////////
// Continuous acquisition
//
/*
*Overrides a virtual function from the CCameraBase class
*Do actual capture
*called from the acquisition thread function
*/
int Universal::ThreadRun(void)
{
   int16 status;
   uns32 byteCnt;
   uns32 bufferCnt;

   int ret=DEVICE_ERR;

   // wait until image is ready
   while (pl_exp_check_cont_status(hPVCAM_, &status, &byteCnt, &bufferCnt) //Circular buffer only
      && (status != READOUT_COMPLETE) 
      && (status != READOUT_FAILED))
   {
      CDeviceUtils::SleepMs(5);
   }

   if (status != READOUT_FAILED)
   {
      ret = PushImage();
   }
   else
   {
      LOG_MESSAGE(std::string("PVCamera readout failed"));
   }

   return ret;
}


/**
* Starts continuous acquisition.
*/
int Universal::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
   if (IsCapturing())
      return ERR_BUSY_ACQUIRING;

   int nRet = DEVICE_ERR;

   stopOnOverflow_ = stopOnOverflow;
   numImages_ = numImages;
   ostringstream os;
   os << "Started sequnce acquisition: " << numImages << " at " << interval_ms << " ms" << endl;
   LogMessage(os.str().c_str());

   // prepare the camera
   nRet = ResizeImageBufferContinuous();
   RETURN_IF_MM_ERROR(nRet);

   // start thread
   // prepare the core
   nRet = GetCoreCallback()->PrepareForAcq(this);
   if (nRet != DEVICE_OK)
   {
      ResizeImageBufferContinuous();
      RETURN_IF_MM_ERROR(nRet);
   }

   if (!pl_exp_start_cont(hPVCAM_, circBuffer_, bufferSize_)) //Circular buffer only
   {
      LOG_CAM_ERROR;
      ResizeImageBufferSingle();
      return DEVICE_ERR;
   }

   thd_->Start(numImages, interval_ms);

   // set actual interval the same as exposure
   // with PVCAM there is no straightforward way to get actual interval
   // TODO: create a better estimate
   SetProperty(MM::g_Keyword_ActualInterval_ms, CDeviceUtils::ConvertToString(exposure_)); 

   os << "Started sequence acquisition: " << numImages << " at " << interval_ms << " ms" << endl;
   LogMessage(os.str().c_str());

   return DEVICE_OK;
}


void Universal::OnThreadExiting() throw ()
{
   try {
      LOG_IF_CAM_ERROR(
         pl_exp_stop_cont(hPVCAM_, CCS_HALT)); //Circular buffer only
      LOG_IF_CAM_ERROR(
         pl_exp_finish_seq(hPVCAM_, circBuffer_, 0));
   } catch (...) {
      LogMessage(g_Msg_EXCEPTION_IN_ON_THREAD_EXITING, false);
      LOG_CAM_ERROR;
   }
   CCameraBase<Universal>::OnThreadExiting();
}

/**
* Stops acquisition
*/
int Universal::StopSequenceAcquisition()
{
   int nRet = DEVICE_ERR;
   //call function of the base class, which does a useful work
   nRet = this->CCameraBase<Universal>::StopSequenceAcquisition();
   LOG_IF_MM_ERROR(nRet);

   LOG_IF_CAM_ERROR(
      pl_exp_stop_cont(hPVCAM_, CCS_HALT)); //Circular buffer only
   LOG_IF_CAM_ERROR(
      pl_exp_finish_seq(hPVCAM_, circBuffer_, 0));

   return nRet;
}

/**
* Waits for new image and inserts it in the circular buffer.
* This method is called by the acquisition thread AcqSequenceThread::svc()
* in an infinite loop.
*
* In case of error or if the sequence is finished StopSequenceAcquisition()
* is called, which will raise the stop_ flag and cause the thread to exit.
*/
int Universal::PushImage()
{
   int nRet = DEVICE_ERR;
   // get the image from the circular buffer
   void_ptr imgPtr;
   bool oldest_frames_get_mode=false;/*=!noSupportForStreaming_*/
   rs_bool result = oldest_frames_get_mode
      ?pl_exp_get_oldest_frame(hPVCAM_, &imgPtr) //Circular buffer only
      :pl_exp_get_latest_frame(hPVCAM_, &imgPtr); //Circular buffer only

   RETURN_DEVICE_ERR_IF_CAM_ERROR(result);

   if(oldest_frames_get_mode)
   {
      result = pl_exp_unlock_oldest_frame(hPVCAM_); //Circular buffer only
      RETURN_DEVICE_ERR_IF_CAM_ERROR(result);
   }

   void* pixBuffer = const_cast<unsigned char*> (img_.GetPixels());
   memcpy(pixBuffer, imgPtr, GetImageBufferSize());

   // process image
   MM::ImageProcessor* ip = GetCoreCallback()->GetImageProcessor(this);
   if (ip)
   {
      nRet = ip->Process((unsigned char*) pixBuffer, GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());
      RETURN_IF_MM_ERROR(nRet);
   }
   // This method inserts new image in the circular buffer (residing in MMCore)
   nRet = GetCoreCallback()->InsertImage(this, (unsigned char*) pixBuffer,
      GetImageWidth(),
      GetImageHeight(),
      GetImageBytesPerPixel());
   if (!stopOnOverflow_ && nRet == DEVICE_BUFFER_OVERFLOW)
   {
      // do not stop on overflow - just reset the buffer
      GetCoreCallback()->ClearImageBuffer(this);
      nRet = GetCoreCallback()->InsertImage(this, (unsigned char*) pixBuffer,
         GetImageWidth(),
         GetImageHeight(),
         GetImageBytesPerPixel());;
   }

   LOG_IF_MM_ERROR(nRet);

   return nRet;
}



rs_bool Universal::pl_get_param_safe(int16 hcam, uns32 param_id,
                                     int16 param_attribute, void_ptr param_value)
{
   rs_bool ret;
   suspend();
   ret = pl_get_param (hcam, param_id, param_attribute, param_value);
   LOG_IF_CAM_ERROR(ret);
   resume();
   return ret;
}


rs_bool Universal::pl_set_param_safe(int16 hcam, uns32 param_id, void_ptr param_value)
{
   rs_bool ret;
   suspend();

   ret = pl_set_param (hcam, param_id, param_value);
   LOG_IF_CAM_ERROR(ret);
   resume();
   return ret;
}


bool Universal::GetLongParam_PvCam_safe(int16 handle, uns32 pvcam_cmd, long *value) 
{
   bool ret;
   suspend();
   ret = GetLongParam_PvCam(handle, pvcam_cmd, value);
   LOG_IF_CAM_ERROR(ret);
   resume();
   return ret;
}

bool Universal::SetLongParam_PvCam_safe(int16 handle, uns32 pvcam_cmd, long value) 
{
   bool ret;
   suspend();
   ret = SetLongParam_PvCam(handle, pvcam_cmd, value);
   LOG_IF_CAM_ERROR(ret);
   resume();
   return ret;
}


rs_bool Universal::pl_get_enum_param_safe (int16 hcam, uns32 param_id, uns32 index,
                                           int32_ptr value, char_ptr desc,
                                           uns32 length)
{
   rs_bool ret;
   suspend();
   ret = pl_get_enum_param(hcam, param_id, index, value, desc, length);
   LOG_IF_CAM_ERROR(ret);
   resume();
   return ret;
}

rs_bool Universal::pl_enum_str_length_safe (int16 hcam, uns32 param_id, uns32 index,
                                            uns32_ptr length)
{
   rs_bool ret;
   suspend();
   ret = pl_enum_str_length(hcam, param_id, index, length);
   LOG_IF_CAM_ERROR(ret);
   resume();
   return ret;
}


void Universal::LogCamError(
                            std::string strMessage, 
                            std::string strLocation, 
                            bool bDebugonly /*=false*/
                            ) const throw()
{
   try
   {
      int16 nErrCode = pl_error_code();
      char msg[ERROR_MSG_LEN];
      if(!pl_error_message (nErrCode, msg))
      {
         CDeviceUtils::CopyLimitedString(msg, "Unknown");
      }
      ostringstream os;
      os << "PVCAM API error: "<< msg <<endl;
      os << strMessage; 
      os << strLocation<<endl;
      LogMessage(os.str(), bDebugonly);
   }
   catch(...){}
}

void Universal::LogMMError(
                           int nErrCode, 
                           std::string strMessage, 
                           std::string strLocation, 
                           bool bDebugonly/*=false*/
                           )const throw()
{
   try
   {
      char strText[MM::MaxStrLength];
      if (!CCameraBase<Universal>::GetErrorText(nErrCode, strText))
      {
         CDeviceUtils::CopyLimitedString(strText, "Unknown");
      }
      ostringstream os;
      os << "MM API error: "<< strText <<endl;
      os << strMessage; 
      os << strLocation<<endl;
      LogMessage(os.str(), bDebugonly);
   }
   catch(...){}
}
