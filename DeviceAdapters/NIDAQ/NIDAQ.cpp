///////////////////////////////////////////////////////////////////////////////
// FILE:          NIDAQ.cpp
// PROJECT:       Micro-Manager
// SUBSYTEM:      DevcieAdapters
//
// DESCRIPTION:   Adapter for National Instruments IO devices
// COPYRIGHT:     University of California, San Francisco, 2008
// LICENSE:       LGPL
//
// AUTHOR:        Nico Stuurman, nico@cmp.ucsf.edu, 10/04/2008
//
//


#include "NIDAQ.h"
#include "../../../3rdParty/trunk/NationalInstruments/NI-DAQmxBase/includes/NIDAQmxBase.h"
#include "../../MMDevice/ModuleInterface.h"


#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define snprintf _snprintf
#endif

const char* g_DeviceNameNIDAQDO = "NI-DAQ-DO";
const char* g_DeviceNameNIDAQAO = "NI-DAQ-AO";
//const char* 

MODULE_API void InitializeModuleData()
{
   AddAvailableDeviceName(g_DeviceNameNIDAQDO, "NI-DAQ DIO");
   AddAvailableDeviceName(g_DeviceNameNIDAQAO, "NI-DAQ AO");
   printf ("Added NI-DAQ DIO and AO");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   else if (strcmp(deviceName,g_DeviceNameNIDAQDO) == 0)
      return new NIDAQDO;

   else if (strcmp(deviceName,g_DeviceNameNIDAQAO) == 0)
      return new NIDAQAO;

   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

NIDAQDO::NIDAQDO() : 
   taskHandle_(0),
   state_(0),
   deviceName_("/Dev1/Port0"),
   inverted_(false),
   open_(false),
   initialized_ (false)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_OPEN_DEVICE_FAILED, "Failed communicating with National Instruments Device");

   CPropertyAction* pAct = new CPropertyAction(this, &NIDAQDO::OnDevice);
   CreateProperty("DeviceName", deviceName_.c_str(), MM::String, false, pAct, true);
}

NIDAQDO::~NIDAQDO()
{
   Shutdown();
}

void NIDAQDO::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameNIDAQDO);
}

int NIDAQDO::Initialize()
{
   int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameNIDAQDO, MM::String, true);
   if (ret != DEVICE_OK)
      return ret;

   CPropertyAction* pAct = new CPropertyAction(this, &NIDAQDO::OnLogic);
   ret = CreateProperty("Logic", "Normal", MM::String, false, pAct);
   if (ret != DEVICE_OK)
      return ret;
   AddAllowedValue("Logic", "Normal");
   AddAllowedValue("Logic", "Inverted");

   pAct = new CPropertyAction(this, &NIDAQDO::OnState);
   ret = CreateProperty("State", "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;
   // This needs to be changed for devices with more than 8 outputs
   SetPropertyLimits("State", 0, 255);
printf("H1\n");
   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;
printf("H2\n");
 
/*
   ret = initializeDevice();
   if (ret != DEVICE_OK)
      return ret;
*/
   initialized_ = true;

   return DEVICE_OK;
} 

int NIDAQDO::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

bool NIDAQDO::Busy()
{
   // TODO: timeouts
   return false;
}

int NIDAQDO::SetOpen(bool open/* = true*/)
{
   open_ = open;
   if (open) {
      return WriteToDevice(state_);
   } else {
      return WriteToDevice(0);
   }
   return DEVICE_OK;
}



int NIDAQDO::HandleDAQError(int ret, TaskHandle taskHandle)
{
   char errBuff[2048];
   DAQmxBaseGetExtendedErrorInfo (errBuff, 2048);
   SetErrorText(ret, errBuff);

   if (taskHandle != 0) {
      DAQmxBaseStopTask (taskHandle);
      DAQmxBaseClearTask (taskHandle);
   }                                                                         

   std::ostringstream os;
   os << "DAQmxBase Error: " << ret << "-" << errBuff;
   LogMessage(os.str().c_str());

   return ret;
}

int NIDAQDO::WriteToDevice(long lnValue) 
{
   // Note: the following will fail whith everything but 8-bit values
   if (inverted_)
      lnValue = (255 & ~lnValue);

   uInt32 w_data[1];
   int32 written;
   w_data[0] = lnValue;

   int ret = DAQmxBaseWriteDigitalU32(taskHandle_,1,1,10.0,DAQmx_Val_GroupByChannel,w_data,&written,NULL);
   if (DAQmxFailed(ret))
      HandleDAQError(ret, taskHandle_);

   return DEVICE_OK;
}

int NIDAQDO::InitializeDevice()
{
   // Create task
   int ret = DAQmxBaseCreateTask("", &taskHandle_);
   if (DAQmxFailed(ret))
      return HandleDAQError(ret, taskHandle_);

   ret = DAQmxBaseCreateDOChan(taskHandle_,deviceName_.c_str(),"",DAQmx_Val_ChanForAllLines);
   if (DAQmxFailed(ret))
      return HandleDAQError(ret, taskHandle_);

   return DEVICE_OK;
}


////////////////////////////
// Action Handlers
///////////////////////////

int NIDAQDO::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      // Nothing to do, let the caller use cached property
   } else if (eAct ==MM::AfterSet) {
      long state;
      pProp->Get(state);
      state_ = state;
      SetOpen(state_);
   }

   return DEVICE_OK;
}

int NIDAQDO::OnLogic(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      // Nothing to do, let the caller use cached property
   } else if (eAct ==MM::AfterSet) {
      std::string logic;
      pProp->Get(logic);
      if (logic ==  "Inverted")
         inverted_ = true;
      else
         inverted_ = false;
      SetOpen(open_);
   }

   return DEVICE_OK;
}

int NIDAQDO::OnDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      pProp->Set(deviceName_.c_str());
   } else if (eAct ==MM::AfterSet) {
      pProp->Get(deviceName_);
      LogMessage(deviceName_.c_str());
      return InitializeDevice();
   }

   return DEVICE_OK;
}

/*
 * Analog output device
 */
NIDAQAO::NIDAQAO() : 
   taskHandle_(0),
   deviceName_("/Dev1/ao0"),
   initialized_ (false),
   busy_(false),
   volts_ (0.0),
   minVolt_(0.0),
   maxVolt_(5.0),
   gateOpen_(true)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_OPEN_DEVICE_FAILED, "Failed communicating with National Instruments Device");

   CPropertyAction* pAct = new CPropertyAction(this, &NIDAQAO::OnDevice);
   CreateProperty("DeviceName", deviceName_.c_str(), MM::String, false, pAct, true);
   
   pAct = new CPropertyAction(this, &NIDAQAO::OnMinVolt);
   CreateProperty("Minimum Voltage", "0.0", MM::Float, false, pAct, true);
   
   pAct = new CPropertyAction(this, &NIDAQAO::OnMaxVolt);
   CreateProperty("Maximum Voltage", "5.0", MM::Float, false, pAct, true);
}

NIDAQAO::~NIDAQAO()
{
   Shutdown();
}

void NIDAQAO::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameNIDAQAO);
}

int NIDAQAO::Initialize()
{
   int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameNIDAQAO, MM::String, true);
   if (ret != DEVICE_OK)
      return ret;

   CPropertyAction* pAct = new CPropertyAction(this, &NIDAQAO::OnVolt);
   ret = CreateProperty("Volt", "0.0", MM::Float, false, pAct);
   if (ret != DEVICE_OK)
      return ret;
   SetPropertyLimits("Volt", minVolt_, maxVolt_);

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;
 
   initialized_ = true;

   return DEVICE_OK;
} 

int NIDAQAO::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

bool NIDAQAO::Busy()
{
   // TODO: timeouts
   return false;
}

int NIDAQAO::SetGateOpen(bool open/* = true*/)
{
   gateOpen_ = open;
   if (open) {
      return WriteToDevice(volts_);
   } else {
      return WriteToDevice(0);
   }
   return DEVICE_OK;
}

int NIDAQAO::SetSignal(double volts)
{
   volts_ = volts;
   if (gateOpen_)
      return WriteToDevice(volts_);

   return DEVICE_OK;
}


int NIDAQAO::HandleDAQError(int ret, TaskHandle taskHandle)
{
   char errBuff[2048];
   DAQmxBaseGetExtendedErrorInfo (errBuff, 2048);
   SetErrorText(ret, errBuff);

   if (taskHandle != 0) {
      DAQmxBaseStopTask (taskHandle);
      DAQmxBaseClearTask (taskHandle);
   }                                                                         

   std::ostringstream os;
   os << "DAQmxBase Error: " << ret << "-" << errBuff;
   LogMessage(os.str().c_str());

   return ret;
}

int NIDAQAO::WriteToDevice(double volt) 
{
   uInt64 samplesPerChan = 1;

   float64 data = volt;
   int32 pointsWritten;
   float64 timeout = 10.0;

   int ret = DAQmxBaseWriteAnalogF64(taskHandle_,samplesPerChan, 0, timeout, DAQmx_Val_GroupByChannel, &data, &pointsWritten, NULL);
   if (DAQmxFailed(ret))
      HandleDAQError(ret, taskHandle_);

   return DEVICE_OK;
}

int NIDAQAO::InitializeDevice()
{
   // Create task
   int ret = DAQmxBaseCreateTask("", &taskHandle_);
   if (DAQmxFailed(ret))
      return HandleDAQError(ret, taskHandle_);

   ret = DAQmxBaseCreateAOVoltageChan(taskHandle_,deviceName_.c_str(),"",minVolt_, maxVolt_, DAQmx_Val_Volts, NULL);
   if (DAQmxFailed(ret))
      return HandleDAQError(ret, taskHandle_);

   return DEVICE_OK;
}


////////////////////////////
// Action Handlers
///////////////////////////

int NIDAQAO::OnVolt(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      pProp->Set(volts_);
      // Nothing to do, let the caller use cached property
   } else if (eAct ==MM::AfterSet) {
      pProp->Get(volts_);
      if (gateOpen_)
         WriteToDevice(volts_);
   }

   return DEVICE_OK;
}

int NIDAQAO::OnMinVolt(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      pProp->Set(minVolt_);
      // Nothing to do, let the caller use cached property
   } else if (eAct ==MM::AfterSet) {
      pProp->Get(minVolt_);
   }

   return DEVICE_OK;
}

int NIDAQAO::OnMaxVolt(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      pProp->Set(maxVolt_);
      // Nothing to do, let the caller use cached property
   } else if (eAct ==MM::AfterSet) {
      pProp->Get(maxVolt_);
   }
   return DEVICE_OK;
}


int NIDAQAO::OnDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      pProp->Set(deviceName_.c_str());
   } else if (eAct ==MM::AfterSet) {
      pProp->Get(deviceName_);
      LogMessage(deviceName_.c_str());
      return InitializeDevice();
   }

   return DEVICE_OK;
}

