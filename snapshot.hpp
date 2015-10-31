/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: snapshot.hpp,v 1.11 2015/05/21 18:52:43 thor Exp $
 **
 ** This class defines an interface that loads and saves state configurations
 ** to an (possibly external) source.
 ** It is an extension of the argument parser that also allows to save entire
 ** blocks (e.g. memory pages) to an external source.
 **********************************************************************************/

#ifndef SNAPSHOT_HPP
#define SNAPSHOT_HPP

/// Includes
#include "types.hpp"
#include "string.hpp"
#include "argparser.hpp"
///

/// Class SnapShot
// This class extends the argument parser by a method that loads and saves
// entire blocks of continuous data.
class SnapShot : public ArgParser {
  //
  // The following methods from the argparser class are no longer public
  // because they don't make sense here. We overload them with dummies.
  // Print help text over the required output stream.
  virtual void PrintHelp(const char *,...) PRINTF_STYLE;
  //
  // Signal a change in the argument change flag, i.e. prepare to re-read
  // some arguments if this is required. This method is here for client
  // purposes that may require to enforce an argument re-parsing.
  void SignalBigChange(ArgParser::ArgumentChange changeflag);
  //
protected:
  // This class must be derived to work
  SnapShot(void);
public:
  virtual ~SnapShot(void);
  //
  // Print an error formatted to somewhere
  virtual void PrintError(const char *,...)  PRINTF_STYLE = 0;
  //
  // Argument registering methods borrowed from the argparser.
  //
  // Define the module this and the following arguments
  // are good for.
  virtual void DefineTitle(const char *title) = 0;
  //
  // Define a boolean argument, resp. parse it.
  virtual void DefineBool(const char *argname,const char *help,bool &var) = 0;
  //
  // Define a string argument, resp. parse it. This function
  // releases the old string and allocate a new one.
  virtual void DefineString(const char *argname,const char *help,char *&var) = 0;
  //
  // A specialized version of the above, reads file names.
  virtual void DefineFile(const char *argname,const char *help,char *&var,
			  bool forsave,bool filesonly,bool dirsonly) = 0;
  //
  // Define a LONG argument and its range given by min and max.
  // Both are inclusive.
  virtual void DefineLong(const char *argname,const char *help,
			  LONG min,LONG max,LONG &var) = 0;
  //
  // For simplicity, two defineLONG's that take UBYTE or UWORD's as arguments because
  // this is a relatively frequent case.
  void DefineLong(const char *argname,const char *help,LONG min,LONG max,UWORD &var)
  {
    LONG tmp = var;
    DefineLong(argname,help,min,max,tmp);
    var      = UWORD(tmp);
  }
  void DefineLong(const char *argname,const char *help,LONG min,LONG max,UBYTE &var)
  {
    LONG tmp = var;
    DefineLong(argname,help,min,max,tmp);
    var      = UBYTE(tmp);
  }
  //
  // 
  //
  // Define a radio switch type argument given by an array of options and
  // values.
  virtual void DefineSelection(const char *argname,const char *help,
			       const struct SelectionVector selections[],
			       LONG &var) = 0;
  //
  // This is the only additional method here, required to load/save entire blocks
  virtual void DefineChunk(const char *argname,const char *help,UBYTE *mem,size_t size) = 0;
  //
};
///

///
#endif
