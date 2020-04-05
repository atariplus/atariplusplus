/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atximage.hpp,v 1.8 2020/04/04 18:01:41 thor Exp $
 **
 ** In this module: Disk image class for .atx images.
 **********************************************************************************/

#ifndef ATXIMAGE_HPP
#define ATXIMAGE_HPP

/// Includes
#include "types.hpp"
#include "diskimage.hpp"
#include "imagestream.hpp"
#include "hbiaction.hpp"
///

/// Forwards
class Machine;
///

/// Class ATXImage
// This class implements ATX images, disk images with a header node
// defining the type of the image plus additional information for
// weak sectors.
// It derives from HBIAction to get a 15kHz clock, which is the same
// type of clock the serial input is driven by.
class ATXImage : public DiskImage, private HBIAction {
  //
  // If opened from a file, here it is.
  class ImageStream *Image;
  //
  // Protection Status of the image. True if this is write
  // protected.
  bool               Protected;
  //
  // True if a CRC error has been detected.
  bool               CRCError;
  //
  // True if a lost data error has been detected.
  bool               LostDataError;
  //
  // True if a sector has not been found.
  bool               SectorMissing;
  //
  // True if a sector has been marked as deleted.
  bool               SectorDeleted;
  //
  // Current track position.
  UBYTE              TrackUnderHead;
  //
  // Number of tracks per sector (nominally). This is typically
  // 18 for SD and 26 for ED.
  UBYTE              SectorsPerTrack;
  //
  // The nominal size of a sector.
  UWORD              DefaultSectorSize;
  //
  // The clock counter. Required for the disk timing, i.e.
  // when a read happens - and which sector will be read in
  // case duplicate sectors are present. This is the head
  // position in usecs.
  ULONG              HeadPosition;
  //
  // Read a byte from the image
  // An ATX image is comprized of several tracks, listed here.
  struct Track {
    // Next track on the disk
    struct Track    *Next;
    //
    // The track number, zero-based.
    UBYTE            TrackIdx;
    //
    // Sectors in this track.
    UBYTE            Sectors;
    //
    // Sector data.
    struct Sector {
      // Next sector in the track.
      struct Sector *Next;
      //
      // Track the sector is part of.
      struct Track  *Track;
      //
      // Sector index. This counts from one up. There could be
      // multiple sectors of the same sector index.
      UBYTE          SectorIdx;
      //
      // Sector status
      enum StatusFlags {
	Extended  = 64,
	NoRecord  = 32, // undocumented, but also means that the sector is not there.
	Missing   = 16,
	CRCError  = 8,
	LostData  = 4
      };
      //
      // The status, one of the above.
      UBYTE          SectorStatus;
      //
      // Sector position relative to the start of the track in 8us intervals
      UWORD          SectorPosition;
      //
      // Offset to the first weak data in the track.
      // Only valid if Extended is set in the sector status.
      UWORD          WeakOffset;
      //
      // Sector size in bytes.
      UWORD          SectorSize;
      //
      // Offset of the data to the start of the file.
      ULONG          Offset;
      //
      // Offset of the corresponding header, to be adapted when
      // modifying the sector.
      ULONG          HeaderOffset;
      //
      // Offset of the corresponding sector extension.
      ULONG          ExtensionOffset;
      //
      // Constructor and destructor.
      Sector(void)
	: Next(NULL), SectorStatus(0)
      { }
      //
      ~Sector(void)
      { }
    }               *SectorList;
    //
    // Constructor and destructor
    Track(void)
      : Next(NULL), SectorList(NULL)
    { }
    //
    ~Track(void)
    {
      struct Sector *sec;

      while((sec = SectorList)) {
	SectorList = sec->Next;
	delete sec;
      }
    }
  } *TrackList;
  //
  // Find a sector given a sector number.
  struct Track::Sector *FindSector(UWORD sector,UWORD *delay);
  //
  // Implements the HBIAction and drives the clock
  virtual void HBI(void);
  //
  // Increate the time by the given amount of microseconds.
  void PassTime(ULONG micros);
  //
public:
  ATXImage(class Machine *mach);
  virtual ~ATXImage(void);
  //
  // Open a disk image from a file given an image stream.
  virtual void OpenImage(class ImageStream *image);
  //
  // Restore the image to its initial state if necessary.
  virtual void Reset(void);
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector);
  //
  // Return the number of sectors.
  virtual ULONG SectorCount(void);
  //
  // Return the status of this image. 
  virtual UBYTE Status(void);
  //
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer,UWORD &delay);
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD sector,const UBYTE *buffer,UWORD &delay);
  //
  // Protect an image on user request
  virtual void ProtectImage(void);  
};
///

///
#endif
