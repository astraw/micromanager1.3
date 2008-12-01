///////////////////////////////////////////////////////////////////////////////
// FILE:          Arduino.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Arduino adapter.  Needs acompanying firmware
// COPYRIGHT:     University of California, San Francisco, 2008
// LICENSE:       LGPL
// 
// AUTHOR:        Nico Stuurman, nico@cmp.ucsf.edu 11/09/2008
//
//

#include "Arduino.h"
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define snprintf _snprintf 
#endif

const char* g_DeviceNameArduinoHub = "Arduino-Hub";
const char* g_DeviceNameArduinoSwitch = "Arduino-Switch";
const char* g_DeviceNameArduinoShutter = "Arduino-Shutter";
const char* g_DeviceNameArduinoDA = "Arduino-DAC";
const char* g_DeviceNameArduinoInput = "Arduino-Input";


// Global info about the state of the Arduino.  This should be folded into a class
unsigned g_switchState = 0;
unsigned g_shutterState = 0;
std::string g_port;
std::string g_version;
const std::string g_MMVersion = "1";
bool g_portAvailable = false;
bool g_invertedLogic = false;
bool g_triggerMode = false;
const char* g_normalLogicString = "Normal";
const char* g_invertedLogicString = "Inverted";



///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
   AddAvailableDeviceName(g_DeviceNameArduinoHub, "Hub");
   AddAvailableDeviceName(g_DeviceNameArduinoSwitch, "Switch");
   AddAvailableDeviceName(g_DeviceNameArduinoShutter, "Shutter");
   AddAvailableDeviceName(g_DeviceNameArduinoDA, "DA");
   AddAvailableDeviceName(g_DeviceNameArduinoInput, "Input");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   if (strcmp(deviceName, g_DeviceNameArduinoHub) == 0)
   {
      return new CArduinoHub;
   }
   else if (strcmp(deviceName, g_DeviceNameArduinoSwitch) == 0)
   {
      return new CArduinoSwitch;
   }
   else if (strcmp(deviceName, g_DeviceNameArduinoShutter) == 0)
   {
      return new CArduinoShutter;
   }
   else if (strcmp(deviceName, g_DeviceNameArduinoDA) == 0)
   {
      return new CArduinoDA();
   }
   /*
   else if (strcmp(deviceName, g_DeviceNameArduinoInput) == 0)
   {
      return new CArduinoInput;
   }
   */


   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// CArduinoHUb implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
CArduinoHub::CArduinoHub() :
initialized_ (false)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_PORT_OPEN_FAILED, "Failed opening Arduino USB device");
   SetErrorText(ERR_BOARD_NOT_FOUND, "Did not find an Arduino board with the correct firmware");
   SetErrorText(ERR_NO_PORT_SET, "Hub Device not found.  The Arduino Hub device is needed to create this device");
   std::string errorText = "The firmware version on the Arduino is not compatible with this adapter.  Please use firmware version " + g_MMVersion;
   SetErrorText(ERR_VERSION_MISMATCH, errorText.c_str());

   CPropertyAction* pAct = new CPropertyAction(this, &CArduinoHub::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

   pAct = new CPropertyAction(this, &CArduinoHub::OnLogic);
   CreateProperty("Logic", g_invertedLogicString, MM::String, false, pAct, true);

   AddAllowedValue("Logic", g_invertedLogicString);
   AddAllowedValue("Logic", g_normalLogicString);
}

CArduinoHub::~CArduinoHub()
{
   Shutdown();
}

void CArduinoHub::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameArduinoHub);
}

bool CArduinoHub::Busy()
{
   return false;
}

int CArduinoHub::Initialize()
{
   // Name
   int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameArduinoHub, MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   // Check that we have a controller:
   PurgeComPort(g_port.c_str());

   unsigned char command[1];
   command[0] = 30;
   ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 1);
   if (ret != DEVICE_OK)
      return ret;

   MM::MMTime startTime = GetCurrentMMTime();
   unsigned long bytesRead = 0;
   unsigned char answer[7];
   while ((bytesRead < 7) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
      unsigned long br;
      ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 7, br);
      if (ret != DEVICE_OK)
         return ret;
      bytesRead += br;
   }
   if (answer[0] != 30)
      return ERR_COMMUNICATION;

   /*
   command[0] = 31;
   ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 1);
   if (ret != DEVICE_OK)
      return ret;

   ret = GetSerialAnswer(g_port.c_str(), "\n", g_version);
   if (ret != DEVICE_OK) {
      // try again since we might be missing the first byte after opening the port
      ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 1);
      if (ret != DEVICE_OK)
         return ret;
      ret = GetSerialAnswer(g_port.c_str(), "\n", g_version);
      if (ret != DEVICE_OK) 
         return ret;
   }

   if (g_version != g_MMVersion)
      return ERR_VERSION_MISMATCH;

   CPropertyAction* pAct = new CPropertyAction(this, &CArduinoHub::OnVersion);
   CreateProperty("Version", g_version.c_str(), MM::String, true, pAct);
   */

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;
   return DEVICE_OK;
}

int CArduinoHub::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int CArduinoHub::OnPort(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet)
   {
      pProp->Set(g_port.c_str());
   }
   else if (pAct == MM::AfterSet)
   {
      pProp->Get(g_port);
      g_portAvailable = true;
   }
   return DEVICE_OK;
}

int CArduinoHub::OnVersion(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet)
   {
      pProp->Set(g_version.c_str());
   }
   return DEVICE_OK;
}

int CArduinoHub::OnLogic(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet)
   {
      if (g_invertedLogic)
         pProp->Set(g_invertedLogicString);
      else
         pProp->Set(g_normalLogicString);
   } else if (pAct == MM::AfterSet)
   {
      std::string logic;
      pProp->Get(logic);
      if (logic.compare(g_invertedLogicString)==0)
         g_invertedLogic = true;
      else g_invertedLogic = false;
   }
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// CArduinoSwitch implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~

CArduinoSwitch::CArduinoSwitch() : 
   blanking_(false),
   initialized_(false),
   numPos_(64), 
   busy_(false)
{
   InitializeDefaultErrorMessages();

   // add custom error messages
   SetErrorText(ERR_UNKNOWN_POSITION, "Invalid position (state) specified");
   SetErrorText(ERR_INITIALIZE_FAILED, "Initialization of the device failed");
   SetErrorText(ERR_WRITE_FAILED, "Failed to write data to the device");
   SetErrorText(ERR_CLOSE_FAILED, "Failed closing the device");
   SetErrorText(ERR_COMMUNICATION, "Error in communication with Arduino board");
   SetErrorText(ERR_NO_PORT_SET, "Hub Device not found.  The Arduino Hub device is needed to create this device");

   for (int i=0; i < NUMPATTERNS; i++)
      pattern_[i] = 0;
}

CArduinoSwitch::~CArduinoSwitch()
{
   Shutdown();
}

void CArduinoSwitch::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameArduinoSwitch);
}


int CArduinoSwitch::Initialize()
{
   if (!g_portAvailable) {
      return ERR_NO_PORT_SET;
   }

   // set property list
   // -----------------
   
   // Name
   int nRet = CreateProperty(MM::g_Keyword_Name, g_DeviceNameArduinoSwitch, MM::String, true);
   if (DEVICE_OK != nRet)
      return nRet;

   // Description
   nRet = CreateProperty(MM::g_Keyword_Description, "Arduino digital output driver", MM::String, true);
   if (DEVICE_OK != nRet)
      return nRet;

   // create positions and labels
   const int bufSize = 65;
   char buf[bufSize];
   for (long i=0; i<numPos_; i++)
   {
      snprintf(buf, bufSize, "%d", (unsigned)i);
      SetPositionLabel(i, buf);
   }

   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &CArduinoSwitch::OnState);
   nRet = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   SetPropertyLimits(MM::g_Keyword_State, 0, numPos_ - 1);

   // Label
   // -----
   pAct = new CPropertyAction (this, &CStateBase::OnLabel);
   nRet = CreateProperty(MM::g_Keyword_Label, "", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;

   // Patterns are used for triggered output
   pAct = new CPropertyAction (this, &CArduinoSwitch::OnSetPattern);
   nRet = CreateProperty("SetPattern", "", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   pAct = new CPropertyAction (this, &CArduinoSwitch::OnGetPattern);
   nRet = CreateProperty("GetPattern", "", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   AddAllowedValue("SetPattern", "");
   AddAllowedValue("GetPattern", "");
   for (int i=0; i< NUMPATTERNS; i++) {
      std::ostringstream os;
      os.fill(' ');
      os.width(2);
      os << i;
      os.flags(std::ios::left);
      AddAllowedValue("SetPattern", os.str().c_str());
      AddAllowedValue("GetPattern", os.str().c_str());
   }

   pAct = new CPropertyAction(this, &CArduinoSwitch::OnPatternsUsed);
   nRet = CreateProperty("Nr. Patterns Used", "0", MM::Integer, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   SetPropertyLimits("Nr. Patterns Used", 0, NUMPATTERNS);

   pAct = new CPropertyAction(this, &CArduinoSwitch::OnSkipTriggers);
   nRet = CreateProperty("Skip Patterns (Nr.)", "0", MM::Integer, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;

   pAct = new CPropertyAction(this, &CArduinoSwitch::OnStartTrigger);
   nRet = CreateProperty("TriggerMode", "Idle", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   AddAllowedValue("TriggerMode", "Stop");
   AddAllowedValue("TriggerMode", "Start");
   AddAllowedValue("TriggerMode", "Running");
   AddAllowedValue("TriggerMode", "Idle");

   pAct = new CPropertyAction(this, &CArduinoSwitch::OnBlanking);
   nRet = CreateProperty("Blanking Mode", "Idle", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   AddAllowedValue("Blanking Mode", "Stop");
   AddAllowedValue("Blanking Mode", "Start");
   AddAllowedValue("Blanking Mode", "Running");
   AddAllowedValue("Blanking Mode", "Idle");

   pAct = new CPropertyAction(this, &CArduinoSwitch::OnBlankingTriggerDirection);
   nRet = CreateProperty("Blank On", "Low", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   AddAllowedValue("Blank On", "Low");
   AddAllowedValue("Blank On", "High");

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int CArduinoSwitch::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int CArduinoSwitch::WriteToPort(long value)
{
   value = 63 & value;
   if (g_invertedLogic)
      value = ~value;

   PurgeComPort(g_port.c_str());

   unsigned char command[2];
   command[0] = 1;
   command[1] = (unsigned char) value;
   int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 2);
   if (ret != DEVICE_OK)
      return ret;

   MM::MMTime startTime = GetCurrentMMTime();
   unsigned long bytesRead = 0;
   unsigned char answer[1];
   while ((bytesRead < 1) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
      ret = ReadFromComPort(g_port.c_str(), answer, 1, bytesRead);
      if (ret != DEVICE_OK)
         return ret;
   }
   if (answer[0] != 1)
      return ERR_COMMUNICATION;

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int CArduinoSwitch::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);
      g_switchState = pos;
      if (g_shutterState > 0)
         return WriteToPort(pos);
   }

   return DEVICE_OK;
}

int CArduinoSwitch::OnSetPattern(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      // Never set anything here 
      pProp->Set(" ");
   }
   else if (eAct == MM::AfterSet)
   {
      std::string tmp;
      pProp->Get(tmp);
      if (tmp == " ")
         return DEVICE_OK;
      int pos =  atoi(tmp.c_str());

      unsigned value = g_switchState;
      pattern_[pos] = g_switchState;

      value = 63 & value;
      if (g_invertedLogic)
         value = ~value;

      PurgeComPort(g_port.c_str());

      unsigned char command[3];
      command[0] = 5;
      command[1] = (unsigned char) pos;
      command[2] = (unsigned char) value;
      int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 3);
      if (ret != DEVICE_OK)
         return ret;

      MM::MMTime startTime = GetCurrentMMTime();
      unsigned long bytesRead = 0;
      unsigned char answer[3];
      while ((bytesRead < 3) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
         unsigned long br;
         ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 3, br);
         if (ret != DEVICE_OK)
            return ret;
         bytesRead += br;
      }
      if (answer[0] != 5)
         return ERR_COMMUNICATION;

      g_triggerMode = false;
   }

   return DEVICE_OK;
}

int CArduinoSwitch::OnGetPattern(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      // find the first pattern that corresponds to the current switch state
      int i = 0;
      bool found = false;
      while (!found && i<NUMPATTERNS) {
         if (pattern_[i] == g_switchState)
            found = true;
         else
            i++;
      }
      std::ostringstream os;
      if (found)
         os << i;
      else 
         os << " ";
      pProp->Set(os.str().c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      std::string tmp;
      pProp->Get(tmp);
      if (tmp == " ")
         return DEVICE_OK;
      int pos =  atoi(tmp.c_str());

      g_switchState = pattern_[pos];
      std::ostringstream os;
      os << g_switchState;
      SetProperty(MM::g_Keyword_State, os.str().c_str());
   }

   return DEVICE_OK;
}

int CArduinoSwitch::OnPatternsUsed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      PurgeComPort(g_port.c_str());

      unsigned char command[2];
      command[0] = 6;
      command[1] = (unsigned char) pos;
      int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 2);
      if (ret != DEVICE_OK)
         return ret;

      MM::MMTime startTime = GetCurrentMMTime();
      unsigned long bytesRead = 0;
      unsigned char answer[2];
      while ((bytesRead < 2) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
         unsigned long br;
         ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 2, br);
         if (ret != DEVICE_OK)
            return ret;
         bytesRead += br;
      }
      if (answer[0] != 6)
         return ERR_COMMUNICATION;

      g_triggerMode = false;
   }

   return DEVICE_OK;
}


int CArduinoSwitch::OnSkipTriggers(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      long prop;
      pProp->Get(prop);

      PurgeComPort(g_port.c_str());
      unsigned char command[2];
      command[0] = 7;
      command[1] = (unsigned char) prop;
      int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 2);
      if (ret != DEVICE_OK)
         return ret;

      MM::MMTime startTime = GetCurrentMMTime();
      unsigned long bytesRead = 0;
      unsigned char answer[2];
      while ((bytesRead < 2) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
         unsigned long br;
         ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 2, br);
         if (ret != DEVICE_OK)
            return ret;
         bytesRead += br;
      }
      if (answer[0] != 7)
         return ERR_COMMUNICATION;

      g_triggerMode = false;
   }

   return DEVICE_OK;
}

int CArduinoSwitch::OnStartTrigger(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      if (g_triggerMode)
         pProp->Set("Running");
      else
         pProp->Set("Idle");
   }
   else if (eAct == MM::AfterSet)
   {
      std::string prop;
      pProp->Get(prop);

      if (prop =="Start") {
         PurgeComPort(g_port.c_str());
         unsigned char command[1];
         command[0] = 8;
         int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 1);
         if (ret != DEVICE_OK)
            return ret;

         MM::MMTime startTime = GetCurrentMMTime();
         unsigned long bytesRead = 0;
         unsigned char answer[1];
         while ((bytesRead < 1) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
            unsigned long br;
            ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 1, br);
            if (ret != DEVICE_OK)
               return ret;
            bytesRead += br;
         }
         if (answer[0] != 8)
            return ERR_COMMUNICATION;
         g_triggerMode = true;
      } else {
         unsigned char command[1];
         command[0] = 9;
         int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 1);
         if (ret != DEVICE_OK)
            return ret;

         MM::MMTime startTime = GetCurrentMMTime();
         unsigned long bytesRead = 0;
         unsigned char answer[2];
         while ((bytesRead < 2) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
            unsigned long br;
            ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 2, br);
            if (ret != DEVICE_OK)
               return ret;
            bytesRead += br;
         }
         if (answer[0] != 9)
            return ERR_COMMUNICATION;
         g_triggerMode = false;
      }
   }

   return DEVICE_OK;
}

int CArduinoSwitch::OnBlanking(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      if (blanking_)
         pProp->Set("Running");
      else
         pProp->Set("Idle");
   }
   else if (eAct == MM::AfterSet)
   {
      std::string prop;
      pProp->Get(prop);

      if (prop =="Start" && !blanking_) {
         PurgeComPort(g_port.c_str());
         unsigned char command[1];
         command[0] = 20;
         int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 1);
         if (ret != DEVICE_OK)
            return ret;

         MM::MMTime startTime = GetCurrentMMTime();
         unsigned long bytesRead = 0;
         unsigned char answer[1];
         while ((bytesRead < 1) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
            unsigned long br;
            ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 1, br);
            if (ret != DEVICE_OK)
               return ret;
            bytesRead += br;
         }
         if (answer[0] != 20)
            return ERR_COMMUNICATION;
         blanking_ = true;
         g_triggerMode = false;
      } else if (prop =="Stop" && blanking_){
         unsigned char command[1];
         command[0] = 21;
         int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 1);
         if (ret != DEVICE_OK)
            return ret;

         MM::MMTime startTime = GetCurrentMMTime();
         unsigned long bytesRead = 0;
         unsigned char answer[2];
         while ((bytesRead < 2) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
            unsigned long br;
            ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 2, br);
            if (ret != DEVICE_OK)
               return ret;
            bytesRead += br;
         }
         if (answer[0] != 21)
            return ERR_COMMUNICATION;
         blanking_ = false;
         g_triggerMode = false;
      }
   }

   return DEVICE_OK;
}

int CArduinoSwitch::OnBlankingTriggerDirection(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet) {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      std::string direction;
      pProp->Get(direction);


      PurgeComPort(g_port.c_str());
      unsigned char command[1];
      command[0] = 22;
      if (direction == "Low") 
         command[1] = 1;
      else
         command[1] = 0;

      int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 2);
      if (ret != DEVICE_OK)
         return ret;

      MM::MMTime startTime = GetCurrentMMTime();
      unsigned long bytesRead = 0;
      unsigned char answer[1];
      while ((bytesRead < 1) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
         unsigned long br;
         ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 1, br);
         if (ret != DEVICE_OK)
            return ret;
         bytesRead += br;
      }
      if (answer[0] != 22)
         return ERR_COMMUNICATION;

   }

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// CArduinoDA implementation
// ~~~~~~~~~~~~~~~~~~~~~~

CArduinoDA::CArduinoDA() :
      busy_(false), 
      minV_(0.0), 
      maxV_(5.0), 
      volts_(0.0),
      gatedVolts_(0.0),
      encoding_(0), 
      resolution_(8), 
      channel_(1), 
      maxChannel_(2),
      name_(""),
      gateOpen_(true)
{
   InitializeDefaultErrorMessages();

   // add custom error messages
   SetErrorText(ERR_UNKNOWN_POSITION, "Invalid position (state) specified");
   SetErrorText(ERR_INITIALIZE_FAILED, "Initialization of the device failed");
   SetErrorText(ERR_WRITE_FAILED, "Failed to write data to the device");
   SetErrorText(ERR_CLOSE_FAILED, "Failed closing the device");
   SetErrorText(ERR_NO_PORT_SET, "Hub Device not found.  The Arduino Hub device is needed to create this device");

   CPropertyAction* pAct = new CPropertyAction(this, &CArduinoDA::OnChannel);
   CreateProperty("Channel", "2", MM::Integer, false, pAct, true);
   for (int i=1; i<= 2; i++){
      std::ostringstream os;
      os << i;
      AddAllowedValue("Channel", os.str().c_str());
   }

   pAct = new CPropertyAction(this, &CArduinoDA::OnMaxVolt);
   CreateProperty("MaxVolt", "5.0", MM::Float, false, pAct, true);

}

CArduinoDA::~CArduinoDA()
{
   Shutdown();
}

void CArduinoDA::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}


int CArduinoDA::Initialize()
{
   if (!g_portAvailable) {
      return ERR_NO_PORT_SET;
   }

   // set property list
   // -----------------
   
   // Name
   int nRet = CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);
   if (DEVICE_OK != nRet)
      return nRet;

   // Description
   nRet = CreateProperty(MM::g_Keyword_Description, "Arduino DAC driver", MM::String, true);
   if (DEVICE_OK != nRet)
      return nRet;

   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &CArduinoDA::OnVolts);
   nRet = CreateProperty("Volts", "0.0", MM::Float, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   SetPropertyLimits("Volts", minV_, maxV_);

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int CArduinoDA::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int CArduinoDA::WriteToPort(unsigned long value)
{
   PurgeComPort(g_port.c_str());

   unsigned char command[4];
   command[0] = 3;
   command[1] = (unsigned char) (channel_ -1);
   command[2] = (unsigned char) (value / 256L);
   command[3] = (unsigned char) (value & 255);
   int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 4);
   if (ret != DEVICE_OK)
      return ret;

   MM::MMTime startTime = GetCurrentMMTime();
   unsigned long bytesRead = 0;
   unsigned char answer[4];
   while ((bytesRead < 4) && ( (GetCurrentMMTime() - startTime).getMsec() < 2500)) {
      unsigned long bR;
      ret = ReadFromComPort(g_port.c_str(), answer + bytesRead, 4 - bytesRead, bR);
      if (ret != DEVICE_OK)
         return ret;
      bytesRead += bR;
   }
   if (answer[0] != 3)
      return ERR_COMMUNICATION;

   // TODO: make triggermode a global:)
   g_triggerMode = false;

   return DEVICE_OK;
}


int CArduinoDA::WriteSignal(double volts)
{
   long value = (long) ( (volts - minV_) / maxV_ * 4095);

   std::ostringstream os;
    os << "Volts: " << volts << " Max Voltage: " << maxV_ << " digital value: " << value;
    LogMessage(os.str().c_str(), true);

   return WriteToPort(value);
}

int CArduinoDA::SetSignal(double volts)
{
   volts_ = volts;
   if (gateOpen_) {
      gatedVolts_ = volts_;
      return WriteSignal(volts_);
   } else {
      gatedVolts_ = 0;
   }

   return DEVICE_OK;
}

int CArduinoDA::SetGateOpen(bool open)
{
   if (open) {
      gateOpen_ = true;
      gatedVolts_ = volts_;
      return WriteSignal(volts_);
   } else {
      gateOpen_ = false;
      gatedVolts_ = 0;
      return WriteSignal(0.0);
   }

   // return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int CArduinoDA::OnVolts(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      double volts;
      pProp->Get(volts);
      return SetSignal(volts);
   }

   return DEVICE_OK;
}

int CArduinoDA::OnMaxVolt(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(maxV_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(maxV_);
      if (HasProperty("Volts"))
         SetPropertyLimits("Volts", 0.0, maxV_);

   }
   return DEVICE_OK;
}

int CArduinoDA::OnChannel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set((long int)channel_);
   }
   else if (eAct == MM::AfterSet)
   {
      long channel;
      pProp->Get(channel);
      if (channel >=1 && ( (unsigned) channel <=maxChannel_) )
         channel_ = channel;
   }
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// CArduinoShutter implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~

CArduinoShutter::CArduinoShutter() : initialized_(false), name_(g_DeviceNameArduinoShutter)
{
   InitializeDefaultErrorMessages();
   EnableDelay();

   SetErrorText(ERR_NO_PORT_SET, "Hub Device not found.  The Arduino Hub device is needed to create this device");
}

CArduinoShutter::~CArduinoShutter()
{
   Shutdown();
}

void CArduinoShutter::GetName(char* name) const
{
   assert(name_.length() < CDeviceUtils::GetMaxStringLength());
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

bool CArduinoShutter::Busy()
{
   MM::MMTime interval = GetCurrentMMTime() - changedTime_;

   if (interval < (1000.0 < GetDelayMs() ))
      return true;
   else
       return false;
}

int CArduinoShutter::Initialize()
{
   if (!g_portAvailable) {
      return ERR_NO_PORT_SET;
   }

   // set property list
   // -----------------
   
   // Name
   int ret = CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   // Description
   ret = CreateProperty(MM::g_Keyword_Description, "Arduino shutter driver", MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   // OnOff
   // ------
   CPropertyAction* pAct = new CPropertyAction (this, &CArduinoShutter::OnOnOff);
   ret = CreateProperty("OnOff", "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   // set shutter into the off state
   //WriteToPort(0);

   std::vector<std::string> vals;
   vals.push_back("0");
   vals.push_back("1");
   ret = SetAllowedValues("OnOff", vals);
   if (ret != DEVICE_OK)
      return ret;

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   changedTime_ = GetCurrentMMTime();
   initialized_ = true;

   return DEVICE_OK;
}

int CArduinoShutter::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

int CArduinoShutter::SetOpen(bool open)
{
   if (open)
      return SetProperty("OnOff", "1");
   else
      return SetProperty("OnOff", "0");
}

int CArduinoShutter::GetOpen(bool& open)
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty("OnOff", buf);
   if (ret != DEVICE_OK)
      return ret;
   long pos = atol(buf);
   pos > 0 ? open = true : open = false;

   return DEVICE_OK;
}

int CArduinoShutter::Fire(double /*deltaT*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int CArduinoShutter::WriteToPort(long value)
{
   value = 63 & value;
   if (g_invertedLogic)
      value = ~value;

   PurgeComPort(g_port.c_str());

   unsigned char command[2];
   command[0] = 1;
   command[1] = (unsigned char) value;
   int ret = WriteToComPort(g_port.c_str(), (const unsigned char*) command, 2);
   if (ret != DEVICE_OK)
      return ret;

   MM::MMTime startTime = GetCurrentMMTime();
   unsigned long bytesRead = 0;
   unsigned char answer[1];
   while ((bytesRead < 1) && ( (startTime - GetCurrentMMTime()).getMsec() < 250)) {
      ret = ReadFromComPort(g_port.c_str(), answer, 1, bytesRead);
      if (ret != DEVICE_OK)
         return ret;
   }
   if (answer[0] != 1)
      return ERR_COMMUNICATION;

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int CArduinoShutter::OnOnOff(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // use cached state
      pProp->Set((long)g_shutterState);
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);
      int ret;
      if (pos == 0)
         ret = WriteToPort(0); // turn everything off
      else
         ret = WriteToPort(g_switchState); // restore old setting
      if (ret != DEVICE_OK)
         return ret;
      g_shutterState = pos;
      changedTime_ = GetCurrentMMTime();
   }

   return DEVICE_OK;
}


/*
 * Reads digital and analogue input from Velleman board
 * Mainly to test read functions in USBManager
 */

CArduinoInput::CArduinoInput() :
name_(g_DeviceNameArduinoInput)
{
}

CArduinoInput::~CArduinoInput()
{
   Shutdown();
}

int CArduinoInput::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

int CArduinoInput::Initialize()
{
   if (!g_portAvailable) {
      return ERR_NO_PORT_SET;
   }

   // Name
   int ret = CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   // Description
   ret = CreateProperty(MM::g_Keyword_Description, "Arduino input", MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   // Digital Input
   CPropertyAction* pAct = new CPropertyAction (this, &CArduinoInput::OnDigitalInput);
   ret = CreateProperty("Digital Input", "0", MM::Integer, true, pAct);
   if (ret != DEVICE_OK)
      return ret;

   for (long i=1; i <=8; i++) 
   {
      CPropertyActionEx *pExAct = new CPropertyActionEx(this, &CArduinoInput::OnAnalogInput, i);
      std::ostringstream os;
      os << "Analogue Input A" << i;
      ret = CreateProperty(os.str().c_str(), "0.0", MM::Float, true, pExAct);
      if (ret != DEVICE_OK)
         return ret;
   }

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;

   return DEVICE_OK;
}

void CArduinoInput::GetName(char* name) const
{
  CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

bool CArduinoInput::Busy()
{
   return false;
}


///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int CArduinoInput::OnDigitalInput(MM::PropertyBase* /* pProp */, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      /*
      unsigned char result;
      int ret =  g_ArduinoInterface.ReadAllDigital(*this, *GetCoreCallback(), result);
      if (ret != DEVICE_OK)
         return ret;
      pProp->Set((long)result);
      */
   }

   return DEVICE_OK;
}

int CArduinoInput::OnAnalogInput(MM::PropertyBase* /* pProp */, MM::ActionType eAct, long /* channel*/ )
{
   if (eAct == MM::BeforeGet)
   {
      /*
      long result;
      int ret = g_ArduinoInterface.ReadAnalogChannel(*this, *GetCoreCallback(), (unsigned char) channel, result);
      if (ret != DEVICE_OK)
         return ret;

      pProp->Set(result);
      */
   }
   return DEVICE_OK;
}

