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

# ifndef _AFUTILS_H_
# define _AFUTILS_H_

# include <string>
# include "../../MMDevice/MMDevice.h"
# include "../../MMDevice/DeviceBase.h"
# include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/ModuleInterface.h"

/////////////////////////////////////////////////////////////
// AFHistogramStretcher:
// ---------------------
// Stretch the histogram based on cthe dynamic range of the 
// pixeltype.
/////////////////////////////////////////////////////////////
template <class PixelType> class AFHistogramStretcher
{
	public:
		AFHistogramStretcher():fContentThreshold(0.001f),fStretchPercent(0.99f)
			,operationmodel_(INPLACE),stretchingmodel_(HISTOGRAMEQUALIZATION){}
		typedef typename PixelType PixelDataType;
		float fContentThreshold;
		float fStretchPercent;
		enum OperationType {INPLACE, OUTOFPLACE};
		enum StretchType   {HISTOGRAMSTETCH, HISTOGRAMEQUALIZATION};
		StretchType stretchingmodel_;
		OperationType operationmodel_;

	public:
		
		int Stretch(PixelDataType * src, int nWidth, int nHeight, PixelDataType * returnimage = 0);
		
};

//////////////////////////////////////////////////////////////////
// Shutter Manager:
// ----------------
// Open Shutter and remember state 
class ShutterManager
{
	public:
		ShutterManager():initialized_(false),core_(0){}
		int OpenCoreShutter();
		int RestoreCoreShutter();
		void SetCore(MM::Core *);

	private:
		std::string shutterName_;
		std::string autoShutterState_;
		std::string shutterState_;
		bool initialized_;
		MM::Core * core_;
};

//////////////////////////////////////////////////////////
// Exposure Manager:
// -----------------
// Manages the exposure for autofocus
///////////////////////////////////////////////////////////

class ExposureManager
{
	public:
		ExposureManager():core_(0),working_(false){}
		void SetCore(MM::Core * );
		int SetExposureToAF(double afExp);
		int RestoreExposure();
		bool IsExposureManagerManagingExposure();

	private:
		double systemExposure_;
		double autofocusExposure_;	
		MM::Core * core_;
		bool working_;

};

class ImageSharpnessScorer
{
public:
	ImageSharpnessScorer();
	~ImageSharpnessScorer();
	void SetImage(ImgBuffer &);
	void MedianFilter(int xsize, int ysize);
	void LaplacianFilter();
	double GetScore();
	double GetScore(ImgBuffer & );
	void SetCore(MM::Core * );
	void SetImage(unsigned char * buffer, int width, int height, int depth);


private:
	ImgBuffer buffer_;	
};
# endif
