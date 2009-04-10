///////////////////////////////////////////////////////////////////////////////
// FILE:          SimpleAFImageUtils.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Image based autofocus module - Utilities
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


# include "SimpleAFImageUtils.h"
// For numeric limits, to work with the different pixel types
# include <limits>



# ifdef max
# undef max
# endif
# ifdef min
# undef min
# endif

template <class PixelType>
int AFHistogramStretcher<PixelType>::
Stretch(PixelDataType *src, int nWidth, int nHeight, PixelDataType *returnimage = 0)
{
	double * histogram = new double[std::numeric_limits<PixelDataType>::max()+1];
	// Get the max and the minimum

		PixelDataType val_max = std::numeric_limits<PixelDataType>::min(), 
			      val_min = std::numeric_limits<PixelDataType>::max(),
				  typemax = val_min,
				  typemin = val_max;
	// Getting min and max in one pass
	for(long i = 0; i < nWidth * nHeight; ++i)
	{
		if(src[i] > val_max)
			val_max = src[i];
		if(src[i] < val_min)
			val_min = src[i]; 
		++histogram[src[i]];

	}



	// If the image has very low dynamic range.. do nothing, 
	// you might just be amplifying the noise

	if(((float)abs(val_min - val_max)/(float)typemax) < fContentThreshold)
	{
		if(operationmodel_ == OUTOFPLACE)
		{
			memcpy(returnimage,src,nWidth*nHeight*sizeof(PixelType));	
			delete [] histogram;
			histogram = 0;
		}
		return 0;
	}


	if(stretchingmodel_ == HISTOGRAMSTETCH)
	{

		if(operationmodel_ == INPLACE)
		{
			float fFactor = ((float)typemax)/((float)(val_max-val_min));
			// Setting the scaling again
			for(long i = 0; i < nWidth * nHeight; ++i)
			{					
				src[i] = static_cast<PixelDataType>((fFactor)*(src[i] - val_min));
			}
		}
		else if(operationmodel_ == OUTOFPLACE)
		{
			float fFactor = ((float)typemax)/((float)(val_max-val_min));
			// Setting the scaling again
			for(long i = 0; i < nWidth * nHeight; ++i)
			{					
				returnimage[i] = static_cast<PixelDataType>((fFactor)*(src[i] - val_min));
			}
		}
		return 1;
	}
	else
	if(stretchingmodel_ == HISTOGRAMEQUALIZATION)
	{
		// Come in from the end and identify the cutoff point of the 
		// histogram, which ensures that all hot pixels have been chucked out
		// Also the cdf (cumulative distribution function) is generated in the same pass


		long incidence  = 0;
		long thresh = (long)((float)nWidth*(float)nHeight*(fStretchPercent));
		long uppercutoff = 0;

		double * cdf = new double [std::numeric_limits<PixelDataType>::max()];

		for(long i = 0; i < std::numeric_limits<PixelDataType>::max(); ++i)
		{
			incidence += (long)histogram[i];
			if(incidence > thresh && uppercutoff == 0)
			{
				uppercutoff = i;						
			}
			// CDF is generated here
			if(i > 0)
				cdf[i] = histogram[i] + cdf[i-1];         // For the later indices
			else
				cdf[i] = histogram[i];			          // For the first index
		}
		for(long i = 0; i < nWidth * nHeight; ++i)
		{
			if(operationmodel_ == INPLACE)
			{
				src[i] = static_cast<PixelType>(cdf[src[i]]);
			}
			else if(operationmodel_ == OUTOFPLACE)
			{
				returnimage[i] = static_cast<PixelType>(cdf[src[i]]);
				
			}
		}
		delete[] cdf;
		cdf = 0;
		if(histogram != 0)
		{
			delete[] histogram; 
			histogram = 0;
		}

					
		return 1;
	}
	return 1;
}

////////////////////////////////////////////////////////////////
// Shutter Manager class:
// ----------------------
// Open shutter and close shutter, while setting auto-shutter
// to what it was. Designed such that it can be stateless i.e
// the callee does not need to know the state of the shutter to
// call it
/////////////////////////////////////////////////////////////////

int ShutterManager::OpenCoreShutter(MM::Core * core)
{

   // first get the current state of the auto-shutter and shutter
   char * value = new char[MM::MaxStrLength];
   core->GetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoShutter, value);
   autoShutterState_ = value;;

   // then get the current state of the shutter
   core->GetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreShutter, value);
   shutterName_ = value;;
   core->GetDeviceProperty(shutterName_.c_str(), MM::g_Keyword_State, value);
   shutterState_ = value;;

   // now we can open the shutter
   core->SetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoShutter, "0"); // disable auto-shutter
   core->SetDeviceProperty(shutterName_.c_str(), MM::g_Keyword_State, "1"); // open shutter

   initialized_ = true;

   delete [] value; value = 0;

   return DEVICE_OK;
}

/////////////////////////////////////////////////////////////////////////
// Restore Shutter Settings to what they were before
//
//////////////////////////////////////////////////////////////////////////

int ShutterManager::RestoreCoreShutter(MM::Core * core)
{
	if(!initialized_)
		return DEVICE_ERR;
	// restore shutter and auto-shutter settings
	// TODO: make sure to restore this in case there is an error and the flow never reaches this point
	core->SetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoShutter, autoShutterState_.c_str());
	core->SetDeviceProperty(shutterName_.c_str(), MM::g_Keyword_State, shutterState_.c_str()); // open/close shutter

	// Now state is not remembered, so set it back
	initialized_ = false;

	return DEVICE_OK;
}


void ImageScorer::ImageScorer():score_(-1.0f)
{
	kernel_
}

void ImageScorer::Score(ImgBuffer &img)
{
	if (img.Height() == 0 || img.Width() == 0 || img.Depth() == 0)
		return;
	
}



