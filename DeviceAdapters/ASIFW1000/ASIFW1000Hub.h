///////////////////////////////////////////////////////////////////////////////
// FILE:          ASIFW1000Hub.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   ASIFW1000 hub module. Required for operation of all 
//                ASIFW1000 devices
//                
// COPYRIGHT:     University of California, San Francisco, 2006
//                All rights reserved
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
// AUTHOR: Nico Stuurman, nico@cmp.ucsf.edu, 02/02/2007                                                 
//                                                                                   
// Based on NikonTE2000 controller adapter by Nenad Amodaj                           
// 
//
#ifndef _ASIFW1000HUB_H_
#define _ASIFW1000HUB_H_

//#include <deque>
#include <map>
#include "../../MMDevice/MMDevice.h"

/////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_NOT_CONNECTED           11002
#define ERR_COMMAND_CANNOT_EXECUTE  11003
#define ERR_NO_ANSWER               11004


enum CommandMode
{
   Sync = 0,
   Async
};

class ASIFW1000Hub
{
public:
   ASIFW1000Hub();
   ~ASIFW1000Hub();

   void SetPort(const char* port) {port_ = port;}
   int GetVersion(MM::Device& device, MM::Core& core, char* version);
   bool IsBusy();

   int OpenShutter(MM::Device& device, MM::Core& core, int shutterNr);
   int CloseShutter(MM::Device& device, MM::Core& core, int shutterNr);
   int GetShutterPosition(MM::Device& device, MM::Core& core, int shutterNr, bool& pos);

   int SetFilterWheelPosition(MM::Device& device, MM::Core& core, int wheelNr, int pos);
   int GetFilterWheelPosition(MM::Device& device, MM::Core& core, int wheelNr, int& pos);
   int SetCurrentWheel(MM::Device& device, MM::Core& core, int wheelNr);
   int GetNumberOfPositions(MM::Device& device, MM::Core& core, int wheelNr, int& nrPos);
   int FilterWheelBusy(MM::Device& device, MM::Core& core, bool& busy);

private:
   int ExecuteCommand(MM::Device& device, MM::Core& core, const char* command);
   int GetAcknowledgment(MM::Device& device, MM::Core& core);
   int ParseResponse(const char* cmdId, std::string& value);
   void FetchSerialData(MM::Device& device, MM::Core& core);

   static const int RCV_BUF_LENGTH = 1024;
   char rcvBuf_[RCV_BUF_LENGTH];
   char asynchRcvBuf_[RCV_BUF_LENGTH];
   void ClearRcvBuf();
   void ClearAllRcvBuf(MM::Device& device, MM::Core& core);
   std::string port_;
   std::vector<char> answerBuf_;
   std::multimap<std::string, long> waitingCommands_;
   long expireTimeUs_;
   std::string commandMode_;
   long activeWheel_;
};

#endif // _ASIFW1000HUB_H_