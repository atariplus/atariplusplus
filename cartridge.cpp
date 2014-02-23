/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartridge.cpp,v 1.21 2013/09/02 15:35:46 thor Exp $
 **
 ** In this module: The superclass individual cart types are derived from
 **********************************************************************************/

/// Includes
#include "cartridge.hpp"
#include "monitor.hpp"
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
#include "choicerequester.hpp"
#include "mmu.hpp"
#include "stdio.hpp"
#include "unistd.hpp"
#include "directory.hpp"
///

/// Cartridge::Cartridge
Cartridge::Cartridge(void)
  : CartPath(NULL)
{ }
///

/// Cartridge::~Cartridge
Cartridge::~Cartridge(void)
{
  delete[] CartPath;
}
///

/// Cartridge::GuessCartType
// Guess the cart type from the path name by looking at its
// length. If "withheader" is set, then this cart type has been guessed
// from an atari800 type header and is for sure correct.
// Size is the size of the image in bytes. This might be required
// for cart types that comes in variable sizes.
Cartridge::CartTypeId Cartridge::GuessCartType(class Machine *mach,FILE *cart,bool &withheader,LONG &length)
{  
  CartTypeId carttype = Cart_None; // nothing we could possibly know
  struct CartHeader hdr;
  
  // Check whether we can read a cart header.
  if (fread(&hdr,sizeof(hdr),1,cart) == 1) {
    // Yes, that worked. Check whether the magicID is right.
    if (hdr.CartID[0] == 'C' &&
	hdr.CartID[1] == 'A' &&
	hdr.CartID[2] == 'R' &&
	hdr.CartID[3] == 'T') {
      ULONG type;
      // Yes, this could actually work. Now check for the type.
      type = (hdr.Type[0] << 24) | (hdr.Type[1] << 16) | (hdr.Type[2] << 8) | (hdr.Type[3]);
      switch(type) {
      case 0x01:
	withheader = true;
	carttype   = Cart_8K;
	break;
      case 0x02:
	withheader = true;
	carttype   = Cart_16K;
	break;
      case 0x03:
	withheader = true;
	carttype   = Cart_8KSuperCart;
	break;
      case 0x04:
      case 0x10:
      case 0x13:
	withheader = true;
	carttype   = Cart_32K_5200;
	break;
      case 0x05:
	withheader = true;
	carttype   = Cart_DB32;
	break;
      case 0x06:
	withheader = true;
	carttype   = Cart_32KEE16;
	break;
      case 0x07:
	withheader = true;
	carttype   = Cart_BBOB;
	break;
      case 0x08:
      case 0x16:
	withheader = true;
	carttype   = Cart_Will;
	break;
      case 0x09:
	withheader = true;
	carttype   = Cart_32KExp;
	break;
      case 0x0a:
	withheader = true;
	carttype   = Cart_32KDiamond;
	break;
      case 0x0b:
	withheader = true;
	carttype   = Cart_32KSDX;
	break;
      case 0x0c:
      case 0x0d:
      case 0x0e:
      case 0x17:
      case 0x18:
      case 0x19:
	withheader = true;
	carttype   = Cart_XEGS;
	break;
      case 0x15:
	withheader = true;
	carttype   = Cart_Right8K;
	break;
      case 0x21:
      case 0x22:
      case 0x23:
      case 0x24:
      case 0x25:
	withheader = true;
	carttype   = Cart_ExtXEGS;
	break;
      case 0x0f:
	withheader = true;
	carttype   = Cart_8KSuperCartB;
	break;
      case 0x11:
	withheader = true;
	carttype   = Cart_Atrax;
	break;
      case 0x12:
	withheader = true;
	carttype   = Cart_BBOB;
	break;
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0x1e:
      case 0x1f:
      case 0x20:
	withheader = true;
	carttype   = Cart_Mega;
	break;
      case 0x26:
	withheader = true;
	carttype   = Cart_Flash;
	break;
      case 0x27:
      case 0x28:
	withheader = true;
	carttype   = Cart_Phoenix;
	break;
      case 0x29:
      case 0x2a:
	withheader = true;
	carttype   = Cart_ATMax;
	break;
      }
    } else {
      withheader   = false;
    }
  }
  // Seek to the end and get by this the length of the file.
  if (fseek(cart,0,SEEK_END) == 0) {
    // Get the current file pointer position.
    length = ftell(cart);
    if (withheader == false) {
      carttype = Cart_None;
      switch(mach->MachType()) {
      case Mach_5200:
	if (length >= 0 && (length & 0x0fff) == 0) {
	  if (length == 8*1024) {
	    carttype = Cart_32K_5200;
	  } else if (length == 16*1024) {
	    carttype = Cart_32K_5200; 
	  } else if (length == 32*1024) {
	    carttype = Cart_32K_5200;
	  }
	}
	break;
      default:
	if (length >= 0 && (length & 0x0fff) == 0) {
	  if (length == 8*1024) {
	    carttype = Cart_8K;
	  } else if (length == 16*1024) {
	    carttype = Cart_16K; // Could also be OSS
	  } else if (length == 64*1024) {
	    carttype = Cart_32KSDX;
	  } else if (length == 40*1024) {
	    carttype = Cart_BBOB;
	  } else if (length >= 32*1024) {
	    carttype = Cart_ExtXEGS; // Could also be FLASH or MEGA
	  }
	}
	break;
      }
    }
  }

  return carttype;
}
///

/// Cartridge::BuildCart
// Check for the cart type we previously selected, and build, as a prototype, the
// mentioned cart. Since some carts require additional activity for
// mapping, we also need to pass the machine here.
class Cartridge *Cartridge::BuildCart(class Machine *mach,CartTypeId carttype,LONG size)
{
  switch(carttype) {
  case Cart_None:
    return NULL;
  case Cart_8K:
    return new class Cart8K;
  case Cart_Right8K:
    return new class CartRight8K;
  case Cart_16K:
    return new class Cart16K;
  case Cart_32K_5200:
    if (size < 256 << 12 && size > 0)
      return new class Cart32K(UBYTE(size >> 12)); // number of 4K blocks
    return NULL;
  case Cart_32KEE16:
    return new class Cart32KEE16;
  case Cart_DB32:
    return new class CartDB32;
  case Cart_8KSuperCart:
    return new class CartOSS;
  case Cart_8KSuperCartB:
    return new class CartOSSB;
  case Cart_32KSDX:
    return new class CartSDX(0xe0);
  case Cart_32KDiamond:
    return new class CartSDX(0xd0);
  case Cart_32KExp:
    return new class CartSDX(0x70);
  case Cart_XEGS:
    // This type requires an additional cart size to work correctly.
    // The size is here in number of banks, each of them 8K in size.
    if (size < 256 << 13 && size > 0)
      return new class CartXEGS(UBYTE(size >> 13),false);  
    return NULL;
  case Cart_ExtXEGS:
    // Same as above, but allows getting disabled.
    if (size < 256 << 13 && size > 0)
      return new class CartXEGS(UBYTE(size >> 13),true);
    return NULL;
  case Cart_Will:
    if (size < 256 << 13 && size > 0)
      return new class CartWill(UBYTE(size >> 13));
    return NULL;
  case Cart_BBOB:
    // This cart type requires access to the MMU.
    return new class CartBBOB(mach->MMU());
  case Cart_RTime8:
    return new class CartRT8;
  case Cart_Flash:
    if (size == 0) {
      class ChoiceRequester req(mach);
      // The flash cart can be build from scratch. If so,
      // request possible choices from the user.
      switch(req.Request("Please select the size of the flash cartridge to build:\n",
			 "128K","512K","1MB",NULL)) {
      case 0:
	size = 128 << 10;
	break;
      case 1:
	size = 512 << 10;
	break;
      case 2:
	size = 1024 << 10;
	break;
      }
    }
    if (size < 256 << 13 && size > 0)
      return new class CartFlash(mach,UBYTE(size >> 13));
    return NULL;
  case Cart_Mega:
    if (size < 256 << 14 && size > 0)
      return new class CartMEGA(UBYTE(size >> 14));
    return NULL;
  case Cart_Atrax:
    return new class CartAtrax;
  case Cart_Phoenix:
    if (size < 16384 && size > 0)
      return new class CartPhoenix(UBYTE(size >> 13));
    return NULL;
  case Cart_ATMax:
    if (size < 1L << 20 && size > 0)
      return new class CartATMax(UBYTE(size >> 13));
    return NULL;
  case Cart_8KSuperCart8K:
    return new class CartOSS8K;
  }
  //
  // Shut up the compiler.
  return NULL;
}
///

/// Cartridge::WriteToFile
// Write the contents of this cart to an open file. Headers have been
// written already here. Throws on failure. Carts need not to implement
// this.
void Cartridge::WriteToFile(FILE *) const
{
  Throw(NotImplemented,"Cartridge::WriteToFile","this cartridge cannot be saved");
}
///

/// Cartridge::LoadFromFile
// Load one or several pages from a file into the Cartridge ROM
// If skipheader is true, the atari800 CART type header is skipped
void Cartridge::LoadFromFile(const char *path,bool skipheader)
{
  FILE *fp;
  struct CartHeader hdr;
  
  delete[] CartPath;
  CartPath = NULL;
  //
  // Make a copy of the cart path to be able to save the
  // cartridge back in case it was modified.
  CartPath = new char[strlen(path) + 1];
  strcpy(CartPath,path);
  //
  fp = fopen(path,"rb");
  if (fp) {
    try {
      // Check whether we should skip any header in the file.
      if (skipheader) {
	if (fread(&hdr,sizeof(hdr),1,fp) != 1) {
	  // outch, this didn't work out!
	  ThrowIo("Cartridge::LoadFromFile","unable to read or parse the cart header");
	}
      }
      ReadFromFile(fp);
      fclose(fp);
    } catch(...) {
      // clean up at least.
      fclose(fp);
      throw;
    }
  } else if (errno != ENOENT) {
    ThrowIo("Cartridge::LoadFromFile","unable to open the cartridge dump");
  }
}
///

/// Cartridge::SaveCart
// Save this cartridge to a given path name.
// Currently only relevent to the CartFlash class. If the
// path is NULL, the save path is identical to the path the
// cart was loaded from.
void Cartridge::SaveCart(const char *path, bool withheader)
{
  FILE *fp;
  struct CartHeader hdr; 

  if (path == NULL)
    path = CartPath;

  if (path != CartPath) {
    delete[] CartPath;CartPath = NULL;
    CartPath = new char[strlen(path)+1];
    strcpy(CartPath,path);
  }
  //
  // Try only to save in case the path is writable
  fp = fopen(path,"wb");
  if (fp) {
    try {
      if (withheader) {	
	memset(&hdr,0,sizeof(hdr));
	// Here we need to build the header structure. Luckily, only one cart
	// can currently enter here. Later modifications might require to extend
	// this method. 
	hdr.CartID[0] = 'C';
	hdr.CartID[1] = 'A';
	hdr.CartID[2] = 'R';
	hdr.CartID[3] = 'T';
	hdr.Type[0]   = 0;
	hdr.Type[1]   = 0;
	hdr.Type[2]   = 0;
	hdr.Type[3]   = 0x26; // Flash cart
	if (fwrite(&hdr,sizeof(hdr),1,fp) != 1) {
	  ThrowIo("Cartridge::SaveToFile","unable to write the cart header");
	}
      }
      WriteToFile(fp);
      fclose(fp);
    } catch(...) {
      fclose(fp);
      // Remove the incomplete cart.
      unlink(path);
      throw;
    }
  } else {
    ThrowIo("Cartridge::SaveToFile","unable to save the cart image");
  }
}
///

/// Cartridge::DisplayStatus
// Display the status over the monitor.
void Cartridge::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Cart type inserted : %s\n",CartType());
}
///
