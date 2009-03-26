/////////////////////////////////////////////////////////////////////////////
// Name:        ABSCamera.cpp
// Purpose:     Implementierung der Kameraklasse als Adapter für µManager
// Author:      Michael Himmelreich
// Created:     31. Juli 2007
// Copyright:   (c) Michael Himmelreich
// Project:     ConfoVis
/////////////////////////////////////////////////////////////////////////////


// ------------------------------ Includes --------------------------------------
//

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define snprintf _snprintf 
#endif

#include <string>
#include <math.h>
#include <sstream>

#include "ABSCamera.h"
#include "ModuleInterface.h"
#include "CamUSB_API.h"
#include "CamUSB_API_Util.h"
#include "CamUSB_API_Ext.h"


#include "SafeUtil.h"			//!< macros to delete release pointers safely


// ------------------------------ Defines --------------------------------------
//

using namespace std;

// External names used used by the rest of the system
// to load particular device from the "DemoCamera.dll" library
const char* g_CameraDeviceNameBase = "ABSCam";

//! \brief list of supported pixeltypes (supported for this application)
struct tagSupportedPixelTypes
{
   std::string szPixelTypeName;
   u32         dwPixelType;
} g_sSupportedPixelTypes[] =
{
   {"8Bit_Mono",   PIX_MONO8},
   {"10Bit_Mono",  PIX_MONO10},
   {"12Bit_Mono",  PIX_MONO12},
   {"8Bit_Color",  PIX_BGRA8_PACKED}
   //,{"10Bit_Color", PIX_BGRA10_PACKED}
   //,{"12Bit_Color", PIX_BGRA12_PACKED}
};


// ------------------------------ Macros --------------------------------------
//
// return the error code of the function instead of Successfull or Failed!!!
#define GET_RC(func, dev)  ((!func) ? CamUSB_GetLastError(dev) : retOK)

// ------------------------------ DLL main --------------------------------------
//
// windows DLL entry code
#ifdef WIN32
  BOOL APIENTRY DllMain( HANDLE /*hModule*/, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/ ) 
  {
    switch (ul_reason_for_call)
   	{
      case DLL_PROCESS_ATTACH:
  	  case DLL_THREAD_ATTACH:
   	  case DLL_THREAD_DETACH:
   	  case DLL_PROCESS_DETACH:
   		break;
   	}
    return TRUE;
  }
#endif

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
   // fire the debugger directly or crash without one
   /*
   __asm
   {
      int 3;
   }
   */

	u32 dwCountCameras;
	u32 dwElements = 0;
	S_CAMERA_LIST_EX* pCamLstEx = NULL;

	// suche angeschlossene ABS Camera's
	dwCountCameras = CamUSB_GetCameraListEx( pCamLstEx, dwElements );

	if (dwCountCameras > 0)
	{
      dwElements= dwCountCameras;
		pCamLstEx = new S_CAMERA_LIST_EX[dwElements];      
		dwCountCameras = CamUSB_GetCameraListEx( pCamLstEx, dwElements );
	}

	if (dwCountCameras > 0)
	{
		std::string strCameraDescription;	// description of the camera			
		std::string strDeviveName;			// used devicename
		for (DWORD dwCam = 0; dwCam < dwCountCameras; dwCam++)
		{
			strCameraDescription.resize(MAX_PATH);
         snprintf( (char*)strCameraDescription.c_str(), MAX_PATH, "ABS %s #%X", (char*)pCamLstEx[dwCam].sVersion.szDeviceName, pCamLstEx[dwCam].sVersion.dwSerialNumber); 			
			
			strDeviveName.resize(MAX_PATH);
			snprintf( (char*)strDeviveName.c_str(), MAX_PATH, "%s%02d", g_CameraDeviceNameBase, dwCam);
			AddAvailableDeviceName((char*)strDeviveName.c_str(), (char*)strCameraDescription.c_str() );
		}		
	}	

	SAFE_DELETE_ARRAY(pCamLstEx);	
}

MODULE_API MM::Device* CreateDevice(const char* szDeviceName)
{
    // fire the debugger directly or crash without one
   /*
   __asm
   {
      int 3;
   }
   */

	// decide which device class to create based on the deviceName parameter
	if (NULL != szDeviceName)       
	{
      int iDeviceNumber = 0;

      // should the first camera be used
      if ((std::string(g_CameraDeviceNameBase).compare(szDeviceName) == 0) ||                    // first one
         (-1 != sscanf_s(szDeviceName + strlen(g_CameraDeviceNameBase), "%d", &iDeviceNumber)) ) // speciffic camera should be used
      {
         // create camera with specific camera id
         return new ABSCamera(iDeviceNumber, szDeviceName);
      }      
	}
   // ...supplied name not recognized
   return NULL;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   SAFE_DELETE(pDevice);   
}

///////////////////////////////////////////////////////////////////////////////
// ABSCamera implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~

/**
 * ABSCamera constructor.
 * Setup default all variables and create device properties required to exist
 * before intialization. In this case, no such properties were required. All
 * properties will be created in the Initialize() method.
 *
 * As a general guideline Micro-Manager devices do not access hardware in the
 * the constructor. We should do as little as possible in the constructor and
 * perform most of the initialization in the Initialize() method.
 */
ABSCamera::ABSCamera(int iDeviceNumber, const char* szDeviceName) : 
   initialized(false),
   busy(false),
   deviceNumber(0),
   cameraFunctionMask(0),
   resolutionCap( NULL ),
   pixelTypeCap( NULL ),
   gainCap( NULL ),
   exposureCap( NULL ),
   autoExposureCap( NULL ),
   numberOfChannels(1)
{
   // remember device name and set the device index    
   m_szDeviceName = std::string(szDeviceName);
   deviceNumber   = (BYTE) iDeviceNumber;

   // call the base class method to set-up default error codes/messages
   InitializeDefaultErrorMessages();
}

/**
 * ABSCamera destructor.
 * If this device used as intended within the Micro-Manager system,
 * Shutdown() will be always called before the destructor. But in any case
 * we need to make sure that all resources are properly released even if
 * Shutdown() was not called.
 */
ABSCamera::~ABSCamera()
{
  if ( this->initialized ) this->Shutdown();
}

/**
 * Obtains device name.
 * Required by the MM::Device API.
 */
void ABSCamera::GetName(char* name) const
{
   // We just return the name we use for referring to this
   // device adapter. 
   CDeviceUtils::CopyLimitedString(name, (char*) m_szDeviceName.c_str());
}

/**
 * Tells us if device is still processing asynchronous command.
 * Required by the MM:Device API.
 */
bool ABSCamera::Busy()
{
  return this->busy;
}

/**
 * Intializes the hardware.
 * Required by the MM::Device API.
 * Typically we access and initialize hardware at this point.
 * Device properties are typically created here as well, except
 * the ones we need to use for defining initialization parameters.
 * Such pre-initialization properties are created in the constructor.
 * (This device does not have any pre-initialization properties)
 */
int ABSCamera::Initialize()
{
  u32 dwRC;
  if ( this->initialized ) return DEVICE_OK;

  this->busy = true;

  /* 
  // initialise camera
  // try to init camera without reboot (makes the init process a bit fast)
  dwRC = GET_RC(CamUSB_InitCamera( this->deviceNumber, NO_SERIAL_NUMBER, FALSE ), this->deviceNumber);
  
  // on error try to boot the camera
  if ( !IsNoError(dwRC) )
  */
  {
      // init camera with reboot
      dwRC = GET_RC(CamUSB_InitCamera( this->deviceNumber ), this->deviceNumber);

      if ( !IsNoError(dwRC) )
      {
        this->ShowError();
        return DEVICE_ERR;
      }
  }

  // read camera information
  camVersion.dwStructSize = sizeof( S_CAMERA_VERSION );
  if ( !CamUSB_GetCameraVersion( &camVersion ) )
  {
    this->ShowError();
    return DEVICE_ERR;
  }

  // set device name
  int nRet = CreateProperty(MM::g_Keyword_Name, (char*) m_szDeviceName.c_str(), MM::String, true);
  if ( nRet != DEVICE_OK ) return nRet;

  // set device description
  nRet = CreateProperty(MM::g_Keyword_Description, "ABS Camera Device Adapter", MM::String, true);
  if ( nRet != DEVICE_OK ) return nRet;

  // set camera name
  ostringstream cameraName;
  cameraName << camVersion.szDeviceName;
  nRet = CreateProperty(MM::g_Keyword_CameraName, cameraName.str().c_str(), MM::String, true);
  assert(nRet == DEVICE_OK);
        
  // set camera ID
  ostringstream serialNumber;
  serialNumber << camVersion.dwSerialNumber;
  nRet = CreateProperty(MM::g_Keyword_CameraID, serialNumber.str().c_str(), MM::String, true);
  assert(nRet == DEVICE_OK);


  // try to set the default resolution for this camera
  dwRC = GET_RC(CamUSB_SetStandardRes(STDRES_FULLSENSOR, this->deviceNumber), this->deviceNumber);

  // read function mask of the camera
  if ( !CamUSB_GetCameraFunctions( &this->cameraFunctionMask, this->deviceNumber ) )
  {
    this->ShowError();
    return DEVICE_ERR;
  }

  // clear camaera caps
  if ( this->resolutionCap != NULL ) { delete this->resolutionCap; this->resolutionCap = NULL; }
  if ( this->pixelTypeCap != NULL ) { delete this->pixelTypeCap; this->pixelTypeCap = NULL; }
  if ( this->gainCap != NULL ) { delete this->gainCap; this->gainCap = NULL; }
  if ( this->exposureCap != NULL ) { delete this->exposureCap; this->exposureCap = NULL; }
  if ( this->autoExposureCap != NULL ) { delete this->autoExposureCap; this->autoExposureCap = NULL; }

  // read camera capabilities
  try
  {
    this->resolutionCap = (S_RESOLUTION_CAPS*) this->GetCameraCap( FUNC_RESOLUTION );
    this->pixelTypeCap = (S_PIXELTYPE_CAPS*) this->GetCameraCap( FUNC_PIXELTYPE );
    this->gainCap = (S_GAIN_CAPS*) this->GetCameraCap( FUNC_GAIN );
    this->exposureCap = (S_EXPOSURE_CAPS*) this->GetCameraCap( FUNC_EXPOSURE );
    this->autoExposureCap = (S_AUTOEXPOSURE_CAPS*) this->GetCameraCap( FUNC_AUTOEXPOSURE );
  }
  catch ( exception* /* e */ )
  {
    this->ShowError();
    return DEVICE_ERR;
  }

  // turn autoexposure off
  if ( this->autoExposureCap != NULL )
  {
    S_AUTOEXPOSURE_PARAMS autoExposurePara;
    if ( this->GetCameraFunction( FUNC_AUTOEXPOSURE, &autoExposurePara, sizeof(S_AUTOEXPOSURE_PARAMS) ) != DEVICE_OK ) return DEVICE_ERR;

    autoExposurePara.bAECActive = 0;
    autoExposurePara.bAGCActive = 0;

    if ( this->SetCameraFunction( FUNC_AUTOEXPOSURE, &autoExposurePara, sizeof(S_AUTOEXPOSURE_PARAMS) ) != DEVICE_OK ) return DEVICE_ERR;
  }     

  // set capture mode to continuous -> higher Framerates for the live view in µManager
  dwRC = GET_RC(CamUSB_SetCaptureMode(MODE_CONTINUOUS, 0, this->deviceNumber), this->deviceNumber);
  
  if ( !IsNoError(dwRC) )
  {
    this->ShowError();
    return DEVICE_ERR;
  }

  // setup exposure
  nRet = this->createExposure();
  if ( nRet != DEVICE_OK ) return nRet;

  // setup binning
  nRet = this->createBinning();
  if ( nRet != DEVICE_OK ) return nRet;

  // setup pixel type
  nRet = this->createPixelType();
  if ( nRet != DEVICE_OK ) return nRet;

  // setup gain
  nRet = this->createGain();
  if ( nRet != DEVICE_OK ) return nRet;

  // synchronize all properties
  // --------------------------
  nRet = UpdateStatus();
  if (nRet != DEVICE_OK) return nRet;

  // setup the image buffer
  // ----------------
  nRet = this->ResizeImageBuffer();
  if (nRet != DEVICE_OK) return nRet;

  this->initialized = true;
  this->busy = false;

  return DEVICE_OK;
}

// setup exposure
int ABSCamera::createExposure()
{
  u32 dwRC;
  int nRet;
  float exposureTime = 100.0f; // exposure time in ms

  unsigned long exposureValue; // current hardware exposure time in µs

  // set basic exposure value
  if (camVersion.wSensorType & ST_COLOR) // color camera?
  {
    exposureValue = 75*1000; // µs
  }
  else  
  {
    exposureValue = 35*1000; // µs
  }
    
  // set and read back the exposure
  dwRC = GET_RC(CamUSB_SetExposureTime( &exposureValue, this->deviceNumber ), this->deviceNumber);
  if (!IsNoError(dwRC))
  {
    this->ShowError();
    return DEVICE_ERR;
  }

  // µs -> ms
  exposureTime = static_cast<float>( exposureValue ) / 1000.0f;

  // create exposure property 
  ostringstream exposureTimeStr;
  exposureTimeStr << exposureTime;

  CPropertyAction *pAct = new CPropertyAction (this, &ABSCamera::OnExposure); // function called if exposure time property is read or written
  nRet = CreateProperty(MM::g_Keyword_Exposure, exposureTimeStr.str().c_str(), MM::Float, false, pAct ); // create property
  if ( nRet != DEVICE_OK ) return nRet;

  return UpdateExposureLimits();
 
}

// set limits for exposure time
int ABSCamera::UpdateExposureLimits(void)
{  
  float minExposureTime; // min. allowed exposure time
  float maxExposureTime; // max. allowed exposure time

  if ( this->exposureCap != NULL )
  {
    minExposureTime = static_cast<float>( this->exposureCap->sExposureRange[0].dwMin ) / 1000.0f;
    maxExposureTime = static_cast<float>( this->exposureCap->sExposureRange[this->exposureCap->dwCountRanges-1].dwMax ) / 1000.0f;
  }
  else
  {
    u32 dwRC;
    u32 dwExposure = 0;
    // get exposure time
    dwRC = GET_RC(CamUSB_SetExposureTime( &dwExposure, this->deviceNumber ), this->deviceNumber);
    if (!IsNoError(dwRC))
    {
      this->ShowError();
      return DEVICE_ERR;
    }
    minExposureTime = static_cast<float>( dwExposure ) / 1000.0f;
    maxExposureTime = static_cast<float>( dwExposure ) / 1000.0f;
  }
  return SetPropertyLimits( MM::g_Keyword_Exposure, minExposureTime, maxExposureTime ); // set limits 
}

// setup binning
int ABSCamera::createBinning()
{
  int nRet;

  CPropertyAction *pAct = new CPropertyAction (this, &ABSCamera::OnBinning); // function called if binning property is read or written
  nRet = CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false, pAct); // create binning property
  if ( nRet != DEVICE_OK ) return nRet;

  // setup allowed values for binning
  if ( this->resolutionCap == NULL || this->resolutionCap->dwBinModes == XY_BIN_NONE ) return DEVICE_OK;

  // create vector of allowed values
  vector<string> binValues;
  binValues.push_back("1");
  
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_2X ) != 0 ) binValues.push_back("2");
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_3X ) != 0 ) binValues.push_back("3");
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_4X ) != 0 ) binValues.push_back("4");
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_5X ) != 0 ) binValues.push_back("5");
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_6X ) != 0 ) binValues.push_back("6");
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_7X ) != 0 ) binValues.push_back("7");
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_8X ) != 0 ) binValues.push_back("8");
  if ( ( this->resolutionCap->dwBinModes & XY_BIN_9X ) != 0 ) binValues.push_back("9");

  return SetAllowedValues(MM::g_Keyword_Binning, binValues); // set allowed values for binning property
}

// setup pixel types
int ABSCamera::createPixelType()
{
  int nRet;
  string pixelTypeHW;
  bool   bColorSupported = false;
  bool   bSupportedPixeltypeUsed = false;
  CPropertyAction* pAct = new CPropertyAction (this, &ABSCamera::OnPixelType);  // function called if pixel type property is read or written
  vector<string> pixelTypeValues; // vector of allowed pixel types


  // fill vector of allowed pixel types
  if ( this->pixelTypeCap != NULL )
  {
    for ( unsigned int p=0; p < pixelTypeCap->dwCount; p++ )
    {
       if (!bColorSupported) bColorSupported = (IsBayer(pixelTypeCap->dwPixelType[p]) == TRUE);

       for ( u32 n=0; n < CNT_ELEMENTS(g_sSupportedPixelTypes); n++ )
       {
          if( pixelTypeCap->dwPixelType[p] == g_sSupportedPixelTypes[n].dwPixelType )
          {
             pixelTypeValues.push_back( g_sSupportedPixelTypes[n].szPixelTypeName );             
             break;
          }
       }
    }     
    
    // read current pixel type from hardware
    unsigned long pixelType;
    if ( !CamUSB_GetPixelType( &pixelType, this->deviceNumber ) )
    {
      this->ShowError();
      return DEVICE_ERR;
    }

   // create pixel type value from current pixel type
   for ( u32 n=0; n < CNT_ELEMENTS(g_sSupportedPixelTypes); n++ )
   {
       if( pixelType == g_sSupportedPixelTypes[n].dwPixelType )
       {
          pixelTypeHW = g_sSupportedPixelTypes[n].szPixelTypeName;
          bSupportedPixeltypeUsed = true;
          break;
       }
   }

   // check if the current pixeltype is supported for this application
   if (!bSupportedPixeltypeUsed)
   {
      unsigned long size = sizeof(S_PIXELTYPE_PARAMS);
      S_PIXELTYPE_PARAMS pixelTypePara;

      if (bColorSupported)
      {
         pixelTypePara.dwPixelType = PIX_BGR8_PACKED;
      }
      else
      {
         pixelTypePara.dwPixelType = PIX_MONO8;
      }

      if (!CamUSB_SetFunction( FUNC_PIXELTYPE, &pixelTypePara, size ) )
      {
         this->ShowError();         
      }

      // read current pixel type from hardware       
       if ( !CamUSB_GetPixelType( &pixelType, this->deviceNumber ) )
       {
         this->ShowError();
         return DEVICE_ERR;
       }

      // create pixel type value from current pixel type
      for ( u32 n=0; n < CNT_ELEMENTS(g_sSupportedPixelTypes); n++ )
      {
          if( pixelType == g_sSupportedPixelTypes[n].dwPixelType )
          {
             pixelTypeHW = g_sSupportedPixelTypes[n].szPixelTypeName;
             bSupportedPixeltypeUsed = true;
             break;
          }
      }
   }

    // setup number of channels depending on current pixel type
    numberOfChannels = 1;
    if ( (GetBpp( pixelType ) / GetUsedBpp( pixelType )) > 1 ) 
    {
       numberOfChannels = 4;
    }
   
    if (( pixelTypeValues.size() == 0 ) || (!bSupportedPixeltypeUsed)) return DEVICE_ERR; // there must be at least one allowed pixel type
    nRet = CreateProperty(MM::g_Keyword_PixelType, pixelTypeHW.c_str(), MM::String, false, pAct); // create pixel type property
    if ( nRet != DEVICE_OK ) return nRet; // bei Fehler -> Abbruch
  }
  else
  {
    nRet = DEVICE_ERR;
  }

  return SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues); // set allowed pixel types 
}

// setup gain
int ABSCamera::createGain()
{
  int nRet;

  float gain = 0.0f;
  CPropertyAction *pAct = new CPropertyAction (this, &ABSCamera::OnGain);   // function called if gein is read or written


  // read current gain from hardware
  if ( this->gainCap == NULL )
  {
    nRet = CreateProperty(MM::g_Keyword_Gain, "1.0", MM::Float, true, pAct ); // Property für Gain anlegen
    if ( nRet != DEVICE_OK ) return nRet;
    return SetPropertyLimits( MM::g_Keyword_Gain, 1.0, 1.0 ); // limits setzen
  }

  float gainFactor = 1.0f; // factor to calculate value for gain property

  if ( this->gainCap->bGainUnit == GAINUNIT_NONE ) gainFactor =  0.001f;
  if ( this->gainCap->bGainUnit == GAINUNIT_10X  ) gainFactor = 0.0001f;

  u32 dwRC;
  u32 dwGain;
  u16 wGainChannel;
  // check if color or monochrom camera
  BOOL bRGBGain = ((gainCap->wGainChannelMask & GAIN_RGB) == GAIN_RGB);

  // set default gain 0
  wGainChannel = (bRGBGain) ? (GAIN_GREEN | GAIN_LOCKED) : GAIN_GLOBAL;
  dwGain = 0;
  dwRC = GET_RC(CamUSB_SetGain( &dwGain, wGainChannel, this->deviceNumber), this->deviceNumber);

  // get the current gain (may it is different from the one set (WhiteBalance)
  wGainChannel = (bRGBGain) ? GAIN_GREEN : GAIN_GLOBAL;
  dwRC = GET_RC(CamUSB_GetGain( &dwGain, wGainChannel, this->deviceNumber), this->deviceNumber);
  if ( !IsNoError(dwRC) )
  {
    nRet = CreateProperty(MM::g_Keyword_Gain, "1.0", MM::Float, true, pAct ); // Property für Gain anlegen
    if ( nRet != DEVICE_OK ) return nRet;
    return SetPropertyLimits( MM::g_Keyword_Gain, 1.0, 1.0 ); // limits setzen
  }
  else // gain value read
  {
    gain = static_cast<float>( dwGain ) * gainFactor;
  }

  ostringstream gainStr;
  gainStr << gain;

  nRet = CreateProperty(MM::g_Keyword_Gain, gainStr.str().c_str(), MM::Float, false, pAct ); // create gain property 
  if ( nRet != DEVICE_OK ) return nRet;

  // set limits for gain property
  float minGain = static_cast<float>( this->gainCap->sGainRange[0].dwMin) * gainFactor; // min. allowed gain value
  float maxGain =  static_cast<float>( this->gainCap->sGainRange[this->gainCap->wGainRanges-1].dwMax) * gainFactor; // max. allowed gain value

  return SetPropertyLimits( MM::g_Keyword_Gain, minGain, maxGain ); // set limits for gain property
}


/**
 * Shuts down (unloads) the device.
 * Required by the MM::Device API.
 * Ideally this method will completely unload the device and release all resources.
 * Shutdown() may be called multiple times in a row.
 * After Shutdown() we should be allowed to call Initialize() again to load the device
 * without causing problems.
 */
int ABSCamera::Shutdown()
{
   CamUSB_FreeCamera( this->deviceNumber );
   if ( this->resolutionCap != NULL ) { delete this->resolutionCap; this->resolutionCap = NULL; }
   if ( this->pixelTypeCap != NULL ) { delete this->pixelTypeCap; this->pixelTypeCap = NULL; }
   if ( this->gainCap != NULL ) { delete this->gainCap; this->gainCap = NULL; }
   if ( this->exposureCap != NULL ) { delete this->exposureCap; this->exposureCap = NULL; }
 
   this->initialized = false;
   return DEVICE_OK;
}



/**
 * Performs exposure and grabs a single image.
 * Required by the MM::Camera API.
 */
int ABSCamera::SnapImage()
{
  // useing own buffers
  u32 dwRC;
  // read required size of the image buffer
  unsigned long imageSize = static_cast<unsigned long>( this->GetImageBufferSize() );

  u08*            pImg    = (u08*) imageBuffer.GetPixels(); // pointer of image
  S_IMAGE_HEADER  sImgHdr = {0};
  S_IMAGE_HEADER* pImgHdr = &sImgHdr; // pointer of image header
  
  dwRC = GET_RC(CamUSB_GetImage( &pImg, &pImgHdr, imageSize, this->deviceNumber), this->deviceNumber);
  
  if (retOK != dwRC)
  {  
    this->ShowError();
    return DEVICE_ERR;
  }

  return DEVICE_OK;
}

/**
 * Returns pixel data.
 * Required by the MM::Camera API.
 * The calling program will assume the size of the buffer based on the values
 * obtained from GetImageBufferSize(), which in turn should be consistent with
 * values returned by GetImageWidth(), GetImageHight() and GetImageBytesPerPixel().
 * The calling program allso assumes that camera never changes the size of
 * the pixel buffer on its own. In other words, the buffer can change only if
 * appropriate properties are set (such as binning, pixel type, etc.)
 */
const unsigned char* ABSCamera::GetImageBuffer()
{
   return imageBuffer.GetPixels();
}
// used for color images
const unsigned int* ABSCamera::GetImageBufferAsRGB32()
{
    return (const unsigned int*)imageBuffer.GetPixels();    
}

unsigned int ABSCamera::GetNumberOfChannels() const
{
   return this->numberOfChannels;
}
 

/**
 * Returns image buffer X-size in pixels.
 * Required by the MM::Camera API.
 */
unsigned ABSCamera::GetImageWidth() const
{
   return imageBuffer.Width();
}

/**
 * Returns image buffer Y-size in pixels.
 * Required by the MM::Camera API.
 */
unsigned ABSCamera::GetImageHeight() const
{
   return imageBuffer.Height();
}

/**
 * Returns image buffer pixel depth in bytes.
 * Required by the MM::Camera API.
 */
unsigned ABSCamera::GetImageBytesPerPixel() const
{
   return imageBuffer.Depth() / this->GetNumberOfChannels();
} 

/**
 * Returns the bit depth (dynamic range) of the pixel.
 * This does not affect the buffer size, it just gives the client application
 * a guideline on how to interpret pixel values.
 * Required by the MM::Camera API.
 */
unsigned ABSCamera::GetBitDepth() const
{
  
  unsigned int bitDepth = 8;

  // read current pixel type from hardware
  unsigned long pixelType;
  if ( CamUSB_GetPixelType( &pixelType, this->deviceNumber ) )
  {
      bitDepth = GetUsedBpp( pixelType );    
  }
  else
  {
    this->ShowError();
  }

  return bitDepth;
}

/**
 * Returns the size in bytes of the image buffer.
 * Required by the MM::Camera API.
 */
long ABSCamera::GetImageBufferSize() const
{
   return imageBuffer.Width() * imageBuffer.Height() * GetImageBytesPerPixel() * GetNumberOfChannels();
}

/**
 * Sets the camera Region Of Interest.
 * Required by the MM::Camera API.
 * This command will change the dimensions of the image.
 * Depending on the hardware capabilities the camera may not be able to configure the
 * exact dimensions requested - but should try do as close as possible.
 * If the hardware does not have this capability the software should simulate the ROI by
 * appropriately cropping each frame.
 * @param x - top-left corner coordinate
 * @param y - top-left corner coordinate
 * @param xSize - width
 * @param ySize - height
 */
int ABSCamera::SetROI( unsigned x, unsigned y, unsigned xSize, unsigned ySize )
{ 
  u32 dwRC;
  if ( this->resolutionCap == NULL ) return DEVICE_ERR;

  short x_HW, y_HW; // hardware offsets
  unsigned short xSize_HW, ySize_HW; // hardware size
  unsigned long skip_HW, binning_HW; // hardware binning and skip

  // read hardware resolution values
  if ( !CamUSB_GetCameraResolution( &x_HW, &y_HW, &xSize_HW, &ySize_HW, &skip_HW, &binning_HW ) )
  {
    this->ShowError();
    return DEVICE_ERR;
  }

  dwRC = GET_RC(CamUSB_SetCameraResolution( static_cast<SHORT>( x ), static_cast<SHORT>( y ),
                                            static_cast<WORD>( xSize ), static_cast<WORD>( ySize ),
                                            skip_HW, binning_HW,                                              
                                            TRUE,
                                            this->deviceNumber),
                                            this->deviceNumber);

  if (IsNoError(dwRC))
  {
      // update exposure range setting because the depends on the currently set resolution
      try
      {
        SAFE_DELETE_ARRAY(this->exposureCap);
        this->exposureCap = (S_EXPOSURE_CAPS*) this->GetCameraCap( FUNC_EXPOSURE );    
        UpdateExposureLimits();
      }
      catch ( exception* /* e */ )
      {
        this->ShowError();
        return DEVICE_ERR;
      }
  }
  else
  {
      this->ShowError();
      return DEVICE_ERR;
  }

  // resize image buffer
  return this->ResizeImageBuffer();
}

/**
 * Returns the actual dimensions of the current ROI.
 * Required by the MM::Camera API.
 */
int ABSCamera::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{

  // read current ROI from hardware
  S_RESOLUTION_RETVALS resolutionReturn;
  unsigned long size = sizeof(S_RESOLUTION_RETVALS);
  if ( CamUSB_GetFunction( FUNC_RESOLUTION, &resolutionReturn, &size ) == false )
  {
    this->ShowError();
    return DEVICE_ERR;
  }
  
  x = resolutionReturn.wOffsetX;
  y = resolutionReturn.wOffsetY;
  xSize = resolutionReturn.wSizeX;
  ySize = resolutionReturn.wSizeY;

  return DEVICE_OK;
}

/**
 * Resets the Region of Interest to full frame.
 * Required by the MM::Camera API.
 */
int ABSCamera::ClearROI()
{
  // set ROI to full size
  if ( this->resolutionCap == NULL ) return DEVICE_ERR;
  this->SetROI( 0, 0, this->resolutionCap->wVisibleSizeX, this->resolutionCap->wVisibleSizeY );
  return DEVICE_OK;
}

/**
 * Returns the current exposure setting in milliseconds.
 * Required by the MM::Camera API.
 */
double ABSCamera::GetExposure() const
{
  char buf[MM::MaxStrLength];
  int ret = GetProperty(MM::g_Keyword_Exposure, buf);
  if (ret != DEVICE_OK) return 0.0;

  return atof(buf);
}

/**
 * Sets exposure in milliseconds.
 * Required by the MM::Camera API.
 */
void ABSCamera::SetExposure(double exp)
{
  SetProperty( MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(exp) );
}

// handels exposure property
int ABSCamera::OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{

  if ( this->exposureCap == NULL ) return DEVICE_OK;
  if (eAct == MM::AfterSet) // property was written -> apply value to hardware
  {
    double exposure;
    pProp->Get(exposure);

    unsigned long exposureValue = static_cast<unsigned long>(exposure) * 1000l;

    // set exposure time 
    if (!CamUSB_SetExposureTime( &exposureValue, this->deviceNumber ) )
    {
      this->ShowError();
      return DEVICE_ERR;
    }

    // value can be changed by hardware
    exposure = static_cast<double>( exposureValue ) / 1000.0;

    // write back current exposure time
    pProp->Set( exposure );
  }
  else if (eAct == MM::BeforeGet) // property will be read -> update property with value from hardware
  {
    double exposureHW;
    unsigned long exposureValue;

    // read exposure time from hardware
    if (!CamUSB_GetExposureTime( &exposureValue, this->deviceNumber ) )
    {
      this->ShowError();
      return DEVICE_ERR;
    }

    // µs -> ms
    exposureHW = static_cast<double>( exposureValue ) / 1000.0;

    // write hardware value to property
    pProp->Set( exposureHW );
  }

  return DEVICE_OK; 
  
}

// handels gain property
int ABSCamera::OnGain(MM::PropertyBase* pProp, MM::ActionType eAct)
{
  if ( this->gainCap == NULL ) return DEVICE_OK;
    
  // check if color or monochrom camera
  BOOL bRGBGain = ((this->gainCap->wGainChannelMask & GAIN_RGB) == GAIN_RGB);
  u32 dwRC;
  u32 dwGain;
  u16 wGainChannel;
  double gain;
  float gainFactor = 1.0f; // factor for calculating property value from hardware value 
  if ( this->gainCap->bGainUnit == GAINUNIT_NONE ) gainFactor =  0.001f;
  if ( this->gainCap->bGainUnit == GAINUNIT_10X  ) gainFactor = 0.0001f;

  if (eAct == MM::AfterSet) // property was written -> apply value to hardware
  {    
    pProp->Get(gain);

    // setup value
    dwGain = (u32) (gain / gainFactor);                             
    // set channel
    wGainChannel = (bRGBGain) ? (GAIN_GREEN | GAIN_LOCKED) : GAIN_GLOBAL;   

    dwRC = GET_RC(CamUSB_SetGain( &dwGain, wGainChannel, this->deviceNumber), this->deviceNumber);
    if ( !IsNoError(dwRC) )
    {
        this->ShowError();
        return DEVICE_ERR;
    }
    else // gain value set
    {
        gain = static_cast<float>( dwGain ) * gainFactor;
        pProp->Set( gain ); // write back to GUI
    }
  }
  else if (eAct == MM::BeforeGet) // property will be read -> update property with value from hardware
  {         
    dwGain = 0;
    // set channel
    wGainChannel = (bRGBGain) ? GAIN_GREEN : GAIN_GLOBAL;   

    dwRC = GET_RC(CamUSB_GetGain( &dwGain, wGainChannel, this->deviceNumber), this->deviceNumber);
    if ( !IsNoError(dwRC) )
    {
        this->ShowError();
        return DEVICE_ERR;
    }
    else // gain value set
    {
        gain = static_cast<float>( dwGain ) * gainFactor;
        pProp->Set( gain ); // write back to GUI
    }
  }
  return DEVICE_OK; 
}

/**
 * Returns the current binning factor.
 * Required by the MM::Camera API.
 */
int ABSCamera::GetBinning() const
{
  char buf[MM::MaxStrLength];
  int ret = GetProperty(MM::g_Keyword_Binning, buf);
  if (ret != DEVICE_OK) return ret;
  return atoi(buf);

}

/**
 * Sets binning factor.
 * Required by the MM::Camera API.
 */
int ABSCamera::SetBinning(int binFactor)
{
  return SetProperty(MM::g_Keyword_Binning, CDeviceUtils::ConvertToString(binFactor));
}


///////////////////////////////////////////////////////////////////////////////
// ABSCamera Action handlers
///////////////////////////////////////////////////////////////////////////////

/**
 * Handles "Binning" property.
 */
int ABSCamera::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
  u32 dwRC;
  if ( this->resolutionCap == NULL ) return DEVICE_OK; // property was written -> apply value to hardware
  if (eAct == MM::AfterSet)
  {
    long binning;
    pProp->Get(binning);

    unsigned long size = sizeof(S_RESOLUTION_RETVALS);
    S_RESOLUTION_RETVALS resolutionReturn;

    if (!CamUSB_GetFunction( FUNC_RESOLUTION, &resolutionReturn, &size ) )
    {
      this->ShowError();
      return DEVICE_ERR;
    }

    if ( binning == 1 ) resolutionReturn.dwBin = XY_BIN_NONE;
    if ( binning == 2 ) resolutionReturn.dwBin = X_BIN_2X;
    if ( binning == 3 ) resolutionReturn.dwBin = XY_BIN_3X;
    if ( binning == 4 ) resolutionReturn.dwBin = XY_BIN_4X;
    if ( binning == 5 ) resolutionReturn.dwBin = XY_BIN_5X;
    if ( binning == 6 ) resolutionReturn.dwBin = XY_BIN_6X;
    if ( binning == 7 ) resolutionReturn.dwBin = XY_BIN_7X;
    if ( binning == 8 ) resolutionReturn.dwBin = XY_BIN_8X;
    if ( binning == 9 ) resolutionReturn.dwBin = XY_BIN_9X;

    // don't allow negativ offset's to set by the user directly
    if ((resolutionReturn.wOffsetX < 0 ) || (resolutionReturn.wOffsetY < 0 ))
    {
        resolutionReturn.wOffsetX = 0;
        resolutionReturn.wOffsetY = 0;
    } 
    /*
    // This does not seem to fix the binning problem
    else {
       resolutionReturn.wOffsetX /= (i16) binning;
       resolutionReturn.wOffsetY /= (i16) binning;
    }

    resolutionReturn.wSizeX /= (u16) binning;
    resolutionReturn.wSizeY /= (u16) binning;
    */

    // must be at least the same (Skip may be higher than bin but not contrary
    resolutionReturn.dwSkip = resolutionReturn.dwBin;

    // try to keep the current exposure, each time the resolution is changed...
    resolutionReturn.bKeepExposure = (u08) 1;

    // use CamUSB_SetCameraResolution => to update the ROI for Skip and binning
    dwRC = GET_RC(CamUSB_SetCameraResolution( resolutionReturn.wOffsetX,
                                              resolutionReturn.wOffsetY,
                                              resolutionReturn.wSizeX,
                                              resolutionReturn.wSizeY,
                                              resolutionReturn.dwSkip,
                                              resolutionReturn.dwBin,
                                              TRUE,
                                              this->deviceNumber),
                                              this->deviceNumber);

    ///if (!CamUSB_SetFunction( FUNC_RESOLUTION, &resolutionReturn, size ) )
    if ( !IsNoError(dwRC) )
    {
      this->ShowError();
      return DEVICE_ERR;
    }

    this->ResizeImageBuffer();
  }
  else if (eAct == MM::BeforeGet) // property will be read -> update property with value from hardware
  {
    long binning, binningHW;
    pProp->Get(binning);
    binningHW = binning;

    unsigned long size = sizeof(S_RESOLUTION_RETVALS);
    S_RESOLUTION_RETVALS resolutionReturn;

    if (!CamUSB_GetFunction( FUNC_RESOLUTION, &resolutionReturn, &size ) )
    {
      this->ShowError();
      return DEVICE_ERR;
    }

    if ( resolutionReturn.dwBin == XY_BIN_NONE ) binningHW = 1;
    if ( resolutionReturn.dwBin == XY_BIN_2X ) binningHW = 2;
    if ( resolutionReturn.dwBin == XY_BIN_3X ) binningHW = 3;
    if ( resolutionReturn.dwBin == XY_BIN_4X ) binningHW = 4;
    if ( resolutionReturn.dwBin == XY_BIN_5X ) binningHW = 5;
    if ( resolutionReturn.dwBin == XY_BIN_6X ) binningHW = 6;
    if ( resolutionReturn.dwBin == XY_BIN_7X ) binningHW = 7;
    if ( resolutionReturn.dwBin == XY_BIN_8X ) binningHW = 8;
    if ( resolutionReturn.dwBin == XY_BIN_9X ) binningHW = 9;

    pProp->Set( binningHW );
    if ( binning != binningHW ) this->ResizeImageBuffer();
  }

  return DEVICE_OK; 
}

/**
 * Handles "PixelType" property.
 */
int ABSCamera::OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct)
{
  if ( this->pixelTypeCap == NULL ) return DEVICE_OK;

  if (eAct == MM::AfterSet) // property was written -> apply value to hardware
  {
    unsigned long size = sizeof(S_PIXELTYPE_PARAMS);
    S_PIXELTYPE_PARAMS pixelTypePara; 
    std::string pixelType;
    pProp->Get(pixelType);

    for ( u32 n=0; n < CNT_ELEMENTS(g_sSupportedPixelTypes); n++ )
    {
      if ( pixelType.compare( g_sSupportedPixelTypes[n].szPixelTypeName ) == 0 ) 
      {
         pixelTypePara.dwPixelType = g_sSupportedPixelTypes[n].dwPixelType;
         break;
      }
    }

    if (!CamUSB_SetFunction( FUNC_PIXELTYPE, &pixelTypePara, size ) )
    {
        this->ShowError();
        return DEVICE_ERR;
    }

    // setup number of channels depending on current pixel type
    numberOfChannels = 1;
    if ( (GetBpp( pixelTypePara.dwPixelType ) / GetUsedBpp( pixelTypePara.dwPixelType )) > 1 ) 
    {
       numberOfChannels = 4;
    }

    ResizeImageBuffer();
  }
  else if (eAct == MM::BeforeGet) // property will be read -> update property with value from hardware
  {
    std::string pixelType, pixelTypeHW;
    pProp->Get(pixelType);

    // read current pixel type from hardware
    unsigned long pixType;
    if ( !CamUSB_GetPixelType( &pixType, this->deviceNumber ) )
    {
      this->ShowError();
      return DEVICE_ERR;
    }

    // create pixel type value from current pixel type
    for ( u32 n=0; n < CNT_ELEMENTS(g_sSupportedPixelTypes); n++ )
    {
       if( pixType == g_sSupportedPixelTypes[n].dwPixelType )
       {
          pixelTypeHW = g_sSupportedPixelTypes[n].szPixelTypeName;          
          break;
       }
    }

    pProp->Set( pixelTypeHW.c_str() );

    if ( pixelType != pixelTypeHW ) 
    {
        ResizeImageBuffer();
    }
  }

  return DEVICE_OK; 
}

/**
 * Sync internal image buffer size to the chosen property values.
 */
int ABSCamera::ResizeImageBuffer()
{
  unsigned int x, y, sizeX, sizeY;

  // read current ROI
  int nRet = this->GetROI( x, y, sizeX, sizeY );
  if ( nRet != DEVICE_OK ) return nRet;

  // read binning property
  char buf[MM::MaxStrLength];
  int ret = GetProperty(MM::g_Keyword_Binning, buf);
  if (ret != DEVICE_OK) return ret;
  long binSize = atol(buf);

  // read current pixel type from hardware
  unsigned long pixelType;
  if ( !CamUSB_GetPixelType( &pixelType, this->deviceNumber ) )
  {
    this->ShowError();
    return DEVICE_ERR;
  }
  // calculate required bytes
  int byteDepth = GetBpp( pixelType ) / 8;
   
  // resize image buffer
  imageBuffer.Resize( sizeX / binSize, sizeY / binSize, byteDepth);

  return DEVICE_OK;
}

// show ABSCamera error
void ABSCamera::ShowError() const
{
  unsigned long errorNumber = CamUSB_GetLastError( this->deviceNumber ); // read last error code
  this->ShowError( errorNumber );
}

// show ABSCamera error with the spezified number
void ABSCamera::ShowError( unsigned int errorNumber ) const
{
  unsigned int messageType;
  char errorMessage[MAX_PATH]; // error string (MAX_PATH is normally around 255)
  char messageCaption[MAX_PATH];      // caption string

  memset(errorMessage, 0, sizeof(errorMessage));
  memset(messageCaption, 0, sizeof(messageCaption));

  CamUSB_GetErrorString( errorMessage, MAX_PATH, errorNumber );

  if ( IsNoError(errorNumber) )
  {
    sprintf( messageCaption, "Warning for device number %i", this->deviceNumber );
    messageType = MB_OK | MB_ICONWARNING;
  }
  else
  {
    sprintf( messageCaption, "Error for device number %i", this->deviceNumber );
    messageType = MB_OK | MB_ICONERROR;
  }

  ::MessageBox( NULL, errorMessage, messageCaption, messageType );
}

// read camera caps
void* ABSCamera::GetCameraCap( unsigned __int64 CamFuncID ) const
{
  void* cameraCap = NULL;
  if ( ( this->cameraFunctionMask & CamFuncID ) == CamFuncID )
  {
    unsigned long capSize;
    CamUSB_GetFunctionCaps( CamFuncID, NULL ,&capSize );
    cameraCap = new char[capSize];
    if ( CamUSB_GetFunctionCaps( CamFuncID, cameraCap, &capSize, this->deviceNumber ) == false )
    {
      throw new exception();
    }
  }
  return cameraCap;
}

// read camera parameter from hardware
int ABSCamera::GetCameraFunction( unsigned __int64 CamFuncID, void* functionPara, unsigned long size, void* functionParaOut, unsigned long sizeOut) const
{
  if ( ( this->cameraFunctionMask & CamFuncID ) == CamFuncID )
  {
    if ( CamUSB_GetFunction( CamFuncID, functionPara, &size, functionParaOut, sizeOut, NULL, 0, this->deviceNumber ) == false )
    {
      this->ShowError();
      return DEVICE_ERR;
    }
  }
  return DEVICE_OK;
}

// set camera parameter to hardware
int ABSCamera::SetCameraFunction( unsigned __int64 CamFuncID, void* functionPara, unsigned long size )  const
{
  if ( ( this->cameraFunctionMask & CamFuncID ) == CamFuncID )
  {
    if ( CamUSB_SetFunction( CamFuncID, functionPara, size, NULL, NULL, NULL, 0 ) == false )
    {
      this->ShowError();
      return DEVICE_ERR;
    }
  }
  return DEVICE_OK;
}

//int ABSCamera::StartSequenceAcquisition(long /*numImages*/, double /*interval_ms*/, bool /*stopOnOverflow*/)
//{
//  return DEVICE_UNSUPPORTED_COMMAND;
//}
