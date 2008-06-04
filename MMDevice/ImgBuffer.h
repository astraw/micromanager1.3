///////////////////////////////////////////////////////////////////////////////
// MODULE:			ImgBuffer.h
// SYSTEM:        ImageBase subsystem
// AUTHOR:			Nenad Amodaj, nenad@amodaj.com
//
// DESCRIPTION:	Basic implementation for the raw image buffer data structure.
//
// COPYRIGHT:     Nenad Amodaj, 2005. All rigths reserved.
//
// LICENSE:       This file is free for use, modification and distribution and
//                is distributed under terms specified in the BSD license
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// NOTE:          Imported from ADVI for use in Micro-Manager
///////////////////////////////////////////////////////////////////////////////

#if !defined(_IMG_BUFFER_)
#define _IMG_BUFFER_

#include <string>
#include <vector>
#include <map>
#include "MMDevice.h"

///////////////////////////////////////////////////////////////////////////////
//
// CImgBuffer class
// ~~~~~~~~~~~~~~~~~~
// Variable pixel depth image buffer
//

class ImgBuffer
{
public:
   ImgBuffer(unsigned xSize, unsigned ySize, unsigned pixDepth);
   ImgBuffer(const ImgBuffer& ib);
   ImgBuffer();
   ~ImgBuffer();

   unsigned int Width() const {return width_;}
   unsigned int Height() const {return height_;}
   unsigned int Depth() const {return pixDepth_;}
   void SetPixels(const void* pixArray);
   void ResetPixels();
   const unsigned char* GetPixels() const;

   void Resize(unsigned xSize, unsigned ySize, unsigned pixDepth);
   void Resize(unsigned xSize, unsigned ySize);
   bool Compatible(const ImgBuffer& img) const;

   void SetName(const char* name) {name_ = name;}
   const std::string& GetName() {return name_;}
   void SetMetadata(MM::ImageMetadata& md) {metadata_ = md;}
   const MM::ImageMetadata& GetMetadata() const {return metadata_;}

   void Copy(const ImgBuffer& rhs);
   ImgBuffer& operator=(const ImgBuffer& rhs);

private:
   unsigned char* pixels_;
   unsigned int width_;
   unsigned int height_;
   unsigned int pixDepth_;
   std::string name_;
   MM::ImageMetadata metadata_;
};

class FrameBuffer
{
public:
   FrameBuffer();
   FrameBuffer(unsigned xSize, unsigned ySize, unsigned byteDepth);
   ~FrameBuffer();

   void Resize(unsigned xSize, unsigned ySize, unsigned pixDepth);
   void Clear();
   void Preallocate(unsigned channels, unsigned slices);

   bool SetImage(unsigned channel, unsigned slice, const ImgBuffer& img);
   bool GetImage(unsigned channel, unsigned slice, ImgBuffer& img) const;
   ImgBuffer* FindImage(unsigned channel, unsigned slice) const;
   const unsigned char* GetPixels(unsigned channel, unsigned slice) const;
   bool SetPixels(unsigned channel, unsigned slice, const unsigned char* pixels);
   unsigned Width() const {return width_;}
   unsigned Height() const {return height_;}
   unsigned Depth() const {return depth_;}

   void SetID(long id)   {frameID_ = id;}
   long GetID() const      {return frameID_;}
   bool IsHandlePending() const {return handlePending_;}
   void SetHandlePending() {handlePending_ = true;}

private:
   static unsigned long GetIndex(unsigned channel, unsigned slice);
   ImgBuffer* InsertNewImage(unsigned channel, unsigned slice);

   std::vector<ImgBuffer*> images_;
   std::map<unsigned long, ImgBuffer*> indexMap_;
   long frameID_;
   bool handlePending_;
   unsigned int width_;
   unsigned int height_;
   unsigned int depth_;
};


#endif // !defined(_IMG_BUFFER_)
