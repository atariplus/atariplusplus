/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: binaryimage.hpp,v 1.11 2015/10/30 17:28:33 thor Exp $
 **
 ** In this module: Disk image class for binary load files
 **********************************************************************************/

#ifndef BINARYIMAGE_HPP
#define BINARYIMAGE_HPP

/// Includes
#include "types.hpp"
#include "imagestream.hpp"
#include "diskimage.hpp"
#include "patchprovider.hpp"
#include "patch.hpp"
///

/// Forwards
class Machine;
class ChoiceRequester;
class AdrSpace;
class CPU;
///

/// Class BinaryImage
// This class defines an disk image for binary-load files. We
// prepend to this image a binary-load mini-dos to boot off
// the image without fuzz.
class BinaryImage : public DiskImage, private PatchProvider, private Patch {
  //
  // Contents of the emulated disk goes here.
  UBYTE                 *Contents;
  class  ImageStream    *Image;
  ULONG                  ByteSize;
  //
  // The escape code used by the binary loader
  UBYTE                  loaderEscape;
  //
  // The state machine.
  UBYTE                  BootStage;
  //
  // Defined boot stages
  enum {
    Init           = 0, // startup the bootstrap process
    Fill           = 1, // load a new sector into the buffer
    SIOReturn      = 2, // SIO returned, check status
    Loader         = 3, // the binary loader itself
    JumpInit       = 4, // Jump through the init vector if it is there.
    JumpRun        = 5, // Jump through the run vector if it is there and the file is at EOF
    JumpInitReturn = 6, // the init vector returned, re-initialize
    WaitVBI        = 7, // wait for the VBI to happen, phase 1
    WaitVBI2       = 8  // wait for the VBI to happen, phase 2
  };
  //
  // The state machine for the binary loader.
  UBYTE                 LoaderStage;
  //
  // Defined loader stages
  enum {
    CheckHeader   = 0, // check whether the first byte is 0xff
    CheckHeader2  = 1, // check whether the second byte is a 0xff
    StartAdrLo    = 2, // load the low-byte of the start address
    StartAdrHi    = 3, // load the hi-byte of the start address
    EndAdrLo      = 4, // load the low-byte of the end address
    EndAdrHi      = 5, // load the hi-byte of the end-address
    ReadByte      = 6  // load a byte into the RAM
  };
  //
  // The next sector to load
  UWORD                 NextSector;
  //
  // Number of bytes in the sector
  UBYTE                 AvailBytes;
  //
  // Next byte index in the loader buffer.
  UBYTE                 NextByte;
  //
  // Start address
  ADR                   StartAddress;
  //
  // And end address for the binary load.
  ADR                   EndAddress;
  //
  // A requester we pop up for error conditions.
  class ChoiceRequester *FixupRequester;
  //
  // The following structure keeps a byte/sector offset
  // and reads from an "image" file.
  struct FilePointer {
    UBYTE *Sector;       // pointer to current sector
    UBYTE  ByteOffset;   // byte offset in the current sector.
    //
    UBYTE Get(void); // Read from the image sectored.
    UWORD GetWord(void); // Read a word from the above, LO, HI.
    void Put(UBYTE data);
    void PutWord(UWORD data);
    //
    // Check whether we are at the EOF.
    bool Eof(void) const;
    //
    // Truncate the file at the current position.
    void Truncate(void);
    //
    // Setup the file pointer from an image
    FilePointer(UBYTE *src);
    // Empty constructor
    FilePointer(void)
    { }
  };
  //
  // Check whether the loaded image is setup fine.
  void VerifyImage(UBYTE *src);
  //
  // Install a new escape code.
  virtual void InstallPatch(class AdrSpace *,UBYTE code)
  {
    loaderEscape = code;
  }
  //
  // This entry is called by the CPU emulator to run the patch at hand
  // whenever an ESC (HLT, JAM) code is detected.
  virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  //
  // Create the binary boot sector with the loader containing the patch.
  void CreateBootSector(UBYTE *image);
  //
  // Create the VTOC at offset 0x168
  void CreateVTOC(UBYTE *image,UWORD totalcount);
  //
  // Create the directory at offset 0x169
  void CreateDirectory(UBYTE *image,UWORD sectorcount);
  //
  // Push the indicated return address onto the
  // stack of the emulated CPU to make it call the
  // given routine.
  void PushReturn(class AdrSpace *adr,class CPU *cpu,ADR where);
  //
  // Initialize the boot process by setting a couple of Os variables.
  void InitStage(class AdrSpace *adr,class CPU *cpu);
  //
  // Read the next sector into the buffer
  void FillStage(class AdrSpace *adr,class CPU *cpu);
  //
  // The call from SIO returned. Check the OS status and fill in the
  // number of available bytes in this sector, and the next sector.
  void SIOReturnStage(class AdrSpace *adr,class CPU *cpu);
  //
  // Place a single byte into the RAM or advance the loader.
  // The byte is already placed in the argument.
  void RunLoaderStage(class AdrSpace *adr,class CPU *cpu,UBYTE byte);
  //
  // Jump through the run vector.
  void JumpRunStage(class AdrSpace *adr,class CPU *cpu);
  //
  // Jump through the init vector.
  void JumpInitStage(class AdrSpace *adr,class CPU *cpu);
  //
  // Return from the init vector, reset it again and prepare for the next segment.
  void JumpInitReturnStage(class AdrSpace *adr,class CPU *cpu);
  //
  // Wait until a VBI happens, phase 1
  void WaitVBIStage(class AdrSpace *adr,class CPU *cpu);
  //
  // Wait until a VBI happens, phase 2
  void WaitVBI2Stage(class AdrSpace *adr,class CPU *cpu);
  //
public:
  BinaryImage(class Machine *mach);
  virtual ~BinaryImage(void);
  //
  // Open a disk image from a file given an image stream class.
  virtual void OpenImage(class ImageStream *image); 
  //
  // Reset this patch. 
  virtual void Reset(void);
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector);
  // Return the number of sectors.
  virtual ULONG SectorCount(void);
  //
  // Return the status of this image. 
  virtual UBYTE Status(void);
  //
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer,UWORD &);
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD sector,const UBYTE *buffer,UWORD &);
  // 
  // Protect an image on user request
  virtual void ProtectImage(void);  
};
///

///
#endif
