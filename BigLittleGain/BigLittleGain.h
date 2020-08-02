#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Extras/Synth/ControlRamp.h"

const int kNumPresets = 1;

enum EParams
{
  kPCoarse = 0,
  kPFine,
  kPCoarseRange,
  kPFineRange,
  kPHighQuality,
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
  struct ParamInfo
  {
    bool hasRamp;
  };

  BigLittleGain(const InstanceInfo& info);

  double GetFineAtomic() const;
  double GetCoarseAtomic() const;

#if IPLUG_EDITOR
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void ReInitKnob(int paramIdx, int controlTag, EParamSource source, const char* name, double range);
#endif

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnParamChange(int paramIdx, EParamSource source, int sampleOffset) override;
  double getParamValue(int paramIdx) const;

  using CRP = ControlRampProcessor;

  CRP::ProcessorArray<kNumParams>* mParamRamps;
  CRP::RampArray<kNumParams> mParamRampsData;
  std::array<WDL_TypedBuf<float>, kNumParams> mParamRampsBuf;
  WDL_TypedBuf<sample> mGainBuf;
#endif

  ParamInfo mParamInfo[kNumParams];
};
