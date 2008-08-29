///////////////////////////////////////////////////////////////////////////////
// FILE:          MMCore.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMCore
//-----------------------------------------------------------------------------
// DESCRIPTION:   The interface to the MM core services. 
//              
// COPYRIGHT:     University of California, San Francisco, 2006
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
// REVISIONS:     see MMCore.h
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

#ifdef WIN32
#pragma warning(disable : 4312 4244)
#endif

#include <ace/OS.h>
#include <ace/High_Res_Timer.h>
#include <ace/Mutex.h>
#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>

#ifdef WIN32
#pragma warning(default : 4312 4244)
#endif


#include "CoreUtils.h"
#include "MMCore.h"
#include "../MMDevice/DeviceUtils.h"
#include "../MMDevice/ModuleInterface.h"
#include "../MMDevice/ImageMetadata.h"
#include "Configuration.h"
#include "MMEventCallback.h"
#include "ConfigGroup.h"
#include "CoreCallback.h"
#include "CoreProperty.h"
#include "CircularBuffer.h"
#include <assert.h>
#include <sstream>
#include <algorithm>
#include <vector>

#ifndef WIN32
// Needed on Unix for getcwd()
#include <unistd.h>
#endif

using namespace std;

// constants
const char* g_logFileName = "CoreLog.txt";

// version info
const int MMCore_versionMajor = 2;
const int MMCore_versionMinor = 3;
const int MMCore_versionBuild = 1;

// mutex
//static ACE_Mutex g_lock;

///////////////////////////////////////////////////////////////////////////////
// CMMcore class
// -------------

/**
 * Constructor.
 * Initializes buffers and error message text. It does not load any hardware
 * devices at this point.
 */
CMMCore::CMMCore() :
   camera_(0), shutter_(0), focusStage_(0), xyStage_(0), autoFocus_(0), imageProcessor_(0), pollingIntervalMs_(10), timeoutMs_(5000),
   logStream_(0), autoShutter_(true), callback_(0), configGroups_(0), properties_(0), externalCallback_(0), pixelSizeGroup_(0), cbuf_(0)
{
   configGroups_ = new ConfigGroupCollection();
   pixelSizeGroup_ = new PixelSizeConfigGroup();

   // build list of error strings
   errorText_[MMERR_OK] = "No errors.";
   errorText_[MMERR_GENERIC] = "Core error occured.";
   errorText_[MMERR_DEVICE_GENERIC] = "Device error encountered.";
   errorText_[MMERR_NoDevice] = "Device not defined or initialized.";
   errorText_[MMERR_SetPropertyFailed] = "Property does not exist, or value not allowed.";
   errorText_[MMERR_LoadLibraryFailed] = "Unable to load library: file not accessible or corrupted.";
   errorText_[MMERR_LibaryFunctionNotFound] =
      "Unable to identify expected interface: the library is not comaptible or corrupted.";
   errorText_[MMERR_CreateNotFound] =
      "Unable to identify CreateDevice function: the library is not comaptible or corrupted.";
   errorText_[MMERR_DeleteNotFound] =
      "Unable to identify DeleteDevice function: the library is not comaptible or corrupted.";
   errorText_[MMERR_CreateFailed] = "DeviceCreate function failed.";
   errorText_[MMERR_DeleteFailed] = "DeviceDelete function failed.";
   errorText_[MMERR_UnknownModule] = "Current device can't be unloaded: using unknown library.";
   errorText_[MMERR_UnexpectedDevice] = "Unexpected device encountered.";
   errorText_[MMERR_ModuleVersionMismatch] = "Module version mismatch.";
   errorText_[MMERR_DeviceVersionMismatch] = "Device interface version mismatch.";
   errorText_[MMERR_DeviceUnloadFailed] = "Requested device seems fine, but the current one failed to unload.";
   errorText_[MMERR_CameraNotAvailable] = "Camera not loaded or initialized.";
   errorText_[MMERR_InvalidStateDevice] = "Unsupported API. This device is not a state device";
   errorText_[MMERR_NoConfiguration] = "Configuration not defined";
   errorText_[MMERR_InvalidPropertyBlock] = "Property block not defined";
   errorText_[MMERR_UnhandledException] = "Internal inconsistency: unknown system exception encountered";
   errorText_[MMERR_DevicePollingTimeout] = "Device timed out";
   errorText_[MMERR_InvalidShutterDevice] = "Unsupported interface. This device is not a shutter.";
   errorText_[MMERR_DuplicateLabel] = "Specified device label already in use.";
   errorText_[MMERR_InvalidSerialDevice] = "Unsupported interface. The specified device is not a serial port.";
   errorText_[MMERR_InvalidSpecificDevice] = "Unsupported interface. Device is not of the correct type.";
   errorText_[MMERR_InvalidLabel] = "Can't find the device with the specified label.";
   errorText_[MMERR_FileOpenFailed] = "File open failed.";
   errorText_[MMERR_InvalidCFGEntry] = "Invalid configuration file line encountered. Wrong number of tokens for the current context.";
   errorText_[MMERR_InvalidContents] = "Reserved character(s) encountered in the value or name string.";
   errorText_[MMERR_InvalidCoreProperty] = "Unrecognized core property.";
   errorText_[MMERR_InvalidCoreValue] = "Core property is read-only or the requested value is not allowed.";
   errorText_[MMERR_NoConfigGroup] = "Configuration group not defined";
   errorText_[MMERR_DuplicateConfigGroup] = "Group name already in use.";
   errorText_[MMERR_CameraBufferReadFailed] = "Camera image buffer read failed.";
   errorText_[MMERR_CircularBufferFailedToInitialize] = "Failed to intialize circular buffer - memory requirements not adequate.";
   errorText_[MMERR_CircularBufferEmpty] = "Circular buffer is empty.";
   errorText_[MMERR_ContFocusNotAvailable] = "Auto-focus focus device not defined.";

   // open the log output stream
   logStream_= new std::ofstream();
   initializeLogging();
   CORE_LOG("-------->>\n");
   CORE_LOG2("Core session started on %D by %s on %s\n", getUserId().c_str(), getHostName().c_str());
   enableDebugLog(false);

   callback_ = new CoreCallback(this);
   cbuf_ = new CircularBuffer(10); // allocate 10MB initially

   // build the core property collection
   properties_ = new CorePropertyCollection(this);

   // set-up core properties
   try {

      // Initialize
      CoreProperty propInit("0", false);
      propInit.AddAllowedValue("0");
      propInit.AddAllowedValue("1");
      properties_->Add(MM::g_Keyword_CoreInitialize, propInit);

      // Auto shutter
      CoreProperty propAutoShutter("1", false);
      propAutoShutter.AddAllowedValue("0");
      propAutoShutter.AddAllowedValue("1");
      properties_->Add(MM::g_Keyword_CoreAutoShutter, propAutoShutter);

      // Camera device
      CoreProperty propCamera;
      properties_->Add(MM::g_Keyword_CoreCamera, propCamera);

      // Shutter device
      CoreProperty propShutter;
      properties_->Add(MM::g_Keyword_CoreShutter, propShutter);
      
      // Focus device
      CoreProperty propFocus;
      properties_->Add(MM::g_Keyword_CoreFocus, propFocus);

      // XYStage device
      CoreProperty propXYStage;
      properties_->Add(MM::g_Keyword_CoreXYStage, propXYStage);

      // Auto-focus device
      CoreProperty propAutoFocus;
      properties_->Add(MM::g_Keyword_CoreAutoFocus, propAutoFocus);

      // Processor device
      CoreProperty propImageProc;
      properties_->Add(MM::g_Keyword_CoreImageProcessor, propImageProc);

      // Time after which we give up on checking the Busy flag status
      CoreProperty propBusyTimeoutMs;
      properties_->Add(MM::g_Keyword_CoreTimeoutMs, propBusyTimeoutMs);

      properties_->Refresh();
   }
   catch(CMMError& err)
   {
      // trap exceptions and just log the message
      CORE_LOG1("Error in the core constructor\n%s\n", err.getMsg().c_str());
   }
}

/**
 * Destructor.
 * Cleans-up and unloads all devices.
 */
CMMCore::~CMMCore()
{
   try {
      reset();
   } catch (...) {
      CORE_LOG("Exception occured in MMCore destructor.\n");
      ; // don't let any exceptions leak through
   }
   CORE_LOG("Core session ended on %D\n");

   shutdownLogging();

   delete logStream_;
   delete callback_;
   delete configGroups_;
   delete properties_;
   delete cbuf_;
   delete pixelSizeGroup_;
}

/**
 * Delete an exisiting log file and start a new one.
 */
void CMMCore::clearLog()
{
   if (logStream_->is_open())
      logStream_->close();

   logStream_->open(g_logFileName, ios_base::trunc);

   initializeLogging();
   CORE_LOG("-------->>\n");
   CORE_LOG2("\nLog cleared and re-started on %D by %s on %s\n", getUserId().c_str(), getHostName().c_str());
}

/**
 * Record text message in the log file.
 */
void CMMCore::logMessage(const char* msg)
{
   CORE_LOG1("> %s\n", msg);
}

/**
 * Enable or disable logging of debug messages.
 * @param enable - if set to true debug messages will be recorded in the log file 
 */
void CMMCore::enableDebugLog(bool enable)
{
   debugLog_ = enable;
   if (debugLog_)
      ACE_LOG_MSG->priority_mask (LM_DEBUG|LM_INFO|LM_TRACE, ACE_Log_Msg::PROCESS);
   else
      ACE_LOG_MSG->priority_mask (LM_INFO, ACE_Log_Msg::PROCESS);

   CORE_LOG1("Debug logging %s\n", enable ? "enabled" : "disabled");
}

/**
 * Enables or disables log message display on the standard console.
 * @param enable - if set to true, log file messages will be echoed on the stderr.
 */
void CMMCore::enableStderrLog(bool enable)
{
   if (enable)
   {
      ACE_LOG_MSG->set_flags (ACE_Log_Msg::OSTREAM);
      ACE_LOG_MSG->set_flags (ACE_Log_Msg::STDERR);
   }
   else
   {
      ACE_LOG_MSG->set_flags (ACE_Log_Msg::OSTREAM);
      ACE_LOG_MSG->clr_flags (ACE_Log_Msg::STDERR);
   }
}

/**
 * Displays current user name.
 */
string CMMCore::getUserId() const
{
   char buf[ACE_MAX_USERID];
   ACE_OS::cuserid(buf);
   return string(buf);
}

/**
 * Displays current host name.
 */
string CMMCore::getHostName() const
{
   const int maxHostName(100);
   char buf[maxHostName] = "";
   ACE_OS::hostname(buf, maxHostName);
   return string(buf);
}

/**
 * Displays core version.
 */
string CMMCore::getVersionInfo() const
{
   ostringstream txt;
   string debug;
   txt << "MMCore version " << MMCore_versionMajor << "." << MMCore_versionMinor << "." << MMCore_versionBuild;
   #ifdef _DEBUG
   txt << " (debug)";
   #endif
   return txt.str();
}

/**
 * Get available devices from the specified library.
 */
vector<string> CMMCore::getAvailableDevices(const char* library) throw (CMMError)
{
   try
   {
      return pluginManager_.GetAvailableDevices(library);
   }
   catch (CMMError& e)
   {
      logError("core", e.getMsg().c_str());
      throw;
   }
}
/**
 * Get descriptions for available devices from the specified library.
 */
vector<string> CMMCore::getAvailableDeviceDescriptions(const char* library) throw (CMMError)
{
   try
   {
      return pluginManager_.GetAvailableDeviceDescriptions(library);
   }
   catch (CMMError& e)
   {
      logError("core", e.getMsg().c_str());
      throw;
   }
}
/**
 * Get type information for available devices from the specified library.
 */
vector<int> CMMCore::getAvailableDeviceTypes(const char* library) throw (CMMError)
{
   try
   {
      return pluginManager_.GetAvailableDeviceTypes(library);
   }
   catch (CMMError& e)
   {
      logError("core", e.getMsg().c_str());
      throw;
   }
}

/**
 * Displays the module and device interface versions.
 */
string CMMCore::getAPIVersionInfo() const
{
   ostringstream txt;
   txt << "Device API version " << DEVICE_INTERFACE_VERSION << ", " << "Module API version " << MODULE_INTERFACE_VERSION;
   return txt.str();
}

/**
 * Returns the entire system state, i.e. the collection of all property values from all devices.
 * @return - Configuration object containing a collection of device-property-value triplets
 */
Configuration CMMCore::getSystemState() const
{
   Configuration config;
   vector<string> devices = pluginManager_.GetDeviceList();
   vector<string>::const_iterator i;
   for (i=devices.begin(); i!=devices.end(); i++)
   {
      MM::Device* pDev = getDevice(i->c_str());
      for (unsigned j=0; j<pDev->GetNumberOfProperties(); j++)
      {
         char name[MM::MaxStrLength]="";
         char val[MM::MaxStrLength]="";
         bool readOnly;
         pDev->GetPropertyName(j, name);
         pDev->GetPropertyReadOnly(name, readOnly);
         pDev->GetProperty(name, val);
         config.addSetting(PropertySetting(i->c_str(), name, val, readOnly));
     }   
   }

   // add core properties
   vector<string> coreProps = properties_->GetNames();
   for (unsigned i=0; i < coreProps.size(); i++)
   {
      string name = coreProps[i];
      string val = properties_->Get(name.c_str());
      config.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, name.c_str(), val.c_str(), properties_->IsReadOnly(name.c_str())));
   }

   return config;
}

/**
 * Returns the entire system state, i.e. the collection of all property values from all devices.
 * This method will return cached values instead of querying each device
 * @return - Configuration object containing a collection of device-property-value triplets
 */
Configuration CMMCore::getSystemStateCache() const
{
   return stateCache_;
}

/**
 * Returns the parital state of the system, only for the devices included in the
 * specified configuration.
 */
Configuration CMMCore::getConfigState(const char* group, const char* config) const throw (CMMError)
{
   Configuration cfgData = getConfigData(group, config);
   Configuration state;
   for (size_t i=0; i < cfgData.size(); i++)
   {
      PropertySetting cs = cfgData.getSetting(i); // config setting
      string value = getProperty(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str());
      PropertySetting ss(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str(), value.c_str()); // state setting
      state.addSetting(ss);
   }
   return state;
}


/**
 * Returns the parital state of the system, only for the devices included in the
 * specified group. It will create a union of all devices referenced in a group.
 */
Configuration CMMCore::getConfigGroupState(const char* group) const throw (CMMError)
{
   vector<string> configs = configGroups_->GetAvailableConfigs(group);
   Configuration state;
   for (size_t i=0; i<configs.size(); i++) {
      Configuration cfgData = getConfigData(group, configs[i].c_str());
      for (size_t i=0; i < cfgData.size(); i++)
      {
         PropertySetting cs = cfgData.getSetting(i); // config setting
         if (!state.isPropertyIncluded(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str()))
         {
            string value = getProperty(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str());
            PropertySetting ss(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str(), value.c_str()); // state setting
            state.addSetting(ss);
         }
      }
   }
   return state;
}

/**
 * Sets all properties contained in the Configuration object.
 * The procedure will attempt to set each property it enocunters, but won't stop
 * if any of the properties fail or if the requested device is not present. It will
 * just quietly continue.
 *
 * @param conf - configuration object
 */
void CMMCore::setSystemState(const Configuration& conf)
{
   for (unsigned i=0; i<conf.size(); i++)
   {
      PropertySetting s = conf.getSetting(i);
      if (!s.getReadOnly())
      {
         try
         {
            setProperty(s.getDeviceLabel().c_str(), s.getPropertyName().c_str(), s.getPropertyValue().c_str());
         }
         catch (CMMError& err)
         {
            CORE_LOG3("Property setting failed: %s-%s-%s\n",
            s.getDeviceLabel().c_str(), s.getPropertyName().c_str(), s.getPropertyValue().c_str());
            CORE_LOG1("%s\n", err.getMsg().c_str());
         }
      }
   }

   updateSystemStateCache();
}

/**
 * Returns a list of library names available in the specified directory.
 * @param path - serach path. If zero, current working directory will be used as default. 
 */
vector<string> CMMCore::getDeviceLibraries(const char* path)
{
   string searchPath;
   if (path != 0 && strlen(path) > 0)
      searchPath = path;
   else
   {
#ifdef WIN32
      char* pathBuf = _getcwd(NULL, 0);
#else
      char* pathBuf = getcwd(NULL,0);
#endif
      if (pathBuf == NULL)
         CORE_DEBUG("getDeviceLibraries(): Failed to obtain current working directory.\n");
      else
      {
         searchPath = pathBuf;
         free(pathBuf);
      }
   }

   CORE_DEBUG1("Search path: %s\n", searchPath.c_str());
   return pluginManager_.GetModules(searchPath.c_str());
}

/**
 * Loads a device from the plugin library.
 * @param label assigned name for the device during the core session
 * @param library the name of the plugin library (dll). The name should be supplied without the
 *                extension and path since the naming convention and locations are paltform
 *                dependent
 * @param device the name of the device. The name must correspond to one of the names recognized
 *                   by the specific plugin library.
 */
void CMMCore::loadDevice(const char* label, const char* library, const char* device) throw (CMMError)
{
   initializeLogging(); // we need to do this here as well as in the constructor
                        // beacuse ACE global variables are initalized on per thread basis

   try
   {
      MM::Device* pDevice = pluginManager_.LoadDevice(label, library, device);
      CORE_LOG3("Device %s loaded from %s and labeled as %s\n", device, library, label);
      pDevice->SetCallback(callback_);
      
      // default special roles for particular devices
      // The roles which are assigned at the load time will make sense for a simple
      // configuration. More complicated configurations will typically override default settings.
      switch(pDevice->GetType())
      {
         case MM::CameraDevice:
            /* >>> Linux build asserts false here - apparently rtti does not work accross DLL!?
            camera_ = dynamic_cast<MM::Camera*>(pDevice);
            assert(camera_);
            */
            camera_ = static_cast<MM::Camera*>(pDevice);
            CORE_LOG1("Device %s set as camera.\n", label);
         break;

         case MM::StateDevice:
            // nothing to do for now
         break;

         case MM::ShutterDevice:
            shutter_ = static_cast<MM::Shutter*>(pDevice);
            //assignImageSynchro(label);
            CORE_LOG1("Device %s set as shutter.\n", label);
         break;

         case MM::XYStageDevice:
            xyStage_ = static_cast<MM::XYStage*>(pDevice);
            CORE_LOG1("Device %s set as xyStage.\n", label);
         break;

         case MM::AutoFocusDevice:
            autoFocus_ = static_cast<MM::AutoFocus*>(pDevice);
            CORE_LOG1("Device %s set as auto-focus.\n", label);
         break;

         default:
            // no action on unrecognized device
            //CORE_LOG1("%s: unknown device type\n", label);
        break;
      }
   }
   catch (CMMError& err)
   {
      // augment the error message with the core text
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      logError("MMCore::loadDevice", err.getMsg().c_str());
      throw;
   }
}

/**
 * Unloads all devices from the core and resets all configuration data.
 */
void CMMCore::unloadAllDevices() throw (CMMError)
{
   try {

      // clear all roles
      camera_ = 0;
      shutter_ = 0;
      focusStage_ = 0;
      xyStage_ = 0;
      autoFocus_ = 0;
      imageProcessor_ = 0;

      // unload modules
      pluginManager_.UnloadAllDevices();
      CORE_LOG("All devices unloaded.\n");
      imageSynchro_.clear();
      
      // clear configurations
      CConfigMap::const_iterator it;
      for (it = configs_.begin(); it != configs_.end(); it++)
      {
         delete it->second;
      }
      configs_.clear();

      configGroups_->Clear();

	   properties_->Refresh();
   
     // clear equipment definitions ???
	  // TODO
   }
   catch (CMMError& err) {
      // augment the error message with the core text
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      logError("MMCore::unloadAllDevices", err.getMsg().c_str());
      throw; 
   }
}

/**
 * Unloads all devices from the core, clears all configration data and property blocks.
 */
void CMMCore::reset() throw (CMMError)
{
   // before unloading everything try to apply shutdown configuration
   if (isConfigDefined(MM::g_CFGGroup_System, MM::g_CFGGroup_System_Shutdown))
      this->setConfig(MM::g_CFGGroup_System, MM::g_CFGGroup_System_Shutdown);
   waitForSystem();

   // unload devices
   unloadAllDevices();

   // clear property blocks
   CPropBlockMap::const_iterator i;
   for (i = propBlocks_.begin(); i != propBlocks_.end(); i++)
      delete i->second;
   propBlocks_.clear();

   // clear configurations
   CConfigMap::const_iterator j;
   for (j = configs_.begin(); j != configs_.end(); j++)
      delete j->second;
   configs_.clear();

   properties_->Refresh();

   CORE_LOG("System reset at %D.\n");
}


/**
 * Calls Initialize() method for each loaded device.
 * This method also initialized allowed values for core properties, based
 * on the collection of loaded devices.
 */
void CMMCore::initializeAllDevices() throw (CMMError)
{
   vector<string> devices = pluginManager_.GetDeviceList();
   CORE_LOG1("Starting initialization sequence for %d devices...\n", devices.size());
   for (size_t i=0; i<devices.size(); i++)
   {
      MM::Device* pDevice;
      try {
         pDevice = pluginManager_.GetDevice(devices[i].c_str());
      }
      catch (CMMError& err) {
         err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
         logError(devices[i].c_str(), err.getMsg().c_str(), __FILE__, __LINE__);
         throw;
      }
      int nRet = pDevice->Initialize();
      if (nRet != DEVICE_OK)
      {
         logError(devices[i].c_str(), getDeviceErrorText(nRet, pDevice).c_str(), __FILE__, __LINE__);
         throw CMMError(getDeviceErrorText(nRet, pDevice).c_str(), MMERR_DEVICE_GENERIC);
      }

      CORE_LOG1("Device %s initialized.\n", devices[i].c_str());
   }

   // Camera device
   vector<string> cameras = getLoadedDevicesOfType(MM::CameraDevice);
   cameras.push_back(""); // add empty value
   properties_->ClearAllowedValues(MM::g_Keyword_CoreCamera);
   for (size_t i=0; i<cameras.size(); i++)
      properties_->AddAllowedValue(MM::g_Keyword_CoreCamera, cameras[i].c_str());

   // Shutter device
   vector<string> shutters = getLoadedDevicesOfType(MM::ShutterDevice);
   shutters.push_back(""); // add empty value
   properties_->ClearAllowedValues(MM::g_Keyword_CoreShutter);
   for (size_t i=0; i<shutters.size(); i++)
      properties_->AddAllowedValue(MM::g_Keyword_CoreShutter, shutters[i].c_str());
   
   // focus device
   vector<string> stages = getLoadedDevicesOfType(MM::StageDevice);
   stages.push_back(""); // add empty value
   properties_->ClearAllowedValues(MM::g_Keyword_CoreFocus);
   for (size_t i=0; i<stages.size(); i++)
      properties_->AddAllowedValue(MM::g_Keyword_CoreFocus, stages[i].c_str());

   // XY device
   vector<string> xystages = getLoadedDevicesOfType(MM::XYStageDevice);
   xystages.push_back(""); // add empty value
   properties_->ClearAllowedValues(MM::g_Keyword_CoreXYStage);
   for (size_t i=0; i<xystages.size(); i++)
      properties_->AddAllowedValue(MM::g_Keyword_CoreXYStage, xystages[i].c_str());

    // auto-focus device
   vector<string> afmodules = getLoadedDevicesOfType(MM::AutoFocusDevice);
   afmodules.push_back(""); // add empty value
   properties_->ClearAllowedValues(MM::g_Keyword_CoreAutoFocus);
   for (size_t i=0; i<afmodules.size(); i++)
      properties_->AddAllowedValue(MM::g_Keyword_CoreAutoFocus, afmodules[i].c_str());

   // image processor device
   vector<string> procmodules = getLoadedDevicesOfType(MM::ImageProcessorDevice);
   procmodules.push_back(""); // add empty value
   properties_->ClearAllowedValues(MM::g_Keyword_CoreImageProcessor);
   for (size_t i=0; i<procmodules.size(); i++)
      properties_->AddAllowedValue(MM::g_Keyword_CoreImageProcessor, procmodules[i].c_str());

   // Busy flag time out: all positive values are allowed
   
   properties_->Refresh();

}

/**
 * Initializes specific device.
 *
 * @param label device label
 */
void CMMCore::initializeDevice(const char* label) throw (CMMError)
{
   MM::Device* pDevice = getDevice(label);
   
   int nRet = pDevice->Initialize();
   if (nRet != DEVICE_OK)
   {
      logError(label, getDeviceErrorText(nRet, pDevice).c_str(), __FILE__, __LINE__);
      throw CMMError(getDeviceErrorText(nRet, pDevice).c_str(), MMERR_DEVICE_GENERIC);
   }
   
   CORE_LOG1("Device %s initialized.\n", label);
}

/**
 * Updates the state of the entire hardware.
 */
void CMMCore::updateSystemStateCache()
{
   stateCache_ = getSystemState();
   CORE_LOG("Syestem state cache updated.\n");
}

/**
 * Returns device type.
 */
MM::DeviceType CMMCore::getDeviceType(const char* label) throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
      return MM::CoreDevice;

   MM::Device* pDevice = getDevice(label);
   return pDevice->GetType();
}

/**
 * Reports action delay in milliseconds for the specific device.
 * The delay is used in the synchronization process to ensure that
 * the action is performed, without polling.
 * Value of "0" means that action is either blocking or that polling
 * of device status is required.
 * Some devices ignore this setting.
 *
 * @return double - delay time
 * @param const char* label
 */
double CMMCore::getDeviceDelayMs(const char* label) const throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return 0.0;
   }
   MM::Device* pDevice = getDevice(label);
   return pDevice->GetDelayMs();
}


/**
 * Overrides the built-in value for the action delay.
 * Some devices ignore this setting.
 *
 * @return void 
 * @param const char* label
 * @param double delayMs
 */
void CMMCore::setDeviceDelayMs(const char* label, double delayMs) throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
      return; // ignore

   MM::Device* pDevice = getDevice(label);
   pDevice->SetDelayMs(delayMs);
}

/**
 * Signals if the device will use the delay setting or not.
 *
 * @return bool - uses delay if true
 */
bool CMMCore::usesDeviceDelay(const char* label) const throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return false;
   }
   MM::Device* pDevice = getDevice(label);
   return pDevice->UsesDelay();
}

/**
 * Checks the busy status of the specific device.
 * @param label - device label
 */
bool CMMCore::deviceBusy(const char* label) throw (CMMError)
{
   MM::Device* pDevice = getDevice(label);
   return pDevice->Busy();
}


/**
 * Waits (blocks the calling thread) for specified time in milliseconds.
 * @param double intervalMs
 */
void CMMCore::sleep(double intervalMs) const
{
   ACE_Time_Value tv(0, (long)intervalMs * 1000);
   ACE_OS::sleep(tv);
}


/**
 * Waits (blocks the calling thread) until the specified device becomes
 * non-busy.
 * @param const char* label - device label
 */
void CMMCore::waitForDevice(const char* label) throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
      return; // core property commands always block - no need to poll

   MM::Device* pDevice = getDevice(label);
   waitForDevice(pDevice);
}


/**
 * Waits (blocks the calling thread) until the specified device becomes
 * @param const MM::Device* pDev - device
 */
void CMMCore::waitForDevice(MM::Device* pDev) throw (CMMError)
{
   CORE_DEBUG1("Waiting for device %s...\n", pluginManager_.GetDeviceLabel(*pDev).c_str());

   TimeoutMs timeout(timeoutMs_);

   while (pDev->Busy())
   {
      if (timeout.expired())
      {
         string label = pluginManager_.GetDeviceLabel(*pDev);
         logError(label.c_str(), "wait() timed out", __FILE__, __LINE__);
         throw CMMError(label.c_str(), getCoreErrorText(MMERR_DevicePollingTimeout).c_str(), MMERR_DevicePollingTimeout);
      }

     CORE_DEBUG("Polling...\n");
     sleep(pollingIntervalMs_);
   }
   CORE_DEBUG("Finished waiting.\n");
}
/**
 * Checks the busy status of the entire system. The system will report busy if any
 * of the devices is busy.
 * @return bool status - true on busy
 */
bool CMMCore::systemBusy() throw (CMMError)
{
   return deviceTypeBusy(MM::AnyType);
}


/**
 * Blocks until all devices in the system become ready (not-busy).
 */
void CMMCore::waitForSystem() throw (CMMError)
{
   waitForDeviceType(MM::AnyType);
}


/**
 * Checks the busy status for all devices of the specific type.
 * The system will report busy if any of the devices of the spefified type are busy.
 *
 * @return bool - true on busy
 * @param MM::DeviceType devType
 */
bool CMMCore::deviceTypeBusy(MM::DeviceType devType) throw (CMMError)
{
   vector<string> devices = pluginManager_.GetDeviceList(devType);
   for (size_t i=0; i<devices.size(); i++)
   {
      try {
         MM::Device* pDevice;
         pDevice = pluginManager_.GetDevice(devices[i].c_str());
         if (pDevice->Busy())
            return true;
      }
      catch (...) {
         // trap all exceptions
         assert(!"Plugin manager can't access device it reported as available.");
      }
   }
   return false;
}


/**
 * Blocks until all devices of the specific type become ready (not-busy).
 * @param devType constant specifying the device type
 */
void CMMCore::waitForDeviceType(MM::DeviceType devType) throw (CMMError)
{
   vector<string> devices = pluginManager_.GetDeviceList(devType);
   for (size_t i=0; i<devices.size(); i++)
      waitForDevice(devices[i].c_str());
}

/**
 * Blocks until all devices included in the configuration become ready.
 * @param group
 * @param config
 */
void CMMCore::waitForConfig(const char* group, const char* configName) throw (CMMError)
{
   Configuration cfg = getConfigData(group, configName);
   try {
      for(size_t i=0; i<cfg.size(); i++)
         waitForDevice(cfg.getSetting(i).getDeviceLabel().c_str());
   } catch (CMMError& err) {
      // trap MM exceptions and keep quiet - this is not a good time to blow up
      logError("waitForConfig", err.getMsg().c_str());
   }
}

/**
 * Wait for the slowest device in the ImageSynchro list. 
 */
void CMMCore::waitForImageSynchro() throw (CMMError)
{
   for (size_t i=0; i<imageSynchro_.size(); i++)
   {
      // poll the device until it stops...
      waitForDevice(imageSynchro_[i]);
   }
}

/**
 * Sets the position of the stage in microns.
 * @param const char* label
 * @param double position
 */
void CMMCore::setPosition(const char* label, double position) throw (CMMError)
{
   MM::Stage* pStage = getSpecificDevice<MM::Stage>(label);
   int ret = pStage->SetPositionUm(position);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_DEBUG2("%s set to %.5g um\n", label, position);
}

/**
 * Sets the relative position of the stage in microns.
 * @param const char* label
 * @param double d
 */
void CMMCore::setRelativePosition(const char* label, double d) throw (CMMError)
{
   MM::Stage* pStage = getSpecificDevice<MM::Stage>(label);
   int ret = pStage->SetRelativePositionUm(d);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_DEBUG2("%s moved relative %.5g um\n", label, d);
}

/**
 * Returns the current position of the stage in microns.
 * @return position 
 * @param label
 */
double CMMCore::getPosition(const char* label) const throw (CMMError)
{
   MM::Stage* pStage = getSpecificDevice<MM::Stage>(label);
   double pos;
   int ret = pStage->GetPositionUm(pos);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   return pos;
}

/**
 * Sets the position of the XY stage in microns.
 * @param const char* label
 * @param x
 * @param y
 */
void CMMCore::setXYPosition(const char* deviceName, double x, double y) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   int ret = pXYStage->SetPositionUm(x, y);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pXYStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_DEBUG3("%s set to %g.3 %g.3 um\n", deviceName, x, y);
}

/**
 * Sets the relative position of the XY stage in microns.
 * @param const char* label
 * @param dx
 * @param dy
 */
void CMMCore::setRelativeXYPosition(const char* deviceName, double dx, double dy) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   int ret = pXYStage->SetRelativePositionUm(dx, dy);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pXYStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_DEBUG3("%s moved relative %g.3 %g.3 um\n", deviceName, dx, dy);
}

/**
 * Obtains the current position of the XY stage in microns.
 * @param const char* label
 * @param x
 * @param y
 */
void CMMCore::getXYPosition(const char* deviceName, double& x, double& y) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   int ret = pXYStage->GetPositionUm(x, y);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pXYStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }
}

/**
 * Obtains the current position of the X axis of the XY stage in microns.
 * @return x position 
 * @param const char* label
 */
double CMMCore::getXPosition(const char* deviceName) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   double x, y;
   int ret = pXYStage->GetPositionUm(x, y);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pXYStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }

   return x;
}

/**
 * Obtains the current position of the Y axis of the XY stage in microns.
 * @return y position 
 * @param const char* label
 */
double CMMCore::getYPosition(const char* deviceName) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   double x, y;
   int ret = pXYStage->GetPositionUm(x, y);
   if (ret != DEVICE_OK)
   {
      char name[MM::MaxStrLength];
      pXYStage->GetName(name);
      logError(name, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }

   return y;
}

/**
 * stop the XY stage motors.
 * @return void 
 * @param const char* label
 */
///**
// * Aborts current action.
// */
void CMMCore::stop(const char* deviceName) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   int ret = pXYStage->Stop();
   if (ret != DEVICE_OK)
   {
      logError(deviceName, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_LOG1("Device %s stopped!\n", deviceName);
}

/**
 * Calibrates and homes the XY stage.
 */
void CMMCore::home(const char* deviceName) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   int ret = pXYStage->Home();
   if (ret != DEVICE_OK)
   {
      logError(deviceName, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_LOG1("Stage %s moved to the HOME position.\n", deviceName);
}

//jizhen, 4/12/2007
/**
 * zero the current XY position.
 * @return void 
 * @param const char* deviceName
 */
void CMMCore::setOriginXY(const char* deviceName) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   int ret = pXYStage->SetOrigin();
   if (ret != DEVICE_OK)
   {
      logError(deviceName, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_LOG1("Stage %s's current position was zeroed.\n", deviceName);
}
//eof jizhen

/**
 * Define x,y coordinates in um for the current position.
 */
void CMMCore::setAdapterOriginXY(const char* deviceName, double x, double y) throw (CMMError)
{
   MM::XYStage* pXYStage = getSpecificDevice<MM::XYStage>(deviceName);
   int ret = pXYStage->SetAdapterOriginUm(x, y);
   if (ret != DEVICE_OK)
   {
      logError(deviceName, getDeviceErrorText(ret, pXYStage).c_str());
      throw CMMError(getDeviceErrorText(ret, pXYStage).c_str(), MMERR_DEVICE_GENERIC);
   }
   CORE_LOG3("Stage %s's current position was set as %.2f,%.2f um.\n", deviceName, x, y);
}

/**
 * Acquires a single image. 
 */
void CMMCore::snapImage() throw (CMMError)
{
   if (camera_)
   {
      int ret = DEVICE_OK;
      try {

         // wait for all synchronized devices to stop before taking an image
         waitForImageSynchro();

         // open the shutter
         if (shutter_ && autoShutter_)
         {
            shutter_->SetOpen(true);
            waitForDevice(shutter_);
         }
         ret = camera_->SnapImage();

         // close the shutter
         if (shutter_ && autoShutter_)
         {
            shutter_->SetOpen(false);
            waitForDevice(shutter_);
         }
      } catch (...) {
         logError("CMMCore::snapImage", getCoreErrorText(MMERR_UnhandledException).c_str(), __FILE__, __LINE__);
         throw CMMError(getCoreErrorText(MMERR_UnhandledException).c_str(), MMERR_UnhandledException);
      }

      if (ret != DEVICE_OK)
      {
         logError("CMMCore::snapImage", getDeviceErrorText(ret, camera_).c_str(), __FILE__, __LINE__);
         throw CMMError(getDeviceErrorText(ret, camera_).c_str(), MMERR_DEVICE_GENERIC);
      }

      CORE_DEBUG("Image acquired at %D\n");
   }
   else
      throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);
}


/**
 * Add device to the image-synchro list. Image acquistion waits for all devices
 * in this list.
 * @param const char* label - device label
 */
void CMMCore::assignImageSynchro(const char* label) throw (CMMError)
{
   imageSynchro_.push_back(getDevice(label));
   CORE_LOG1("Image acquisition synchronized with %s\n", label);
}

/**
 * Removes device from the image-synchro list.
 * @param const char* label - device label
 */
void CMMCore::removeImageSynchro(const char* label) throw (CMMError)
{
   MM::Device* dev = getDevice(label);
   vector<MM::Device*>::iterator it = find(imageSynchro_.begin(), imageSynchro_.end(), dev);
   if(it != imageSynchro_.end())
   {
      imageSynchro_.erase(it);
      CORE_LOG1("Device %s removed from the image synchro list.\n", label);
   }
   else
      CORE_DEBUG1("Device %s requested for removal was not in the image synchro list.\n", label);
      //throw CMMError(getCoreErrorText(MMERR_NotInImageSynchro).c_str(), MMERR_NotInImageSynchro);
}

/**
 * Clears the image synchro device list.
 */
void CMMCore::removeImageSynchroAll()
{
   imageSynchro_.clear();
   CORE_LOG("Image synchro cleared. No devices are synchronized with the camera.\n");
}

/**
 * If this option is enabled Shutter automatically opens and closes when the image
 * is acquired.
 * @param bool state - true for enabled
 */
void CMMCore::setAutoShutter(bool state)
{
   properties_->Set(MM::g_Keyword_CoreAutoShutter, state ? "1" : "0");
   autoShutter_ = state;
   stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoShutter, state ? "1" : "0"));
   CORE_LOG1("Auto shutter %s.\n", state ? "ON" : "OFF");
}

/**
 * Returns the current setting of the auto-shutter option.
 */
bool CMMCore::getAutoShutter()
{
   return autoShutter_;
}

/**
 * Opens or closes the default shutter.
 */
void CMMCore::setShutterOpen(bool state) throw (CMMError)
{
   if (shutter_)
   {
      int ret = shutter_->SetOpen(state);
      if (ret != DEVICE_OK)
      {
         logError("CMMCore::setShutterOpen()", getDeviceErrorText(ret, shutter_).c_str());
         throw CMMError(getDeviceErrorText(ret, shutter_).c_str(), MMERR_DEVICE_GENERIC);
      }

      if (shutter_->HasProperty(MM::g_Keyword_State))
      {
         char shutterName[MM::MaxStrLength];
         shutter_->GetLabel(shutterName);
         stateCache_.addSetting(PropertySetting(shutterName, MM::g_Keyword_State, CDeviceUtils::ConvertToString(state)));
      }
   }
}

/**
 * Return the default shutter state.
 */
bool CMMCore::getShutterOpen() throw (CMMError)
{
   bool state = true; // default open
   if (shutter_)
   {
      int ret = shutter_->GetOpen(state);
      if (ret != DEVICE_OK)
      {
         logError("CMMCore::getShutterOpen()", getDeviceErrorText(ret, shutter_).c_str());
         throw CMMError(getDeviceErrorText(ret, shutter_).c_str(), MMERR_DEVICE_GENERIC);
      }
   }
   return state;
}
/**
 * Exposes the internal image buffer.
 *
 * Designed specifically for the SWIG wrapping for Java and scripting languages.
 * @return a pointer to the internal image buffer.
 */
void* CMMCore::getImage() const throw (CMMError)
{
   if (!camera_)
      throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);
   else
   {
      void* pBuf(0);
      try {
         pBuf = const_cast<unsigned char*> (camera_->GetImageBuffer());
      } catch (...) {
         logError("CMMCore::getImage()", getCoreErrorText(MMERR_UnhandledException).c_str());
         throw CMMError(getCoreErrorText(MMERR_UnhandledException).c_str(), MMERR_UnhandledException);
      }

      if (pBuf != 0)
         return pBuf;
      else
      {
         logError("CMMCore::getImage()", getCoreErrorText(MMERR_CameraBufferReadFailed).c_str());
         throw CMMError(getCoreErrorText(MMERR_CameraBufferReadFailed).c_str(), MMERR_CameraBufferReadFailed);
      }
   }
}

/**
 * Exposes the internal image buffer as RGB pixels.
 *
 * Designed specifically for the SWIG wrapping for Java and scripting languages.
 * @return a pointer to the internal image buffer.
 */
unsigned int* CMMCore::getRGB32Image() const throw (CMMError)
{
   if (!camera_)
      throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);
   else
   {
      unsigned int* pBuf(0);
      try {
         pBuf = const_cast<unsigned int*> (camera_->GetImageBufferAsRGB32());
      } catch (...) {
         throw CMMError(getCoreErrorText(MMERR_UnhandledException).c_str(), MMERR_UnhandledException);
      }

      if (pBuf != 0)
         return pBuf;
      else
         throw CMMError(getCoreErrorText(MMERR_CameraBufferReadFailed).c_str(), MMERR_CameraBufferReadFailed);
   }
}

/**
 * Returns the size of the internal image buffer.
 *
 * @return buffer size
 */
long CMMCore::getImageBufferSize() const
{
   if (camera_)
      return camera_->GetImageBufferSize();
   else
      return 0;
}

/**
 * Starts straming camera sequence acquisition.
 * This command does not block the calling thread for the duration of the acquisition.
 */
void CMMCore::startSequenceAcquisition(long numImages, double intervalMs) throw (CMMError)
{
   if (camera_)
   {
      if (!cbuf_->Initialize(1, 1, camera_->GetImageWidth(), camera_->GetImageHeight(), camera_->GetImageBytesPerPixel()))
      {
         logError(getDeviceName(camera_).c_str(), getCoreErrorText(MMERR_CircularBufferFailedToInitialize).c_str());
         throw CMMError(getCoreErrorText(MMERR_CircularBufferFailedToInitialize).c_str(), MMERR_CircularBufferFailedToInitialize);
      }
      cbuf_->Clear();
      int nRet = camera_->StartSequenceAcquisition(numImages, intervalMs);
      if (nRet != DEVICE_OK)
         throw CMMError(getDeviceErrorText(nRet, camera_).c_str(), MMERR_DEVICE_GENERIC);
   }
   else
   {
      logError(getDeviceName(camera_).c_str(), getCoreErrorText(MMERR_CameraNotAvailable).c_str());
      throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);
   }
   CORE_DEBUG("Sequence acquisition started.");
}

/**
 * Stops straming camera sequence acquisition.
 */
void CMMCore::stopSequenceAcquisition() throw (CMMError)
{
   if (camera_)
   {
      int nRet = camera_->StopSequenceAcquisition();
      if (nRet != DEVICE_OK)
      {
         logError(getDeviceName(camera_).c_str(), getDeviceErrorText(nRet, camera_).c_str());
         throw CMMError(getDeviceErrorText(nRet, camera_).c_str(), MMERR_DEVICE_GENERIC);
      }
   }
   else
   {
      logError(getDeviceName(camera_).c_str(), getCoreErrorText(MMERR_CameraNotAvailable).c_str());
      throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);
   }

   CORE_DEBUG("Sequence acquisition stopped.");
}

/**
 * Gets the last image from the circular buffer.
 * Returns 0 if the buffer is empty.
 */
void* CMMCore::getLastImage() const throw (CMMError)
{
   unsigned char* pBuf = const_cast<unsigned char*>(cbuf_->GetTopImage());
   if (pBuf != 0)
      return pBuf;
   else
   {
      logError("CMMCore::getLastImage", getCoreErrorText(MMERR_CircularBufferEmpty).c_str());
      throw CMMError(getCoreErrorText(MMERR_CircularBufferEmpty).c_str(), MMERR_CircularBufferEmpty);
   }
}

void* CMMCore::getLastImageMD(unsigned channel, unsigned slice, Metadata& md) const throw (CMMError)
{
   const ImgBuffer* pBuf = cbuf_->GetTopImageBuffer(channel, slice);
   if (pBuf != 0)
   {
      md.Clear();
      const MM::ImageMetadata imd = pBuf->GetMetadata();
      MetadataSingleTag mt;
      mt.SetName(MM::g_Keyword_Elapsed_Time_ms);
      mt.SetValue(CDeviceUtils::ConvertToString(imd.timestamp.getMsec()));
      md.SetTag(mt);
      mt.SetName(MM::g_Keyword_Exposure);
      mt.SetValue(CDeviceUtils::ConvertToString(imd.exposureMs));
      md.SetTag(mt);
      return const_cast<unsigned char*>(pBuf->GetPixels());
   }
   else
      throw CMMError(getCoreErrorText(MMERR_CircularBufferEmpty).c_str(), MMERR_CircularBufferEmpty);
}

/**
 * Gets and removes the next image from the circular buffer.
 * Returns 0 if the buffer is empty.
 */
void* CMMCore::popNextImage() throw (CMMError)
{
   unsigned char* pBuf = const_cast<unsigned char*>(cbuf_->GetNextImage());
   if (pBuf != 0)
      return pBuf;
   else
      throw CMMError(getCoreErrorText(MMERR_CircularBufferEmpty).c_str(), MMERR_CircularBufferEmpty);
}

void* CMMCore::popNextImageMD(unsigned channel, unsigned slice, Metadata& md) throw (CMMError)
{
   const ImgBuffer* pBuf = cbuf_->GetNextImageBuffer(channel, slice);
   if (pBuf != 0)
   {
      md.Clear();
      const MM::ImageMetadata imd = pBuf->GetMetadata();
      MetadataSingleTag mt;
      mt.SetName(MM::g_Keyword_Elapsed_Time_ms);
      mt.SetValue(CDeviceUtils::ConvertToString(imd.timestamp.getMsec()));
      md.SetTag(mt);
      mt.SetName(MM::g_Keyword_Exposure);
      mt.SetValue(CDeviceUtils::ConvertToString(imd.exposureMs));
      md.SetTag(mt);
      return const_cast<unsigned char*>(pBuf->GetPixels());
   }
   else
      throw CMMError(getCoreErrorText(MMERR_CircularBufferEmpty).c_str(), MMERR_CircularBufferEmpty);
}

long CMMCore::snapImageMD() throw (CMMError)
{
   return 0;
}

void* CMMCore::getImageMD(long /*handle*/, unsigned /*channel*/, unsigned /*slice*/) throw (CMMError)
{
   return 0;
}

Metadata CMMCore::getImageMetadata(long /*handle*/, unsigned /*channel*/, unsigned /*slice*/) throw (CMMError)
{
   Metadata md;
   return md;
}


/**
 * Reserve memory for the circular buffer.
 */
void CMMCore::setCircularBufferMemoryFootprint(unsigned sizeMB) throw (CMMError)
{
   delete cbuf_; // discard old buffer
   cbuf_ = new CircularBuffer(sizeMB);

   // attempt to initialize based on the current camera settings
   if (camera_)
   {
      if (!cbuf_->Initialize(1, 1, camera_->GetImageWidth(), camera_->GetImageHeight(), camera_->GetImageBytesPerPixel()))
         throw CMMError(getCoreErrorText(MMERR_CircularBufferFailedToInitialize).c_str(), MMERR_CircularBufferFailedToInitialize);
   }

   CORE_DEBUG1("Circular buffer set to %d MB.\n", sizeMB);
}

long CMMCore::getRemainingImageCount()
{
   return cbuf_->GetRemainingImageCount();
}

long CMMCore::getBufferTotalCapacity()
{
   return cbuf_->GetSize();
}

long CMMCore::getBufferFreeCapacity()
{
   return cbuf_->GetFreeSize();
}

double CMMCore::getBufferIntervalMs() const
{
   return cbuf_->GetAverageIntervalMs();
}

bool CMMCore::isBufferOverflowed() const
{
   return cbuf_->Overflow();
}

/**
 * Returns the label of the currently selected camera device.
 * @return camera name
 */
string CMMCore::getCameraDevice()
{
   string deviceName;
   if (camera_)
      try {
         deviceName = pluginManager_.GetDeviceLabel(*camera_);
      }
      catch (CMMError& err)
      {
         // trap the error and ignore it in this case.
         // This happens only if the system is in the inconsistent internal state
         CORE_DEBUG1("Internal error: plugin manager does not recognize device reference. %s\n", err.getMsg().c_str());
      }
   return deviceName;
}

/**
 * Returns the label of the currently selected shutter device.
 * @return shutter name
 */
string CMMCore::getShutterDevice()
{
   string deviceName;
   if (shutter_)
      try {
         deviceName = pluginManager_.GetDeviceLabel(*shutter_);
      }
      catch (CMMError& err)
      {
         // trap the error and ignore it in this case.
         // This happens only if the system is in the inconistent internal state
         CORE_DEBUG1("Internal error: plugin manager does not recognize device reference. %s\n", err.getMsg().c_str());
      }
   return deviceName;
}

/**
 * Returns the label of the currently selected focus device.
 * @return focus stage name
 */
string CMMCore::getFocusDevice()
{
   string deviceName;
   if (focusStage_)
      try {
         deviceName = pluginManager_.GetDeviceLabel(*focusStage_);
      }
      catch (CMMError& err)
      {
         // trap the error and ignore it in this case.
         // This happens only if the system is in the inconistent internal state
         CORE_DEBUG1("Internal error: plugin manager does not recognize device reference. %s\n", err.getMsg().c_str());
      }
   return deviceName;
}

/**
 * Returns the label of the currently selected XYStage device.
 */
string CMMCore::getXYStageDevice()
{
   string deviceName;
   if (xyStage_)
      try {
         deviceName = pluginManager_.GetDeviceLabel(*xyStage_);
      }
      catch (CMMError& err)
      {
         // trap the error and ignore it in this case.
         // This happens only if the system is in the inconistent internal state
         CORE_DEBUG1("Internal error: plugin manager does not recognize device reference. %s\n", err.getMsg().c_str());
      }
   return deviceName;
}

/**
 * Returns the label of the currently selected auto-focus device.
 */
string CMMCore::getAutoFocusDevice()
{
   string deviceName;
   if (autoFocus_)
      try {
         deviceName = pluginManager_.GetDeviceLabel(*autoFocus_);
      }
      catch (CMMError& err)
      {
         // trap the error and ignore it in this case.
         // This happens only if the system is in the inconistent internal state
         CORE_DEBUG1("Internal error: plugin manager does not recognize device reference. %s\n", err.getMsg().c_str());
      }
   return deviceName;
}

/**
 * Sets the current auto-focus device.
 */
void CMMCore::setAutoFocusDevice(const char* autofocusLabel) throw (CMMError)
{
   if (autofocusLabel && strlen(autofocusLabel)>0)
   {
      autoFocus_ = getSpecificDevice<MM::AutoFocus>(autofocusLabel);
      CORE_LOG1("Auto-focus device set to %s\n", autofocusLabel);
   }
   else
   {
      autoFocus_ = 0;
      CORE_LOG("Auto-focus device removed.\n");
   }
   properties_->Refresh(); // TODO: more efficient
   stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreAutoFocus, getAutoFocusDevice().c_str()));
}

/**
 * Returns the label of the currently selected image processor device.
 */
string CMMCore::getImageProcessorDevice()
{
   string deviceName;
   if (imageProcessor_)
      try {
         deviceName = pluginManager_.GetDeviceLabel(*imageProcessor_);
      }
      catch (CMMError& err)
      {
         // trap the error and ignore it in this case.
         // This happens only if the system is in the inconistent internal state
         CORE_DEBUG1("Internal error: plugin manager does not recognize device reference. %s\n", err.getMsg().c_str());
      }
   return deviceName;
}

/**
 * Sets the current image processor device.
 */
void CMMCore::setImageProcessorDevice(const char* procLabel) throw (CMMError)
{
   if (procLabel && strlen(procLabel)>0)
   {
      imageProcessor_ = getSpecificDevice<MM::ImageProcessor>(procLabel);
      CORE_LOG1("Image processor device set to %s\n", procLabel);
   }
   else
   {
      imageProcessor_ = 0;
      CORE_LOG("Image processor device removed.\n");
   }
   properties_->Refresh(); // TODO: more efficient
   stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreImageProcessor, getImageProcessorDevice().c_str()));
}


/**
 * Sets the current shutter device.
 * @param shutter label
 */
void CMMCore::setShutterDevice(const char* shutterLabel) throw (CMMError)
{
   // Nothing to do if this is the current shutter device:
   if (getShutterDevice().compare(shutterLabel) == 0)
      return;

   // To avoid confusion close the current shutter:
   bool shutterOpen = false;
   if (shutter_) {
      shutterOpen = getShutterOpen();
      if (shutterOpen)
         setShutterOpen(false);
   }

   if (shutterLabel && strlen(shutterLabel)>0)
   {
      // Nothing to do if this is the current shutter device:
      if (getShutterDevice().compare(shutterLabel) == 0)
         return;

      shutter_ = getSpecificDevice<MM::Shutter>(shutterLabel);
      // if old shutter was open, open the new one
      if (shutterOpen)
         setShutterOpen(true);
      //assignImageSynchro(shutterLabel);
      CORE_LOG1("Shutter device set to %s\n", shutterLabel);
   }
   else
   {
      if (shutter_)
      {
         removeImageSynchro(pluginManager_.GetDeviceLabel(*shutter_).c_str());
      }
      shutter_ = 0;
      CORE_LOG("Shutter device removed.\n");
   }
   properties_->Refresh(); // TODO: more efficient
   stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreShutter, getShutterDevice().c_str()));
}

/**
 * Sets the current focus device.
 * @param focus stage label
 */
void CMMCore::setFocusDevice(const char* focusLabel) throw (CMMError)
{
   if (focusLabel && strlen(focusLabel)>0)
   {
      focusStage_ = getSpecificDevice<MM::Stage>(focusLabel);
      CORE_LOG1("Focus device set to %s\n", focusLabel);
   }
   else
   {
      focusStage_ = 0;
      CORE_LOG("Focus device removed.\n");
   }
   properties_->Refresh(); // TODO: more efficient
   stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreFocus, getFocusDevice().c_str()));
}

/**
 * Sets the current XY device.
 */
void CMMCore::setXYStageDevice(const char* xyDeviceLabel) throw (CMMError)
{
   if (xyDeviceLabel && strlen(xyDeviceLabel)>0)
   {
      xyStage_ = getSpecificDevice<MM::XYStage>(xyDeviceLabel);
      CORE_LOG1("XYStage device set to %s\n", xyDeviceLabel);
   }
   else
   {
      xyStage_ = 0;
      CORE_LOG("XYDevice device removed.\n");
   }
   stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreXYStage, getXYStageDevice().c_str()));
}

/**
 * Sets the current camera device.
 * @param camera label
 */
void CMMCore::setCameraDevice(const char* cameraLabel) throw (CMMError)
{
   if (cameraLabel && strlen(cameraLabel) > 0)
   {
      camera_ = getSpecificDevice<MM::Camera>(cameraLabel);
      CORE_LOG1("Camera device set to %s\n", cameraLabel);
   }
   else
   {
      camera_ = 0;
      CORE_LOG("Camera device removed.\n");
   }
   properties_->Refresh(); // TODO: more efficient
   stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, MM::g_Keyword_CoreCamera, getCameraDevice().c_str()));
}

/**
 * Returns all property names supported by the device.
 *
 * @return vector<string> property name array
 * @param const char* label device label
 */
vector<string> CMMCore::getDevicePropertyNames(const char* label) const throw (CMMError)
{
   vector<string> propList;

   // in case we requested Core device
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      propList = properties_->GetNames();
      return propList;
   }

   // regular devices
   try {
      MM::Device* pDevice = pluginManager_.GetDevice(label);
      for (unsigned i=0; i<pDevice->GetNumberOfProperties(); i++)
      {
         char Name[MM::MaxStrLength];
         pDevice->GetPropertyName(i, Name);
         propList.push_back(string(Name));
      }   
   }
   catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   return propList;
}

/**
 * Returns an array of labels for currently loaded devices.
 * @return vector<string> array of labels
 */
vector<string> CMMCore::getLoadedDevices() const
{
  vector<string> deviceList = pluginManager_.GetDeviceList();
  deviceList.push_back(MM::g_Keyword_CoreDevice);
  return deviceList;
}

/**
 * Returns an array of labels for currently loaded devices of specific type.
 * @param devType - device type identifier
 * @return vector<string> array of labels
 */
vector<string> CMMCore::getLoadedDevicesOfType(MM::DeviceType devType) const
{
   if (devType == MM::CoreDevice) {
      vector<string> coreDev;
      coreDev.push_back(MM::g_Keyword_CoreDevice);
      return coreDev;
   }

   return pluginManager_.GetDeviceList(devType);
}

/**
 * Returns all valid values for the specified property.
 * If the array is empty it means that there are no restrictions for values.
 * However, even if all values are allowed it is not guaranteed that all of them will be
 * acually accepted by the device at run time.
 *
 * @return vector<string> the array of values
 * @param const char* label device label
 * @param const std::string& propName property name
 */
std::vector<std::string> CMMCore::getAllowedPropertyValues(const char* label, const char* propName) const throw (CMMError)
{
   // in case we requested Core device
   std::vector<std::string> valueList;
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      valueList = properties_->GetAllowedValues(propName);
   }
   else
   {
      try {
         MM::Device* pDevice = pluginManager_.GetDevice(label);
         for (unsigned i=0; i<pDevice->GetNumberOfPropertyValues(propName); i++)
         {
            char value[MM::MaxStrLength];
            pDevice->GetPropertyValueAt(propName, i, value);
            string strVal(value);
            valueList.push_back(strVal);
         }
      } catch (CMMError& err) {
         err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
         throw;
      }
   }
   return valueList;
}

/**
 * Returns the property value for the specified device.

 * @return string property value
 * @param const char* label device label
 * @param const char* propName property name
 */
string CMMCore::getProperty(const char* label, const char* propName) const throw (CMMError)
{
   // in case we requested Core device
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return properties_->Get(propName);
   }

   MM::Device* pDevice;
   try {
      pDevice = pluginManager_.GetDevice(label);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   char value[MM::MaxStrLength];
   int nRet = pDevice->GetProperty(propName, value);
   if (nRet != DEVICE_OK)
      throw CMMError(label, getDeviceErrorText(nRet, pDevice).c_str(), MMERR_DEVICE_GENERIC);
   
   // uste the opportunity to update the cache
   /* TODO: it would be nice but actually we can't do that since this is a const method
   PropertySetting s(label, propName, value);
   stateCache_.addSetting(s);
   */

   return string(value);
}
/**
 * Changes the value of the device property.
 *
 * @return void 
 * @param const char* label device label
 * @param const char* propName property name
 * @param const char* propValue the new property value
 */
void CMMCore::setProperty(const char* label, const char* propName, 
                          const char* propValue) throw (CMMError)
{
   // check for forbiden characters
   string val(propValue);
   if (std::string::npos != val.find_first_of(MM::g_FieldDelimiters, 0))
      throw CMMError(label, getCoreErrorText(MMERR_InvalidContents).c_str(), MMERR_InvalidContents);

   // perform special processing for core initialization commands
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      properties_->Execute(propName, propValue);
      stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, propName, propValue));
   }
   else
   {
      MM::Device* pDevice;
      try {
         pDevice = pluginManager_.GetDevice(label);
      } catch (CMMError& err) {
         err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
         throw;
      }

      int nRet = pDevice->SetProperty(propName, propValue);
      if (nRet != DEVICE_OK)
         throw CMMError(label, getDeviceErrorText(nRet, pDevice).c_str(), MMERR_DEVICE_GENERIC);
      stateCache_.addSetting(PropertySetting(label, propName, propValue));
   }

   CORE_DEBUG3("Property set: device=%s, name=%s, value=%s\n", label, propName, propValue);
}

/**
 * Checks if device has a property with a specified name.
 * The exception will be thrown in case device label is not defined.
 */
bool CMMCore::hasProperty(const char* label, const char* propName) const throw (CMMError)
{
   // in case we requested Core device
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return properties_->Has(propName);
   }

   MM::Device* pDevice;
   try {
      pDevice = pluginManager_.GetDevice(label);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   return pDevice->HasProperty(propName);
}

/**
 * Tells us whether the property can be modified. 
 *
 * @return bool true for read-only property
 * @param const char* label device label
 * @param const char* propName property name
 */
bool CMMCore::isPropertyReadOnly(const char* label, const char* propName) const throw (CMMError)
{
   // in case we requested Core device
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return properties_->IsReadOnly(propName);
   }

   MM::Device* pDevice;
   try {
      pDevice = pluginManager_.GetDevice(label);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   bool bReadOnly;
   int nRet = pDevice->GetPropertyReadOnly(propName, bReadOnly);
   if (nRet != DEVICE_OK)
      throw CMMError(label, getDeviceErrorText(nRet, pDevice).c_str(), MMERR_DEVICE_GENERIC);

   return bReadOnly;
}

/**
 * Tells us whether the property must be defined prior to initialization. 
 *
 * @return bool true for pre-init property
 * @param const char* label device label
 * @param const char* propName property name
 */
bool CMMCore::isPropertyPreInit(const char* label, const char* propName) const throw (CMMError)
{
   // in case we requested Core device
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return false;
   }

   MM::Device* pDevice;
   try {
      pDevice = pluginManager_.GetDevice(label);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   bool preInit;
   int nRet = pDevice->GetPropertyInitStatus(propName, preInit);
   if (nRet != DEVICE_OK)
      throw CMMError(label, getDeviceErrorText(nRet, pDevice).c_str(), MMERR_DEVICE_GENERIC);

   return preInit;
}

/**
 * Returns the property lower limit value, if the property has limits - 0 otherwise.
 */
double CMMCore::getPropertyLowerLimit(const char* label, const char* propName) const throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return 0.0;
   }
   else
   {
      MM::Device* pDevice;
      try {
         pDevice = pluginManager_.GetDevice(label);
      } catch (CMMError& err) {
         err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
         throw;
      }
      double limit;
      int ret = pDevice->GetPropertyLowerLimit(propName, limit);
      if (ret != DEVICE_OK)
         throw CMMError(label, getDeviceErrorText(ret, pDevice).c_str(), MMERR_DEVICE_GENERIC);

      return limit;
   }
}

/**
 * Returns the property uper limit value, if the property has limits - 0 otherwise.
 */
double CMMCore::getPropertyUpperLimit(const char* label, const char* propName) const throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return 0.0;
   }
   else
   {
      MM::Device* pDevice;
      try {
         pDevice = pluginManager_.GetDevice(label);
      } catch (CMMError& err) {
         err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
         throw;
      }
      double limit;
      int ret = pDevice->GetPropertyUpperLimit(propName, limit);
      if (ret != DEVICE_OK)
         throw CMMError(label, getDeviceErrorText(ret, pDevice).c_str(), MMERR_DEVICE_GENERIC);

      return limit;
   }
}

/**
 * Queries device if the specific property has limits.
 */
bool CMMCore::hasPropertyLimits(const char* label, const char* propName) const throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      return false;
   }
   else
   {
      MM::Device* pDevice;
      try {
         pDevice = pluginManager_.GetDevice(label);
      } catch (CMMError& err) {
         err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
         throw;
      }

      bool hasLimits;
      int ret = pDevice->HasPropertyLimits(propName, hasLimits);
      if (ret != DEVICE_OK)
         throw CMMError(label, getDeviceErrorText(ret, pDevice).c_str(), MMERR_DEVICE_GENERIC);

      return hasLimits;
   }
}

/**
 * Returns the intrinsic property type.
 */
MM::PropertyType CMMCore::getPropertyType(const char* label, const char* propName) const throw (CMMError)
{
   if (strcmp(label, MM::g_Keyword_CoreDevice) == 0)
   {
      // TODO: return the proper core type
      return MM::Undef;
   }
   else
   {
      MM::Device* pDevice;
      try {
         pDevice = pluginManager_.GetDevice(label);
      } catch (CMMError& err) {
         err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
         throw;
      }
      MM::PropertyType pt;
      int ret = pDevice->GetPropertyType(propName, pt);
      if (ret != DEVICE_OK)
         throw CMMError(label, getDeviceErrorText(ret, pDevice).c_str(), MMERR_DEVICE_GENERIC);

      return pt;
   }
}


/**
 * Horizontal dimentsion of the image buffer in pixels.
 * @return unsigned X size
 */
unsigned CMMCore::getImageWidth() const
{
   if (camera_ == 0)
      return 0;

   return camera_->GetImageWidth();
}

/**
 * Vertical dimentsion of the image buffer in pixels.
 * @return unsigned Y size
 */
unsigned CMMCore::getImageHeight() const
{
   if (camera_ == 0)
      return 0;

   return camera_->GetImageHeight();
}

/**
 * How many bytes for each pixel. This value does not necessarily reflect the
 * capabilities of the particular camera A/D converter.
 * @return unsigned number of bytes
 */
unsigned CMMCore::getBytesPerPixel() const
{
   if (camera_ == 0)
      return 0;

   return camera_->GetImageBytesPerPixel();
}

/**
 * How many bits of dynamic range are to be expected from the camera. This value should
 * be used only as a guideline - it does not guarante that image buffer will contain
 * only values from the returned dynamic range.
 *
 * @return unsigned number of bits
 */
unsigned CMMCore::getImageBitDepth() const
{
   if (camera_)
      return camera_->GetBitDepth();
   return 0;
}

/**
 * Returns the number of instantenuous channels the default camera is returning.
 * For example color camera will return 3 channels on each snap.
 */
unsigned CMMCore::getNumberOfChannels() const
{
   if (camera_)
      return camera_->GetNumberOfChannels();

   return 0;
}

std::vector<std::string> CMMCore::getChannelNames() const
{
   vector<string> channelNames;
   if (camera_)
   {
      for (unsigned i=0; i<camera_->GetNumberOfChannels(); i++)
      {
         char name[MM::MaxStrLength];
         camera_->GetChannelName(i, name);
         channelNames.push_back(name);
      }
   }
   return channelNames;
}


/**
 * Sets the current exposure setting of the camera in milliseconds.
 * @param double dExp exposure in milliseconds
 */
void CMMCore::setExposure(double dExp) throw (CMMError)
{
   if (camera_)
   {
      camera_->SetExposure(dExp);
      if (camera_->HasProperty(MM::g_Keyword_Exposure))
      {
         char cameraName[MM::MaxStrLength];
         camera_->GetLabel(cameraName);
         stateCache_.addSetting(PropertySetting(cameraName, MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(dExp)));
      }
   }
   else
      throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);

   CORE_DEBUG1("Exposure set to %f.2 ms\n", dExp);
}

/**
 * Returns the current exposure setting of the camera in milliseconds.
 * @return double dExp exposure in milliseconds
 */
double CMMCore::getExposure() const throw (CMMError)
{
   if (camera_)
      return camera_->GetExposure();
   else
      //throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);
      return 0.0;
}

/**
 * Sets hardware Region Of Interest (ROI). This command will
 * change dimensions of the image.
 * 
 * @param int x coordinate of the top left corner
 * @param int y coordinate of the top left corner
 * @param int xSize horizontal dimension
 * @param int ySize vertical dimension
 */
void CMMCore::setROI(int x, int y, int xSize, int ySize) throw (CMMError)
{
   if (camera_)
   {
      int nRet = camera_->SetROI(x, y, xSize, ySize);
      if (nRet != DEVICE_OK)
         throw CMMError(getDeviceErrorText(nRet, camera_).c_str(), MMERR_DEVICE_GENERIC);
   }
   else
      throw CMMError(getCoreErrorText(MMERR_CameraNotAvailable).c_str(), MMERR_CameraNotAvailable);

   CORE_DEBUG4("ROI set to (%d, %d, %d, %d)\n", x, y, xSize, ySize);
}

/**
 * Returns info on the current Region Of Interest (ROI).
 * 
 * @param int x coordinate of the top left corner
 * @param int y coordinate of the top left corner
 * @param int xSize horizontal dimension
 * @param int ySize vertical dimension
 */
void CMMCore::getROI(int& x, int& y, int& xSize, int& ySize) const throw (CMMError)
{
   unsigned uX(0), uY(0), uXSize(0), uYSize(0);
   if (camera_)
   {
      int nRet = camera_->GetROI(uX, uY, uXSize, uYSize);
      if (nRet != DEVICE_OK)
         throw CMMError(getDeviceErrorText(nRet, camera_).c_str(), MMERR_DEVICE_GENERIC);
   }

   x = (int) uX;
   y = (int) uY;
   xSize = (int) uXSize;
   ySize = (int) uYSize;
}

/**
 * Resets the current ROI to the full frame.
 */
void CMMCore::clearROI() throw (CMMError)
{
   if (camera_)
   {
      // effectively clears the current ROI setting
      int nRet = camera_->ClearROI();
      if (nRet != DEVICE_OK)
         throw CMMError(getDeviceErrorText(nRet, camera_).c_str(), MMERR_DEVICE_GENERIC);
   }
}

/**
 * Sets the state (position) on the specific device. The command will fail if
 * the device does not support states.
 *
 * @param const char* deviceLabel device label
 * @param long state new state
 */
void CMMCore::setState(const char* deviceLabel, long state) throw (CMMError)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);

   int nRet = pStateDev->SetPosition(state);
   if (nRet != DEVICE_OK)
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);

   if (pStateDev->HasProperty(MM::g_Keyword_State))
   {
      stateCache_.addSetting(PropertySetting(deviceLabel, MM::g_Keyword_State, CDeviceUtils::ConvertToString(state)));
   }
   if (pStateDev->HasProperty(MM::g_Keyword_Label))
   {
      stateCache_.addSetting(PropertySetting(deviceLabel, MM::g_Keyword_Label, getStateLabel(deviceLabel).c_str()));
   }

   CORE_DEBUG2("%s set to state %d\n", deviceLabel, (int)state);
}

/**
 * Returns the current state (position) on the specific device. The command will fail if
 * the device does not support states.
 *
 * @return long current state
 * @param const char* deviceLabel device label
 */
long CMMCore::getState(const char* deviceLabel) const throw (CMMError)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);

   long state;
   int nRet = pStateDev->GetPosition(state);
   if (nRet != DEVICE_OK)
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);

   return state;
}

/**
 * Returns the total number of available positions (states).
 */
long CMMCore::getNumberOfStates(const char* deviceLabel)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);

   return pStateDev->GetNumberOfPositions();
}

/**
 * Sets device state using the previously assigned label (string).
 * 
 * @param const char* deviceLabel device label
 * @param const char* stateLabel state label
 */
void CMMCore::setStateLabel(const char* deviceLabel, const char* stateLabel) throw (CMMError)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);

   int nRet = pStateDev->SetPosition(stateLabel);
   if (nRet != DEVICE_OK)
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);

   if (pStateDev->HasProperty(MM::g_Keyword_Label))
   {
      stateCache_.addSetting(PropertySetting(deviceLabel, MM::g_Keyword_Label, stateLabel));
   }
   if (pStateDev->HasProperty(MM::g_Keyword_State))
   {
      stateCache_.addSetting(PropertySetting(deviceLabel, MM::g_Keyword_State,
                             CDeviceUtils::ConvertToString(getStateFromLabel(deviceLabel, stateLabel))));
   }

   CORE_DEBUG2("%s set to state label %s\n", deviceLabel, stateLabel);
}

/**
 * Returns the current state as the label (string).
 *
 * @return string state label 
 * @param const char* deviceLabel device label
 */
string CMMCore::getStateLabel(const char* deviceLabel) const throw (CMMError)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);

   char pos[MM::MaxStrLength];
   int nRet = pStateDev->GetPosition(pos);
   if (nRet != DEVICE_OK)
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);

   return string(pos);
}

/**
 * Defines a label for the specific state/
 *
 * @param const char* deviceLabel device label
 * @param long state state
 * @param const char* label assingned label
 */
void CMMCore::defineStateLabel(const char* deviceLabel, long state, const char* label) throw (CMMError)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);
   int nRet = pStateDev->SetPositionLabel(state, label);
   if (nRet != DEVICE_OK)
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);

   CORE_DEBUG3("State %d for device %s defined as label %s\n", (int)state, deviceLabel, label);
}

/**
 * Return labels for all states
 *
 * @return vector<string> an array of labels
 * @param const char* deviceLabel device label
 */
vector<string> CMMCore::getStateLabels(const char* deviceLabel) const throw (CMMError)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);
   vector<string> stateLabels;
   char label[MM::MaxStrLength];
   for (unsigned i=0; i<pStateDev->GetNumberOfPositions(); i++)
   {
      int nRet = pStateDev->GetPositionLabel(i, label);
      if (nRet != DEVICE_OK)
         throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);
      stateLabels.push_back(label);
   }
   return stateLabels;
}

/**
 * Obtain the state for a given label.
 *
 * @return long state 
 * @param const char* deviceLabel device label
 * @param const char* stateLabel state label
 */
long CMMCore::getStateFromLabel(const char* deviceLabel, const char* stateLabel) const throw (CMMError)
{
   MM::State* pStateDev = getSpecificDevice<MM::State>(deviceLabel);
   long state;
   int nRet = pStateDev->GetLabelPosition(stateLabel, state);
   if (nRet != DEVICE_OK)
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);

   return state;
}

/**
 * Defines a single configuration entry (setting). If the configuration name
 * was not previously defined a new configuration will be automatically created.
 * If the name was previously defined the new setting will be added to its list of
 * property settings. The new setting will override previously defined ones if it
 * refers to the same property name.
 *
 * @param const char* configName configuration name
 * @param const char* deviceLabel device label
 * @param const char* propName property name
 * @param const char* value property value
 */
void CMMCore::defineConfiguration(const char* configName, const char* deviceLabel, const char* propName, const char* value)
{
   // check if the configuration allready exists
   CConfigMap::const_iterator it = configs_.find(configName);
   Configuration* pConf;
   if (it == configs_.end())
   {
      pConf = new Configuration();
      configs_[configName] = pConf; // add new configuration
   }   
   else
      pConf = it->second;

   // add the setting
   PropertySetting setting(deviceLabel, propName, value);
   pConf->addSetting(setting);

   CORE_DEBUG4("Configuration %s: new setting for device %s defined as %s=%s\n", configName, deviceLabel, propName, value);
}

/**
 * Applies a configuration. The command will fail if the
 * configuration was not previously defined.
 *
 * @param const char* configName configuration name
 */
void CMMCore::setConfiguration(const char* configName) throw (CMMError)
{
   // tolerate empty configuration names
   if (strlen(configName) == 0)
      return;

   CConfigMap::const_iterator it = configs_.find(configName);
   if (it == configs_.end())
      throw CMMError(configName, getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   
   try {
      applyConfiguration(*it->second);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   CORE_DEBUG1("Configuration %s applied.\n", configName);
}

/**
 * Deletes a configuration. The command will fail if the
 * configuration was not previously defined.
 *
 * @param const char* configName configuration name
 */
void CMMCore::deleteConfiguration(const char* configName) throw (CMMError)
{
   // tolerate empty configuration names
   if (strlen(configName) == 0)
      return;

   CConfigMap::const_iterator it = configs_.find(configName);
   if (it == configs_.end())
      throw CMMError(configName, getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   configs_.erase(configName);
   CORE_LOG1("Configuration %s deleted.\n", configName);
}


/**
 * Returns all defined configuration names.
 * @return std::vector<string> an array of configuration names
 */
std::vector<string> CMMCore::getAvailableConfigurations() const
{
   vector<string> configList;
   CConfigMap::const_iterator it = configs_.begin();
   while(it != configs_.end())
      configList.push_back(it++->first);

   return configList;
}

/**
 * Returns the current configuration.
 * An empty string as a valide return value, since the system state will not
 * always correspond to any of the defined configurations.
 * Also, in general it is possible that the system state fits multiple configurations.
 * This method will return only the first maching configuration, if any.
 *
 * @return string configuration name
 */
string CMMCore::getConfiguration() const
{
   // Here we find all configurations that "fit" the current state of the system
   // but return only the first one.
   // Still not sure how to treat multiple configurations fitting the system state -
   // but for the time being we will assume that all configurations are unique and
   // therefore we need to return only one. In the future we may revert to returning
   // a vector of mathcing configurations...

   vector<string> configList;
   string empty("");

   CConfigMap::const_iterator it = configs_.begin();
   while(it != configs_.end())
   {
      if (isConfigurationCurrent(*it->second))
         configList.push_back(it->first);
      it++;
   }
   if (configList.empty())
      return empty;
   else
      return configList[0];
}

/**
 * Returns the configuration object for a given name.
 *
 * @return Configuration configuration object
 * @param const char* configName configuration name
 */
Configuration CMMCore::getConfigurationData(const char* configName) const throw (CMMError)
{
   CConfigMap::const_iterator it = configs_.find(configName);
   if (it == configs_.end())
      throw CMMError(configName, getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   return *it->second;
}

/**
 * Checks if the configuration already exists.
 *
 * @return true if the configuration is already defined
 * @param configName configuration name 
 */
bool CMMCore::isConfigurationDefined(const char* configName)
{
   CConfigMap::const_iterator it = configs_.find(configName);
   return it == configs_.end() ? false : true;
}

/**
 * Creates an empty configuration group.
 */
void CMMCore::defineConfigGroup(const char* groupName) throw (CMMError)
{
   if (!configGroups_->Define(groupName))
      throw CMMError(groupName, getCoreErrorText(MMERR_DuplicateConfigGroup).c_str(), MMERR_DuplicateConfigGroup);

   CORE_LOG1("Configuration group %s created.\n", groupName);
}

/**
 * Deletes an entire configuration group.
 */
void CMMCore::deleteConfigGroup(const char* groupName) throw (CMMError)
{
   if (!configGroups_->Delete(groupName))
      throw CMMError(groupName, getCoreErrorText(MMERR_NoConfigGroup).c_str(), MMERR_NoConfigGroup);
   CORE_LOG1("Configuration group %s deleted.\n", groupName);
}

/**
 * Defines a single configuration entry (setting). If the configuration group/name
 * was not previously defined a new configuration will be automatically created.
 * If the name was previously defined the new setting will be added to its list of
 * property settings. The new setting will override previously defined ones if it
 * refers to the same property name.
 *
 * @param groupName group name
 * @param configName configuration name
 * @param deviceLabel device label
 * @param propName property name
 * @param value property value
 */
void CMMCore::defineConfig(const char* groupName, const char* configName, const char* deviceLabel, const char* propName, const char* value)
{
   configGroups_->Define(groupName, configName, deviceLabel, propName, value);
   ostringstream txt;
   txt << groupName << "/" << configName;
   CORE_LOG4("Configuration %s: new setting for device %s defined as %s=%s\n", txt.str().c_str(), deviceLabel, propName, value);
}

/**
 * Defines a single pixel size entry (setting). 
 * The system will treat pixel size configurations very similar to configuration presets,
 * i.e. it will try to detect if any of the pixel size presets matches the current state of
 * the system.
 * If the pixel size was previously defined the new setting will be added to its list of
 * property settings. The new setting will override previously defined ones if it
 * refers to the same property name.
 *
 * @param resolutionID identifier for one unique property setting
 * @param deviceLabel device label
 * @param propName property name
 * @param value property value
*/
void CMMCore::definePixelSizeConfig(const char* resolutionID, const char* deviceLabel, const char* propName, const char* value)
{
   pixelSizeGroup_->Define(resolutionID, deviceLabel, propName, value);
   CORE_LOG4("Resolution ID %s: for device %s defined as %s=%s\n", resolutionID, deviceLabel, propName, value);
}

/**
 * Checks if the Pixel Size Resolution already exists
 *
 * @return true if the configuration is already defined
 */
bool CMMCore::isPixelSizeConfigDefined(const char* resolutionID) const
{
   return  pixelSizeGroup_->Find(resolutionID) != 0;
}

/**
 * Sets pixel size in microns for the specified resolution sensing configuration preset.
 */ 
void CMMCore::setPixelSizeUm(const char* resolutionID, double pixSize)  throw (CMMError)
{
   PixelSizeConfiguration* psc = pixelSizeGroup_->Find(resolutionID);
   if (psc == 0)
      throw CMMError(resolutionID, getCoreErrorText(MMERR_NoConfigGroup).c_str(), MMERR_NoConfigGroup);
   psc->setPixelSizeUm(pixSize);
   CORE_LOG2("Pixel size %f um set for resolution ID: %s\n", pixSize, resolutionID);
}

/**
 * Applies a Pixel Size Configurdation. The command will fail if the
 * configuration was not previously defined.
 * 
 * @param groupName
 * @param configName
 */
void CMMCore::setPixelSizeConfig(const char* resolutionID) throw (CMMError)
{
   PixelSizeConfiguration* psc = pixelSizeGroup_->Find(resolutionID);
   ostringstream os;
   os << resolutionID;
   if (!psc)
   {
      throw CMMError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   }
   
   try {
      applyConfiguration(*psc);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      logError("setPixelSizeConfig", getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   CORE_DEBUG1("Pixel Size Configuration %s applied.\n", os.str().c_str());
}

/**
 * Applies a configuration to a group. The command will fail if the
 * configuration was not previously defined.
 * 
 * @param groupName
 * @param configName
 */
void CMMCore::setConfig(const char* groupName, const char* configName) throw (CMMError)
{
   Configuration* pCfg = configGroups_->Find(groupName, configName);
   ostringstream os;
   os << groupName << "/" << configName;
   if (!pCfg)
   {
      throw CMMError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   }
   
   try {
      applyConfiguration(*pCfg);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      logError("setConfig", getCoreErrorText(err.getCode()).c_str());
      throw;
   }

   CORE_DEBUG1("Configuration %s applied.\n", os.str().c_str());
}

/**
 * Deletes a configuration from a group. The command will fail if the
 * configuration was not previously defined.
 *
 */
void CMMCore::deleteConfig(const char* groupName, const char* configName) throw (CMMError)
{
   ostringstream os;
   os << groupName << "/" << configName;
   if (!configGroups_->Delete(groupName, configName)) {
      logError("deleteConfig", getCoreErrorText(MMERR_NoConfiguration).c_str());
      throw CMMError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   }
   CORE_LOG1("Configuration %s deleted.\n", os.str().c_str());
}

/**
 * Returns all defined configuration names in a given group
 * @return std::vector<string> an array of configuration names
 */
vector<string> CMMCore::getAvailableConfigs(const char* group) const
{
   return configGroups_->GetAvailableConfigs(group);
}

vector<string> CMMCore::getAvailableConfigGroups() const
{
   return configGroups_->GetAvailableGroups();
}

/**
 * Returns all defined resolution preset names
 * @return std::vector<string> an array of resolution presets
 */
vector<string> CMMCore::getAvailablePixelSizeConfigs() const
{
   return pixelSizeGroup_->GetAvailable();
}

/**
 * Returns the current configuration for a given group.
 * An empty string as a valide return value, since the system state will not
 * always correspond to any of the defined configurations.
 * Also, in general it is possible that the system state fits multiple configurations.
 * This method will return only the first maching configuration, if any.
 *
 * @return string configuration name
 */
string CMMCore::getCurrentConfig(const char* groupName) const throw (CMMError)
{
   string empty("");
   vector<string> cfgs = configGroups_->GetAvailableConfigs(groupName);
   if (cfgs.empty())
      return empty;

   Configuration curState = getConfigGroupState(groupName);

   for (size_t i=0; i<cfgs.size(); i++)
   {
      Configuration* pCfg = configGroups_->Find(groupName, cfgs[i].c_str());
      if (pCfg && curState.isConfigurationIncluded(*pCfg))
         return cfgs[i];
   }

   // no match
   return empty;
}

/**
 * Returns the configuration object for a given group and name.
 *
 * @return Configuration configuration object
 */
Configuration CMMCore::getConfigData(const char* groupName, const char* configName) const throw (CMMError)
{
   Configuration* pCfg = configGroups_->Find(groupName, configName);
   if (!pCfg)
   {
      // not found
      ostringstream os;
      os << groupName << "/" << configName;
      logError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str());
      throw CMMError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   }
   return *pCfg;
}

/**
 * Returns the configuration object for a give pixel size preset.
 * @return Configuration configuration object
 */
Configuration CMMCore::getPixelSizeConfigData(const char* configName) const throw (CMMError)
{
   Configuration* pCfg = pixelSizeGroup_->Find(configName);
   if (!pCfg)
   {
      // not found
      ostringstream os;
      os << "Pixel size" << "/" << configName;
      logError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str());
      throw CMMError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   }
   return *pCfg;
}

/**
 * Deletes a pixel size configuration. The command will fail if the
 * configuration was not previously defined.
 *
 */
void CMMCore::deletePixelSizeConfig(const char* configName) const throw (CMMError)
{
   ostringstream os;
   os << "Pixel size" << "/" << configName;
   if (!pixelSizeGroup_->Delete(configName)) {
      logError("deletePixelSizeConfig", getCoreErrorText(MMERR_NoConfiguration).c_str());
      throw CMMError(os.str().c_str(), getCoreErrorText(MMERR_NoConfiguration).c_str(), MMERR_NoConfiguration);
   }
   CORE_LOG1("Pixel Size Configuration %s deleted.\n", os.str().c_str());
}

/**
 * Returns the curent pixel size in microns.
 * This method is based on sensing the current pixel size configuration and adjusting
 * for the binning.
 */
double CMMCore::getPixelSizeUm() const
{
   // get a list of configuration names
   vector<string> cfgs = pixelSizeGroup_->GetAvailable();
   if (cfgs.empty())
      return 0.0;

   // create a union of configuration settings used in this group
   // and obtain the current state of the system
   Configuration curState;
   for (size_t i=0; i<cfgs.size(); i++) {
      PixelSizeConfiguration* cfgData = pixelSizeGroup_->Find(cfgs[i].c_str());
      assert(cfgData);
      for (size_t i=0; i < cfgData->size(); i++)
      {
         PropertySetting cs = cfgData->getSetting(i); // config setting
         if (!curState.isPropertyIncluded(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str()))
         {
            try
            {
               string value = getProperty(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str());
               PropertySetting ss(cs.getDeviceLabel().c_str(), cs.getPropertyName().c_str(), value.c_str()); // state setting
               curState.addSetting(ss);
            }
            catch (CMMError& err)
            {
               // just log error
               logError("GetPixelSizeUm", err.getMsg().c_str());
            }
         }
      }
   }

   // check which one matches the current state
   for (size_t i=0; i<cfgs.size(); i++)
   {
      PixelSizeConfiguration* pCfg = pixelSizeGroup_->Find(cfgs[i].c_str());
      if (pCfg && curState.isConfigurationIncluded(*pCfg))
      {
         double pixSize = pCfg->getPixelSizeUm();
         if (camera_)
         {
            pixSize *= camera_->GetBinning() / getMagnificationFactor();
         }
         return pixSize;
      }
   }

   return 0.0;
}

/**
 * Returns the pixel size in um for the requested pixel size group
 */
double CMMCore::getPixelSizeUm(const char* resolutionID) throw (CMMError)
{
   PixelSizeConfiguration* psc = pixelSizeGroup_->Find(resolutionID);
   if (psc == 0)
      throw CMMError(resolutionID, getCoreErrorText(MMERR_NoConfigGroup).c_str(), MMERR_NoConfigGroup);
   return psc->getPixelSizeUm();
}


/**
 * Returns the product of all Magnifiers in the system or 1.0 when none is found
 * This is used internally by GetPixelSizeUm (NS, 5/24/08)
 */
double CMMCore::getMagnificationFactor() const
{
   double magnification = 1.0;
   vector<string> magnifiers = getLoadedDevicesOfType(MM::MagnifierDevice);
   for (size_t i=0; i<magnifiers.size(); i++)
   {
      try
      {
         magnification *= getSpecificDevice<MM::Magnifier>(magnifiers[i].c_str())->GetMagnification();
      }
      catch (CMMError e)
      {
         assert(!"Internal error in generating a list of specific devices");
      }
   }
   return magnification;
}

/**
 * Checks if the configuration already exists within a group.
 *
 * @return true if the configuration is already defined
 */
bool CMMCore::isConfigDefined(const char* groupName, const char* configName)
{
   return  configGroups_->Find(groupName, configName) != 0;
}
/**
 * Checks if the group already exists.
 *
 * @return true if the group is already defined
 */
bool CMMCore::isGroupDefined(const char* groupName)
{
   return  configGroups_->isDefined(groupName);
}

/**
 * Defines a reference for the collection of property-value pairs.
 * This construct is useful for defining
 * interchangeable equipment features, such as objective magnifications, filter wavelengths, etc.
 */
void CMMCore::definePropertyBlock(const char* blockName, const char* propertyName, const char* propertyValue)
{
   // check if the block allready exists
   CPropBlockMap::const_iterator it = propBlocks_.find(blockName);
   PropertyBlock* pBlock;
   if (it == propBlocks_.end())
   {
      pBlock = new PropertyBlock();
      propBlocks_[blockName] = pBlock; // add new block
   }   
   else
      pBlock = it->second;

   // add the pair
   PropertyPair pair(propertyName, propertyValue);
   pBlock->addPair(pair);

   CORE_DEBUG3("Property block %s defined as %s=%s\n", blockName, propertyName, propertyValue);
}

/**
 * Returns all defined property block identifiers.
 */
std::vector<std::string> CMMCore::getAvailablePropertyBlocks() const
{
   vector<string> blkList;
   CPropBlockMap::const_iterator it = propBlocks_.begin();
   while(it != propBlocks_.end())
      blkList.push_back(it++->first);

   return blkList;
}

/**
 * Returns the collection of property-value pairs defined in this block.
 */
PropertyBlock CMMCore::getPropertyBlockData(const char* blockName) const
{
   CPropBlockMap::const_iterator it = propBlocks_.find(blockName);
   if (it == propBlocks_.end())
   {
      logError(blockName, getCoreErrorText(MMERR_InvalidPropertyBlock).c_str());
      throw CMMError(blockName, getCoreErrorText(MMERR_InvalidPropertyBlock).c_str(), MMERR_InvalidPropertyBlock);
   }
   return *it->second;
}

/**
 * Returns the collection of property-value pairs defined for the specific device and state label.
 */
PropertyBlock CMMCore::getStateLabelData(const char* deviceLabel, const char* stateLabel) const
{
   MM::Device* pDevice;
   try {
      pDevice = pluginManager_.GetDevice(deviceLabel);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      logError(deviceLabel, err.getMsg().c_str());
      throw;
   }

   if (pDevice->GetType() != MM::StateDevice)
   {
      logError(deviceLabel, getCoreErrorText(MMERR_InvalidStateDevice).c_str());
      throw CMMError(deviceLabel, getCoreErrorText(MMERR_InvalidStateDevice).c_str(), MMERR_InvalidStateDevice);
   }

   MM::State* pStateDev = static_cast<MM::State*>(pDevice);

   // check if corresponding label exists
   long pos;
   int nRet = pStateDev->GetLabelPosition(stateLabel, pos);
   if (nRet != DEVICE_OK)
   {
      logError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str());
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);
   }

   PropertyBlock blk;
   try {
      blk = getPropertyBlockData(stateLabel);
   } catch (...) {
      ;
      // if getting data did not succeed for any reason we will assume
      // that there is no connection between state label and property block.
      // In this context it is not an error, we'll just say there is no data. 
   }
   return blk;
}

/**
 * Returns the collection of property-value pairs defined for the current state.
 */
PropertyBlock CMMCore::getData(const char* deviceLabel) const
{
   // here we could have written simply: 
   // return getStateLabelData(deviceLabel, getStateLabel(deviceLabel).c_str());
   // but that would be inefficient beacuse of the multiple index lookup, so we'll
   // do it explicitely:

   // find the device
   MM::Device* pDevice;
   try {
      pDevice = pluginManager_.GetDevice(deviceLabel);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      logError(deviceLabel, err.getMsg().c_str());
      throw;
   }

   if (pDevice->GetType() != MM::StateDevice)
   {
      logError(deviceLabel, getCoreErrorText(MMERR_InvalidStateDevice).c_str());
      throw CMMError(deviceLabel, getCoreErrorText(MMERR_InvalidStateDevice).c_str(), MMERR_InvalidStateDevice);
   }

   MM::State* pStateDev = static_cast<MM::State*>(pDevice);

   // obatin the current state label
   char pos[MM::MaxStrLength];
   int nRet = pStateDev->GetPosition(pos);
   if (nRet != DEVICE_OK)
   {
      logError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str());
      throw CMMError(deviceLabel, getDeviceErrorText(nRet, pStateDev).c_str(), MMERR_DEVICE_GENERIC);
   }

   PropertyBlock blk;
   try {
      blk = getPropertyBlockData(pos);
   } catch (...) {
      ;
      // not an error here - there is just no data for this entry. 
   }
   return blk;
}

/**
 * Send string to the serial device and return an answer.
 * This command blocks until it recives an answer fromt he device terminated by the specified
 * sequence.
 */
void CMMCore::setSerialPortCommand(const char* name, const char* command, const char* term) throw (CMMError)
{
   MM::Serial* pSerial = getSpecificDevice<MM::Serial>(name);
   int ret = pSerial->SetCommand(command, term);
   if (ret != DEVICE_OK)
   {
      logError(name, getDeviceErrorText(ret, pSerial).c_str());
      throw CMMError(name, getDeviceErrorText(ret, pSerial).c_str(), MMERR_DEVICE_GENERIC);
   }
}

/**
 * Contouously read from the serial port until the terminating sequence is encountered.
 */
std::string CMMCore::getSerialPortAnswer(const char* name, const char* term) throw (CMMError) 
{
   MM::Serial* pSerial = getSpecificDevice<MM::Serial>(name);

   const int bufLen = 1024;
   char answerBuf[bufLen];
   int ret = pSerial->GetAnswer(answerBuf, bufLen, term);
   if (ret != DEVICE_OK)
   {
      string errText = getDeviceErrorText(ret, pSerial).c_str();
      logError(name, errText.c_str());
      throw CMMError(name, errText.c_str(), MMERR_DEVICE_GENERIC);
   }

   return string(answerBuf);
}

/**
 * Sends an array of characters to the serial port and returns immediately.
 */
void CMMCore::writeToSerialPort(const char* name, const std::vector<char> &data) throw (CMMError)
{
   MM::Serial* pSerial = getSpecificDevice<MM::Serial>(name);
   int ret = pSerial->Write((unsigned char*)(&(data[0])), (unsigned long)data.size());
   if (ret != DEVICE_OK)
   {
      logError(name, getDeviceErrorText(ret, pSerial).c_str());
      throw CMMError(name, getDeviceErrorText(ret, pSerial).c_str(), MMERR_DEVICE_GENERIC);
   }
}

/**
 * Reads the contents of the Rx buffer.
 */
vector<char> CMMCore::readFromSerialPort(const char* name) throw (CMMError)
{
   MM::Serial* pSerial = getSpecificDevice<MM::Serial>(name);

   const int bufLen = 1024; // internal chunk size limit
   unsigned char answerBuf[bufLen];
   unsigned long read;
   int ret = pSerial->Read(answerBuf, bufLen, read);
   if (ret != DEVICE_OK)
   {
      logError(name, getDeviceErrorText(ret, pSerial).c_str());
      throw CMMError(name, getDeviceErrorText(ret, pSerial).c_str(), MMERR_DEVICE_GENERIC);
   }

   vector<char> data;
   data.resize(read, 0);
   memcpy(&(data[0]), answerBuf, read);

   return data;
}

/**
 * Saves the current system state to a text file of the MM specific format.
 * The file records only read-write properties.
 * The file format is directly readable by the complementary loadSystemState() command.
 */
void CMMCore::saveSystemState(const char* fileName) throw (CMMError)
{
   ofstream os;
   os.open(fileName, ios_base::out | ios_base::trunc);
   if (!os.is_open())
   {
      logError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str());
      throw CMMError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str(), MMERR_FileOpenFailed);
   }

   // save system state
   Configuration config = getSystemState();
   for (size_t i=0; i<config.size(); i++)
   {
      PropertySetting s = config.getSetting(i);
      if (!isPropertyReadOnly(s.getDeviceLabel().c_str(), s.getPropertyName().c_str()))
      {
         os << MM::g_CFGCommand_Property << ',' << s.getDeviceLabel()
            << ',' << s.getPropertyName() << ',' << s.getPropertyValue() << endl;
      }
   }
}

/**
 * Loads the system configuration from the text file conforming to the MM specific format.
 * The configuration contains a list of commands to build the desired system state from
 * read-write properties.
 *
 * Format specification: the same as in loadSystemConfiguration() command
 */
void CMMCore::loadSystemState(const char* fileName) throw (CMMError)
{
   ifstream is;
   is.open(fileName, ios_base::in);
   if (!is.is_open())
   {
      logError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str());
      throw CMMError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str(), MMERR_FileOpenFailed);
   }

   // Process commands
   const int maxLineLength = 4 * MM::MaxStrLength + 4; // accomodate up to 4 strings and delimiters
   char line[maxLineLength+1];
   vector<string> tokens;
   while(is.getline(line, maxLineLength, '\n'))
   {
      // strip a potential Windows/dos CR
      istringstream il(line);
      il.getline(line, maxLineLength, '\r');
      if (strlen(line) > 0)
      {
         if (line[0] == '#')
         {
            // comment, so skip processing
            continue;
         }

         // parse tokens
         tokens.clear();
         CDeviceUtils::Tokenize(line, tokens, MM::g_FieldDelimiters);

         // non-empty and non-comment lines mush have at least one token
         if (tokens.size() < 1)
            throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
            
         if(tokens[0].compare(MM::g_CFGCommand_Property) == 0)
         {
            // set property command
            // --------------------
            if (tokens.size() != 4)
               // invalid format
               throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
            try
            {          
               // apply the command
               setProperty(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str());
            }
            catch (CMMError& err)
            {
               // log the failure but continue parsing
               CORE_LOG3("Property setting failed: %s-%s-%s\n",
                         tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str());
               CORE_LOG1("%s\n", err.getMsg().c_str());
            }
         }
      }
   }
}


/**
 * Saves the current system configuration to a text file of the MM specific format.
 * The configuration file records only the information essential to the hardware
 * setup: devices, labels, equipment pre-initialization properties, and configurations. 
 * The file format is the same as for the system state.
 */
void CMMCore::saveSystemConfiguration(const char* fileName) throw (CMMError)
{
   ofstream os;
   os.open(fileName, ios_base::out | ios_base::trunc);
   if (!os.is_open())
   {
      logError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str());
      throw CMMError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str(), MMERR_FileOpenFailed);
   }

   // insert the system reset command
   // this will unload all current devices
   os << "# Unload all devices" << endl; 
   os << "Property,Core,Initialize,0" << endl;

   // save device list
   os << "# Load devices" << endl;
   vector<string> devices = pluginManager_.GetDeviceList();
   vector<string>::const_iterator it;
   for (it=devices.begin(); it != devices.end(); it++)
   {
      MM::Device* pDev = pluginManager_.GetDevice((*it).c_str());
      char deviceName[MM::MaxStrLength] = "";
      char moduleName[MM::MaxStrLength] = "";
      pDev->GetName(deviceName);
      pDev->GetModuleName(moduleName);
      os << MM::g_CFGCommand_Device << "," << *it << "," << moduleName << "," << deviceName << endl; 
   }

   // save equipment
   os << "# Equipment attributes" << endl;
   vector<string> propBlocks = getAvailablePropertyBlocks();
   for (size_t i=0; i<propBlocks.size(); i++)
   {
      PropertyBlock pb = getPropertyBlockData(propBlocks[i].c_str());
      PropertyPair p;
      for (size_t j=0; j<pb.size(); j++)
      {
         p = pb.getPair(j);
         os << MM::g_CFGCommand_Equipment << ',' << propBlocks[i] << ',' << p.getPropertyName() << ',' << p.getPropertyValue() << endl;
      }
   }

   // save the pre-initlization properties
   os << "# Pre-initialization properties" << endl;
   Configuration config = getSystemState();
   for (size_t i=0; i<config.size(); i++)
   {
      PropertySetting s = config.getSetting(i);

      // check if the property must be set before initialization
      MM::Device* pDevice = pluginManager_.GetDevice(s.getDeviceLabel().c_str());
      if (pDevice)
      {
         bool initStatus = false;
         pDevice->GetPropertyInitStatus(s.getPropertyName().c_str(), initStatus);
         if (initStatus)
         {
            os << MM::g_CFGCommand_Property << ',' << s.getDeviceLabel()
               << ',' << s.getPropertyName() << ',' << s.getPropertyValue() << endl;
         }
      }
   }

   // insert the initialize command
   os << "Property,Core,Initialize,1" << endl;

   // save delays
   os << "# Delays" << endl;
   for (it=devices.begin(); it != devices.end(); it++)
   {
      MM::Device* pDev = pluginManager_.GetDevice((*it).c_str());
      if (pDev->GetDelayMs() > 0.0)
         os << MM::g_CFGCommand_Delay << "," << *it << "," << pDev->GetDelayMs() << endl; 
   }

   // save labels
   os << "# Labels" << endl;
   vector<string> deviceLabels = pluginManager_.GetDeviceList(MM::StateDevice);
   for (size_t i=0; i<deviceLabels.size(); i++)
   {
      MM::State* pSD = getSpecificDevice<MM::State>(deviceLabels[i].c_str());
      unsigned numPos = pSD->GetNumberOfPositions();
      for (unsigned long j=0; j<numPos; j++)
      {
         char lbl[MM::MaxStrLength];
         pSD->GetPositionLabel(j, lbl);
         os << MM::g_CFGCommand_Label << ',' << deviceLabels[i] << ',' << j << ',' << lbl << endl;
      }
   }

   // save global configurations
   os << "# Global configurations" << endl;
   vector<string> configs = getAvailableConfigurations();
   for (size_t i=0; i<configs.size(); i++)
   {
      Configuration c = getConfigurationData(configs[i].c_str());
      for (size_t j=0; j<c.size(); j++)
      {
         PropertySetting s = c.getSetting(j);
         os << MM::g_CFGCommand_Configuration << ',' << configs[i] << ',' << s.getDeviceLabel() << ',' << s.getPropertyName() << ',' << s.getPropertyValue() << endl;
      }
   }

   // save configuration groups
   os << "# Group configurations" << endl;
   vector<string> groups = getAvailableConfigGroups();
   for (size_t i=0; i<groups.size(); i++)
   {
      // empty group record
      vector<string> configs = getAvailableConfigs(groups[i].c_str());
      if (configs.size() == 0)
            os << MM::g_CFGCommand_ConfigGroup << ',' << groups[i] << endl;

      // normal group records
      for (size_t j=0; j<configs.size(); j++)
      {
         Configuration c = getConfigData(groups[i].c_str(), configs[j].c_str());
         for (size_t k=0; k<c.size(); k++)
         {
            PropertySetting s = c.getSetting(k);
            os << MM::g_CFGCommand_ConfigGroup << ',' << groups[i] << ','
               << configs[j] << ',' << s.getDeviceLabel() << ',' << s.getPropertyName() << ',' << s.getPropertyValue() << endl;
         }
      }
   }

   // save device roles
   os << "# Roles" << endl;
   if (camera_)
   {
      os << MM::g_CFGCommand_Property << ',' << MM::g_Keyword_CoreDevice << ',' << MM::g_Keyword_CoreCamera << ',' << getCameraDevice() << endl; 
   }
   if (shutter_)
   {
      os << MM::g_CFGCommand_Property << ',' << MM::g_Keyword_CoreDevice << ',' << MM::g_Keyword_CoreShutter << ',' << getShutterDevice() << endl; 
   }
   if (focusStage_)
   {
      os << MM::g_CFGCommand_Property << ',' << MM::g_Keyword_CoreDevice << ',' << MM::g_Keyword_CoreFocus << ',' << getFocusDevice() << endl; 
   }
}

/**
 * Loads the system configuration from the text file conforming to the MM specific format.
 * The configuration contains a list of commands to build the desired system state:
 * devices, labels, equipment, properties, and configurations.
 *
 * Format specification:
 * Each line consists of a number of string fields separated by "," (comma) characters.
 * Lines beggining with "#" are ignored (can be used for comments).
 * Each line in the file will be parsed by the system and as a result a corresponding command 
 * will be immediately extecuted.
 * The first field in the line always specifies the command from the following set of values:
 *    Device - executes loadDevice()
 *    Label - executes defineStateLabel() command
 *    Equipment - executes definePropertyBlockCommand()
 *    Property - executes setPropertyCommand()
 *    Configuration - executes defineConfiguration()
 *  
 * The remaining fields in the line will be used for corresponding command parameters.
 * The number of parameters depends on the actual command used.
 * 
 */
void CMMCore::loadSystemConfiguration(const char* fileName) throw (CMMError)
{
   ifstream is;
   is.open(fileName, ios_base::in);
   if (!is.is_open())
   {
      logError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str());
      throw CMMError(fileName, getCoreErrorText(MMERR_FileOpenFailed).c_str(), MMERR_FileOpenFailed);
   }

   // Process commands
   const int maxLineLength = 4 * MM::MaxStrLength + 4; // accomodate up to 4 strings and delimiters
   char line[maxLineLength+1];
   vector<string> tokens;

   const int errorLimit = 100; // errors allowed before aborting the load
   ostringstream summaryErrorText;
   int errorCount = 0;
   int lineCount = 0;

   while(is.getline(line, maxLineLength, '\n'))
   {
      // strip a potential Windows/dos CR
      istringstream il(line);
      il.getline(line, maxLineLength, '\r');

      lineCount++;
      if (strlen(line) > 0)
      {
         if (line[0] == '#')
         {
            // comment, so skip processing
            continue;
         }

         // parse tokens
         tokens.clear();
         CDeviceUtils::Tokenize(line, tokens, MM::g_FieldDelimiters);

         try
         {

            // non-empty and non-comment lines mush have at least one token
            if (tokens.size() < 1)
               throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
               
            if(tokens[0].compare(MM::g_CFGCommand_Device) == 0)
            {
               // load device command
               // -------------------
               if (tokens.size() != 4)
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
               loadDevice(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str());
            }
            else if(tokens[0].compare(MM::g_CFGCommand_Property) == 0)
            {
               // set property command
               // --------------------
               if (tokens.size() == 4)
                  setProperty(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str());
               else if (tokens.size() == 3)
                  // ...assuming here that the last missing toke represents an empty string
                  setProperty(tokens[1].c_str(), tokens[2].c_str(), "");
               else
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
            }
            else if(tokens[0].compare(MM::g_CFGCommand_Delay) == 0)
            {
               // set delay command
               // -----------------
               if (tokens.size() != 3)
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
               setDeviceDelayMs(tokens[1].c_str(), atof(tokens[2].c_str()));
            }
            else if(tokens[0].compare(MM::g_CFGCommand_Label) == 0)
            {
               // define label command
               // --------------------
               if (tokens.size() != 4)
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
               defineStateLabel(tokens[1].c_str(), atol(tokens[2].c_str()), tokens[3].c_str());
            }
            else if(tokens[0].compare(MM::g_CFGCommand_Configuration) == 0)
            {
               // define configuration command
               // ----------------------------
               if (tokens.size() != 5)
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
               defineConfiguration(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str(), tokens[4].c_str());
               CORE_LOG1("Obsolete command %s used in configuration file.\n", MM::g_CFGCommand_Configuration);
            }
            else if(tokens[0].compare(MM::g_CFGCommand_ConfigGroup) == 0)
            {
               // define grouped configuration command
               // ------------------------------------
               if (tokens.size() == 6)
                  defineConfig(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str(), tokens[4].c_str(), tokens[5].c_str());
               else if (tokens.size() == 5)
               {
                  // we will assume here that the last (missing) token is representing an empty string
                  defineConfig(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str(), tokens[4].c_str(), "");
               }
               else if (tokens.size() == 2)
                  defineConfigGroup(tokens[1].c_str());
               else
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
            }
            else if(tokens[0].compare(MM::g_CFGCommand_ConfigPixelSize) == 0)
            {
               // define pxiel size configuration command
               // ---------------------------------------
               if (tokens.size() == 5)
                  definePixelSizeConfig(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str(), tokens[4].c_str());
               else
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
            }
            else if(tokens[0].compare(MM::g_CFGCommand_PixelSize_um) == 0)
            {
               // set pixel size
               // --------------
               if (tokens.size() == 3)
                  setPixelSizeUm(tokens[1].c_str(), atof(tokens[2].c_str()));
               else
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
            }
            else if(tokens[0].compare(MM::g_CFGCommand_Equipment) == 0)
            {
               // define configuration command
               // ----------------------------
               if (tokens.size() != 4)
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
               definePropertyBlock(tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str());
            }
            else if(tokens[0].compare(MM::g_CFGCommand_ImageSynchro) == 0)
            {
               // define image sycnhro
               // --------------------
               if (tokens.size() != 2)
                  throw CMMError(line, getCoreErrorText(MMERR_InvalidCFGEntry).c_str(), MMERR_InvalidCFGEntry);
               assignImageSynchro(tokens[1].c_str());
            }

         }
         catch (CMMError& err)
         {
            summaryErrorText << "Line " << lineCount << ": " << line << endl;
            summaryErrorText << err.getMsg() << endl << endl;
            errorCount++;
            if (errorCount >= errorLimit)
            {
               summaryErrorText << "Too many errors. Loading stopped.";
               throw CMMError(summaryErrorText.str().c_str(), MMERR_InvalidConfigurationFile);
            }
         }
      }
   }

   if (errorCount > 0)
   {
      throw CMMError(summaryErrorText.str().c_str(), MMERR_InvalidConfigurationFile);
   }

   // file parsing finished, try to set startup configuration
   if (isConfigDefined(MM::g_CFGGroup_System, MM::g_CFGGroup_System_Startup))
      this->setConfig(MM::g_CFGGroup_System, MM::g_CFGGroup_System_Startup);
   waitForSystem();

   // update the system cache
   updateSystemStateCache();
}


/**
 * Register a callback (listener class).
 * MMCore will send notifications on internal events using this interface
 */
void CMMCore::registerCallback(MMEventCallback* cb)
{
   externalCallback_ = cb;
}


/**
 * Returns the latest focus score from the focusing device.
 * Use this value to estimate or record how reliable the focus is.
 * The range of values is device dependent.
 */
double CMMCore::getFocusScore()
{
   if (autoFocus_ != 0)
   {
      double score;
      int ret = autoFocus_->GetFocusScore(score);
      if (ret != DEVICE_OK)
      {
         logError(getDeviceName(autoFocus_).c_str(), getDeviceErrorText(ret, autoFocus_).c_str());
         throw CMMError(getDeviceErrorText(ret, autoFocus_).c_str(), MMERR_DEVICE_GENERIC);
      }
      return score;
   }
   else
      return 0.0;
}

/**
 * Enables or disables the operation of the continouous focusing hardware device.
 */
void CMMCore::enableContinuousFocus(bool enable) throw (CMMError)
{
   if (autoFocus_ != 0)
   {
      int ret = autoFocus_->SetContinuousFocusing(enable);
      if (ret != DEVICE_OK)
      {
         logError(getDeviceName(autoFocus_).c_str(), getDeviceErrorText(ret, autoFocus_).c_str());
         throw CMMError(getDeviceErrorText(ret, autoFocus_).c_str(), MMERR_DEVICE_GENERIC);
      }

      if (enable)
         CORE_LOG("Continuous focusing enabled\n");
      else
         CORE_LOG("Continuous focusing disabled\n");
   }
   else
   {
      if (enable)
      {
         logError(getDeviceName(autoFocus_).c_str(), getCoreErrorText(MMERR_ContFocusNotAvailable).c_str());
         throw CMMError(getCoreErrorText(MMERR_ContFocusNotAvailable).c_str(), MMERR_ContFocusNotAvailable);
      }

   }
}

/**
 * Checks if the continouous focusing hardware device is ON or OFF.
 */
bool CMMCore::isContinuousFocusEnabled() throw (CMMError)
{
   if (autoFocus_ != 0)
   {
      bool state;
      int ret = autoFocus_->GetContinuousFocusing(state);
      if (ret != DEVICE_OK)
      {
         logError(getDeviceName(autoFocus_).c_str(), getDeviceErrorText(ret, autoFocus_).c_str());
         throw CMMError(getDeviceErrorText(ret, autoFocus_).c_str(), MMERR_DEVICE_GENERIC);
      }
      return state;
   }
   else
      return false; // no auto-focus device
}

/**
 * Returns the lock-in status of the continuous focusing device.
 */
bool CMMCore::isContinuousFocusLocked() throw (CMMError)
{
   if (autoFocus_ != 0)
      return autoFocus_->IsContinuousFocusLocked();
   else
      return false; // no auto-focus device
}

/**
 * Performs focus acquisition and lock for the one-shot focusing device.
 */
void CMMCore::fullFocus() throw (CMMError)
{
   if (autoFocus_)
      autoFocus_->FullFocus();
   else

}

/**
 * Performs incremental focusl for the one-shot focusing device.
 */
void CMMCore::incrementalFocus() throw (CMMError)
{
   if (autoFocus_)
      autoFocus_->FullFocus();
}

///////////////////////////////////////////////////////////////////////////////
// Private methods
///////////////////////////////////////////////////////////////////////////////

bool CMMCore::isConfigurationCurrent(const Configuration& config) const
{
  // getConfigState(   

   // to dermine whether the current state of the system matches our configuration
   // we need to check property settings one by one
   for (size_t i=0; i<config.size(); i++)
   {
      PropertySetting setting = config.getSetting(i);

      // perform special processing for core commands
      if (setting.getDeviceLabel().compare(MM::g_Keyword_CoreDevice) == 0)
      {
         string coreValue = properties_->Get(setting.getPropertyName().c_str());
         if (coreValue.compare(setting.getPropertyValue()) != 0)
            return false;
         else
            continue;
      }

      // first get device
	   MM::Device* pDevice = 0;
	   try {
		   // >>> TODO: is throwing an exception an efficient way of doing this???
	      // (is getting a wrong device is going to happen a lot?)
		   pDevice = pluginManager_.GetDevice(setting.getDeviceLabel().c_str());
	   } catch (CMMError&) {
	      // trap exception and return "device not found"
         return false;
	   }

      // then fetch property
      char value[MM::MaxStrLength];
      int ret = pDevice->GetProperty(setting.getPropertyName().c_str(), value);
      if (ret != DEVICE_OK)
         return false; // property not found

      // and finally check the value
      if (setting.getPropertyValue().compare(value) != 0)
         return false; // value does not match
   }
   return true;
}

void CMMCore::applyConfiguration(const Configuration& config) throw (CMMError)
{
   for (size_t i=0; i<config.size(); i++)
   {
      PropertySetting setting = config.getSetting(i);
      
      // perform special processing for core commands
      if (setting.getDeviceLabel().compare(MM::g_Keyword_CoreDevice) == 0)
      {
         properties_->Execute(setting.getPropertyName().c_str(), setting.getPropertyValue().c_str());
         stateCache_.addSetting(PropertySetting(MM::g_Keyword_CoreDevice, setting.getPropertyName().c_str(), setting.getPropertyValue().c_str()));
      }
      else
      {
         // normal processing
         MM::Device* pDevice = pluginManager_.GetDevice(setting.getDeviceLabel().c_str());
         int ret = pDevice->SetProperty(setting.getPropertyName().c_str(), setting.getPropertyValue().c_str());
         if (ret != DEVICE_OK)
         {
            logError(setting.getDeviceLabel().c_str(), getDeviceErrorText(ret, pDevice).c_str());
            throw CMMError(setting.getDeviceLabel().c_str(), getDeviceErrorText(ret, pDevice).c_str(), MMERR_DEVICE_GENERIC);
         }
         stateCache_.addSetting(setting);
      }
   }
}

string CMMCore::getDeviceErrorText(int deviceCode, MM::Device* pDevice) const
{
   ostringstream txt;
   if (pDevice)
   {
      // device specific error
      char devName[MM::MaxStrLength];
      pDevice->GetName(devName);
      txt << "Device " << devName << ". ";

      char text[MM::MaxStrLength];
      pDevice->GetErrorText(deviceCode, text);
      if (strlen(text) > 0)
         txt << text << ". ";
   }

   return txt.str();
}

string CMMCore::getCoreErrorText(int code) const
{
   // core info
   string txt;
   map<int, string>::const_iterator it;
   it = errorText_.find(code);   
   if (it != errorText_.end())
      txt = it->second;

   return txt;
}

MM::Device* CMMCore::getDevice(const char* label) const throw (CMMError)
{
   try {
      return pluginManager_.GetDevice(label);
   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      logError("core", getCoreErrorText(err.getCode()).c_str());
      throw;
   }
}

template <class T>
T* CMMCore::getSpecificDevice(const char* deviceLabel) const throw (CMMError)
{
   MM::Device* pDevice;
   T* pSpecDev = 0;
   try {
      pDevice = pluginManager_.GetDevice(deviceLabel);
      // The most appropriate thing to do here is to use
      // pSpecDev = dynamic_cast<T*>(pDevice). But, we can't use dynamic_cast beacuse
      // GCC linker on Linux does not interpret RTTI properly across the DLL boundary.
      // Instead we'll check the type through the Type identifier and use static_cast.
      if (pDevice->GetType() != T::Type)
         throw CMMError(deviceLabel, MMERR_InvalidSpecificDevice);

      pSpecDev = static_cast<T*>(pDevice);

   } catch (CMMError& err) {
      err.setCoreMsg(getCoreErrorText(err.getCode()).c_str());
      throw;
   } catch (...) {
      throw CMMError(getCoreErrorText(MMERR_UnhandledException).c_str(), MMERR_UnhandledException);
   }

   return pSpecDev;
}

void CMMCore::initializeLogging()
{
   //ACE_LOG_MSG->exists();
   if (!logStream_->is_open())
      logStream_->open(g_logFileName, ios_base::app);
   ACE_LOG_MSG->msg_ostream (logStream_, 0);
   ACE_LOG_MSG->set_flags (ACE_Log_Msg::OSTREAM);
   ACE_LOG_MSG->set_flags (ACE_Log_Msg::STDERR);
}

void CMMCore::shutdownLogging()
{
   ACE_LOG_MSG->clr_flags (ACE_Log_Msg::OSTREAM);
   ACE_LOG_MSG->msg_ostream (0, 0);

   //ACE_LOG_MSG->exists();
   if (!logStream_->is_open())
      logStream_->close();
}

void CMMCore::logError(const char* device, const char* msg, const char* fileName, int line) const
{
   ostringstream os;
   os << "Device " << device << ". " << msg << endl;
   if (fileName == 0)
      CORE_LOG1("Error occured. %s\n", os.str().c_str());
   else
      CORE_LOG3("Error occured. %s, file %s, line %d\n", os.str().c_str(), fileName, line);
}

string CMMCore::getDeviceName(MM::Device* pDev)
{
   char devName[MM::MaxStrLength];
   pDev->GetName(devName);
   string name(devName);
   return name;
}
