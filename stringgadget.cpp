/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: stringgadget.cpp,v 1.9 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a string-entry gadget
 **********************************************************************************/

/// Includes
#include "stringgadget.hpp"
#include "string.hpp"
#include "new.hpp"
#include <ctype.h>
///

/// StringGadget::StringGadget
// create a new string gadget from a buffer.
StringGadget::StringGadget(List<Gadget> &gadgetlist,
				 class RenderPort *rp,
				 LONG le,LONG te,LONG w,LONG h,
				 const char *initialvalue)
  : Gadget(gadgetlist,rp,le,te,w,h), Buffer(new char[256]), UndoBuffer(new char[256])
{
  strncpy(Buffer,initialvalue,256);
  strncpy(UndoBuffer,initialvalue,256);
  //
  // Unfortunately, strncpy does not NUL-terminate the buffer. (What a nonsense!)
  // So we have to do this manually!
  Buffer[255]     = 0;
  UndoBuffer[255] = 0;
  //
  // Compute the size of the buffer and the cursor position.
  Size            = (int)strlen(Buffer);
  Cursor          = 0;
  BufPos          = 0;
  Visible         = (Width - 4) >> 3; // expect 8x8 character matrices
}
///

/// StringGadget::~StringGadget
// clean up the string gadget and dispose all buffers
StringGadget::~StringGadget(void)
{
  delete[] Buffer;
  delete[] UndoBuffer;
}
///

/// StringGadget::HitTest
// Perform the hit test for the gadget: Return the gadget contents as soon
// as the gadget is left.
bool StringGadget::HitTest(struct Event &ev)
{
  switch(ev.Type) {
  case Event::Click:
    if (Within(ev) && ev.Button) {
      // We hit the buffer inside the gadget. Hence, activate it.
      Active = true;
      // Re-set the cursor position to the click position. Expect character
      // cells of 8x8 pixels (yuck!)
      Cursor = BufPos + ((ev.X - LeftEdge - 2) >> 3);
      if (Cursor > Size) 
	Cursor = Size;
      // We need to refresh and re-render it now.
      Refresh();
      ev.Type   = Event::GadgetDown;
      ev.Object = this;
      return true;
    } else if (!Within(ev) && ev.Button && Active) {
      // A click outside of the active gadget. This deactivates the thing and
      // returns a valid message aborting the entry.
      Active    = false;
      SetContents(UndoBuffer); // implies a refresh
      ev.Resent = true; // repeat this event: Something else may go down instead.
      ev.Type   = Event::GadgetUp;
      ev.Object = NULL;
      return true;
    }
    return false;
  case Event::Mouse:
    // A mouse movement. We do not care about these at all.
    return false;
  case Event::Keyboard:
    // A key down/up movement. We only care about keyboard down events.
    if (Active && ev.DownUp) {
      return HandleKey(ev);
    } else if (!Active && ev.DownUp == false && (ev.Key == 0x0a  || ev.Key == 0x0d)) {
      // Swallow the event to avoid that the keyboard navigator gets it and
      // re-activates this gadget.
      ev.Type = Event::Nothing;
      return false;
    }
    return false;
  default:
    return false;
  }
}
///

/// StringGadget::HandleKey
// Handle keyboard input for the string gadget. This
// method here only handles "down" events.
bool StringGadget::HandleKey(struct Event &ev)
{    
  char key;
  //
  key = ev.Key;
  // Simple case: control is not held, and the key is printable.
  if (ev.Control == false) {
    if (isprint(key)) {
      // Now insert the character at the current position if there is still some
      // room.
      if (Size < 255) {
	memmove(Buffer+Cursor+1,Buffer+Cursor,Size-Cursor+1);
	Buffer[Cursor] = key;
	Size++;
	Cursor++;
	if (Cursor - BufPos >= Visible) {
	  BufPos++;
	}
	Refresh();
      }
      return true;
    }
    // Check for some special keys: Backspace, erase, return.
    switch(key) {
    case 0x0a:
    case 0x0d:
      // Entry complete. Send a gadgetup, mark the gadget as inactive and
      // let it go.
      ev.Type   = Event::GadgetUp;
      ev.Object = this;
      Active    = false;
      Refresh();
      return true;
    case 0x08:
      // Backspace. Shift+Backspace clears all, Backspace alone just erases
      // the character in front of the cursor.
      if (ev.Shift) {
	// Erase all of the buffer.
	*Buffer = 0;
	Size    = 0;
	Cursor  = 0;
	BufPos  = 0;
      } else {
	if (Cursor > 0) {
	  memmove(Buffer+Cursor-1,Buffer+Cursor,Size-Cursor+1);
	  Size--;
	  Cursor--;
	  if (Cursor - BufPos < 0) {
	    BufPos--;
	  }
	}
      }
      Refresh();
      return true;
    }
  } else {
    // Control is pressed. Atari cursor movement keys are handled here.
    switch(key) {
    case '+': // left movement.
      if (ev.Shift) {
	Cursor = 0;
	BufPos = 0;
	Refresh();
      } else {
	if (Cursor > 0) {
	  Cursor--;
	  if (Cursor - BufPos < 0) {
	    BufPos--;
	  }
	  Refresh();
	}
      }
      ev.Type = Event::Nothing;
      return true;
    case '*': // right movement
      if (ev.Shift) {
	Cursor = Size;
	BufPos = (Cursor - Visible + 1 > 0)?(Cursor - Visible + 1):(0);
	Refresh();
      } else {
	if (Cursor < Size) {
	  Cursor++;
	  if (Cursor - BufPos >= Visible) {
	    BufPos++;
	  }
	  Refresh();
	}
      }
      ev.Type = Event::Nothing;
      return true;
    case 'q': // Buffer undo
    case 'Q':
      strcpy(Buffer,UndoBuffer);
      Size = (int)strlen(Buffer);
      if (Cursor > Size) {
	BufPos = (Cursor - Visible + 1 > 0)?(Cursor - Visible + 1):(0);
	Cursor = Size;
      }
      Refresh();
      return true;
    case 0x08: // rub-out alias delete
      // delete the character under the cursor, then scroll back
      if (ev.Shift) {
	// Erase all of the buffer.
	*Buffer = 0;
	Size    = 0;
	Cursor  = 0;
	BufPos  = 0;
	Refresh();
      } else if (Size > 0 && Cursor < Size) {
	memmove(Buffer+Cursor,Buffer+Cursor+1,Size-Cursor);
	Size--;
	Refresh();
      }
      return true;
    case '<': // clear screen aka "Home"
      Cursor = 0;
      BufPos = 0;
      Refresh();
      return true;
    }
  }
  return false;
}
///

/// StringGadget::Refresh
// Refresh the contents of this string gadget, render the frame, the
// contents and the cursor.
void StringGadget::Refresh(void)
{
  int position;
  char *contents;
  LONG x,y,t;
  //
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,0x0c);
  // Render a double frame around the text to indicate that we
  // may enter text here.
  RPort->Draw3DFrame(LeftEdge,TopEdge,Width,Height,false);
  RPort->Draw3DFrame(LeftEdge+1,TopEdge+1,Width-2,Height-2,true);
  // Now render the text within. We use a dark color for this (black on white).
  RPort->SetPen(0x00);
  contents = Buffer;
  if (strlen(contents) < (size_t)BufPos) {
    // Truncate the position to at most the NUL, i.e. one character behind 
    // the last valid character.
    BufPos = (int)strlen(contents);
  }
  // First character we render to the screen.
  contents += BufPos;
  position  = BufPos;
  x         = LeftEdge + 2;
  y         = TopEdge + 2 + ((Height - 4 - 8) >> 1);
  do {
    char buf[2];
    // Render the NUL as a blank space. This is done here to display the
    // cursor effectively.
    buf[0] = (*contents)?(*contents):(' ');
    buf[1] = 0;
    RPort->Position(x,y);
    // This renders just a single character
    RPort->Text(buf,Active && (position == Cursor));
    // Increment x and the position. Repeat until either the buffer
    // runs out of data, or the gadget runs out of free space.
    RPort->ReadPosition(x,t);
    if (x >= LeftEdge + Width - 4 || *contents == 0)
      break;
    // Continue with the next character.
    contents++;
    position++;
  } while(true);
}
///

/// StringGadget::ReadContents
void StringGadget::ReadContents(char *&var)
{
  delete[] var;
  var = NULL;
  var = new char[strlen(Buffer) + 1];
  strcpy(var,Buffer);
}
///

/// StringGadget::SetContents
// Define the contents of the string gadget
void StringGadget::SetContents(const char *var)
{
  // Copy the contents back into the buffer, then truncate the
  // cursor position and the display position.
  strncpy(Buffer,var,256);
  Buffer[255] = 0; // §&%&$§! strncpy!
  memcpy(UndoBuffer,Buffer,256);
  Size = (int)strlen(Buffer);
  if (BufPos > Size) BufPos = Size;
  if (Cursor > Size) Cursor = Size;
  Refresh();
}
///
