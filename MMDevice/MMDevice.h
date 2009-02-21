///////////////////////////////////////////////////////////////////////////////
// FILE:          MMDevice.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMDevice - Device adapter kit
//-----------------------------------------------------------------------------
// DESCRIPTION:   The interface to the Micro-Manager devices. Defines the 
//                plugin API for all devices.
//
// NOTE:          This file is also used in the main control module MMCore.
//                Do not change it undless as a part of the MMCore module
//                revision. Discrepancy between this file and the one used to
//                build MMCore will cause a malfunction and likely a crash too.
// 
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 06/08/2005
//
// COPYRIGHT:     University of California, San Francisco, 2006
//                100X Imaging Inc, 2008
//
// LICENSE:       This file is distributed under the BSD license.
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
// CVS:           $Id$
//

///////////////////////////////////////////////////////////////////////////////
// Header version
// If any of the class declarations changes, the interface version
// must be incremented
#define DEVICE_INTERFACE_VERSION 30
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MMDeviceConstants.h"
#include <string>
#include <cstring>
#include <climits>
#include <cstdlib>
#include <vector>
#include <sstream>

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define snprintf _snprintf 
#endif

#define HDEVMODULE void*

class Metadata;
class ImgBuffer;

namespace MM {

   // forward declaration for the MMCore callback class
   class Core;

   /**
    * Utility class used both MMCore and devices to maintain time intervals
    * in the uniform, platfrom independent way.
    */
   class MMTime
   {
      public:
         MMTime(double uSecTotal = 0.0)
         {
            sec_ = (long) (uSecTotal / 1.0e6);
            uSec_ = (long) (uSecTotal - sec_ * 1.0e6); 
         }

         MMTime(long sec, long uSec) : sec_(sec), uSec_(uSec)
         {
            Normalize();
         }

         ~MMTime() {}

         MMTime(std::string serialized) {
            std::stringstream is(serialized);
            is >> sec_ >> uSec_;
            Normalize();
         }

         std::string serialize() {
            std::ostringstream os;
            os << sec_ << " " << uSec_;
            return os.str().c_str();
         }

         long sec_;
         long uSec_;

         MMTime operator+(const MMTime &other) const
         {
            MMTime res(sec_ + other.sec_, uSec_ + other.uSec_);
            return res;
         }

         MMTime operator-(const MMTime &other) const
         {
            MMTime res(sec_ - other.sec_, uSec_ - other.uSec_);
            return res;
         }

         bool operator>(const MMTime &other) const
         {
            if (sec_ > other.sec_)
               return true;
            else if (sec_ < other.sec_)
               return false;

            if (uSec_ > other.uSec_)
               return true;
            else
               return false;
         }

         bool operator<(const MMTime &other) const
         {
            if (*this == other)
               return false;

            return ! (*this > other);
         }

         bool operator==(const MMTime &other) const
         {
            if (sec_ == other.sec_ && uSec_ == other.uSec_)
               return true;
            else
               return false;
         }

         double getMsec() const
         {
            return sec_ * 1000.0 + uSec_ / 1000.0;
         }

         double getUsec() const
         {
            return sec_ * 1.0e6 + uSec_;
         }

      private:
         void Normalize()
         {
            if (sec_ < 0)
            {
               sec_ = 0L;
               uSec_ = 0L;
               return;
            }

            if (uSec_ < 0)
            {
               sec_--;
               uSec_ = 1000000L + uSec_; 
            }

            long overflow = uSec_ / 1000000L;
            if (overflow > 0)
            {
               sec_ += overflow;
               uSec_ -= overflow * 1000000L;
            }
         }
   };

   struct ImageMetadata
   {
      ImageMetadata() : exposureMs(0.0), ZUm(0.0), score(0.0) {}
      ImageMetadata(MMTime& time, double expMs) : exposureMs(expMs), timestamp(time) {}

      double exposureMs;
      MMTime timestamp;
      double ZUm;
      double score;
   };

   /**
    * Generic device interface.
    */
   class Device {
   public:
      Device() {}
      virtual ~Device() {}
 
      virtual unsigned GetNumberOfProperties() const = 0;
      virtual int GetProperty(const char* name, char* value) const = 0;  
      virtual int SetProperty(const char* name, const char* value) = 0;
      virtual bool HasProperty(const char* name) const = 0;
      virtual bool GetPropertyName(unsigned idx, char* name) const = 0;
      virtual int GetPropertyReadOnly(const char* name, bool& readOnly) const = 0;
      virtual int GetPropertyInitStatus(const char* name, bool& preInit) const = 0;
      virtual int HasPropertyLimits(const char* name, bool& hasLimits) const = 0;
      virtual int GetPropertyLowerLimit(const char* name, double& lowLimit) const = 0;
      virtual int GetPropertyUpperLimit(const char* name, double& hiLimit) const = 0;
      virtual int GetPropertyType(const char* name, MM::PropertyType& pt) const = 0;

      virtual unsigned GetNumberOfPropertyValues(const char* propertyName) const = 0;
      virtual bool GetPropertyValueAt(const char* propertyName, unsigned index, char* value) const = 0;
      virtual bool GetErrorText(int errorCode, char* errMessage) const = 0;
      virtual bool Busy() = 0;
      virtual double GetDelayMs() const = 0;
      virtual void SetDelayMs(double delay) = 0;
      virtual bool UsesDelay() = 0;

      // library handle management (for use only in the client code)
      virtual HDEVMODULE GetModuleHandle() const = 0;
      virtual void SetModuleHandle(HDEVMODULE hLibraryHandle) = 0;
      virtual void SetLabel(const char* label) = 0;
      virtual void GetLabel(char* name) const = 0;
      virtual void SetModuleName(const char* label) = 0;
      virtual void GetModuleName(char* name) const = 0;

      virtual int Initialize() = 0;
      /**
       * Shuts down (unloads) the device.
       * Required by the MM::Device API.
       * Ideally this method will completely unload the device and release all resources.
       * Shutdown() may be called multiple times in a row.
       * After Shutdown() we should be allowed to call Initialize() again to load the device
       * without causing problems.
       */
      virtual int Shutdown() = 0;
   
      virtual DeviceType GetType() const = 0;
      virtual void GetName(char* name) const = 0;
      virtual void SetCallback(Core* callback) = 0;
   };

   /** 
    * Camera API
    */
   class Camera : public Device {
   public:
      Camera() {}
      virtual ~Camera() {}

      DeviceType GetType() const {return Type;}
      static const DeviceType Type = CameraDevice;

      // Camera API
      /**
       * Performs exposure and grabs a single image.
       * Required by the MM::Camera API.
       *
       * SnapImage should start the image exposure in the camera and block until
       * the exposure is finished.  It should not wait for read-out and transfer of data.
       * Return DEVICE_OK on succes, error code otherwise.
       */
      virtual int SnapImage() = 0;
      /**
       * Returns pixel data.
       * Required by the MM::Camera API.
       * GetImageBuffer will be called shortly after SnapImag returns.  
       * Use it to wait for camera read-out and transfer of data into memory
       * Return a pointer to a buffer containing the image data
       * The calling program will assume the size of the buffer based on the values
       * obtained from GetImageBufferSize(), which in turn should be consistent with
       * values returned by GetImageWidth(), GetImageHight() and GetImageBytesPerPixel().
       * The calling program allso assumes that camera never changes the size of
       * the pixel buffer on its own. In other words, the buffer can change only if
       * appropriate properties are set (such as binning, pixel type, etc.)
       *
       */
      virtual const unsigned char* GetImageBuffer() = 0;
      /**
       * Returns pixel data with interleaved RGB pixels in 32 bpp format
       */
      virtual const unsigned int* GetImageBufferAsRGB32() = 0;
      /**
       * Returns the number of channels in this image.  This is '1' for grayscale cameras,
       * and '4' for RGB cameras.
       */
      virtual unsigned GetNumberOfChannels() const = 0;
      /**
       * Returns the name for each channel 
       */
      virtual int GetChannelName(unsigned channel, char* name) = 0;
      /**
       * Returns the size in bytes of the image buffer.
       * Required by the MM::Camera API.
       */
      virtual long GetImageBufferSize()const = 0;
      /**
       * Returns image buffer X-size in pixels.
       * Required by the MM::Camera API.
       */
      virtual unsigned GetImageWidth() const = 0;
      /**
       * Returns image buffer Y-size in pixels.
       * Required by the MM::Camera API.
       */
      virtual unsigned GetImageHeight() const = 0;
      /**
       * Returns image buffer pixel depth in bytes.
       * Required by the MM::Camera API.
       */
      virtual unsigned GetImageBytesPerPixel() const = 0;
      /**
       * Returns the bit depth (dynamic range) of the pixel.
       * This does not affect the buffer size, it just gives the client application
       * a guideline on how to interpret pixel values.
       * Required by the MM::Camera API.
       */
      virtual unsigned GetBitDepth() const = 0;
      /**
       * Returns binnings factor.  Used to calculate current pixelsize
       * Not appropriately named.  Implemented in DeviceBase.h
       */
      virtual double GetPixelSizeUm() const = 0;
      /**
       * Returns the current binning factor.
       */
      virtual int GetBinning() const = 0;
      /**
       * Sets binning factor.
       */
      virtual int SetBinning(int binSize) = 0;
      /**
       * Sets exposure in milliseconds.
       */
      virtual void SetExposure(double exp_ms) = 0;
      /**
       * Returns the current exposure setting in milliseconds.
       */
      virtual double GetExposure() const = 0;
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
      virtual int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize) = 0; 
      /**
       * Returns the actual dimensions of the current ROI.
       */
      virtual int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize) = 0;
      /**
       * Resets the Region of Interest to full frame.
       */
      virtual int ClearROI() = 0;

      /**
       * Starts continuous acquisition.
       */
      virtual int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow) = 0;
      virtual int StartSequenceAcquisition(double interval_ms) = 0;
      virtual int StopSequenceAcquisition() = 0;
      /**
       * Flag to indicate whether Sequence Acquisition is currently running.
       * Return true when Sequence acquisition is activce, false otherwise
       */
      virtual bool IsCapturing() = 0;

   };

   /** 
    * Shutter API
    */
   class Shutter : public Device
   {
   public:
      Shutter() {}
      virtual ~Shutter() {}
   
      // Device API
      virtual DeviceType GetType() const {return Type;}
      static const DeviceType Type = ShutterDevice;
   
      // Shutter API
      virtual int SetOpen(bool open = true) = 0;
      virtual int GetOpen(bool& open) = 0;
      virtual int Fire(double deltaT) = 0;
   };

   /** 
    * Single axis stage API
    */
   class Stage : public Device
   {
   public:
      Stage() {}
      virtual ~Stage() {}
   
      // Device API
      virtual DeviceType GetType() const {return Type;}
      static const DeviceType Type = StageDevice;
   
      // Stage API
      virtual int SetPositionUm(double pos) = 0;
      virtual int SetRelativePositionUm(double d) = 0;
      virtual int Move(double velocity) = 0;
      virtual int SetAdapterOriginUm(double d) = 0;
      virtual int GetPositionUm(double& pos) = 0;
      virtual int SetPositionSteps(long steps) = 0;
      virtual int GetPositionSteps(long& steps) = 0;
      virtual int SetOrigin() = 0;
      virtual int GetLimits(double& lower, double& upper) = 0;
   };

   /** 
    * Dual axis stage API
    */
   class XYStage : public Device
   {
   public:
      XYStage() {}
      virtual ~XYStage() {}

      // Device API
      virtual DeviceType GetType() const {return Type;}
      static const DeviceType Type = XYStageDevice;

      // XYStage API
      virtual int SetPositionUm(double x, double y) = 0;
      virtual int SetRelativePositionUm(double dx, double dy) = 0;
      virtual int SetAdapterOriginUm(double x, double y) = 0;
      virtual int GetPositionUm(double& x, double& y) = 0;
      virtual int GetLimitsUm(double& xMin, double& xMax, double& yMin, double& yMax) = 0;
      virtual int Move(double vx, double vy) = 0;

      virtual int SetPositionSteps(long x, long y) = 0;
      virtual int GetPositionSteps(long& x, long& y) = 0;
      virtual int SetRelativePositionSteps(long x, long y) = 0;
      virtual int Home() = 0;
      virtual int Stop() = 0;
	   virtual int SetOrigin() = 0;//jizhen, 4/12/2007
      virtual int GetStepLimits(long& xMin, long& xMax, long& yMin, long& yMax) = 0;
      virtual double GetStepSizeXUm() = 0;
      virtual double GetStepSizeYUm() = 0;
   };

   /**
    * State device API, e.g. filter wheel, objective turret, etc.
    */
   class State : public Device
   {
   public:
      State() {}
      virtual ~State() {}
      
      // MMDevice API
      virtual DeviceType GetType() const {return Type;}
      static const DeviceType Type = StateDevice;
      
      // MMStateDevice API
      virtual int SetPosition(long pos) = 0;
      virtual int SetPosition(const char* label) = 0;
      virtual int GetPosition(long& pos) const = 0;
      virtual int GetPosition(char* label) const = 0;
      virtual int GetPositionLabel(long pos, char* label) const = 0;
      virtual int GetLabelPosition(const char* label, long& pos) const = 0;
      virtual int SetPositionLabel(long pos, const char* label) = 0;
      virtual unsigned long GetNumberOfPositions() const = 0;
      virtual int SetGateOpen(bool open = true) = 0;
      virtual int GetGateOpen(bool& open) = 0;
   };

   /**
    * Serial port API.
    */
   class Serial : public Device
   {
   public:
      Serial() {}
      virtual ~Serial() {}
      
      // MMDevice API
      virtual DeviceType GetType() const {return Type;}
      static const DeviceType Type = SerialDevice;
      
      // Serial API
      virtual PortType GetPortType() const = 0;
      virtual int SetCommand(const char* command, const char* term) = 0;
      virtual int GetAnswer(char* txt, unsigned maxChars, const char* term) = 0;
      virtual int Write(const unsigned char* buf, unsigned long bufLen) = 0;
      virtual int Read(unsigned char* buf, unsigned long bufLen, unsigned long& charsRead) = 0;
      virtual int Purge() = 0; 
   };

   /**
    * Auto-focus device API.
    */
   class AutoFocus : public Device
   {
   public:
      AutoFocus() {}
      virtual ~AutoFocus() {}
      
      // MMDevice API
      virtual DeviceType GetType() const {return AutoFocusDevice;}
      static const DeviceType Type = AutoFocusDevice;

      // AutoFocus API
      virtual int SetContinuousFocusing(bool state) = 0;
      virtual int GetContinuousFocusing(bool& state) = 0;
      virtual bool IsContinuousFocusLocked() = 0;
      virtual int FullFocus() = 0;
      virtual int IncrementalFocus() = 0;
      virtual int GetLastFocusScore(double& score) = 0;
      virtual int GetCurrentFocusScore(double& score) = 0;
      virtual int AutoSetParameters() = 0;
   };

   /**
    * Streaming API.
    */
   class ImageStreamer : public Device
   {
   public:
      ImageStreamer();
      virtual ~ImageStreamer();

      // MM Device API
      virtual DeviceType GetType() const {return ImageStreamerDevice;}
      static const DeviceType Type = ImageStreamerDevice;

      // image streaming API
      virtual int OpenContext(unsigned width, unsigned height, unsigned depth, const char* path, const Metadata* contextMd = 0);
      virtual int CloseContext();
      virtual int SaveImage(unsigned char* buffer, unsigned width, unsigned height, unsigned depth, const Metadata* imageMd = 0);
   };

   /**
    * Image processor API.
    */
   class ImageProcessor : public Device
   {
      public:
         ImageProcessor() {}
         virtual ~ImageProcessor() {}

      // MMDevice API
      virtual DeviceType GetType() const {return ImageProcessorDevice;}
      static const DeviceType Type = ImageProcessorDevice;

      // image processor API
      virtual int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth) = 0;
   };

   /**
    * ADC and DAC interface.
    */
   class SignalIO : public Device
   {
   public:
      SignalIO() {}
      virtual ~SignalIO() {}

      // MMDevice API
      virtual DeviceType GetType() const {return SignalIODevice;}
      static const DeviceType Type = SignalIODevice;

      // signal io API
      virtual int SetGateOpen(bool open = true) = 0;
      virtual int GetGateOpen(bool& open) = 0;
      virtual int SetSignal(double volts) = 0;
      virtual int GetSignal(double& volts) = 0;
      virtual int GetLimits(double& minVolts, double& maxVolts) = 0;
   };

   /**
   * Devices that can change magnification of the system
   */
   class Magnifier : public Device
   {
   public:
      Magnifier() {}
      virtual ~Magnifier() {}

      // MMDevice API
      virtual DeviceType GetType() const {return MagnifierDevice;}
      static const DeviceType Type = MagnifierDevice;

      virtual double GetMagnification() = 0;
   };


   /**
    * Callback API to the core control module.
    * Devices use this abstract interface to use services of the control client.
    */
   class Core
   {
   public:
      Core() {}
      virtual ~Core() {}

      virtual int LogMessage(const Device* caller, const char* msg, bool debugOnly) const = 0;
      virtual Device* GetDevice(const Device* caller, const char* label) = 0;
      virtual int GetDeviceProperty(const char* deviceName, const char* propName, char* value) = 0;
      virtual int SetDeviceProperty(const char* deviceName, const char* propName, const char* value) = 0;
      virtual std::vector<std::string> GetLoadedDevicesOfType(const Device* caller, MM::DeviceType devType) = 0;
      virtual int SetSerialCommand(const Device* caller, const char* portName, const char* command, const char* term) = 0;
      virtual int GetSerialAnswer(const Device* caller, const char* portName, unsigned long ansLength, char* answer, const char* term) = 0;
      virtual int WriteToSerial(const Device* caller, const char* port, const unsigned char* buf, unsigned long length) = 0;
      virtual int ReadFromSerial(const Device* caller, const char* port, unsigned char* buf, unsigned long length, unsigned long& read) = 0;
      virtual int PurgeSerial(const Device* caller, const char* portName) = 0;
      virtual MM::PortType GetSerialPortType(const char* portName) const = 0;
      virtual int OnStatusChanged(const Device* caller) = 0;
      virtual int OnFinished(const Device* caller) = 0;
      virtual int OnPropertiesChanged(const Device* caller) = 0;
      virtual long GetClockTicksUs(const Device* caller) = 0;
      virtual MM::MMTime GetCurrentMMTime() = 0;

      // continuous acquisition
      virtual int OpenFrame(const Device* caller) = 0;
      virtual int CloseFrame(const Device* caller) = 0;
      virtual int AcqFinished(const Device* caller, int statusCode) = 0;
      virtual int PrepareForAcq(const Device* caller) = 0;
      virtual int InsertImage(const Device* caller, const ImgBuffer& buf) = 0;
      virtual int InsertImage(const Device* caller, const unsigned char* buf, unsigned width, unsigned height, unsigned byteDepth, const Metadata* md = 0) = 0;
      virtual void ClearImageBuffer(const Device* caller) = 0;
      virtual bool InitializeImageBuffer(unsigned channels, unsigned slices, unsigned int w, unsigned int h, unsigned int pixDepth) = 0;
      virtual int InsertMultiChannel(const Device* caller, const unsigned char* buf, unsigned numChannels, unsigned width, unsigned height, unsigned byteDepth, Metadata* md = 0) = 0;
      virtual void SetAcqStatus(const Device* caller, int statusCode) = 0;

      // autofocus
      virtual const char* GetImage() = 0;
      virtual int GetImageDimensions(int& width, int& height, int& depth) = 0;
      virtual int GetFocusPosition(double& pos) = 0;
      virtual int SetFocusPosition(double pos) = 0;
      virtual int MoveFocus(double velocity) = 0;
      virtual int SetXYPosition(double x, double y) = 0;
      virtual int GetXYPosition(double& x, double& y) = 0;
      virtual int MoveXYStage(double vX, double vY) = 0;
      virtual int SetExposure(double expMs) = 0;
      virtual int GetExposure(double& expMs) = 0;
      virtual int SetConfig(const char* group, const char* name) = 0;
      virtual int GetCurrentConfig(const char* group, int bufLen, char* name) = 0;

      // device management
      virtual MM::ImageProcessor* GetImageProcessor(const MM::Device* caller) = 0;
      virtual MM::State* GetStateDevice(const MM::Device* caller, const char* deviceName) = 0;
   
   };

} // namespace MM

//usage: int ret = INVOKE_CALLBACK(AcqFinished(...))
# define INVOKE_CALLBACK(callback) \
   GetCoreCallback()?GetCoreCallback()->callback:DEVICE_OK;

