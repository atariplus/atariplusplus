/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: stringgadget.hpp,v 1.4 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a string-entry gadget
 **********************************************************************************/

#ifndef STRINGADGET_HPP
#define STRINGADGET_HPP

/// Includes
#include "gadget.hpp"
///

/// Class StringGadget
// A string gadget that allows to enter strings
class StringGadget : public Gadget {
  //
  // The buffer containing the current text that gets rendered.
  char *Buffer;
  //
  // The undo-buffer containing the previous setting of the option at the
  // time the gadget got created.
  char *UndoBuffer;
  //
  // size of the current buffer in characters (not including the NUL)
  int Size;
  //
  // position of the cursor within the buffer.
  int Cursor;
  //
  // index of the first visible character within the buffer
  int BufPos;
  //
  // number of visible characters in the gadget
  int Visible;
  //
  // Handle keyboard input for the string gadget
  bool HandleKey(struct Event &ev);
  //
public:
  StringGadget(List<Gadget> &gadgetlist,class RenderPort *rp,
	       LONG le,LONG te,LONG w,LONG h,
	       const char *initialvalue);
  ~StringGadget(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Refresh this gadget and all gadgets inside.
  virtual void Refresh(void);
  //
  // Return the current setting of the string gadget without allocating a new
  // buffer for it.
  const char *GetStatus(void) const
  {
    return Buffer;
  }
  //
  // Read the contents of this gadget, allocate a new buffer and fill
  // the result in.
  void ReadContents(char *&var);
  //
  // Set the contents of this gadget.
  void SetContents(const char *var);
};
///

///
#endif
