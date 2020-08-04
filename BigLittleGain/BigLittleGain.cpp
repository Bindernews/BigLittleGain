#include "BigLittleGain.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "IPopupMenuControl.h"

#define PLUGIN_URL "https://github.com/bindernews/BigLittleGain"
#define SAMPLE_UPDATE_COUNT (128)
#define DEF_COLOR(name, code, A) const IColor name = IColor::FromColorCode(code, A)
DEF_COLOR(COLOR_BG_2, 0x603F8B, 255); // dark purple
DEF_COLOR(COLOR_BG_1, 0x695E93, 255); // light purple
DEF_COLOR(COLOR_BG_3, 0x9166cc, 255); // medium purple
DEF_COLOR(COLOR_TEXT, 0xF9F6F0, 255); // cream
DEF_COLOR(COLOR_HL,   0xFFFFFF,  10); // transparent white

#define ABOUT_BOX_TEXT "BigLittleGain   " PLUG_COPYRIGHT_STR "\n \n" \
  "BigLittleGain is a simple volume plugin for both \ngeneral-purpose volume adjustment and for \ncreating volume effects using automation. \n \n" \
  "Made using iPlug2.\n"

static const std::array<int, 8> VIEW_SIZE_OPTIONS = { 50, 75, 85, 100, 125, 150, 175, 200 };

enum EMenuItems 
{
  kMenuAbout,
  kMenuHomepage,
};

#if IPLUG_EDITOR
class VSep : public IVectorBase
           , public IControl
{
public:
  VSep(const IRECT& bounds, const IVStyle& style)
    : IVectorBase(style), IControl(bounds, SplashClickActionFunc)
  {
    AttachIControl(this, "");
  }

  void Draw(IGraphics& g) override
  {
    auto color = mStyle.colorSpec.GetColor(EVColor::kFG);
    auto rect = mRECT.GetPadded(-1.f);
    g.FillRect(color, rect);
    g.DrawRoundRect(color, rect, mStyle.roundness);
  }
};

class MenuButton1 : public IVButtonControl
{
public:
  MenuButton1(const IRECT& bounds, IActionFunction aF = SplashClickActionFunc, const IVStyle& style=DEFAULT_STYLE)
    : IVButtonControl(bounds, aF, "", style, true, true, EVShape::Ellipse)
  {}

  void DrawWidget(IGraphics& g) override
  {
    bool pressed = (bool)GetValue();
    DrawPressableEllipse(g, mRECT, pressed, mMouseIsOver, IsDisabled());
    const float rpad = 6;
    g.FillTriangle(COLOR_TEXT, mRECT.L + rpad, mRECT.T + rpad, mRECT.R - rpad, mRECT.T + rpad, mRECT.MW(), mRECT.B - (rpad));
  }

  bool IsHit(float x, float y) const override
  {
    return mRECT.GetPadded(-1).Contains(x, y);
  }
};

class BCAboutBox : public IVButtonControl
{
public:
  BCAboutBox(const IRECT& bounds, IActionFunction aF = SplashClickActionFunc, const char* label = "", const IVStyle& style = DEFAULT_STYLE)
    : IVButtonControl(bounds, aF, label, style)
  {}

  void DrawLabel(IGraphics& g) override
  {
    //DrawPressableRectangle(g, mWidgetBounds, (bool)GetValue(), mMouseIsOver, IsDisabled());

    const char* textP = mLabelStr.Get();
    WDL_String line;
    float lineY = mWidgetBounds.T + 4;
    float lineX = mWidgetBounds.L + 12;
    while (true) {
      const char* nextP = textP;
      while (*nextP != 0 && *nextP != '\n') {
        nextP++;
      }

      line.SetFormatted(nextP - textP, "%s", textP);
      IRECT lineBounds = IRECT(0, 0, 1, 1);
      g.MeasureText(mStyle.labelText, line.Get(), lineBounds);
      lineBounds = IRECT::MakeXYWH(lineX, lineY, lineBounds.W(), lineBounds.H());
      g.DrawText(mStyle.labelText, line.Get(), lineBounds);

      // Update lineY and textP
      lineY = lineBounds.B;
      if (*nextP == 0) {
        break;
      }
      else {
        textP = nextP + 1;
      }
    }
  }
};
#endif

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
    ui->AttachPanelBackground(COLOR_BG_1);
    ui->LoadFont("Roboto-Regular", ROBOTO_FN);
    ui->LoadFont("Roboto-Bold", ROBOTO_BOLD_FN);
    ui->EnableMouseOver(true);

    const float TITLE_H = 40;
    const float ROW1_H = 100;
    const float ROW2_H = 24;
    const float PAD = 8;
    const float OPT_BTN_SIZE = 10;
    const float BOX_PAD = 4;

    const IRECT b = ui->GetBounds();
    const IRECT rMenuRow = b.GetFromTop(40).GetPadded(-PAD);
    const IRECT rBlgTitle = rMenuRow.GetFromLeft(120);
    const IRECT rOptionsMenuButton = rMenuRow.GetMidHPadded(OPT_BTN_SIZE).GetMidVPadded(OPT_BTN_SIZE);
    const IRECT rTitle = rMenuRow.GetFromTop(40).GetVShifted(rMenuRow.H()).GetPadded(-PAD);
    const IRECT r1 = rTitle.GetFromTop(ROW1_H).GetVShifted(rTitle.H());
    const IRECT r1Coarse = r1.GetGridCell(0, 1, 2).GetReducedFromRight(PAD * 2);
    const IRECT r1Fine = r1.GetGridCell(1, 1, 2).GetReducedFromLeft(PAD * 2);
    const IRECT separatorR = r1.GetMidHPadded(PAD / 2).GetVPadded(PAD);
    const IRECT rAboutBox = b.GetPadded(-BOX_PAD, -BOX_PAD * 2, -BOX_PAD, -BOX_PAD);


    auto pSizeMenu = new IPopupMenu("View Size");
    WDL_String opt;
    for (int i = 0; i < VIEW_SIZE_OPTIONS.size(); i++) {
      opt.SetFormatted(10, "%d%%", VIEW_SIZE_OPTIONS[i]);
      pSizeMenu->AddItem(opt.Get());
    }

    mMenu = new IPopupMenu("[root]");
    mMenu->AddItem("View Size", pSizeMenu);
    mMenu->AddItem(new IPopupMenu::Item("Open Homepage", 0, kMenuHomepage));
    mMenu->AddSeparator();
    mMenu->AddItem(new IPopupMenu::Item("About", 0, kMenuAbout));

    mMenu->SetFunction([this](IPopupMenu* pop) {
      if (pop->GetChosenItem() == nullptr || GetUI() == nullptr) {
        return;
      }
      switch (pop->GetChosenItem()->GetTag())
      {
      case kMenuAbout:
        GetUI()->GetControlWithTag(kCtrlAbout)->Hide(false);
        break;
      case kMenuHomepage:
        GetUI()->OpenURL(PLUGIN_URL);
        break;
      }
    });
    pSizeMenu->SetFunction([this](IPopupMenu* pop) {
      if (pop->GetChosenItem() == nullptr || GetUI() == nullptr) {
        return;
      }
      int size = VIEW_SIZE_OPTIONS[pop->GetChosenItemIdx()];
      const float scale = (float)size / 100.f;
      GetUI()->Resize(PLUG_WIDTH, PLUG_HEIGHT, scale, true);
    });


    const auto blgTitleStyle = DEFAULT_STYLE
      .WithValueText(IText(20.f, COLOR_TEXT, "Roboto-Bold"))
      .WithColor(EVColor::kFG, COLOR_BG_1)
      .WithDrawFrame(false)
      .WithDrawShadows(false)
      ;
    auto blgTitle = new IVLabelControl(rBlgTitle, "Big Little Gain", blgTitleStyle);

    const auto menuButtonStyle = DEFAULT_STYLE
      .WithColors(IVColorSpec({ DEFAULT_BGCOLOR, COLOR_BG_2, COLOR_TRANSPARENT, COLOR_BG_3, COLOR_HL, COLOR_TRANSPARENT,
        DEFAULT_X1COLOR, DEFAULT_X2COLOR, DEFAULT_X3COLOR }));
    auto menuButtonClick = [this](IControl* ctrl) {
      ctrl->GetUI()->CreatePopupMenu(*ctrl, *mMenu, ctrl->GetRECT());
      DefaultClickActionFunc(ctrl);
      //SplashClickActionFunc(ctrl);
    };
    auto menuButton = new MenuButton1(rOptionsMenuButton, menuButtonClick, menuButtonStyle);

    const auto styleTitle = DEFAULT_STYLE
      .WithDrawFrame(false)
      .WithDrawShadows(false)
      .WithValueText(DEFAULT_STYLE.labelText.WithFGColor(COLOR_TEXT).WithSize(20.0f))
      ;
    auto titleLabelCoarse = new IVLabelControl(IRECT(r1Coarse.L, rTitle.T, r1Coarse.R, rTitle.B), "Coarse", styleTitle);
    auto titleLabelFine = new IVLabelControl(IRECT(r1Fine.L, rTitle.T, r1Fine.R, rTitle.B), "Fine", styleTitle);

    const auto separatorStyle = DEFAULT_STYLE
      .WithColor(EVColor::kFG, COLOR_WHITE)
      .WithDrawFrame(false)
      .WithRoundness(PAD / 2)
      ;

    const auto style = DEFAULT_STYLE
      .WithColor(EVColor::kFR, COLOR_TEXT)
      .WithColor(EVColor::kPR, COLOR_BG_2)
      .WithColor(EVColor::kFG, COLOR_BG_2)
      .WithColor(EVColor::kX1, COLOR_BG_1)
      .WithLabelText(DEFAULT_STYLE.labelText.WithFGColor(COLOR_TEXT))
      .WithValueText(DEFAULT_STYLE.valueText.WithFGColor(COLOR_TEXT).WithSize(16.0f))
      ;
    auto sliderBigRange = new IVSliderControl(r1Coarse.GetGridCell(0, 1, 2), kPCoarseRange, "Range", style, true, EDirection::Vertical);
    auto knobCoarse = new IVKnobControl(r1Coarse.GetGridCell(1, 1, 2), kPCoarse, "Gain", style, true);
    auto separator = new VSep(separatorR, separatorStyle);
    auto knobFine = new IVKnobControl(r1Fine.GetGridCell(0, 1, 2), kPFine, "Gain", style, true);
    auto sliderLittleRange = new IVSliderControl(r1Fine.GetGridCell(1, 1, 2), kPFineRange, "Range", style, true, EDirection::Vertical);

    const auto aboutBoxStyle = style
      .WithColor(EVColor::kBG, COLOR_BG_1)
      .WithColor(EVColor::kFG, COLOR_BG_1)
      .WithLabelText(style.labelText.WithAlign(EAlign::Near).WithSize(16.f))
      ;
    auto aboutBox = new BCAboutBox(rAboutBox, [](IControl* ctrl) { ctrl->Hide(true); DefaultClickActionFunc(ctrl); }, ABOUT_BOX_TEXT, aboutBoxStyle);
    aboutBox->Hide(true);

    ui->AttachControl(blgTitle, kCtrlTitleText);
    ui->AttachControl(menuButton, kCtrlMenuButton);
    ui->AttachControl(titleLabelCoarse, kCtrlTitleCoarse);
    ui->AttachControl(titleLabelFine, kCtrlTitleFine);
    ui->AttachControl(sliderBigRange, kCtrlBigSlider);
    ui->AttachControl(separator);
    ui->AttachControl(knobCoarse, kCtrlBigKnob);
    ui->AttachControl(knobFine, kCtrlLittleKnob);
    ui->AttachControl(sliderLittleRange, kCtrlLittleSlider);
    ui->AttachControl(aboutBox, kCtrlAbout);
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
  default:
    return 0.0;
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
