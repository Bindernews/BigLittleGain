#include "BigLittleGain.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#define PLUGIN_URL "https://github.com/bindernews/BigLittleGain"
#define SAMPLE_UPDATE_COUNT (128)
#define DEF_COLOR(name, code) const IColor name = IColor::FromColorCode(code)
DEF_COLOR(COLOR_BG_2, 0x603F8B); // dark purple
DEF_COLOR(COLOR_BG_1, 0x695E93); // light purple
DEF_COLOR(COLOR_TEXT, 0xF9F6F0); // cream

/** This title button class opens the plugin's homepage URL once it's been double-clicked. */
class TitleButton : public IVButtonControl
{
public:
  TitleButton(const IRECT& bounds, const char* label, const IVStyle& style)
    : IVButtonControl(bounds, SplashClickActionFunc, label, style)
  {}

  void OnMouseOut() override
  {
    IVButtonControl::OnMouseOut();
    // Reset click count if the user moves out of the control
    mClickCount = 0;
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    IVButtonControl::OnMouseUp(x, y, mod);

    double tm = GetTimestamp();
    if ((tm < mLastClickTime + mClickTimeout) || (mClickCount == 0)) {
      mClickCount++;
    }
    else {
      mClickCount = 0;
    }
    mLastClickTime = tm;

    if (mClickCount == 2) {
      GetUI()->OpenURL(PLUGIN_URL);
    }
  }

  /** How long before the click count timeout expires. */
  double mClickTimeout = 1.0 / 4.0;
  /** Previous click time */
  double mLastClickTime = 0;
  int mClickCount = 0;
};


BigLittleGain::BigLittleGain(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kPCoarse)->InitDouble("Coarse Gain", 0.0, -50, 50, 1, "dB", IParam::kFlagStepped, "", IParam::ShapeLinear(), IParam::kUnitLinearGain);
  GetParam(kPCoarseRange)->InitDouble("Coarse Range", 50, 10, 150, 0.1, "dB", IParam::kFlagMeta);
  GetParam(kPFine)->InitDouble("Fine Gain", 0, -4, 4, 0.1, "dB", IParam::kFlagsNone, "", IParam::ShapeLinear(), IParam::kUnitLinearGain);
  GetParam(kPFineRange)->InitDouble("Fine Range", 4, 1, 50, 0.1, "dB", IParam::kFlagMeta);
  GetParam(kPHighQuality)->InitBool("High Quality", false, "", IParam::kFlagCannotAutomate, "");

  mParamInfo[kPCoarse] = { true };
  mParamInfo[kPCoarseRange] = { true };
  mParamInfo[kPFine] = { true };
  mParamInfo[kPFineRange] = { true };
  mParamInfo[kPHighQuality] = { false };

#if IPLUG_DSP
  mParamRamps = CRP::Create(mParamRampsData);
#endif

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* ui) {
    ui->AttachCornerResizer(EUIResizerMode::Scale, false);
    ui->AttachPanelBackground(COLOR_BG_1);
    ui->LoadFont("Roboto-Regular", ROBOTO_FN);

    const float TITLE_H = 40;
    const float ROW1_H = 100;
    const float ROW2_H = 24;
    const float PAD = 8;

    const IRECT b = ui->GetBounds();
    const IRECT rTitle = b.GetFromTop(60).GetPadded(-PAD);
    const IRECT r1 = b.GetFromTop(ROW1_H).GetVShifted(TITLE_H);
    const IRECT r2 = b.GetFromBottom(ROW2_H + PAD).GetPadded(-PAD);
    const IRECT r1Coarse = r1.GetGridCell(0, 1, 2).GetReducedFromRight(PAD * 2);
    const IRECT r1Fine = r1.GetGridCell(1, 1, 2).GetReducedFromLeft(PAD * 2);
    const IRECT separatorR = r1.GetMidHPadded(PAD / 2).GetVPadded(PAD);

    const auto styleTitle = DEFAULT_STYLE
      .WithDrawFrame(false)
      .WithDrawShadows(false)
      .WithValueText(DEFAULT_STYLE.labelText.WithFGColor(COLOR_TEXT).WithSize(20.0f))
      ;
    auto titleLabelCoarse = new IVLabelControl(rTitle.GetGridCell(0, 1, 2), "Coarse", styleTitle);
    auto titleLabelFine = new IVLabelControl(rTitle.GetGridCell(1, 1, 2), "Fine", styleTitle);

    const auto separatorStyle = DEFAULT_STYLE
      .WithColor(EVColor::kBG, COLOR_WHITE)
      .WithDrawFrame(false)
      .WithRoundness(PAD)
      ;

    const auto style = DEFAULT_STYLE
      .WithColor(EVColor::kFR, COLOR_WHITE)
      .WithColor(EVColor::kPR, COLOR_BG_2)
      .WithColor(EVColor::kFG, COLOR_BG_2)
      .WithColor(EVColor::kX1, COLOR_BG_1)
      .WithLabelText(DEFAULT_STYLE.labelText.WithFGColor(COLOR_TEXT))
      .WithValueText(DEFAULT_STYLE.valueText.WithFGColor(COLOR_TEXT).WithSize(16.0f))
      ;
    auto sliderBigRange = new IVSliderControl(r1Coarse.GetGridCell(0, 1, 2), kPCoarseRange, "Range", style, true, EDirection::Vertical);
    auto knobCoarse = new IVKnobControl(r1Coarse.GetGridCell(1, 1, 2), kPCoarse, "Gain", style, true);
    auto separator = new IVLabelControl(separatorR, "", separatorStyle);
    auto knobFine = new IVKnobControl(r1Fine.GetGridCell(0, 1, 2), kPFine, "Gain", style, true);
    auto sliderLittleRange = new IVSliderControl(r1Fine.GetGridCell(1, 1, 2), kPFineRange, "Range", style, true, EDirection::Vertical);

    const auto styleUrlButton = style
      .WithColor(EVColor::kFG, COLOR_BG_1)
      .WithLabelText(style.valueText)
      .WithDrawFrame(false)
      .WithDrawShadows(false)
      ;
    auto textTitleAction = [ui](IControl* ctrl) {
      ctrl->GetUI()->OpenURL(PLUGIN_URL, "Would you like to open this page?");
      SplashClickActionFunc(ctrl);
    };
    auto textTitle = new TitleButton(r2, /*textTitleAction,*/ "big.little.gain   Copyright (c) BinderNews 2020", styleUrlButton);

    ui->AttachControl(titleLabelCoarse, kCtrlTitleCoarse);
    ui->AttachControl(titleLabelFine, kCtrlTitleFine);
    ui->AttachControl(sliderBigRange, kCtrlBigSlider);
    ui->AttachControl(separator);
    ui->AttachControl(knobCoarse, kCtrlBigKnob);
    ui->AttachControl(knobFine, kCtrlLittleKnob);
    ui->AttachControl(sliderLittleRange, kCtrlLittleSlider);
    ui->AttachControl(textTitle);
  };
#endif
}

#if IPLUG_EDITOR
void BigLittleGain::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (paramIdx == kPFineRange) {
    // We re-init the value with the new min and max so the automation shows the correct value in hosts.
    double vRange = GetParam(paramIdx)->Value();
    ReInitKnob(kPFine, kCtrlLittleKnob, source, "Fine Gain", vRange);
  }
  else if (paramIdx == kPCoarseRange) {
    // We re-init the value with the new min and max so the automation shows the correct value in hosts.
    double vRange = GetParam(paramIdx)->Value();
    ReInitKnob(kPCoarse, kCtrlBigKnob, source, "Coarse Gain", vRange);
  }
}

void BigLittleGain::ReInitKnob(int paramIdx, int controlTag, EParamSource source, const char* name, double range)
{
  auto param = GetParam(paramIdx);
  double norm = param->GetNormalized();
  param->InitDouble(name, 0, -range, range, 0.1, "dB", IParam::kFlagsNone, "", IParam::ShapeLinear(), IParam::kUnitLinearGain);
  param->SetNormalized(norm);
  if (source != EParamSource::kReset) {
    InformHostOfParameterDetailsChange();
  }
  auto ui = GetUI();
  if (ui) {
    ui->GetControlWithTag(controlTag)->SetDirty(false);
  }
}
#endif

#if IPLUG_DSP
void BigLittleGain::OnReset()
{
  mGainBuf.Resize(GetBlockSize());
  for (int i = 0; i < mParamRampsBuf.size(); i++) {
    if (mParamInfo[i].hasRamp) {
      mParamRampsBuf.at(i).Resize(GetBlockSize());
      ControlRamp& rData = mParamRampsData.at(i);
      rData.transitionStart = rData.transitionEnd = 0;
      rData.startValue = rData.endValue = getParamValue(i);
    }
  }
}

void BigLittleGain::OnParamChange(int paramIdx, EParamSource source, int sampleOffset)
{
  if (paramIdx >= kNumParams || !mParamInfo[paramIdx].hasRamp) {
    return;
  }

  double val = getParamValue(paramIdx);  
  // Set the param ramp values accordingly
  if (sampleOffset == -1) {
    mParamRamps->at(paramIdx).SetTarget(val, 0, GetBlockSize(), GetBlockSize());
  }
  else if (sampleOffset == 0) {
    mParamRampsData.at(paramIdx).endValue = val;
  }
  else {
    mParamRamps->at(paramIdx).SetTarget(val, 0, sampleOffset, GetBlockSize());
  }
}

void BigLittleGain::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  sample* gain_buf = mGainBuf.Get();

  if (GetParam(kPHighQuality)->Bool()) {
    // High quality is sample-accurate but takes more time to generate the gain array
    for (int i = 0; i < kNumParams; i++) {
      if (mParamInfo[i].hasRamp) {
        mParamRamps->at(i).Process(nFrames);
        mParamRampsData.at(i).Write(mParamRampsBuf.at(i).Get(), 0, nFrames);
      }
    }
    // Now that we have our ramp values, create the gain buffer accordingly
    auto coarseBuf = mParamRampsBuf[kPCoarse].Get();
    auto coarseRangeBuf = mParamRampsBuf[kPCoarseRange].Get();
    auto fineBuf = mParamRampsBuf[kPFine].Get();
    auto fineRangeBuf = mParamRampsBuf[kPFineRange].Get();
    for (int i = 0; i < nFrames; i++) {
      auto cr = coarseRangeBuf[i];
      auto fr = fineRangeBuf[i];
      gain_buf[i] = DBToAmp((double)Lerp(-cr, cr, coarseBuf[i]) + (double)Lerp(-fr, fr, fineBuf[i]));
    }
  }
  else {
    // Low quality is easier on the processor, but not sample-accurate
    const sample gain = (sample)DBToAmp(GetCoarseAtomic() + GetFineAtomic());
    for (int i = 0; i < nFrames; i++) {
      gain_buf[i] = gain;
    }
  }
  
  // Now we can update the outputs. 
  for (int c = 0; c < nChans; c++) {
    // N.B. This could potentially use SIMD, but that's more effort than I want to spend.
    // At least on MSVC this does produce SIMD code through auto-vectorization.
    for (int i = 0; i < nFrames; i++) {
      outputs[c][i] = inputs[c][i] * gain_buf[i];
    }
  }
}

double BigLittleGain::getParamValue(int paramIdx) const
{
  switch (paramIdx) {
  case kPCoarse:
  case kPFine:
    return GetParam(paramIdx)->GetNormalized();
  case kPCoarseRange:
  case kPFineRange:
  case kPHighQuality:
    return GetParam(paramIdx)->Value();
  }
}
#endif

double BigLittleGain::GetFineAtomic() const
{
  double vFine = GetParam(kPFine)->GetNormalized();
  double vRange = GetParam(kPFineRange)->Value();
  return Lerp(-vRange, vRange, vFine);
}

double BigLittleGain::GetCoarseAtomic() const
{
  double vKnob = GetParam(kPCoarse)->GetNormalized();
  double vRange = GetParam(kPCoarseRange)->Value();
  return Lerp(-vRange, vRange, vKnob);
}
