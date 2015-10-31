/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: argparser.cpp,v 1.18 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: Generic argument parser for configuration file and command line
 **********************************************************************************/

/// Includes
#include "string.hpp"
#include <ctype.h>

#include "argparser.hpp"
#include "exceptions.hpp"
///

/// ArgParser::ArgParser
ArgParser::ArgParser(bool help)
  : givehelp(help), ArgChangeFlag(NoChange)
{ 
}
///

/// ArgParser::~ArgParser
ArgParser::~ArgParser(void)
{
}
///

/// ArgParser::Matches
// Returns true if the string passed in matches the other string
// Note that this is differnent to what strcmp does.
bool ArgParser::Matches(const char *match,const char *str)
{
  if (!strcasecmp(match,str))
    return true;
  return false;
}
///

/// ArgParser::MatchesBool
// Evaluate a boolean true/false condition. Returns false
// if the result is invalid.
bool ArgParser::MatchesBool(const char *str,bool &result)
{
  LONG val;

  if (Matches(str,"true") || Matches(str,"on") || Matches(str,"yes")) {
    result = true;
    return true;
  }  
  if (Matches(str,"false") || Matches(str,"off") || Matches(str,"no")) {
    result = false;
    return true;
  }
  if (MatchesLong(str,val)) {
    if (val != 0) {
      result = true;
    } else {
      result = false;
    }
    return true;
  }

  if (Matches(str,"on")) {
    result = true;
    return true;
  }

  return false;
}
///

/// ArgParser::MatchesLong
// Evaluate an integer argument. Returns false if the result
// is invalid.
bool ArgParser::MatchesLong(const char *str,LONG &result)
{
  LONG res;
  char *endptr;

  res = strtol(str,&endptr,10);
  if (*endptr == '\0') {
    result = res;
    return true;
  }
  return false;
}
///

/// ArgParser::SignalBigChange
// Signal a change in the argument change flag, i.e. prepare to re-read
// some arguments if this is required. This method is here for client
// purposes that may require to enforce an argument re-parsing.
void ArgParser::SignalBigChange(ArgumentChange changeflag)
{
  if (changeflag > ArgChangeFlag) {
    ArgChangeFlag = changeflag;
  }
}
///
