/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartrom.cpp,v 1.53 2020/07/18 15:20:39 thor Exp $
 **
 ** In this module: Administration/loading of cartridges
 **********************************************************************************/

/// Includes
#include "argparser.hpp"
#include "monitor.hpp"
#include "adrspace.hpp"
#include "mmu.hpp"
#include "new.hpp"
#include "cartrom.hpp"
#include "exceptions.hpp"
#include "snapshot.hpp"
#include "cartridge.hpp"
#include "cart8k.hpp"
#include "cart16k.hpp"
#include "cart32k.hpp"
#include "cart32kee16.hpp"
#include "cartoss.hpp"
#include "cartossb.hpp"
#include "cartoss8k.hpp"
#include "cartsdx.hpp"
#include "cartxegs.hpp"
#include "cartbbob.hpp"
#include "cartrt8.hpp"
#include "cartflash.hpp"
#include "cartmega.hpp"
#include "cartatrax.hpp"
#include "cartwill.hpp"
#include "cartdb32.hpp"
#include "cartright8k.hpp"
#include "cartphoenix.hpp"
#include "cartatmax.hpp"
///

/// CartROM::CartROM
CartROM::CartROM(class Machine *mach)
  : Chip(mach,"CartROM"),
    LeftCart(NULL), RightCart(NULL), RealTimeCart(NULL),
    possiblecarts(NULL)
{
  cartpath     = NULL;
  cartinsert   = NULL;
  cartsize     = 0;
  skipheader   = false;
  carttoload   = Cartridge::Cart_None;
  newcart      = NULL;
  insertrtime8 = false;
  swapcarts    = true;
  machtype     = machine->MachType();
}
///

/// CartROM::~CartROM
CartROM::~CartROM(void)
{
  class Cartridge *c,*next;
  // Dispose all cartridges in the cartlist
  next = CartList.First();
  while((c = next)) {
    next = c->NextOf();
    c->Remove();
    delete c;
  }
  delete   newcart;
  delete[] cartpath;
  delete[] cartinsert;
  delete[] possiblecarts;
}
///

/// CartROM::ExceptionForward::ExceptionPrinter
// Print an exception as a warning
void CartROM::ExceptionForward::PrintException(const char *fmt,...)
{
  va_list fmtlist;
  va_start(fmtlist,fmt);
  Machine->VPutWarning(fmt,fmtlist);
  va_end(fmtlist);
}
///

/// CartROM::LoadFromFile
// Load one or several pages from a file into the Cartridge ROM
// If skipheader is true, the atari800 CART type header is skipped
void CartROM::LoadFromFile(const char *path,bool skipheader)
{
  if (newcart) {
    newcart->LoadFromFile(path,skipheader);
  }
}
///

/// CartROM::RemoveSlot
// Remove the cartridge in the indicated slot
void CartROM::RemoveSlot(class Cartridge *&slot)
{
  if (slot) {
    slot->Remove();
    delete slot;
    slot = NULL;
  }
}
///

/// CartROM::Initialize
// Pre-Coldstart-Phase: Load the selected ROM
// then install the patches.
void CartROM::Initialize(void)
{
  class Cartridge *cart;
  // Do we really need to re-insert carts? Avoid to do that
  // since this invalidates the temporary state of the
  // flash rom carts.
  if (swapcarts) {
    // 
    // Check whether the slot has been modified. If so, save it back.
    if (LeftCart) {
      try {
	LeftCart->SaveCart();
      } catch (const class AtariException &ex) {
	class ExceptionForward printer(machine);     
	ex.PrintException(printer);
      }
    }
    // Now check whether we should add the rtime8 cartridge. If so, do now.
    // This cart gets the signals first, and passes all data thru that
    // it does not care about. Hence, it has to go first.
    if (insertrtime8) {
      RemoveSlot(RealTimeCart);
      CartList.AddHead(RealTimeCart = new class CartRT8);
    } else {
      RemoveSlot(RealTimeCart);
    }
    // We do not return an error if we can't. We just
    // have to say so the MMU.
    if (carttoload != Cartridge::Cart_None) {
      // Now build the real cart.
#if CHECK_LEVEL > 0
      if (newcart) 
	Throw(ObjectExists,"CartROM::Initialize","new cart exists already");
#endif
      newcart    = BuildCart(carttoload,cartsize);
      try { 
	// try to load the mentioned file from disk. This may fail, though.
	LoadFromFile(cartpath,skipheader);
	//
	// Check the type of the cart we hold here. This depends on
	// whether we put it into the left or the right slot.
	switch(carttoload) {
	case Cartridge::Cart_Right8K:
	  // Remove the currently loaded right cartridge
	  RemoveSlot(RightCart);
	  RightCart = newcart;
	  break;
	case Cartridge::Cart_RTime8:
	  RemoveSlot(RealTimeCart);
	  RealTimeCart = newcart;
	  break;
	default:
	  // Everything else goes into the left slot
	  RemoveSlot(LeftCart);
	  LeftCart = newcart;
	  break;
	}
	if (newcart)
	  CartList.AddTail(newcart);
	newcart = NULL;
      } catch(const class AtariException &ex) {
	class ExceptionForward printer(machine);     
	carttoload = Cartridge::Cart_None;
	// Retain the previously selected
	// cartridge in case we caught an error with the last
	// one.
	delete newcart;
	newcart    = NULL;
	ex.PrintException(printer);
      }
    } else {      
      // Cart type is none. This empties left and right slot, but
      // does not remove the real time cart.
      RemoveSlot(LeftCart);
      RemoveSlot(RightCart);
    }
  }
  //
  // Initialize all carts installed here.
  for(cart = CartList.First();cart;cart = cart->NextOf()) {
    cart->Initialize();
  }
  //
  // Adjust the cart area in the MMU now.
  machine->MMU()->BuildCartArea();
  //
  // Carts swapped successfully.
  swapcarts = false;
}
///

/// CartROM::WarmStart
// Run a warmstart. Nothing happens here
// CartCtrl is a separate module with a separate reset line that
// gets reset on its own.
void CartROM::WarmStart(void)
{
}
///

/// CartROM::ColdStart
// Nothing happens here.
void CartROM::ColdStart(void)
{ 
  // Check whether the slot has been modified. If so, save it back.
  if (LeftCart) {
    try {
      LeftCart->SaveCart();
    } catch (const class AtariException &ex) {
      class ExceptionForward printer(machine);     
      ex.PrintException(printer);
    }
  }
}
///

/// CartROM::BuildCart
// Check for the cart type we previously selected, and build, as a prototype, the
// mentioned cart.
class Cartridge *CartROM::BuildCart(Cartridge::CartTypeId carttype,LONG size)
{
  return Cartridge::BuildCart(machine,carttype,size);
}
///

/// CartROM::GuessCartType
// Guess the cart type from the path name by looking at its
// length. If "withheader" is set, then this cart type has been guessed
// from an atari800 type header and is for sure correct.
// Size is the size of the image in bytes. This might be required
// for cart types that comes in variable sizes.
Cartridge::CartTypeId CartROM::GuessCartType(const char *path,bool &withheader,LONG &length)
{
  FILE *cart;
  Cartridge::CartTypeId carttype = Cartridge::Cart_None; // nothing we could possibly know
  
  // Dispose the old new cartridge if we should have one.
  delete newcart;
  newcart = NULL;

  withheader = false;
  if (path == NULL || *path == 0) {
    carttype = Cartridge::Cart_None;
    length   = 0;
  } else if ((cart = fopen(path,"rb"))) {
    carttype = Cartridge::GuessCartType(machine,cart,withheader,length);
    fclose(cart);
  } else if (errno == ENOENT) {
    carttype = Cartridge::Cart_None;
    length   = 0;
  } else {
    ThrowIo("CartROM::GuessCartType","failed to open the cart image");
  }
  // 

  return carttype;
}
///

/// CartROM::AddSelectionVector
// Fill or enhance the selection vector by the given
// name-value pair.
void CartROM::AddSelectionVector(LONG &cnt,bool withheader,LONG size,LONG type,const UWORD *sizevector,
				 struct ArgParser::SelectionVector *&vec,
				 LONG newtype,const char *name)
{ 
  UWORD ksize;
  //
  // Zero is also an allowed size, indication that this cart
  // can be created from an empty file.
  if (size) {
    // If the size is not a multiple of 1K, do
    // not match at all.
    if (withheader)
      size -= sizeof(struct Cartridge::CartHeader);
    //
    if (size & 0x3ff)
      return;
  }
  //
  // Otherwise, compute the size in Kbyte
  ksize = UWORD(size >> 10);
  while (*sizevector) {
    if (*sizevector == ksize || (*sizevector == UWORD(~0) && ksize == 0)) {
      // If we have a header, the type must match exactly.
      if (withheader==false || type == newtype) {
	cnt++;
	// Fill in the vector should we have one.
	if (vec) {
	  vec->Name  = name;
	  vec->Value = newtype;
	  vec++;
	}
      }
    }
    sizevector++;
  }
}
///

/// CartROM::BuildSelectionVector
// Build the selection vector for all
// possible carts we can select from, or just count them.
LONG CartROM::BuildSelectionVector(bool withheader,LONG size,LONG type)
{
  LONG cnt = 2; // Counts the possible cart types, we need at least NONE, and the terminating NULL
  struct ArgParser::SelectionVector *vec = possiblecarts;
  //
  // Add selection vectors for all cart types we know.
  // Count the possible carts.
  switch(machine->MachType()) {
  case Mach_5200:
    // All cart types for the 5200 games system  
    AddSelectionVector(cnt,withheader,size,type,Cart32K::CartSizes    ,vec,Cartridge::Cart_32K_5200,    "32K");
    AddSelectionVector(cnt,withheader,size,type,Cart32KEE16::CartSizes,vec,Cartridge::Cart_32KEE16,     "32KEE16");
    AddSelectionVector(cnt,withheader,size,type,CartBBOB::CartSizes   ,vec,Cartridge::Cart_BBOB,        "BountyBob");
    AddSelectionVector(cnt,withheader,size,type,Cart32K::CartSizes    ,vec,Cartridge::Cart_DB32,        "Debug32");
    break;
  case Mach_Atari800:
    // All cart types for the A800.
    // The right cart slot exists only on the A800.
    AddSelectionVector(cnt,withheader,size,type,CartRight8K::CartSizes,vec,Cartridge::Cart_8K,          "Right8K");
    // Falls through.
  default:
    AddSelectionVector(cnt,withheader,size,type,Cart8K::CartSizes     ,vec,Cartridge::Cart_8K,          "8K");
    AddSelectionVector(cnt,withheader,size,type,Cart16K::CartSizes    ,vec,Cartridge::Cart_16K,         "16K");
    AddSelectionVector(cnt,withheader,size,type,CartOSS::CartSizes    ,vec,Cartridge::Cart_8KSuperCart, "Oss");
    AddSelectionVector(cnt,withheader,size,type,CartOSSB::CartSizes   ,vec,Cartridge::Cart_8KSuperCartB,"OssB");
    AddSelectionVector(cnt,withheader,size,type,CartOSS8K::CartSizes  ,vec,Cartridge::Cart_8KSuperCart8K,"Oss8K");
    AddSelectionVector(cnt,withheader,size,type,CartSDX::CartSizes    ,vec,Cartridge::Cart_32KSDX,      "SDX");
    AddSelectionVector(cnt,withheader,size,type,CartSDX::CartSizes    ,vec,Cartridge::Cart_32KDiamond,  "Diamond");
    AddSelectionVector(cnt,withheader,size,type,CartSDX::CartSizes    ,vec,Cartridge::Cart_32KExp,      "EXP");
    AddSelectionVector(cnt,withheader,size,type,CartXEGS::CartSizes   ,vec,Cartridge::Cart_XEGS,        "XEGS");
    AddSelectionVector(cnt,withheader,size,type,CartXEGS::CartSizes   ,vec,Cartridge::Cart_ExtXEGS,     "ExtXEGS");
    AddSelectionVector(cnt,withheader,size,type,CartWill::CartSizes   ,vec,Cartridge::Cart_Will,        "Will");
    AddSelectionVector(cnt,withheader,size,type,CartFlash::CartSizes  ,vec,Cartridge::Cart_Flash,       "Flash");
    AddSelectionVector(cnt,withheader,size,type,CartMEGA::CartSizes   ,vec,Cartridge::Cart_Mega,        "MegaROM");
    AddSelectionVector(cnt,withheader,size,type,CartAtrax::CartSizes  ,vec,Cartridge::Cart_Atrax,       "Atrax");
    AddSelectionVector(cnt,withheader,size,type,CartPhoenix::CartSizes,vec,Cartridge::Cart_Phoenix,     "Phoenix");
    AddSelectionVector(cnt,withheader,size,type,CartATMax::CartSizes  ,vec,Cartridge::Cart_ATMax,       "ATMax");
    break;
  }
  // Add the "None" vector.
  if (vec) {
    vec->Name  = "None";
    vec->Value = Cartridge::Cart_None;
    vec++;
    // Add the terminating zero.
    vec->Name  = NULL;
    vec->Value = 0;
  }
  return cnt;
}
///

/// CartROM::ParseArgs
// The argument parser: Pull off arguments specific to this class.
void CartROM::ParseArgs(class ArgParser *args)
{  
  LONG type          = carttoload;
  bool withheader    = false;
  bool rtime         = insertrtime8;
  Machine_Type mtype = machine->MachType();

  args->DefineTitle("Cartridge");
  args->OpenSubItem("Cart");
  args->DefineFile("CartPath","path to load cartridge from",cartinsert,true,true,false);
  //
  // If a new rom has been inserted, a reboot and re-parse is required, ditto
  // if the machine type changed.
  if (mtype != machtype || (cartinsert && *cartinsert && (cartpath == NULL || strcmp(cartinsert,cartpath)))) {
    LONG carttype,slots; 
    //
    delete[] cartpath;
    cartpath  = NULL;
    if (cartinsert) {
      cartpath  = new char[strlen(cartinsert) + 1];
      strcpy(cartpath,cartinsert);
    }
    swapcarts = true;
    args->SignalBigChange(ArgParser::ColdStart);    
    // Try to figure out what this could be, and build a more specific 
    // selection vector.
    carttype      = GuessCartType(cartinsert,withheader,cartsize);
    delete[] possiblecarts;
    possiblecarts = NULL;
    // Count how many slots we need.
    slots         = BuildSelectionVector(withheader,cartsize,carttype);
    // Ok, now allocate this.
    possiblecarts = new struct ArgParser::SelectionVector[slots];
    BuildSelectionVector(withheader,cartsize,carttype);
    skipheader    = withheader;
    machtype      = mtype;
  }
  //
  // Now let the cart figure out what the user really wants as the above
  // guess might have been wrong. This also implies that the class we
  // just build might be of the wrong kind.
  if (possiblecarts) {
    bool foundvector                        = false;
    struct ArgParser::SelectionVector *vecs = possiblecarts;
    // Check whether the current type is within the available selections. If not,
    // select "none" as cart type, which is always available.
    while(vecs->Name) {
      if (vecs->Value == type) {
	foundvector = true;
	break;
      }
      vecs++;
    }
    if (!foundvector)
      type = Cartridge::Cart_None;
    //
    args->DefineSelection("CartType","select the cartridge type",possiblecarts,type);
  }
  //
  // Default handling if no carttype possible.
  if (cartpath == NULL || *cartpath == '\0') {
    type = Cartridge::Cart_None;
  }
  if (carttoload != type) {
    swapcarts = true; 
    args->SignalBigChange(ArgParser::ColdStart);
  }
  carttoload = (Cartridge::CartTypeId)type;
  //
  // Finally, check whether the rtime8-cart should be inserted.
  args->DefineBool("RTime8","emulate inserted rtime8 real time clock cartridge",rtime);
  if (rtime != insertrtime8) {
    insertrtime8 = rtime; 
    swapcarts    = true;
    args->SignalBigChange(ArgParser::ColdStart);
  }
  args->CloseSubItem();
}
///

/// CartROM::DisplayStatus
void CartROM::DisplayStatus(class Monitor *mon)
{  
  class Cartridge *cart = CartList.First();
  //
  // The following must be fixed as soon as we support more than one cart at once.
  mon->PrintStatus("Cartridge Status    :\n"
		   "\tCart type to load : %s\n"
		   "\tCartridge path    : %s\n"
		   "\tInserted carts    : ",
		   (newcart)?(newcart->CartType()):("(none)"),
		   (cartpath && *cartpath)?(cartpath):("(none)")
		   );
  if (cart) {
    do {
      mon->PrintStatus("%s",cart->CartType());
      cart = cart->NextOf();
      if (cart) mon->PrintStatus(",");
      else      mon->PrintStatus("\n");
    } while(cart);
  } else {
    mon->PrintStatus("none");
  }
  mon->PrintStatus("\n");
}
///
