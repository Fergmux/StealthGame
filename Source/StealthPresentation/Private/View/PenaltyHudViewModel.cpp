#include "View/PenaltyHudViewModel.h"

#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

#define LOCTEXT_NAMESPACE "StealthPenaltyHud"

namespace PenaltyHudPalette
{
	FLinearColor Cochineal() { return FLinearColor(0.85f, 0.12f, 0.22f); }
	FLinearColor CochinealDeep() { return FLinearColor(0.42f, 0.04f, 0.09f); }
	FLinearColor Bone() { return FLinearColor(0.92f, 0.89f, 0.82f); }
	FLinearColor Obsidian() { return FLinearColor(0.04f, 0.04f, 0.05f); }
}

namespace PenaltyHudViewModelLocal
{
	FString ColourText(const FLinearColor& C)
	{
		return FString::Printf(TEXT("(%.3f,%.3f,%.3f,%.3f)"), C.R, C.G, C.B, C.A);
	}

	/** easeOut(x) = 1 - (1 - x)^2 over clamp(x, 0, 1) (HUD F3's eased entrance for the card). */
	float EaseOut(double X)
	{
		const double C = FMath::Clamp(X, 0.0, 1.0);
		return static_cast<float>(1.0 - (1.0 - C) * (1.0 - C));
	}
}

// ---- Tally (UI-NKP-1, HUD F6) ---------------------------------------------------------------------------------------------

FPenaltyTallyView FPenaltyTallyView::Make(int32 Tally, int32 Limit, bool bPresentedInformant)
{
	FPenaltyTallyView View;
	View.bVisible = true;
	View.Tally = FMath::Max(0, Tally);
	View.Limit = FMath::Max(0, Limit);
	View.Filled = FMath::Min(View.Tally, View.Limit);
	View.bFrozen = bPresentedInformant;
	// F6: triangle = (n >= L - 1) and PresentedOwnState = Active (ruling D4: a frozen informant tally shows none).
	View.bTriangle = !bPresentedInformant && View.Limit > 0 && View.Tally >= View.Limit - 1;
	View.Text = FText::Format(LOCTEXT("WrongTargets", "Wrong targets: {0}"), FText::AsNumber(View.Tally));
	View.FillColor = PenaltyHudPalette::Cochineal();
	View.OutlineColor = PenaltyHudPalette::Bone();
	View.TextColor = PenaltyHudPalette::Bone();
	View.TriangleColor = PenaltyHudPalette::Cochineal();
	return View;
}

FString FPenaltyTallyView::Dump() const
{
	using namespace PenaltyHudViewModelLocal;
	return FString::Printf(TEXT("visible=%d|text=%s|filled=%d/%d|triangle=%d|frozen=%d|fill=%s|outline=%s|textc=%s|tri=%s"), bVisible ? 1 : 0, *Text.ToString(),
		Filled, Limit, bTriangle ? 1 : 0, bFrozen ? 1 : 0, *ColourText(FillColor), *ColourText(OutlineColor), *ColourText(TextColor), *ColourText(TriangleColor));
}

// ---- Exile card (UI-NKP-3, HUD-E10, F3) -----------------------------------------------------------------------------------

FPenaltyExileCardView FPenaltyExileCardView::Make(double EnteredServerTime, double ServerNow, const FPenaltyCardTiming& Timing)
{
	using namespace PenaltyHudViewModelLocal;
	FPenaltyExileCardView View;
	if (EnteredServerTime < 0.0 || ServerNow < EnteredServerTime)
	{
		return View;
	}
	const double T = ServerNow - EnteredServerTime;
	const double InSec = FMath::Max(Timing.InSec, 0.0);
	const double HoldSec = FMath::Max(Timing.HoldSec, 0.0);
	const double OutSec = FMath::Max(Timing.OutSec, 0.0);
	if (T >= InSec + HoldSec + OutSec)
	{
		return View; // collapsed into the brackets: gone
	}
	View.Title = LOCTEXT("CardTitle", "Informant");
	View.Subtitle = LOCTEXT("CardSubtitleTooMany", "Too many wrong targets");
	View.Line2 = LOCTEXT("CardLine2", "You watch for your team now.");
	if (T < InSec)
	{
		View.Phase = EPenaltyCardPhase::Entering;
		View.Alpha = InSec > 0.0 ? EaseOut(T / InSec) : 1.f;
	}
	else if (T < InSec + HoldSec)
	{
		View.Phase = EPenaltyCardPhase::Holding;
		View.Alpha = 1.f;
	}
	else
	{
		View.Phase = EPenaltyCardPhase::Collapsing;
		const double X = OutSec > 0.0 ? (T - InSec - HoldSec) / OutSec : 1.0;
		View.Collapse = static_cast<float>(FMath::Clamp(X, 0.0, 1.0));
		View.Alpha = 1.f - View.Collapse;
	}
	return View;
}

FString FPenaltyExileCardView::Dump() const
{
	return FString::Printf(TEXT("phase=%d|alpha=%.3f|collapse=%.3f|%s|%s|%s"), static_cast<int32>(Phase), Alpha, Collapse, *Title.ToString(), *Subtitle.ToString(),
		*Line2.ToString());
}

// ---- Returning countdown (UI-NKP-4, HUD-E11, F7) ---------------------------------------------------------------------------

FPenaltyCountdownView FPenaltyCountdownView::Make(bool bAwaitingBody, double DueServerTime, double ServerNow, bool bFadeComplete)
{
	FPenaltyCountdownView View;
	if (!bAwaitingBody || DueServerTime <= 0.0 || !bFadeComplete)
	{
		return View;
	}
	View.bVisible = true;
	// Whole-second steps: ceil of the remaining time; a value within 1 ms of a whole second counts as that second.
	const double Remaining = DueServerTime - ServerNow;
	View.Seconds = Remaining > 0.0 ? FMath::Max(0, FMath::CeilToInt(Remaining - 0.001)) : 0;
	View.Text = View.Seconds > 0 ? FText::Format(LOCTEXT("ReturningIn", "Returning in {0}"), FText::AsNumber(View.Seconds))
		: LOCTEXT("Returning", "Returning");
	return View;
}

FString FPenaltyCountdownView::Dump() const
{
	return FString::Printf(TEXT("visible=%d|seconds=%d|text=%s"), bVisible ? 1 : 0, Seconds, *Text.ToString());
}

// ---- The overlay ---------------------------------------------------------------------------------------------------------

/** The minimal production Penalty HUD overlay: paints one FPenaltyHudFrame per frame, never takes input. */
class SPenaltyHudOverlay final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SPenaltyHudOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TFunction<FPenaltyHudFrame()> InProvider)
	{
		Provider = MoveTemp(InProvider);
		SetVisibility(EVisibility::HitTestInvisible);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.0, 1.0); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 LayerId,
		const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		if (!Provider)
		{
			return LayerId;
		}
		const FPenaltyHudFrame Frame = Provider();
		const FVector2D Size = Geometry.GetLocalSize();
		// Sizes scale with the viewport's pixel height (1080p reference), never below a legible minimum on small PIE windows,
		// and are expressed in this widget's local units (the viewport's DPI scale is undone, so the tally lines up with the
		// canvas-drawn status panel, which draws in pixels).
		const float PxToLocal = Geometry.Scale > 0.f ? 1.f / Geometry.Scale : 1.f;
		const float S = FMath::Max(0.6f, static_cast<float>(Size.Y / 1080.0) / PxToLocal) * PxToLocal;
		PaintTally(Frame.Tally, Geometry, Out, LayerId, S, PxToLocal);
		PaintCountdown(Frame.Countdown, Geometry, Out, LayerId, S, Size);
		PaintCard(Frame.Card, Geometry, Out, LayerId + 2, S, Size);
		return LayerId + 4;
	}

private:
	static const FSlateBrush* White()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	static FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}

	static void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, float X, float Y, float W, float H, const FLinearColor& Colour)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2D(W, H), FSlateLayoutTransform(FVector2D(X, Y))), White(), ESlateDrawEffect::None, Colour);
	}

	static void Outline(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, float X, float Y, float W, float H, float T, const FLinearColor& Colour)
	{
		Box(Out, Layer, Geometry, X, Y, W, T, Colour);
		Box(Out, Layer, Geometry, X, Y + H - T, W, T, Colour);
		Box(Out, Layer, Geometry, X, Y, T, H, Colour);
		Box(Out, Layer, Geometry, X + W - T, Y, T, H, Colour);
	}

	/** An upward triangle in a Side x Side box at (X, Y). */
	static void Triangle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, float X, float Y, float Side, float Thickness, const FLinearColor& Colour)
	{
		TArray<FVector2f> Points;
		Points.Add(FVector2f(X + Side * 0.5f, Y));
		Points.Add(FVector2f(X, Y + Side));
		Points.Add(FVector2f(X + Side, Y + Side));
		Points.Add(FVector2f(X + Side * 0.5f, Y));
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Thickness);
	}

	static FSlateFontInfo Font(float Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(6, FMath::RoundToInt(Size)));
	}

	static FVector2D Measure(const FText& Text, const FSlateFontInfo& FontInfo)
	{
		FSlateRenderer* Renderer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer() : nullptr;
		return Renderer != nullptr ? FVector2D(Renderer->GetFontMeasureService()->Measure(Text, FontInfo)) : FVector2D::ZeroVector;
	}

	static void Label(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FText& Content, float X, float Y, const FSlateFontInfo& FontInfo,
		const FLinearColor& Colour, bool bCentred)
	{
		const FVector2D Extent = Measure(Content, FontInfo);
		const float DrawX = bCentred ? X - static_cast<float>(Extent.X) * 0.5f : X;
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2D(FMath::Max(1.0, Extent.X), FMath::Max(1.0, Extent.Y)),
			FSlateLayoutTransform(FVector2D(DrawX, Y))), Content, FontInfo, ESlateDrawEffect::None, Colour);
	}

	/** The tally under the canvas status panel (HUD Z1, top left). */
	static void PaintTally(const FPenaltyTallyView& Tally, const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float S, float PxToLocal)
	{
		if (!Tally.bVisible)
		{
			return;
		}
		// Directly under the canvas status panel (unscaled 24 px margin; the panel ends above 160 px at every role).
		const float St = FMath::Max(S, 0.8f * PxToLocal);
		const float X = 24.f * PxToLocal;
		const float Y = 166.f * PxToLocal;
		const float Square = 18.f * St;
		const float Gap = 6.f * St;
		const FSlateFontInfo LineFont = Font(18.f * St);
		const float RowWidth = Tally.Limit * (Square + Gap) + Square + 4.f * St;
		const float TextWidth = static_cast<float>(Measure(Tally.Text, LineFont).X);
		Box(Out, Layer, Geometry, X - 6.f * St, Y - 6.f * St, FMath::Max(RowWidth, TextWidth) + 12.f * St, Square + 12.f * St + 26.f * St,
			WithAlpha(PenaltyHudPalette::Obsidian(), 0.55f));
		for (int32 Index = 0; Index < Tally.Limit; ++Index)
		{
			const float SX = X + Index * (Square + Gap);
			if (Index < Tally.Filled)
			{
				Box(Out, Layer + 1, Geometry, SX, Y, Square, Square, Tally.FillColor);
			}
			Outline(Out, Layer + 1, Geometry, SX, Y, Square, Square, FMath::Max(1.f, 2.f * St), Tally.OutlineColor);
		}
		if (Tally.bTriangle)
		{
			Triangle(Out, Layer + 1, Geometry, X + Tally.Limit * (Square + Gap) + 4.f * St, Y, Square, FMath::Max(1.f, 3.f * St), Tally.TriangleColor);
		}
		Label(Out, Layer + 1, Geometry, Tally.Text, X, Y + Square + 4.f * St, LineFont, Tally.TextColor, false);
	}

	/** The Notice slot on the neutral view: Bone on Obsidian, no pulse, no colour change near zero. */
	static void PaintCountdown(const FPenaltyCountdownView& Countdown, const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float S, const FVector2D& Size)
	{
		if (!Countdown.bVisible)
		{
			return;
		}
		const float W = 360.f * S;
		const float H = 56.f * S;
		const float X = static_cast<float>(Size.X) * 0.5f - W * 0.5f;
		const float Y = static_cast<float>(Size.Y) * 0.40f;
		Box(Out, Layer, Geometry, X, Y, W, H, WithAlpha(PenaltyHudPalette::Obsidian(), 0.92f));
		Label(Out, Layer + 1, Geometry, Countdown.Text, X + W * 0.5f, Y + 12.f * S, Font(24.f * S), PenaltyHudPalette::Bone(), true);
	}

	/** The card in the central band (560 x 200 at 1080p: well under 30 % of the screen), collapsing towards the brackets. */
	static void PaintCard(const FPenaltyExileCardView& Card, const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float S, const FVector2D& Size)
	{
		if (Card.Phase == EPenaltyCardPhase::Hidden || Card.Alpha <= 0.f)
		{
			return;
		}
		const float Scale = FMath::Lerp(1.f, 0.15f, Card.Collapse);
		const float W = 560.f * S * Scale;
		const float H = 200.f * S * Scale;
		const FVector2D Centre(Size.X * 0.5, Size.Y * 0.36);
		const FVector2D Brackets(140.0 * S, 60.0 * S);
		const FVector2D At = FMath::Lerp(Centre, Brackets, static_cast<double>(Card.Collapse));
		const float X = static_cast<float>(At.X) - W * 0.5f;
		const float Y = static_cast<float>(At.Y) - H * 0.5f;
		const float A = Card.Alpha;
		Box(Out, Layer, Geometry, X, Y, W, H, WithAlpha(PenaltyHudPalette::Obsidian(), 0.94f * A));
		const float Bar = 64.f * S * Scale;
		Box(Out, Layer + 1, Geometry, X, Y, W, Bar, WithAlpha(PenaltyHudPalette::CochinealDeep(), A));
		Triangle(Out, Layer + 2, Geometry, X + 18.f * S * Scale, Y + 14.f * S * Scale, 36.f * S * Scale, FMath::Max(1.f, 3.f * S * Scale), WithAlpha(PenaltyHudPalette::Bone(), A));
		Label(Out, Layer + 2, Geometry, Card.Title, X + W * 0.5f, Y + 12.f * S * Scale, Font(32.f * S * Scale), WithAlpha(PenaltyHudPalette::Bone(), A), true);
		Label(Out, Layer + 2, Geometry, Card.Subtitle, X + W * 0.5f, Y + Bar + 24.f * S * Scale, Font(24.f * S * Scale), WithAlpha(PenaltyHudPalette::Bone(), A), true);
		Label(Out, Layer + 2, Geometry, Card.Line2, X + W * 0.5f, Y + Bar + 72.f * S * Scale, Font(18.f * S * Scale), WithAlpha(PenaltyHudPalette::Bone(), 0.85f * A), true);
	}

	TFunction<FPenaltyHudFrame()> Provider;
};

TSharedRef<SWidget> MakePenaltyHudOverlay(TFunction<FPenaltyHudFrame()> Provider)
{
	return SNew(SPenaltyHudOverlay, MoveTemp(Provider));
}

#undef LOCTEXT_NAMESPACE
