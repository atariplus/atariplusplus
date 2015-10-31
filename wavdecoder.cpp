/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: wavdecoder.cpp,v 1.9 2015/11/07 18:53:12 thor Exp $
 **
 ** In this module: The two-tone tape decoder, reads WAV files and 
 ** creates records from them suitable for CAS type reading.
 **********************************************************************************/

/// Includes
#include "wavdecoder.hpp"
#include "new.hpp"
#ifdef HAVE_WAVDECODER
#include "wavfile.hpp"
#include "machine.hpp"
#include "exceptions.hpp"
#define _USE_MATH_DEFINES
#include <math.h>
///

/// WavDecoder::GoertzelFFT::SetupFilter
// Setup a filter for the given frequency
void WavDecoder::GoertzelFFT::SetupFilter(double freq)
{
  m_dCos       = 2.0 * cos(2.0 * M_PI / freq);
  m_dSin       = 2.0 * sin(2.0 * M_PI / freq);
  m_dStabilize = 7.0 / 8.0;
  m_dLeak      = 1.0 / (1 + m_dStabilize - m_dCos * m_dStabilize); // computed
  m_d2MinusCos = 2.0 - m_dCos;
}
///

/// WavDecoder::GoertzelFFT::GoertzelFFT
WavDecoder::GoertzelFFT::GoertzelFFT(double samplingfreq)
  : m_dSamplingFreq(samplingfreq), m_dSN_2(0.0), m_dSN_1(0.0)
{
}
///

/// WavDecoder::GoertzelFFT::Filter
// Drive a value through the filter, record the output, which is
// the squared of the filter response.
double WavDecoder::GoertzelFFT::Filter(double v)
{
  double s_n;
  double y,h;
  
  // Drive the filter
  s_n       = m_dCos * m_dSN_1 - m_dSN_2 + v;
  y         = v * m_dLeak;
  h         = s_n * s_n + m_dSN_1 * m_dSN_1 - m_dCos * s_n * m_dSN_1
    - y * m_d2MinusCos * (s_n + m_dSN_1) + y * y * m_d2MinusCos;
  m_dSN_2   = m_dSN_1;
  m_dSN_1   = m_dStabilize * s_n;
  
  return h;
}
///

/// WavDecoder::GoertzelFFT::StartOscillator
// Restart the oscillator with initial conditions of y=0, and y' sufficient
// to create a +1 amplitude. This is for the synthesis.
void WavDecoder::GoertzelFFT::StartOscillator(bool positive)
{
  m_dSN_1 = 0.0;
  m_dCN_1 = +1.0;
  m_dSN_2 = -0.5 * m_dSin;
  m_dCN_2 = +0.5 * m_dCos;
  if (!positive)
    m_dSN_2 = -m_dSN_2;
}
///

/// WavDecoder::GoertzelFFT::NextSample
// Generate a sample of the oscillator for synthesis.
double WavDecoder::GoertzelFFT::NextSample(void)
{
  double s_n;
  double c_n;
  double amp;
  
  // Drive the filter
  s_n       = m_dCos * m_dSN_1 - m_dSN_2;
  m_dSN_2   = m_dSN_1;
  m_dSN_1   = s_n;
  
  // 90 degree rotated signal.
  c_n       = m_dCos * m_dCN_1 - m_dCN_2;
  m_dCN_2   = m_dCN_1;
  m_dCN_1   = c_n;
  
  amp       = s_n * s_n + c_n * c_n;
  
  // Normalize the filter.
  return s_n / (2.0 * sqrt(amp));
}
///

/// WavDecoder::FilterPair::FilterPair
WavDecoder::FilterPair::FilterPair(double samplingfreq,double shift)
  : m_Mark(samplingfreq), m_Space(samplingfreq), m_bOut(false),
    m_dHysteresis(1.5), m_dRatio(1.0),
    m_dMarkAmplitude(0.0), m_dSpaceAmplitude(0.0),
    m_dMarkNormalize(1.0), m_dSpaceNormalize(1.0),
    m_dFreq(samplingfreq)
{
  m_Mark.SetFrequency(5327.0 * shift);
  m_Space.SetFrequency(3995.0 * shift);
}
///

/// WavDecoder::FilterPair::Filter
// Drive a value through the filter, record the output, which is
// the squared of the filter response.
bool WavDecoder::FilterPair::Filter(double v)
{
  double m,s;
  
  m = m_Mark.Filter(v);
  s = m_Space.Filter(v);
  
  if (m_bOut) {
    // Switch potentially back to space
    if (s * m_dSpaceNormalize > m * m_dHysteresis * m_dMarkNormalize)
      m_bOut = false;
  } else {
    if (m * m_dMarkNormalize  > s * m_dHysteresis * m_dSpaceNormalize)
      m_bOut = true;
  }
  
  if (m_bOut) {
    m_dRatio          = (31.0 * m_dRatio          + m) / 32.0;
    m_dMarkAmplitude  = (31.0 * m_dMarkAmplitude  + m) / 32.0;
  } else {
    m_dRatio          = (31.0 * m_dRatio          + s) / 32.0;
    m_dSpaceAmplitude = (31.0 * m_dSpaceAmplitude + s) / 32.0;
  }
  
  return m_bOut;
}
///

/// WavDecoder::FilterPair::NormalizeFilterGains
// Normalize the filter gains.
void WavDecoder::FilterPair::NormalizeFilterGains(void)
{
  if (m_dMarkAmplitude > 0.0)
    m_dMarkNormalize  = 1.0 / sqrt(m_dMarkAmplitude);
  if (m_dSpaceAmplitude > 0.0)
    m_dSpaceNormalize = 1.0 / sqrt(m_dSpaceAmplitude);
}
///

/// WavDecoder::FilterPair::WriteBit
// Generate a mark or space output, true for mark, false for space.
// The second argument is the number of seconds. The synthesizer
// tries to stop at a negative to positive transition of the wave and
// hence may take longer than intended. The overshoot in seconds is
// returned and must be subtraced from the next bit.
void WavDecoder::FilterPair::WriteBit(class WavFile *out,bool bitvalue,double seconds,double &lag,bool &positive)
{
  ULONG i,samples        = ULONG((seconds - lag) * m_dFreq);
  class GoertzelFFT &osc = (bitvalue)?(m_Mark):(m_Space);
  double v = 0.0;
  
  if (seconds <= lag)
    Throw(InvalidParameter,"WavDecoder::FilterPair::WriteBit",
	  "sampling frequency is too low, accumulated error is too large");
  
  osc.StartOscillator(positive);
  for(i = 0;i < samples;i++) {
    v = osc.NextSample();
    out->WriteSample(v);
  }
  //
  // Now wait for the transition.
  if (v > 0.0) {
    while ((v = osc.NextSample()) > 0.0) {
      out->WriteSample(v);
      samples++;
    }
    positive = false;
  } else {
    while ((v = osc.NextSample()) < 0.0) {
      out->WriteSample(v);
      samples++;
    }
    positive = true;
  }

  lag += samples / m_dFreq - seconds;
}
///

/// WavDecoder::FilterPair::WriteByte
// Write a complete byte, with start and stop bit.
// Takes the "lag" the previous bit required, and adjusts the output
// time accordingly. Returns the output lag.
void WavDecoder::FilterPair::WriteByte(class WavFile *out,UBYTE byte,UWORD baudrate,double &lag,bool &positive)
{
  UBYTE bitmask = 0x01;
  // Compute the duration of a single bit.
  double bitsize = 1.0 / baudrate;

  //
  // Write the start bit.
  WriteBit(out,false,bitsize,lag,positive);
  //
  // Write the bits, start with the LSB.
  while(bitmask) {
    WriteBit(out,(byte & bitmask)?true:false,bitsize,lag,positive);
    bitmask <<= 1;
  }
  //
  // Write the stop bit.
  WriteBit(out,true,bitsize,lag,positive);
}
///

/// WavDecoder::FilterCascade::FilterCascade
WavDecoder::FilterCascade::FilterCascade(double samplingfreq)
  : m_pActiveFilter(NULL), m_dRatio(0.0),
    m_iOptimal(0), m_dHysteresis(1.2)
{
  int i;
  double lshift = 1.0;
  double hshift = 1.0;
  
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    m_ppFilter[i] = NULL;
  }
  
#if DEBUG_LEVEL > 0
  memset(ones,0,sizeof(ones));
#endif
  
  try {
    m_ppFilter[0] = new class FilterPair(samplingfreq,1.0);
    m_Switch[0].m_iNext = 1; // lower frequency
    m_Switch[0].m_iPrev = 2; // higher frequency
    for(i = 1;i <= m_iNFilters;i++) {
      lshift /= 1.03;
      hshift *= 1.03;
      m_ppFilter[2 * i - 1] = new class FilterPair(samplingfreq,lshift);
      m_Switch  [2 * i - 1].m_iNext = ((i + 1) <= m_iNFilters)?(2 * i + 1):(-1); // lower frequency
      m_Switch  [2 * i - 1].m_iPrev = ((i - 1) > 1)           ?(2 * i - 3):(0);  // higher frequency
      m_ppFilter[2 * i - 0] = new class FilterPair(samplingfreq,hshift);
      m_Switch  [2 * i - 0].m_iNext = ((i - 1) > 1)           ?(2 * i - 2):(0);  // lower frequency
      m_Switch  [2 * i - 0].m_iPrev = ((i + 1) <= m_iNFilters)?(2 * i + 2):(-1); // higher frequency
    }
  } catch(...) {
    for(i = 0;i < m_iNFilters * 2 + 1;i++) {
      delete m_ppFilter[i];
      m_ppFilter[i] = NULL;
    }
    throw;
  }
}
///

/// WavDecoder::FilterCascade::ResetFilters
// Reset all filters and filter states
void WavDecoder::FilterCascade::ResetFilters(double samplingfreq)
{
  int i;
  double lshift = 1.0;
  double hshift = 1.0;
  
#if DEBUG_LEVEL > 0
  memset(ones,0,sizeof(ones));
#endif
  
  if (m_ppFilter[0] == NULL)
    m_ppFilter[0] = new class FilterPair(samplingfreq,1.0);
  
  for(i = 1;i <= m_iNFilters;i++) {
    lshift /= 1.03;
    hshift *= 1.03;
    if (m_ppFilter[2 * i - 1] == NULL)
      m_ppFilter[2 * i - 1] = new class FilterPair(samplingfreq,lshift);
    if (m_ppFilter[2 * i - 0] == NULL)
      m_ppFilter[2 * i - 0] = new class FilterPair(samplingfreq,hshift);
  }
}
///

/// WavDecoder::FilterCascade::~WavDecoder
WavDecoder::FilterCascade::~FilterCascade(void)
{
  int i;
  
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    delete m_ppFilter[i];
    m_ppFilter[i] = NULL;
  }
}
///

/// WavDecoder::FilterCascade::Filter
// Drive a value through the filter, record the output, which is
// the squared of the filter response.
bool WavDecoder::FilterCascade::Filter(double v,bool adjustfilters)
{
  int i;
  
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    if (m_ppFilter[i])
      m_ppFilter[i]->Filter(v);
  }
  
#if DEBUG
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    if (m_ppFilter[i]) {
      if (m_ppFilter[i]->OutputOf())
	ones[i]++;
    }
  }
#endif
  
  if (m_ppFilter[m_iOptimal]) {
    if (adjustfilters) {
      if (m_Switch[m_iOptimal].m_iNext >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iNext] &&
	  m_ppFilter[m_Switch[m_iOptimal].m_iNext]->QualityOf() > m_ppFilter[m_iOptimal]->QualityOf() * m_dHysteresis) {
	m_iOptimal = m_Switch[m_iOptimal].m_iNext;
      }
      if (m_Switch[m_iOptimal].m_iPrev >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iPrev] &&
	  m_ppFilter[m_Switch[m_iOptimal].m_iPrev]->QualityOf() > m_ppFilter[m_iOptimal]->QualityOf() * m_dHysteresis) {
	m_iOptimal = m_Switch[m_iOptimal].m_iPrev;
      }
      m_dRatio = m_ppFilter[m_iOptimal]->QualityOf();
    }
    
    return m_ppFilter[m_iOptimal]->OutputOf();
  } else {
    // Always return mark, i.e. no start bit.
    return true;
  }
}
///

/// WavDecoder::FilterCascade::Filter
// Drive a value through the filter, record the output, which is
// the squared of the filter response. Ignore filters that do not
// have the expected result - if possible.
bool WavDecoder::FilterCascade::Filter(double v,bool adjustfilters,bool expected)
{
  int i;
  
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    if (m_ppFilter[i])
      m_ppFilter[i]->Filter(v);
  }
  
#if DEBUG_LEVEL > 0
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    if (m_ppFilter[i]) {
      if (m_ppFilter[i]->OutputOf())
	ones[i]++;
    }
  }
#endif 
  
  if (m_ppFilter[m_iOptimal]) {
    if (adjustfilters) {
      if (m_Switch[m_iOptimal].m_iNext >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iNext] &&
	  m_ppFilter[m_Switch[m_iOptimal].m_iNext]->OutputOf() == expected &&
	  m_ppFilter[m_Switch[m_iOptimal].m_iNext]->QualityOf() > m_ppFilter[m_iOptimal]->QualityOf() * m_dHysteresis) {
	m_iOptimal = m_Switch[m_iOptimal].m_iNext;
      }
      if (m_Switch[m_iOptimal].m_iPrev >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iPrev] &&
	  m_ppFilter[m_Switch[m_iOptimal].m_iPrev]->OutputOf() == expected &&
	  m_ppFilter[m_Switch[m_iOptimal].m_iPrev]->QualityOf() > m_ppFilter[m_iOptimal]->QualityOf() * m_dHysteresis) {
	m_iOptimal = m_Switch[m_iOptimal].m_iPrev;
      }
      m_dRatio = m_ppFilter[m_iOptimal]->QualityOf();
    }
    
    if (m_ppFilter[m_iOptimal]->OutputOf() == expected) {
      return expected;
    } else if (m_Switch[m_iOptimal].m_iNext >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iNext] &&
	       m_ppFilter[m_Switch[m_iOptimal].m_iNext]->OutputOf() == expected) {
      if (adjustfilters) {
	m_iOptimal = m_Switch[m_iOptimal].m_iNext;
	m_dRatio   = m_ppFilter[m_iOptimal]->QualityOf();
      }
      return expected;
    } else if (m_Switch[m_iOptimal].m_iPrev >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iPrev] &&
	       m_ppFilter[m_Switch[m_iOptimal].m_iPrev]->OutputOf() == expected) {
      if (adjustfilters) {
	m_iOptimal = m_Switch[m_iOptimal].m_iPrev;	
	m_dRatio   = m_ppFilter[m_iOptimal]->QualityOf();
      }
      return expected;
    }
    
    return m_ppFilter[m_iOptimal]->OutputOf();
  } else {
      // Always return mark, i.e. the idle line.
    return true;
  }
}
///

/// WavDecoder::FilterCascade::QualityOf
// Return the best quality ratio we could find.
double WavDecoder::FilterCascade::QualityOf(void) const
{
  if (m_ppFilter[m_iOptimal] == NULL)
    return 0.0; // dead channel
  
  return m_dRatio;
}
///

/// WavDecoder::FilterCascade::RemoveIncorrectFiltersFor
// Remove the filters that decode the given bit value incorrectly.
// Returns false if no filters are left that might be working.
bool WavDecoder::FilterCascade::RemoveIncorrectFiltersFor(bool bitvalue)
{
  int i;
  
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    if (m_ppFilter[i]) {
      if (m_ppFilter[i]->OutputOf() != bitvalue) {
	delete m_ppFilter[i];
	m_ppFilter[i] = NULL;
      }
    }
  }
  
  if (m_ppFilter[m_iOptimal] == NULL) {
    if (m_Switch[m_iOptimal].m_iNext >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iNext]) {
      m_iOptimal = m_Switch[m_iOptimal].m_iNext;
    } else if (m_Switch[m_iOptimal].m_iPrev >= 0 && m_ppFilter[m_Switch[m_iOptimal].m_iPrev]) {
      m_iOptimal = m_Switch[m_iOptimal].m_iPrev;
    } else {
      return false;
    }
  }
  if (m_ppFilter[m_iOptimal] == NULL) {
    return false;
  }
  return true;
}
///

/// WavDecoder::FilterCascade::FindOptimalFilterFor
// Find the optimal filter pair such that the output is the expected bit value.
// Returns true if such a filter exists, false otherwise.
bool WavDecoder::FilterCascade::FindOptimalFilterFor(bool bitvalue)
{
  int i;
  double bestratio = 0.0;
  
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    if (m_ppFilter[i]) {
      if (m_ppFilter[i]->OutputOf() == bitvalue) {
	if (m_ppFilter[i]->QualityOf() > bestratio) {
	  bestratio  = m_ppFilter[m_iOptimal]->QualityOf();
	  m_iOptimal = i;
	}
      }
    }
  }
  
  if (m_ppFilter[m_iOptimal] == NULL)
    return false;
      
  m_dRatio = m_ppFilter[m_iOptimal]->QualityOf();
  return true;
}
///

/// WavDecoder::FilterCascade::NormalizeFilterGains
// Normalize the filter gains.
void WavDecoder::FilterCascade::NormalizeFilterGains(void)
{
  int i;
  
  for(i = 0;i < m_iNFilters * 2 + 1;i++) {
    if (m_ppFilter[i]) {
      m_ppFilter[i]->NormalizeFilterGains();
    }
  }
}
///

/// WavDecoder::ChannelFilter::ChannelFilter
WavDecoder::ChannelFilter::ChannelFilter(double samplingfreq)
  : m_Left(samplingfreq), m_Right(samplingfreq), m_dHysteresis(2.0),
    m_iActiveInput(0), m_dRatio(1.0)
{
}
///

/// WavDecoder::ChannelFilter::Filter
// Perform a filtering on the stereo pair (left,right)
// And adapt the filter given what is best.
bool WavDecoder::ChannelFilter::Filter(double left,double right,bool adjustfilters)
{
  bool left_out,right_out;
  
  left_out  = m_Left.Filter(left,adjustfilters);
  right_out = m_Right.Filter(right,adjustfilters);
  
  if (adjustfilters) {
    if (m_iActiveInput == 0) {
      if (m_Right.QualityOf() > m_Left.QualityOf() * m_dHysteresis) {
	m_iActiveInput = 1;
      }
    } else {
      if (m_Left.QualityOf() > m_Right.QualityOf() * m_dHysteresis) {
	m_iActiveInput = 0;
      }
    }
  }
  
  if (m_iActiveInput) {
    return right_out;
  } else {
    return left_out;
  }
}
///

/// WavDecoder::ChannelFilter::Filter
// And adapt the filter given what is best. Also pass in what
// the expected output should be, and throw out filters that
// do not give the expected result.
bool WavDecoder::ChannelFilter::Filter(double left,double right,bool adjustfilters,bool expected)
{
  bool left_out,right_out;
  
  left_out  = m_Left.Filter(left,adjustfilters,expected);
  right_out = m_Right.Filter(right,adjustfilters,expected);
  
  if (adjustfilters) {
    if (m_iActiveInput == 0) {
      if (m_Right.QualityOf() > m_Left.QualityOf() * m_dHysteresis) {
	m_iActiveInput = 1;
      }
    } else {
      if (m_Left.QualityOf() > m_Right.QualityOf() * m_dHysteresis) {
	m_iActiveInput = 0;
      }
    }
  }
  
  if (m_iActiveInput) {
    return right_out;
  } else {
    return left_out;
  }
}
///

/// WavDecoder::ChannelFilter::QualityOf
// Return a quality estimate of the decoded input.
double WavDecoder::ChannelFilter::QualityOf(void) const
{
  if (m_iActiveInput) {
    return m_Right.QualityOf();
  } else {
    return m_Left.QualityOf();
  }
}
///

/// WavDecoder::ChannelFilter::FindOptimalFilterFor
// Find the optimal filter pair such that the output is the expected bit value.
void WavDecoder::ChannelFilter::FindOptimalFilterFor(bool bitvalue)
{
  bool worked = false;
  
  if (m_Left.FindOptimalFilterFor(bitvalue))
    worked = true;
  
  if (m_Right.FindOptimalFilterFor(bitvalue))
    worked = true;
  
  if (!worked)
    Throw(InvalidParameter,"WavDecoder::ChannelFilter::FindOptimalFilterFor",
	  "input signal is too distorted, cannot decode");
}
///

/// WavDecoder::ChannelFilter::RemoveIncorrectFiltersFor
// Remove the filters that decode the given bit value incorrectly
// and do not consider them in the future.
void WavDecoder::ChannelFilter::RemoveIncorrectFiltersFor(bool bitvalue)
{
  bool worked = false;
  
  if (m_Left.RemoveIncorrectFiltersFor(bitvalue))
    worked = true;
  if (m_Right.RemoveIncorrectFiltersFor(bitvalue))
    worked = true;
  
  if (!worked)
    Throw(InvalidParameter,"WavDecoder::ChannelFilter::RemoveIncorrectFiltersFor",
	  "input signal is too distorted, cannot decode");
}
///

/// WavDecoder::ChannelFilter::NormalizeFilterGains
// Normalize the filter gains.
void WavDecoder::ChannelFilter::NormalizeFilterGains(void)
{
  m_Left.NormalizeFilterGains();
  m_Right.NormalizeFilterGains();
}
///

/// WavDecoder::ChannelFilter::ResetFilters
// Reset the filters for left and right channel to initial filters
// because a new recording starts.
void WavDecoder::ChannelFilter::ResetFilters(double samplingfreq)
{
  m_Left.ResetFilters(samplingfreq);
  m_Right.ResetFilters(samplingfreq);
}
///

/// WavDecoder::SerialDecoder::SerialDecoder
// Create a serial decoder class.
WavDecoder::SerialDecoder::SerialDecoder(class WavFile *source,class ChannelFilter *demodulator)
  : m_pSource(source), m_pDemodulator(demodulator), m_ulSampleOffset(0)
{
  m_dFrequency      = source->FrequencyOf();
  m_dBaudRate       = 600.0;
  m_ulCyclesPerByte = 0;
}
///

/// WavDecoder::SerialDecoder::SkipInitialHeader
// Skip half of the audio header, i.e. the initial gap.
void WavDecoder::SerialDecoder::SkipInitialHeader(double secs,bool adjustfilters)
{
  ULONG samples;
  //
  // Similar to the original Os implementation, wait 9.6 seconds into the header.
  samples = ULONG(m_dFrequency * secs);
  //
  while(samples) {
    m_pDemodulator->Filter(m_pSource->Normalize(m_pSource->LeftSample()),
			   m_pSource->Normalize(m_pSource->RightSample()),
			   adjustfilters);
    m_ulSampleOffset++;
    if (!m_pSource->Advance())
      Throw(OutOfRange,"WavDecoder::SerialDecoder::SkipInitialHeader",
	    "unexpected end of tape while scanning the initial gap header");
    samples--;
  }
}
///

/// WavDecoder::SerialDecoder::ScanBit16th
// Scan n/16th of a bit, return the value recognized in this sub-bit.
// Returns in 31th the time the bit was true.
int WavDecoder::SerialDecoder::ScanBit16th(ULONG &skipped,int n,bool adjustfilter)
{
  int bit16th = int((n * m_dFrequency / m_dBaudRate) / 16.0);
  int samples = bit16th;
  int avg     = 0;
  bool bit;
  
  if (samples < 2)
    Throw(OutOfRange,"WavDecoder::SerialDecoder::ScanBit16th",
	  "sampling frequency is too low, cannot gain enough statistics to collect information on a sub-bit.");
  
  while(samples) {
    bit =  m_pDemodulator->Filter(m_pSource->Normalize(m_pSource->LeftSample()),
				  m_pSource->Normalize(m_pSource->RightSample()),
				  adjustfilter);
    m_ulSampleOffset++;
    if (!m_pSource->Advance() && samples > (bit16th >> 1))
      Throw(OutOfRange,"WavDecoder::SerialDecoder::ScanBit16th",
	    "unexpected end of tape while scanning for bits");
    //
    if (bit)
      avg++;
    samples--;
  }
  
  skipped = bit16th;
  
  return avg * 31 / bit16th;
}
///

/// WavDecoder::SerialDecoder::ScanBit16th
// Scan n/16th of a bit, return the value recognized in this sub-bit.
// This also provides an information what the bit should be, and
// then, if adjustfilters is set, adjusts the filters accordingly by
// removing the filters for the non-fitting bits.
int WavDecoder::SerialDecoder::ScanBit16th(ULONG &skipped,int n,bool adjustfilters,bool expected)
{ 
  int bit16th = int((n * m_dFrequency / m_dBaudRate) / 16.0);
  int samples = bit16th;
  int avg     = 0;
  bool bit;
  
  if (samples < 2)
    Throw(OutOfRange,"WavDecoder::SerialDecoder::ScanBit16th",
	  "sampling frequency is too low, cannot gain enough statistics to collect information on a sub-bit.");
  
  while(samples) {
    double left  = m_pSource->Normalize(m_pSource->LeftSample());
    double right = m_pSource->Normalize(m_pSource->RightSample());
    m_ulSampleOffset++;
    if (!m_pSource->Advance() && samples > (bit16th >> 1))
      Throw(OutOfRange,"WavDecoder::SerialDecoder::ScanBit16th",
	    "unexpected end of tape while scanning for bits");
    //
    bit =  m_pDemodulator->Filter(left,right,adjustfilters,expected);
    //
    if (bit)
      avg++;
    samples--;
  }
  
  skipped = bit16th;
  
  return avg * 31 / bit16th;
}
///

/// WavDecoder::SerialDecoder::FindBaudRate
// Determine the baud rate by scanning for the initial 0xAA 0xAA sync marker
// on the tape. Returns the size of the IRG, or zero if no sync bit has
// been found in the given time.
double WavDecoder::SerialDecoder::FindBaudRate(double secs,bool adjustfilters)
{
  ULONG hdrsamples = ULONG(m_dFrequency * secs); // maximum number of samples we use for scanning for the scan header
  ULONG samples;
  ULONG maxwait;
  ULONG bit16th    = ULONG((m_dFrequency / m_dBaudRate) / 16.0);
  ULONG bit16th3   = ULONG((3  * m_dFrequency / m_dBaudRate) / 16.0);
  ULONG bit16th12  = ULONG((12 * m_dFrequency / m_dBaudRate) / 16.0);
  ULONG irg     = 0;
  bool bit = true;
  bool bit2;
  bool bitnew;
  int transitions = 0;

  do {
    do {
      while(hdrsamples) {
	bit =  m_pDemodulator->Filter(m_pSource->Normalize(m_pSource->LeftSample()),
				      m_pSource->Normalize(m_pSource->RightSample()),
				      adjustfilters);
	m_ulSampleOffset++;
	if (!m_pSource->Advance())
	  Throw(OutOfRange,"WavDecoder::SerialDecoder::FindBaudRate",
		"unexpected end of tape while scanning for the initial sync header");
	//
	// Found the start bit?
	if (bit == false)
	  break;
	hdrsamples--;
	irg++;
      }

      // Has the start bit been found? If not, return zero.
      if (bit)
	return -double(irg) * 1000.0 / m_dFrequency;
      
      // Estimate the baud rate to 600 baud. Will be fixed soon.
      m_dBaudRate = 600.0;
      samples     = 0;
      transitions = 0;
      maxwait     = ULONG(40 * m_dFrequency / m_dBaudRate); // wait at most 40 bits. 
      //
      // Must remain low for half a bit.
      bit2        = ScanBit16th(bit16th,12,adjustfilters)<10?false:true;
      samples    += bit16th12;
      maxwait    -= bit16th12;
      if (bit2) // If we need to repeat, this drop-out will become part of the IRG.
	irg      += bit16th12;
    } while(bit2);
    //
    //
    do {
      //
      // Now scan for the bit transition
      do {
	bitnew = m_pDemodulator->Filter(m_pSource->Normalize(m_pSource->LeftSample()),
					m_pSource->Normalize(m_pSource->RightSample()),
					adjustfilters);
	m_ulSampleOffset++;
	samples++;
	maxwait--;
	if (!m_pSource->Advance())
	  Throw(OutOfRange,"WavDecoder::SerialDecoder::FindBaudRate",
		"unexpected end of tape while scanning the initial sync header");
	//
	if (bitnew != bit) {
	  // Found a bit transition
	  bit = bitnew;
	  transitions++;
	  break;
	}
      } while(maxwait); 
      //
      if (maxwait < bit16th * 8) {
	maxwait = 0;
	break;
      }
      //
      // Found a bit transition.
      if (transitions > 2) {
	// Here already assume that we know the result.
	bit2     = ScanBit16th(bit16th,3,adjustfilters,bitnew)>15?true:false;
	samples += bit16th3;
	maxwait -= bit16th3;
      } else if (transitions > 1) {
	bit2     = ScanBit16th(bit16th,3,adjustfilters)>15?true:false;
	samples += bit16th3;
	maxwait -= bit16th3;
      } else {
	bit2     = ScanBit16th(bit16th,12,adjustfilters)>15?true:false;
	samples += bit16th12;
	maxwait -= bit16th12;
      }
      //
      if (bit2 != bitnew) {
	// False transition detected.
	if (transitions <= 2) {
	  // Early enough. Try again to detect the sync marker.
	  maxwait = 0;
	  break;
	} else {
	  Throw(InvalidParameter,"WavDecoder::SerialDecoder::FindBaudRate",
		"initial sync header is too noisy, found unexpected bit transition in the sync marker");
	}
      }
      //
      if (transitions > 1 && adjustfilters) {
	m_pDemodulator->NormalizeFilterGains();
	m_pDemodulator->RemoveIncorrectFiltersFor(bit);
      }
      //
      // In total 20 transitions (one of the initial), making up for two bytes, plus start and stop bits
      // i.e. (8+1+1)*2
      if (transitions == 19)
	break;
    } while(maxwait);
    // Need to restart again?
  } while(maxwait == 0);

  // Should now be in the stop bit of the last byte.
  assert(bit);
  assert(samples > 0);
  //
  // Number of samples skipped from the first transition to the final stop bit is in samples is 19 times the
  // baud rate.
  m_dBaudRate       = 19.0 * m_dFrequency / samples;
  m_ulCyclesPerByte = 0;

  return irg * 1000.0 / m_dFrequency;
}
///

/// WavDecoder::SerialDecoder::ReadByte
// Decode a single byte. Stream must be in front of the start bit, baudrate must
// be initialized.
UBYTE WavDecoder::SerialDecoder::ReadByte(void)
{
  ULONG maxwait      = ULONG(20 * m_dFrequency / m_dBaudRate); // wait at most 20 bits for the start bit.
  double bitduration = m_dFrequency / m_dBaudRate; // size of a bit in samples
  ULONG samples      = 0;
  ULONG bit16th;
  bool bit;
  UWORD byteout = 0;
  UWORD bitmask = 0x1;
  int bitcount  = 0;
  
  // 
  // Wait for the start bit.
  do {
    bit = m_pDemodulator->Filter(m_pSource->Normalize(m_pSource->LeftSample()),
				 m_pSource->Normalize(m_pSource->RightSample()),
				 true);
    m_ulSampleOffset++;
    maxwait--;
    samples++;
    if (!m_pSource->Advance())
      Throw(InvalidParameter,"WavDecoder::SerialDecoder::ReadByte",
	    "unexpected end of tape while scanning for the initial sync header");
    if (!bit)
      break; // we got it.
  } while(maxwait);
  
  if (maxwait == 0)
    Throw(InvalidParameter,"WavDecoder::SerialDecoder::ReadByte",
	  "serial stream mangled, unable to detect the start bit");
  
  //
  // Here we detected the start-byte. Compute a new baud rate from
  // the total time it took to scan the last byte.
  if (m_ulCyclesPerByte) {
    // Compute the total number of cycles the last byte required, start of last start bit
    // to start of this start bit.
    samples += m_ulCyclesPerByte;
    // This makes in total 10 bits.
    if (samples > 0) {
      m_dBaudRate = 0.5 * (m_dBaudRate + 10.0 * m_dFrequency / samples);
      bitduration = m_dFrequency / m_dBaudRate;
    }
  }

  samples  = 0;
  bitcount = 0;
  while(bitmask <= 0x200) {
    // Wait 5/16 of the time, skip the initial part of the bit
    ScanBit16th(bit16th,5,true);
    samples += bit16th;
    //
    // sample for 7/16th of the interior of the bit.
    if (bitmask == 0x01) {
      // This must be the start-bit
      bit      = ScanBit16th(bit16th,7,true,false)>15?true:false;
    } else if (bitmask == 0x200) {
      // This must be the stop-bit
      bit      = ScanBit16th(bit16th,5,true,true)>15?true:false;
    } else {
      bit      = ScanBit16th(bit16th,7,true)>15?true:false;
    }
    samples += bit16th;
    //
    // The bit value is the average.
    if (bit)
      byteout |= bitmask;
    bitmask <<= 1;
    bitcount++;
    //
    // Wait the remaining samples without doing anything with the data - except for the stop bit.
    // Leave instead parts of the stop bit in the stream to allow synchronization with the next
    // start bit.
    if (bitmask != 0x400) {
      while(samples < bitduration * bitcount) {
	m_pDemodulator->Filter(m_pSource->Normalize(m_pSource->LeftSample()),
			       m_pSource->Normalize(m_pSource->RightSample()),
			       true);
	m_ulSampleOffset++;
	samples++;
	if (!m_pSource->Advance())
	  Throw(InvalidParameter,"WavDecoder::SerialDecoder::ReadByte",
		"unexpected end of tape while scanning the stream");
      }
    }
  }
  //
  m_ulCyclesPerByte = samples;
  //
  // Check whether the stop bit is ok.
  if ((byteout & 0x201) != 0x200)
    Throw(InvalidParameter,"WavDecoder::SerialDecoder::ReadByte",
	  "serial framing error - stop bit or start bit not received");

  return UBYTE(byteout >> 1);
}
///

/// WavDecoder::SerialDecoder::RemainingTape
// Find the number of remaining seconds on the tape.
double WavDecoder::SerialDecoder::RemainingTape(void) const
{
  return m_pSource->RemainingSamples() / double(m_pSource->FrequencyOf());
}
///
//

/// WavDecoder::SerialDecoder::ResetFilters
// Reset the filters to a working state after failing to detect a block start
// beyond the EOF.
void WavDecoder::SerialDecoder::ResetFilters(void)
{
  m_pDemodulator->ResetFilters(m_pSource->FrequencyOf());
}
///

/// WavDecoder::WavDecoder
// Cerate a wav decoder from a file.
WavDecoder::WavDecoder(class Machine *mach,FILE *in)
  : VBIAction(mach), m_pMachine(mach), m_pDecoder(NULL), m_pFilter(NULL), m_pFile(in), m_pWav(NULL),
    m_ucChecksum(0), m_ucRecordType(0), m_pSynthesis(NULL), m_dLag(0.0), m_bPositive(true),
    m_dIRG(0.0), m_dSyncDuration(0.0), m_DecoderState(TapeEOF), m_pucBufPtr(NULL), m_bBadEOF(false)
{
}
///

/// WavDecoder::~WavDecoder
WavDecoder::~WavDecoder(void)
{
  delete m_pSynthesis;
  delete m_pDecoder;
  delete m_pFilter;
  delete m_pWav;
}
///

/// WavDecoder::UpdateSum
// Update the checksum.
void WavDecoder::UpdateSum(UBYTE b)
{
  int sum = int(m_ucChecksum) + b;
  
  if (sum < 256) {
    m_ucChecksum = sum;
  } else {
    m_ucChecksum = sum + 1;
  }
}
///

/// WavDecoder::OpenForReading
// Open a wav file for reading.
void WavDecoder::OpenForReading(void)
{
  assert(m_pWav == NULL);
  assert(m_pFilter == NULL);
  assert(m_pDecoder == NULL);
  assert(m_pFile);
  //
  // Open the WAV file.
  m_pWav = new WavFile(m_pFile);
  m_pWav->ParseHeader();

  // Create the frequency analysing filter.
  m_pFilter = new ChannelFilter(m_pWav->FrequencyOf());

  //
  // The decoder is not yet there. So create it now.
  m_pDecoder = new SerialDecoder(m_pWav,m_pFilter);
  //
  // No IRG found so far.
  m_dIRG         = 0.0;
  //
  // Record type not yet read.
  m_ucRecordType = 0;
  //
  // Start decoding by looking at the header.
  m_DecoderState = SkipHeader_Blind;
  //
  // Record type not yet read.
  m_ucRecordType = 0;
}
///

/// WavDecdoder::VBI
// Implementation of the VIBAction class that also advances the state machine
// to avoid hogging the emulator core too much.
void WavDecoder::VBI(class Timer *,bool quick,bool pause)
{
  if (!quick && !pause && m_pDecoder && m_pWav && m_pFilter) {
    AdvanceDecoding();
  }
}
///

/// WavDecoder::AdvanceDecoding
// Advance the WAV decoding logic, and the state machine attached to it.
void WavDecoder::AdvanceDecoding(void)
{
  switch(m_DecoderState) {
  case SkipHeader_Blind:
    // Skip the initial header, without updating the filters because we do not
    // really know what's on the tape here.
    m_pDecoder->SkipInitialHeader(0.02,false);
    m_dIRG += 20.0;
    if (m_dIRG >= 2000.0)
      m_DecoderState = SkipHeader_Adapt;
    break;
  case SkipHeader_Adapt:
    // Continue to the skip the header, but start adapting the filter state
    // so we can find the ideal filter pair.
    m_pDecoder->SkipInitialHeader(0.02,false);
    m_dIRG += 20.0;
    if (m_dIRG >= 7000.0) {
      m_dSyncDuration = 0.0;
      m_DecoderState  = FindSync;
      //
      // The filter should now read a "mark" value.
      m_pFilter->FindOptimalFilterFor(true);
    }
    break;
  case FindSync:
    // Continue checking the header, go for finding the sync bytes.
    {
      double sirg = m_pDecoder->FindBaudRate(0.02);
      //
      // If a sync has been found, add up the total sync duration
      // and advance the state machine.
      if (sirg > 0.0) {
	m_dIRG        += sirg + m_dSyncDuration;
	m_dBaud        = m_pDecoder->BaudRateOf();
	m_DecoderState = FindRecordType;
      } else {
	// Advance the sync duration. If nothing could be found within 24.2 seconds, fail.
	m_dSyncDuration += -sirg;
	if (m_dSyncDuration >= 24200.0) {
	  Throw(InvalidParameter,"WavDecoder::AdvanceDecoding",
		"unable to find the sync marker at the start of a gap.");
	}
      }
    }
    break;
  case FindRecordType:
    // Find the record type now. Reset the checksum.
    // Include the sync types
    m_ucChecksum   = 0;
    m_bBadEOF      = false;
    m_pucBufPtr    = m_ucRecordBuffer;
    *m_pucBufPtr++ = 0x55;
    *m_pucBufPtr++ = 0x55;
    UpdateSum(0x55);
    UpdateSum(0x55);
    m_ucRecordType = m_pDecoder->ReadByte();
    *m_pucBufPtr++ = m_ucRecordType;
    UpdateSum(m_ucRecordType);
    if (m_ucRecordType != 0xfc && m_ucRecordType != 0xfa && m_ucRecordType != 0xfe)
      m_pMachine->PutWarning("Found invalid record type 0x%02x when decoding a tape file",m_ucRecordType);
    m_DecoderState = ReadBody;
    break;
  case ReadBody:
    // Read the next 12 bytes into the buffer, or until we hit the end of the record.
    // This is an approximate number of bytes that are received per VBI.
    {
      UBYTE byte;
      UBYTE *endbuffer = m_ucRecordBuffer + 2 + 1 + 128;
      if (endbuffer > m_pucBufPtr + 12) {
	endbuffer = m_pucBufPtr + 12;
      } else {
	m_DecoderState = TestChecksum;
      }
      //
      do {
	byte = m_pDecoder->ReadByte();
	if (m_ucRecordType == 0xfe && byte != 0x00) {
	  m_bBadEOF = true;
	}
	UpdateSum(byte);
	*m_pucBufPtr++ = byte;
      } while(m_pucBufPtr < endbuffer);
    }
    break;
  case TestChecksum:
    // Here test the checksum for validity.
    *m_pucBufPtr++ = m_pDecoder->ReadByte();
    if (m_pucBufPtr[-1] != m_ucChecksum)
      m_pMachine->PutWarning("Recorded checksum 0x%02x does not match computed checksum 0x%02x when decoding a tape file",
			     m_ucRecordBuffer[2 + 1 + 128],m_ucChecksum);
    if (m_bBadEOF)
      m_pMachine->PutWarning("Detected a bad byte in an EOF chunk when decding a tape file");
    m_DecoderState = Idle;
    break;
  case Idle:
    // This state does nothing. It is the default state. Wait until somebody picks up the buffered data.
    break;
  case StartNextRecord:
    // Start the next record. This is the state the caller must put us in to continue
    // decoding the next record. If the last record type was an EOF record,
    // go to another state to find the next header.
    m_dIRG          = 0.0;
    m_dSyncDuration = 0.0;
    if (m_ucRecordType == 0xfe) {
      // This was an EOF record, advance.
      m_DecoderState = SkipOverEOF;
    } else {
      m_DecoderState = SkipOverIRG;
    }
    break;
  case SkipOverIRG:
    // Skip over the next IRG, prepare for the next record.
    m_pDecoder->SkipInitialHeader(0.02,false);
    m_dIRG += 20.0;
    if (m_dIRG >= 160.0) {
      m_DecoderState  = FindSync;
      m_dSyncDuration = 0.0;
    }
    break;
  case SkipOverEOF:
    // First make a three-second skip here.
    try {
      // Ignore potential errors here.
      m_pDecoder->SkipInitialHeader(0.02,false);
    } catch(const AtariException &ae) {
    }
    m_dIRG += 20.0;
    if (m_dIRG >= 3000.0) {
      m_DecoderState  = FindNextFile;
      m_dSyncDuration = 0.0;
    }
    break;
  case FindNextFile:
    // Test whether another file is coming. If so, re-initiate the read.
    // For that, read for seven seconds at least.
    if (m_pDecoder->RemainingTape() > 0.0) {
      double sirg;
      try {
	sirg = m_pDecoder->FindBaudRate(0.02,false);
      } catch(const AtariException &ae) {
	// Nothing suitable found, continue searching.
	m_dSyncDuration = 0.0;
	// Do not advance the state.
	break;
      }
      //
      // If a sync byte has been found, check whether the baud rate is reasonable.
      if (sirg > 0.0) {
	m_dBaud = m_pDecoder->BaudRateOf();
	if (m_dBaud >= 500.0 && m_dBaud <= 700.0) {
	  // Found a valid header. Now go and find the record
	  // type.
	  m_dIRG          += sirg + m_dSyncDuration;
	  m_DecoderState   = FindRecordType;
	  break;
	} else {
	  // Otherwise, continue trying.
	  m_dIRG         += -sirg + m_dSyncDuration;
	  m_dSyncDuration = 0.0;
	}
      } else {
	// Advance the sync duration. Do not error here.
	m_dSyncDuration   += 0.02;
      }
    } else {
      m_DecoderState = TapeEOF;
    }
    break;
  case TapeEOF:
    // No further data on tape, abort.
    break;
  }
}
///

/// WavDecoder::ReadChunk
// Read from the image into the buffer if supplied.
// Returns the number of bytes read. Returns also
// the IRG size in milliseconds.
UWORD WavDecoder::ReadChunk(UBYTE *buffer,LONG size,UWORD &irg)
{
  //
  assert(m_pWav && m_pFilter && m_pDecoder);

  // This advances the decoder state until either the tape runs out of data
  // or the buffer is complete. Hopefully, the VBI calls have already
  // done part of the job.
  while(m_DecoderState != Idle && m_DecoderState != TapeEOF) {
    AdvanceDecoding();
  }
  //
  // If we got an IDLE state, provide the buffer contents to the emulator
  // core.
  if (m_DecoderState == Idle) {
    LONG bytes = m_pucBufPtr - m_ucRecordBuffer;
    if (bytes > size)
      bytes = size;
    memcpy(buffer,m_ucRecordBuffer,bytes);
    irg = UWORD(m_dIRG);
    //
    // Prepare the state machine to continue with the next block.
    m_DecoderState = StartNextRecord;
    return UWORD(bytes);
  } else {
    // Tape is at EOF. Do not return any bytes any more.
    return 0;
  }
}
///

/// WavDecoder::WriteChunk
// Create a new chunk, write it to the file.
void WavDecoder::WriteChunk(UBYTE *buffer,LONG buffersize,UWORD irg)
{
  const ULONG freq = 44100;  // The sampling frequency. Could specify, but don't...
  const UWORD baud = 600;    // Write exactly with the specified baud rate.
  //
  // Is the wav file already there?
  if (m_pWav == NULL) {
    m_pWav = new WavFile(m_pFile);
    // We could select the frequency here, but no matter. Just use the generic 44.1kHz
    m_pWav->WriteHeader(freq);
  }
  //
  // Create the synthesis filter pair if not yet there.
  if (m_pSynthesis == NULL) {
    m_pSynthesis = new FilterPair(freq,1.0);
    m_dLag       = 0.0;
    m_bPositive  = true;
  }
  //
  // Write write the IRG. This is written as "mark".
  m_pSynthesis->WriteBit(m_pWav,true,irg / 1000.0,m_dLag,m_bPositive);
  //
  // Write the body of the chunk.
  while(buffersize) {
    m_pSynthesis->WriteByte(m_pWav,*buffer,baud,m_dLag,m_bPositive);
    buffersize--;
    buffer++;
  }
}
///

/// WavDecoder::Close
// Cleanup the Wav file and complete its header
void WavDecoder::Close(void)
{
  // Is it open in first place?
  if (m_pSynthesis && m_pWav) {
    m_pWav->CompleteFile();
    delete m_pSynthesis; m_pSynthesis = NULL;
    delete m_pWav      ; m_pWav       = NULL;
  }
}
///

///
#endif

