#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;

enum EParams
{
  kPCoarse = 0,
  kPFine,
  kPRange,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class BigLittleGain final : public Plugin
{
public:
  BigLittleGain(const InstanceInfo& info);

  double GetFineAtomic() const;

#if IPLUG_EDITOR
  /** Number of times the user has clicked the title button. */
  int mClickCount = 0;
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
#endif

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
};
