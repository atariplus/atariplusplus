/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: wavdecoder.hpp,v 1.7 2015/10/10 20:57:41 thor Exp $
 **
 ** In this module: The two-tone tape decoder, reads WAV files and 
 ** creates records from them suitable for CAS type reading.
 **********************************************************************************/

#ifndef WAVDECODER_HPP
#define WAVDECODER_HPP

/// Includes
#include "types.hpp"
#include "vbiaction.hpp"
#ifdef HAVE_COS
#include "stdio.hpp"
#include "tapeimage.hpp"
#define HAVE_WAVDECODER
#include <math.h>
///

/// Forwards
class WavFile;
class Machine;
///

/// class WavDecoder
// This class decodes WAV files to tape archives and provides
// record-based IO to the contents of the file - if it is well-formed
// enough. Raw access to the file, especially for "tape speeders"
// is not supported currently.
class WavDecoder : public TapeImage, private VBIAction {
  //
  // A simple fast filter using the Goertzel-Algorithm.
  class GoertzelFFT {
    //
    // The cosine of the frequency, defines the response frequency.
    double m_dCos;
    double m_dSin;
    //
    // 2.0 - cos
    double m_d2MinusCos;
    //
    // The DC leakage compensation of the groetzel filter.
    double m_dLeak;
    //
    // The frequency for which the filter was setup.
    double m_dFreq;
    //
    // The sampling frequency of the wav file.
    double m_dSamplingFreq;
    //
    // Stabilization constant.
    double m_dStabilize;
    //
    //
    // Filter delay lines.
    double m_dSN_2;
    double m_dSN_1;
    //
    // Filter delay lines for a 90 degrees rotated signal.
    double m_dCN_1;
    double m_dCN_2;
    //
    // Setup a filter for the given frequency
    void SetupFilter(double freq);
    //
  public:
    GoertzelFFT(double samplingfreq);
    //
    // Return the currently defined frequency.
    double FrequencyOf(void) const
    {
      return m_dFreq;
    }
    //
    // Define the frequency (or modify)
    void SetFrequency(double freq)
    {
      SetupFilter(m_dSamplingFreq / freq);
    }
    //
    // Drive a value through the filter, record the output, which is
    // the squared of the filter response.
    double Filter(double v);
    //
    // Restart the oscillator with initial conditions of y=0, and y' sufficient
    // to create a +1 amplitude. This is for the synthesis.
    void StartOscillator(bool positive);
    //
    // Generate a sample of the oscillator for synthesis.
    double NextSample(void);
  };
  //
  // This class implements a pair of FFT filters (mark,space) and monitors their
  // quality and measures the ratio of mark/space.
  class FilterPair {
    //
    // One for mark, one for space.
    class GoertzelFFT m_Mark;
    class GoertzelFFT m_Space;
    //
    // The current output bit.
    bool              m_bOut;
    //
    // The hysteresis for the output decision.
    double            m_dHysteresis;
    //
    double            m_dRatio;
    //
    // Average amplitudes of mark and space
    double            m_dMarkAmplitude;
    double            m_dSpaceAmplitude;
    //
    // Correction for source amplitude divergence.
    double            m_dMarkNormalize;
    double            m_dSpaceNormalize;
    //
    // Sampling frequency
    double            m_dFreq;
    //
  public:
    // Setup a filter pair. Samplingfreq is the sampling frequency of the wav file.
    // shift is the frequency bias. For the exact frequencies, specify here 1.0.
    FilterPair(double samplingfreq,double shift);
    //
    // Drive a value through the filter, record the output, which is
    // the squared of the filter response.
    bool Filter(double v);
    //
    // Return the quality of the filter as the ratio of recognized
    // signals.
    double QualityOf(void) const
    {
      return m_dRatio;
    }
    //
    // Return the last output bit again.
    bool OutputOf(void) const
    {
      return m_bOut;
    }
    //
    // Normalize the filter gains.
    void NormalizeFilterGains(void);
    //
    // Generate a mark or space output, true for mark, false for space.
    // The second argument is the number of seconds. The synthesizer
    // tries to stop at a negative to positive transition of the wave and
    // hence may take longer than intended. The overshoot in seconds is
    // returned and must be subtraced from the next bit.
    void WriteBit(class WavFile *out,bool bitvalue,double seconds,double &lag,bool &positive);
    //
    // Write a complete byte, with start and stop bit.
    // Takes the "lag" the previous bit required, and adjusts the output
    // time accordingly. Returns the output lag.
    void WriteByte(class WavFile *out,UBYTE byte,UWORD baudrate,double &lag,bool &positive);
  };
  //
  // A series of mark/space filters at various mildly modified frequencies
  // which automatically selects the best possible filter pair for
  // monitoring the input.
  class FilterCascade {
    //
    // Number of filters in each frequency direction.
    enum {
      m_iNFilters = 12
    };
    //
    class FilterPair *m_ppFilter[2 * m_iNFilters + 1];
    //
    // The currently active filter
    class FilterPair *m_pActiveFilter;
    //
    // The best quality ratio we could find.
    double m_dRatio;
    //
    // The best filter pair we could find.
    int    m_iOptimal;
    //
    // The hysteresis by which we switch filters.
    double m_dHysteresis;
    //
    // The filter switching logic, contains two entries (next,prev) to
    // define the switching direction.
    struct FilterSwitch {
      int m_iNext;
      int m_iPrev;
    }      m_Switch[2 * m_iNFilters + 1];
    //
    //
#if DEBUG_LEVEL > 0
    int ones[2 * m_iNFilters + 1];
#endif
    //
  public:
    FilterCascade(double samplingfreq);
    //
    // Reset all filters and filter states
    void ResetFilters(double samplingfreq);
    //
    ~FilterCascade(void);
    //
    // Drive a value through the filter, record the output, which is
    // the squared of the filter response.
    bool Filter(double v,bool adjustfilters);
    //
    // Drive a value through the filter, record the output, which is
    // the squared of the filter response. Ignore filters that do not
    // have the expected result - if possible.
    bool Filter(double v,bool adjustfilters,bool expected);
    //
    // Return the best quality ratio we could find.
    double QualityOf(void) const;
    //
    // Remove the filters that decode the given bit value incorrectly.
    // Returns false if no filters are left that might be working.
    bool RemoveIncorrectFiltersFor(bool bitvalue);
    //
    // Find the optimal filter pair such that the output is the expected bit value.
    // Returns true if such a filter exists, false otherwise.
    bool FindOptimalFilterFor(bool bitvalue);
    //
    // Normalize the filter gains.
    void NormalizeFilterGains(void);
  };
  //
  // This filter uses the stereo left/right input and from that selects
  // the best possible output
  class ChannelFilter {
    //
    class FilterCascade m_Left;
    class FilterCascade m_Right;
    //
    double m_dHysteresis;
    //
    // The currently active input
    int    m_iActiveInput;
    //
    // The best quality ratio.
    double m_dRatio;
    //
  public:
    ChannelFilter(double samplingfreq);
    //
    // Perform a filtering on the stereo pair (left,right)
    // And adapt the filter given what is best.
    bool Filter(double left,double right,bool adjustfilters);
    //
    // Perform a filtering on the stereo pair (left,right)
    // And adapt the filter given what is best. Also pass in what
    // the expected output should be, and throw out filters that
    // do not give the expected result.
    bool Filter(double left,double right,bool adjustfilters,bool expected);
    //
    // Return a quality estimate of the decoded input.
    double QualityOf(void) const;
    //
    // Find the optimal filter pair such that the output is the expected bit value.
    void FindOptimalFilterFor(bool bitvalue);
    //
    // Remove the filters that decode the given bit value incorrectly
    // and do not consider them in the future.
    void RemoveIncorrectFiltersFor(bool bitvalue);
    //
    // Normalize the filter gains.
    void NormalizeFilterGains(void);
    //
    // Reset the filters for left and right channel to initial filters
    // because a new recording starts.
    void ResetFilters(double samplingfreq);
  };
  //
  // This class performs the higher level serial I/O decoding
  class SerialDecoder {
    //
    // The wavfile the data comes from.
    class WavFile       *m_pSource;
    //
    // The two-tone decoder.
    class ChannelFilter *m_pDemodulator;
    //
    // The source frequency, defines the time base.
    double               m_dFrequency;
    //
    // The baud rate (estimated from the tape sync bytes)
    double               m_dBaudRate;
    //
    // sample offset into the file for debugging purposes.
    ULONG                m_ulSampleOffset;
    //
    // Cycles used to scan an entire byte, excluding parts
    // of the stop bit.
    ULONG                m_ulCyclesPerByte;
    //
    // Checksum so far.
    UBYTE                m_ucChecksum;
    //
    // Read a single bit, return the bit value. The current position must be at the
    // start of the bit, the baud-rate must be initialized correctly.
    //
    // Scan n/16th of a bit, return the value recognized in this sub-bit.
    // Returns in 31th the time the bit was true.
    int ScanBit16th(ULONG &skipped,int n,bool adjustfilters);
    //
    // Scan n/16th of a bit, return the value recognized in this sub-bit.
    // This also provides an information what the bit should be, and
    // then, if adjustfilters is set, adjusts the filters accordingly by
    // removing the filters for the non-fitting bits.
    int ScanBit16th(ULONG &skipped,int n,bool adjustfilters,bool expected);
    //
public:
    SerialDecoder(class WavFile *source,class ChannelFilter *demodulator);
    //
    // Skip half of the audio header, i.e. the initial gap.
    void SkipInitialHeader(double secs,bool adjustfilters);
    //
    // Determine the baud rate by scanning for the initial 0xAA 0xAA sync marker
    // on the tape. Return the size of the IRG in milliseconds
    double FindBaudRate(double secs,bool adjustfilters = true);
    //
    // Decode a single byte. Stream must be in front of the start bit, baudrate must
    // be initialized.
    UBYTE ReadByte(void);
    //
    // Return the estimated baud rate.
    double BaudRateOf(void) const
    {
      return m_dBaudRate;
    }
    //
    // Find the number of remaining seconds on the tape.
    double RemainingTape(void) const;
    //
    //
    // Reset the filters to a working state after failing to detect a block start
    // beyond the EOF.
    void ResetFilters(void);
    //
    // The current sample offset into the file.
    ULONG SampleOffsetOf(void) const
    {
      return m_ulSampleOffset;
    }
  };
  //
  // Update the checksum.
  void UpdateSum(UBYTE b);
  //
  // Advance the WAV decoding logic, and the state machine attached to it. This
  // drives the state machine of the decoder.
  void AdvanceDecoding(void);
  //
  // Implementation of the VIBAction class that also advances the state machine
  // to avoid hogging the emulator core too much.
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  // The main class for warnings.
  class Machine       *m_pMachine;
  //
  // The serial decoder that is used within this class.
  class SerialDecoder *m_pDecoder;
  //
  // The filter for performing the frequency analysis
  class ChannelFilter *m_pFilter;
  //
  // The file we read from or write to.
  FILE                *m_pFile;
  //
  // The WavInterface.
  class WavFile       *m_pWav;
  //
  // The checksum so far.
  UBYTE                m_ucChecksum;
  //
  // The last record type. Determines the size of the
  // IRG we skip.
  UBYTE                m_ucRecordType;
  //
  // The filter pair for synthesis. Only one is needed.
  class FilterPair    *m_pSynthesis;
  //
  // These two are internal state variables for the synthesis.
  double               m_dLag;
  bool                 m_bPositive;
  //
  // Inter-record gap duration collected so far.
  double               m_dIRG;
  //
  // Number of seconds collected so far waiting for the sync bytes
  double               m_dSyncDuration;
  //
  // The baud rate detected.
  double               m_dBaud;
  //
  // Decoder states. This is part of the decoder state machine.
  enum {
    TapeEOF,            // tape run out of data.
    Idle,               // waiting for someone picking up the record buffer
    SkipHeader_Blind,   // skip the initial header, data may be junk, do not adjust filters
    SkipHeader_Adapt,   // skip the header, data should be valid, adjust filters
    FindSync,           // find the sync bytes
    FindRecordType,     // read the record type
    ReadBody,           // read the body of the record
    TestChecksum,       // test the validity of the record
    StartNextRecord,    // advance to the next record.
    SkipOverIRG,        // skip over the IRG of the next record
    SkipOverEOF,        // skip over the EOF record, keep looking for the next frame
    FindNextFile        // keep looking for the next file
  }                    m_DecoderState;     // Decoder state machine state...
  //
  // Record buffer, keeps the record until it is picked up.
  UBYTE                m_ucRecordBuffer[256 + 3 + 1];
  //
  // The pointer into the record buffer.
  UBYTE               *m_pucBufPtr;
  //
  // This flag is set in case non-zero bytes have been found in the EOF record.
  bool                 m_bBadEOF;
  //
public:
  // Cerate a wav decoder from a file.
  WavDecoder(class Machine *mach,FILE *in);
  //
  virtual ~WavDecoder(void);
  //
  // Read from the image into the buffer if supplied.
  // Returns the number of bytes read. Returns also
  // the IRG size in milliseconds.
  virtual UWORD ReadChunk(UBYTE *buffer,LONG buffersize,UWORD &irg);
  //
  // Create a new chunk, write it to the file.
  virtual void WriteChunk(UBYTE *buffer,LONG buffersize,UWORD irg);
  //
  // Open a wav file for reading.
  virtual void OpenForReading(void);
  //
  // After the work is done, complete the file. This is only
  // required for writing.
  virtual void Close(void);
};
///

///
#endif
#endif


    

    
    
    
