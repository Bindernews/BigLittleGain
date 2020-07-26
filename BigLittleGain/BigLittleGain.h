#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;

enum EParams
{
  kPCoarse = 0,
  kPFine,
  kPCoarseRange,
  kPFineRange,
  kNumParams
};

enum EControls
{
  kCtrlBigKnob = 0,
  kCtrlBigSlider,
  kCtrlLittleKnob,
  kCtrlLittleSlider,
  kCtrlTitleCoarse,
  kCtrlTitleFine,
  kNumControls,
};

using namespace iplug;
using namespace igraphics;

class BigLittleGain final : public Plugin
{
public:
  BigLittleGain(const InstanceInfo& info);

  double GetFineAtomic() const;
  double GetCoarseAtomic() const;

#if IPLUG_EDITOR
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void ReInitKnob(int paramIdx, int controlTag, EParamSource source, const char* name, double range);
#endif

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
};
