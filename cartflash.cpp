/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartflash.cpp,v 1.15 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of the Flash ROM cart
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "stdio.hpp"
#include "flashpage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "monitor.hpp"
#include "snapshot.hpp"
#include "cartflash.hpp"
#include "choicerequester.hpp"
#include "filerequester.hpp"
#include "new.hpp"
///

/// CartFlash::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartFlash::CartSizes[] = {128,512,1024,0xffff,0};
///

/// CartFlash::CartFlash
// Construct the cart. There is nothing to do here.
CartFlash::CartFlash(class Machine *mach,UBYTE banks)
  : Configurable(mach), TotalBanks(banks), Active(true), EnableOnReset(true),
    Rom1(NULL), Rom2(NULL), 
    ActiveBank(0), Machine(mach), RequestSave(NULL), SavePath(NULL)
  // A page is 256 bytes large, making 32*256 = 8192 bytes
{
}
///  

/// CartFlash::Initialize
// Initialize this memory controller, built its contents.
void CartFlash::Initialize(void)
{ 
  switch(TotalBanks) {
  case 16:
    if (Rom1 && Rom1->GetType() != AmdChip::Am010) {
      delete Rom1;Rom1 = NULL;
    }
    if (Rom1 == NULL)
      Rom1 = new AmdChip(Machine,AmdChip::Am010,"AmdFlash.1",0,this);
    delete Rom2;Rom2 = NULL;
    ActiveBank = 0;
    Active     = true;
    break;
  case 64:
    if (Rom1 && Rom1->GetType() != AmdChip::Am040) {
      delete Rom1;Rom1 = NULL;
    }
    if (Rom1 == NULL)
      Rom1 = new AmdChip(Machine,AmdChip::Am040,"AmdFlash.1",0,this);
    delete Rom2;Rom2 = NULL;
    ActiveBank = TotalBanks - 1;
    Active     = true;
    break;
  case 128:
    if (Rom1 && Rom1->GetType() != AmdChip::Am040) {
      delete Rom1;Rom1 = NULL;
    }
    if (Rom1 == NULL)
      Rom1 = new AmdChip(Machine,AmdChip::Am040,"AmdFlash.1",0,this);
    if (Rom2 && Rom2->GetType() != AmdChip::Am040) {
      delete Rom2;Rom2 = NULL;
    }
    if (Rom2 == NULL)
      Rom2 = new AmdChip(Machine,AmdChip::Am040,"AmdFlash.2",1,this);
    ActiveBank = TotalBanks - 1;
    Active     = true;
    break;
  default:
    // Leave chip sockets unoccupied.
    delete Rom1;Rom1 = NULL;
    delete Rom2;Rom2 = NULL;
    Active     = false;
    break;
  }
  //
  // Initialize the available chips as well.
  if (Rom1)
    Rom1->Initialize();
  if (Rom2)
    Rom2->Initialize();
  //
  // Should we disable it now to be able to flash it?
  if (!EnableOnReset)
    Active = false;
}
///

/// CartFlash::~CartFlash
// Dispose the cart. There is nothing to dispose
CartFlash::~CartFlash(void)
{
  delete Rom1;
  delete Rom2;
  delete RequestSave;
  delete SavePath;
}
///

/// CartFlash::CartType
// Return a string identifying the type of the cartridge.
const char *CartFlash::CartType(void)
{
  return "FlashROM";
}
///

/// CartFlash::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartFlash::ReadFromFile(FILE *fp)
{ 
  // This is the first call made for a cart: Retrieve its contents
  // from an external file. Use it to adjust/build the chips within
  // this cart.
  delete Rom1; Rom1 = NULL;
  delete Rom2; Rom2 = NULL;
  Initialize();
  //
  // Forward the request to the chips itself, whenever loaded/mapped.
  if (Rom1)
    Rom1->ReadFromFile(fp);
  if (Rom2)
    Rom2->ReadFromFile(fp);
}
///

/// CartFlash::WriteToFile
// Write the contents of this cart to an open file. 
// Throws on failure.
void CartFlash::WriteToFile(FILE *fp) const
{ 
  // Foward the requests to the chips within the
  // cart.
  if (Rom1)
    Rom1->WriteToFile(fp);
  if (Rom2)
    Rom2->WriteToFile(fp);
}
///

/// CartFlash::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartFlash::MapCart(class MMU *mmu)
{
  if (Active) {
    class AmdChip *active;
    // Find the active chip by the bank index.
    active = (ActiveBank & 0x40) ? Rom2 : Rom1;
    if (active) {
      active->MapChip(mmu,ActiveBank & 0x3f);
      return true;
    }
  }
  return false;
}
///

/// CartFlash::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartFlash::ComplexWrite(class MMU *mmu,ADR mem,UBYTE)
{  
  int  newbank     = 0;
  //
  // Check whether the write goes to 0xd510. If so, deactivate the
  // cart.
  if (mem == ((TotalBanks == 16) ? 0xd510 : 0xd580)) {
    if (Active) {
      // Deactivate the cart here.
      Active     = false;
      mmu->BuildCartArea();
    }
    // Carts reacts on this address
    return true;
  } else if (mem <= (0xd500+TotalBanks-1)) {
    // Otherwise, select the proper bank.
    newbank      = (mem - 0xd500) % TotalBanks; 
    if (newbank != ActiveBank || Active == false) {
      // Ok, mapping changed, tell the mmu.
      Active     = true;
      ActiveBank = newbank;
      mmu->BuildCartArea();
    }
    // Ditto here: Reacts.
    return true;
  }
  //
  // Ignore the cartctrl write.
  return false;
}
///

/// CartFlash::SaveCart
// Check whether the contents of this cart has been modified. This obviouly
// applies only to user-programmable ROMs. Default is of course unmodified.
// If so, save the cart contents back to disk.
void CartFlash::SaveCart(void)
{
  bool modified = false;

  if (Rom1 && Rom1->IsModified())
    modified = true;
  if (Rom2 && Rom2->IsModified())
    modified = true;

  if (modified) {
    char request[256];
    //
    // Check whether the user wants to save the changes back to disk.
    if (RequestSave == NULL)
      RequestSave = new class ChoiceRequester(Machine);
    //
    if (CartPath) {
      snprintf(request,255,"The flash cartridge\n%s\nwas modified. OK to save the changes back to disk?",CartPath);
    } else {
      snprintf(request,255,"The flash cartridge was modified.\nOK to save the changes back to disk?");
    }
    //
    // Now check what the user wants.
    if (RequestSave->Request(request,"Cancel","Save Changes",NULL) == 1) {
      //
      // Construct the file requester.
      if (SavePath == NULL)
	SavePath = new class FileRequester(Machine);
      //
      if (SavePath->Request("Cartridge Path",(CartPath)?(CartPath):(""),true,true,false)) {
	//
	// Done. Ok, save me.
	Cartridge::SaveCart(SavePath->SelectedItem(),false);
      }
    }
  }
}
///

/// CartFlash::DisplayStatus
// Display the status over the monitor.
void CartFlash::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Cart active        : %s\n"
		   "Number of banks    : %d\n"
		   "Active bank        : %d\n",
		   CartType(),
		   (Active)?("on"):("off"),
		   TotalBanks,
		   ActiveBank);
}
///

/// CartFlash::State
// Perform the snapshot operation for the CartCtrl unit.
void CartFlash::State(class SnapShot *sn)
{
  sn->DefineBool("FlashMapped","Flash cartridge mapped in",Active);
  sn->DefineLong("FlashBank","Flash cartridge active bank selection",0,TotalBanks-1,ActiveBank);
}
///

/// CartFlash::ParseArgs
// Define the arguments for the Flash cartridge. This is uniquely a
// configurable cartridge.
void CartFlash::ParseArgs(class ArgParser *args)
{
  args->DefineTitle("CartFlash");
  args->DefineBool("EnableCartFlash","enable the flash cartrdige mapping",EnableOnReset);
}
///
