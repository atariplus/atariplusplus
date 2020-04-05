/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atximage.cpp,v 1.15 2020/04/05 16:49:36 thor Exp $
 **
 ** In this module: Disk image class for .atx images.
 **********************************************************************************/

/// Includes
#include "atximage.hpp"
#include "exceptions.hpp"
#include "imagestream.hpp"
#include "machine.hpp"
#include "stdlib.hpp"
///

/// Defines
// Number of 15kHz intervals for moving the drive head by one track (estimated, need to measure)
#define LinesPerTrack 150
#define MusecsPerTrack 50000 // 10ms
// Settle delay before reading is attempted
#define LinesPerSettle 300
#define MusecsPerSettle 20000 // 20ms
// Number of lines required to transmit a byte
#define LinesPerByte 8
// One line takes approximately 1/15000 seconds, which makes 67usecs roughly
#define MuSecsPerLine 67
// Number of musecs per rotation
#define MusecsPerRotation 210107
///

/// ATXImage::ATXImage
ATXImage::ATXImage(class Machine *mach)
  : DiskImage(mach), HBIAction(mach),
    Image(NULL),
    Protected(false), CRCError(false), LostDataError(false), SectorMissing(false), SectorDeleted(false),
    TrackUnderHead(0), SectorsPerTrack(18), DefaultSectorSize(128), HeadPosition(0),
    TrackList(NULL)
{
  
}
///

/// ATXImage::~ATXImage
ATXImage::~ATXImage(void)
{
  struct Track *track;

  while((track = TrackList)) {
    TrackList = track->Next;
    delete track;
  }
}
///

/// ATXImage::Reset
// Restore the ATX image to its initial state.
void ATXImage::Reset(void)
{
  CRCError        = false;
  LostDataError   = false;
  SectorMissing   = false;
  SectorDeleted   = false;
  TrackUnderHead  = 0;
  HeadPosition    = 0;
}
///

/// ATXImage::OpenImage
// Open a disk image from a file. This requires to read
// the sector size and other nifties.
void ATXImage::OpenImage(class ImageStream *image)
{
  ULONG bytes;
  bool havesecspertrack = false;
  bool havesectorsize   = false;
  int trackcount        = 0; // number of tracks found.
  //
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (Image || TrackList) {
    Throw(ObjectExists,"ATXImage::OpenImage",
	  "the image has been opened already");
  }
#endif
  //
  bytes           = image->ByteSize();
  Protected       = image->ProtectionStatus();
  CRCError        = false;
  LostDataError   = false;
  SectorMissing   = false;
  SectorDeleted   = false;
  //
  // Default is SD
  SectorsPerTrack   = 18;
  DefaultSectorSize = 128;
  //
  try {
    UBYTE fileheader[4+2+22+4];
    ULONG trackstart;
    //
    if (!image->Read(0,fileheader,sizeof(fileheader)))
      throw false;
    if (fileheader[0] != 'A' && fileheader[1] != 'T' && 
	fileheader[2] != '8' && fileheader[3] != 'X')
      Throw(InvalidParameter,"ATXImage::OpenImage","image is not an ATX image");
    //
    // Start of the track data.
    trackstart = 
      (ULONG(fileheader[4+2+22+0]) << 0 ) |
      (ULONG(fileheader[4+2+22+1]) << 8 ) |
      (ULONG(fileheader[4+2+22+2]) << 16) |
      (ULONG(fileheader[4+2+22+3]) << 24);
    //
    // Read all tracks.
    while(trackstart < bytes) {
      UBYTE trackheader[4+2+2+1+1+2+8+4];
      ULONG trackend;
      ULONG trackdata;
      UBYTE tracknumber;
      UBYTE sectors;
      struct Track *track,**other;
      //
      if (!image->Read(trackstart,trackheader,sizeof(trackheader)))
	throw false;
      //
      // First byte no longer part of this track
      trackend = trackstart + ((ULONG(trackheader[0]) << 0 ) |
			       (ULONG(trackheader[1]) << 8 ) |
			       (ULONG(trackheader[2]) << 16) |
			       (ULONG(trackheader[3]) << 24));
      //
      // Track record type must be zero.
      if (trackheader[4] != 0 || trackheader[5] != 0) {
	if (trackcount < 40) {
	  Throw(InvalidParameter,"ATXImage::OpenImage","ATX track header type invalid, "
		"must be zero");
	} else {
	  Machine->PutWarning("ATX image probably corrupt, bogus data beyond last track.");
	  break;
	}
      }
      //
      tracknumber = trackheader[4+2+2];
      trackcount++;
      //
      // Number of sectors in this track.
      sectors     = 
	(UWORD(trackheader[4+2+2+2+0]) << 0) | 
	(UWORD(trackheader[4+2+2+2+1]) << 8);
      //
      // Assume that track #0 is not corrupt and we can simply use
      // its sector count. This is because the Os has to boot from here.
      // This ^$*@! format does not define whether the content is ED or SD.
      if (!havesecspertrack) {
	// Only if the number of sectors per track is reasonable.
	switch(sectors) {
	case 18: // SD,DD
	case 26: // ED
	  SectorsPerTrack  = sectors;
	  havesecspertrack = true;
#if CHECK_LEVEL > 0
	  printf("ATX: identified %d sectors per track disk image.\n",sectors);
#endif
	  break;
	}
      }
      //
      // Offset to the track data in file.
      trackdata   = trackstart + ((ULONG(trackheader[4+2+2+2+2+8+0]) << 0 ) |
				  (ULONG(trackheader[4+2+2+2+2+8+1]) << 8 ) |
				  (ULONG(trackheader[4+2+2+2+2+8+2]) << 16) |
				  (ULONG(trackheader[4+2+2+2+2+8+3]) << 24));
      //
      // Create a new track
      track             = new struct Track();
      //
      // Enqueue the track in numerical order.
      other             = &TrackList;
      while(*other && (*other)->TrackIdx < tracknumber)
	other = &((*other)->Next);

      track->Next       = *other;
      *other            = track;
      track->TrackIdx   = tracknumber;
      track->Sectors    = sectors;
      //
#if CHECK_LEVEL > 0
      {
	struct Track *other = TrackList;
	while (other) {
	  if (other != track && other->TrackIdx == track->TrackIdx)
		printf("Found duplicate track %d\n",tracknumber);
	  other = other->Next;
	}
      }
#endif
      // Read the track data of this track.
      {
	ULONG sectorlistend;
	ULONG sectorend;
	ULONG sectorlist;
	UBYTE sectorlistheader[8];
	UBYTE idx   = 0;
	UBYTE ext   = 0;
	struct Track::Sector *lastsector = NULL;
	//
	if (!image->Read(trackdata,sectorlistheader,sizeof(sectorlistheader)))
	  throw false;
	//
	// Compute the end of the sector list.
	sectorlist    = trackdata + sizeof(sectorlistheader);
	sectorlistend = trackdata + ((ULONG(sectorlistheader[0]) << 0 ) |
				     (ULONG(sectorlistheader[1]) << 8 ) |
				     (ULONG(sectorlistheader[2]) << 16) |
				     (ULONG(sectorlistheader[3]) << 24));
	//
	// Extended data can start here the earliest.
	sectorend     = sectorlistend;
	//
	if (sectorlistheader[4] != 0x01)
	  Throw(InvalidParameter,"ATXImage::OpenImage","ATX sector list header type invalid, "
		"must be one");
	//
	// Read the sector data.
	while(sectorlist < sectorlistend) {
	  struct Track::Sector *sector;
	  UBYTE sectornumber;
	  UBYTE sectorstatus;
	  UWORD sectorposition;
	  ULONG sectordata;
	  UBYTE sectorheader[8];
	  //
	  if (!image->Read(sectorlist,sectorheader,sizeof(sectorheader)))
	    throw false;
	  //
	  sectornumber   = sectorheader[0];
	  sectorstatus   = sectorheader[1];
	  sectorposition = (UWORD(sectorheader[2]) << 0) | (UWORD(sectorheader[3]) << 8);
	  sectordata     = trackstart + ((ULONG(sectorheader[4]) << 0 ) |
					 (ULONG(sectorheader[5]) << 8 ) |
					 (ULONG(sectorheader[6]) << 16) |
					 (ULONG(sectorheader[7]) << 24));
	  //
	  // Create a new sector.
	  sector                 = new struct Track::Sector();
	  if (lastsector) {
	    lastsector->Next     = sector;
	  } else {
	    track->SectorList    = sector;
	  }
	  lastsector             = sector;
	  sector->SectorIdx      = sectornumber;
	  sector->SectorStatus   = sectorstatus;
	  sector->SectorPosition = sectorposition;
	  sector->HeaderOffset   = sectorlist;
	  sector->Track          = track;
	  //
	  // Add up sectors with extensions.
	  if (sectorstatus & Track::Sector::Extended)
	    ext++;
	  //
	  // How do I get the sector size in bytes???
	  if (sectorstatus & Track::Sector::Missing) {
	    sector->SectorSize   = 0;
	    sector->WeakOffset   = 0;
	    sector->Offset       = 0;
#if CHECK_LEVEL > 0
	    printf("Found a missing sector in track %d, sector %d\n",tracknumber,sectornumber);
#endif
	  } else {
	    sector->SectorSize   = 256; // Fixup later.
	    sector->WeakOffset   = 256; // Fixup later, or never.
	    sector->Offset       = sectordata;
	    if (sectorstatus & Track::Sector::CRCError)
	      sector->WeakOffset = 0;
	  }
#if CHECK_LEVEL > 0
	  {
	    struct Track::Sector *other = track->SectorList;
	    while (other) {
	      if (other != sector && other->SectorIdx == sector->SectorIdx)
		printf("Found duplicate sector in track %d, sector %d\n",
		       tracknumber,sectornumber);
	      other = other->Next;
	    }
	    if (sectorstatus & Track::Sector::CRCError)
	      printf("Found a CRC error in track %d, sector %d\n",tracknumber,sectornumber);
	    if (sectorstatus & Track::Sector::LostData)
	      printf("Found lost data in track %d, sector %d\n",tracknumber,sectornumber);
	    if (sectorstatus & Track::Sector::NoRecord)
	      printf("Found missing record in track %d, sector %d\n",tracknumber,sectornumber);
	  }
#endif
	  //
	  // Advance to the next entry in the sector list.
	  sectorlist            += 1+1+2+4;
	}
	//
	// Read the sector extended data if there are any.
	// The format is a bit misdefined in so far as there
	// is no clear way how to get at the extended data if
	// the sector sizes aren't defined. Thus, use a 
	// best-effort method to check whether the data is correct,
	// and there is a sector to assign it to.
	sectorend  = trackend;
	while(ext && sectorend > trackstart) {
	  UBYTE extendeddata[1+4+1+2];
	  bool found = false;
	  // Check whether the data here is an extended sector data.
	  if (!image->Read(sectorend - sizeof(extendeddata),extendeddata,sizeof(extendeddata)))
	    throw false;
	  //
	  if (extendeddata[4] == 1) {
	    // Interestingly, the format sometimes seem to smuggle in here an empty
	    // sector list header which I just remove.
	    if (extendeddata[0] == 0 && extendeddata[1] == 0 && 
		extendeddata[2] == 0 && extendeddata[3] == 0) {
	      sectorend -= sizeof(extendeddata);
	      continue;
	    }
	  }
	  if (sectorend == trackend &&
	      extendeddata[0] == 0 && extendeddata[1] == 0 && 
	      extendeddata[2] == 0 && extendeddata[3] == 0 &&
	      extendeddata[4] == 0 && extendeddata[5] == 0 && 
	      extendeddata[6] == 0 && extendeddata[7] == 0) {
	    // Sometimes, the format smuggles a zero extended data as trailer in here.
	    sectorend -= sizeof(extendeddata);
	    continue;
	  }
	  if (extendeddata[0] == 0x08) {
	    struct Track::Sector *sector;
	    UBYTE secidx = extendeddata[1+4];
	    UWORD weak   = ((UWORD(extendeddata[1+4+1+0]) << 0) |
			    (UWORD(extendeddata[1+4+1+1]) << 8));
	    //
	    // Maybe extended data.
	    if (secidx < track->Sectors && weak < 512) {
	      // Plausible enough. Ok, try to find the sector this index belongs to.
	      found = true;
	      for(sector = track->SectorList,idx = 0;sector;sector = sector->Next,idx++) {
		if (idx == secidx) {
		  if ((sector->SectorStatus & Track::Sector::Extended) == 0) {
		    Machine->PutWarning("Found extended sector data in ATXImage "
					"for sector %d, track %d "
					"but this sector does not require "
					"any extensions.",
					sector->SectorIdx,track->TrackIdx);
		  }
		  // Yes, indeed, this sector is extended. Fill in its data.
		  sector->WeakOffset      = weak;
		  sector->ExtensionOffset = sectorend - sizeof(extendeddata);
#if CHECK_LEVEL > 0
		  printf("Found weak sector at track %d, sector %d, offset: %d\n",
			 track->TrackIdx,sector->SectorIdx,weak);
#endif
		  // Sector data ends earlier because we have found a valid
		  // sector extension.
		  break;
		}
	      }
	    }
	    ext--;
	  }
	  //
	  // If a valid sector extension has been found, try to locate the next
	  // one, and remove it from the sector data.
	  if (found) {
	    sectorend -= sizeof(extendeddata);
	  } else break;
	}
	//
	// Use a best-effort basis to find the sector size.
	{
	  struct Track::Sector *sector,*other;

	  for(sector = track->SectorList;sector;sector = sector->Next) {
	    if ((sector->SectorStatus & Track::Sector::Missing) == 0) {
	      for(other = track->SectorList;other;other = other->Next) {
		if ((other->SectorStatus & Track::Sector::Missing) == 0) {
		  if (other->Offset > sector->Offset) {
		    if (other->Offset - sector->Offset < sector->SectorSize) {
		      sector->SectorSize = UWORD(other->Offset - sector->Offset);
#if CHECK_LEVEL > 0
		      if (sector->SectorSize != 128)
			printf("Found wierd sector size of %d at track %d, sector %d\n",
			       sector->SectorSize,track->TrackIdx,sector->SectorIdx);
#endif
		    }
		  }
		}
	      }
	      // Also try to match with the track end.
	      if (sectorend > sector->Offset) {
		if (sectorend - sector->Offset < sector->SectorSize) {
		  sector->SectorSize = UWORD(sectorend - sector->Offset);
		  // If this is just a difference of 8, then assume that this
		  // was a dummy extended data at the end.
		  if (sector->SectorSize == 128 + 8)
		    sector->SectorSize = 128;
		  if (sector->SectorSize == 256 + 8)
		    sector->SectorSize = 256;
#if CHECK_LEVEL > 0
		  if (sector->SectorSize != 128)
		    printf("Found wierd sector size of %d at track %d, sector %d\n",
			   sector->SectorSize,track->TrackIdx,sector->SectorIdx);
#endif
		}
	      }
	    }
	    if (sector->SectorStatus == 0 && !havesectorsize) {
	      DefaultSectorSize = sector->SectorSize;
	      havesectorsize = true;
#if CHECK_LEVEL > 0
	      printf("Found nominal sector size of %d bytes.\n",DefaultSectorSize);
#endif
	    }
	  }
	}
	// Done with the track. There is only one sector list header.
      }
      //
      // Advance to the next track.
      trackstart = trackend;
    }
  } catch(bool ioerror) {
    // If the code goes here, reading failed.
    if (errno == 0) {
      Throw(InvalidParameter,"ATXImage::OpenImage",
	    "ATX offset out of range, or ATX image truncated");
    } else {
      ThrowIo("ATXImage::OpenImage","I/O error while reading an ATX image file");
    }
  }
  //
  HeadPosition   = 0;
  TrackUnderHead = 0;
  Image          = image;
}
///

/// ATXImage::PassTime
// Increment the time by the given number of microseconds.
// This moves the head under the moving disk.
void ATXImage::PassTime(ULONG micros)
{ 
  HeadPosition = (HeadPosition + micros) % MusecsPerRotation;
}
///

/// ATXImage::HBI
// Implements the HBIAction and drives the clock: This
// advances the head position
void ATXImage::HBI(void)
{
  PassTime(MuSecsPerLine);
}
///

/// ATXImage::FindSector
// Find a sector given a sector number.
struct ATXImage::Track::Sector *ATXImage::FindSector(UWORD sectornumber,UWORD *delay)
{
  struct Track *track          = TrackList;
  struct Track::Sector *sector = NULL;
  //
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATXImage::FindSector","image is not yet open");
#endif
  //
  //
  if (sectornumber > 0) {
    UBYTE tracknumber  =     (sectornumber - 1) / SectorsPerTrack;
    sectornumber       = 1 + (sectornumber - 1) % SectorsPerTrack;
    //
    // Find the track that has this track number.
    while(track) {
      if (track->TrackIdx == tracknumber)
	break;
      track = track->Next;
    }
    //
    if (track) {
      struct Track::Sector *testsec = track->SectorList;
      ULONG pickupdelay             = MusecsPerRotation;
      ULONG headpos                 = HeadPosition;
      ULONG timespan                = 0;
      //
      // Found the track. Approximate the time due to moving the head.
      if (tracknumber > TrackUnderHead) {
	timespan = (tracknumber - TrackUnderHead) * MusecsPerTrack + MusecsPerSettle;
      } else if (tracknumber < TrackUnderHead) {
	timespan = (TrackUnderHead - tracknumber) * MusecsPerTrack + MusecsPerSettle;
      }
      
      if (delay) {
	*delay        += UWORD(timespan / MuSecsPerLine);
	TrackUnderHead = tracknumber;
      }
      headpos          = (headpos + timespan) % MusecsPerRotation;
      //
      // Note that sector numbers are one-based. Now find the track that is the
      // next one the head will pick up. For that, its sector position
      // must be larger than the current head position, or the first sector if
      // no such sector exists.
      while(testsec) {
	if (testsec->SectorIdx == sectornumber) {
	  ULONG pickuptime;
	  ULONG secpos  = ULONG(testsec->SectorPosition) << 3; // in usecs
	  //
	  // Compute the time that is required to pick up this sector.
	  if (secpos > headpos) {
	    pickuptime = secpos - headpos;
	  } else {
	    // Have to wait another rotation to find it.
	    pickuptime = MusecsPerRotation + secpos - headpos;
	  }
	  if (pickuptime < pickupdelay) {
	    sector      = testsec;
	    pickupdelay = pickuptime;
	  }
	}
	testsec = testsec->Next;
      }
      //
      // Add the delay required to find the sector 
      // (or not, i.e. spend a rotation trying to locate it)
      if (delay)
	*delay += UWORD(pickupdelay / MuSecsPerLine);
    }
  }
  //
  return sector;
}
///

  //
/// ATXImage::SectorSize
// Return the sector size of the image.
UWORD ATXImage::SectorSize(UWORD /*sectornumber*/)
{
  /*
  struct Track::Sector *sector;
  */

#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATXImage::SectorSize","image is not yet open");
#endif
  //
  /*
  sector = FindSector(sectornumber,NULL);
  //
  if (sector == NULL || (sector->SectorStatus & Track::Sector::Missing)) {
    return DefaultSectorSize;
  }
  
  return sector->SectorSize;
  */
  return DefaultSectorSize;
}
///

/// ATXImage::SectorCount
// Return the number of sectors in this image here.
ULONG ATXImage::SectorCount(void)
{
  struct Track *track;
  UWORD count = 0;
  //
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATXImage::SectorCount","image is not yet open");
#endif
  //
  // This is used for identification of the disk format,
  // so return the nominal size.
  for(track = TrackList;track;track = track->Next) {
    count += SectorsPerTrack;
  }
  //
  return count;
}
///

/// ATXImage::Status
// Return the protection status of the image.
UBYTE ATXImage::Status(void)
{
  UBYTE status = 0;

#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATXImage::ProtectionStatus","image is not yet open");
#endif
  
  if (Protected)
    status |= DiskImage::Protected;
  if (CRCError)
    status |= DiskImage::CRCError;
  if (LostDataError)
    status |= DiskImage::LostData | DiskImage::DRQ; // also: request but CPU did not react.
  if (SectorMissing)
    status |= DiskImage::NotFound;
  if (SectorDeleted)
    status |= DiskImage::Deleted;

  return status;
}
///

/// ATXImage::ReadSector
// Read a sector from the image into the supplied buffer. The buffer size
// must fit the above SectorSize. Returns the SIO status indicator.
UBYTE ATXImage::ReadSector(UWORD sectornumber,UBYTE *buffer,UWORD &delay)
{
  struct Track::Sector *sector;
  ULONG offset,size;
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATRImage::ReadSector","image is not yet open");
#endif
  //
  // The transmission overhead is 95 milliseconds
  //
  delay  = 0;
  sector = FindSector(sectornumber,&delay); 
  //
  // Update the FDC hardware flags
  SectorMissing = (sector == NULL || (sector->SectorStatus & Track::Sector::Missing));
  SectorDeleted = (sector)?((sector->SectorStatus & Track::Sector::NoRecord)?true:false):(false);
  CRCError      = (sector)?((sector->SectorStatus & Track::Sector::CRCError)?true:false):(false);
  LostDataError = (sector)?((sector->SectorStatus & Track::Sector::LostData)?true:false):(false);
  //
  // No data present if the sector missing bit is set.
  if (sector == NULL || (sector->SectorStatus & Track::Sector::Missing))
    return 'E';
  //
  // Compute the sector offset now.
  offset = sector->Offset;
  size   = SectorSize(sectornumber);
  //
  if (Image->Read(offset,buffer,size)) {
    //
    // If there is extended data, potentially insert junk into the sector.
    if (sector->SectorStatus & Track::Sector::Extended) {
      UWORD of;
      for(of = sector->WeakOffset;of < size;of++) {
	buffer[of] = (rand() >> 8) & 0xff;
      }
    } 
    //
    // Simulate that the drive head seeks back to track zero and forwards
    // again to the target track to recalibrate.
    if (sector->SectorStatus & (Track::Sector::CRCError |
				Track::Sector::LostData |
				Track::Sector::NoRecord |
				Track::Sector::Missing)) {
      //delay += sector->Track->TrackIdx * LinesPerTrack * 2 + LinesPerSettle;
      return 'E';
    }
    return 'C'; // completed fine.
  }
  return 'E';   // did not.
}
///

/// ATXImage::WriteSector
// Write a sector to the image from the supplied buffer. The buffer size
// must fit the sector size above. Returns also the SIO status indicator.
UBYTE ATXImage::WriteSector(UWORD sectornumber,const UBYTE *buffer,UWORD &delay)
{
  struct Track::Sector *sector;
  ULONG offset;
  UWORD size;
  
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATXImage::WriteSector","image is not yet open");
#endif
  //
  delay  = 0;
  sector = FindSector(sectornumber,&delay);
  //
  SectorMissing = (sector == NULL || (sector->SectorStatus & Track::Sector::Missing));
  SectorDeleted = (sector)?((sector->SectorStatus & Track::Sector::NoRecord)?true:false):(false);
  CRCError      = (sector)?((sector->SectorStatus & Track::Sector::CRCError)?true:false):(false);
  LostDataError = (sector)?((sector->SectorStatus & Track::Sector::LostData)?true:false):(false);
  //
  if (SectorMissing)
    return 'E'; // No chance we can insert the sector.
  //
  // Compute the sector offset now.
  offset = sector->Offset;
  size   = sector->SectorSize;
  //
  if (Image->Write(offset,buffer,size)) {
    UBYTE status;
    // Sector has now no longer a CRC error, nor lost data, nor weak data.
    status = sector->SectorStatus & ~(Track::Sector::CRCError | Track::Sector::LostData);
    // Leave it extended to keep the structure consistent.
    sector->WeakOffset    = size;
    //
    // Write back the status if required.
    if (status != sector->SectorStatus) {
      sector->SectorStatus = status;
      if (!Image->Write(sector->HeaderOffset + 1,&status,sizeof(status))) {
	CRCError = true; // whatever...
	return 'E';
      }
    }
    if (sector->SectorStatus & Track::Sector::Extended) {
      UBYTE weak[2];
      //
      // Update the weak sector offset.
      weak[0] = sector->WeakOffset & 0xff;
      weak[1] = sector->WeakOffset >> 8;
      //
      // Write back the extensions header.
      if (!Image->Write(sector->ExtensionOffset+1+4+1,weak,sizeof(weak))) {
	CRCError = true;
	return 'E';
      }
    }
    return 'C'; // completed fine.
  }
  CRCError = true; // whatever...
  return 'E';   // did not.
}
///

/// ATXImage::ProtectImage
// Protect this image.
void ATXImage::ProtectImage(void)
{
  Protected = true;
}
///

