#include <string>
#include <iostream>
#include <fstream>

#include "MMACELogger.h"
#include <ace/Log_Msg.h>
#include <ace/Log_Priority.h>
#include <ace/Process.h>
#include <ace/String_Base.h>

#include "CoreUtils.h"
#include "../MMDevice/DeviceThreads.h"

using namespace std;

const char* g_textLogIniFiled = "Logging initialization failed\n";
MMThreadLock g_initializeLoggingLock;

//single instance 
IMMLogger * g_MMLogger = NULL;

MMACELogger::MMACELogger()
:level_(any)
,timestamp_level_(any)
,ACE_Log_flags_(any)
,logFile_(NULL)
,failureReported(false)
{
}

MMACELogger::~MMACELogger()
{
   Shutdown();
   if(NULL != g_MMLogger)
   {
      delete g_MMLogger;
      g_MMLogger = NULL;
   }

}

//static 
IMMLogger * IMMLogger::Instance()throw(IMMLogger::runtime_exception)
{
   if(NULL == g_MMLogger)
   {
      try
      {
         g_MMLogger = new MMACELogger();
      }
      catch(...)
      {
         throw(IMMLogger::runtime_exception(g_textLogIniFiled));
      }
   }
   return g_MMLogger;
};

/**
* methods declared in IMMLogger as pure virtual
* Refere to IMMLogger declaration
*/



bool MMACELogger::IsValid()throw()
{
   return NULL != logFile_ && logFile_->is_open();
};


bool MMACELogger::Initialize(std::string logFileName, std::string logInstanceName)throw(IMMLogger::runtime_exception)
{
   bool bRet =false;
   try
   {
      MMThreadGuard guard(g_initializeLoggingLock);
      failureReported=false;
      logInstanceName_=logInstanceName;
      logFileName_ = logFileName;
      if(NULL == logFile_)
      {
         logFile_ = new std::ofstream();
      }
      if (!logFile_->is_open())
      {
         logFile_->open(logFileName_.c_str(), ios_base::app);
      }

      ACE_Log_flags_ = ACE_LOG_MSG->flags();
      if(logFile_->is_open())
      {
         ACE_Log_flags_ |= ACE_Log_Msg::OSTREAM;
      }
      ACE_Log_flags_ |= ACE_Log_Msg::STDERR;
      ACE_LOG_MSG->set_flags (ACE_Log_flags_);
   }
   catch(...)
   {
      ReportLogFailure();
      throw(IMMLogger::runtime_exception(g_textLogIniFiled));
   }
   return bRet;
};


void MMACELogger::Shutdown()throw(IMMLogger::runtime_exception)
{
   try
   {
      MMThreadGuard guard(g_initializeLoggingLock);
      failureReported=false;

      if(NULL != logFile_)
      {
         logFile_->close();
         delete logFile_;
         logFile_ = NULL;
      }
      ACE_LOG_MSG->clr_flags (ACE_Log_Msg::OSTREAM);
      ACE_LOG_MSG->msg_ostream (0, 0);
   }
   catch(...)
   {
      logFile_ = NULL;
      ReportLogFailure();
      throw(IMMLogger::runtime_exception(g_textLogIniFiled));
   }
}

bool MMACELogger::Reset()throw(IMMLogger::runtime_exception)
{
   MMThreadGuard guard(g_initializeLoggingLock);
   bool bRet =false;
   try{
      failureReported=false;
      if(NULL != logFile_)
      {
         if (logFile_->is_open())
         {
            logFile_->close();
         }
         //re-open same file but truncate old log content
         logFile_->open(logFileName_.c_str(), ios_base::trunc);
         bRet = true;
      }
   }
   catch(...)
   {
      ReportLogFailure();
      throw(IMMLogger::runtime_exception(g_textLogIniFiled));
   }

   return bRet;
};

IMMLogger::priority MMACELogger::SetPriorityLevel(IMMLogger::priority level_flag)throw()
{
   MMThreadGuard guard(g_initializeLoggingLock);
   IMMLogger::priority old_level = level_;
   try
   {
      unsigned long ACE_Process_priority_mask = 0;
      switch(level_flag)
      {
      case  trace:
         ACE_Process_priority_mask |= LM_TRACE; 
      case debug:
         ACE_Process_priority_mask |= LM_DEBUG; 
      default:
      case info:
         ACE_Process_priority_mask |= (LM_INFO|LM_NOTICE); 
      case warning:
         ACE_Process_priority_mask |= LM_WARNING; 
      case error:
         ACE_Process_priority_mask |= LM_ERROR; 
      case alert:
         ACE_Process_priority_mask |= (LM_ALERT|LM_EMERGENCY|LM_CRITICAL); 
      }
      ACE_LOG_MSG->priority_mask (ACE_Process_priority_mask, ACE_Log_Msg::PROCESS);
      level_=old_level;
   }
   catch(...)
   {
      ReportLogFailure();
   }
   return    old_level;
}

bool MMACELogger::EnableLogToStderr(bool enable)throw()
{
   MMThreadGuard guard(g_initializeLoggingLock);
   bool bRet = ACE_Log_flags_ | ACE_Log_Msg::STDERR;

   ACE_Log_flags_ |= ACE_Log_Msg::OSTREAM;
   if (enable)
   {
      ACE_Log_flags_ |= ACE_Log_Msg::STDERR;
   }
   else
   {
      ACE_Log_flags_ &= ~ACE_Log_Msg::STDERR;
   }
   ACE_LOG_MSG->set_flags (ACE_Log_flags_);

   return bRet;
};

IMMLogger::priority  MMACELogger::EnableTimeStamp(IMMLogger::priority level_flags)throw()
{
   MMThreadGuard guard(g_initializeLoggingLock);
   priority old_timestamp_level = timestamp_level_;
   timestamp_level_ = level_flags;
   return old_timestamp_level;
};

//
//Writes a single time stamp record
void MMACELogger::TimeStamp(IMMLogger::priority level)throw()
{
   try
   {
      MMThreadGuard guard(g_initializeLoggingLock);
      InitializeInCurrentThread();
      Log(level, "%D\n");
   }
   catch(...)
   {
      ReportLogFailure();
   }
};

void MMACELogger::Log(IMMLogger::priority p, std::string format, ...)throw()
{
   MMThreadGuard guard(g_initializeLoggingLock);
   try
   {
      InitializeInCurrentThread();
      // Start of variable args section.
      va_list argp;

      va_start (argp, format);

      int __ace_error = ACE_Log_Msg::last_error_adapter ();
      ACE_Log_Msg *ace___ = ACE_Log_Msg::instance ();
      ace___->conditional_set (__FILE__, __LINE__, 0, __ace_error);

      ACE_Log_Priority ace_p = MatchACEPriority(p);
      string strFormat = GetFormatPrefix(ace_p);
      strFormat+=format;
      ace___->log (strFormat.c_str(), ace_p, argp);

      va_end (argp);

   }
   catch(...)
   {
      ReportLogFailure();
   }
};

void MMACELogger::SystemLog(std::string message)throw()
{
   MMThreadGuard guard(g_initializeLoggingLock);
   try
   {
      std::cerr<<message;
   }
   catch(...)
   {
      //nothing to do if cerr filed
   }
};

void MMACELogger::ReportLogFailure()throw()
{
   if(!failureReported)
   {
      failureReported=true;
      SystemLog(g_textLogIniFiled);
   }
};

#define CORE_DEBUG_PREFIX_T "DBG(%D, %P, %t:) "
#define CORE_LOG_PREFIX_T "LOG(%D, %P, %t:): "
#define CORE_DEBUG_PREFIX "DBG(%P, t:%t:) "
#define CORE_LOG_PREFIX "LOG(%P, %t:): "

const char * MMACELogger::GetFormatPrefix(ACE_Log_Priority p)
{
   const char * ret = CORE_DEBUG_PREFIX_T;
   if(MatchACEPriority(timestamp_level_) & p)
   {
      if (p== LM_DEBUG) 
         ret = CORE_DEBUG_PREFIX_T;
      else
         ret = CORE_LOG_PREFIX_T;
   } else
   {
      if (p== LM_DEBUG) 
         ret = CORE_DEBUG_PREFIX;
      else
         ret = CORE_LOG_PREFIX;
   }
   return ret;
}

//to support legacy 2-level implementation:
//returns LM_DEBUG or LM_INFO
ACE_Log_Priority MMACELogger::MatchACEPriority(IMMLogger::priority p)
{
   return p <= debug? LM_DEBUG : LM_INFO; 
}

void MMACELogger::InitializeInCurrentThread()
{
   ACE_LOG_MSG->msg_ostream (NULL != logFile_ && logFile_->is_open()?logFile_:0, 0);
   ACE_LOG_MSG->set_flags (ACE_Log_flags_);
}
