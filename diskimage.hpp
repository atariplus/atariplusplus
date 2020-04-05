/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: diskimage.hpp,v 1.6 2020/04/04 18:01:41 thor Exp $
 **
 ** In this module: Abstract base class for disk images
 **********************************************************************************/

#ifndef DISKIMAGE_HPP
#define DISKIMAGE_HPP

/// Includes
#include "types.hpp"
#include "imagestream.hpp"
///

/// Forwards
class Machine;
///

/// Class DiskImage
// This class defines the abstract base class for all types of
// disk images. It is used by the diskdrive class to represent
// inserted disks.
class DiskImage {
protected:
  class Machine *Machine;
  //
public:
  //
  // Status flags of the drive. This is the hardware status. Bit are
  // here set to indicate the corresponding condition. Note that the
  // real hardware returns the inverted status
  enum {
    Busy      = 1, // still working
    DRQ       = 2, // index hole interrupt or data request. Just set this bit.
    LostData  = 4, // lost data error
    CRCError  = 8, // sector checksum invalid
    NotFound  = 16, // record not found: Sector is not there.
    Deleted   = 32, // Sector was marked deleted.
    Protected = 64, // disk is write protected.
    NotReady  = 128  // if no disk is present.
  };
  //
  DiskImage(class Machine *mach);
  virtual ~DiskImage(void);
  //
  // Open a disk image from a file given an image stream class.
  virtual void OpenImage(class ImageStream *image) = 0;
  //
  // Restore the image to its initial state if necessary.
  virtual void Reset(void) = 0;
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector) = 0;
  //
  // Return the number of sectors.
  virtual ULONG SectorCount(void) = 0;
  //
  // Return the drive status, this is a bitmask from the enum above.
  virtual UBYTE Status(void) = 0;
  //
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer,UWORD &delay) = 0;
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD sector,const UBYTE *buffer,UWORD &delay) = 0;
  //
  // Protect an image on user request
  virtual void ProtectImage(void) = 0;
};
///

///
#endif
