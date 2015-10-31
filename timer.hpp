/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: timer.hpp,v 1.17 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Support for various timing related issues
 **********************************************************************************/

#ifndef TIMER_HPP
#define TIMER_HPP

/// Includes
#include "types.h"
#include "time.hpp"
///

/// Typedefs
// use this for files we may use the timer to wait on.
// for linux, this is just a plain file handle and we
// may use select() on it.
typedef int AsyncFileHandle;

#ifndef HAS_STRUCT_TIMEVAL
struct timeval {
  int tv_sec;       /* Seconds.  */
  int tv_usec;      /* Microseconds.  */
};
#endif
///

/// Class Timer
// This class is used to keep precise timing amongst various implementations.
// It might be required that you need to modify this for your operating
// system. The following implementation is unix.
class Timer {
  //
  // This is an abstraction of the timeval structure
  // in case the os doesn't provide one.
  struct Timeval : public timeval {
    //
    // Get the time of the day. This gets implemented by the
    // Os abstraction.
    void GetTimeOfDay(void);
    //
    // Wait a specific time
    void Delay(void);
    //
    // Wait for a specific timeout for the indicated
    // output file to complete
    // Return 0 on timeout, or non-zero if the file
    // I/O returned.
    int WaitForOutput(AsyncFileHandle file);
  };
  //
public:
  //
#ifdef HAS_MEMBER_INIT
  static const long UsecsPerSec = 1000000; // micro seconds in a second
#else
  static const long UsecsPerSec;
#endif
  //
private:
  // This is kept as a timer reference
  struct Timeval    Reference;
  //
  // In case we need to run a timing event once a while, the following
  // keeps the duration of each event.
  struct Timeval    Increment;
  //
  // Private: Add two timevals. Both inputs must be normalized, i.e.
  // tv_usec must be smaller than UsecsPerSec
  static void AddToTime(struct Timeval *addto,const struct Timeval *increment)
  {
    addto->tv_usec += increment->tv_usec;
    addto->tv_sec  += increment->tv_sec;
    while(addto->tv_usec >= UsecsPerSec) {
      addto->tv_usec -= UsecsPerSec;
      addto->tv_sec++;
    }
  }
  //
  // Subtract a time from another. The result might be signed, but is normalized,
  // usecs are always positive and smaller than UsecsPerSec
  // The input has to be normalized as well.
  static void SubtractFromTime(struct Timeval *subfrom,const struct Timeval *decrement)
  {
    subfrom->tv_usec = decrement->tv_usec - subfrom->tv_usec;
    subfrom->tv_sec  = decrement->tv_sec  - subfrom->tv_sec;
    //
    // Handle carry in either direction: Normalize the usecs to be >= 0 but
    // smaller than the result.
    while(subfrom->tv_usec >= UsecsPerSec) {
      subfrom->tv_usec -= UsecsPerSec;
      subfrom->tv_sec++;
    }    
    while(subfrom->tv_usec < 0) {
      subfrom->tv_usec += UsecsPerSec;
      subfrom->tv_sec--;
    }
  }
  //
  // Return true if the first time stamp is later or equal than the second.
  static bool IsLater(const struct Timeval *t1,const struct Timeval *t2)
  {
    if (t1->tv_sec > t2->tv_sec) {
      return true; // definitely later.
    }
    // 
    if (t1->tv_sec == t2->tv_sec) {
      // Need to check closer
      if (t1->tv_usec >= t2->tv_usec)
	return true;
    }
    return false; // is not.
  }
  //
  // Normalize a timer value such that the usecs is in range.
  static void NormalizeTimeVal(struct Timeval *t)
  {
    t->tv_sec  += (t->tv_usec) / UsecsPerSec;
    t->tv_usec  = (t->tv_usec) % UsecsPerSec;
  }
  // 
public:
  // Constructor and destructor
  Timer(void);
  ~Timer(void);
  //
  // Run a timing event once a while: Initialize the timer for this mode.
  // The period is given in a seconds/microseconds pair.
  void StartTimer(long secs,long usecs);
  //
  //
  // Check whether the periodic waiting event has already passed over.
  // Returns true if so.
  bool EventIsOver(void);
  //
  // Wait until the specified period is over or return immediately
  // if the event is over already.
  void WaitForEvent(void);
  //
  // Wait until either the timer event happens or the async IO returns
  // This call returns true if the IO happens earlier than the device.
  bool WaitForIO(AsyncFileHandle file);
  //
  // Check whether the IO handle returned already. Do not wait for
  // any time. This is similar to WaitForIO except that we do not
  // wait at all.
  static bool CheckIO(AsyncFileHandle file);
  //
  // TriggerNextEvent: trigger/advance the timer for the next event
  void TriggerNextEvent(void);
  //
  // Convert the required delay into milliseconds (approximately)
  // This is a pretty rough approximation as it is only 1/1000th of
  // the possible resolution... *sigh*
  long GetMicroDelay(void);
};
///

///
#endif


  
