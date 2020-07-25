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
  GetParam(kPCoarse)->InitDouble("Coarse Gain", 0.0, -150, 150, 1, "dB", IParam::kFlagStepped, "", IParam::ShapeLinear(), IParam::kUnitLinearGain);
  GetParam(kPFine)->InitDouble("Fine Gain", 0, -4, 4, 0.1, "dB", IParam::kFlagsNone, "", IParam::ShapeLinear(), IParam::kUnitLinearGain);
  GetParam(kPRange)->InitDouble("Fine Range", 4, 1, 50, 0.1, "dB", IParam::kFlagMeta);

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* ui) {
    ui->AttachCornerResizer(EUIResizerMode::Scale, false);
    ui->AttachPanelBackground(COLOR_BG_1);
    ui->LoadFont("Roboto-Regular", ROBOTO_FN);

    const float ROW1_H = 100;
    const float ROW2_H = 24;
    const float PAD = 8;

    const IRECT b = ui->GetBounds();
    const IRECT r1 = b.GetFromTop(ROW1_H).GetPadded(-PAD);
    const IRECT r2 = b.GetFromBottom(ROW2_H + PAD).GetPadded(-PAD);

    const auto style = DEFAULT_STYLE
      .WithColor(EVColor::kFR, COLOR_WHITE)
      .WithColor(EVColor::kPR, COLOR_BG_2)
      .WithColor(EVColor::kFG, COLOR_BG_2)
      .WithColor(EVColor::kX1, COLOR_BG_1)
      .WithLabelText(DEFAULT_STYLE.labelText.WithFGColor(COLOR_TEXT))
      .WithValueText(DEFAULT_STYLE.valueText.WithFGColor(COLOR_TEXT).WithSize(16.0f))
      ;
    auto knobCoarse = new IVKnobControl(r1.GetGridCell(0, 0, 1, 3), kPCoarse, "", style);
    auto knobFine = new IVKnobControl(r1.GetGridCell(0, 1, 1, 3), kPFine, "", style);
    auto cRange = new IVSliderControl(r1.GetGridCell(0, 2, 1, 3), kPRange, "", style, true, EDirection::Vertical);

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

    ui->AttachControl(knobCoarse);
    ui->AttachControl(knobFine);
    ui->AttachControl(cRange);
    ui->AttachControl(textTitle);
  };
#endif
}

#if IPLUG_EDITOR
void BigLittleGain::OnParamChangeUI(int paramIdx, EParamSource source)
{
  switch (paramIdx) {
  case kPRange:
    // We re-init the value with the new min and max so the automation shows the correct value in hosts.
    double vRange = GetParam(kPRange)->Value();
    GetParam(kPFine)->InitDouble("Fine Gain", 0, -vRange, vRange, 0.1, "dB", IParam::kFlagsNone, "", IParam::ShapeLinear(), IParam::kUnitLinearGain);
    if (source != EParamSource::kReset) {
      InformHostOfParameterDetailsChange();
    }
    break;
  }
}
#endif

#if IPLUG_DSP
void BigLittleGain::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  //const double gain = GetParam(kGain)->Value() / 100.;
  const int nChans = NOutChansConnected();

  for (int s = 0; s < nFrames; s += SAMPLE_UPDATE_COUNT) {
    // We update the gain every SAMPLE_UPDATE_COUNT samples. This is done outside the crticial loop
    // to make things easier on the optimizer (instead of an if statement inside the loop).
    const double gain = DBToAmp(GetParam(kPCoarse)->Value() + GetFineAtomic());
    const int iMax = s + std::min(SAMPLE_UPDATE_COUNT, nFrames - s);

    for (int c = 0; c < nChans; c++) {
      // N.B. This could potentially use SIMD, but that's more effort than I want to spend.
      for (int i = s; i < iMax; i++) {
        outputs[c][i] = inputs[c][i] * gain;
      }
    }
  }
}
#endif

double BigLittleGain::GetFineAtomic() const
{
  double vFine = GetParam(kPFine)->GetNormalized();
  double vRange = GetParam(kPRange)->Value();
  return Lerp(-vRange, vRange, vFine);
}
