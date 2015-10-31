/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartridge.hpp,v 1.19 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The superclass individual cart types are derived from
 **********************************************************************************/

#ifndef CARTRIDGE_HPP
#define CARTRIDGE_HPP

/// Includes
#include "stdio.hpp"
#include "saveable.hpp"
#include "memcontroller.hpp"
#include "list.hpp"
///

/// Forwards
class ArgParser;
class Machine;
class MMU;
class Monitor;
class SnapShot;
///

/// Class Cartridge
// The cartridge class bundles all activities required to handle a cart
// into one class: Loading from disk, printing its type, cartcontrol
// activity and mapping of its memory.
// Since several carts may be inserted at a time, we need to form a list
// of them.
class Cartridge : public Node<class Cartridge>, public MemController {
  //
protected:
  // Path name the cart was loaded from.
  char *CartPath;
  //
  // Default constructor, does nothing.
  Cartridge(void);
  //
  // Save this cartridge to a given path name. This is a service
  // call for the parameter-less SaveCart() call. If the path is NULL,
  // the same path name as the one used to load the data from.
  void SaveCart(const char *path, bool withheader = false);
  //
public:
  // CartType
  // Various definitions of the cartridge types
  enum CartTypeId {
    Cart_None,            // none inserted
    Cart_8K,              // left  8K cartridge
    Cart_Right8K,         // right 8K cartridge 
    Cart_16K,             // 16K cartridge
    Cart_32K_5200,        // 4K..32K carts with incomplete mapping, only for the 5200
    Cart_32KEE16,         // 16K cart with a special incomplete mapping into the 32K area
    Cart_DB32,            // 32K debug supercart, only for the 5200
    Cart_8KSuperCart,     // OSS 16K supercart with 8K mapping
    Cart_8KSuperCartB,    // OSS 16K supercart with 8K mapping, Atari800 memory dump layout
    Cart_32KSDX,          // 64K supercart with 8K mapping.
    Cart_32KDiamond,      // 64K supercart with 8K mapping.
    Cart_32KExp,          // 64K supercart with 8K mapping.
    Cart_XEGS,            // a variable size bankswitching cart occupying 8K in two banks
    Cart_ExtXEGS,         // an extended version of the above which allows switching it off
    Cart_Will,            // a 32K or 64K supercart with 8K mapping.
    Cart_BBOB,            // bounty bob cartridge
    Cart_RTime8,          // the battery buffered real-time clock
    Cart_Flash,           // Steve's flash ROM
    Cart_Mega,            // Mega ROM cart (16K..1024K)
    Cart_Atrax,           // Atrax cart (128K)
    Cart_Phoenix,         // Phoenix cart (8K) and blizzard cart, (16K) switching carts with simple logic
    Cart_ATMax,           // ATMask cart (128K, 1MB)
    Cart_8KSuperCart8K    // OSS 8K supercart with 8K mapping and bank switching
  }; 
  //
  // The structure of an Atari800 CART type header
  struct CartHeader {
    UBYTE CartID[4];     // the characters 'CART'
    UBYTE Type[4];       // Atari800 encoded Cart type, big endian
    UBYTE ChkSum[4];     // checksum, big endian
    UBYTE reserved[4];   // as said, unused.
  };
  //
  //
  virtual ~Cartridge(void);
  //
  // Return a string identifying the type of the cartridge.
  virtual const char *CartType(void) = 0;
  //
  // Read the contents of this cart from an open file. Headers and other
  // mess has been skipped already here. Throws on failure.
  virtual void ReadFromFile(FILE *fp) = 0;
  //
  // Write the contents of this cart to an open file. Headers have been
  // written already here. Throws on failure. Carts need not to implement
  // this.
  virtual void WriteToFile(FILE *fp) const;
  //
  // Perform a write into the CartCtrl area, possibly modifying the mapping.
  // This never expects a WSYNC. Default is not to perform any operation.
  // It returns, instead, a boolean indicator whether this write was received
  // by the cart and performed any action.
  virtual bool ComplexWrite(class MMU *,ADR,UBYTE)
  {
    return false;
  }
  //
  // Save the file back to the original file in case the
  // cart has been modified.
  virtual void SaveCart(void)
  {
    // By default, nothing to do.
  } 
  //
  // Initialize this memory controller, built its contents.
  virtual void Initialize(void)
  {
    // By default, nothing to do.
  }
  //
  //
  // Perform a read from CartCtrl. Some wierd carts might map user readable
  // values here. By default, this is a write of the "bus noise", meaning
  // 0xff. This method needs not to be implemented as it falls back to
  // the ComplexWrite by default.
  virtual bool ComplexRead(class MMU *mmu,ADR where,UBYTE &value)
  {
    value = 0xff;
    return ComplexWrite(mmu,where,0xff);
  }
  //
  // Remap this cart into the address spaces by using the MMU class.
  // It must know its settings itself, but returns false if it is not
  // mapped. Then the MMU has to decide what to do about it.
  virtual bool MapCart(class MMU *mmu) = 0;
  //
  // Test whether this cart is "available" in the sense that the CartCtl
  // line TRIG3 is pulled. Default result is "yes".
  virtual bool IsMapped(void)
  {
    return true;
  }
  //
  // Display the status over the monitor.
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Perform the snapshot operation for the CartCtrl unit.
  virtual void State(class SnapShot *)
  {
    // default is no internal state at all.
  }
  //
  // Guess the cart type from the path name by looking at its
  // length. If "withheader" is set, then this cart type has been guessed
  // from an atari800 type header and is for sure correct.
  // Size is the size of the image in bytes. This might be required
  // for cart types that comes in variable sizes.
  static CartTypeId GuessCartType(class Machine *mach,FILE *cart,bool &withheader,LONG &length);
  //
  // Check for the cart type we previously selected, and build, as a prototype, the
  // mentioned cart. Since some carts require additional activity for
  // mapping, we also need to pass the machine here.
  static class Cartridge *BuildCart(class Machine *mach,CartTypeId type,LONG size);
  //
  // Load this cartridge from a given path name.
  // Load one or several pages from a file into the Cartridge ROM
  // If skipheader is true, the atari800 CART type header is skipped
  void LoadFromFile(const char *path,bool skipheader); 
};
///

///
#endif
