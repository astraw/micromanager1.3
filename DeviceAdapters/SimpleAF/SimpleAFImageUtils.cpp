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

# include <vector>
# include <cmath>



# ifdef max
# undef max
# endif
# ifdef min
# undef min
# endif

//template <typename PixelDataType>
//int AFHistogramStretcher<PixelDataType>::
//Stretch(PixelDataType *src, int nWidth, int nHeight, PixelDataType *returnimage = 0)
// NOTE: Implemented in the .h see the note there

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
					  accumulation += k * kernel[kindex];
					  weightsum += kernel[kindex];
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
			std::vector<double> kernelmask_(xsize * ysize);
			for(int i = 0; i < xsize*ysize; ++i)
				kernelmask_[i] = 0;
			// For getting the central array
			int count = 0;
			for(int  y_off = -ysize/2 ; y_off <= ysize/2; ++y_off)
			{
				for(int  x_off = -xsize/2 ; x_off <= xsize/2; ++x_off)
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
				//delete [] kernelmask_; kernelmask_ = 0;

			}
			else
			// if even - set the average of the two central values
			{
				int index = (int)((float)(xsize * ysize )/2.0f);
				unsigned char medianvalue_ = (unsigned char)((float)(kernelmask_[(int)(index - 0.5f)] + kernelmask_[(int)(index + 0.5f)])/2.0f);
				long setIndex = buffer_.Width()*j + k;
				*(pBuf + setIndex) = medianvalue_;
				//delete [] kernelmask_; kernelmask_ = 0;
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

	score/= (buffer_.Width() * buffer_.Height());

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

/////////////////////////////////
// Reporting Manager
/////////////////////////////////

void ReportingManager::SetCore(MM::Core * core)
{
	core_ = core;
}

int ReportingManager::InitializeDebugStack(MM::Device * callee)
{
	if(core_ == 0 || callee == 0)
	{
		return DEVICE_ERR;
	}
	
	int ret = core_->GetImageDimensions(width_,height_,depth_);

	if(ret != DEVICE_OK)
	{
		return ret;
	}

	if(!core_->InitializeImageBuffer(1,1,width_,height_,depth_))
	{
		return DEVICE_ERR;
	}

	bufferinitialized_ = true;
	callee_ = callee;

	
	return DEVICE_OK;
}

int ReportingManager::InsertCurrentImageInDebugStack(Metadata &IMd)
{
	if(bufferinitialized_ == false || core_ == 0 || callee_ == 0)
	{
		return DEVICE_ERR;
	}

	// Get the image buffer

	const unsigned char * iBuf = const_cast<unsigned char *>((unsigned char *)core_->GetImage()); 

	int ret  = core_->InsertImage(callee_,iBuf,width_,height_,depth_,&IMd);
	if(ret != DEVICE_OK)
	{
		return ret;
	}

	return DEVICE_OK;
}