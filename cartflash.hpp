/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartflash.hpp,v 1.10 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of a 128K..8MB flash cart
 ** with the friendly permission of Mark Keates
 **********************************************************************************/

#ifndef CARTFLASH_HPP
#define CARTFLASH_HPP

/// Includes
#include "cartridge.hpp"
#include "memcontroller.hpp"
#include "configurable.hpp"
///

/// Forwards
class AmdChip;
class Machine;
class ArgParser;
class ChoiceRequester;
class FileRequester;
///

/// Class CartFlash
// The CartFlash class implements a Flash ROM supercart.
// These carts come with various sizes that are mapped 
// into the address range 0xa000 to 0xbfff and are controlled
// by the CartCtrl area.
class CartFlash : public Cartridge, public Configurable {
  //
  // Number of banks in this cartridge
  UBYTE                  TotalBanks;
  //
  // Flags whether the cart is enabled.
  bool                   Active;
  //
  // Flag whether this should be disabled on
  // the next reset to flash it.
  bool                   EnableOnReset;
  //
  // The two chips within a flash cartridge.
  class AmdChip         *Rom1,*Rom2;
  //
  // The active bank, i.e. which bank of the chip
  // is mapped into the Cart ara.
  UBYTE                  ActiveBank;
  //
  // The system class required for the AMDChip
  class Machine         *Machine;
  //
  // A requester, used when the cartridge contents changed.
  class ChoiceRequester *RequestSave;
  //
  // Requests the path to save the file to.
  class FileRequester   *SavePath;
  //
public:
  // The cartridge requires the number of banks for construction,
  // unlike other carts.
  CartFlash(class Machine *mach,UBYTE banks);
  virtual ~CartFlash(void);
  // 
  // This static array contains the possible cart sizes
  // for this cart type.
  static const UWORD CartSizes[];
  //
  // Return a string identifying the type of the cartridge.
  virtual const char *CartType(void);
  //
  // Read the contents of this cart from an open file. Headers and other
  // mess has been skipped already here. Throws on failure.
  virtual void ReadFromFile(FILE *fp);
  //
  // Save the file back to the original file in case the
  // cart has been modified.
  virtual void SaveCart(void);
  //
  // Write the contents of this cart to an open file. 
  // Throws on failure. This is flashrom specific.
  virtual void WriteToFile(FILE *fp) const;
  //
  // Remap this cart into the address spaces by using the MMU class.
  // It must know its settings itself, but returns false if it is not
  // mapped. Then the MMU has to decide what to do about it.
  virtual bool MapCart(class MMU *mmu);
  // 
  // Initialize this memory controller, built its contents.
  virtual void Initialize(void);
  //
  // Perform a write into the CartCtrl area, possibly modifying the mapping.
  // This never expects a WSYNC. Default is not to perform any operation.
  // Return code indicates whether this write was useful for this cart.
  virtual bool ComplexWrite(class MMU *mmu,ADR mem,UBYTE val);
  //
  // Display the status over the monitor.
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Perform the snapshot operation for the CartCtrl unit.
  virtual void State(class SnapShot *snap);
  //
  // Define the arguments for the Flash cartridge. This is uniquely a
  // configurable cartridge.
  virtual void ParseArgs(class ArgParser *args);
};
///

///
#endif
