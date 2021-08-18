/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gtia.hpp,v 1.71 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: GTIA graphics emulation
 **********************************************************************************/

#ifndef GTIA_HPP
#define GTIA_HPP

/// Includes
#include "types.hpp"
#include "page.hpp"
#include "machine.hpp"
#include "chip.hpp"
#include "argparser.hpp"
#include "hbiaction.hpp"
#include "vbiaction.hpp"
#include "saveable.hpp"
#include "display.hpp"
///

/// Forward declarations
class Monitor;
class PostProcessor;
///

/// Class GTIA
// This class emulates (undoubtfully) the graphics television interface
// adapter responsible for graphics, player-missle and trigger input.
class GTIA : public Chip, public Page, public Saveable, private HBIAction { 
public:
  // Color registers are indexed by one of the following names
  // Note that we use internally more names than there are hardware
  // registers
  typedef enum {
    Player_0            = 0,
    Player_1            = 1,
    Player_2            = 2,
    Player_3            = 3,
    Playfield_0         = 4,  // playfield color register 0
    Playfield_1         = 5,
    Playfield_2         = 6,
    Playfield_3         = 7,
    Background          = 8,  // ColBK = color of the background
    Playfield_1_Fiddled = 9,  // Special PF1 that has the hue of PF2
    Playfield_Artifact1 = 10, // Fiddled artifacted PF1, 01 transition
    Playfield_Artifact2 = 11, // Fiddled artifacted PF1, 10 transition
    Player_0Or1         = 12, // Color of Player 0 or'd with Player 1
    Player_2Or3         = 13, // Color of Player 2 or'd with Player 3
    Black               = 14, // always black for priority conflict
    Background_Mask     = 15, // again a copy of the background register
    PreComputedEntries  = 16  // Size of this table in entries
  } PreComputedColor;
  //
  // Packed RGB color value for true color output
  typedef AtariDisplay::PackedRGB PackedRGB;
  //
private:
  // Color maps for PAL and NTSC follow here.
  static const struct ColorEntry PALColorMap[256];
  static const struct ColorEntry NTSCColorMap[256];
  //
  // Potentially: An external color map.
  struct ColorEntry *ExternalColorMap;
  //
  // The name of the external color map. The one to supply,
  // and the one we have.
  char *ColorMapToLoad;
  const char *LoadedColorMap;
  //
private:
  // Color lookup table, indexed by the player/playfield bits. Note that
  // we have to insert the fiddled color here as well, and we need to
  // mask out bit #0 here manually.
  //
  UBYTE ColorLookup[PreComputedEntries];
  //
  // The player/missile graphics generator. We have one per player and one for the missiles
  struct PMObject {
    UBYTE Graphics;           // the graphics data of this player/or all missiles
    UBYTE Size;               // The size flags for the object
    UBYTE HPos;               // The horizontal position of this object  
    UBYTE DecodedSize;        // Decoded size in half color clocks
    // Collisions are not detected if their positionis left of 
    // the indicated border, or right of the indicated border.
    // This might be because GTIA doesn't simply generate any video data for
    // them if they are out of range, hence player data then never enters
    // the priority logic. Originally, these are 34,221. After applying 
    // PM_XPos, you get these figures, though. Both are inclusive.
    // This happens by simply clipping P/M graphics to the borders below.
    static const int Player_Left_Border  INIT(4);
    static const int Player_Right_Border INIT(380);
    // Collision registers. There's no need to mirror the precise
    // hardware registers. We keep all the stuff here and extract 
    // the information as soon as needed.
    UBYTE DisplayMask;        // identification of this object in the intermediate player array
    UBYTE MeMask;             // the collisions this object creates as a bit mask
    UBYTE CollisionPlayer;    // collisions with other players: Hardware register mirror
    UBYTE CollisionPlayfield; // collisions with the playfield: Hardware register mirror
    // Collision lookup masks. Collision register reads are and-ed with 
    // these before values are returned.
    UBYTE PlayerColMask;      // collision mask for other players
    UBYTE PlayfieldColMask;   // collision mask for the playfields
    //
    // Decoded position of the object
    int   DecodedPosition;    // the target position in half color clocks
    //
    //
    PMObject(void)
      : DisplayMask(0), MeMask(0), 
	PlayerColMask(0x0f), PlayfieldColMask(0x0f),
	DecodedPosition(-64)
    { }
    //
    // Reset all the position information on a GTIA reset.
    void Reset(void)
    {
      Graphics           = 0;
      Size               = 0;
      DecodedSize        = 1;
      HPos               = 0;
      CollisionPlayer    = 0;
      CollisionPlayfield = 0;
      DecodedPosition    = -64;
    }
    // Render the object into the target, possibly remove the leftmost n
    // bits because they are already on the screen.
    void Render(UBYTE *target,int bitsize,UBYTE graf,int deltapos = 0,int deltabits = 0);
    //
    // Render the object with the data in its graphics register.
    void Render(UBYTE *target,int bitsize)
    {
      Render(target,bitsize,Graphics);
    }
    //
    // Remove all objects right to the indicated half color clock.
    // Bitsize is the size of the object in bits in the graphics shift register.
    void RemoveRightOf(UBYTE *target,int bitsize,int retrigger);
    //
    // Reposition an object. Bitsize is the size of
    // the object in bits (8 for players, 2 for missiles), val
    // the new horizontal position, retrigger the earliest
    // possible retrigger position.
    void RetriggerObject(UBYTE *target,int bitsize,UBYTE val,int retrigger);
    //
    // Update the size register while the object is on the screen.
    // Arguments are the render target buffer, the size of the object
    // in bits occupied in its shift register, the new value of the
    // shift scaler as raw value, and the screen position in half
    // color clocks where the change should take place.
    void RetriggerSize(UBYTE *target,int bitsize,UBYTE val,int retrigger);
    //
    // Reposition the object without drawing it.
    void RepositionObject(UBYTE val)
    {
      HPos            = val;
      DecodedPosition = (val - 0x20) << 1;
    }
    //
    // Resize the object without actually drawing it.
    void ResizeObject(UBYTE val)
    { 
      Size = val & 0x03;
      
      switch(val & 0x03) {
      case 0:
      case 2:
	DecodedSize = 0;
	break;
      case 1:
	DecodedSize = 1;
	break;
      case 3:
	DecodedSize = 2;
	break;
      }
    }
    //
    // Change the shape of the object.
    void ReshapeObject(UBYTE val)
    {
      Graphics = val;
    }
    //
  }     Player[4],Missile[4]; // four players, for missiles
  //
  //
  // A display generator. Depending on fiddling/non-fiddling/GTIA mode, we might
  // have several of them. These objects merge the antic output for one
  // CPU clock cycle = four half color clocks into the output color and
  // deliver the data to the priority generator for the P/M graphics.
  // They also trigger the collision detection.
  class DisplayGeneratorBase {
  protected:
    // Pointer to the GTIA since we need its priority engine and its
    // collision detection logic.
    class GTIA           *gtia;
    const UBYTE          *ColorLookup;
    // Color fiddling hue mixer.
    const UBYTE          *HueMix;
    //    
  public:    
    // The collision mask for this mode. Must be initialized by the 
    // descendents of this class and is used by the main loop for
    // the collision detection.
    const UBYTE          *CollisionMask;
    //
    DisplayGeneratorBase(class GTIA *parent)
      : gtia(parent)
    { 
      // For efficient look-up, get the color table.
      ColorLookup = gtia->ColorLookup;
      HueMix      = gtia->HueMix;
    }
    //
    virtual ~DisplayGeneratorBase(void)
    { }
    // This does the main job of a display generator: Merging of the playfield
    // and player data into the output, running the priority and collision
    // engine of the gtia class.
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player) = 0;
    //
    // The next one is run at the start of each scanline to reset internal
    // variables. Not all classes implement it, some have an internal memory
    // that must get reset.
    virtual void SignalHBlank(void)
    { 
    }
  };
  //
  // Generator base for an intermediate LUT that maps fiddled/unfiddled colors
  // to the input data of one of the "processed" modes, i.e. PRIOR & 0xc0 != 0.
  class IntermediateResolver {
  protected:
    // Type definition and implementation of the preprocessor step    
    // The processed modes require an intermediate lookup step
    // that is resolved by a table like this if required.
    typedef UBYTE IntermediateLut[4][PreComputedEntries];
    //
    // This is a table of four lookup-tables that each map the state of
    // one of the four half-color clocks the display generator is based
    // upon into an intermediate output value. Typically, the outputs
    // of these tables are merged by binary OR and then form the input
    // of further processing. Which kind of LUT is active depends on
    // whether we have a fiddled or an unfidled mode.
    const UBYTE  (*Lut)[PreComputedEntries];
  public:
  };
  //
  // Descendents of the above. These two do nothing more than just to
  // initialize the Lut on construction accordingly
  // Preprocessor for unfiddled output. This is the unusual case
  // (i.e. graphics 9..11 on a antic DLIst that does not use HIRES)
  // We derive virtually here since the descendent classes derive
  // not directly from here, but indirectly from the IntermediateResolver
  // to keep the number of distinguished constructors for LUT setup small.
  class IntermediateResolverUnfiddled : public virtual IntermediateResolver {
  public:
    IntermediateResolverUnfiddled(void);
  };
  // The same for fiddled colors, the regular case.
  class IntermediateResolverFiddled : public virtual IntermediateResolver {
  public:
    IntermediateResolverFiddled(void);
  };
  //
  // The generator of unprocessed, unfiddled 00 modes; these work without
  // an intermediate LUT and hence have a separate base class
  class DisplayGenerator00Preprocessor : public DisplayGeneratorBase {
  public:
    DisplayGenerator00Preprocessor(class GTIA *parent)
      : DisplayGeneratorBase(parent)
    { }
  };
  //
  // The processor for unprocessed, unfiddled modes
  class DisplayGenerator00Unfiddled : public DisplayGenerator00Preprocessor {
  public:
    DisplayGenerator00Unfiddled(class GTIA *parent);
    //
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player);
  };
  // The same for fiddled, but without artefacts
  class DisplayGenerator00Fiddled : public DisplayGenerator00Preprocessor {
  public:
    DisplayGenerator00Fiddled(class GTIA *parent);
    //
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player);
  };
  // The same for fiddled and artefacted
  class DisplayGenerator00FiddledArtefacted : public DisplayGenerator00Preprocessor {
    UBYTE last;  // Last generated pixel, required for the delay line to detect 01 and 10 edgesx
    UBYTE other; // last generated background color.
    //
  public:
    DisplayGenerator00FiddledArtefacted(class GTIA *parent);
    //
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player);    
    //
    // Reset the "last" state here by shifting in the background color.
    virtual void SignalHBlank(void)
    {
      last  = Background;
      other = 0x00;
    }
  };
  // Display generator base for mode 0x40, graphics 9, color fiddling mode
  // still unresolved. Virtually derived since the descendents still
  // need to choose one of the two possible LUTs for intermediate processing,
  // but I don't want to type the post-processing step more than absolutely
  // necessary.
  class DisplayGenerator40Base : public DisplayGeneratorBase,
				 private virtual IntermediateResolver {
  public:
    DisplayGenerator40Base(class GTIA *parent);
    //
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player);
  };
  // Display generator base for mode 0x80, graphics 10, color fiddling mode
  // still unresolved.
  class DisplayGenerator80Base : public DisplayGeneratorBase,
				 private virtual IntermediateResolver {
    //
    UBYTE oc; // last color, implements the delay line of graphics 10
    // 
  public:
    DisplayGenerator80Base(class GTIA *parent);
    //
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player);
    //
    // Reset the delay-line by shifting in the background
    virtual void SignalHBlank(void)
    {
      oc = Background;
    }
  };
  // Display generator base for mode 0xc0, graphics 11, color fiddling mode
  // still unresolved.
  class DisplayGeneratorC0Base : public DisplayGeneratorBase,
				 private virtual IntermediateResolver {
  public:
    DisplayGeneratorC0Base(class GTIA *parent);
    //
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player);
    //
  };
  //  
  // The next "strange" mode is used whenever mode 0x40 or mode 0xC0 are disabled
  // in the middle of a scanline. It has a rather bizarre mapping in which it
  // distinguishes between playfield background and frame, and uses PF0..PF3
  // as target registers, not PFBK..PF2. Wierd enough.
  class DisplayGeneratorStrangeBase : public DisplayGeneratorBase,
				      private virtual IntermediateResolver {
  public:
    DisplayGeneratorStrangeBase(class GTIA *parent);
    //
    virtual void PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player);
    //
  };
  //
  // Now for the final display generators: combine the processed modes
  // with fiddled and unfiddled access. These finally setup the LUT for
  // the intermediate processing steps by deriving from the fiddled or unfiddled
  // intermediate setup class virtually.
  // First for mode 40
  class DisplayGenerator40Unfiddled : public DisplayGenerator40Base,
				      private virtual IntermediateResolverUnfiddled
  {
  public:
    DisplayGenerator40Unfiddled(class GTIA *parent)
      : DisplayGenerator40Base(parent)
    { }
  };  
  class DisplayGenerator40Fiddled : public DisplayGenerator40Base,
				    private virtual IntermediateResolverFiddled
  {
  public:
    DisplayGenerator40Fiddled(class GTIA *parent)
      : DisplayGenerator40Base(parent)
    { }
  };
  //
  // Mode 80
  class DisplayGenerator80Unfiddled : public DisplayGenerator80Base,
				      private virtual IntermediateResolverUnfiddled
  {
  public:
    DisplayGenerator80Unfiddled(class GTIA *parent);
  };  
  class DisplayGenerator80Fiddled : public DisplayGenerator80Base,
				    private virtual IntermediateResolverFiddled
  {
  public:
    DisplayGenerator80Fiddled(class GTIA *parent)
      : DisplayGenerator80Base(parent)
    { }
  };
  //
  // Mode C0
  class DisplayGeneratorC0Unfiddled : public DisplayGeneratorC0Base,
				      private virtual IntermediateResolverUnfiddled
  {
  public:
    DisplayGeneratorC0Unfiddled(class GTIA *parent)
      : DisplayGeneratorC0Base(parent)
    { }
  };  
  class DisplayGeneratorC0Fiddled : public DisplayGeneratorC0Base,
				    private virtual IntermediateResolverFiddled
  {
  public:
    DisplayGeneratorC0Fiddled(class GTIA *parent)
      : DisplayGeneratorC0Base(parent)
    { }
  };
  // Strange mode
  class DisplayGeneratorStrangeUnfiddled : public DisplayGeneratorStrangeBase,
					   private virtual IntermediateResolverUnfiddled
  {
  public:
    DisplayGeneratorStrangeUnfiddled(class GTIA *parent)
      : DisplayGeneratorStrangeBase(parent)
    { }
  };
  class DisplayGeneratorStrangeFiddled : public DisplayGeneratorStrangeBase,
					 private virtual IntermediateResolverFiddled
  {
  public:
    DisplayGeneratorStrangeFiddled(class GTIA *parent)
      : DisplayGeneratorStrangeBase(parent)
    { }
  };
  //
  // Pointers to all modes we could possibly support. The GTIA main/priority select then
  // picks the mode that applies here.
  class DisplayGenerator00Unfiddled         *Mode00UF;
  class DisplayGenerator00Fiddled           *Mode00F;
  class DisplayGenerator00FiddledArtefacted *Mode00FA;
  class DisplayGenerator40Unfiddled         *Mode40UF;
  class DisplayGenerator40Fiddled           *Mode40F;
  class DisplayGenerator80Unfiddled         *Mode80UF;
  class DisplayGenerator80Fiddled           *Mode80F;
  class DisplayGeneratorC0Unfiddled         *ModeC0UF;
  class DisplayGeneratorC0Fiddled           *ModeC0F;
  class DisplayGeneratorStrangeUnfiddled    *ModeSUF;
  class DisplayGeneratorStrangeFiddled      *ModeSF;
  //
  // The current mode
  class DisplayGeneratorBase                *CurrentGenerator;
  //
  // Current post-processor, if any.
  class PostProcessor        *PostProcessor;
  //
  // Combined Player/Missile/Playfield control register
  // Prior is the currently visible one, PriorAhead what the CPU
  // wrote into the register, one step earlier, InitialPrior the value
  // at the start of the line.
  // PriorAhead is currently not used.
  UBYTE                       Prior,InitialPrior,PriorAhead;
  //
  // Since fiddling is also part of the mode selection, this
  // is also kept here on a per-scanline basis
  bool                        Fiddling;
  //
  // Rendering target of the P/M engine within the P/M intermediate buffer
  // before it gets merged by the display generator.
  UBYTE                      *PMTarget;
  //
  // Graphics control register.
  // This register is only evaluated at the beginning of a line, not in
  // the middle. As we run the GTIA scanline in the middle, we have to
  // keep a shadow that is updated after the scanline. Decathlon seems
  // to require this, but Gateway to Apshai does not and conflicts.
  // Fixed by moving WSYNCPOS to the right.
  UBYTE                       Gractl,GractlShadow;
  //
  // Vertical overall delay register. Actually, this is required by ANTIC
  UBYTE                       VertDelay;
  // 
  // true if missile 0..3 get playfield color 3
  bool                        MissilePF3; 
  //
  // true if the console speaker is on
  bool Speaker;
  //
  // Active keyboard line for the 5200 model. Unused otherwise.
  UBYTE ActiveInput;
  //
  // The current horizontal position in half-color clocks.
  int   HPos;
  //
  // High level collision setup. The following masks are visible from the
  // configuration interface
  enum CollisionMask {
    NoneC    = 0,
    PlayerC  = 1,
    MissileC = 2,
    AllC     = 3
  };
  //
  // Collisions players can cause
  LONG PlayerCollisions[4];
  //
  // Collisions the playfield can cause.
  LONG PlayfieldCollisions[4];
  //
  static const int PMScanlineSize  INIT(640); // maximum size of a PM scanline
  //
  // Quick color lookup registers for the priority engine. This is indexed
  // by a bitmask set by the individual player/missile presence. Bit 0 is
  // player 0 and so on. PM-Graphics comes in two layers: Player 0,1 and
  // Player 2,3. Bit 4 is reserved for player 5, the missiles combined.
#ifdef HAS_MEMBER_INIT
  static const int PlayerColorLookupSize = 32;
#else
#define            PlayerColorLookupSize   32
#endif
  PreComputedColor Player0ColorLookup[PlayerColorLookupSize];
  PreComputedColor Player2ColorLookup[PlayerColorLookupSize];
  PreComputedColor Player4ColorLookup[PlayerColorLookupSize];
  //
  // Pre-computed colors of players in front of the given playfields.
  PreComputedColor Player0ColorLookupPF01[PlayerColorLookupSize];
  PreComputedColor Player2ColorLookupPF01[PlayerColorLookupSize];
  PreComputedColor Player4ColorLookupPF01[PlayerColorLookupSize];
  PreComputedColor Player0ColorLookupPF23[PlayerColorLookupSize];
  PreComputedColor Player2ColorLookupPF23[PlayerColorLookupSize];
  PreComputedColor Player4ColorLookupPF23[PlayerColorLookupSize];
  //
  // Playfield priority masks for playfields 0,1 and playfields 2,3
  UBYTE            Playfield01Mask[PlayerColorLookupSize];
  UBYTE            Playfield23Mask[PlayerColorLookupSize];
  //
  // The output scanline buffer generated by GTIA
  UBYTE           *PlayerMissileScanLine;  
  //
  //
  // Color artifact hue mixer. Input is bit 0: artefacted hue,
  // bits 4..1: Background hue. Output is effective output hue.
  UBYTE           *HueMix;
  //
  // Pointer to the previous line buffer, required for PAL artefacting.
  //
  // Pointer to the active colormap comes here.
  const struct ColorEntry *ColorMap;
  //
  // GTIA Settings
  bool ColPF1FiddledArtifacts; // If true, then emulate color artifacts.
  bool NTSC;                   // If true, we are running an NTSC GTIA
  bool isAuto;                 // If true, the video mode comes from the machine.
  LONG PMReaction;             // Number of half-color clocks required as pre-fetch for the Player logic.
  LONG PMResize;               // Ditto for resizing the channel.
  LONG PMShape;                // Ditto for writing the graphics register.
  bool PALColorBlur;           // blur colors of the same hue at adjacent lines
  bool AntiFlicker;            // flicker fixer enabled.
  enum {
    CTIA,                      // original GTIA
    GTIA_1,                    // GTIA as in the 400/800
    GTIA_2                     // GTIA as in the XL series
  }    ChipGeneration;
  //
  // Private methods
  //
  // Load an external color map from the indicated file and install it.
  void LoadColorMapFrom(const char *src);
  //
  // Pick the proper mode generator, depending on the PRIOR and the fiddling
  // values.
  void PickModeGenerator(UBYTE prior);
  //
  // This pre-computes all the data for the priority engine from the
  // hardware registers.
  void UpdatePriorityEngine(UBYTE pri);
  //
  // Setup the artifacting colors of GTIA.
  void SetupArtifacting(void);
  //
  // The following is the main method of the priority engine. It should
  // better be fast as it is called on a per-pixel basis. Arguments are the playfield PreComputedColor,
  // the player bitmask and the playfield color in Atari notation. The latter
  // is required for the special color fiddling.
  UBYTE PixelColor(int pf_pixel,int pm_pixel,int pf_color);
  // This method updates the collision masks of the player/missile collisions
  // for the passed in player/playfield masks
  void UpdateCollisions(int pf,int pl,const UBYTE *collisionmask);
  //
  // Render the missiles into the scanline buffer. The first is the
  // base address into the player scanline, the second the address into
  // the corresponding playfield data.
  // The last argument is the collision mask that defines which bits of
  // the collision bits get set on a collision detection. This mask is indexed
  // by the PreComputed indices above.
  void RenderMissiles(UBYTE *player,UBYTE *playfield,const UBYTE *colmask);
  // Render the players into the scanline buffer. Coordinates are as
  // above.
  void RenderPlayers(UBYTE *player,UBYTE *playfield,const UBYTE *colmask);
  //
  // Reading and writing bytes to GTIA
  virtual UBYTE ComplexRead(ADR mem);
  virtual void  ComplexWrite(ADR mem,UBYTE val);  
  //
  // Various register access functions
  UBYTE ConsoleRead(void);              // read console switches
  UBYTE MissilePFCollisionRead(int n);  // collision between missiles and playfield
  UBYTE MissilePLCollisionRead(int n);  // collision between missiles and players
  UBYTE PlayerPFCollisionRead(int n);   // collision between players and playfield
  UBYTE PlayerPLCollisionRead(int n);   // collision between players
  UBYTE PALFlagRead(void);              // read the PAL/NTSC flag
  UBYTE TrigRead(int n);                // read the trigger of joystick #n
  //
  void ColorBKWrite(UBYTE val);              // write into the background register
  void ColorPlayfieldWrite(int n,UBYTE val); // write into a foreground register
  void ColorPlayerWrite(int n,UBYTE val);    // write into the foreground register
  void GraphicsMissilesWrite(UBYTE val);     // write into the missile graphics register
  void GraphicsPlayerWrite(int n,UBYTE val); // write into a player graphics register
  void HitClearWrite(void);                  // write into the HitClr register
  void MissileHPosWrite(int n,UBYTE val);    // write into horizontal missile pos
  void PlayerHPosWrite(int n,UBYTE val);     // write into horizontal player pos
  void MissileSizeWrite(UBYTE val);          // write into missile sizes
  void PlayerSizeWrite(int n,UBYTE val);     // write into player sizes
  void PriorWrite(UBYTE val);                // write into the priority register
  void GractlWrite(UBYTE val);               // write into the graphics control register
  void VDelayWrite(UBYTE val);               // write into the vertical delay register
  void ConsoleWrite(UBYTE val);              // write into the console register
  //
  // 
  // Trigger a GTIA horizontal blank. This happens at the end of the line, not
  // in the middle.
  virtual void HBI(void);
  //
public:
  // Constructor and destructor
  GTIA(class Machine *mach);  
  ~GTIA(void);
  //  
  // Coldstart and Warmstart GTIA
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //  
  // Read or set the internal status
  virtual void State(class SnapShot *);
  //
  // Service for other modules: Return the active color map.
  const struct ColorEntry *ActiveColorMap(void) const
  {
    return ColorMap;
  }
  //
  // Check whether a true color display would be helpful. Returns
  // true in case it is.
  bool SuggestTrueColor(void) const
  {
    // Current logic is simple. If there is a postprocessor,
    // then truecolor would be nice.
    return (PostProcessor)?true:false;
  }
  //
  // Run a single scanline of the GTIA. This requires the pointer to the
  // display buffer we need to insert the player missile graphics into.
  // Note that this does not run the Pokey scanline.
  // Out is the translated color data. If the input and the output buffer
  // are identically, you *must* ensure that out <= playfield.
  // This feature can be used to implement horizontal scrolling.
  // Player_Offset is the displacement by which the player/missile graphic
  // is displaced relative to the *input* playfield. If this is positive,
  // player/missiles move to the right (relative to the playfield).
  //
  // The maximum pmdisplacement is 128, the maximum width is 512.
  //
  // If fiddling is turned on, then the mentioned display uses color fiddling and
  // requires special handling for collision detection and color creation.
  void TriggerGTIAScanline(UBYTE *playfield,UBYTE *player,int width,bool fiddling);
  //
  // Parse off command line arguments here.
  virtual void ParseArgs(class ArgParser *arg);
  //
  // Print the internal status of pokey for the monitor
  virtual void DisplayStatus(class Monitor *mon);
};
///

///
#endif
