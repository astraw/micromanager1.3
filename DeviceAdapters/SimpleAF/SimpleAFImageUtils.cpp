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

template <typename PixelDataType>
int AFHistogramStretcher<PixelDataType>::
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

void ShutterManager::SetCore(MM::Core * core)
{
	core_ = core;
}

int ShutterManager::OpenCoreShutter()
{
	if(core_ == 0)
		return DEVICE_ERR;

   // first get the current state of the auto-shutter and shutter
   char * value = new char[MM::MaxStrLength];
   core_->GetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoShutter, value);
   autoShutterState_ = value;;

   // then get the current state of the shutter
   core_->GetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreShutter, value);
   shutterName_ = value;;
   core_->GetDeviceProperty(shutterName_.c_str(), MM::g_Keyword_State, value);
   shutterState_ = value;;

   // now we can open the shutter
   core_->SetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoShutter, "0"); // disable auto-shutter
   core_->SetDeviceProperty(shutterName_.c_str(), MM::g_Keyword_State, "1"); // open shutter

   initialized_ = true;

   delete [] value; value = 0;

   return DEVICE_OK;
}

/////////////////////////////////////////////////////////////////////////
// Restore Shutter Settings to what they were before
//
//////////////////////////////////////////////////////////////////////////

int ShutterManager::RestoreCoreShutter()
{
	if(!initialized_ || core_ == 0)
		return DEVICE_ERR;
	// restore shutter and auto-shutter settings
	// TODO: make sure to restore this in case there is an error and the flow never reaches this point
	core_->SetDeviceProperty(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoShutter, autoShutterState_.c_str());
	core_->SetDeviceProperty(shutterName_.c_str(), MM::g_Keyword_State, shutterState_.c_str()); // open/close shutter

	// Now state is not remembered, so set it back
	initialized_ = false;
	shutterName_ = "";
	autoShutterState_ = "";
	shutterState_ = "";

	return DEVICE_OK;
}


//
// Image sharpness scorer
//
ImageSharpnessScorer::ImageSharpnessScorer()
{
	
}

ImageSharpnessScorer::~ImageSharpnessScorer()
{

}


void ImageSharpnessScorer::LaplacianFilter()
{
	int xwidth = 3, ywidth = 3;
	long index = 0;
	ImgBuffer convimage_(buffer_.Width(), buffer_.Height(),1);
	unsigned char * poutbuf = const_cast<unsigned char *>(convimage_.GetPixels());
	double kernel[9] = {2.0, 1.0, 0.0, 1.0, 0.0, -1.0, 0.0, -1.0, -2.0};
	// Looping over the image
	for(unsigned int j = 1; j < buffer_.Width() -1 ; ++j)
	{
		for(unsigned int i = 1; i < buffer_.Height() - 1; ++i)
		{
			  double accumulation = 0;
			  double weightsum = 0;
			  // Looping over the kernel
			  int kindex = 0;
			  for(int ii = 0; ii < 3; ++ii)
			  {
				  for(int jj = 0; jj < 3; ++jj)
				  {
					  index = j*buffer_.Width() + j;
					  kindex = jj*3 + i;
					  unsigned char k = *(buffer_.GetPixels() + index);
					  accumulation += k * (*(kernel + kindex));
					  weightsum += *(kernel + kindex);
				  }
			  }
			  *(poutbuf + index) = (unsigned char)(accumulation/weightsum);
		}
	}
	buffer_ = convimage_;	
}

void ImageSharpnessScorer::MedianFilter(int xsize, int ysize)
{
	ImgBuffer filtimage_(buffer_);
	// Navigating through the buffer
	for (unsigned int j=0; j<buffer_.Height(); j++)
	{
		for (unsigned int k=0; k< buffer_.Width(); k++)
		{
			double * kernelmask_ = new double[xsize*ysize];
			for(int i = 0; i < xsize*ysize; ++i)
				kernelmask_[i] = 0;
			// For getting the central array
			int count = 0;
			for(int  y_off = -ysize/2 ; y_off < ysize; ++y_off)
			{
				for(int  x_off = -xsize/2 ; x_off < xsize; ++x_off)
				{
					long lIndex = buffer_.Width()*(j + y_off) + (k + x_off);
					if(lIndex >= 0 && 
						lIndex < (long)(buffer_.Width()*buffer_.Height())&&
						(j + y_off) >= 0 && (k + x_off) >= 0
						)
					{
						kernelmask_[count] = buffer_.GetPixels()[lIndex];						
					}
					++count;
				}
			}
			// sort the kernel mask array

			double min = kernelmask_[0];
			double temp = kernelmask_[0];
			for(int i = 0; i < (ysize * xsize) - 1; ++i)
			{
				for(int j = i; j < xsize*ysize  ; ++j)
				{
					if(kernelmask_[j] < min)
					{
						min = kernelmask_[j];
						temp = kernelmask_[i];
						kernelmask_[i] = kernelmask_[j];
						kernelmask_[i+1] = temp;
					}
				}
			}
	
			// Get the median
			// if odd - set the central value

			unsigned char * pBuf = const_cast<unsigned char *>(filtimage_.GetPixels());
			if((xsize * ysize) % 2 == 1)
			{
				int index = (int)((float)(xsize * ysize )/2.0f);
				unsigned char medianvalue_ = (unsigned char)kernelmask_[index];
				long setIndex = buffer_.Width()*j + k;
				*(pBuf + setIndex) = medianvalue_;
				delete [] kernelmask_; kernelmask_ = 0;

			}
			else
			// if even - set the average of the two central values
			{
				int index = (int)((float)(xsize * ysize )/2.0f);
				unsigned char medianvalue_ = (unsigned char)((float)(kernelmask_[(int)(index - 0.5f)] + kernelmask_[(int)(index + 0.5f)])/2.0f);
				long setIndex = buffer_.Width()*j + k;
				*(pBuf + setIndex) = medianvalue_;
				delete [] kernelmask_; kernelmask_ = 0;
			}

			// delete the buffer
			
		}
	}
	buffer_ = filtimage_;	
}

double ImageSharpnessScorer::GetScore()
{
  // 1. Get the median filtering done
	this->MedianFilter(3,3);
  // 2. Run the laplacian filter
	this->LaplacianFilter();
  // 3. Generate the score
	// Touch all the pixels and sum to get the sharpness score

	double score = 0.0f;
	long index = 0;
	for(unsigned int j = 1; j < buffer_.Height() - 1; ++j)
	{
		for(unsigned int i = 0; i < buffer_.Width() - 1; ++i)
		{
			index = j*buffer_.Width() + i;
			score += pow((float)*(buffer_.GetPixels() + index),2.0f);
		}
	}

	return score;
}

double ImageSharpnessScorer::GetScore(ImgBuffer & buffer)
{
	buffer_.Copy(buffer);
	return GetScore();

}

void ImageSharpnessScorer::SetImage(ImgBuffer & inputbuffer)
{
	buffer_.Copy(inputbuffer);
}

void ImageSharpnessScorer::SetImage(unsigned char * buffer, int width, int height, int depth)
{
	ImgBuffer newbuffer(width,height,depth);
	newbuffer.SetPixels(buffer);
	buffer_ = newbuffer;
}


////////////////////////////////////////////////////////////
// Exposure Manager
//


void ExposureManager::SetCore(MM::Core * core)
{
	core_ = core;	
}

bool ExposureManager::IsExposureManagerManagingExposure()
{
	return working_;
}

int ExposureManager::SetExposureToAF(double afExp)
{
	if(afExp <= 0.0f || core_ == 0)
		return DEVICE_ERR;
	autofocusExposure_ = afExp;
	// 1. Get the current exposure and persist it
	
	int ret = core_->GetExposure(systemExposure_);
	if(ret != DEVICE_OK)
		return ret;
	// 2. Set the expsoure to the requested AF exposure
	ret = core_->SetExposure(autofocusExposure_);
	if (ret != DEVICE_OK)		 
		return ret;
	// 3. Set the state flag to true
	working_ = true;

	return DEVICE_OK;
}

int ExposureManager::RestoreExposure()
{
	int ret = core_->SetExposure(systemExposure_);
	if(ret != DEVICE_OK)
		return ret;
	working_ = false;

	return DEVICE_OK;
}