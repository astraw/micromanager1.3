///////////////////////////////////////////////////////////////////////////////
// FILE:          CircularBuffer.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMCore
//-----------------------------------------------------------------------------
// DESCRIPTION:   Generic implementation of the circular buffer. The buffer
//                allows only one thread to enter at a time by using a mutex lock.
//                This makes the buffer succeptible to race conditions if the
//                calling threads are mutally dependent.
//              
// COPYRIGHT:     University of California, San Francisco, 2007,
//
// LICENSE:       This file is distributed under the "Lesser GPL" (LGPL) license.
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
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 01/05/2007
// 
// CVS:           $Id: PluginManager.h 2 2007-02-27 23:33:17Z nenad $
//
#include "CircularBuffer.h"
#include "CoreUtils.h"

#ifdef WIN32
#pragma warning (disable : 4312 4244)
#endif

#include <ace/Mutex.h>
#include <ace/Guard_T.h>
#include "ace/High_Res_Timer.h"

#ifdef WIN32
#pragma warning (default : 4312 4244)
#endif

#ifdef WIN32
#undef min // avoid clash with the system defined macros
#endif

const int bytesInMB = 1048576;
const long adjustThreshold = LONG_MAX / 2;

// mutex
static ACE_Mutex g_bufferLock;

CircularBuffer::CircularBuffer(unsigned int memorySizeMB) :
   width_(0), height_(0), pixDepth_(0), insertIndex_(0), saveIndex_(0), memorySizeMB_(memorySizeMB), overflow_(false), estimatedIntervalMs_(0)
{
}

CircularBuffer::~CircularBuffer() {}

bool CircularBuffer::Initialize(unsigned channels, unsigned slices, unsigned int w, unsigned int h, unsigned int pixDepth)
{
   if (w == 0 || h==0 || pixDepth == 0 || channels == 0 || slices == 0)
      return false; // does not make sense

   if (w == width_ && height_ == h && pixDepth_ == pixDepth && channels == numChannels_ && slices == numSlices_)
      return true; // nothing to change

   width_ = w;
   height_ = h;
   pixDepth_ = pixDepth;
   numChannels_ = channels;
   numSlices_ = slices;

   insertIndex_ = 0;
   saveIndex_ = 0;
   overflow_ = false;

   // calculate the size of the entire buffer array once all images get allocated
   // the acutual size at the time of the creation is going to be less, because
   // images are not allocated until pixels become available
   unsigned long frameSizeBytes = width_ * height_ * pixDepth_ * numChannels_ * numSlices_;
   unsigned long cbSize = (memorySizeMB_ * bytesInMB) / frameSizeBytes;

   if (cbSize == 0)
      return false; // memory footprint too small

   // TODO: verify if we have enough RAM to satisfy this request   

   // allocate buffers
   frameArray_.resize(cbSize);
   for (unsigned long i=0; i<frameArray_.size(); i++)
   {
      frameArray_[i].Resize(w, h, pixDepth);
      frameArray_[i].Preallocate(1, 1);
   }

   return true;
}

unsigned long CircularBuffer::GetSize() const
{
   return (unsigned long)frameArray_.size();
}

unsigned long CircularBuffer::GetFreeSize() const
{
   //ACE_Guard<ACE_Mutex> guard(g_bufferLock);

   long freeSize = (long)frameArray_.size() - (insertIndex_ - saveIndex_);
   if (freeSize < 0)
      return 0;
   else
      return (unsigned long)freeSize;
}

unsigned long CircularBuffer::GetRemainingImageCount() const
{
   //ACE_Guard<ACE_Mutex> guard(g_bufferLock);
   return (unsigned long)(insertIndex_ - saveIndex_);
}

/**
 * Inserts a single image in the buffer.
 */
bool CircularBuffer::InsertImage(const unsigned char* pixArray, unsigned int width, unsigned int height, unsigned int byteDepth, MM::ImageMetadata* pMd)
{
   return InsertMultiChannel(pixArray, 1, width, height, byteDepth, pMd);
}

/**
 * Inserts a multi-channel frame in the buffer.
 */
bool CircularBuffer::InsertMultiChannel(const unsigned char* pixArray, unsigned numChannels, unsigned width, unsigned height, unsigned byteDepth, MM::ImageMetadata* pMd)
{
   ACE_Guard<ACE_Mutex> guard(g_bufferLock);

   unsigned long singleChannelSize = (unsigned long)width * height * byteDepth;
   static unsigned long previousTicks = 0;

   if (previousTicks > 0)
      estimatedIntervalMs_ = GetClockTicksMs() - previousTicks;
   else
      estimatedIntervalMs_ = 0;

   // check image dimensions
   if (width != width_ || height_ != height || byteDepth != byteDepth)
      return false; // incompatible size

   
   if ((long)frameArray_.size() - (insertIndex_ - saveIndex_) > 0)
   {
      for (unsigned i=0; i<numChannels; i++)
      {
         // check if the requested (channel, slice) combination exists
         // we assume that all buffers are pre-allocated
         ImgBuffer* pImg = frameArray_[insertIndex_ % frameArray_.size()].FindImage(i, 0);
         if (!pImg)
            return false;

         if (pMd)
            pImg->SetMetadata(*pMd);
         else
         {
            // if metadata was not supplied by the camera insert current timestamp
            MM::MMTime timestamp = GetMMTimeNow();
            MM::ImageMetadata md(timestamp, 0.0);
            pImg->SetMetadata(md);
         }
         pImg->SetPixels(pixArray + i*singleChannelSize);
      }

      insertIndex_++;
      if ((insertIndex_ - (long)frameArray_.size()) > adjustThreshold && (saveIndex_- (long)frameArray_.size()) > adjustThreshold)
      {
         printf("adjusting indices\n");
         // adjust buffer indices to avoid overflowing integer size
         insertIndex_ -= adjustThreshold;
         saveIndex_ -= adjustThreshold;
      }
      
      previousTicks = GetClockTicksMs();

      return true;
   }

   // buffer overflow
   overflow_ = true;
   return false;
}


const unsigned char* CircularBuffer::GetTopImage() const
{
//   printf("Entered CB GetTopImage\n");
   ACE_Guard<ACE_Mutex> guard(g_bufferLock);

   if (frameArray_.size() == 0)
      return 0;

   if (insertIndex_ == 0)
      return frameArray_[0].GetPixels(0, 0);
   else
      return frameArray_[(insertIndex_-1) % frameArray_.size()].GetPixels(0, 0);
}

const ImgBuffer* CircularBuffer::GetTopImageBuffer(unsigned channel, unsigned slice) const
{
   ACE_Guard<ACE_Mutex> guard(g_bufferLock);

   if (frameArray_.size() == 0)
      return 0;

   // TODO: we may return NULL pointer if channel and slice indexes are wrong
   // this will cause problem in the SWIG - Java layer
   if (insertIndex_ == 0)
      return frameArray_[0].FindImage(channel, slice);
   else
      return frameArray_[(insertIndex_-1) % frameArray_.size()].FindImage(channel, slice);
}


const unsigned char* CircularBuffer::GetNextImage()
{
   ACE_Guard<ACE_Mutex> guard(g_bufferLock);

   if (saveIndex_ < insertIndex_)
   {
      const unsigned char* pBuf = frameArray_[(saveIndex_) % frameArray_.size()].GetPixels(0, 0);
      saveIndex_++;
      return pBuf;
   }
   return 0;
}

const ImgBuffer* CircularBuffer::GetNextImageBuffer(unsigned channel, unsigned slice)
{
   ACE_Guard<ACE_Mutex> guard(g_bufferLock);

   // TODO: we may return NULL pointer if channel and slice indexes are wrong
   // this will cause problem in the SWIG - Java layer
   if (saveIndex_ < insertIndex_)
   {
      const ImgBuffer* pBuf = frameArray_[(saveIndex_) % frameArray_.size()].FindImage(channel, slice);
      saveIndex_++;
      return pBuf;
   }
   return 0;
}

double CircularBuffer::GetAverageIntervalMs() const
{
   ACE_Guard<ACE_Mutex> guard(g_bufferLock);

   // TODO: below is not working properly
   //const unsigned avgSize = 10;

   //if (insertIndex_ < 2 || imgArray_.size() < avgSize+2)
   //   return 0.0;

   //double sum = 0.0;
   //int count = 0;
   //for (unsigned i=1; i<=avgSize; i++)
   //{
   //   sum += ((imgArray_[(insertIndex_-i) % imgArray_.size()].elapsedTimeMs - imgArray_[(insertIndex_-i-1) % imgArray_.size()].elapsedTimeMs));
   //   count++;
   //}

   //if (count > 0)
   //   sum /= count;

   //return (double)((long)(sum + 0.5));
   return (double)estimatedIntervalMs_;
}

unsigned long CircularBuffer::GetClockTicksMs() const
{
#ifdef __APPLE__
      struct timeval t;
      gettimeofday(&t,NULL);
      return t.tv_sec * 1000L + t.tv_usec * 1000L;
#else
      ACE_High_Res_Timer timer;
      ACE_Time_Value t = timer.gettimeofday();
      return (unsigned long) (t.sec() * 1000L + t.usec() / 1000L);
#endif
}
