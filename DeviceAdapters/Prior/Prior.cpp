///////////////////////////////////////////////////////////////////////////////
// FILE:          Prior.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Prior ProScan controller adapter
// COPYRIGHT:     University of California, San Francisco, 2006
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
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 06/01/2006
//
// CVS:           $Id$
//

#ifdef WIN32
   #include <windows.h>
   #define snprintf _snprintf 
#endif

#include "prior.h"
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>

const char* g_XYStageDeviceName = "XYStage";
const char* g_ZStageDeviceName = "ZStage";
const char* g_BasicControllerName = "BasicController";
const char* g_Shutter1Name="Shutter-1";
const char* g_Shutter2Name="Shutter-2";
const char* g_Shutter3Name="Shutter-3";
const char* g_LumenName="Lumen";
const char* g_Wheel1Name = "Wheel-1";
const char* g_Wheel2Name = "Wheel-2";
const char* g_Wheel3Name = "Wheel-3";
const char* g_TTL0Name="TTL-0";
const char* g_TTL1Name="TTL-1";
const char* g_TTL2Name="TTL-2";
const char* g_TTL3Name="TTL-3";

using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
   AddAvailableDeviceName(g_Shutter1Name, "Pro Scan shutter 1");
   AddAvailableDeviceName(g_Shutter2Name, "Pro Scan shutter 2");
   AddAvailableDeviceName(g_Shutter3Name, "Pro Scan shutter 3");
   AddAvailableDeviceName(g_LumenName, "Lumen 200Pro lamp shutter");
   AddAvailableDeviceName(g_Wheel1Name, "Pro Scan filter wheel 1");
   AddAvailableDeviceName(g_Wheel2Name, "Pro Scan filter wheel 2");
   AddAvailableDeviceName(g_Wheel3Name, "Pro Scan filter wheel 3");
   AddAvailableDeviceName(g_ZStageDeviceName, "Add-on Z-stage");
   AddAvailableDeviceName(g_XYStageDeviceName, "XY Stage");
   AddAvailableDeviceName(g_TTL0Name, "Pro Scan TTL 0");
   AddAvailableDeviceName(g_TTL1Name, "Pro Scan TTL 1");
   AddAvailableDeviceName(g_TTL2Name, "Pro Scan TTL 2");
   AddAvailableDeviceName(g_TTL3Name, "Pro Scan TTL 3");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   if (strcmp(deviceName, g_Shutter1Name) == 0)
   {
      Shutter* s = new Shutter(g_Shutter1Name, 1);
      return s;
   }
   else if (strcmp(deviceName, g_Shutter2Name) == 0)
   {
      Shutter* s = new Shutter(g_Shutter2Name, 2);
      return s;
   }
   else if (strcmp(deviceName, g_Shutter3Name) == 0)
   {
      Shutter* s = new Shutter(g_Shutter3Name, 3);
      return s;
   }
   else if (strcmp(deviceName, g_Wheel1Name) == 0)
   {
      Wheel* s = new Wheel(g_Wheel1Name, 1);
      return s;
   }
   else if (strcmp(deviceName, g_Wheel2Name) == 0)
   {
      Wheel* s = new Wheel(g_Wheel2Name, 2);
      return s;
   }
   else if (strcmp(deviceName, g_Wheel3Name) == 0)
   {
      Wheel* s = new Wheel(g_Wheel3Name, 3);
      return s;
   }
   else if (strcmp(deviceName, g_ZStageDeviceName) == 0)
   {
      ZStage* s = new ZStage();
      return s;
   }
   else if (strcmp(deviceName, g_XYStageDeviceName) == 0)
   {
      XYStage* s = new XYStage();
      return s;
   }
   else if (strcmp(deviceName, g_LumenName) == 0)
   {
      Lumen* s = new Lumen();
      return s;
   }
   else if (strcmp(deviceName, g_TTL0Name) == 0)
   {
      TTLShutter* s = new TTLShutter(g_TTL0Name, 0);
      return s;
   }
   else if (strcmp(deviceName, g_TTL1Name) == 0)
   {
      TTLShutter* s = new TTLShutter(g_TTL1Name, 1);
      return s;
   }
   else if (strcmp(deviceName, g_TTL2Name) == 0)
   {
      TTLShutter* s = new TTLShutter(g_TTL2Name, 0);
      return s;
   }
   else if (strcmp(deviceName, g_TTL3Name) == 0)
   {
      TTLShutter* s = new TTLShutter(g_TTL3Name, 0);
      return s;
   }


   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

int ClearPort(MM::Device& device, MM::Core& core, string port)
{
   // Clear contents of serial port 
   const int bufSize = 255;
   unsigned char clear[bufSize];
   unsigned long read = bufSize;
   int ret;
   while ((int) read == bufSize)
   {
      ret = core.ReadFromSerial(&device, port.c_str(), clear, bufSize, read);
      if (ret != DEVICE_OK)
         return ret;
   }
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Shutter 
// ~~~~~~~

Shutter::Shutter(const char* name, int id) : 
   initialized_(false), 
   id_(id), 
   name_(name), 
   changedTime_(0.0)
{
   InitializeDefaultErrorMessages();
   SetErrorText(ERR_UNRECOGNIZED_ANSWER, "Unrecognized answer recived from the device");

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Prior shutter adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &Shutter::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

   EnableDelay();

   UpdateStatus();
}

Shutter::~Shutter()
{
   Shutdown();
}

void Shutter::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

int Shutter::Initialize()
{
   // set property list
   // -----------------
   
   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &Shutter::OnState);
   int ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   AddAllowedValue(MM::g_Keyword_State, "0");
   AddAllowedValue(MM::g_Keyword_State, "1");

   // Delay
   // -----
   pAct = new CPropertyAction (this, &Shutter::OnDelay);
   ret = CreateProperty(MM::g_Keyword_Delay, "0.0", MM::Float, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

    // Set timer for the Busy signal
   changedTime_ = GetCurrentMMTime();

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;

   return DEVICE_OK;
}

int Shutter::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool Shutter::Busy()
{
   MM::MMTime interval = GetCurrentMMTime() - changedTime_;
   MM::MMTime delay(GetDelayMs()*1000.0);
   if (interval < delay)
      return true;
   else
      return false;
}

int Shutter::SetOpen(bool open)
{
   long pos;
   if (open)
      pos = 1;
   else
      pos = 0;
   return SetProperty(MM::g_Keyword_State, CDeviceUtils::ConvertToString(pos));
}

int Shutter::GetOpen(bool& open)
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_State, buf);
   if (ret != DEVICE_OK)
      return ret;
   long pos = atol(buf);
   pos == 1 ? open = true : open = false;

   return DEVICE_OK;
}
int Shutter::Fire(double /*deltaT*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

/**
 * Sends an open/close command through the serial port.
 */
int Shutter::SetShutterPosition(bool state)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "8," << id_ << "," << (state ? "0" : "1");

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   // Set timer for the Busy signal
   changedTime_ = GetCurrentMMTime();

   if (answer.substr(0,1).compare("R") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

/**
 * Check the state of the shutter.
 */
int Shutter::GetShutterPosition(bool& state)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "8," << id_;

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      if (answer.substr(0,1).compare("0") == 0)
         state = true;
      else
         state = false;
      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int Shutter::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      bool state;
      int ret = GetShutterPosition(state);
      if (ret != DEVICE_OK)
         return ret;
      if (state)
         pProp->Set(1L);
      else
         pProp->Set(0L);
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      // apply the value
      return SetShutterPosition(pos == 0 ? false : true);
   }

   return DEVICE_OK;
}

int Shutter::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int Shutter::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(this->GetDelayMs());
   }
   else if (eAct == MM::AfterSet)
   {
      double delay;
      pProp->Get(delay);
      this->SetDelayMs(delay);
   }

   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Wheel
//

Wheel::Wheel(const char* name, unsigned id) :
      initialized_(false), 
      numPos_(10), 
      id_(id), 
      name_(name), 
      curPos_(0), 
      busy_(false),
      changedTime_(0.0)
{
   assert((int)strlen(name) < (int)MM::MaxStrLength);

   InitializeDefaultErrorMessages();

   // add custom MTB messages
   SetErrorText(ERR_UNKNOWN_POSITION, "Position out of range");
   SetErrorText(ERR_PORT_CHANGE_FORBIDDEN, "You can't change the port after device has been initialized.");
   SetErrorText(ERR_SET_POSITION_FAILED, "Set position failed.");

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Prior filter wheel adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &Wheel::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

   EnableDelay();

   UpdateStatus();
}

Wheel::~Wheel()
{
   Shutdown();
}

void Wheel::GetName(char* name) const
{
   assert(name_.length() < CDeviceUtils::GetMaxStringLength());
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

bool Wheel::Busy()
{
   MM::MMTime interval = GetCurrentMMTime() - changedTime_;
   MM::MMTime delay(GetDelayMs()*1000.0);
   if (interval < delay)
      return true;
   else
      return false;
}

int Wheel::Initialize()
{

   // set property list
   // -----------------
   
   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &Wheel::OnState);
   int ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   // Speed
   // -----
   pAct = new CPropertyAction (this, &Wheel::OnSpeed);
   ret = CreateProperty(MM::g_Keyword_Speed, "3", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   // Label
   // -----
   pAct = new CPropertyAction (this, &CStateBase::OnLabel);
   ret = CreateProperty(MM::g_Keyword_Label, "Undefined", MM::String, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   // Delay
   // -----
   pAct = new CPropertyAction (this, &Wheel::OnDelay);
   ret = CreateProperty(MM::g_Keyword_Delay, "0.0", MM::Float, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   // create default positions and labels
   const int bufSize = 1024;
   char buf[bufSize];
   for (unsigned i=0; i<numPos_; i++)
   {
      snprintf(buf, bufSize, "Filter-%d", i+1);
      SetPositionLabel(i, buf);
   }


   // For good measure, home the wheel
   ostringstream command;
   command << "7," << id_ << ",h";

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // Set timer for Busy signal
   changedTime_ = GetCurrentMMTime();

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   //char setSerial = (char)238;
   //ret = WriteToComPort(port_.c_str(), &setSerial, 1);
   ret = SetWheelPosition(1);
   if (DEVICE_OK != ret)
      return ret;

   // Set timer for the Busy signal
   changedTime_ = GetCurrentMMTime();

   initialized_ = true;

   return DEVICE_OK;
}

int Wheel::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

int Wheel::SetWheelPosition(unsigned pos)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "7," << id_ << "," << pos +1;

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   // Set timer for the Busy signal
   changedTime_ = GetCurrentMMTime();
   
   if (answer.substr(0,1).compare("R") == 0)
   {
      curPos_ = pos;
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int Wheel::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)curPos_);
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      //check if we are already in that state
      if ((unsigned) pos == curPos_)
         return DEVICE_OK;

      if (pos >= (long)numPos_ || pos < 0)
      {
         pProp->Set((long)curPos_); // revert
         return ERR_UNKNOWN_POSITION;
      }

      // try to apply the value
      int ret = SetWheelPosition(pos);
      if (ret != DEVICE_OK)
      {
         pProp->Set((long)curPos_); // revert
         return ret;
      }
      curPos_ = pos;
   }

   return DEVICE_OK;
}

int Wheel::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int Wheel::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(this->GetDelayMs());
   }
   else if (eAct == MM::AfterSet)
   {
      double delay;
      pProp->Get(delay);
      this->SetDelayMs(delay);
   }

   return DEVICE_OK;
}

int Wheel::OnSpeed(MM::PropertyBase* /*pProp*/, MM::ActionType /*eAct*/)
{
   // TODO
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// XYStage
//
XYStage::XYStage() :
   CXYStageBase<XYStage>(),
   initialized_(false), 
   port_("Undefined"), 
   stepSizeXUm_(0.0), 
   stepSizeYUm_(0.0), 
   answerTimeoutMs_(1000),
   originX_(0),
   originY_(0),
   mirrorX_(false),
   mirrorY_(false)

{
   InitializeDefaultErrorMessages();

   // create pre-initialization properties
   // ------------------------------------

   // Name
   //CreateProperty(MM::g_Keyword_Name, g_XYStageDeviceName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Prior XY stage driver adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &XYStage::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

XYStage::~XYStage()
{
   Shutdown();
}

void XYStage::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_XYStageDeviceName);
}

int XYStage::Initialize()
{
   // make sure that we are in compatiblity mode
   ostringstream command;
   command << "comp,0";

   // send command
   int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return false;
   
   
   // set stage step size and resolution
   double resX, resY;
   ret = GetResolution(resX, resY);
   if (ret != DEVICE_OK)
      return ret;

   //if (resX <= 0.0 || resY <= 0.0)
   //   return ERR_INVALID_STEP_SIZE;

   // Since XY stage sometimes returns 0 as the resoloution, we are going to force it to
   // haardcoded value
   if (resX <= 0.0 || resY <= 0.0) {
      resX = 0.1;
      resY = 0.1;
   }

   stepSizeXUm_ = resX;
   stepSizeYUm_ = resY;

   // Step size
   CPropertyAction* pAct = new CPropertyAction (this, &XYStage::OnStepSizeX);
   CreateProperty("StepSizeX_um", "0.0", MM::Float, true, pAct);
   pAct = new CPropertyAction (this, &XYStage::OnStepSizeY);
   CreateProperty("StepSizeY_um", "0.0", MM::Float, true, pAct);

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;
   return DEVICE_OK;
}

int XYStage::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool XYStage::Busy()
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return false; // this is an error, but here we have no choice but to return "not busy"

   const char* command = "$";

   // send command
   ret = SendSerialCommand(port_.c_str(), command, "\r");
   if (ret != DEVICE_OK)
      return false;

   // block/wait for acknowledge, or until we time out;
   string answer="";//jizhen 4/12/2007 == 1)
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return false;
   
   //jizhen 4/12/2007
   if (answer.length() >=1) //jizhen 4/12/2007 == 1)
   {
      int status = atoi(answer.substr(0,1).c_str());
	  int statusX = status&1;
	  int statusY = status&2;
	  if (statusX || statusY) return true;
	  else return false;
	  //return status == 1 ? true : false;
	  //eof jizhen
   }

   return false;
}
 
int XYStage::SetPositionSteps(long x, long y)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "G," << x << "," << y;

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,1).compare("R") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;   
}
 
int XYStage::SetRelativePositionSteps(long x, long y)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "GR," << x << "," << y;

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,1).compare("R") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;   
}

int XYStage::GetPositionSteps(long& x, long& y)
{
   int ret = GetPositionStepsSingle('X', x);
   if (ret != DEVICE_OK)
      return ret;

   return GetPositionStepsSingle('Y', y);
}
 
int XYStage::Home()
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

	//jizhen 4/12/2007
	// do home command
   ret = SendSerialCommand(port_.c_str(), "SIS", "\r"); // command HOME
    if (ret != DEVICE_OK)
      return ret;

   ret = SendSerialCommand(port_.c_str(), "SIS", "\r"); // command HOME
    if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,1).compare("R") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;  //DEVICE_OK; 
   // eof jizhen

   //return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::Stop()
{
	//jizhen 4/12/2007
   int ret = SendSerialCommand(port_.c_str(), "K", "\r"); // command HALT the movement
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,1).compare("R") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER; //DEVICE_OK; 
   //eof jizhen

   //return DEVICE_UNSUPPORTED_COMMAND;
}

//jizhen 4/12/2007
int XYStage::SetOrigin()
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   // send command
   ret = SendSerialCommand(port_.c_str(), "PS,0,0", "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,1).compare("0") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER; 
}
//eof jizhen

// NOTE: SetOrigin was removed from the MMDevice interface but still may be used later
//int XYStage::SetOrigin()
//{
//   // send command
//   int ret = SendSerialCommand(port_.c_str(), "PS,0,0", "\r");
//   if (ret != DEVICE_OK)
//      return ret;
//
//   // block/wait for acknowledge, or until we time out;
//   string answer;
//   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
//   if (ret != DEVICE_OK)
//      return ret;
//
//   if (answer.substr(0,1).compare("0") == 0)
//   {
//      return DEVICE_OK;
//   }
//   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
//   {
//      int errNo = atoi(answer.substr(2).c_str());
//      return ERR_OFFSET + errNo;
//   }
//
//   return ERR_UNRECOGNIZED_ANSWER;   
//}
 
int XYStage::GetLimitsUm(double& /*xMin*/, double& /*xMax*/, double& /*yMin*/, double& /*yMax*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::GetStepLimits(long& /*xMin*/, long& /*xMax*/, long& /*yMin*/, long& /*yMax*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int XYStage::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int XYStage::OnStepSizeX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(stepSizeXUm_);
   }

   return DEVICE_OK;
}

int XYStage::OnStepSizeY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(stepSizeYUm_);
   }

   return DEVICE_OK;
}

// XYStage utility functions
int XYStage::GetResolution(double& resX, double& resY)
{
   // This is a little unclear in the manual, it is possible that this works on sopme controllers, not others
   const char* commandX="RES,s";
   //const char* commandY="RES,Y";

   int ret = GetDblParameter(commandX, resX);
   if (ret != DEVICE_OK)
      return ret;
   resY = resX;

   /*
   ret = GetDblParameter(commandY, resY);
   if (ret != DEVICE_OK)
      return ret;
   */

   return ret;
}

int XYStage::GetDblParameter(const char* command, double& param)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   // send command
   ret = SendSerialCommand(port_.c_str(), command, "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 1).compare("E") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      param = atof(answer.c_str());
      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

int XYStage::GetPositionStepsSingle(char axis, long& steps)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "P" << axis;

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
   {
      return ret;
   }

   if (answer.length() > 2 && answer.substr(0, 1).compare("E") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      steps = atol(answer.c_str());
      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

///////////////////////////////////////////////////////////////////////////////
// ZStage

ZStage::ZStage() :
   initialized_(false),
   port_("Undefined"),
   stepSizeUm_(0.1),
   answerTimeoutMs_(1000)
{
   InitializeDefaultErrorMessages();

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_ZStageDeviceName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Prior Z-stage driver adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &ZStage::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

ZStage::~ZStage()
{
   Shutdown();
}

void ZStage::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_ZStageDeviceName);
}

int ZStage::Initialize()
{
   // set stage step size and resolution
   double res;
   int ret = GetResolution(res);
   if (ret != DEVICE_OK)
      return ret;

   if (res <= 0.0)
      return ERR_INVALID_STEP_SIZE;

   stepSizeUm_ = res;

   ret = GetPositionSteps(curSteps_);
   if (ret != DEVICE_OK)
      return ret;

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;
   return DEVICE_OK;
}

int ZStage::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool ZStage::Busy()
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return false;

   const char* command = "$";

   // send command
   ret = SendSerialCommand(port_.c_str(), command, "\r");
   if (ret != DEVICE_OK)
      return false;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return false;

   if (answer.length() == 1)
   {
      // Z axis status is in bit 3:
      int status = atoi(answer.c_str()) & 4;
      return status > 0 ? true : false;
   }

   return false;
}

int ZStage::SetPositionUm(double pos)
{
   long steps = (long) (pos / stepSizeUm_ + 0.5);
   return SetPositionSteps(steps);
}

int ZStage::GetPositionUm(double& pos)
{
   long steps;
   int ret = GetPositionSteps(steps);
   if (ret != DEVICE_OK)
      return ret;
   pos = steps * stepSizeUm_;
   return DEVICE_OK;
}
  
int ZStage::SetPositionSteps(long pos)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   long delta = pos - curSteps_;
   ostringstream command;
   if (delta >= 0)
      command << "U," << delta;
   else
      command << "D," << -delta;

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0,1).compare("R") == 0)
   {
      curSteps_ = pos;
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;   
}
  
int ZStage::GetPositionSteps(long& steps)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   const char* command="PZ";

   // send command
   ret = SendSerialCommand(port_.c_str(), command, "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
   {
      // failed reading the port
      return ret;
   }

   if (answer.length() > 2 && answer.substr(0, 1).compare("E") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      steps = atol(answer.c_str());
      curSteps_ = steps;
      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

int ZStage::GetResolution(double& res)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   const char* command="RES,Z";

   // send command
   ret = SendSerialCommand(port_.c_str(), command, "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.length() > 2 && answer.substr(0, 1).compare("E") == 0)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      res = atof(answer.c_str());
      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

int ZStage::SetOrigin()
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int ZStage::GetLimits(double& /*min*/, double& /*max*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int ZStage::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// BasicController device  >> NOT FINISHED
///////////////////////////////////////////////////////////////////////////////

BasicController::BasicController() :
   initialized_(false),
   port_("Undefined"),
   answerTimeoutMs_(1000)
{
   InitializeDefaultErrorMessages();

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_BasicControllerName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Prior controller adtapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &BasicController::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

}

BasicController::~BasicController()
{
   Shutdown();
}

void BasicController::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_BasicControllerName);
}

int BasicController::Initialize()
{
   // set property list
   // -----------------
   
   // command
   CPropertyAction *pAct = new CPropertyAction (this, &BasicController::OnCommand);
   CreateProperty("Command", "", MM::String, false, pAct);

   pAct = new CPropertyAction (this, &BasicController::OnResponse);
   CreateProperty("Response", "", MM::String, true, pAct);

   int ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   // switch to selected command protocol;
   const unsigned cmdLen = 2;
   unsigned char cmd[cmdLen];

   // set-up high level commands
   cmd[0] = (unsigned char)0xFF;
   cmd[1] = (unsigned char)65;
   ret = WriteToComPort(port_.c_str(), cmd, cmdLen);
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;
   return DEVICE_OK;
}

int BasicController::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool BasicController::Busy()
{
   const char* cmd = "STATUS\r";

   // send command
   if (DEVICE_OK != WriteToComPort(port_.c_str(), (unsigned char*)cmd, (unsigned)strlen(cmd)))
      return false; // can't write so say we're not busy

   char status=0;
   unsigned long read=0;
   int numTries=0;
   do {
      if (DEVICE_OK != ReadFromComPort(port_.c_str(), (unsigned char*)&status, 1, read))
         return false;
      numTries++;
   } while(read==0 && numTries < 3);

   if (status == 'B')
      return true;
   else
      return false;
}
    
int BasicController::ExecuteCommand(const string& cmd, string& response)
{
   // send command
   PurgeComPort(port_.c_str());
   if (DEVICE_OK != WriteToComPort(port_.c_str(), (unsigned char*)cmd.c_str(), (unsigned) cmd.length()))
      return DEVICE_SERIAL_COMMAND_FAILED;

   // block/wait for acknowledge, or until we time out;
   const unsigned long bufLen = 80;
   char answer[bufLen];
   unsigned curIdx = 0;
   memset(answer, 0, bufLen);
   unsigned long read;
   unsigned long startTime = GetClockTicksUs();

   char* pLF = 0;
   do {
      if (DEVICE_OK != ReadFromComPort(port_.c_str(), (unsigned char*)(answer + curIdx), bufLen - curIdx, read))
         return DEVICE_SERIAL_COMMAND_FAILED;
      curIdx += read;

      // look for the LF
      pLF = strstr(answer, "\n");
      if (pLF)
         *pLF = 0; // terminate the string
   }
   while(!pLF && (GetClockTicksUs() - startTime) / 1000.0 < answerTimeoutMs_);

   if (!pLF)
      return DEVICE_SERIAL_TIMEOUT;

   response = answer;
   return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int BasicController::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int BasicController::OnCommand(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(command_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(command_);
      return ExecuteCommand(command_, response_);
   }

   return DEVICE_OK;
}

int BasicController::OnResponse(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(response_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      assert(!"Read-only property modified");
   }

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Lumen 
// ~~~~~

Lumen::Lumen() :
   initialized_(false), changedTime_(0.0), intensity_(100),
   curState_(false)
{
   InitializeDefaultErrorMessages();
   SetErrorText(ERR_UNRECOGNIZED_ANSWER, "Unrecognized answer recived from the device");

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_LumenName, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Prior Lumen 200Pro", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &Lumen::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

   EnableDelay();

   UpdateStatus();
}

Lumen::~Lumen()
{
   Shutdown();
}

void Lumen::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_LumenName);
}

int Lumen::Initialize()
{
   // set property list
   // -----------------
   
   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &Lumen::OnState);
   int ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   AddAllowedValue(MM::g_Keyword_State, "0");
   AddAllowedValue(MM::g_Keyword_State, "1");

   // Delay
   // -----
   pAct = new CPropertyAction (this, &Lumen::OnDelay);
   ret = CreateProperty(MM::g_Keyword_Delay, "0.0", MM::Float, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   // Intensity
   // ---------
   const char* intensityPropName = "Intensity";
   pAct = new CPropertyAction (this, &Lumen::OnIntensity);
   ret = CreateProperty(intensityPropName, "100", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   // set initial values
   SetProperty(MM::g_Keyword_State, curState_ ? "1" : "0");

   // Set Time for Busy flag
   changedTime_ = GetCurrentMMTime();

   initialized_ = true;

   return DEVICE_OK;
}

int Lumen::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool Lumen::Busy()
{
   MM::MMTime interval = GetCurrentMMTime() - changedTime_;
   MM::MMTime delay(GetDelayMs()*1000.0);
   if (interval < delay)
      return true;
   else
      return false;
}

int Lumen::SetOpen(bool open)
{
   long pos;
   if (open)
      pos = 1;
   else
      pos = 0;
   return SetProperty(MM::g_Keyword_State, CDeviceUtils::ConvertToString(pos));
}

int Lumen::GetOpen(bool& open)
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_State, buf);
   if (ret != DEVICE_OK)
      return ret;
   long pos = atol(buf);
   pos == 1 ? open = true : open = false;

   return DEVICE_OK;
}
int Lumen::Fire(double /*deltaT*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

/**
 * Sends an open/close command through the serial port.
 */
int Lumen::SetShutterPosition(bool state)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "Light," << (state ? intensity_ : 0);

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   // Set timer for Busy signal
   changedTime_ = GetCurrentMMTime();

   if (answer.substr(0,1).compare("R") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int Lumen::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // will return cached value
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);
      curState_ = pos == 0 ? false : true;

      // apply the value
      return SetShutterPosition(curState_);
   }

   return DEVICE_OK;
}

int Lumen::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int Lumen::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(this->GetDelayMs());
   }
   else if (eAct == MM::AfterSet)
   {
      double delay;
      pProp->Get(delay);
      this->SetDelayMs(delay);
   }

   return DEVICE_OK;
}

int Lumen::OnIntensity(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(intensity_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(intensity_);
      if (curState_)
         return SetShutterPosition(curState_);
   }

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// TTLShutter 
// ~~~~~~~

TTLShutter::TTLShutter(const char* name, int id) : 
   name_(name), 
   initialized_(false), 
   id_(id), 
   changedTime_(0.0)
{
   InitializeDefaultErrorMessages();
   SetErrorText(ERR_UNRECOGNIZED_ANSWER, "Unrecognized answer received from the device");

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Prior TTL shutter adapter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &TTLShutter::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

   EnableDelay();

   UpdateStatus();
}

TTLShutter::~TTLShutter()
{
   Shutdown();
}

void TTLShutter::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

int TTLShutter::Initialize()
{
   // set property list
   // -----------------
   
   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &TTLShutter::OnState);
   int ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   AddAllowedValue(MM::g_Keyword_State, "0"); // low = closed?
   AddAllowedValue(MM::g_Keyword_State, "1"); // high = open?

   // Delay
   // -----
   pAct = new CPropertyAction (this, &TTLShutter::OnDelay);
   ret = CreateProperty(MM::g_Keyword_Delay, "0.0", MM::Float, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   // Set timer for Busy signal
   changedTime_ = GetCurrentMMTime();

   initialized_ = true;

   return DEVICE_OK;
}

int TTLShutter::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool TTLShutter::Busy()
{
   MM::MMTime interval = GetCurrentMMTime() - changedTime_;
   MM::MMTime delay(GetDelayMs()*1000.0);
   if (interval < delay)
      return true;
   else
      return false;
}

int TTLShutter::SetOpen(bool open)
{
   long pos;
   if (open)
      pos = 1;
   else
      pos = 0;
   return SetProperty(MM::g_Keyword_State, CDeviceUtils::ConvertToString(pos));
}

int TTLShutter::GetOpen(bool& open)
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_State, buf);
   if (ret != DEVICE_OK)
      return ret;
   long pos = atol(buf);
   pos == 1 ? open = true : open = false;

   return DEVICE_OK;
}
int TTLShutter::Fire(double /*deltaT*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

/**
 * Sends an open/close command through the serial port.
 */
int TTLShutter::SetShutterPosition(bool state)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "TTL," << id_ << "," << (state ? "1" : "0");

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   // Set time for Busy signal
   changedTime_ = GetCurrentMMTime();

   // Although not documented, the ProScan II controller I have here returns '0'
   if (answer.substr(0,1).compare("0") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

/**
 * Check the state of the shutter.
 */
int TTLShutter::GetShutterPosition(bool& state)
{
   // First Clear serial port from previous stuff
   int ret = ClearPort(*this, *GetCoreCallback(), port_);
   if (ret != DEVICE_OK)
      return ret;

   ostringstream command;
   command << "TTL," << id_ << ",?";

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      return ERR_OFFSET + errNo;
   }
   else if (answer.length() > 0)
   {
      if (answer.substr(0,1).compare("1") == 0)
         state = true;
      else
         state = false;
      return DEVICE_OK;
   }

   return ERR_UNRECOGNIZED_ANSWER;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int TTLShutter::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      bool state;
      int ret = GetShutterPosition(state);
      if (ret != DEVICE_OK)
         return ret;
      if (state)
         pProp->Set(1L);
      else
         pProp->Set(0L);
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);

      // apply the value
      return SetShutterPosition(pos == 0 ? false : true);
   }

   return DEVICE_OK;
}

int TTLShutter::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int TTLShutter::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(this->GetDelayMs());
   }
   else if (eAct == MM::AfterSet)
   {
      double delay;
      pProp->Get(delay);
      this->SetDelayMs(delay);
   }

   return DEVICE_OK;
}
