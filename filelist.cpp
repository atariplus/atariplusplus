/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filelist.cpp,v 1.18 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Definition of a gadget representing a list of files to
 ** choose from, i.e. the basic ingredience for a file requester.
 **********************************************************************************/

/// Includes
#include "filelist.hpp"
#include "requesterentry.hpp"
#include "directory.hpp"
#include "new.hpp"
#include <errno.h>
///

/// FileList::FileList
// Initialize a file requester gadget. This requires also an initial path, and a flag
// whether we can accept only directories or only files.
FileList::FileList(List<Gadget> &gadgets,class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
		   const char *initial,bool save,bool,bool dirsonly)
  : Gadget(gadgets,rp,le,te,w,h),
    DirHandle(NULL),
    // We insert the string and ok gadgets not into the vertical group directly as we do not
    // want to scroll them as part of the vertical group.
    ClipRegion(new class RenderPort(rp,0,te,rp->WidthOf(),h-24)),
    PathGadget(new class StringGadget(InternalGadgets,rp,le,te+h-24,w,12,initial)),
    OKButton(new class ButtonGadget(InternalGadgets,rp,le+w-76,te+h-12,76,12,"OK")),
    CancelButton(new class ButtonGadget(InternalGadgets,rp,le,te+h-12,76,12,"Cancel")),
    Directory(new class VerticalGroup(InternalGadgets,ClipRegion,le,0,w,h-24)),
    DirsOnly(dirsonly), ForSave(save), ActiveGadget(NULL),
    TmpPath(NULL), StatPath(NULL)
{ 
}
///

/// FileList::~FileList
// Cleanup the file list gadget. This also removes the internal gadgets
FileList::~FileList(void)
{
  class Gadget *gadget;
  
  delete[] TmpPath;  
  delete[] StatPath;
  // All gadgets within the vertical group are disposed by the vertical group destructor which
  // is called implicitly. This includes the requester entries.
  while((gadget = InternalGadgets.First())) {
    delete gadget;
  }
  // Close the directory handle if applicable.
  if (DirHandle) {
    // There's not much we could do if this fails.
    closedir(DirHandle);
  }
  delete ClipRegion;
}
///

/// FileList::ReadDirectory
// Read the directory contents of the directory the contents of the path gadget points to.
void FileList::ReadDirectory(void)
{
  size_t baselen,statlen = 0;
  LONG le,te,w,h;
  class Gadget *g;
  char *pathpart;
  //
  // Close the directory handle first if it is still open.
  if (DirHandle) {
    closedir(DirHandle);
    DirHandle = NULL;
  }
  // Dispose the contents of the directory now. All gadgets within the vertical group are of
  // this kind.
  while((g = Directory->First())) {
    // Note that the direntry structure unlinks itself from the list
    delete g;
  }
  // The dimensions are those of the vertical group
  le = LeftEdge; // We start right of this gadget.
  te = 0;     // at the top.
  w  = Width-12;
  h  = 8;
  //
  // Check whether we can open the new directory contents here. If not, then the list remains blank.
  // Construct the name of the directory we are in. Hence, we need to strip off the last file.
  PathGadget->ReadContents(TmpPath);
  pathpart  = PathPart(TmpPath);
  if (pathpart) {
    *pathpart = '\0';
  }
  if (*TmpPath && pathpart) {
    DirHandle = opendir(TmpPath);
  } else {
    DirHandle = opendir(".");
    // If we are here, then the temporary path is the current
    // directory. We better clear it here.
    delete[] TmpPath;
    TmpPath  = NULL;
    TmpPath  = new char[2];
    strcpy(TmpPath,".");
  }
  // Initialize the additional path we keep for stat'ing the entries of the
  // directory here.
  delete[] StatPath;
  StatPath = NULL;
  statlen  = 0;
  baselen  = strlen(TmpPath);
  //
  if (DirHandle) {
    struct dirent *de;
    struct stat st;
    // Ok, that worked. Now parse off one directory entry after another.
    while((de = readdir(DirHandle))) {
      class RequesterEntry *re,*in;
      const char *name;
      size_t len;
      // Allocate/enlarge the buffer for stat'ing
      len = baselen + 2 + strlen(de_name(de));
      if (len > statlen) {
	delete[] StatPath;
	StatPath = NULL;
	StatPath = new char[len];
	statlen  = len;
      }
      name = de_name(de);
      strcpy(StatPath,TmpPath);
      strcat(StatPath,"/");
      strcat(StatPath,name);
      // Done with that, now we are ready to stat.
      if (stat(StatPath,&st) == 0) {
	if (name[0] != '.' || !strcmp(name,".") || !strcmp(name,"..")) {
	    // And create new entries for the contents. Note that these allocate the names
	    // themselves and add themselves to the list.
	    re = new class RequesterEntry(*Directory,ClipRegion,le,te,w,h,de_name(de),
					  S_ISDIR(st.st_mode)?(true):(false));
	    // The requester entry is a gadget and adds itself automatically to the end of the
	    // gadget list. However, we prefer to keep the directory sorted. For that reason, 
	    // remove it here and re-insert it in the right place. This does not yet adjust
	    // or correct their positions. Later!
	    re->Remove();
	    for(in = (class RequesterEntry *)Directory->First();in;in=(class RequesterEntry *)in->NextOf()) {
		if (re->Compare(in) < 0) {
		    Directory->InsertBefore(re,in);
		    break;
		}
	    }
	    // If we haven't found an insertion position yet, add it to the tail.
	    if (in == NULL) Directory->AddTail(re);
	    te += h;
	}
      }
    }
    // Ok, the nodes are now sorted in order, but their positions are not. We now perform this step.
    // This is a bit hacky since we need to move all gadgets around to their final target
    // position.
    for(te = 0,g = Directory->First();g;g = g->NextOf()) {
      g->MoveGadget(0,te - g->TopEdgeOf());
      te += h;
    }
    // Add some blank filler gadgets to get a consistent layout if we cannot fill up
    // the entire screen.
    while(te + h <= ClipRegion->HeightOf()) {
      new class RequesterEntry(*Directory,ClipRegion,le,te,w,h,NULL,false);
      te += h;
    }
    // The DirHandle remains open to indicate that we are now requesting this directory.
  }
}
///

/// FileList::AttachPath
// Attach the given string to the string stored in the path gadget. This either enters a
// directory, or goes up one directory, or just accepts a file name. Details depend on the
// type we're working in.
void FileList::AttachPath(const char *add)
{
  bool reread = false;
  char *path;
  
  if (!strcmp(add,"./")) {
    // "current directory". If so, then strip off the file part and leave the directory alone.
    PathGadget->ReadContents(TmpPath);
    // Find the path part of the string and strip it off.
    path = PathPart(TmpPath);
    if (path) {
      // We found where the path stops. Leave the '/' active 
      if (*path)
	path[1] = '\0';    
      PathGadget->SetContents(TmpPath);
    } else {
      PathGadget->SetContents("./");
    }
  } else if (!strcmp(add,"../")) {
    // "upper directory". If so, then first strip off the file part, and then strip off the
    // last directory level if possible.
    PathGadget->ReadContents(TmpPath);
    // If the path is "." or "./", this is the current directory. Just erase, and go up
    // then.
    if (!strcmp(TmpPath,".") || !strcmp(TmpPath,"./")) {
      *TmpPath = '\0';
    }
    //
    path = PathPart(TmpPath);
    if (path) {
      // We found the path part. Everything behind is a file. Strip it off, including the '/'
      *path = '\0';
      // Check whether we have a previous / in front.
      path = strrchr(TmpPath,'/');
      if (path) {
	// We do. Find the position of the name of the directory above. This is at path+1.
	path++;
      } else {
	// We found no "/" in front meaning that the directory name starts at the beginning.
	path = TmpPath;
      }
      // Path points now to the beginning of the directory name of the directory above.
      // We do. Now two things could happen. Either,  it is a "../" in which case we
      // have to attach another "..". Or it is a directory name, in which case we have to 
      // remove it. Or we have "/" as path name in which case we do nothing about it.
      if (strcmp(TmpPath,"/")) {
	// Not the root directory.
	if (!strcmp(path,"..")) {
	  char *t;
	  // We have .. as current directory. Attach another level.
	  t = new char[strlen(TmpPath) + 5];
	  strcpy(t,TmpPath);
	  strcat(t,"/../");
	  delete[] TmpPath;
	  TmpPath = t;
	  PathGadget->SetContents(TmpPath);
	} else {
	  // It is not .. In that case, just remove it.
	  if (strlen(TmpPath) == 0) {
	    // In case we killed it entirely, replace it by "../"
	    PathGadget->SetContents("../");
	  } else {
	    *path = '\0';
	    PathGadget->SetContents(TmpPath);
	  }
	}      
	// Now re-install the path.
	reread = true;
      }
    } else {
      // We have no path part. In that case, just go to ".." directly.
      PathGadget->SetContents("../");
      reread = true;
    }
  } else {
    int addslash = 0;
    char *t;
    // We add a regular file/directory name. If we have a file attached to the end
    // of the current setting, remove it.
    PathGadget->ReadContents(TmpPath);
    // Find the path part of the string and strip it off.
    path = PathPart(TmpPath);
    if (path) {
      // We found where the path stops. Leave the '/' active 
      if (*path) {
	path[1] = '\0';
      } else if (path > TmpPath) {
	// Ensure that the path ends on a "/"
	addslash = 1;
      }
    } else {
      // No path part. Clean it entirely.
      *TmpPath = '\0';
    }
    // Now attach the name of the object. Reserve a bit more room for an additional "/" we
    // might add in case we deal with directories.
    t = new char[strlen(TmpPath) + strlen(add) + 2 + addslash];
    strcpy(t,TmpPath);
    if (addslash)
      strcat(t,"/");
    strcat(t,add);
    delete[] TmpPath;
    TmpPath = t;
    if (IsDirectory(TmpPath)) {
      // If we found a directory, attach a "/" at the end. We allocated enough
      // to ensure that there is the room for it.
      // Well, we do this now already when the directory is read in, so no need
      // to perform this again here.
      /*
      strcat(TmpPath,"/");
      */
      reread = true;
    } else if (DirsOnly) {
      // Do not allow the selection of files, abort here.
      return;
    }
    // Problem case: In case there is a ".." at the end and we re-enter the directory
    // we originally came from, the ".." and "add" should cancel. However, this would be a
    // bit cumbersome to implement and we'd better leave this alone, at least for the
    // moment.
    PathGadget->SetContents(TmpPath);
  }
  // Now re-read the directory.
  if (reread) {    
    Directory->ScrollTo(0x0000);
    ReadDirectory();
  }
  // And redisplay the contents.
  Refresh();
}
///

/// FileList::PathPart
// Return a pointer to the character that separates the directory name from the file part.
char *FileList::PathPart(char *name)
{
  char *result;
  //
  // Get a pointer to last character, i.e. to the NUL.
  result = name + strlen(name);
  // Check whether we deal with a file or a directory. 
  if (IsDirectory(name)) {
    // We're dealing with a directory. This should hopefully end on a "/". If not, return a 
    // pointer to the NUL, otherwise return a pointer to the last slash.
    if (result > name && result[-1] == '/')
      result--;
    return result;
  } else {
    // Is not a directory, but a file. If so, check for the last slash. If the last character is a slash,
    // remove it here. 
    if (result > name && result[-1] == '/') {
      result--;
    }
    // Now scan reversly to the "/" or up to the start of the stream. strrchr is not sufficient here.
    while(result > name) {
      if (*result == '/')
	break;
      result--;
    }
    if (*result == '/') {
      return result;
    } else {
      // Well, there is simply no path part here...
      return NULL;
    }
  }
}
///

/// FileList::IsDirectory
// Check whether the given path represents a directory like entry.
// Returns true if so.
bool FileList::IsDirectory(const char *name)
{
  size_t len = strlen(name);
  char *buf  = new char[len + 1];
  bool isdir = false;    
  struct stat st;
  
  strcpy(buf,name);
  // Dispose a trailing "/" temporary
  if (len) {
    if (buf[len-1] == '/')
      buf[len-1] = 0;
  }
  if (stat(buf,&st) == 0) {
    // Success. If so, we check the type. Note that stat resolves
    // soft links. This is the desired behaivour.
    if (S_ISDIR(st.st_mode)) {
      isdir = true;
    }
  }
  delete[] buf;
  
  return isdir;
}
///

/// FileList::IsFile
// Check whether the given path represents a file like entry.
// Returns true if so. This is called to decide whether we have to remove a trailing "/", so
// be gracious and handle "special files" like regular ones.
bool FileList::IsFile(const char *name)
{  
  size_t len  = strlen(name);
  char *buf   = new char[len + 1];
  bool isfile = false;
  struct stat st;

  strcpy(buf,name);
  // Dispose a trailing "/" temporary
  if (len) {
    if (buf[len-1] == '/')
      buf[len-1] = 0;
  }
  //
  errno = 0;
  if (stat(buf,&st) == 0) {
    // Success. Everything *but* a directory is handled as a "file". Not quite correct,
    // but what we need for the purpose of this gadget.
    if (!S_ISDIR(st.st_mode)) {
      isfile = true;
    }
  } else {
    //
    // If we got the error that the name is in fact a file name, return a "is file" as well.
    if (errno == ENOTDIR) {
      isfile = true;
    }
  }

  delete[] buf;

  return isfile;
}
///

/// FileList::HitTest
// Forward an input event to this gadget
bool FileList::HitTest(struct Event &ev)
{
  class Gadget *g;
  // Mouse wheel events are handled here directly to allow scrolling within
  // the complete region.
  if (ev.Type == Event::Wheel) {
    if (Within(ev)) {
      ev.X = Directory->LeftEdgeOf();
      ev.Y = Directory->TopEdgeOf()+TopEdge;
    }
  }
  //
  // Now check the internal gadgets for "hitting". This could be an "ok" activity or something else.
  for(g = InternalGadgets.First();g;g = g->NextOf()) {
    if ((g == ActiveGadget || ActiveGadget == NULL)) {
      bool hit;
      // In case we are testing the directory gadget for the hit,
      // adjust the offset by the clipping. Yuck! We really should
      // have used relative coordinates for the gadgets all the way.
      if (g == Directory) {
	ev.Y -= TopEdge;
      }
      hit = g->HitTest(ev);
      if (g == Directory) {
	ev.Y += TopEdge;
      }
      if (hit) {
	// The gadget has been hit. Now check what we're up to.
	switch(ev.Type) {
	case Event::GadgetDown:
	  // We do nothing on a gadgetdown directly except that we block all activity
	  // in the vertical group until we're done with it. We do not forward this
	  // event to the outside.
	  if (ev.Object == CancelButton || ev.Object == OKButton || ev.Object == PathGadget) {
	    ActiveGadget   = ev.Object;
	  } else if (ev.Object) {
	    // All remaining gadgets are part of the vertical group and hence the requester
	    // body.
	    ActiveGadget   = Directory;
	  }
	  ev.Object      = NULL;
	  break;
	case Event::GadgetUp:
	  // The/a gadget goes up again. Release the upper list. Then care for the gadget htat
	  // requires activity.
	  ActiveGadget   = NULL;
	  if (ev.Object == OKButton) {
	    // The user hit "OK". Verify that the contents is a "reachable" file and if so,
	    // signal a final "gadget up" to quit.
	    if (ForSave || IsFile(PathGadget->GetStatus()) || IsDirectory(PathGadget->GetStatus()) ) {
	      // Let this object go up.
	      ev.Object = this;
	      ev.Button = true;
	    } else {
	      // Ignore this event.
	      ev.Object = NULL;
	      ev.Type   = Event::Nothing;
	    }
	    break;
	  } else if (ev.Object == CancelButton) {
	    // We return a gadgetup to the caller, but do not send an "ok". For that, the 
	    // object has to be cleared.
	    ev.Object = this;
	    ev.Button = false;
	  } else if (ev.Object == PathGadget) {
	    char *last;
	    // The path gadget goes up. If this happens, then a new component has been
	    // entered. We verify its type: On a directory type, we re-read the
	    // directory contents.
	    PathGadget->ReadContents(TmpPath);
	    last = TmpPath + strlen(TmpPath); // Pointer behind the last character.
	    if (IsDirectory(TmpPath)) {
	      // Is a directory. Check whether the string ends on "/". If not, add one.
	      if (last == TmpPath || last[-1] != '/') {
		char *t;
		// Need to add a "/" for it.
		t = new char[strlen(TmpPath) + 2];
		strcpy(t,TmpPath);
		strcat(t,"/");
		delete[] TmpPath;
		TmpPath = t;
	      }
	    } else if (IsFile(TmpPath)) {
	      // Is a file. If so, check whether the string ends on "/". If it does,
	      // then strip it off.
	      if (last > TmpPath && last[-1] == '/') {
		*last = '\0';
	      }
	    }
	    // Now re-install this setting and re-read the directory.
	    PathGadget->SetContents(TmpPath);
	    Directory->ScrollTo(0x0000);
	    ReadDirectory();
	    Refresh();
	    // Do not forward this request to the outside.
	    ev.Object = NULL;
	  } else if (ev.Object) {
	    const char *entry;
	    // Request from the directory selector, but only if this picked a gadget.
	    // Set this gadget as the "picked" choice and un-pick all other gadgets in the vertical
	    // group.
	    // Now try to "go into" this directory
	    entry = ((class RequesterEntry *)ev.Object)->GetStatus();
	    if (entry) {
	      AttachPath(entry); // includes a refresh
	    }
	    // All other events are never visible from the outside.
	    ev.Object = NULL;
	  }
	  break;
	default:
	  break;
	}
	return true;
      }
    }
  }
  return false;
}
///

/// FileList::Refresh
// Refresh the contents of this gadget and all internal subgadgets
void FileList::Refresh(void)
{
  class Gadget *gadget;
  // If we haven't read yet the directory contents, do now.
  if (DirHandle == NULL) {
    ReadDirectory();
    // And refresh now...
  }
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,8);
  // Now perform the refresh for all gadgets within the internal gadget list.
  for(gadget = InternalGadgets.First();gadget;gadget = gadget->NextOf()) {
    gadget->Refresh();
  }
}
///

/// FileList::FindGadgetInDirectory
// Check for the nearest gadget in the given direction dx,dy,
// return this (or a sub-gadget) if a suitable candidate has been
// found, alter x and y to a position inside the gadget, return NULL
// if this gadget is not in the direction.
const class Gadget *FileList::FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const
{
  LONG mx = x,my = y - TopEdge;
  LONG xmin=x,ymin=y;
  const class Gadget *f,*gad = NULL;
  LONG mdist,dist = 0;
  //
  f = Directory->FindGadgetInDirection(mx,my,dx,dy);
  if (f) {
    gad  = f;
    my  += TopEdge;
    dist = (mx - x) * (mx - x) + (my - y) * (my - y);
    xmin = mx;
    ymin = my;
  }
  //
  mx = x,my = y;
  f = Gadget::FindGadgetInDirection(InternalGadgets,mx,my,dx,dy);
  if (f != Directory) {
    mdist = (mx - x) * (mx - x) + (my - y) * (my - y);
    if (gad == NULL || (mdist < dist)) {
      gad  = f;
      xmin = mx;
      ymin = my;
    }
  }
  //
  if (gad) {
    x = xmin;
    y = ymin;
    return gad;
  }
  return NULL;
}
///
