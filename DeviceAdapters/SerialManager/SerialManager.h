///////////////////////////////////////////////////////////////////////////////
// FILE:          SerialManager.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   serial port device adapter 
//                
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 10/21/2005
//
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
// CVS:           $Id$
//

#ifndef _PARALLELPORT_H_
#define _PARALLELPORT_H_

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include <string>
#include <map>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
//#define ERR_UNKNOWN_LABEL 100
#define ERR_OPEN_FAILED 101
#define ERR_SETUP_FAILED 102
#define ERR_HANDSHAKE_SETUP_FAILED 103
#define ERR_TRANSMIT_FAILED 104
#define ERR_RECEIVE_FAILED 105
#define ERR_BUFFER_OVERRUN 106
#define ERR_TERM_TIMEOUT 107
#define ERR_PURGE_FAILED 108
#define ERR_PORT_CHANGE_FORBIDDEN 109

class CSerial;

//////////////////////////////////////////////////////////////////////////////
// Implementation of the MMDevice and MMStateDevice interfaces
//

class SerialPort : public CSerialBase<SerialPort>  
{
public:
   SerialPort(const char* portName);
   ~SerialPort();
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();
  
   void GetName(char* pszName) const;
   bool Busy() {return busy_;}

   int SetCommand(const char* command, const char* term);
   int GetAnswer(char* answer, unsigned bufLength, const char* term);
   int Write(const unsigned char* buf, unsigned long bufLen);
   int Read(unsigned char* buf, unsigned long bufLen, unsigned long& charsRead);
   MM::PortType GetPortType() const {return MM::SerialPort;}    
   int Purge();
   
   // action interface
   // ----------------
   int OnStopBits(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnParity(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnHandshaking(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBaud(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnTimeout(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDelayBetweenCharsMs(MM::PropertyBase* pProp, MM::ActionType eAct);

   int Open();
   void Close();
   void AddReference() {refCount_++;}
   void RemoveReference() {refCount_--;}
   bool OKToDelete() {return refCount_ < 1;}

private:
   std::string portName_;
   std::string portNameWinAPI_;

   bool initialized_;
   bool busy_;
   CSerial* port_;
   double portTimeoutMs_;
   double answerTimeoutMs_;
   int refCount_;
   double transmitCharWaitMs_;
   std::map<std::string, int> baudList_;

   std::string stopBits_;
   std::string parity_;



};

class SerialManager
{
public:
   SerialManager() {}
   ~SerialManager();

   MM::Device* CreatePort(const char* portName);
   void DestroyPort(MM::Device* port);

private:
   std::vector<SerialPort*> ports_;
};


#endif //_PARALLELPORT_H_
