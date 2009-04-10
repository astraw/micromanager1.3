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
# include "MMDevice.h"
# include "DeviceBase.h"
# include "ImgBuffer.h"
#include "ModuleInterface.h"

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
		ShutterManager():initialized_(false){}
		int OpenCoreShutter(MM::Core *);
		int RestoreCoreShutter(MM::Core *);

	private:
		std::string shutterName_;
		std::string autoShutterState_;
		std::string shutterState_;
		bool initialized_;
};


///////////////////////////////////////////////////////////
// Image Sharpness computations
//
////////////////////////////////////////////////////////////

class ImageScorer
{
public :
	ImageScorer(): 
	  m_nThreshold(25),
	  m_bNonMax(false),
	  m_imagepointer(0),
	  m_nTotalCorners(-1)
	  {}
	~ImageScorer()
	{
	}
	
	void SetUseNonMaxOn()
	{
		m_bNonMax = true;
	}
	void SetImage(unsigned char * imagepointer, int nWidth, int nHeight)
	{
		m_imagepointer = imagepointer;
		m_nImageWidth = nWidth;
		m_nImageHeight = nHeight;
	}

	int Score();

	void SetThreshold(int nThreshold){m_nThreshold = nThreshold;}

private:
	int m_nImageWidth;
	int m_nImageHeight;
	int m_nThreshold;
	unsigned char * m_imagepointer;
	bool m_bNonMax;
	long int m_nTotalCorners;
};
# endif