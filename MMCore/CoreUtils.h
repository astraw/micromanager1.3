///////////////////////////////////////////////////////////////////////////////
// FILE:          CoreUtils.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMCore
//-----------------------------------------------------------------------------
// DESCRIPTION:   Utility classes and functions for use in MMCore
//              
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 09/27/2005
//
// COPYRIGHT:     University of California, San Francisco, 2006
//
// LICENSE:       This file is distributed under the "Lesser GPL" (LGPL) license.
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

#pragma once

#include "../MMDevice/MMDevice.h"
#ifndef _REENTRANT
#define MM_CORE_UTILS_UNDFINE__REENTRANT
#define _REENTRANT
#endif
#include "ace/High_Res_Timer.h"
#include "ace/Log_Msg.h"
#ifdef MM_CORE_UTILS_UNDFINE__REENTRANT
#undef _REENTRANT
#undef MM_CORE_UTILS_UNDFINE__REENTRANT
#endif

#define CORE_LOG_PREFIX "LOG(%P, %t:): "

///////////////////////////////////////////////////////////////////////////////
// Utility classes
// ---------------

class TimeoutMs
{
public:
   TimeoutMs(double intervalMs) : intervalMs_(intervalMs)
   {
      timer_ = new ACE_High_Res_Timer();
      startTime_ = timer_->gettimeofday();
   }
   ~TimeoutMs()
   {
      delete timer_;
   }
   bool expired()
   {
      ACE_Time_Value elapsed = timer_->gettimeofday() - startTime_;
      //CORE_DEBUG2("Elapsed=%d, limit=%d\n", elapsed.usec()/1000, (long)intervalMs_);
      double elapsedMs = (double)(elapsed.sec() * 1000 + elapsed.usec() / 1000);
      if (elapsedMs > intervalMs_)
         return true;
      else
         return false;
   }

private:
   TimeoutMs(const TimeoutMs&) {}
   const TimeoutMs& operator=(const TimeoutMs&) {return *this;}

   double intervalMs_;
   ACE_High_Res_Timer* timer_;
   ACE_Time_Value startTime_;
};

class TimerMs
{
public:
   TimerMs()
   {
      startTime_ = timer_.gettimeofday();
   }
   ~TimerMs()
   {
   }
   double elapsed()
   {
      ACE_Time_Value elapsed = timer_.gettimeofday() - startTime_;
      return (double)(elapsed.sec() * 1000 + elapsed.usec() / 1000);
   }

private:
   TimerMs(const TimeoutMs&) {}
   const TimerMs& operator=(const TimeoutMs&) {return *this;}

   ACE_High_Res_Timer timer_;
   ACE_Time_Value startTime_;
};

inline MM::MMTime GetMMTimeNow()
{
   #ifdef __APPLE__
      struct timeval t;
      gettimeofday(&t,NULL);
      return MM::MMTime(t.tv_sec, t.tv_usec);
   #else
      ACE_High_Res_Timer timer;
      ACE_Time_Value t = timer.gettimeofday();
      return MM::MMTime((long)t.sec(), (long)t.usec());
   #endif
}

