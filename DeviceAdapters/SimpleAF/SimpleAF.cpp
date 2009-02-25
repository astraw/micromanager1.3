///////////////////////////////////////////////////////////////////////////////
// FILE:          SimpleAF.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Image based autofocus module
//                
// AUTHOR:        Prashanth Ravindran 
//                prashanth@100ximaging.com, February, 2009
//
// COPYRIGHT:     100X Imaging Inc, 2009, http://www.100ximaging.com 
//
// Redistribution and use in source and binary forms, with or without modification, are 
// permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright 
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright 
//       notice, this list of conditions and the following disclaimer in 
//       the documentation and/or other materials provided with the 
//       distribution.
//     * Neither the name of 100X Imaging Inc nor the names of its 
//       contributors may be used to endorse or promote products 
//       derived from this software without specific prior written 
//       permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
// CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define snprintf _snprintf 
#endif

#include "SimpleAF.h"
#include <string>
#include <math.h>
#include "ModuleInterface.h"
#include <sstream>
#include <ctime>

using namespace std;
const char* g_AutoFocusDeviceName = "SimpleAF";

// windows DLL entry code
#ifdef WIN32
   BOOL APIENTRY DllMain( HANDLE /*hModule*/, 
                          DWORD  ul_reason_for_call, 
                          LPVOID /*lpReserved*/
		   			 )
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
   AddAvailableDeviceName(g_AutoFocusDeviceName, "Exaustive search AF - 100XImaging Inc.");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0; 
   
   // decide which device class to create based on the deviceName parameter
   if (strcmp(deviceName, g_AutoFocusDeviceName) == 0)
   {
      // create autoFocus
      return new SimpleAF();
   }

   // ...supplied name not recognized
   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

SimpleAF::SimpleAF():
   busy_(false),initialized_(false)
{
}

void SimpleAF::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_AutoFocusDeviceName);
}

int SimpleAF::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   // set property list
   // -----------------
   
   // Name
   int ret = CreateProperty(MM::g_Keyword_Name, g_AutoFocusDeviceName, MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   // Description
   ret = CreateProperty(MM::g_Keyword_Description, "AF search, 100X Imaging Inc", MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   // Set Exposure
   CPropertyAction *pAct = new CPropertyAction (this, &SimpleAF::OnExposure);
   CreateProperty(MM::g_Keyword_Exposure, "10.0", MM::Float, false, pAct);
  
   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;
   initialized_ = true;
   return DEVICE_OK;
}

int SimpleAF::FullFocus()
{
	return DEVICE_OK;
}

int SimpleAF::IncrementalFocus()
{
	return DEVICE_OK;	
}


// Get the best observed focus score

int SimpleAF::GetLastFocusScore(double & score)
{
   score = 0.0;
	return DEVICE_OK;
}

// Calculate the focus score at a given point
int SimpleAF::GetCurrentFocusScore(double &score)
{
   score = 0.0;
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Properties
///////////////////////////////////////////////////////////////////////////////

int SimpleAF::OnExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
	   double dExposure;
	   pProp->Get(dExposure);

   }
   else if (eAct == MM::BeforeGet)
   {
	   double dExposure;
	   pProp->Get(dExposure);
   }
   return DEVICE_OK; 
}

int SimpleAF::OnStepsizeCoarse(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double StepSizeCoarse = 0.0f;
		pProp->Get(StepSizeCoarse);
		//this->param_stepsize_coarse_ = StepSizeCoarse;

	}
	else if(eAct == MM::BeforeGet)
	{
		double StepSizeCoarse = 0.0f;
		pProp->Get(StepSizeCoarse);
		//this->param_stepsize_coarse_ = StepSizeCoarse;
	}
	return DEVICE_OK;
}

int SimpleAF::OnStepSizeFine(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double StepSizeFine = 0.0f;
		pProp->Get(StepSizeFine);
		//this->param_stepsize_fine_ = StepSizeFine;
	}
	else if(eAct == MM::BeforeGet)
	{
	    double StepSizeFine = 0.0f;
		pProp->Get(StepSizeFine);
		//this->param_stepsize_fine_ = StepSizeFine;
	}
	return DEVICE_OK;
}
