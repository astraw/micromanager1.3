///////////////////////////////////////////////////////////////////////////////
// FILE:          MMACELogger.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMCore
//-----------------------------------------------------------------------------
// DESCRIPTION:   Implements realization of IMMLogger with ACE logging facility
//
// COPYRIGHT:     University of California, San Francisco, 2007
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
// CVS:           $Id:$
//
#pragma once

#include "IMMLogger.h"
#include <ace/Log_Priority.h>



/**
* class MMACELogger 
* Implements interface IMMLogger with ACE logging facility
*/

class MMACELogger: public IMMLogger
{
public:

   explicit MMACELogger();
   virtual ~MMACELogger();

   /**
   * methods declared in IMMLogger as pure virtual
   * refere to IMMLogger declaration
   */
   bool Initialize(std::string logFileName, std::string logInstanceName)throw(IMMLogger::runtime_exception);
   bool IsValid()throw();
   void Shutdown()throw(IMMLogger::runtime_exception);
   bool Reset()throw(IMMLogger::runtime_exception);
   priority SetPriorityLevel(priority level)throw();
   bool EnableLogToStderr(bool enable)throw();
   IMMLogger::priority  EnableTimeStamp(IMMLogger::priority flags)throw();
   void TimeStamp(IMMLogger::priority level = IMMLogger::info)throw();
   void Log(IMMLogger::priority p, std::string format, ...)throw();
   void SystemLog(std::string format)throw();

private:
   //helpers
   void InitializeInCurrentThread();
   char * GetFormatPrefix(ACE_Log_Priority p);
   void ReportLogFailure()throw();
   //to support legacy 2-level implementation:
   //returns LM_DEBUG or LM_INFO
   ACE_Log_Priority MatchACEPriority(IMMLogger::priority p);

private:
   priority       level_;
   priority       timestamp_level_;
   unsigned long  ACE_Log_flags_;
   std::string    logFileName_;
   std::ofstream * logFile_;
   bool           failureReported;
   std::string    logInstanceName_;
};
