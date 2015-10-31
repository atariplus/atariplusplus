/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartrom.hpp,v 1.30 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Administration/loading of cartidges
 **********************************************************************************/

#ifndef CARTROM_HPP
#define CARTROM_HPP

/// Includes
#include "chip.hpp"
#include "memcontroller.hpp"
#include "rompage.hpp"
#include "exceptions.hpp"
#include "list.hpp"
#include "cartridge.hpp"
///

/// Forwards
class Monitor;
class ArgParser;
class Machine;
///

/// Class CartROM
// This class implements the loading of various cartidges. It does not
// handle the cartcontrol logic, though; this is handled by the cart-
// control class and the MMU class.
class CartROM : public Chip, public MemController {
  //
  // A little helper to print an exception
  class ExceptionForward : public ExceptionPrinter {
    class Machine *Machine;
    //
  public:
    ExceptionForward(class Machine *mach)
      : Machine(mach)
    { }
    ~ExceptionForward(void)
    { }
    //
    // The following method will be called to print out the exception.
    virtual void PrintException(const char *fmt,...) PRINTF_STYLE;
    //
  };
  //
  // The list of inserted cartridges. This array controls the lifetime
  // of the cartridges.
  List<Cartridge>                    CartList;
  //
  // The left cartridge slot, rather: The cart inserted there.
  class Cartridge                   *LeftCart;
  // The right cartridge slot, or rather: What's in here.
  class Cartridge                   *RightCart;
  // The real-time cart is special in the sense that it is
  // a pass-thru cartridge that can be inserted into the
  // left slot aLONG with another cart. We keep it here.
  class Cartridge                   *RealTimeCart;
  //
  // The (not yet) loaded cartridge. This is a temporary that
  // keeps the cart as LONG as it is not inserted.
  class Cartridge                   *newcart;
  //
  // Cart preferences:
  // name of the cart image
  char                              *cartpath;
  // name of the image to be loaded next.
  char                              *cartinsert;
  // boolean that checks whether the RTime8 pass-thru cart should be
  // made available.
  bool                               insertrtime8;
  //
  // Boolean whether the cart was changed and must be swapped
  // on the next reset.
  bool                               swapcarts;
  //
  // The machine type we used to build the cartridge type. In case
  // the cart type changes, we must rebuild the selection vector.
  Machine_Type                       machtype;
  //
  // A selection vector of possible cartridge types
  // we may select from for a given size.
  struct ArgParser::SelectionVector *possiblecarts;
  //
  // Type of the cart to loaded
  Cartridge::CartTypeId              carttoload;
  // Size of the cart to load. Some cart types come in variable
  // sizes requiring this as an argument.
  LONG                               cartsize;
  // Set if the cart to load has a header we need to skip
  bool                               skipheader;
  //
  // Fill the cart ROM from an image file
  // If skipheader is true, the atari800 CART type header is skipped
  void LoadFromFile(const char *path,bool skipheader);
  //
  // Guess the cart type from the path name by looking at its
  // length. If "withheader" is set, then this cart type has been guessed
  // from an atari800 type header and is for sure correct.
  // Size is the size of the image in bytes. This might be required
  // for cart types that comes in variable sizes.
  Cartridge::CartTypeId GuessCartType(const char *path,bool &withheader,LONG &size);
  //
  // Return the name identified by the cart type
  const char *CartTypeName(void);
  //
  // Check for the cart type we previously selected, and build, as a prototype, the
  // mentioned cart. It also requires the cart size in bytes since some cartridges
  // come in variable sizes.
  class Cartridge *BuildCart(Cartridge::CartTypeId carttype,LONG size);
  //
  // Fill or enhance the selection vector by the given
  // name-value pair.
  void AddSelectionVector(LONG &cnt,bool withheader,LONG size,LONG type,const UWORD *possiblesize,
			  struct ArgParser::SelectionVector *&vec,
			  LONG newtype,const char *name);
  //
  // Build the selection vector for all
  // possible carts we can select from, or just count them.
  LONG BuildSelectionVector(bool withheader,LONG size,LONG type);
  //
  // Remove the cartridge in the indicated slot
  void RemoveSlot(class Cartridge *&slot);
  //
  //
public:
  CartROM(class Machine *mach);
  ~CartROM(void);
  //
  //
  // We include here the interface routines imported from the chip class.
  //
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // The argument parser: Pull off arguments specific to this class.
  virtual void ParseArgs(class ArgParser *args);
  virtual void DisplayStatus(class Monitor *mon);
  //
  //
  // Implementations of the MemController features
  virtual void Initialize(void);
  //
  // NOTE: Snapshot activity is not taken here, but is part of the
  // cartCtrl logic which triggers all the cartrom state saving
  // activity.
  //
  // General service: Return the first cart currently inserted, hence
  // the head of the cart list.
  class Cartridge *Cart(void) const
  {
    return CartList.First();
  }
};
///

///
#endif
