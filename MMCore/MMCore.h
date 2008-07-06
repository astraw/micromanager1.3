///////////////////////////////////////////////////////////////////////////////
// FILE:          MMCore.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMCore
//-----------------------------------------------------------------------------
// DESCRIPTION:   The interface to the MM core services. 
//              
// COPYRIGHT:     University of California, San Francisco, 2006,
//                100X Imaging Inc, www.100ximaging.com, 2008
//                All Rights reserved
//
// LICENSE:       This library is free software; you can redistribute it and/or
//                modify it under the terms of the GNU Lesser General Public
//                License as published by the Free Software Foundation.
//                
//                You should have received a copy of the GNU Lesser General Public
//                License along with the source distribution; if not, write to
//                the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
//                Boston, MA  02111-1307  USA
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 06/07/2005
// 
// REVISIONS:     08/09/2005, N.A. - run-time loading of device libraries
//                08/22/2005, N.A. - intelligent loading of devices
//                                   (automatic device type)
//                05/15/2007, N.A. - Circular buffer interface and thread synchronization
//                12/18/2007  N.A. - Callbacks for GUI side notifications
//                05/20/2008  N.A. - Relative positions for stages, cached system state
//
// NOTES:                   
//                Public methods follow slightly different naming conventions than
//                the rest of the C++ code, i.e we have:
//                   getConfiguration();
//                instead of:
//                   GetConfiguration();
//                The alternative (lowercase function names) convention is used
//                because all public methods will most likely appear in other
//                programming environments (Java or Python).
//
// CVS:           $Id$
//
#ifndef _MMCORE_H_
#define _MMCORE_H_

// disable exception scpecification warnings in MSVC
#ifdef WIN32
#pragma warning( disable : 4290 )
#endif

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "../MMDevice/MMDeviceConstants.h"
#include "../MMDevice/MMDevice.h"
#include "PluginManager.h"
#include "Configuration.h"
#include "Error.h"
#include "ErrorCodes.h"

// forward declarations
class CircularBuffer;
class Configuration;
class PropertyBlock;
class CSerial;
class ConfigGroupCollection;
class CorePropertyCollection;
class CoreCallback;
class PixelSizeConfigGroup;
class Metadata;
class MMEventCallback;

/**
 * The interface to the core image acquisition services.
 * This class is intended as the top-most level interface to the core services.
 * Its public methods define the programmatic API, typically wrapped into the
 * high-level language wrapper (Python, Java, etc.). Public methods are designed
 * to conform to default processing conventions for the automatic wrapper generator
 * SWIG (http://www.swig.org).
 */
class CMMCore
{
friend class CoreCallback;

public:

	CMMCore();
	~CMMCore();
   
   /** @name Initialization and set-up
    * Loading of drivers, initialization and setting-up the environment.
    */
   //@ {
   void loadDevice(const char* label, const char* library, const char* name) throw (CMMError);
   void unloadAllDevices() throw (CMMError);
   void initializeAllDevices() throw (CMMError);
   void initializeDevice(const char* label) throw (CMMError);
   void reset() throw (CMMError);
   void clearLog();
   void initializeLogging();
   void shutdownLogging();
   void enableDebugLog(bool enable);
   void enableStderrLog(bool enable);
   std::string getUserId() const;
   std::string getHostName() const;
   void logMessage(const char* msg);
   std::string getVersionInfo() const;
   std::string getAPIVersionInfo() const;
   Configuration getSystemState() const;
   Configuration getSystemStateCache() const;
   void updateSystemStateCache();
   void setSystemState(const Configuration& conf);
   Configuration getConfigState(const char* group, const char* config) const throw (CMMError);
   Configuration getConfigGroupState(const char* group) const throw (CMMError);
   void saveSystemState(const char* fileName) throw (CMMError);
   void loadSystemState(const char* fileName) throw (CMMError);
   void saveSystemConfiguration(const char* fileName) throw (CMMError);
   void loadSystemConfiguration(const char* fileName) throw (CMMError);
   void registerCallback(MMEventCallback* cb);
   //@ }

   /** @name Device discovery and configuration interface.
    */
   std::vector<std::string> getAvailableDevices(const char* library) throw (CMMError);
   std::vector<std::string> getAvailableDeviceDescriptions(const char* library) throw (CMMError);
   std::vector<int> getAvailableDeviceTypes(const char* library) throw (CMMError);
 
   /** @name Generic device interface
    * API guaranteed to work for all devices.
    */
   //@ {
   std::vector<std::string> getDeviceLibraries(const char* path);
   std::vector<std::string> getLoadedDevices() const;
   std::vector<std::string> getLoadedDevicesOfType(MM::DeviceType devType) const;
   std::vector<std::string> getDevicePropertyNames(const char* label) const throw (CMMError);
   std::string getProperty(const char* label, const char* propName) const throw (CMMError);
   void setProperty(const char* label, const char* propName, const char* propValue) throw (CMMError);
   bool hasProperty(const char* label, const char* propName) const throw (CMMError);
   std::vector<std::string> getAllowedPropertyValues(const char* label, const char* propName) const throw (CMMError);
   bool isPropertyReadOnly(const char* label, const char* propName) const throw (CMMError);
   bool isPropertyPreInit(const char* label, const char* propName) const throw (CMMError);
   bool hasPropertyLimits(const char* label, const char* propName) const throw (CMMError);
   double getPropertyLowerLimit(const char* label, const char* propName) const throw (CMMError);
   double getPropertyUpperLimit(const char* label, const char* propName) const throw (CMMError);
   MM::PropertyType getPropertyType(const char* label, const char* propName) const throw (CMMError);
   MM::DeviceType getDeviceType(const char* label) throw (CMMError);
   bool deviceBusy(const char* deviceName) throw (CMMError);
   void waitForDevice(const char* deviceName) throw (CMMError);
   void waitForConfig(const char* group, const char* configName) throw (CMMError);
   bool systemBusy() throw (CMMError);
   void waitForSystem() throw (CMMError);
   void waitForImageSynchro() throw (CMMError);
   bool deviceTypeBusy(MM::DeviceType devType) throw (CMMError);
   void waitForDeviceType(MM::DeviceType devType) throw (CMMError);
   void sleep(double intervalMs) const;
   double getDeviceDelayMs(const char* label) const throw (CMMError);
   void setDeviceDelayMs(const char* label, double delayMs) throw (CMMError);
   void setTimeoutMs(long timeoutMs) {if (timeoutMs > 0) timeoutMs_ = timeoutMs;}
   long getTimeoutMs() { return timeoutMs_;}
   bool usesDeviceDelay(const char* label) const throw (CMMError);
   std::string getCoreErrorText(int code) const;
   //@ }

   /**
    * @name System role identification for devices.
    */
   //@ {
   std::string getCameraDevice();
   std::string getShutterDevice();
   std::string getFocusDevice();
   std::string getXYStageDevice();
   std::string getAutoFocusDevice();
   std::string getImageProcessorDevice();
   void setCameraDevice(const char* cameraLabel) throw (CMMError);
   void setShutterDevice(const char* shutterLabel) throw (CMMError);
   void setFocusDevice(const char* focusLabel) throw (CMMError);
   void setXYStageDevice(const char* xyStageLabel) throw (CMMError);
   void setAutoFocusDevice(const char* focusLabel) throw (CMMError);
   void setImageProcessorDevice(const char* procLabel) throw (CMMError);
   //@ }


   /** @name Multiple property settings
    * A single configuration applies to multiple devices at the same time.
    */
   //@ {
   void defineConfig(const char* groupName, const char* configName, const char* deviceName, const char* propName, const char* value);
   void defineConfigGroup(const char* groupName) throw (CMMError);
   void deleteConfigGroup(const char* groupName) throw (CMMError);
   bool isGroupDefined(const char* groupName);
   bool isConfigDefined(const char* groupName, const char* configName);
   void setConfig(const char* groupName, const char* configName) throw (CMMError);
   void deleteConfig(const char* groupName, const char* configName) throw (CMMError);
   std::vector<std::string> getAvailableConfigGroups() const;
   std::vector<std::string> getAvailableConfigs(const char* configGroup) const;
   std::string getCurrentConfig(const char* groupName) const throw (CMMError);
   Configuration getConfigData(const char* configGroup, const char* configName) const throw (CMMError);
   double getPixelSizeUm() const;
   double getPixelSizeUm(const char* resolutionID) throw (CMMError);
   double getMagnificationFactor() const;
   void setPixelSizeUm(const char* resolutionID, double pixSize)  throw (CMMError);
   void definePixelSizeConfig(const char* resolutionID, const char* deviceName, const char* propName, const char* value);
   std::vector<std::string> getAvailablePixelSizeConfigs() const;
   bool isPixelSizeConfigDefined(const char* resolutionID) const;
   void setPixelSizeConfig(const char* resolutionID) throw (CMMError);
   void deletePixelSizeConfig(const char* configName) const throw (CMMError);
   Configuration getPixelSizeConfigData(const char* configName) const throw (CMMError);

   //@ }

   /** @name Imaging support
    * Imaging related API.
    */
   //@ {
   void setROI(int x, int y, int xSize, int ySize) throw (CMMError); 
   void getROI(int& x, int& y, int& xSize, int& ySize) const throw (CMMError); 
   void clearROI() throw (CMMError);
   void setExposure(double exp) throw (CMMError);
   double getExposure() const throw (CMMError);
   void* getImage() const throw (CMMError);
   unsigned int* getRGB32Image() const throw (CMMError);
   void snapImage() throw (CMMError);
   unsigned getImageWidth() const;
   unsigned getImageHeight() const;
   unsigned getBytesPerPixel() const;
   unsigned getImageBitDepth() const;
   unsigned getNumberOfChannels() const;
   std::vector<std::string> getChannelNames() const;
   long getImageBufferSize() const;
   void assignImageSynchro(const char* deviceLabel) throw (CMMError);
   void removeImageSynchro(const char* label) throw (CMMError);
   void removeImageSynchroAll();
   void setAutoShutter(bool state);
   bool getAutoShutter();
   void setShutterOpen(bool state) throw (CMMError);
   bool getShutterOpen() throw (CMMError);
   void startSequenceAcquisition(long numImages, double intervalMs) throw (CMMError);
   void stopSequenceAcquisition() throw (CMMError);
   void* getLastImage() const throw (CMMError);
   void* popNextImage() throw (CMMError);

   void* getLastImageMD(unsigned channel, unsigned slice, Metadata& md) const throw (CMMError);
   void* popNextImageMD(unsigned channel, unsigned slice, Metadata& md) throw (CMMError);

   long snapImageMD() throw (CMMError);
   void* getImageMD(long handle, unsigned channel, unsigned slice) throw (CMMError);
   Metadata getImageMetadata(long handle, unsigned channel, unsigned slice) throw (CMMError);

   long getRemainingImageCount();
   long getBufferTotalCapacity();
   long getBufferFreeCapacity();
   double getBufferIntervalMs() const;
   bool isBufferOverflowed() const;
   void setCircularBufferMemoryFootprint(unsigned sizeMB) throw (CMMError);
   //@ }

   /** @name Auto-focusing
    * API for controlling auto-focusing devices or software modules.
    */
   //@ {
   double getFocusScore();
   void enableContinuousFocus(bool enable) throw (CMMError);
   bool isContinuousFocusEnabled() throw (CMMError);
   bool isContinuousFocusLocked() throw (CMMError);
   void fullFocus() throw (CMMError);
   void incrementalFocus() throw (CMMError);
   //@}

   /** @name State device support
    * API for controlling state devices (filters, turrets, etc.)
    */
   //@ {
   void setState(const char* deviceLabel, long state) throw (CMMError);
   long getState(const char* deviceLabel) const throw (CMMError);
   long getNumberOfStates(const char* deviceLabel);
   void setStateLabel(const char* deviceLabel, const char* stateLabel) throw (CMMError);
   std::string getStateLabel(const char* deviceLabel) const throw (CMMError);
   void defineStateLabel(const char* deviceLabel, long state, const char* stateLabel) throw (CMMError);
   std::vector<std::string> getStateLabels(const char* deviceLabel) const throw (CMMError);
   long getStateFromLabel(const char* deviceLabel, const char* stateLabel) const throw (CMMError);
   PropertyBlock getStateLabelData(const char* deviceLabel, const char* stateLabel) const;
   PropertyBlock getData(const char* deviceLabel) const;
   //@ }

   /** @name Property blocks
    * API for defining interchangeable equipment attributes
    */
   //@ {
   void definePropertyBlock(const char* blockName, const char* propertyName, const char* propertyValue);
   std::vector<std::string> getAvailablePropertyBlocks() const;
   PropertyBlock getPropertyBlockData(const char* blockName) const;
   //@ }

   /** @name Stage control
    * API for controlling X, Y and Z stages
    */
   //@ {
   void setPosition(const char* deviceName, double position) throw (CMMError);
   double getPosition(const char* deviceName) const throw (CMMError);
   void setRelativePosition(const char* deviceName, double d) throw (CMMError);
   void setXYPosition(const char* deviceName, double x, double y) throw (CMMError);
   void setRelativeXYPosition(const char* deviceName, double dx, double dy) throw (CMMError);
   void getXYPosition(const char* deviceName, double &x, double &y) throw (CMMError);
   double getXPosition(const char* deviceName) throw (CMMError);
   double getYPosition(const char* deviceName) throw (CMMError);
   void stop(const char* deviceName) throw (CMMError);
   void home(const char* deviceName) throw (CMMError);
   void setOriginXY(const char* deviceName) throw (CMMError);
   //@ }

   /** @name Serial port control
    * API for serial ports
    */
   //@ {
   void setSerialPortCommand(const char* name, const char* command, const char* term) throw (CMMError);
   std::string getSerialPortAnswer(const char* name, const char* term) throw (CMMError);
   void writeToSerialPort(const char* name, const std::vector<char> &data) throw (CMMError);
   std::vector<char> readFromSerialPort(const char* name) throw (CMMError);
   //@ }

   /** @name "  "
    */
   //@ {
   
   //@ }

private:
   // make object non-copyable
   CMMCore(const CMMCore& /*c*/) {}
   CMMCore& operator=(const CMMCore& /*rhs*/);

   typedef std::map<std::string, Configuration*> CConfigMap;
   typedef std::map<std::string, PropertyBlock*> CPropBlockMap;

   MM::Camera* camera_;
   MM::Shutter* shutter_;
   MM::Stage* focusStage_;
   MM::XYStage* xyStage_;
   MM::AutoFocus* autoFocus_;
   MM::ImageProcessor* imageProcessor_;
   long pollingIntervalMs_;
   long timeoutMs_;
   std::ofstream* logStream_;
   bool autoShutter_;
   MM::Core* callback_;                 // core services for devices
   ConfigGroupCollection* configGroups_;
   CorePropertyCollection* properties_;
   MMEventCallback* externalCallback_;  // notification hook to the higher layer (e.g. GUI)
   PixelSizeConfigGroup* pixelSizeGroup_;
   CircularBuffer* cbuf_;

   std::vector<MM::Device*> imageSynchro_;
   CPluginManager pluginManager_;
   std::map<int, std::string> errorText_;
   CConfigMap configs_;
   CPropBlockMap propBlocks_;
   bool debugLog_;
   Configuration stateCache_; // system state cache

   bool isConfigurationCurrent(const Configuration& config) const;
   void applyConfiguration(const Configuration& config) throw (CMMError);
   MM::Device* getDevice(const char* label) const throw (CMMError);
   template <class T>
   T* getSpecificDevice(const char* deviceLabel) const throw (CMMError);
   void waitForDevice(MM::Device* pDev) throw (CMMError);
   std::string getDeviceErrorText(int deviceCode, MM::Device* pDevice) const;
   std::string getDeviceName(MM::Device* pDev);
   void logError(const char* device, const char* msg, const char* file=0, int line=0) const;

   // >>>>> OBSOLETE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
   void defineConfiguration(const char* configName, const char* deviceName, const char* propName, const char* value);
   bool isConfigurationDefined(const char* configName);
   void setConfiguration(const char* configName) throw (CMMError);
   void deleteConfiguration(const char* configName) throw (CMMError);
   std::vector<std::string> getAvailableConfigurations() const;
   std::string getConfiguration() const;
   Configuration getConfigurationData(const char* config) const throw (CMMError);
};

#endif //_MMCORE_H_
