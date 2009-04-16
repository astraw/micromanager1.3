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
#include <cmath>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <ctime>
#include "SimpleAFImageUtils.h"


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
   AddAvailableDeviceName(g_AutoFocusDeviceName, "Exhaustive search AF - 100XImaging Inc.");
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
		busy_(false),initialized_(false),timemeasurement_(false),
			start_(0),stop_(0),timestamp_(0),param_channel_("DAPI")
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

   // Set the depth for coarse search
   pAct = new CPropertyAction(this, &SimpleAF::OnSearchSpanCoarse);
   CreateProperty("Full Search Span","300",MM::Float, false, pAct);

   // Set the depth for fine search
   pAct = new CPropertyAction(this, &SimpleAF::OnSearchSpanFine);
   CreateProperty("Incremental Search Span","100",MM::Float, false, pAct);

   // Set the span for coarse search
   pAct = new CPropertyAction(this, &SimpleAF::OnStepsizeCoarse);
   CreateProperty("Full Search Step","10",MM::Float, false, pAct);

   // Set the span for fine search
   pAct = new CPropertyAction(this, &SimpleAF::OnStepSizeFine);
   CreateProperty("Incremental Search Step","3",MM::Float, false, pAct);

   // Set the channel for autofocus
   pAct = new CPropertyAction(this, &SimpleAF::OnChannelForAutofocus);
   CreateProperty("Channel for autofocus","DAPI",MM::String, false, pAct);

   // Set the threshold for decision making
   pAct = new CPropertyAction(this, &SimpleAF::OnThreshold);
   CreateProperty("Threshold for decision making","0.05",MM::Float, false, pAct);

  
   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;
   initialized_ = true;
   return DEVICE_OK;
}

int SimpleAF::FullFocus()
{
	Focus(FULLFOCUS);
	return DEVICE_OK;
}

int SimpleAF::IncrementalFocus()
{
	Focus(INCREMENTALFOCUS);
	return DEVICE_OK;	
}


// Get the best observed focus score

int SimpleAF::GetLastFocusScore(double & score)
{
	score = score_;
	return DEVICE_OK;
}

// Calculate the focus score at a given point
int SimpleAF::GetCurrentFocusScore(double &score)
{
	GetCoreCallback()->LogMessage(this, "Acquiring image for profiling",false);
	int width = 0, height = 0, depth = 0;
	int ret  = GetCoreCallback()->GetImageDimensions(width, height, depth);
	if(ret != DEVICE_OK)
		return ret;
   score = 0.0;
   // Get the image for analysis
	ImgBuffer image(width,height,depth);
	GetImageForAnalysis(image);
   // score it
	score = GetScore(image);
	std::stringstream mesg;
	mesg<<"Score is "<<score;
	GetCoreCallback()->LogMessage(this,mesg.str().c_str() ,false);
	mesg.str("");
	
   // report
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Properties
///////////////////////////////////////////////////////////////////////////////

int SimpleAF::OnStepsizeCoarse(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double StepSizeCoarse = 0.0f;
		pProp->Get(StepSizeCoarse);
		this->param_stepsize_coarse_ = StepSizeCoarse;

	}
	else if(eAct == MM::BeforeGet)
	{
		double StepSizeCoarse = 0.0f;
		pProp->Get(StepSizeCoarse);
		this->param_stepsize_coarse_ = StepSizeCoarse;
	}
	return DEVICE_OK;
}

int SimpleAF::OnChannelForAutofocus(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		std::string Channel;
		pProp->Get(Channel);
		this->param_channel_ = Channel;

	}
	if(eAct == MM::AfterSet)
	{
		std::string Channel;
		pProp->Get(Channel);
		this->param_channel_ = Channel;

	}
	return DEVICE_OK;
}


int SimpleAF::OnStepSizeFine(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double StepSizeFine = 0.0f;
		pProp->Get(StepSizeFine);
		this->param_stepsize_fine_ = StepSizeFine;
	}
	else if(eAct == MM::BeforeGet)
	{
	    double StepSizeFine = 0.0f;
		pProp->Get(StepSizeFine);
		this->param_stepsize_fine_ = StepSizeFine;
	}


	return DEVICE_OK;
}

int SimpleAF::OnThreshold(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double Threshold = 0.0f;
		pProp->Get(Threshold);
		this->param_decision_threshold_ = Threshold;
	}
	else if(eAct == MM::BeforeGet)
	{
		double Threshold = 0.0f;
		pProp->Get(Threshold);
		this->param_decision_threshold_ = Threshold;
	}


	return DEVICE_OK;
}

int SimpleAF::OnExposure(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double Exposure = 0.0f;
		pProp->Get(Exposure);
		this->param_afexposure_ = Exposure;
	}
	else if(eAct == MM::BeforeGet)
	{
		double Exposure = 0.0f;
		pProp->Get(Exposure);
		this->param_afexposure_ = Exposure;
	}
	return DEVICE_OK;
}

int SimpleAF::OnSearchSpanCoarse(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double CoarseSpan = 0.0f;
		pProp->Get(CoarseSpan);
		this->param_coarse_search_span_ = CoarseSpan;
	}
	else if(eAct == MM::BeforeGet)
	{
		double CoarseSpan = 0.0f;
		pProp->Get(CoarseSpan);
		this->param_coarse_search_span_ = CoarseSpan;
	}
	return DEVICE_OK;
}

int SimpleAF::OnSearchSpanFine(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if(eAct == MM::AfterSet)
	{
		double FineSpan = 0.0f;
		pProp->Get(FineSpan);
		this->param_coarse_search_span_ = FineSpan;
	}
	else if(eAct == MM::BeforeGet)
	{
		double FineSpan = 0.0f;
		pProp->Get(FineSpan);
		this->param_fine_search_span_ = FineSpan;
	}
	return DEVICE_OK;
}
// End of properties

int SimpleAF::Focus(SimpleAF::FocusMode focusmode)
{
	// Set Channel to the required channel
	int ret = GetCoreCallback()->SetConfig("Channel",param_channel_.c_str());
	if(ret != DEVICE_OK)
		return ret;
	// Do full focus
	if(focusmode == FULLFOCUS)
	{
		activespan_ = param_coarse_search_span_;
		activestep_ = param_stepsize_coarse_;

	}
	// Do incremental focus
	if(focusmode == INCREMENTALFOCUS)
	{
		activespan_ = param_fine_search_span_;
		activestep_ = param_stepsize_fine_;
	}

	// Set scope to the way you need it to do AF
	ret = InitScope();
	if(ret != DEVICE_OK)
		return ret;

	// Start looking for object of interest

	double dZHomePos = 0.0f, dzPos = 0.0f,dzTopPos = 0.0,dBestZPos = 0.0;
	GetCoreCallback()->GetFocusPosition(dZHomePos);

	GetCoreCallback()->LogMessage(this, "Started focus method",false);

	// The old value is stored in dzHomePos -- To restore if focus fails
	// dzPos is the variable that always stores the position that you want to go to

	dzPos = dZHomePos - activespan_/2.0f;
	dzTopPos = dZHomePos + activespan_/2.0f;

	// Go to the lowest pos

	ret = GetCoreCallback()->SetFocusPosition(dzPos);
	if(ret != DEVICE_OK)
		return ret;
	bool proceed = true;

	// Get the camera and image parameters
	int width = 0, height = 0, depth = 0;
	ret  = GetCoreCallback()->GetImageDimensions(width, height, depth);
	if(ret != DEVICE_OK)
		return ret;
	int count = 0;
	std::stringstream mesg;
	while(proceed)
	{
		//1. Get an image
		ImgBuffer image(width,height,depth);
		GetImageForAnalysis(image);
		Metadata IMd;
		MetadataSingleTag stgExp(MM::g_Keyword_Exposure, g_AutoFocusDeviceName, true);
		stgExp.SetValue(CDeviceUtils::ConvertToString(param_afexposure_));
		IMd.SetTag(stgExp);

		MetadataSingleTag stgZ(MM::g_Keyword_Metadata_Z, g_AutoFocusDeviceName, true);
		stgZ.SetValue(CDeviceUtils::ConvertToString(dzPos));
		IMd.SetTag(stgZ);
		
		//2. Get its sharness score
		score_ = GetScore(image);

		//3. Do the exit test
		MetadataSingleTag stgScore(MM::g_Keyword_Metadata_Score, g_AutoFocusDeviceName, true);
		stgScore.SetValue(CDeviceUtils::ConvertToString(score_));
		IMd.SetTag(stgScore);
		
		reporter_.InsertCurrentImageInDebugStack(IMd);
		if(score_ >= bestscore_)
		{
			bestscore_ = score_;
			dBestZPos  = dzPos;
		}
		else if (bestscore_ - score_ > param_decision_threshold_*bestscore_ && dBestZPos < 5000) 
		{
			proceed = false;
        }		
		//4. Move the stage for the next run
		dzPos += activestep_;
		if(dzPos < dzTopPos)
		{
			ret = GetCoreCallback()->SetFocusPosition(dzPos);
			if(ret != DEVICE_OK)
				return ret;
		}
		else
		{
			proceed = false;
			// Go to the best pos
			ret = GetCoreCallback()->SetFocusPosition(dBestZPos);
			if(ret != DEVICE_OK)
				return ret;

		}
		++count;

		// Logging messages
		mesg<<"Acquired image "<<(count);
		GetCoreCallback()->LogMessage(this, mesg.str().c_str(),false);
		mesg.str("");
		mesg<<"Current z-pos is :"<<dzPos<<", The span is : "<<activespan_<<", The step is :"<<activestep_;
		GetCoreCallback()->LogMessage(this, mesg.str().c_str(),false);
		mesg.str("");
		mesg<<"Current score is :"<<score_;
		GetCoreCallback()->LogMessage(this, mesg.str().c_str(),false);
		mesg.str("");
		mesg<<"The top point for the search is "<<dzTopPos;
		GetCoreCallback()->LogMessage(this, mesg.str().c_str(),false);
		mesg.str("");
		// End of logging messages
	}
	
	// Restore scope to the old settings
	ret = RestoreScope();
	if(ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;

}

void SimpleAF::StartClock()
{
	timemeasurement_ = true;
	this->timestamp_ = clock();
}

long SimpleAF::GetElapsedTime()
{
	if(timemeasurement_ == false)
	{
		return 0;
	}
	long elapsedtime = clock() - timestamp_;
	timestamp_ = 0;
	timemeasurement_ = false;
	return elapsedtime;
}

int SimpleAF::InitScope()
{
	// Open the shutter
	shutter_.SetCore(GetCoreCallback());
	// Set the exposure
	exposure_.SetCore(GetCoreCallback());
	int ret = shutter_.OpenCoreShutter();
	if(ret != DEVICE_OK)
		return ret;
	ret = exposure_.SetExposureToAF(param_afexposure_);
	reporter_.SetCore(GetCoreCallback());
	reporter_.InitializeDebugStack(this);
	if(ret != DEVICE_OK)
		return ret;
	return DEVICE_OK;
}

int SimpleAF::RestoreScope()
{
	// Retore the core shutter
	int ret  = shutter_.RestoreCoreShutter();
	if(ret != DEVICE_OK)
		return ret;
	// Restore the core exposure
	ret = exposure_.RestoreExposure();
	if(ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;
}

int SimpleAF::GetImageForAnalysis(ImgBuffer & buffer, bool stretch )
{
	// Do unsigned char related stuff
	unsigned char * pBuf = const_cast<unsigned char *>(buffer.GetPixels());
	if(buffer.Depth() == 1)
	{
		void * sourcepixel = malloc(buffer.Depth());
		char * imageBuf = const_cast<char *>(GetCoreCallback()->GetImage());
		for(unsigned long j = 0; j < buffer.Width()*buffer.Height() ; ++j )
		{
			if(memcpy(sourcepixel,(void *)(imageBuf + j),buffer.Depth()))
			{
				unsigned char val = *(static_cast<unsigned char *>(sourcepixel));
				*(pBuf + j) = val;
			}
		}
		// Stretch the image			
		AFHistogramStretcher<unsigned char> stretcher;
		stretcher.fStretchPercent = 0.95f;		// Reject top 5% of pixels that is mostly shot noise
		stretcher.fContentThreshold = 0.05f;		// If dynamic range < 5% reject that image
		stretcher.operationmodel_ = AFHistogramStretcher<unsigned char>::INPLACE;
		stretcher.stretchingmodel_ = AFHistogramStretcher<unsigned char>::HISTOGRAMSTETCH;
		int ret = stretcher.Stretch(pBuf,buffer.Width(),buffer.Height());
		free(sourcepixel); sourcepixel = 0;
		return DEVICE_OK;
	}
	// Do unsigned short related stuff here
	else
	if(buffer.Depth() == 2)
	{
		// Getting a handle to the pixels
		unsigned char * pBuf = const_cast<unsigned char *>(buffer.GetPixels());
		// In the case of U-Short we are in slightly tricky territiry
		// We need to create another array for stretching, that has to be stretched as unsigned short, before
		// we can cast it as a char, otherwise the bit shift operation will ensure that the dynamic range is 
		// pathetic. This also means that we need to clean up in this function itself

		// 1. Create a ushort Array here
		unsigned short * StretchedImage = new unsigned short [buffer.Width()*buffer.Height()];

		// 2. Create a ushort pointer that points to the camera's image buffer

		unsigned short * ImagePointer = static_cast<unsigned short *>(
											static_cast<void *>(
												const_cast<char*>(GetCoreCallback()->GetImage())));
		// 3. Stretch the image

		AFHistogramStretcher<unsigned short> stretcher;
		stretcher.fStretchPercent = 0.95f;		// Reject top 5% of pixels that is mostly shot noise
		stretcher.fContentThreshold = 0.05f;		// If dynamic range < 5% reject that image
		stretcher.operationmodel_ = AFHistogramStretcher<unsigned short>::OUTOFPLACE;
		stretcher.stretchingmodel_ = AFHistogramStretcher<unsigned short>::HISTOGRAMSTETCH;
		int ret = stretcher.Stretch(ImagePointer,buffer.Width(),buffer.Height(),StretchedImage);

		// 4. Cast the stretched image back into u-char for processing	

		for(unsigned long i = 0; i < buffer.Width()*buffer.Height(); ++i)
		{
			*(pBuf + i) = 
				static_cast<unsigned char>(StretchedImage[i]>>((buffer.Depth() -sizeof(unsigned char))*8));
		}
	    
		// Delete the ushort array that you declared. 
		delete [] StretchedImage; StretchedImage = 0;
		// Store a native copy in the image handle		
		return DEVICE_OK;
	}
	else
	{
		return DEVICE_UNSUPPORTED_DATA_FORMAT;
	}
	return DEVICE_OK;
}

double SimpleAF::GetScore(ImgBuffer & buffer)
{
	return scorer_.GetScore(buffer);
}
