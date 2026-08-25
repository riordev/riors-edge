#include "UI/BreakerMenu.h"

#include "Save/BreakerCharacterRoster.h"
#include "Characters/BreakerCharacter.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"

#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Characters/BreakerCharacter.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerForgeLibrary.h"
// The legendary registry: the only items in the game that carry a display
// name, which is what a card's line one wants.
#include "Items/BreakerItemRules.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerTravelPoint.h"
// The breakpoint sandbox's three suppliers: the XP curve arithmetic, the
// seeded loot roll, and the gym's area level (a public BlueprintReadWrite
// tunable on the game mode — written directly, the same access the editor
// details panel already has).
#include "Progression/BreakerExperience.h"
#include "Items/BreakerLootLibrary.h"
#include "Game/BreakerGameMode.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SLeafWidget.h"
#include "Animation/CurveSequence.h"
// FActiveTimerHandle is engine-private in 5.8; SWidget.h forward-declares it,
// and TSharedPtr's type-erased deleter makes that enough to hold and Reset()
// the handle RegisterActiveTimer returns.
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SCanvas.h"
#include "UI/BreakerSkillProjection.h"
#include "UI/BreakerUIStyle.h"
#include "UI/BreakerCharacterSheetMath.h"
#include "Combat/BreakerStatusComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Algo/Reverse.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    // ---------------------------------------------------------------------
    // FIELDPLATE. Every colour on this screen comes from BreakerUIStyle.h —
    // see Docs/Design/UI-Style-Guide-Fieldplate.md. The old local names are
    // kept as aliases so the whole file moves onto the system in one place
    // instead of a thousand call sites.
    // ---------------------------------------------------------------------
    const FLinearColor Background = BreakerUI::BgVoid;      // screen field
    const FLinearColor Panel = BreakerUI::Panel00;          // plate face
    const FLinearColor PanelRaised = BreakerUI::Panel10;    // cards, rows, slots
    const FLinearColor PanelHover = BreakerUI::Panel20;     // headers, selected
    const FLinearColor Cyan = BreakerUI::Cyan;              // player / system
    const FLinearColor Primary = BreakerUI::TextPrimary;
    const FLinearColor SoftText = BreakerUI::TextSecondary;
    const FLinearColor Muted = BreakerUI::TextMuted;
    const FLinearColor Disabled = BreakerUI::TextDisabled;
    const FLinearColor BorderRest = BreakerUI::BorderRest;
    const FLinearColor BorderEmphasis = BreakerUI::BorderEmphasis;
    const FLinearColor Harm = BreakerUI::Harm;
    const FLinearColor HarmDeep = BreakerUI::HarmDeep;
    // Reward / purchase-confirm gold. Gold is the only colour that means
    // "spend now", which is what makes scanning a tree work.
    const FLinearColor Amber = BreakerUI::Gold;
    const FLinearColor Transparent(0.0f, 0.0f, 0.0f, 0.0f);

    // ---- Fieldplate type roles --------------------------------------------
    // Archivo (display), IBM Plex Sans (body), IBM Plex Mono (numbers and
    // chrome captions): the three role fonts Scripts/import_fonts.py builds
    // from Assets/fonts into /Game/Breaker/UI/Fonts. Loaded once and ROOTED —
    // the cache outlives every widget, and a collected font under a live
    // FSlateFontInfo is a crash on the next paint. Every helper falls back to
    // the engine face when the asset is absent (a clone before its LFS pull,
    // a suite run with no content), so the menu degrades to the old look
    // rather than to tofu.
    FSlateFontInfo BreakerRoleFont(const TCHAR* AssetPath, const FName& Typeface, int32 Size,
        const TCHAR* EngineFallback, float TrackingEm = 0.0f)
    {
        static TMap<FString, UFont*> Cache;
        UFont*& Slot = Cache.FindOrAdd(FString(AssetPath));
        if (!Slot)
        {
            Slot = LoadObject<UFont>(nullptr, AssetPath);
            if (Slot) Slot->AddToRoot();
        }
        FSlateFontInfo Font = Slot
            ? FSlateFontInfo(Slot, Size, Typeface)
            : FCoreStyle::GetDefaultFontStyle(EngineFallback, Size);
        // FSlateFontInfo::LetterSpacing is 1/1000 em — the unit the pack's
        // tracking_em values state.
        Font.LetterSpacing = FMath::RoundToInt(TrackingEm * 1000.0f);
        return Font;
    }

    // Display: uppercase headings, weights 600/700, the pack's +0.01em.
    FSlateFontInfo BreakerDisplayFont(int32 Size, bool bHeavy = false)
    {
        return BreakerRoleFont(TEXT("/Game/Breaker/UI/Fonts/F_BreakerDisplay.F_BreakerDisplay"),
            bHeavy ? FName(TEXT("Bold")) : FName(TEXT("SemiBold")), Size, TEXT("Bold"), 0.01f);
    }

    // Body: mixed-case copy, 400/500/600. The bold flag maps to SemiBold —
    // the pack's body family carries no 700.
    FSlateFontInfo BreakerBodyFont(int32 Size, bool bSemiBold = false)
    {
        return BreakerRoleFont(TEXT("/Game/Breaker/UI/Fonts/F_BreakerBody.F_BreakerBody"),
            bSemiBold ? FName(TEXT("SemiBold")) : FName(TEXT("Regular")), Size,
            bSemiBold ? TEXT("Bold") : TEXT("Regular"));
    }

    // Mono: every number, key cap and tracked chrome caption. Tabular figures
    // come with the face; the tracking parameter is the caption's 0.16em.
    FSlateFontInfo BreakerMonoFont(int32 Size, float TrackingEm = 0.0f)
    {
        return BreakerRoleFont(TEXT("/Game/Breaker/UI/Fonts/F_BreakerMono.F_BreakerMono"),
            FName(TEXT("Regular")), Size, TEXT("Mono"), TrackingEm);
    }

    TSharedRef<STextBlock> BreakerMonoText(const FText& Text, int32 Size, const FLinearColor& Color,
        float TrackingEm = 0.0f)
    {
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Color)
            .Font(BreakerMonoFont(Size, TrackingEm));
    }

    TSharedRef<STextBlock> MenuText(const FText& Text, int32 Size, const FLinearColor& Color = BreakerUI::TextPrimary, bool bBold = false)
    {
        // Role by size, one seam for every screen: 20 and up is a heading
        // (TypeH2/TypeH1) and takes the display face; 14 is body and 11 is a
        // caption, both on the body face. Numbers do not come through here —
        // they are MenuValueColumn and BreakerMonoText, on the mono role.
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Color)
            .Font(Size >= BreakerUI::TypeH2 ? BreakerDisplayFont(Size, bBold) : BreakerBodyFont(Size, bBold));
    }

    // Text that WRAPS at a known pixel width instead of running past the edge
    // of its box.
    //
    // WrapTextAt, never AutoWrapText, and the distinction is the whole point:
    // AutoWrapText wraps at the widget's ALLOTTED width, which is the widget
    // measuring its own arrangement — the pattern that is banned in this file
    // because it caused a real bug. WrapTextAt takes a number the caller
    // computed before layout started, from a card width that came from the
    // viewport. The wrap only ever changes HEIGHT, and every plate on this
    // screen is now a fixed height, so nothing downstream can move.
    //
    // This is the answer to clipped text rather than a wider box, because a box
    // sized to the longest string is only possible when the longest string is
    // knowable; an affix name plus a rolled value plus a tier is not. Wrapping
    // degrades to a second line, which is readable. Clipping does not.
    TSharedRef<STextBlock> MenuWrappedText(const FText& Text, int32 Size, const FLinearColor& Color, float WrapAt,
        bool bBold = false)
    {
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Color)
            .WrapTextAt(WrapAt)
            .Font(Size >= BreakerUI::TypeH2 ? BreakerDisplayFont(Size, bBold) : BreakerBodyFont(Size, bBold));
    }

    // A number in a fixed-width column, right-aligned by JUSTIFICATION rather
    // than by the box's HAlign. Owner: "numbers are cut off in some fashion".
    // An SBox with HAlign_Right (or _Left, or _Center) arranges its child at
    // the child's DESIRED width — for an STextBlock that is its MEASURED width
    // — and Slate clips the drawn run to that same box, while measuring and
    // rasterising round independently. A value whose last glyph lands on the
    // rounding boundary gets shaved. HAlign_Fill hands the text block the whole
    // column to draw into and lets justification place the glyphs, which is the
    // fix MakeMarkerLabel already carries for the marker captions.
    TSharedRef<SWidget> MenuValueColumn(const FText& Text, float Width, int32 Size, const FLinearColor& Color)
    {
        // The mono role: every value column is a number, and the pack puts
        // ALL numbers on IBM Plex Mono with tabular figures.
        return SNew(SBox).WidthOverride(Width).HAlign(HAlign_Fill)
        [
            SNew(STextBlock)
                .Text(Text)
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Color)
                .Font(BreakerMonoFont(Size))
        ];
    }

    TSharedRef<SWidget> SolidBlock(const FLinearColor& Color)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Color)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    // 1px ring around a control. Buttons in this system are a fill plus a
    // border; Slate's button brush has no border, so it gets one here.
    TSharedRef<SWidget> BorderWrap(const TSharedRef<SWidget>& Inner, const FLinearColor& BorderColor, float Thickness = BreakerUI::BorderThin)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BorderColor)
            .Padding(FMargin(Thickness))
            [
                Inner
            ];
    }

    // A plate: flat face, 1px border, one 3px rail full-bleed to the edge.
    // FIELDPLATE 03 — the rail is the signature, and one plate never carries
    // two of them. RailEdge Left is identity, Top is transient status.
    TSharedRef<SWidget> MakePlate(const TSharedRef<SWidget>& Content, const FLinearColor& Face, const FLinearColor& Rail,
        const FMargin& ContentPadding = FMargin(16.0f, 12.0f), bool bTopRail = false,
        const FLinearColor& BorderColor = BreakerUI::BorderRest)
    {
        TSharedRef<SWidget> Face2 = SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Face)
            .Padding(ContentPadding)
            [
                Content
            ];

        TSharedRef<SWidget> Railed = bTopRail
            ? StaticCastSharedRef<SWidget>(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(BreakerUI::RailThickness)[SolidBlock(Rail)]]
                + SVerticalBox::Slot().FillHeight(1.0f)[Face2])
            : StaticCastSharedRef<SWidget>(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(BreakerUI::RailThickness)[SolidBlock(Rail)]]
                + SHorizontalBox::Slot().FillWidth(1.0f)[Face2]);

        // The 1px border is the outermost ring: borders carry depth in this
        // system, gradients do not exist.
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BorderColor)
            .Padding(FMargin(BreakerUI::BorderThin))
            [
                Railed
            ];
    }

    // ---------------------------------------------------------------------
    // Path-board primitives.
    //
    // The skill matrix board is drawn on an SCanvas at fixed pixel positions
    // (FIELDPLATE authors at 1920x1080). Nothing on the board measures itself
    // against its allotted size, so there is no layout feedback loop of the
    // kind SWrapBox/UseAllottedSize produced inside a scroll box.
    // ---------------------------------------------------------------------

    // A dashed hairline. Slate has no dash pattern, so it is a fixed run of
    // blocks — the count comes from the caller's pixel width, never from an
    // allotted size.
    TSharedRef<SWidget> DashedLine(float Width, const FLinearColor& Color, float Dash = 6.0f, float Gap = 6.0f)
    {
        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        const int32 Count = FMath::Clamp(FMath::CeilToInt(Width / (Dash + Gap)), 1, 240);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Gap, 0.0f)
            [
                SNew(SBox).WidthOverride(Dash)[SolidBlock(Color)]
            ];
        }
        return Row;
    }

    // A straight 2px segment between two board points, drawn as a bar rotated
    // about its own centre. Trunks pass A/B on the same X; diagonals do not.
    void AddCanvasSegment(const TSharedRef<SCanvas>& Canvas, const FVector2D& A, const FVector2D& B,
        const FLinearColor& Color, float Thickness = 2.0f)
    {
        const FVector2D Delta = B - A;
        const float Length = FMath::Max(1.0f, static_cast<float>(Delta.Size()));
        const float Angle = FMath::Atan2(static_cast<float>(Delta.Y), static_cast<float>(Delta.X));
        const FVector2D Mid = (A + B) * 0.5;
        Canvas->AddSlot()
            .Position(FVector2D(Mid.X - Length * 0.5f, Mid.Y - Thickness * 0.5f))
            .Size(FVector2D(Length, Thickness))
            [
                SNew(SBox)
                .RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(Angle))))
                .RenderTransformPivot(FVector2D(0.5, 0.5))
                [
                    SolidBlock(Color)
                ]
            ];
    }

    // ---------------------------------------------------------------------
    // PLATE GEOMETRY PROBE — instrumentation for the reported screen jitter.
    //
    // Two fixes have now been guessed at and neither held, so this stops
    // guessing and MEASURES. Wrapped around the screen plate in BuildFrame and
    // BuildZonedFrame, it logs the plate's DESIRED size (what the content asks
    // for) and its ARRANGED size (what Slate gave it) whenever either moves.
    //
    // Reading the log is the whole point, and the two outcomes are different
    // bugs with different fixes:
    //
    //  * [MenuGeom] lines that appear only just after a [MenuRebuild] line, one
    //    per rebuild — the plate is a different size on each build and, being
    //    centred, lands in a different place. That is a REBUILD-TIME jitter and
    //    the fix is a plate whose size does not come from its content.
    //  * [MenuGeom] lines with ADVANCING frame numbers and no rebuild between
    //    them — a genuine per-frame layout oscillation, i.e. something reads its
    //    own arrangement. The frame delta and the two sizes name which axis.
    //
    // Cost is a float compare per paint and nothing at all when the geometry is
    // steady, which is the state this screen is supposed to be in.
    // ---------------------------------------------------------------------
    class SBreakerPlateProbe : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SBreakerPlateProbe) {}
            SLATE_ARGUMENT(FString, Label)
            SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            Label = InArgs._Label;
            ChildSlot
            [
                InArgs._Content.Widget
            ];
        }

        virtual FVector2D ComputeDesiredSize(float LayoutScale) const override
        {
            const FVector2D Desired = SCompoundWidget::ComputeDesiredSize(LayoutScale);
            Report(Desired, LastArranged);
            return Desired;
        }

        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
            bool bParentEnabled) const override
        {
            Report(LastDesired, AllottedGeometry.GetLocalSize());
            return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
                InWidgetStyle, bParentEnabled);
        }

    private:
        // Half a pixel: below that it is measurement rounding, not motion.
        static constexpr float MotionEpsilon = 0.5f;

        void Report(const FVector2D& Desired, const FVector2D& Arranged) const
        {
            const bool bMoved =
                FMath::Abs(Desired.X - LastDesired.X) > MotionEpsilon ||
                FMath::Abs(Desired.Y - LastDesired.Y) > MotionEpsilon ||
                FMath::Abs(Arranged.X - LastArranged.X) > MotionEpsilon ||
                FMath::Abs(Arranged.Y - LastArranged.Y) > MotionEpsilon;
            if (!bMoved && bReported) return;

            LastDesired = Desired;
            LastArranged = Arranged;
            ++ChangeCount;
            bReported = true;
            UE_LOG(LogTemp, Log,
                TEXT("[MenuGeom] %s desired=%.1fx%.1f arranged=%.1fx%.1f change=%d frame=%llu dframes=%llu"),
                *Label, Desired.X, Desired.Y, Arranged.X, Arranged.Y, ChangeCount,
                static_cast<uint64>(GFrameCounter), static_cast<uint64>(GFrameCounter - LastFrame));
            LastFrame = GFrameCounter;
        }

        FString Label;
        mutable FVector2D LastDesired = FVector2D::ZeroVector;
        mutable FVector2D LastArranged = FVector2D::ZeroVector;
        mutable uint64 LastFrame = 0;
        mutable int32 ChangeCount = 0;
        mutable bool bReported = false;
    };

    // ---------------------------------------------------------------------
    // Screen metrics.
    //
    // The skill matrix used to be authored at a hard 1760x1000 and simply
    // fell off the edge of anything smaller — which is a PIE window at its
    // default size, i.e. the way the owner actually sees it. The panel now
    // sizes itself from the VIEWPORT, read once here.
    //
    // This is NOT the banned "size off my allotted width" pattern: nothing
    // measures its own arrangement, so there is no layout feedback loop. It is
    // the same rule ABreakerPlaytestHUD already follows (spec pixels scaled by
    // the viewport), and it is sampled once per Rebuild — an event — never
    // from a paint attribute or a tick.
    // ---------------------------------------------------------------------
    // -----------------------------------------------------------------------
    // Chip packing.
    //
    // A chip row that does not fit is the defect this exists to kill: the
    // loadout's nine slot-filter chips ran off the right edge of a 1920 screen
    // and printed straight through the input legend beside them, because they
    // sat in an SHorizontalBox FillWidth slot whose children overflow rather
    // than wrap.
    //
    // MeasureChipWidth measures TEXT against a FONT. That is emphatically NOT
    // the banned pattern: SWrapBox with UseAllottedSize measures a widget
    // against its own arrangement, which feeds the arrangement back into
    // itself and oscillates. Nothing here reads an allotted size, so the row
    // count is arithmetic on numbers known before layout starts — the same
    // rule the Core cluster grids already follow.
    // -----------------------------------------------------------------------
    float MeasureChipWidth(const FString& Label, float HorizontalPad, float BorderThickness)
    {
        // Fallback for headless runs, where Slate has no renderer and nothing
        // is on screen anyway. Deliberately generous so a headless layout errs
        // toward more rows rather than an overflowing one.
        float TextWidth = Label.Len() * (BreakerUI::TypeCaption * 0.68f);
        if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
        {
            const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption);
            TextWidth = static_cast<float>(Measure->Measure(Label, Font).X);
        }
        // SButton draws its own style padding UNDER the ContentPadding we ask
        // for, so a chip is always wider than text + our own margins. MEASURED
        // from a 1920 capture rather than derived: the first pass at this
        // arithmetic under-counted by ~32px per chip and the eight-chip row
        // still ran through the plate edge. Erring high only costs an extra
        // row, and the bar that holds these is auto-height.
        constexpr float ButtonChromeAllowance = 32.0f;
        return TextWidth + 2.0f * HorizontalPad + 2.0f * BorderThickness + ButtonChromeAllowance;
    }

    // Lays chips out across as many rows as MaxWidth needs. One chip wider
    // than the whole row still gets its own row rather than being dropped.
    TSharedRef<SWidget> PackChipRows(const TArray<TSharedRef<SWidget>>& Chips, const TArray<float>& Widths,
        float MaxWidth, float Gap)
    {
        TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
        TSharedPtr<SHorizontalBox> Row;
        float Used = 0.0f;
        for (int32 Index = 0; Index < Chips.Num(); ++Index)
        {
            const float Width = Widths.IsValidIndex(Index) ? Widths[Index] + Gap : Gap;
            if (!Row.IsValid() || (Used + Width > MaxWidth && Used > 0.0f))
            {
                Row = SNew(SHorizontalBox);
                Rows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)[Row.ToSharedRef()];
                Used = 0.0f;
            }
            Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Gap, 0.0f)[Chips[Index]];
            Used += Width;
        }
        return Rows;
    }

    struct FWideScreenMetrics
    {
        float PanelWidth = 1760.0f;
        float PanelHeight = 1000.0f;
        // The fixed detail rail. Two steps only, so it is a layout constant on
        // any given screen and the board can never reflow when the rail fills.
        float RailWidth = 420.0f;
        // Usable width of the board viewport, scrollbar allowance removed.
        float BoardViewWidth = 1300.0f;
    };

    // Shared by BOTH wide screens. The loadout used to hard-code 1760 and
    // simply run off the edge of anything narrower; the skill matrix learned
    // that lesson first, and there is no reason for two answers to one
    // question.
    FWideScreenMetrics MeasureWideScreen()
    {
        FWideScreenMetrics Metrics;

        FVector2D Viewport(1920.0f, 1080.0f);
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->GetViewportSize(Viewport);
        }
        if (Viewport.X < 640.0f || Viewport.Y < 360.0f) Viewport = FVector2D(1920.0f, 1080.0f);

        // Space40 screen margin on each side, as the style guide asks, and the
        // 1760 authored width as the ceiling.
        Metrics.PanelWidth = FMath::Clamp(static_cast<float>(Viewport.X) - 2.0f * BreakerUI::Space40, 720.0f, 1760.0f);
        Metrics.PanelHeight = FMath::Clamp(static_cast<float>(Viewport.Y) - 2.0f * BreakerUI::Space40, 420.0f, 1000.0f);
        Metrics.RailWidth = Metrics.PanelWidth >= 1360.0f ? 420.0f : 320.0f;
        // Panel minus the rail, the gutter between them, and the scroll bar.
        Metrics.BoardViewWidth = FMath::Max(320.0f,
            Metrics.PanelWidth - Metrics.RailWidth - BreakerUI::Space24 - 20.0f);

        // Jitter instrumentation, and this is the SUSPECT half of it: every
        // panel on both wide screens is sized from this one reading. If
        // GetViewportSize ever returns a different value on alternate frames
        // — DPI, a resize settling, a letterbox recalculating — then every
        // plate derived from it breathes, and the log says so in one line
        // rather than being inferred from a screenshot. Logged on CHANGE only,
        // so a steady viewport prints once per session.
        static FVector2D LastViewport = FVector2D::ZeroVector;
        if (!Viewport.Equals(LastViewport, 0.5))
        {
            LastViewport = Viewport;
            UE_LOG(LogTemp, Log, TEXT("[MenuGeom] viewport=%.1fx%.1f panel=%.1fx%.1f frame=%llu"),
                Viewport.X, Viewport.Y, Metrics.PanelWidth, Metrics.PanelHeight,
                static_cast<uint64>(GFrameCounter));
        }
        return Metrics;
    }

    // -----------------------------------------------------------------------
    // The board viewport: zoom and pan for the skill matrix.
    //
    // The board used to be an SCanvas of fixed pixel positions inside a
    // horizontal SScrollBox inside a vertical one. Two scroll boxes on one
    // surface is what "scrolling is off by a little bit" felt like: a wheel
    // event landed in whichever of the two claimed it first, and the pair
    // fight over the same gesture. Both are gone. This widget is the ONE way
    // the board moves.
    //
    // How it works, and why it cannot become the per-frame-rebuild trap this
    // project has been bitten by twice:
    //
    //  * The board is laid out ONCE, at its full authored pixel size, by
    //    OnArrangeChildren handing the child exactly BoardSize regardless of
    //    how much room this widget has. Nothing measures its own arrangement,
    //    so there is no layout feedback loop.
    //  * Zoom and pan are a RENDER transform on that already-arranged child,
    //    set imperatively from the input handlers. Painting a transform is
    //    free; no widget is rebuilt, no attribute is polled, and no text is
    //    re-laid-out at a new size, so zooming cannot reflow the board.
    //  * Slate carries the render transform through the hit-test grid, so the
    //    marker buttons stay hoverable and clickable at every zoom level, and
    //    hover is what populates the detail rail.
    //  * The rail is a sibling column of fixed width and this widget reports a
    //    desired size of zero, so nothing here can push the rail or resize it.
    // -----------------------------------------------------------------------
    DECLARE_DELEGATE_TwoParams(FOnBoardViewChanged, float /*Zoom*/, FVector2D /*Pan*/);

    class SBreakerBoardViewport : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SBreakerBoardViewport)
            : _BoardSize(FVector2D(1000.0f, 800.0f))
            , _InitialZoom(1.0f)
            , _InitialPan(FVector2D::ZeroVector)
            {}
            SLATE_ARGUMENT(FVector2D, BoardSize)
            SLATE_ARGUMENT(float, InitialZoom)
            SLATE_ARGUMENT(FVector2D, InitialPan)
            SLATE_EVENT(FOnBoardViewChanged, OnViewChanged)
            SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()

        // Zoom limits. Below MinZoom the 11px caption floor stops being
        // readable at all, so there is no point offering it; above MaxZoom a
        // 48px marker is bigger than a HUD ability square and the board stops
        // being a map.
        static constexpr float MinZoom = 0.5f;
        static constexpr float MaxZoom = 2.0f;
        // One wheel notch. 1.15 is roughly seven notches across the whole
        // range, which is enough travel to feel continuous and few enough that
        // a player can get back to 1.00 by counting.
        static constexpr float ZoomStep = 1.15f;

        void Construct(const FArguments& InArgs)
        {
            BoardSize = InArgs._BoardSize;
            DefaultZoom = FMath::Clamp(InArgs._InitialZoom, MinZoom, MaxZoom);
            Zoom = DefaultZoom;
            Pan = InArgs._InitialPan;
            OnViewChanged = InArgs._OnViewChanged;

            // The viewport is a window onto a larger board, so it must clip.
            SetClipping(EWidgetClipping::ClipToBounds);
            ChildSlot
            [
                InArgs._Content.Widget
            ];
            ApplyTransform();
        }

        float GetZoom() const { return Zoom; }

        // The two button controls. They zoom about the middle of the viewport,
        // which is the only sensible anchor when the gesture did not come from
        // a cursor position.
        void StepZoom(float Notches)
        {
            const FVector2D View = GetViewSize();
            ZoomAbout(View * 0.5f, Notches);
        }

        // Back to the view the board OPENED on, which for a board wider than
        // its window is the zoom that fits it — not a bare 1.00, which would
        // "reset" a COMPARE ALL board to a state where a third of it is off
        // the right edge.
        void ResetView()
        {
            Zoom = DefaultZoom;
            Pan = FVector2D::ZeroVector;
            ClampPan();
            ApplyTransform();
            Notify();
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            // Take whatever the parent column gives. A desired size here would
            // let the board argue with the fixed detail rail beside it.
            return FVector2D::ZeroVector;
        }

        virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
        {
            const TSharedRef<SWidget>& Child = ChildSlot.GetWidget();
            if (ArrangedChildren.Accepts(Child->GetVisibility()))
            {
                // Full authored size, always. Clamping the board to the
                // viewport is what a scroll box does, and it is what made the
                // board's own geometry depend on the window.
                ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Child, BoardSize, FSlateLayoutTransform()));
            }
        }

        virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            // About the CURSOR, never about the origin. Zooming about the
            // origin is what makes a board feel like it is fighting you: the
            // thing you were looking at slides away from under the pointer.
            ZoomAbout(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), MouseEvent.GetWheelDelta());
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            const FKey Button = MouseEvent.GetEffectingButton();
            if (Button != EKeys::LeftMouseButton && Button != EKeys::MiddleMouseButton)
            {
                return FReply::Unhandled();
            }
            // A press only reaches this widget when no marker took it first —
            // SButton handles its own press and captures the mouse — so
            // left-drag pans the empty board without ever stealing a purchase.
            bDragging = true;
            DragOrigin = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            PanOrigin = Pan;
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }

        virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            if (!bDragging || !HasMouseCapture()) return FReply::Unhandled();
            Pan = PanOrigin + (MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()) - DragOrigin);
            ClampPan();
            ApplyTransform();
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override
        {
            if (!bDragging) return FReply::Unhandled();
            bDragging = false;
            Notify();
            return FReply::Handled().ReleaseMouseCapture();
        }

        virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override
        {
            bDragging = false;
        }

        virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override
        {
            return bDragging ? FCursorReply::Cursor(EMouseCursor::GrabHandClosed) : FCursorReply::Unhandled();
        }

    private:
        FVector2D GetViewSize() const
        {
            const FVector2D View = GetTickSpaceGeometry().GetLocalSize();
            return FVector2D(FMath::Max(1.0f, static_cast<float>(View.X)), FMath::Max(1.0f, static_cast<float>(View.Y)));
        }

        void ZoomAbout(const FVector2D& LocalPoint, float Notches)
        {
            const float Previous = Zoom;
            Zoom = FMath::Clamp(Zoom * FMath::Pow(ZoomStep, Notches), MinZoom, MaxZoom);
            if (FMath::IsNearlyEqual(Previous, Zoom)) return;
            // Keep the board point under LocalPoint exactly where it is:
            //   board = (Local - Pan) / Previous, and Pan' = Local - Zoom*board.
            Pan = LocalPoint - (LocalPoint - Pan) * (Zoom / Previous);
            ClampPan();
            ApplyTransform();
            Notify();
        }

        void ClampPan()
        {
            const FVector2D View = GetViewSize();
            const FVector2D Scaled = BoardSize * Zoom;
            auto ClampAxis = [](float Offset, float Content, float Visible)
            {
                // Smaller than the window: pinned inside it. Larger: the edges
                // may not be dragged past the window, so the board can never
                // be flung somewhere the player cannot find it again.
                return Content <= Visible
                    ? FMath::Clamp(Offset, 0.0f, Visible - Content)
                    : FMath::Clamp(Offset, Visible - Content, 0.0f);
            };
            Pan.X = ClampAxis(static_cast<float>(Pan.X), static_cast<float>(Scaled.X), static_cast<float>(View.X));
            Pan.Y = ClampAxis(static_cast<float>(Pan.Y), static_cast<float>(Scaled.Y), static_cast<float>(View.Y));
        }

        void ApplyTransform()
        {
            const TSharedRef<SWidget>& Child = ChildSlot.GetWidget();
            Child->SetRenderTransformPivot(FVector2D::ZeroVector);
            Child->SetRenderTransform(TOptional<FSlateRenderTransform>(
                FSlateRenderTransform(FScale2D(Zoom, Zoom), Pan)));
        }

        void Notify()
        {
            OnViewChanged.ExecuteIfBound(Zoom, Pan);
        }

        FVector2D BoardSize = FVector2D(1000.0f, 800.0f);
        FVector2D Pan = FVector2D::ZeroVector;
        FVector2D PanOrigin = FVector2D::ZeroVector;
        FVector2D DragOrigin = FVector2D::ZeroVector;
        float Zoom = 1.0f;
        float DefaultZoom = 1.0f;
        bool bDragging = false;
        FOnBoardViewChanged OnViewChanged;
    };

    // A board always OPENS at 1:1, even when it is wider than the window.
    //
    // Opening at a fit-to-width zoom was tried and photographed: COMPARE ALL
    // is about 2600px of board in a 1300px column, so fitting it means 0.5x,
    // and 0.5x of the 11px caption floor is 5px of unreadable type — FIELDPLATE
    // 02 says never below 11px and it means it. Zooming out to find your
    // bearings is a deliberate act the player takes; it is not a state to hand
    // them on arrival. RESET VIEW returns here.
    inline constexpr float BoardOpeningZoom = 1.0f;

    // The board's view controls. The wheel and the drag are the real verbs;
    // these exist because a gesture nobody knows about is not a feature, and
    // because a trackpad without a wheel still has to be able to zoom.
    TSharedRef<SWidget> MakeBoardViewControls(const TSharedPtr<SBreakerBoardViewport>& Viewport)
    {
        TWeakPtr<SBreakerBoardViewport> Weak = Viewport;
        auto MakeStep = [Weak](const FString& Label, float Notches) -> TSharedRef<SWidget>
        {
            return BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space4))
                .OnClicked(FOnClicked::CreateLambda([Weak, Notches]()
                {
                    if (const TSharedPtr<SBreakerBoardViewport> View = Weak.Pin())
                    {
                        if (Notches == 0.0f) View->ResetView();
                        else View->StepZoom(Notches);
                    }
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, true)
                ],
                BorderEmphasis);
        };

        return MakePlate(
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(TEXT("VIEW")), BreakerUI::TypeCaption, Muted, true)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, BreakerUI::Space8, 0.0f)
            [
                MakeStep(TEXT("-"), -1.0f)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                MakeStep(TEXT("+"), 1.0f)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                MakeStep(TEXT("RESET VIEW"), 0.0f)
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).HAlign(HAlign_Right)
            [
                // Clipped, because an SHorizontalBox draws an oversized child
                // straight through its neighbour rather than shrinking it.
                SNew(SBox).Clipping(EWidgetClipping::ClipToBounds).HAlign(HAlign_Right)
                [
                    MenuText(FText::FromString(TEXT("WHEEL ZOOMS AT THE CURSOR  ·  DRAG THE BOARD TO PAN")),
                        BreakerUI::TypeCaption, Muted, true)
                ]
            ],
            BreakerUI::BgRaised, BorderEmphasis, FMargin(BreakerUI::Space16, BreakerUI::Space4));
    }

    // Diamond markers are square markers turned 45 degrees. The rotation is a
    // render transform, so the layout box stays axis-aligned and the board
    // geometry stays trivially predictable.
    TSharedRef<SWidget> RotateFortyFive(const TSharedRef<SWidget>& Inner)
    {
        return SNew(SBox)
            .RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.0f)))))
            .RenderTransformPivot(FVector2D(0.5, 0.5))
            [
                Inner
            ];
    }
}

void SBreakerMenu::Construct(const FArguments& InArgs)
{
    Character = InArgs._Character;
    ChildSlot
    [
        SAssignNew(ContentHost, SBox)
    ];
    ShowMainMenu();
}

void SBreakerMenu::ShowMainMenu()
{
    RootScreen = EBreakerMenuScreen::Main;
    Rebuild(EBreakerMenuScreen::Main);
}

void SBreakerMenu::ShowPauseMenu()
{
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Pause);
}

void SBreakerMenu::ShowInventory()
{
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Inventory);
}

void SBreakerMenu::ShowCharacterSheet()
{
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::CharacterSheet);
}

void SBreakerMenu::ShowDialogue(ABreakerNPC* NPC)
{
    DialogueNPC = NPC;
    // Per-NPC entry state: which node an NPC opens on depends on what the
    // player has done. This used to be an unconditional GetStartNodeId(), so
    // every NPC greeted the player identically forever.
    const UBreakerQuestJournal* Journal = Character.IsValid() ? Character->GetQuestJournal() : nullptr;
    static const FBreakerQuestFlagSet EmptyFlags;
    DialogueNodeId = NPC ? NPC->ResolveStartNodeId(Journal ? Journal->GetState() : EmptyFlags) : NAME_None;
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Dialogue);
}

void SBreakerMenu::ShowTravel(ABreakerTravelPoint* InTravelPoint)
{
    TravelPoint = InTravelPoint;
    TravelStatus = FText::GetEmpty();
    // Open with the first available destination marked. The screen is never
    // shown with nothing selected — a picker whose selected state only appears
    // after you have already clicked something is telling you what you just
    // did, not what you are about to do.
    SelectedTravelDestinationId = NAME_None;
    if (InTravelPoint)
    {
        const TArray<FBreakerTravelDestination> Available = InTravelPoint->GetAvailableDestinations();
        if (Available.Num() > 0) SelectedTravelDestinationId = Available[0].Id;
    }
    // Pause, not Main, and for the same reason ShowDialogue does it: this
    // screen is entered from gameplay by walking into a thing and pressing F,
    // so "back" from here is the paused game, never the title.
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Travel);
}

void SBreakerMenu::HandleEscape()
{
    // A listening keybind row eats Escape before the screen does. Escape
    // reaches this widget by TWO independent routes — Slate preview
    // (OnPreviewKeyDown) and the player-input BindKey that has always driven
    // the pause toggle (Characters/BreakerCharacter.cpp:343) — and only one of
    // them is guaranteed to fire. Both cancel, so cancelling works whichever
    // one wins; and without this guard the OTHER outcome is the worst one on
    // the screen: Escape leaves settings entirely while a row is still armed.
    if (ListeningKeybindAction != NAME_None)
    {
        CancelKeybindListen();
        KeybindStatus = FText::FromString(TEXT("REBIND CANCELLED."));
        bKeybindStatusIsClash = false;
        Rebuild(EBreakerMenuScreen::Settings);
        return;
    }
    // Both world-interactable screens leave the same way: back to the game,
    // having done nothing. Escape on the travel screen must NOT travel — it is
    // the "I walked up to the wrong thing" key, and a picker that departed on
    // the way out would be the single worst button in the game.
    if (CurrentScreen == EBreakerMenuScreen::Dialogue || CurrentScreen == EBreakerMenuScreen::Travel)
    {
        if (Character.IsValid()) Character->ResumeFromMenu();
        return;
    }
    // CharacterCreate backs out to CharacterSelect rather than to the root:
    // it is a step INSIDE character selection, and dropping the player all the
    // way to the title from a half-filled create form loses their work with no
    // warning.
    if (CurrentScreen == EBreakerMenuScreen::CharacterCreate)
    {
        CharacterScreenStatus = FText::GetEmpty();
        Rebuild(EBreakerMenuScreen::CharacterSelect);
        return;
    }
    // Every screen that is a leaf off the root backs out to the root. The two
    // character screens were MISSING from this list when they were added, so
    // Escape on them did nothing at all — a dead end on the one screen a new
    // player cannot avoid.
    if (CurrentScreen == EBreakerMenuScreen::Settings || CurrentScreen == EBreakerMenuScreen::Inventory || CurrentScreen == EBreakerMenuScreen::ClassSelect || CurrentScreen == EBreakerMenuScreen::SkillTrees || CurrentScreen == EBreakerMenuScreen::Forge || CurrentScreen == EBreakerMenuScreen::Abilities || CurrentScreen == EBreakerMenuScreen::Quartermaster || CurrentScreen == EBreakerMenuScreen::CharacterSelect || CurrentScreen == EBreakerMenuScreen::DevSandbox)
    {
        Rebuild(RootScreen);
    }
    else if (CurrentScreen == EBreakerMenuScreen::Pause && Character.IsValid())
    {
        Character->ResumeFromMenu();
    }
}

void SBreakerMenu::ShowScreenForCapture(EBreakerMenuScreen Screen)
{
    // Dev capture only, and a command-line switch by construction, so a
    // shipped build cannot reach it. It exists because "capture SKILLTREES"
    // photographed exactly one of that screen's three boards, and the two it
    // skipped are the two nobody has ever looked at.
    FString Board;
    if (FParse::Value(FCommandLine::Get(), TEXT("BreakerCaptureBoard="), Board))
    {
        Board = Board.ToUpper();
        if (Board == TEXT("CORE")) { SkillBoardTab = 1; }
        else if (Board == TEXT("COMPARE")) { SkillBoardTab = 0; SkillBranchIndex = -1; }
        else if (Board.StartsWith(TEXT("BRANCH"))) { SkillBoardTab = 0; SkillBranchIndex = FCString::Atoi(*Board.RightChop(6)); }
        // FORGE and ABILITIES have no -BreakerCaptureMenu= string of their own
        // (that mapping lives in Characters/BreakerCharacter.cpp, out of this
        // pass's territory); reuse the existing sub-view switch instead, e.g.
        // -BreakerCaptureMenu=INVENTORY -BreakerCaptureBoard=FORGE. Harmless
        // for the skill screen: neither string matches CORE/COMPARE/BRANCH*.
        else if (Board == TEXT("FORGE")) { Screen = EBreakerMenuScreen::Forge; }
        else if (Board == TEXT("ABILITIES")) { Screen = EBreakerMenuScreen::Abilities; }
        // Settings panes, same pattern: the sidebar shows one pane at a time,
        // so a capture run that says only SETTINGS photographs INPUT and the
        // other three panes ship unlooked-at, e.g.
        // -BreakerCaptureMenu=SETTINGS -BreakerCaptureBoard=KEYBINDS.
        else if (Board == TEXT("INPUT")) { SettingsPane = 0; }
        else if (Board == TEXT("KEYBINDS")) { SettingsPane = 1; }
        else if (Board == TEXT("VIDEO")) { SettingsPane = 2; }
        else if (Board == TEXT("AUDIO")) { SettingsPane = 3; }
    }
    Rebuild(Screen);
}

void SBreakerMenu::HandleBoardViewChanged(float NewZoom, FVector2D NewPan)
{
    SkillBoardZoom = NewZoom;
    SkillBoardPan = NewPan;
}

void SBreakerMenu::ResetBoardView()
{
    SkillBoardZoom = 0.0f;
    SkillBoardPan = FVector2D::ZeroVector;
}

void SBreakerMenu::Rebuild(EBreakerMenuScreen NewScreen)
{
    // Diagnostic for the reported screen flip-flop: every transition is
    // logged with a timestamp so a repro session shows exactly what drives
    // the loop. Cheap enough to leave in during playtests.
    UE_LOG(LogTemp, Log, TEXT("[MenuRebuild] %d -> %d at %.3f"),
        static_cast<int32>(CurrentScreen), static_cast<int32>(NewScreen),
        FPlatformTime::Seconds());
    // Flip-flop diagnosis: when the transition is between Inventory and
    // SkillTrees, dump the callstack so the log names the caller.
    if ((CurrentScreen == EBreakerMenuScreen::Inventory && NewScreen == EBreakerMenuScreen::SkillTrees) ||
        (CurrentScreen == EBreakerMenuScreen::SkillTrees && NewScreen == EBreakerMenuScreen::Inventory))
    {
        ANSICHAR StackTrace[4096];
        StackTrace[0] = 0;
        FPlatformStackWalk::StackWalkAndDump(StackTrace, UE_ARRAY_COUNT(StackTrace), 1);
        UE_LOG(LogTemp, Log, TEXT("[MenuRebuild] caller:\n%hs"), StackTrace);
    }

    // WHICH SCREEN WE ARE ON UPDATES NOW; only the WIDGET SWAP is deferred.
    // These were both deferred, and that is the reported "snaps to other menus
    // or flickers to them" bug: between the click and the next Slate tick,
    // CurrentScreen still named the screen we had just left, so every reader
    // branched on stale state. HandleEscape is the worst of them — pressing
    // Escape in that window took the branch for the PREVIOUS screen and sent
    // the player somewhere neither screen would have gone. Separating the two
    // costs nothing (an enum assignment cannot destroy a widget) and removes
    // the whole class of bug rather than one instance of it.
    CurrentScreen = NewScreen;
    PendingScreen = NewScreen;
    if (!bRebuildScheduled)
    {
        bRebuildScheduled = true;
        RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda(
            [this](double, float) -> EActiveTimerReturnType
            {
                bRebuildScheduled = false;
                ApplyScreen(PendingScreen);
                return EActiveTimerReturnType::Stop;
            }));
    }
}

void SBreakerMenu::ApplyScreen(EBreakerMenuScreen NewScreen)
{
    CurrentScreen = NewScreen;
    // Consume the one-shot cleanup arm: only the rebuild triggered by the
    // arming click sees it, everything else disarms.
    CleanupArmedIndex = PendingCleanupArm;
    PendingCleanupArm = -1;
    // A confirmation modal belongs to the screen that raised it; leaving the
    // screen answers it with "no".
    if (CurrentScreen != EBreakerMenuScreen::Inventory) DiscardModalIndex = -1;
    // Same rule for the travel refusal line: it describes one screen's last
    // click and means nothing anywhere else.
    if (CurrentScreen != EBreakerMenuScreen::Travel) TravelStatus = FText::GetEmpty();
    // Same rule for the sandbox's result line: it reports one screen's last
    // click and means nothing anywhere else.
    if (CurrentScreen != EBreakerMenuScreen::DevSandbox) DevSandboxStatus = FText::GetEmpty();
    // Same rule for the rebind flow: a "press a key" state belongs to the
    // settings screen, and leaving it answers the prompt with "never mind".
    // Without this, a row left listening would keep swallowing every keypress
    // and every mouse click on whatever screen came next.
    if (CurrentScreen != EBreakerMenuScreen::Settings)
    {
        CancelKeybindListen();
        KeybindStatus = FText::GetEmpty();
        bKeybindStatusIsClash = false;
        // Dropped on the way OUT so the next entry re-reads the ini. Sensitivity
        // and FOV have a SECOND writer — ABreakerCharacter's keyboard nudges
        // (BreakerCharacter.cpp:944-947) write the same three legacy keys this
        // model loads — so a model cached across a play session would show a
        // stale FOV on the slider the moment anyone used those keys. Everything
        // this screen changes is already saved before any screen change, so
        // there is nothing to lose by re-reading. DefaultKeybinds is
        // deliberately NOT dropped: it describes an asset, not a player value.
        GameSettings.Reset();
    }
    if (!ContentHost.IsValid()) return;
    switch (CurrentScreen)
    {
        case EBreakerMenuScreen::Pause: ContentHost->SetContent(BuildPauseScreen()); break;
        case EBreakerMenuScreen::Settings: ContentHost->SetContent(BuildSettingsScreen()); break;
        // Loadout is RETIRED (owner ruling 2026-08-17: "the loadout button and
        // the ability to just pick a weapon shouldnt exist"). Equipment IS the
        // loadout: which gun you hold comes from the item in your Primary/
        // Secondary slot (SyncArchetypesToEquipment), never from a picker. A
        // stale request for the value falls through to the default arm below.
        case EBreakerMenuScreen::Inventory: ContentHost->SetContent(BuildInventoryScreen()); break;
        case EBreakerMenuScreen::ClassSelect: ContentHost->SetContent(BuildClassSelectScreen()); break;
        case EBreakerMenuScreen::CharacterSelect: ContentHost->SetContent(BuildCharacterSelectScreen()); break;
        case EBreakerMenuScreen::CharacterCreate: ContentHost->SetContent(BuildCharacterCreateScreen()); break;
        case EBreakerMenuScreen::SkillTrees: ContentHost->SetContent(BuildSkillTreesScreen()); break;
        case EBreakerMenuScreen::Forge: ContentHost->SetContent(BuildForgeScreen()); break;
        case EBreakerMenuScreen::Quartermaster: ContentHost->SetContent(BuildQuartermasterScreen()); break;
        case EBreakerMenuScreen::Abilities: ContentHost->SetContent(BuildAbilitiesScreen()); break;
        case EBreakerMenuScreen::Dialogue: ContentHost->SetContent(BuildDialogueScreen()); break;
        case EBreakerMenuScreen::Travel: ContentHost->SetContent(BuildTravelScreen()); break;
        case EBreakerMenuScreen::DevSandbox: ContentHost->SetContent(BuildDevSandboxScreen()); break;
        case EBreakerMenuScreen::CharacterSheet: ContentHost->SetContent(BuildCharacterSheetScreen()); break;
        default: ContentHost->SetContent(BuildMainScreen()); break;
    }

    // THE ONE-FRAME BLANK, owner report 2026-08-16: "when clicking any menu
    // button the screen flashes ... for a fraction of a second (like a menu
    // rebuild bug)". This function runs from Rebuild's active timer, and Slate
    // executes active timers INSIDE SWidget::Paint (SWidget.cpp, the
    // NeedsActiveTimerUpdate block) — which is AFTER this frame's prepass has
    // already measured the tree. So the screen built above arrived with NO
    // cached desired size, and BuildFrame's plate sits in an SOverlay slot
    // that centres its child at DESIRED size: the plate was arranged at 0x0
    // and the menu painted one frame of bare background before the next
    // frame's prepass measured it. The probe log proves the shape — every
    // single rebuild in the owner's session logs
    //   [MenuGeom] BuildFrame desired=0.0x0.0 arranged=0.0x0.0   (click frame)
    //   [MenuGeom] BuildFrame desired=WxH   arranged=WxH         (frame after)
    // Prepassing the fresh tree here, before the paint pass descends into it,
    // closes the gap: the same-frame arrangement sees real desired sizes, so a
    // transition never presents an intermediate frame. The scale is this
    // widget's own cached layout scale (DPI), 1.0 on the first-ever apply when
    // nothing has painted yet.
    const float LayoutScale = GetTickSpaceGeometry().Scale > 0.0f ? GetTickSpaceGeometry().Scale : 1.0f;
    ContentHost->SlatePrepass(LayoutScale);
}

TSharedRef<SWidget> SBreakerMenu::BuildFrame(const FText& Title, const FText& Subtitle, const TSharedRef<SWidget>& Body, float PanelWidth) const
{
    // Header zone: h1 title top-left with the caption directly beneath it,
    // separated from the body by a 1px divider rather than by whitespace —
    // the system reads structure off borders, not off gaps.
    TSharedRef<SVerticalBox> PanelContent = SNew(SVerticalBox);
    PanelContent->AddSlot().AutoHeight()
    [
        MenuText(Title, BreakerUI::TypeH1, Primary, true)
    ];
    PanelContent->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space16)
    [
        MenuText(Subtitle, BreakerUI::TypeCaption, Muted, true)
    ];
    PanelContent->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(BorderRest)]
    ];
    PanelContent->AddSlot().FillHeight(1.0f)
    [
        // SCROLLED, not merely capped. MaxDesiredHeight let the plate grow to
        // its content and then stop growing while the content kept going, so a
        // third character drew CREATE and PLAY outside the panel entirely.
        // Deliberately a plain SScrollBox: SWrapBox with UseAllottedSize inside
        // a scroll box is banned in this project because it caused a bug the
        // owner personally hit.
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            Body
        ]
    ];

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Background)
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(BreakerUI::Space40)
        [
            // FIXED height, derived from the viewport rather than from the
            // content. This is the jitter fix: a content-sized plate that is
            // also centred moves every time anything inside it changes size,
            // which is once per click. A stable rectangle cannot.
            SNew(SBreakerPlateProbe).Label(TEXT("BuildFrame"))
            [
                SNew(SBox).WidthOverride(PanelWidth).HeightOverride(MeasureWideScreen().PanelHeight)
                [
                    // The screen plate carries the cyan identity rail: the front
                    // end belongs to the player/system family.
                    MakePlate(PanelContent, Panel, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space24))
                ]
            ]
        ];
}

TSharedRef<SWidget> SBreakerMenu::BuildZonedFrame(const FText& Title, const FText& Meta, const TSharedRef<SWidget>& HeaderRight,
    const TSharedRef<SWidget>& Body, const TSharedRef<SWidget>& Footer, float PanelWidth, float PanelHeight,
    bool bFillHeight) const
{
    // Header band, 88 tall at bg/raised on the cyan identity rail: h1 title
    // with the meta caption beneath it, the screen's own controls pinned to
    // the right of the same band. Zones are separated by the band, never by
    // whitespace.
    TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);
    Root->AddSlot().AutoHeight()
    [
        SNew(SBox).HeightOverride(88.0f)
        [
            MakePlate(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[MenuText(Title, BreakerUI::TypeH1, Primary, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                    [
                        MenuText(Meta, BreakerUI::TypeCaption, Muted, true)
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerUI::Space40, 0.0f, 0.0f, 0.0f)
                [
                    HeaderRight
                ],
                BreakerUI::BgRaised, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space8))
        ]
    ];
    Root->AddSlot().FillHeight(1.0f).Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)[Body];
    Root->AddSlot().AutoHeight()[Footer];

    // THE SHRINK-WRAP BRANCH IS THE JITTER, and it survived the fix that was
    // supposed to kill it.
    //
    // The previous pass made BuildFrame's plate a FIXED height for exactly this
    // reason — "a content-sized plate that is also centred moves every time
    // anything inside it changes size". That edit landed on BuildFrame and on
    // the skill matrix (which passes bFillHeight=true) and NOWHERE ELSE. The
    // loadout, the forge and the abilities screen all take this function's
    // default, so all three kept a MaxDesiredHeight plate: a rectangle whose
    // height is its content's height, centred by the VAlign_Center slot below.
    // Equip an item, arm a discard chip, let InventoryStatus appear, filter the
    // backpack — the body's desired height changes, the plate's height follows,
    // and a centred plate whose height changed has moved by half the delta at
    // both edges. That is the jitter the owner is still looking at.
    //
    // Every caller now passes true; the parameter is kept so the shape of the
    // decision stays visible rather than being silently removed.
    TSharedRef<SBox> Plate = SNew(SBox).WidthOverride(PanelWidth);
    if (bFillHeight) Plate->SetHeightOverride(PanelHeight);
    else Plate->SetMaxDesiredHeight(PanelHeight);
    Plate->SetContent(Root);

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Background)
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(BreakerUI::Space40)
        [
            SNew(SBreakerPlateProbe).Label(FString::Printf(TEXT("Zoned:%s"), *Title.ToString()))
            [
                Plate
            ]
        ];
}

TSharedRef<SWidget> SBreakerMenu::BuildScreenTabs(EBreakerMenuScreen ActiveScreen)
{
    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
    auto AddTab = [this, &Tabs, ActiveScreen](const FString& Label, EBreakerMenuScreen Target)
    {
        const bool bActive = ActiveScreen == Target;
        // Selected carries the 2px accent border; unselected keeps the same
        // geometry on a neutral 1px ring. Never a teal underline — teal is a
        // noun in this system, and a tab is not a rift object.
        Tabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bActive ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, Target, bActive]()
                {
                    if (!bActive)
                    {
                        if (Target == EBreakerMenuScreen::SkillTrees) SkillTreeStatus = FText::GetEmpty();
                        Rebuild(Target);
                    }
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bActive ? Primary : Muted, true)
                ],
                bActive ? Cyan : BorderEmphasis,
                bActive ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    };
    // GEAR / SKILLS rather than EQUIPMENT / SKILL TREES. The header is one row
    // of AutoWidth slots that an SHorizontalBox will not shrink, so an
    // overlong label here does not wrap or ellipsize — it pushes BACK off the
    // right edge, which capture confirmed at a 1920 viewport. Shortening the
    // two longest tabs is the cheapest width to buy back, and neither loses
    // meaning.
    AddTab(TEXT("GEAR"), EBreakerMenuScreen::Inventory);
    AddTab(TEXT("SKILLS"), EBreakerMenuScreen::SkillTrees);
    // NO FORGE TAB. It is an Anchor interaction (content-and-modes), and a tab
    // here is a pause-menu path: the pause menu's INVENTORY button opens this
    // strip. Kess's dialogue is the only door, exactly as the quartermaster's
    // is — that screen was built without a tab for this reason and the Forge
    // was the inconsistency beside it.
    AddTab(TEXT("ABILITIES"), EBreakerMenuScreen::Abilities);
    return Tabs;
}

// FIELDPLATE 01, interaction states. Primary: panel/20 fill inside a 1px cyan
// ring, text/primary. Secondary: no fill inside a 1px #2A3E58 ring,
// text/secondary. Neither ever changes opacity — that would show the plate
// seams behind it.
TSharedRef<SWidget> SBreakerMenu::MakeButton(const FText& Label, const FOnClicked& OnClicked, bool bPrimary) const
{
    return SNew(SBox).HeightOverride(BreakerUI::MinHitTarget + BreakerUI::Space8)
    [
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bPrimary ? PanelHover : Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            // HAlign_Fill, not HAlign_Left. An SButton arranges its child at
            // the child's DESIRED width under any non-Fill alignment, and for
            // an STextBlock that is its measured width — which Slate then
            // clips the drawn run to, rounding measurement and rasterisation
            // independently. This is the same defect that once clipped the
            // skill board's rank numbers, and it reaches every button on every
            // screen; the tightest instance is BACK inside a hard 88px box in
            // the compact header. Filling gives the label the button's real
            // width and lets justification place it.
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Center)
            .OnClicked(OnClicked)
            [
                SNew(STextBlock)
                    .Text(Label)
                    .Justification(ETextJustify::Left)
                    .ColorAndOpacity(bPrimary ? Primary : SoftText)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
            ],
            bPrimary ? Cyan : BorderEmphasis)
    ];
}

void SBreakerMenu::HandleConfirmKey()
{
    // The title gate, and only the title gate. This is the path that actually
    // fires: it comes from the player input component, which works while the
    // game is paused, rather than from Slate keyboard focus, which the menu
    // widget does not reliably hold in a standalone session.
    if (CurrentScreen == EBreakerMenuScreen::Main && !bTitleRevealed)
    {
        bTitleRevealed = true;
        Rebuild(EBreakerMenuScreen::Main);
    }
}

FReply SBreakerMenu::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    // ---- Rebind capture, first, and only while a row is listening ---------
    // Preview is what makes this work at all: the player reached this state by
    // CLICKING a row, so Slate's focus path runs through this widget, and a
    // preview handler on an ancestor sees the key before any focused
    // descendant can consume it. That is the same mechanism the title gate's
    // comment below describes — with the difference that the gate had to fire
    // with NO prior interaction, which is exactly the case where this widget
    // does not reliably hold focus under FInputModeGameAndUI. A click one
    // event earlier is what separates the two.
    //
    // Escape is spent CANCELLING rather than bound. It means "back" everywhere
    // else in this front end, and a player who binds it has no way to reach a
    // menu to unbind it. HandleEscape carries the same guard, because Escape
    // also arrives on the player-input path
    // (Characters/BreakerCharacter.cpp:343) and either route must cancel.
    if (ListeningKeybindAction != NAME_None)
    {
        const FKey Key = KeyEvent.GetKey();
        if (Key == EKeys::Escape)
        {
            CancelKeybindListen();
            KeybindStatus = FText::FromString(TEXT("REBIND CANCELLED."));
            bKeybindStatusIsClash = false;
            Rebuild(EBreakerMenuScreen::Settings);
            return FReply::Handled();
        }
        // Bare modifier presses are NOT filtered out. Shift is the project's
        // own sprint key (the legend this screen replaced said so), so a
        // rebinder that refused modifiers could not reproduce the default
        // layout it starts from.
        CommitKeybind(Key);
        return FReply::Handled();
    }

    // PREVIEW, not OnKeyDown. Preview runs on ancestors BEFORE descendants, so
    // the gate fires even when Slate has parked keyboard focus on a child —
    // and it had: the CONTINUE button is focusable, so Enter was being routed
    // to the button's own activation path and consumed before ever reaching
    // this widget. That is why the owner reported "enter doesnt move the
    // screen forward" while the mouse fallback worked.
    //
    // Still the title gate and ONLY the title gate: intercepting keys globally
    // here would silently steal them from the name field on the create screen.
    if (CurrentScreen == EBreakerMenuScreen::Main && !bTitleRevealed)
    {
        // Enter is what the owner asked for, but any key dismisses an attract
        // plate — a player who presses Space and sees nothing happen concludes
        // the game is frozen, not that they pressed the wrong key. Escape is
        // excluded because it means "back" everywhere else in this front end
        // and must not come to mean "forward" on one screen.
        if (KeyEvent.GetKey() != EKeys::Escape)
        {
            bTitleRevealed = true;
            Rebuild(EBreakerMenuScreen::Main);
            return FReply::Handled();
        }
    }
    return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
}

FReply SBreakerMenu::OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent)
{
    // Mouse buttons never arrive as key events, and the two most-used binds in
    // the game are Fire (LMB) and Aim (RMB) — a keyboard-only listener could
    // not rebind either. Preview so the click is consumed here rather than
    // being treated as a press by whatever button is under the cursor; the
    // click that STARTED listening cannot be caught by this, because listening
    // is entered from SButton::OnClicked, which fires on mouse UP.
    if (ListeningKeybindAction != NAME_None)
    {
        CommitKeybind(PointerEvent.GetEffectingButton());
        return FReply::Handled();
    }
    return SCompoundWidget::OnPreviewMouseButtonDown(Geometry, PointerEvent);
}

void SBreakerMenu::EnsureRosterLoaded()
{
    if (Roster.IsValid()) return;
    Roster.Reset(UBreakerCharacterRoster::LoadOrCreate());
    if (!Roster.IsValid()) return;
    // A player who has been playing this project before the roster existed has
    // a character in the old single slot. Adopting it here — at the first
    // moment anything asks for the roster — means their progress is simply
    // present on the select screen rather than apparently deleted.
    Roster->AdoptLegacySaveIfPresent();
    if (!SelectedCharacterId.IsValid())
    {
        SelectedCharacterId = Roster->LastPlayedCharacterId.IsValid()
            ? Roster->LastPlayedCharacterId
            : (Roster->Characters.Num() > 0 ? Roster->Characters[0].CharacterId : FGuid());
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildMainScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    // ---- The attract plate --------------------------------------------
    if (!bTitleRevealed)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
        [
            MenuText(FText::FromString(TEXT("LOOT THE RIFTS. OUTRUN THE EDGE.")), 11, SoftText)
        ];
        Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("PRESS ENTER")), BreakerUI::TypeH2, Cyan, true)
        ];
        // A visible fallback for the case the keyboard path is somehow not
        // reaching us. The owner asked for a key, and a key is what this
        // listens for — but a title screen with no clickable way forward is
        // unrecoverable if focus is wrong, and that is a bad thing to be
        // certain about without having looked.
        Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
        [
            MakeButton(FText::FromString(TEXT("CONTINUE")), FOnClicked::CreateLambda([this]()
            {
                bTitleRevealed = true;
                Rebuild(EBreakerMenuScreen::Main);
                return FReply::Handled();
            }), true)
        ];
        return BuildFrame(FText::FromString(TEXT("RIOR'S EDGE")),
            FText::FromString(TEXT("")), Body, 720.0f);
    }

    // ---- PLAY / SETTINGS / QUIT ----------------------------------------
    // Exactly the three the owner asked for. LOADOUT, INVENTORY and BREAKER
    // CLASS used to sit here and have MOVED to the pause menu, where they
    // belong: they act on a character, and at the title screen there is not
    // one yet — every one of them silently operated on whatever pawn the gym
    // happened to have spawned.
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        MenuText(FText::FromString(TEXT("LOOT THE RIFTS. OUTRUN THE EDGE.")), 11, SoftText)
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        MakeButton(FText::FromString(TEXT("PLAY")), FOnClicked::CreateLambda([this]()
        {
            Rebuild(EBreakerMenuScreen::CharacterSelect);
            return FReply::Handled();
        }), true)
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        MakeButton(FText::FromString(TEXT("SETTINGS")), FOnClicked::CreateLambda([this]()
        {
            Rebuild(EBreakerMenuScreen::Settings);
            return FReply::Handled();
        }))
    ];
    Body->AddSlot().AutoHeight()
    [
        MakeButton(FText::FromString(TEXT("QUIT GAME")), FOnClicked::CreateLambda([this]()
        {
            if (Character.IsValid()) Character->QuitFromMenu();
            return FReply::Handled();
        }))
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 26.0f, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("BUILD 0.1  |  WIN64 DEVELOPMENT")), 9, SoftText)
    ];
    return BuildFrame(FText::FromString(TEXT("RIOR'S EDGE")), FText::FromString(TEXT("BREAK THE LINE. KEEP THE MOMENTUM.")), Body);
}

TSharedRef<SWidget> SBreakerMenu::BuildPauseScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    auto AddButton = [&Body](const TSharedRef<SWidget>& Button)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Button];
    };
    AddButton(MakeButton(FText::FromString(TEXT("RESUME")), FOnClicked::CreateLambda([this]()
    {
        if (Character.IsValid()) Character->ResumeFromMenu();
        return FReply::Handled();
    }), true));
    // LOADOUT is gone from this column by ruling (2026-08-17): the archetype
    // picker let players conjure any gun without owning one. INVENTORY is the
    // loadout now — the weapon you carry is the weapon item you equip.
    AddButton(MakeButton(FText::FromString(TEXT("INVENTORY")), FOnClicked::CreateLambda([this]()
    {
        Rebuild(EBreakerMenuScreen::Inventory);
        return FReply::Handled();
    })));
    // SKILL TREES intentionally absent: the INVENTORY screen's tab strip owns
    // that route now.
    AddButton(MakeButton(FText::FromString(TEXT("SETTINGS")), FOnClicked::CreateLambda([this]()
    {
        Rebuild(EBreakerMenuScreen::Settings);
        return FReply::Handled();
    })));
    // The breakpoint sandbox. DEV is in the label rather than implied by
    // placement, because this button changes the character and the world, not
    // preferences — a playtester who clicks it should know they are stepping
    // out of the game's rules before the screen opens.
    AddButton(MakeButton(FText::FromString(TEXT("DEV — BREAKPOINT SANDBOX")), FOnClicked::CreateLambda([this]()
    {
        Rebuild(EBreakerMenuScreen::DevSandbox);
        return FReply::Handled();
    })));
    AddButton(MakeButton(FText::FromString(TEXT("RETURN TO TITLE")), FOnClicked::CreateLambda([this]()
    {
        if (Character.IsValid()) Character->ReturnToTitleMenu();
        return FReply::Handled();
    })));
    AddButton(MakeButton(FText::FromString(TEXT("QUIT TO DESKTOP")), FOnClicked::CreateLambda([this]()
    {
        if (Character.IsValid()) Character->QuitFromMenu();
        return FReply::Handled();
    })));
    Body->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("ESC  RESUME")), 10, SoftText)
    ];
    return BuildFrame(FText::FromString(TEXT("PAUSED")), FText::FromString(TEXT("PLAYTEST GYM / SESSION ACTIVE")), Body);
}

// ---------------------------------------------------------------------------
// SETTINGS
//
// The screen is a view onto UBreakerGameSettings (Settings/BreakerGameSettings
// .h) and nothing else. It used to own its three controls outright, reading and
// writing ABreakerCharacter directly and persisting only what that pawn chose
// to persist; the model had every field the owner asked for — scoped
// sensitivity, window mode, frame cap, vsync, three volumes, keybind
// overrides — and zero callers.
//
// Two house rules are load-bearing here and are called out again at their use
// sites: no Text_Lambda anywhere (the old screen had two, and a per-frame text
// attribute is banned in this file), and every control whose LABEL CHANGES
// sits in a box sized to its LONGEST label, never its shortest.
// ---------------------------------------------------------------------------

void SBreakerMenu::EnsureSettingsLoaded()
{
    if (!GameSettings.IsValid())
    {
        GameSettings.Reset(NewObject<UBreakerGameSettings>(GetTransientPackage()));
        if (GameSettings.IsValid()) GameSettings->LoadOrDefaults();
    }
    if (DefaultKeybinds.Num() == 0)
    {
        // A synchronous asset load. Once per settings-screen entry, never per
        // row and never per rebuild after the first — an empty result is also
        // a legitimate answer (no input asset cooked), so this retries in that
        // case, which costs one failed LoadObject on a screen nobody can open
        // more than once a frame.
        DefaultKeybinds = UBreakerGameSettingsLibrary::ProjectDefaultKeybinds();
    }
}

namespace
{
    // ---- The PREVIOUS settings idiom, now the dev-screen idiom -------------
    // The settings screen moved onto the Fieldplate pack's sidebar layout and
    // control geometry (below). These three survive because the breakpoint
    // sandbox and the character sheet were built on them and are not in this
    // pass; they leave when those screens get theirs.
    TSharedRef<SWidget> SettingsSectionHeader(const FString& Label)
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuText(FText::FromString(Label), BreakerUI::TypeCaption, BreakerUI::Cyan, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space16)
            [
                SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(BreakerUI::BorderRest)]
            ];
    }

    constexpr float SettingsLabelWidth = 210.0f;
    constexpr float SettingsValueWidth = 96.0f;

    TSharedRef<SWidget> SettingsRow(const FString& Label, const TSharedRef<SWidget>& Control,
        const TSharedRef<SWidget>& Readout)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(SettingsLabelWidth).HAlign(HAlign_Fill)
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeBody, BreakerUI::TextPrimary, true)
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Control]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
            [
                Readout
            ];
    }

    // ---- Fieldplate settings geometry -------------------------------------
    // Transcribed from the reference plates (screens.zip export,
    // screen_settings_*.png at 1920x1080) and README-UE5.txt's control
    // geometry. The plates are pixel REFERENCE, never textures: these
    // constants are the transcription, and the plate is held beside the build.
    constexpr float BreakerSettingsSidebarWidth = 300.0f;  // 299 face + 1px divider
    constexpr float BreakerSettingsNavRowHeight = 44.0f;   // every row 44 minimum
    constexpr float BreakerSettingsContentPad = 40.0f;     // pane margin, both sides
    constexpr float BreakerSettingsLabelWidth = 332.0f;    // labels at x340, controls at x672
    constexpr float BreakerSettingsControlHeight = 44.0f;
    // README caps the track at 460; the plate draws 380 with the value column
    // reading at track-end + 16, and the plate is what the build is held
    // against.
    constexpr float BreakerSettingsSliderWidth = 380.0f;
    constexpr float BreakerSettingsValueWidth = 64.0f;     // fixed mono readout column
    constexpr float BreakerSettingsDropdownWidth = 260.0f; // plate: x672-931
    constexpr float BreakerSettingsKeyBoxWidth = 110.0f;   // plate: x672-781
    constexpr float BreakerSettingsDefaultWidth = 87.0f;   // plate: x1791-1878
    // 73px row pitch on the plate: 14 above the 44px control, 14 below, then
    // the 1px divider.
    constexpr float BreakerSettingsRowPad = 14.0f;

    // The type roles live in the file-top namespace beside MenuText:
    // BreakerDisplayFont / BreakerBodyFont / BreakerMonoFont, built from the
    // imported role fonts with the engine faces as their fallback.

    // The 1px row separator the plates thread between control rows. Structure
    // comes off borders in this system, not off gaps.
    TSharedRef<SWidget> BreakerSettingsDivider()
    {
        return SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(Panel)];
    }

    // Pane header: display title, body subtitle, divider. The subtitle is
    // mixed case on the plates — body copy carries no uppercase transform,
    // only display does.
    TSharedRef<SWidget> BreakerSettingsPaneHeader(const FString& Title, const FString& Subtitle)
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuText(FText::FromString(Title), BreakerUI::TypeH1, Primary, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
            [
                MenuText(FText::FromString(Subtitle), BreakerUI::TypeBody, SoftText)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
            [
                BreakerSettingsDivider()
            ];
    }

    // One settings row: fixed label column (optionally carrying a mono
    // sub-caption), the control on the shared control edge, the readout in a
    // fixed mono column 16px after it. Readout colour is text/primary — the
    // plates reserve cyan for the control itself.
    TSharedRef<SWidget> BreakerSettingsRow(const FString& Label, const FString& SubCaption,
        const TSharedRef<SWidget>& Control, const TSharedRef<SWidget>& Readout)
    {
        TSharedRef<SVerticalBox> LabelCell = SNew(SVerticalBox);
        LabelCell->AddSlot().AutoHeight()
        [
            MenuText(FText::FromString(Label), BreakerUI::TypeBody, Primary, true)
        ];
        if (!SubCaption.IsEmpty())
        {
            // WRAPPED at the label column's own width, never clipped: the
            // first capture of this pane photographed "SAVED, NOT Y" running
            // under the slider. WrapTextAt with a width known before layout,
            // per the file's standing rule; a second caption line only grows
            // the row downward.
            LabelCell->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(SubCaption))
                    .ColorAndOpacity(Muted)
                    .WrapTextAt(BreakerSettingsLabelWidth - BreakerUI::Space8)
                    .Font(BreakerMonoFont(BreakerUI::TypeCaption, 0.16f))
            ];
        }
        return SNew(SBox).MinDesiredHeight(BreakerSettingsControlHeight)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(BreakerSettingsLabelWidth).HAlign(HAlign_Fill)[LabelCell]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Control]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
            [
                Readout
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))]
        ];
    }

    // ---- Drawn marks ------------------------------------------------------
    // Every non-alphanumeric mark is drawn geometry, never a glyph: the stock
    // Slate face carries 878 codepoints and no Geometric Shapes block, which
    // is why the pack ships every mark as a texture or a measurement.

    // README's dropdown caret: 12x8, two 2px strokes.
    TSharedRef<SWidget> BreakerDrawnCaret(const FLinearColor& Color)
    {
        TSharedRef<SCanvas> Canvas = SNew(SCanvas);
        AddCanvasSegment(Canvas, FVector2D(1.0, 2.0), FVector2D(6.0, 7.0), Color, 2.0f);
        AddCanvasSegment(Canvas, FVector2D(6.0, 7.0), FVector2D(11.0, 2.0), Color, 2.0f);
        return SNew(SBox).WidthOverride(12.0f).HeightOverride(8.0f)[Canvas];
    }

    // The refusal mark drawn beside a clash badge. The plate draws a warning
    // triangle; Slate's flat brushes cannot fill a triangle, so this is a
    // harm-red diamond (the file's existing drawn-diamond vocabulary) until a
    // mark texture exists. The WORDS beside it are what carry the refusal —
    // red alone is never a refusal.
    TSharedRef<SWidget> BreakerWarnMark()
    {
        return SNew(SBox).WidthOverride(12.0f).HeightOverride(12.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            RotateFortyFive(SNew(SBox).WidthOverride(7.0f).HeightOverride(7.0f)[SolidBlock(Harm)])
        ];
    }

    // The header's « mark: two drawn chevrons, never the glyph.
    TSharedRef<SWidget> BreakerBackChevrons()
    {
        TSharedRef<SCanvas> Canvas = SNew(SCanvas);
        AddCanvasSegment(Canvas, FVector2D(6.0, 1.0), FVector2D(1.0, 5.0), Muted, 2.0f);
        AddCanvasSegment(Canvas, FVector2D(1.0, 5.0), FVector2D(6.0, 9.0), Muted, 2.0f);
        AddCanvasSegment(Canvas, FVector2D(12.0, 1.0), FVector2D(7.0, 5.0), Muted, 2.0f);
        AddCanvasSegment(Canvas, FVector2D(7.0, 5.0), FVector2D(12.0, 9.0), Muted, 2.0f);
        return SNew(SBox).WidthOverride(14.0f).HeightOverride(10.0f)[Canvas];
    }

    // The sidebar's stub marker: a 6px drawn square and the word, both at the
    // disabled colour. Words beside geometry — colour is never alone.
    TSharedRef<SWidget> BreakerSettingsStubMark()
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                SNew(SBox).WidthOverride(6.0f).HeightOverride(6.0f)[SolidBlock(Disabled)]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                BreakerMonoText(FText::FromString(TEXT("STUB")), BreakerUI::TypeCaption, Disabled, 0.16f)
            ];
    }

    // ---- Quiet buttons ----------------------------------------------------
    // The settings screen's button voice: no fill, 1px emphasis ring, tracked
    // mono caption. DEFAULT and RESET ALL KEYBINDS are this; the plates keep
    // primary buttons off the screen entirely.
    TSharedRef<SWidget> BreakerSettingsGhostButton(const FString& Label, const FOnClicked& OnClicked, float Width)
    {
        return SNew(SBox).WidthOverride(Width).HeightOverride(BreakerSettingsControlHeight)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(Transparent)
                .ContentPadding(FMargin(BreakerUI::Space8, 0.0f))
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                .OnClicked(OnClicked)
                [
                    BreakerMonoText(FText::FromString(Label), BreakerUI::TypeCaption, SoftText, 0.16f)
                ],
                BorderEmphasis)
        ];
    }

    // The same geometry with the interaction gone. Disabled is PAINTED, never
    // faded: geometry kept, fill to bg/raised, ring to border/rest, text to
    // text/disabled, accent stripped — opacity would reveal the plate seams.
    TSharedRef<SWidget> BreakerSettingsDisabledButton(const FString& Label, float Width)
    {
        return SNew(SBox).WidthOverride(Width).HeightOverride(BreakerSettingsControlHeight)
        [
            BorderWrap(
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(BreakerUI::BgRaised)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    BreakerMonoText(FText::FromString(Label), BreakerUI::TypeCaption, Disabled, 0.16f)
                ],
                BorderRest)
        ];
    }

    DECLARE_DELEGATE_OneParam(FBreakerOnToggle, bool);
    DECLARE_DELEGATE_OneParam(FBreakerOnPick, int32);

    // ---- FIELDPLATE slider ------------------------------------------------
    // README: 6px track, 14x20 drawn thumb (a rectangle, not a circle), value
    // right-aligned in a fixed 64px mono column. A custom leaf widget rather
    // than a restyled SSlider because the reference fills the travelled half
    // of the track in system cyan and SSlider has no filled segment — the
    // fill is half of what the control says at a glance.
    class SBreakerFieldplateSlider : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SBreakerFieldplateSlider)
            : _Value(0.0f)
            {}
            SLATE_ARGUMENT(float, Value)
            SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
            SLATE_EVENT(FSimpleDelegate, OnCaptureEnd)
        SLATE_END_ARGS()

        static constexpr float TrackHeight = 6.0f;
        static constexpr float ThumbWidth = 14.0f;
        static constexpr float ThumbHeight = 20.0f;

        void Construct(const FArguments& InArgs)
        {
            Value = FMath::Clamp(InArgs._Value, 0.0f, 1.0f);
            OnValueChanged = InArgs._OnValueChanged;
            OnCaptureEnd = InArgs._OnCaptureEnd;
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            // The whole 44px strip is the hit target, not the 6px bar.
            return FVector2D(BreakerSettingsSliderWidth, BreakerSettingsControlHeight);
        }

        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
            const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
            const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
        {
            const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
            const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
            const float TrackY = Size.Y * 0.5f - TrackHeight * 0.5f;
            const float Span = FMath::Max(1.0f, Size.X - ThumbWidth);
            const float ThumbX = Span * Value;

            // Untravelled track, the travelled span over it, then the thumb —
            // a cyan 14x20 rectangle with the panel face inset 1px.
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, TrackHeight),
                    FSlateLayoutTransform(FVector2f(0.0f, TrackY))),
                Brush, ESlateDrawEffect::None, PanelRaised);
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(FVector2f(ThumbX + ThumbWidth * 0.5f, TrackHeight),
                    FSlateLayoutTransform(FVector2f(0.0f, TrackY))),
                Brush, ESlateDrawEffect::None, Cyan);
            const float ThumbY = Size.Y * 0.5f - ThumbHeight * 0.5f;
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
                AllottedGeometry.ToPaintGeometry(FVector2f(ThumbWidth, ThumbHeight),
                    FSlateLayoutTransform(FVector2f(ThumbX, ThumbY))),
                Brush, ESlateDrawEffect::None, Cyan);
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3,
                AllottedGeometry.ToPaintGeometry(FVector2f(ThumbWidth - 2.0f, ThumbHeight - 2.0f),
                    FSlateLayoutTransform(FVector2f(ThumbX + 1.0f, ThumbY + 1.0f))),
                Brush, ESlateDrawEffect::None, PanelRaised);
            return LayerId + 3;
        }

        virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
        {
            if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
            SetValueFromPointer(Geometry, Event);
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }

        virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
        {
            if (!HasMouseCapture()) return FReply::Unhandled();
            SetValueFromPointer(Geometry, Event);
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override
        {
            if (!HasMouseCapture()) return FReply::Unhandled();
            // Saved on RELEASE, not per drag frame: the drag handler fires
            // dozens of times a second and Save() flushes an ini file.
            OnCaptureEnd.ExecuteIfBound();
            return FReply::Handled().ReleaseMouseCapture();
        }

        virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override
        {
            return FCursorReply::Cursor(EMouseCursor::Hand);
        }

    private:
        void SetValueFromPointer(const FGeometry& Geometry, const FPointerEvent& Event)
        {
            const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
            const float Span = FMath::Max(1.0f, static_cast<float>(Geometry.GetLocalSize().X) - ThumbWidth);
            const float NewValue = FMath::Clamp((static_cast<float>(Local.X) - ThumbWidth * 0.5f) / Span, 0.0f, 1.0f);
            if (!FMath::IsNearlyEqual(NewValue, Value))
            {
                Value = NewValue;
                OnValueChanged.ExecuteIfBound(Value);
                Invalidate(EInvalidateWidgetReason::Paint);
            }
        }

        float Value = 0.0f;
        FOnFloatValueChanged OnValueChanged;
        FSimpleDelegate OnCaptureEnd;
    };

    // ---- FIELDPLATE toggle ------------------------------------------------
    // README: 52x26 plate, 20x20 knob translating 24px in 120ms, and the word
    // ON or OFF ALWAYS sits beside it — colour is never alone. The word is
    // the row's job (it needs a live text handle); this widget is the plate
    // and the knob. The README's 2px radius is not drawable with the flat
    // WhiteBrush this file draws with; the whole menu is square today and the
    // toggle stays consistent with it.
    class SBreakerFieldplateToggle : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SBreakerFieldplateToggle)
            : _IsOn(false)
            {}
            SLATE_ARGUMENT(bool, IsOn)
            SLATE_EVENT(FBreakerOnToggle, OnToggled)
        SLATE_END_ARGS()

        static constexpr float PlateWidth = 52.0f;
        static constexpr float PlateHeight = 26.0f;
        static constexpr float KnobSize = 20.0f;
        // 1px border + 3 inset + 20 knob + 24 travel + 3 inset + 1px = 52.
        static constexpr float KnobTravel = 24.0f;
        static constexpr float TravelSeconds = 0.12f;

        void Construct(const FArguments& InArgs)
        {
            bOn = InArgs._IsOn;
            OnToggled = InArgs._OnToggled;
            Anim = FCurveSequence(0.0f, TravelSeconds, ECurveEaseFunction::QuadInOut);
            if (bOn) Anim.JumpToEnd();

            SAssignNew(KnobFace, SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(bOn ? Cyan : Disabled)
                [
                    SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
                ];
            SAssignNew(Knob, SBox).WidthOverride(KnobSize).HeightOverride(KnobSize)
            [
                KnobFace.ToSharedRef()
            ];

            ChildSlot
            [
                // The 44px hit target the row demands, holding the 26px plate.
                SNew(SBox).WidthOverride(PlateWidth).HeightOverride(BreakerSettingsControlHeight).VAlign(VAlign_Center)
                [
                    SNew(SBox).HeightOverride(PlateHeight)
                    [
                        BorderWrap(
                            SNew(SOverlay)
                            + SOverlay::Slot()[SolidBlock(BreakerUI::BgRaised)]
                            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(3.0f, 0.0f, 0.0f, 0.0f)
                            [
                                Knob.ToSharedRef()
                            ],
                            BorderEmphasis)
                    ]
                ]
            ];
            ApplyKnob();
        }

        virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
        {
            if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
            bOn = !bOn;
            if (bOn) Anim.Play(AsShared()); else Anim.PlayReverse(AsShared());
            KnobFace->SetBorderBackgroundColor(bOn ? Cyan : Disabled);
            // Drive the knob imperatively for the 120ms of travel: an active
            // timer that STOPS, never a per-frame attribute.
            if (!TimerHandle.IsValid())
            {
                TimerHandle = RegisterActiveTimer(0.0f,
                    FWidgetActiveTimerDelegate::CreateSP(this, &SBreakerFieldplateToggle::DriveKnob));
            }
            OnToggled.ExecuteIfBound(bOn);
            return FReply::Handled();
        }

        virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override
        {
            return FCursorReply::Cursor(EMouseCursor::Hand);
        }

    private:
        EActiveTimerReturnType DriveKnob(double, float)
        {
            ApplyKnob();
            if (!Anim.IsPlaying())
            {
                TimerHandle.Reset();
                return EActiveTimerReturnType::Stop;
            }
            return EActiveTimerReturnType::Continue;
        }

        void ApplyKnob()
        {
            if (Knob.IsValid())
            {
                Knob->SetRenderTransform(TOptional<FSlateRenderTransform>(
                    FSlateRenderTransform(FVector2D(KnobTravel * Anim.GetLerp(), 0.0f))));
            }
        }

        bool bOn = false;
        FBreakerOnToggle OnToggled;
        FCurveSequence Anim;
        TSharedPtr<SBox> Knob;
        TSharedPtr<SBorder> KnobFace;
        TSharedPtr<FActiveTimerHandle> TimerHandle;
    };

    // ---- Listening frame --------------------------------------------------
    // README: "click arms it, border blinks gold at 1s". Visible for the
    // first half of each second, at the rest colour for the second half — the
    // same 500ms step the loading screen's cursor block names. A 2Hz active
    // timer flips an SBorder colour imperatively; nothing here is a
    // paint-time attribute.
    class SBreakerBlinkBorder : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SBreakerBlinkBorder)
            : _OnColor(FLinearColor::White)
            , _OffColor(FLinearColor::Transparent)
            , _Thickness(BreakerUI::BorderThin)
            {}
            SLATE_ARGUMENT(FLinearColor, OnColor)
            SLATE_ARGUMENT(FLinearColor, OffColor)
            SLATE_ARGUMENT(float, Thickness)
            SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            OnColor = InArgs._OnColor;
            OffColor = InArgs._OffColor;
            ChildSlot
            [
                SAssignNew(Frame, SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(OnColor)
                    .Padding(FMargin(InArgs._Thickness))
                [
                    InArgs._Content.Widget
                ]
            ];
            RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SBreakerBlinkBorder::Step));
        }

    private:
        EActiveTimerReturnType Step(double, float)
        {
            bLit = !bLit;
            if (Frame.IsValid()) Frame->SetBorderBackgroundColor(bLit ? OnColor : OffColor);
            return EActiveTimerReturnType::Continue;
        }

        TSharedPtr<SBorder> Frame;
        FLinearColor OnColor = FLinearColor::White;
        FLinearColor OffColor = FLinearColor::Transparent;
        bool bLit = true;
    };

    // ---- FIELDPLATE dropdown ----------------------------------------------
    // README: 44px min height, 1px border, drawn 12x8 caret. Open list is
    // flat, no shadow, selected row marked by a 3px left rail, not a check.
    // SMenuAnchor in-window (UseCurrentWindow) so the list is a plain plate
    // rather than an OS popup with a drop shadow.
    class SBreakerFieldplateDropdown : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SBreakerFieldplateDropdown)
            : _SelectedIndex(0)
            {}
            SLATE_ARGUMENT(TArray<FString>, Options)
            SLATE_ARGUMENT(int32, SelectedIndex)
            SLATE_EVENT(FBreakerOnPick, OnPicked)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            Options = InArgs._Options;
            Selected = FMath::Clamp(InArgs._SelectedIndex, 0, FMath::Max(0, Options.Num() - 1));
            OnPicked = InArgs._OnPicked;

            const FString Current = Options.IsValidIndex(Selected) ? Options[Selected] : FString();
            ChildSlot
            [
                SAssignNew(Anchor, SMenuAnchor)
                .Placement(MenuPlacement_ComboBox)
                .Method(EPopupMethod::UseCurrentWindow)
                .OnGetMenuContent(FOnGetContent::CreateSP(this, &SBreakerFieldplateDropdown::BuildList))
                [
                    SNew(SBox).WidthOverride(BreakerSettingsDropdownWidth).HeightOverride(BreakerSettingsControlHeight)
                    [
                        BorderWrap(
                            SNew(SButton)
                            .ButtonColorAndOpacity(BreakerUI::BgRaised)
                            .ContentPadding(FMargin(BreakerUI::Space16, 0.0f))
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Center)
                            .OnClicked(FOnClicked::CreateSP(this, &SBreakerFieldplateDropdown::ToggleOpen))
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                                [
                                    BreakerMonoText(FText::FromString(Current), BreakerUI::TypeCaption, Primary, 0.16f)
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [
                                    BreakerDrawnCaret(Muted)
                                ]
                            ],
                            BorderEmphasis)
                    ]
                ]
            ];
        }

    private:
        FReply ToggleOpen()
        {
            if (Anchor.IsValid()) Anchor->SetIsOpen(!Anchor->IsOpen());
            return FReply::Handled();
        }

        TSharedRef<SWidget> BuildList()
        {
            TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
            for (int32 Index = 0; Index < Options.Num(); ++Index)
            {
                const bool bSelected = Index == Selected;
                Rows->AddSlot().AutoHeight()
                [
                    SNew(SBox).HeightOverride(BreakerSettingsControlHeight)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SBox).WidthOverride(BreakerUI::RailThickness)
                            [
                                SolidBlock(bSelected ? Cyan : Transparent)
                            ]
                        ]
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(SButton)
                            .ButtonColorAndOpacity(bSelected ? PanelRaised : BreakerUI::BgRaised)
                            .ContentPadding(FMargin(BreakerUI::Space16 - BreakerUI::RailThickness, 0.0f))
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Center)
                            .OnClicked(FOnClicked::CreateSP(this, &SBreakerFieldplateDropdown::Pick, Index))
                            [
                                BreakerMonoText(FText::FromString(Options[Index]), BreakerUI::TypeCaption,
                                    bSelected ? Primary : SoftText, 0.16f)
                            ]
                        ]
                    ]
                ];
            }
            return SNew(SBox).WidthOverride(BreakerSettingsDropdownWidth)
            [
                BorderWrap(Rows, BorderEmphasis)
            ];
        }

        FReply Pick(int32 Index)
        {
            if (Anchor.IsValid()) Anchor->SetIsOpen(false);
            if (Index != Selected) OnPicked.ExecuteIfBound(Index);
            return FReply::Handled();
        }

        TArray<FString> Options;
        int32 Selected = 0;
        FBreakerOnPick OnPicked;
        TSharedPtr<SMenuAnchor> Anchor;
    };
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsInputSection()
{
    UBreakerGameSettings* Model = GameSettings.Get();
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    Section->AddSlot().AutoHeight()
    [
        BreakerSettingsPaneHeader(TEXT("INPUT"), TEXT("Sensitivity and invert. Applied live."))
    ];
    if (!Model) return Section;

    // Live readouts are held as widget handles and written imperatively from
    // the slider's own OnValueChanged. NOT a Text_Lambda: a per-frame text
    // attribute is banned in this file, and it is not needed — a slider that
    // moves fires an event, and an event is exactly when the number changes.
    // Each one sits in the fixed 64px mono column so a value going from "1.0"
    // to "1.25" cannot reflow the row.
    //
    // The readout widget is built FIRST and the handle captured BY VALUE. Both
    // matter: the handle is captured into a lambda that outlives this function,
    // and the order in which a compiler evaluates two arguments to the same
    // call is unspecified — a by-reference capture of a handle SAssignNew has
    // not filled in yet would be a dangling reference on some builds and fine
    // on others, which is the worst kind of both.
    TSharedPtr<STextBlock> SensitivityReadout;
    const TSharedRef<SWidget> SensitivityValue =
        SNew(SBox).WidthOverride(BreakerSettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(SensitivityReadout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.2f"), Model->MouseSensitivity)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Primary)
                .Font(BreakerMonoFont(BreakerUI::TypeBody))
        ];

    TSharedPtr<STextBlock> ScopedReadout;
    const TSharedRef<SWidget> ScopedValue =
        SNew(SBox).WidthOverride(BreakerSettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(ScopedReadout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.2fx"), Model->ScopedSensitivityMultiplier)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Primary)
                .Font(BreakerMonoFont(BreakerUI::TypeBody))
        ];

    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
    [
        BreakerSettingsRow(TEXT("Look sensitivity"), FString(),
            SNew(SBox).WidthOverride(BreakerSettingsSliderWidth)
            [
                SNew(SBreakerFieldplateSlider)
                // 0.2 .. 2.0, the range ClampMouseSensitivity enforces
                // (Settings/BreakerGameSettings.cpp:12-15) and the range the
                // previous screen's slider already spanned.
                .Value((Model->MouseSensitivity - 0.2f) / 1.8f)
                .OnValueChanged(FOnFloatValueChanged::CreateLambda([this, SensitivityReadout](float Value)
                {
                    UBreakerGameSettings* Live = GameSettings.Get();
                    if (!Live) return;
                    Live->MouseSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(0.2f + Value * 1.8f);
                    // The live pawn keeps its own copy of these three; pushing
                    // them through ApplyMenuSettings is what makes the slider
                    // FELT rather than merely stored.
                    if (Character.IsValid())
                    {
                        Character->ApplyMenuSettings(Live->MouseSensitivity, Live->FieldOfView, Live->bInvertVerticalLook);
                    }
                    if (SensitivityReadout.IsValid())
                    {
                        SensitivityReadout->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Live->MouseSensitivity)));
                    }
                }))
                .OnCaptureEnd(FSimpleDelegate::CreateLambda([this]() { if (GameSettings.IsValid()) GameSettings->Save(); }))
            ],
            SensitivityValue)
    ];
    Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];

    // The plate's name for this row is ADS; the model keeps
    // ScopedSensitivityMultiplier and the honesty line stays until the aim
    // path reads it.
    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
    [
        BreakerSettingsRow(TEXT("ADS sensitivity"),
            TEXT("MULTIPLIER OF LOOK — SAVED, NOT YET READ BY THE AIM PATH"),
            SNew(SBox).WidthOverride(BreakerSettingsSliderWidth)
            [
                SNew(SBreakerFieldplateSlider)
                // 0.1 .. 3.0 (ClampScopedSensitivityMultiplier,
                // BreakerGameSettings.cpp:17-20).
                .Value((Model->ScopedSensitivityMultiplier - 0.1f) / 2.9f)
                .OnValueChanged(FOnFloatValueChanged::CreateLambda([this, ScopedReadout](float Value)
                {
                    UBreakerGameSettings* Live = GameSettings.Get();
                    if (!Live) return;
                    Live->ScopedSensitivityMultiplier =
                        UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(0.1f + Value * 2.9f);
                    if (ScopedReadout.IsValid())
                    {
                        ScopedReadout->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Live->ScopedSensitivityMultiplier)));
                    }
                }))
                .OnCaptureEnd(FSimpleDelegate::CreateLambda([this]() { if (GameSettings.IsValid()) GameSettings->Save(); }))
            ],
            ScopedValue)
    ];
    Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];

    // The toggle's word, built first for the same capture-order reason as the
    // readouts. The word IS the state — colour is never alone.
    TSharedPtr<STextBlock> InvertWord;
    const TSharedRef<SWidget> InvertWordWidget =
        SAssignNew(InvertWord, STextBlock)
            .Text(FText::FromString(Model->bInvertVerticalLook ? TEXT("ON") : TEXT("OFF")))
            .ColorAndOpacity(Model->bInvertVerticalLook ? SoftText : Muted)
            .Font(BreakerMonoFont(BreakerUI::TypeCaption, 0.16f));

    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
    [
        BreakerSettingsRow(TEXT("Invert vertical"), FString(),
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBreakerFieldplateToggle)
                .IsOn(Model->bInvertVerticalLook)
                .OnToggled(FBreakerOnToggle::CreateLambda([this, InvertWord](bool bOn)
                {
                    UBreakerGameSettings* Live = GameSettings.Get();
                    if (!Live) return;
                    Live->bInvertVerticalLook = bOn;
                    if (Character.IsValid())
                    {
                        Character->ApplyMenuSettings(Live->MouseSensitivity, Live->FieldOfView, Live->bInvertVerticalLook);
                    }
                    Live->Save();
                    if (InvertWord.IsValid())
                    {
                        InvertWord->SetText(FText::FromString(bOn ? TEXT("ON") : TEXT("OFF")));
                        InvertWord->SetColorAndOpacity(bOn ? SoftText : Muted);
                    }
                }))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
            [
                InvertWordWidget
            ],
            SNew(SBox).WidthOverride(BreakerSettingsValueWidth))
    ];
    Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];
    return Section;
}

TSharedRef<SWidget> SBreakerMenu::MakeKeybindRow(FName Action, const TMap<FName, TArray<FKey>>& DefaultKeys,
    const TMap<FName, FKey>& FlatDefaults)
{
    UBreakerGameSettings* Model = GameSettings.Get();
    const TArray<FKey>* ActionDefaults = DefaultKeys.Find(Action);
    // COMPOSITE means more than one KEYBOARD-OR-MOUSE key, not more than one
    // key. Counting every default made almost every action composite — the
    // mapping context binds a gamepad button alongside the keyboard one for
    // Jump, Sprint, Dash, Slide, Fire, Aim and Reload — so the screen marked
    // them "AXIS — MULTI-KEY" and gave them no BIND button at all. The owner
    // asked for keybinds "rebindable to whatever", and the seven most-used
    // actions in the game were the ones that could not be rebound.
    //
    // Gamepad defaults are deliberately excluded from the whole question
    // rather than merely from the count: the override map holds one FKey per
    // action, rebinding is a keyboard-and-mouse act, and a pad binding the
    // player never touched should not be destroyed by it. Move and Look stay
    // composite for the real reason — WASD and the two mouse axes genuinely
    // cannot be expressed as one FKey.
    TArray<FKey> DeskDefaults;
    if (ActionDefaults)
    {
        for (const FKey& Key : *ActionDefaults)
        {
            if (!Key.IsGamepadKey()) DeskDefaults.Add(Key);
        }
    }
    // An ANALOG AXIS is not rebindable to a key either, and for a different
    // reason than a composite: Look resolves to exactly one default (Mouse XY
    // 2D-Axis), so the composite rule let it through and offered a BIND button
    // that would have replaced a two-dimensional analog input with a single
    // digital key. Both cases end at the same place — no BIND button, and the
    // row says which of the two reasons applies.
    const bool bAxisBound = DeskDefaults.Num() == 1 && (DeskDefaults[0].IsAxis1D() || DeskDefaults[0].IsAxis2D() || DeskDefaults[0].IsAxis3D());
    const bool bComposite = DeskDefaults.Num() > 1 || bAxisBound;

    // The override if one is set, otherwise this action's first keyboard/mouse
    // default. FlatDefaults takes the first key of ANY kind, which for a
    // pad-first action resolved to a gamepad button and displayed it as the
    // thing the player was about to rebind.
    FKey Resolved = Model && Model->KeybindOverrides.Contains(Action)
        ? Model->KeybindOverrides[Action]
        : (DeskDefaults.Num() > 0 ? DeskDefaults[0] : FKey());
    if (!Resolved.IsValid() && Model)
    {
        Resolved = UBreakerGameSettingsLibrary::ResolveActionKey(Action, Model->KeybindOverrides, FlatDefaults);
    }
    const bool bOverridden = Model && Model->KeybindOverrides.Contains(Action);

    // What the key cap says. SHORT display names throughout — the cap is a
    // fixed 110px box (see the budget note below), and only the keyboard/mouse
    // half: joining every default including the gamepad names produced strings
    // like "GAMEPAD LEFT THUMBSTICK 2D-AXIS  MOUSE XY 2D-AXIS" and the row
    // clipped them on the left.
    FString KeyLabel;
    FString CompositeList;
    if (bComposite)
    {
        // The CAP shows only the first key — the full list cannot fit a 110px
        // box (the first capture photographed "USE XY 2D-AX" clipped at both
        // ends) — and the BADGE carries the whole list beside its reason,
        // where there is a screen-half of room. Axis suffixes come off the
        // cap: "MOUSE XY" says what the badge's "MOUSE XY 2D-AXIS" details.
        for (const FKey& Key : DeskDefaults)
        {
            if (!CompositeList.IsEmpty()) CompositeList += TEXT(" ");
            CompositeList += Key.GetDisplayName(false).ToString().ToUpper();
        }
        KeyLabel = DeskDefaults.Num() > 0 ? DeskDefaults[0].GetDisplayName(false).ToString().ToUpper() : TEXT("UNBOUND");
        KeyLabel.RemoveFromEnd(TEXT(" 2D-AXIS"));
        KeyLabel.RemoveFromEnd(TEXT(" 3D-AXIS"));
        KeyLabel.RemoveFromEnd(TEXT(" AXIS"));
    }
    else if (Resolved.IsValid())
    {
        KeyLabel = Resolved.GetDisplayName(false).ToString().ToUpper();
    }
    else
    {
        KeyLabel = TEXT("UNBOUND");
    }

    // The clash badge. FindKeybindConflict is the same function the commit path
    // uses, asked here about the key the action ALREADY holds — so two actions
    // that legitimately share a key (the player confirmed it) keep saying so on
    // both rows for as long as it is true, instead of the clash being a
    // one-shot message that scrolls away.
    FName ClashWith = NAME_None;
    if (Model && !bComposite && Resolved.IsValid())
    {
        UBreakerGameSettingsLibrary::FindKeybindConflict(Action, Resolved, Model->KeybindOverrides, FlatDefaults, ClashWith);
    }

    const bool bListening = ListeningKeybindAction == Action;
    const bool bPending = PendingKeybindAction == Action && PendingKeybindKey.IsValid();
    // The pending clash's owner, for the badge — the same live question the
    // committed-share badge asks, asked about the key the player just pressed.
    FName PendingClashWith = NAME_None;
    if (bPending && Model)
    {
        UBreakerGameSettingsLibrary::FindKeybindConflict(Action, PendingKeybindKey, Model->KeybindOverrides,
            FlatDefaults, PendingClashWith);
    }

    // ---- THE KEY CAP, and its width budget --------------------------------
    // The key display IS the control (README: no BIND button; click arms).
    // The cap is the plate's fixed 110px box, so the strings in it are SHORT
    // display names — GetDisplayName(false): LMB, RMB, SPACE. MEASURED, not
    // derived: IBM Plex Mono rendered "LEFT SHIFT" at ~98px (~9.8px per glyph
    // at size 13 — wider than the metrics tables suggest), which is why the
    // cap is mono 12 on a 2px pad: 11 glyphs ("SCROLL LOCK", the widest short
    // name the desk produces) ≈ 100px against 110 - 2px ring - 4px pad = 104.
    // The cap never says "PRESS A KEY…": listening is the blinking gold frame
    // plus the status line, which has room for whole sentences. Long key
    // names stay in the status line too, where they fit.
    if (bPending)
    {
        // The cap previews the key the player pressed; the badge names who
        // holds it; nothing has been written.
        KeyLabel = PendingKeybindKey.GetDisplayName(false).ToString().ToUpper();
    }

    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        SNew(SBox).WidthOverride(BreakerSettingsLabelWidth).HAlign(HAlign_Fill)
        [
            MenuText(UBreakerGameSettingsLibrary::DescribeAction(Action), BreakerUI::TypeBody,
                bListening ? Amber : Primary, true)
        ]
    ];

    // ---- The key column ---------------------------------------------------
    if (bComposite)
    {
        // Not single-key rebindable — there is nothing one FKey could replace
        // a 2D axis or four movement keys with — so the cap is PAINTED
        // disabled: geometry kept, fill to bg/raised, ring to border/rest,
        // text to disabled. Never a missing control, never opacity.
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(BreakerSettingsKeyBoxWidth).HeightOverride(BreakerSettingsControlHeight)
            [
                BorderWrap(
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(BreakerUI::BgRaised)
                    .HAlign(HAlign_Fill)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(KeyLabel))
                            .Justification(ETextJustify::Center)
                            .ColorAndOpacity(Disabled)
                            .Font(BreakerMonoFont(BreakerUI::TypeCaption))
                    ],
                    BorderRest)
            ]
        ];
    }
    else
    {
        // Rest state text: cyan is the overridden tell (beside the DEFAULT
        // button going live — two carriers, never colour alone).
        const FLinearColor CapColor = bPending
            ? Primary
            : (Resolved.IsValid() ? (bOverridden ? Cyan : Primary) : Disabled);
        // The NoBorder button style, with the face painted by an explicit
        // SBorder underneath. The default style's brush both DARKENS the
        // ButtonColorAndOpacity tint (the cap face photographed near-black
        // against the plate's 18263A) and spends ~32px of hidden style
        // padding, which is what clipped "LEFT SHIFT" inside a box whose
        // arithmetic said it fit. With the style chrome gone the interior is
        // 110 - 2px ring - 8px pad = 100px — 11 glyphs at mono 13, clearing
        // "SCROLL LOCK", the widest short name the desk produces.
        const TSharedRef<SWidget> Cap =
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(PanelRaised)
            .Padding(FMargin(0.0f))
            [
                SNew(SButton)
                .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                .ContentPadding(FMargin(2.0f, 0.0f))
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                .OnClicked(FOnClicked::CreateLambda([this, Action]()
                {
                    // While ANY row is listening, mouse presses are captured
                    // by OnPreviewMouseButtonDown before this can fire — which
                    // is correct, LMB is bindable — so this click is always
                    // the arm.
                    BeginKeybindListen(Action);
                    return FReply::Handled();
                }))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(KeyLabel))
                        .Justification(ETextJustify::Center)
                        .ColorAndOpacity(CapColor)
                        .Font(BreakerMonoFont(12))
                ]
            ];

        // The frame carries the state: gold blink while listening (README's
        // 1s step), harm-deep while a named clash waits for its second press,
        // emphasis ring at rest.
        TSharedRef<SWidget> Framed = bPending
            ? BorderWrap(Cap, HarmDeep)
            : (bListening
                ? StaticCastSharedRef<SWidget>(SNew(SBreakerBlinkBorder).OnColor(Amber).OffColor(BorderEmphasis)[Cap])
                : BorderWrap(Cap, BorderEmphasis));
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(BreakerSettingsKeyBoxWidth).HeightOverride(BreakerSettingsControlHeight)[Framed]
        ];
    }

    // ---- The badge --------------------------------------------------------
    const FName BadgeNames = bPending ? PendingClashWith : ClashWith;
    TSharedRef<SWidget> Badge = SNew(SSpacer).Size(FVector2D(1.0f, 1.0f));
    if (bComposite)
    {
        // The reason, then the whole list the cap could not hold.
        const FString Reason = bAxisBound ? TEXT("ANALOG AXIS") : TEXT("MULTI-KEY");
        Badge = BreakerMonoText(FText::FromString(FString::Printf(TEXT("%s: %s"), *Reason, *CompositeList)),
            BreakerUI::TypeCaption, Muted, 0.16f);
    }
    else if (BadgeNames != NAME_None)
    {
        // A refusal states its reason in words beside a drawn mark — red
        // alone is never a refusal. The committed share keeps the same shape:
        // it is a fact the player chose, said on both rows for as long as it
        // is true.
        const FString Owner = UBreakerGameSettingsLibrary::DescribeAction(BadgeNames).ToString();
        const FString BadgeText = bPending
            ? FString::Printf(TEXT("HELD BY %s"), *Owner)
            : FString::Printf(TEXT("SHARED: %s"), *Owner);
        Badge = SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                BreakerWarnMark()
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                BreakerMonoText(FText::FromString(BadgeText), BreakerUI::TypeCaption, Harm, 0.16f)
            ];
    }
    Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f)
    [
        // Clipped, because an SHorizontalBox draws an oversized child straight
        // through its neighbour — a runaway badge must not print through
        // DEFAULT.
        SNew(SBox).Clipping(EWidgetClipping::ClipToBounds).HAlign(HAlign_Left).VAlign(VAlign_Center)
        [
            Badge
        ]
    ];

    // ---- DEFAULT ----------------------------------------------------------
    // Always present, per the plate: live when there is an override to clear,
    // otherwise painted disabled — geometry kept, never a hole in the column.
    const bool bResettable = !bComposite && bOverridden;
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        bResettable
            ? BreakerSettingsGhostButton(TEXT("DEFAULT"), FOnClicked::CreateLambda([this, Action]()
              {
                  if (UBreakerGameSettings* Live = GameSettings.Get())
                  {
                      Live->ClearKeybindOverride(Action);
                      Live->Save();
                      KeybindStatus = FText::FromString(FString::Printf(TEXT("%s IS BACK ON ITS DEFAULT KEY."),
                          *UBreakerGameSettingsLibrary::DescribeAction(Action).ToString()));
                      bKeybindStatusIsClash = false;
                  }
                  CancelKeybindListen();
                  Rebuild(EBreakerMenuScreen::Settings);
                  return FReply::Handled();
              }), BreakerSettingsDefaultWidth)
            : BreakerSettingsDisabledButton(TEXT("DEFAULT"), BreakerSettingsDefaultWidth)
    ];
    return Row;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsKeybindSection()
{
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    // The subtitle is the behavioural contract, in the pack's own words: the
    // first press of a clashing key names the clash and commits nothing; the
    // second press takes it.
    Section->AddSlot().AutoHeight()
    [
        BreakerSettingsPaneHeader(TEXT("KEYBINDS"),
            TEXT("The key display is the control. Click it, press a key, press again to take it."))
    ];

    const TMap<FName, FKey> FlatDefaults = UBreakerGameSettingsLibrary::FirstKeyPerAction(DefaultKeybinds);

    if (DefaultKeybinds.Num() == 0)
    {
        // Said out loud rather than drawn as a list of UNBOUND rows that would
        // read as "this game has no controls".
        Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
        [
            BreakerMonoText(FText::FromString(TEXT("NO INPUT CONFIG COULD BE LOADED — DEFAULT KEYS ARE UNKNOWN. REBINDS STILL SAVE.")),
                BreakerUI::TypeCaption, Harm, 0.16f)
        ];
    }

    for (const FName& Action : UBreakerGameSettingsLibrary::BindableActionNames())
    {
        Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
        [
            MakeKeybindRow(Action, DefaultKeybinds, FlatDefaults)
        ];
        Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];
    }

    // The status line. One line, always present so the section cannot change
    // height when it populates — an empty FText still reserves the row.
    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space12, 0.0f, 0.0f)
    [
        SNew(SBox).HeightOverride(20.0f)
        [
            BreakerMonoText(KeybindStatus, BreakerUI::TypeCaption, bKeybindStatusIsClash ? Harm : SoftText, 0.16f)
        ]
    ];

    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [
            BreakerSettingsGhostButton(TEXT("RESET ALL KEYBINDS"), FOnClicked::CreateLambda([this]()
            {
                if (UBreakerGameSettings* Live = GameSettings.Get())
                {
                    Live->ResetKeybindsToDefault();
                    Live->Save();
                    KeybindStatus = FText::FromString(TEXT("EVERY ACTION IS BACK ON ITS DEFAULT KEY."));
                    bKeybindStatusIsClash = false;
                }
                CancelKeybindListen();
                Rebuild(EBreakerMenuScreen::Settings);
                return FReply::Handled();
            }), 240.0f)
        ]
    ];
    // The honest line, second edition. The disclosure that used to end this
    // sentence ("live input still uses the default keys") became FALSE when
    // ABreakerCharacter grew ApplyKeybindOverrides — overrides now rebuild a
    // transient mapping context at spawn and live on change — so it is gone.
    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        BreakerMonoText(FText::FromString(TEXT("REBINDS SAVE TO YOUR PROFILE AND APPLY IMMEDIATELY.  ESC CANCELS A LISTENING ROW.")),
            BreakerUI::TypeCaption, Muted, 0.16f)
    ];
    return Section;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsVideoSection()
{
    UBreakerGameSettings* Model = GameSettings.Get();
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    Section->AddSlot().AutoHeight()
    [
        BreakerSettingsPaneHeader(TEXT("VIDEO"), TEXT("Window, frame cap, and field of view. Applied on change."))
    ];
    if (!Model) return Section;

    // Built first, captured by value — see the note in BuildSettingsInputSection.
    TSharedPtr<STextBlock> FOVReadout;
    const TSharedRef<SWidget> FOVValue =
        SNew(SBox).WidthOverride(BreakerSettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(FOVReadout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.0f"), Model->FieldOfView)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Primary)
                .Font(BreakerMonoFont(BreakerUI::TypeBody))
        ];

    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
    [
        BreakerSettingsRow(TEXT("Field of view"), FString(),
            SNew(SBox).WidthOverride(BreakerSettingsSliderWidth)
            [
                // 70 .. 120 (ClampFOV, BreakerGameSettings.cpp:22-25), the
                // same span the previous screen used and the same one
                // ABreakerCharacter::BaseFieldOfView enforces.
                SNew(SBreakerFieldplateSlider)
                .Value((Model->FieldOfView - 70.0f) / 50.0f)
                .OnValueChanged(FOnFloatValueChanged::CreateLambda([this, FOVReadout](float Value)
                {
                    UBreakerGameSettings* Live = GameSettings.Get();
                    if (!Live) return;
                    Live->FieldOfView = UBreakerGameSettingsLibrary::ClampFOV(70.0f + Value * 50.0f);
                    if (Character.IsValid())
                    {
                        Character->ApplyMenuSettings(Live->MouseSensitivity, Live->FieldOfView, Live->bInvertVerticalLook);
                    }
                    if (FOVReadout.IsValid())
                    {
                        FOVReadout->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Live->FieldOfView)));
                    }
                }))
                .OnCaptureEnd(FSimpleDelegate::CreateLambda([this]() { if (GameSettings.IsValid()) GameSettings->Save(); }))
            ],
            FOVValue)
    ];
    Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];

    // ---- Window mode -------------------------------------------------------
    // A dropdown over the whole closed set — the pack's control for a closed
    // choice. Video is the one group that reaches the engine, and it does so
    // IMMEDIATELY: a mode that only took effect on restart would be
    // indistinguishable from a broken one.
    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
    [
        BreakerSettingsRow(TEXT("Window mode"), FString(),
            SNew(SBreakerFieldplateDropdown)
            .Options(TArray<FString>({ TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED") }))
            .SelectedIndex(static_cast<int32>(Model->WindowMode))
            .OnPicked(FBreakerOnPick::CreateLambda([this](int32 Index)
            {
                if (UBreakerGameSettings* Live = GameSettings.Get())
                {
                    Live->WindowMode = static_cast<EBreakerWindowMode>(
                        FMath::Clamp(Index, 0, static_cast<int32>(EBreakerWindowMode::Windowed)));
                    Live->Save();
                    Live->ApplyToEngine();
                }
                Rebuild(EBreakerMenuScreen::Settings);
                return;
            })),
            SNew(SBox).WidthOverride(BreakerSettingsValueWidth))
    ];
    Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];

    // ---- Frame cap ---------------------------------------------------------
    // Still a closed set, for the reason the old chip strip documented: a
    // SLIDER would bury "uncapped" as a magic zero at one end of a continuum.
    // Every offered value is inside ClampFrameRateCap's [30, 360] band, or the
    // 0 sentinel (BreakerGameSettings.cpp:27-33), so no row in this list can
    // be silently rewritten by the clamp into a different row's value. A
    // hand-edited ini can hold any clamped value, so an unlisted one is shown
    // as its own row rather than lighting the wrong one.
    TArray<FString> CapLabels = { TEXT("UNCAPPED"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("240"), TEXT("360") };
    TArray<float> CapValues = { 0.0f, 60.0f, 120.0f, 144.0f, 240.0f, 360.0f };
    int32 CapIndex = INDEX_NONE;
    for (int32 Index = 0; Index < CapValues.Num(); ++Index)
    {
        if (FMath::IsNearlyEqual(Model->FrameRateCapFPS, CapValues[Index]))
        {
            CapIndex = Index;
            break;
        }
    }
    if (CapIndex == INDEX_NONE)
    {
        CapLabels.Add(FString::Printf(TEXT("%.0f"), Model->FrameRateCapFPS));
        CapValues.Add(Model->FrameRateCapFPS);
        CapIndex = CapValues.Num() - 1;
    }
    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
    [
        BreakerSettingsRow(TEXT("Frame rate cap"), FString(),
            SNew(SBreakerFieldplateDropdown)
            .Options(CapLabels)
            .SelectedIndex(CapIndex)
            .OnPicked(FBreakerOnPick::CreateLambda([this, CapValues](int32 Index)
            {
                if (UBreakerGameSettings* Live = GameSettings.Get())
                {
                    Live->FrameRateCapFPS = UBreakerGameSettingsLibrary::ClampFrameRateCap(
                        CapValues.IsValidIndex(Index) ? CapValues[Index] : 0.0f);
                    Live->Save();
                    Live->ApplyToEngine();
                }
                Rebuild(EBreakerMenuScreen::Settings);
                return;
            })),
            SNew(SBox).WidthOverride(BreakerSettingsValueWidth))
    ];
    Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];

    // ---- Vertical sync ------------------------------------------------------
    TSharedPtr<STextBlock> VSyncWord;
    const TSharedRef<SWidget> VSyncWordWidget =
        SAssignNew(VSyncWord, STextBlock)
            .Text(FText::FromString(Model->bVSyncEnabled ? TEXT("ON") : TEXT("OFF")))
            .ColorAndOpacity(Model->bVSyncEnabled ? SoftText : Muted)
            .Font(BreakerMonoFont(BreakerUI::TypeCaption, 0.16f));

    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
    [
        BreakerSettingsRow(TEXT("Vertical sync"), FString(),
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBreakerFieldplateToggle)
                .IsOn(Model->bVSyncEnabled)
                .OnToggled(FBreakerOnToggle::CreateLambda([this, VSyncWord](bool bOn)
                {
                    if (UBreakerGameSettings* Live = GameSettings.Get())
                    {
                        Live->bVSyncEnabled = bOn;
                        Live->Save();
                        Live->ApplyToEngine();
                    }
                    if (VSyncWord.IsValid())
                    {
                        VSyncWord->SetText(FText::FromString(bOn ? TEXT("ON") : TEXT("OFF")));
                        VSyncWord->SetColorAndOpacity(bOn ? SoftText : Muted);
                    }
                }))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
            [
                VSyncWordWidget
            ],
            SNew(SBox).WidthOverride(BreakerSettingsValueWidth))
    ];
    Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];
    return Section;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsAudioSection()
{
    UBreakerGameSettings* Model = GameSettings.Get();
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    // The subtitle is the same honesty the model's own header carries: all
    // three values persist and route nowhere yet. A player who drags these
    // and hears no change should be told why on the screen, not left to
    // conclude the sliders are broken. The sidebar's STUB mark on this pane
    // states the same fact and both leave together, when the routing lands.
    Section->AddSlot().AutoHeight()
    [
        BreakerSettingsPaneHeader(TEXT("AUDIO"), TEXT("Three volumes. Saved; not yet routed to the sound path."))
    ];
    if (!Model) return Section;

    // All three are the same control over a different float, so they are built
    // by one lambda rather than copied three times. The member pointer is what
    // keeps it one function: each slider needs to write a DIFFERENT field of
    // the same live object, looked up fresh inside the handler (the model is
    // reloaded on screen entry, so capturing the object here would be a stale
    // pointer after a rebuild).
    auto AddVolumeRow = [this, &Section](const FString& Label, float UBreakerGameSettings::* Field, float Initial)
    {
        TSharedPtr<STextBlock> Readout;
        const TSharedRef<SWidget> ValueColumn = SNew(SBox).WidthOverride(BreakerSettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(Readout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.0f%%"), Initial * 100.0f)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Primary)
                .Font(BreakerMonoFont(BreakerUI::TypeBody))
        ];
        Section->AddSlot().AutoHeight().Padding(0.0f, BreakerSettingsRowPad)
        [
            BreakerSettingsRow(Label, FString(),
                SNew(SBox).WidthOverride(BreakerSettingsSliderWidth)
                [
                    SNew(SBreakerFieldplateSlider)
                    .Value(Initial)   // volumes are already 0..1, so no remap
                    .OnValueChanged(FOnFloatValueChanged::CreateLambda([this, Field, Readout](float Value)
                    {
                        UBreakerGameSettings* Live = GameSettings.Get();
                        if (!Live) return;
                        Live->*Field = UBreakerGameSettingsLibrary::ClampVolume(Value);
                        if (Readout.IsValid())
                        {
                            Readout->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Live->*Field * 100.0f)));
                        }
                    }))
                    .OnCaptureEnd(FSimpleDelegate::CreateLambda([this]()
                    {
                        if (UBreakerGameSettings* Live = GameSettings.Get())
                        {
                            Live->Save();
                            Live->ApplyToEngine();
                        }
                    }))
                ],
                ValueColumn)
        ];
        Section->AddSlot().AutoHeight()[BreakerSettingsDivider()];
    };
    AddVolumeRow(TEXT("Master volume"), &UBreakerGameSettings::MasterVolume, Model->MasterVolume);
    AddVolumeRow(TEXT("Effects volume"), &UBreakerGameSettings::EffectsVolume, Model->EffectsVolume);
    AddVolumeRow(TEXT("Music volume"), &UBreakerGameSettings::MusicVolume, Model->MusicVolume);
    return Section;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsScreen()
{
    EnsureSettingsLoaded();

    // One pane at a time — the reference is a sidebar layout, not a scroll of
    // stacked sections.
    TSharedRef<SWidget> Pane =
          SettingsPane == 1 ? BuildSettingsKeybindSection()
        : SettingsPane == 2 ? BuildSettingsVideoSection()
        : SettingsPane == 3 ? BuildSettingsAudioSection()
        : BuildSettingsInputSection();

    // ---- Sidebar navigation ------------------------------------------------
    // 44px rows, active row on the plate face behind a 3px cyan rail, group
    // captions in tracked mono. The reference lists GAMEPLAY and
    // ACCESSIBILITY as well; neither has a model behind it yet, so both are
    // painted disabled with the STUB mark rather than drawn live over nothing
    // — a nav entry that opens an empty pane would be reachable content
    // nothing pays for.
    auto MakeNavGroup = [](const FString& Label) -> TSharedRef<SWidget>
    {
        return SNew(SBox).Padding(FMargin(BreakerSettingsContentPad, BreakerUI::Space24, BreakerUI::Space24, BreakerUI::Space8))
        [
            BreakerMonoText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, 0.16f)
        ];
    };
    auto MakeNavRow = [this](const FString& Label, int32 Pane, bool bEnabled, bool bStub) -> TSharedRef<SWidget>
    {
        const bool bActive = bEnabled && SettingsPane == Pane;
        TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
        Content->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            MenuText(FText::FromString(Label), BreakerUI::TypeBody, bEnabled ? Primary : Disabled, true)
        ];
        if (bStub)
        {
            Content->AddSlot().AutoWidth().VAlign(VAlign_Center)[BreakerSettingsStubMark()];
        }

        const FMargin RowPad(BreakerSettingsContentPad - BreakerUI::RailThickness, 0.0f, BreakerUI::Space24, 0.0f);
        TSharedRef<SWidget> Face = bEnabled
            ? StaticCastSharedRef<SWidget>(
                SNew(SButton)
                .ButtonColorAndOpacity(bActive ? Panel : Transparent)
                .ContentPadding(RowPad)
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                .OnClicked(FOnClicked::CreateLambda([this, Pane]()
                {
                    if (SettingsPane != Pane)
                    {
                        SettingsPane = Pane;
                        CancelKeybindListen();
                        Rebuild(EBreakerMenuScreen::Settings);
                    }
                    return FReply::Handled();
                }))
                [
                    Content
                ])
            : StaticCastSharedRef<SWidget>(
                // Painted, never faded: geometry kept, accent stripped, the
                // STUB mark says why in words.
                SNew(SBox).Padding(RowPad).VAlign(VAlign_Center)[Content]);

        return SNew(SBox).HeightOverride(BreakerSettingsNavRowHeight)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).WidthOverride(BreakerUI::RailThickness)
                [
                    SolidBlock(bActive ? Cyan : Transparent)
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)[Face]
        ];
    };

    TSharedRef<SVerticalBox> Nav = SNew(SVerticalBox);
    Nav->AddSlot().AutoHeight()[MakeNavGroup(TEXT("PLAY"))];
    Nav->AddSlot().AutoHeight()[MakeNavRow(TEXT("INPUT"), 0, true, false)];
    Nav->AddSlot().AutoHeight()[MakeNavRow(TEXT("KEYBINDS"), 1, true, false)];
    Nav->AddSlot().AutoHeight()[MakeNavRow(TEXT("GAMEPLAY"), INDEX_NONE, false, true)];
    Nav->AddSlot().AutoHeight()[MakeNavGroup(TEXT("PRESENT"))];
    Nav->AddSlot().AutoHeight()[MakeNavRow(TEXT("VIDEO"), 2, true, false)];
    Nav->AddSlot().AutoHeight()[MakeNavRow(TEXT("ACCESSIBILITY"), INDEX_NONE, false, true)];
    // AUDIO opens and its sliders save; the STUB mark stays until the values
    // route to a sound path (see BuildSettingsAudioSection).
    Nav->AddSlot().AutoHeight()[MakeNavRow(TEXT("AUDIO"), 3, true, true)];
    Nav->AddSlot().FillHeight(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    // The sidebar's standing fact, from the plate: this screen is the same
    // screen whichever door it was entered through — Main's SETTINGS button
    // and the pause menu both land here, and RootScreen only decides where
    // BACK returns.
    Nav->AddSlot().AutoHeight().Padding(FMargin(BreakerSettingsContentPad, 0.0f, BreakerUI::Space24, BreakerUI::Space24))
    [
        SNew(STextBlock)
            .Text(FText::FromString(TEXT("SETTINGS IS IDENTICAL FROM TITLE AND FROM PAUSE")))
            .ColorAndOpacity(Muted)
            .WrapTextAt(220.0f)
            .Font(BreakerMonoFont(BreakerUI::TypeCaption, 0.16f))
    ];

    const TSharedRef<SWidget> Sidebar = SNew(SBox).WidthOverride(BreakerSettingsSidebarWidth)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SNew(SOverlay)
            + SOverlay::Slot()[SolidBlock(BreakerUI::BgRaised)]
            + SOverlay::Slot()[Nav]
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(BreakerUI::BorderThin)[SolidBlock(BorderRest)]
        ]
    ];

    // ---- Header band -------------------------------------------------------
    // 87px + its 1px divider: the drawn « chevrons and BACK, then the screen
    // title. BACK is the same GoBack Escape drives, so the two doors agree.
    const TSharedRef<SWidget> Header = SNew(SBox).HeightOverride(87.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerSettingsContentPad, 0.0f, 0.0f, 0.0f)
        [
            SNew(SBox).HeightOverride(BreakerUI::MinHitTarget)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(Transparent)
                .ContentPadding(FMargin(0.0f, 0.0f, BreakerUI::Space8, 0.0f))
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .OnClicked(FOnClicked::CreateSP(this, &SBreakerMenu::GoBack))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BreakerBackChevrons()]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
                    [
                        BreakerMonoText(FText::FromString(TEXT("BACK")), BreakerUI::TypeCaption, Muted, 0.16f)
                    ]
                ]
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("SETTINGS")), BreakerUI::TypeH1, Primary, true)
        ]
    ];

    // Full-bleed, not a centred plate: the reference is a full-screen layout
    // — header band, sidebar, one pane at a time — and fixed-geometry zones
    // cannot jitter, which is all BuildFrame's fixed plate existed to stop.
    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BreakerUI::BgBase)
        ]
        + SOverlay::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Header]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(BorderRest)]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[Sidebar]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot().Padding(FMargin(BreakerSettingsContentPad, BreakerUI::Space24,
                        BreakerSettingsContentPad, BreakerUI::Space40))
                    [
                        Pane
                    ]
                ]
            ]
        ];
}

void SBreakerMenu::BeginKeybindListen(FName Action)
{
    ListeningKeybindAction = Action;
    PendingKeybindAction = NAME_None;
    PendingKeybindKey = FKey();
    KeybindStatus = FText::FromString(FString::Printf(TEXT("PRESS A KEY OR MOUSE BUTTON FOR %s.  ESC CANCELS."),
        *UBreakerGameSettingsLibrary::DescribeAction(Action).ToString()));
    bKeybindStatusIsClash = false;

    // Ask for keyboard focus explicitly. The click that got us here has
    // already put Slate focus on the key control — a descendant of this
    // widget, which is enough for OnPreviewKeyDown to run on the way down —
    // but the button is about to be destroyed by the rebuild below, and a
    // focus path whose tail has been deleted is exactly the state this file's
    // existing comments describe going wrong. Taking focus onto the menu root
    // survives the rebuild, because the root is what persists across it.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
    }
    Rebuild(EBreakerMenuScreen::Settings);
}

void SBreakerMenu::CancelKeybindListen()
{
    ListeningKeybindAction = NAME_None;
    PendingKeybindAction = NAME_None;
    PendingKeybindKey = FKey();
}

void SBreakerMenu::CommitKeybind(const FKey& Key)
{
    const FName Action = ListeningKeybindAction;
    if (Action == NAME_None) return;

    UBreakerGameSettings* Model = GameSettings.Get();
    if (!Model || !Key.IsValid())
    {
        CancelKeybindListen();
        KeybindStatus = FText::FromString(TEXT("THAT INPUT CANNOT BE BOUND."));
        bKeybindStatusIsClash = true;
        Rebuild(EBreakerMenuScreen::Settings);
        return;
    }

    // THE SECOND TAKE. README: "first press names the clash and commits
    // nothing; second takes it" — the same key arriving while its own clash
    // is on the board is the player answering yes. This is also the mouse
    // path's confirm: a mouse-button clash re-arrives through
    // OnPreviewMouseButtonDown exactly like a key does.
    if (PendingKeybindAction == Action && PendingKeybindKey == Key)
    {
        CancelKeybindListen();
        Model->SetKeybindOverride(Action, Key);
        Model->Save();
        KeybindStatus = FText::FromString(FString::Printf(TEXT("%s BOUND TO %s — THE KEY IS NOW SHARED."),
            *UBreakerGameSettingsLibrary::DescribeAction(Action).ToString(),
            *Key.GetDisplayName().ToString().ToUpper()));
        bKeybindStatusIsClash = false;
        Rebuild(EBreakerMenuScreen::Settings);
        return;
    }

    const TMap<FName, FKey> FlatDefaults = UBreakerGameSettingsLibrary::FirstKeyPerAction(DefaultKeybinds);
    FName ClashWith = NAME_None;
    if (UBreakerGameSettingsLibrary::FindKeybindConflict(Action, Key, Model->KeybindOverrides, FlatDefaults, ClashWith))
    {
        // NAMED, not refused and not stolen. The model's own header says two
        // actions sharing a key is sometimes a deliberate choice, which is why
        // FindKeybindConflict is offered separately from SetKeybindOverride
        // rather than being enforced inside it (BreakerGameSettings.h:208-211).
        // The row KEEPS LISTENING, because the answer is another press: the
        // same key to share, a different key to try instead, Escape to walk
        // away. Nothing was written by this press.
        PendingKeybindAction = Action;
        PendingKeybindKey = Key;
        KeybindStatus = FText::FromString(FString::Printf(TEXT("%s IS HELD BY %s. PRESS %s AGAIN TO SHARE IT, OR A DIFFERENT KEY.  ESC CANCELS."),
            *Key.GetDisplayName().ToString().ToUpper(),
            *UBreakerGameSettingsLibrary::DescribeAction(ClashWith).ToString(),
            *Key.GetDisplayName().ToString().ToUpper()));
        bKeybindStatusIsClash = true;
        Rebuild(EBreakerMenuScreen::Settings);
        return;
    }

    CancelKeybindListen();
    Model->SetKeybindOverride(Action, Key);
    Model->Save();
    KeybindStatus = FText::FromString(FString::Printf(TEXT("%s BOUND TO %s."),
        *UBreakerGameSettingsLibrary::DescribeAction(Action).ToString(),
        *Key.GetDisplayName().ToString().ToUpper()));
    bKeybindStatusIsClash = false;
    Rebuild(EBreakerMenuScreen::Settings);
}

void SBreakerMenu::HandleRebindKey(const FKey& Key)
{
    // See the header comment: the seam for a player-input AnyKey bind, live
    // only while a row is listening.
    if (ListeningKeybindAction == NAME_None) return;
    if (Key == EKeys::Escape)
    {
        CancelKeybindListen();
        KeybindStatus = FText::FromString(TEXT("REBIND CANCELLED."));
        bKeybindStatusIsClash = false;
        Rebuild(EBreakerMenuScreen::Settings);
        return;
    }
    CommitKeybind(Key);
}

TSharedRef<SWidget> SBreakerMenu::MakeGearCard(const FText& Slot, const FText& Name, const FText& Details, const FLinearColor& Accent) const
{
    // Card face stays panel/10 at every rarity so a wall of loot does not
    // become a wall of colour; the accent lives on the rail and the name.
    return MakePlate(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[MenuText(Slot, BreakerUI::TypeCaption, Muted, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space4)[MenuText(Name, BreakerUI::TypeH2, Accent, true)]
        + SVerticalBox::Slot().AutoHeight()[MenuText(Details, BreakerUI::TypeCaption, SoftText)],
        PanelRaised, Accent, FMargin(BreakerUI::Space16, BreakerUI::Space16));
}

// BuildLoadoutScreen is GONE (owner ruling 2026-08-17: "the loadout button and
// the ability to just pick a weapon shouldnt exist / players should start with
// a basic rifle"). The archetype tiles let a player conjure any of the eight
// guns with no item behind them; the weapon you hold now comes from the weapon
// ITEM equipped in Primary/Secondary (UBreakerWeaponComponent::
// SyncArchetypesToEquipment), and every fresh character spawns holding the
// Issue Rifle (UBreakerEquipmentComponent::EnsureStarterKit). Nothing was
// rehomed from the screen: everything on it was archetype furniture (the
// picker tiles and an armory reference listing of the same eight rows), and
// ability selection already lives on the ABILITIES tab.

namespace
{
    // One rarity ramp for the whole game: the same values the HUD draws a
    // ground drop's rail and beam with.
    FLinearColor RarityColor(EBreakerItemRarity Rarity)
    {
        return BreakerUI::RarityColor(Rarity);
    }

    // A card whose rarity reads from its 3px left rail. Anomalous also takes
    // a full 1px border, because it is the only tier that is simultaneously a
    // world object class.
    TSharedRef<SWidget> MakeRarityCard(const TSharedRef<SWidget>& Inner, EBreakerItemRarity Rarity, bool bHasItem)
    {
        const FLinearColor Rail = bHasItem ? BreakerUI::RarityColor(Rarity) : BreakerUI::BorderEmphasis;
        const FLinearColor Ring = bHasItem && BreakerUI::RarityGetsFullBorder(Rarity)
            ? BreakerUI::RarityColor(Rarity) : BreakerUI::BorderRest;
        return BorderWrap(
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(BreakerUI::RailThickness)[SolidBlock(Rail)]]
            + SHorizontalBox::Slot().FillWidth(1.0f)[Inner],
            Ring);
    }

    FString RarityName(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon: return TEXT("UNCOMMON");
            case EBreakerItemRarity::Exceptional: return TEXT("EXCEPTIONAL");
            case EBreakerItemRarity::Aberrant: return TEXT("ABERRANT");
            // O50: the DISPLAY name is Unwritten. O49 gave "Anomalies" to the
            // endgame content type, so the rarity gave up the word. The
            // ENUMERATOR stays Anomalous and must never move -- it is
            // serialized, and CLAUDE.md's append-only rule protects it
            // independently of this rename.
            case EBreakerItemRarity::Anomalous: return TEXT("UNWRITTEN");
            default: return TEXT("STANDARD");
        }
    }

    FString SlotName(EBreakerEquipSlot Slot)
    {
        switch (Slot)
        {
            case EBreakerEquipSlot::Helmet: return TEXT("HELMET");
            case EBreakerEquipSlot::BodyArmour: return TEXT("BODY ARMOUR");
            case EBreakerEquipSlot::Gloves: return TEXT("GLOVES");
            case EBreakerEquipSlot::Boots: return TEXT("BOOTS");
            case EBreakerEquipSlot::Necklace: return TEXT("NECKLACE");
            case EBreakerEquipSlot::Waist: return TEXT("WAIST");
            case EBreakerEquipSlot::Primary: return TEXT("PRIMARY");
            case EBreakerEquipSlot::Secondary: return TEXT("SECONDARY");
            default: return TEXT("SLOT");
        }
    }

    // What to print on an item card's slot line. For armour that is the slot;
    // for a weapon it is the GUN, because "PRIMARY" tells the player nothing
    // they did not already know from where the card sits, while "SIDEARM" is
    // the entire reason they are looking at it. Weapon drops randomise their
    // archetype, so this is the only place the class is visible before
    // equipping.
    FString ItemSlotLabel(const FBreakerItemInstance& Item)
    {
        if (Item.IsWeapon())
        {
            return FString::Printf(TEXT("%s · %s"),
                *SlotName(Item.Slot), *BreakerWeaponArchetypeNames::Short(Item.WeaponArchetype));
        }
        return SlotName(Item.Slot);
    }

    FString ClassDisplayName(EBreakerClassId ClassId)
    {
        switch (ClassId)
        {
            case EBreakerClassId::Caster:   return TEXT("CASTER");
            case EBreakerClassId::Swift:    return TEXT("SWIFT");
            case EBreakerClassId::Gunsmith: return TEXT("GUNSMITH");
            case EBreakerClassId::Tank:     return TEXT("TANK");
            case EBreakerClassId::Support:  return TEXT("SUPPORT");
            default:                        return TEXT("UNCLASSED");
        }
    }

    // O39 SLICE CLASS HONESTY: is this class a real choice, or would picking
    // it permanently strand a character on nothing? A RUNTIME QUERY, not a
    // hardcoded list, so a class unlocks itself the day its kit lands with no
    // edit here.
    //
    // Deliberately NOT UBreakerProgressionLibrary::GetFallbackClassDefinition
    // != nullptr: that function is Swift-only today ("Only Swift is authored
    // in full for the slice", Progression/BreakerProgressionLibrary.cpp) and
    // would misclassify Caster as unimplemented — contradicting O39's own
    // text ("today Swift and Caster") and the fact that Caster is fully
    // playable on E/T/G. THE ABILITY CATALOGUE is the query that actually
    // answers "does this class have a kit": UBreakerAbilityDefinition's
    // fallback registry has rows for Swift and Caster and nothing else
    // (Abilities/BreakerAbilityDefinition.cpp), and it is the same registry
    // the ABILITIES tab reads to build its picker — one fact, two screens.
    bool ClassHasImplementedKit(EBreakerClassId ClassId)
    {
        // ASKS WHETHER THE ABILITIES EXECUTE, not whether rows exist. Counting
        // registry rows was a safe proxy for exactly as long as every row had a
        // real AbilityClass behind it, and it stopped being one the moment
        // Gunsmith, Tank and Support gained 21 catalogued-but-unimplemented
        // rows: this function immediately began reporting all three as
        // implemented, which would have offered them on the class screen and
        // let a player PERMANENTLY lock a character into a class that grants
        // nothing. That is precisely the trap O39 exists to close.
        //
        // The progression layer would still have refused the lock, so the
        // damage was presentational — but a class screen that offers a choice
        // the game then rejects is its own defect.
        return UBreakerAbilityDefinition::ClassHasImplementedKit(ClassId);
    }

    // The two bulk-discard thresholds the header offers, indexed by arm. One
    // function so the chip, the modal's count and the modal's Destroy button
    // cannot disagree about what "below" means.
    EBreakerItemRarity CleanupThresholdForArm(int32 ArmIndex)
    {
        return ArmIndex == 1 ? EBreakerItemRarity::Exceptional : EBreakerItemRarity::Uncommon;
    }

    FString TierLabel(int32 Tier)
    {
        return Tier < 0 ? TEXT("T-1") : FString::Printf(TEXT("T%d"), Tier);
    }

    // One affix as the player reads it: "+5.0% Movement Speed  T4".
    //
    // VALUE FIRST, which is the reference's own order ("+22% slide speed") and
    // is also what makes the line survive a narrow card: the value is short and
    // fixed-ish, the name is long and variable, so leading with the value puts
    // the wrap point inside the name where a line break is harmless. Name-first
    // put the wrap point between the name and its number, which reads as two
    // unrelated fragments.
    FString DescribeAffix(const FBreakerRolledAffix& Affix)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Affix.AffixId);
        const FString Name = Definition ? Definition->DisplayName.ToString() : Affix.AffixId.ToString();
        const bool bPercent = Definition && Definition->StatBucket != EBreakerStatBucket::Flat;
        // Critical Chance and Critical Damage roll as flat numbers but are
        // printed as percentages, because that is what they mean.
        const bool bPercentStyleFlat = Definition &&
            (Definition->StatTarget == EBreakerStatTarget::CriticalChance || Definition->StatTarget == EBreakerStatTarget::CriticalDamage);
        return FString::Printf(TEXT("+%.1f%s %s  %s"), Affix.Value,
            bPercent || bPercentStyleFlat ? TEXT("%") : TEXT(""), *Name, *TierLabel(Affix.Tier));
    }

    // DescribeItem (a printf blob of item level plus every affix on one string)
    // is GONE with the equipment column's rebuild: the reference's equipment
    // row is a name, a level and a rarity tag, and its affix detail belongs on
    // the backpack card where the deltas are. MakeAffixLines is the one place
    // an affix list is built now.

    // The affix list with per-affix deltas (reference, CARD ANATOMY line 3):
    // "the affix list, each affix carrying its delta against the equipped
    // piece: cyan up, red down, muted = for parity".
    //
    // THE LINE THAT CLIPPED. The affix text used to sit in a bare FillWidth
    // slot with two fixed columns beside it, so on a 300px card the stat names
    // came out as "Physical Damag+" and "Dash Cooldown+". It now WRAPS at a
    // pixel width derived from the card's own width — computed before layout,
    // never from an allotted size — so a long name takes a second line instead
    // of losing its last word. The two right-hand columns stay fixed so the
    // glyphs and magnitudes keep a straight edge, and the magnitude cannot be
    // shaved by its own measured width (MenuValueColumn's comment has that
    // mechanism).
    //
    // Deltas is UBreakerEquipmentComponent's answer, one row per affix in the
    // same order as Item.Affixes — this function decides nothing about better
    // or worse, it only picks a glyph and a colour. Pass an empty array for a
    // card with nothing to compare against (an equipped piece).
    TSharedRef<SWidget> MakeAffixLines(const FBreakerItemInstance& Item, const TArray<FBreakerAffixComparison>& Deltas,
        float CardWidth)
    {
        const float NameWrap = BreakerInventoryLayout::AffixNameWrapWidth(CardWidth);
        TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
        for (int32 Index = 0; Index < Item.Affixes.Num(); ++Index)
        {
            FString Glyph;
            FString Magnitude;
            FLinearColor GlyphColor = Muted;
            if (Deltas.IsValidIndex(Index))
            {
                const FBreakerAffixComparison& Comparison = Deltas[Index];
                Glyph = BreakerInventoryLayout::DeltaGlyph(Comparison.Delta);
                Magnitude = BreakerInventoryLayout::FormatDelta(Comparison);
                switch (Comparison.Delta)
                {
                    case EBreakerAffixDelta::Better: GlyphColor = Cyan; break;
                    case EBreakerAffixDelta::Worse:  GlyphColor = Harm; break;
                    default:                         GlyphColor = Muted; break;
                }
            }
            Lines->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
            [
                SNew(SHorizontalBox)
                // VAlign_Top on the two right columns: when the name takes a
                // second line the glyph and its magnitude stay with the FIRST
                // line, which is the one carrying the stat.
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    MenuWrappedText(FText::FromString(DescribeAffix(Item.Affixes[Index])),
                        BreakerUI::TypeCaption, SoftText, NameWrap)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                [
                    SNew(SBox).WidthOverride(BreakerUI::DeltaGlyphColumn)
                    [
                        MenuText(FText::FromString(Glyph), BreakerUI::TypeCaption, GlyphColor, true)
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                [
                    MenuValueColumn(FText::FromString(Magnitude), BreakerInventoryLayout::DeltaValueColumn,
                        BreakerUI::TypeCaption, GlyphColor)
                ]
            ];
        }
        return Lines;
    }

    // WHAT THE CARD CALLS THIS ITEM.
    //
    // The reference's line one is "name plus item level — the two things
    // scanned first", and its sample names ("Riftstep Greaves") are authored
    // copy. FBreakerItemInstance has no display name: the only named items in
    // the game are the legendaries, which carry one on
    // FBreakerLegendaryDefinition. Everything else is identified by what it is,
    // which for a weapon is its ARCHETYPE (the one fact not already implied by
    // where the card sits) and for armour is its slot.
    //
    // This is a CONTENT GAP, not a layout choice: line two still prints rarity
    // and slot as the reference asks, so an unnamed piece of armour repeats its
    // slot on both lines until item names exist.
    FString ItemDisplayName(const FBreakerItemInstance& Item)
    {
        if (Item.IsLegendary())
        {
            const FBreakerLegendaryDefinition Legendary = UBreakerItemRuleLibrary::FindLegendary(Item.LegendaryId);
            if (Legendary.IsValid() && !Legendary.DisplayName.IsEmpty())
            {
                return Legendary.DisplayName.ToString().ToUpper();
            }
        }
        // The starter. The one non-legendary item with a name, because it is
        // the one non-legendary item every character is guaranteed to meet:
        // standard-issue kit says so on the card, and the first drop that
        // outclasses it reads as an upgrade over "the gun they gave me".
        if (Item.DefinitionId == UBreakerEquipmentComponent::StarterRifleDefinitionId)
        {
            return TEXT("ISSUE RIFLE");
        }
        if (Item.IsWeapon())
        {
            return BreakerWeaponArchetypeNames::Short(Item.WeaponArchetype);
        }
        return SlotName(Item.Slot);
    }

    // Line two: rarity and slot. Aberrant and Anomalous say so in their own
    // colour, because those two are the tiers with a cap behind them; the rest
    // stay muted so a wall of loot does not become a wall of colour.
    FString ItemRarityAndSlot(const FBreakerItemInstance& Item)
    {
        return FString::Printf(TEXT("%s · %s"), *RarityName(Item.Rarity), *ItemSlotLabel(Item));
    }

    FLinearColor RarityTagColor(EBreakerItemRarity Rarity)
    {
        return Rarity == EBreakerItemRarity::Aberrant || Rarity == EBreakerItemRarity::Anomalous
            ? BreakerUI::RarityColor(Rarity) : Muted;
    }

    // A dashed hairline running DOWN. Mirror of DashedLine; the pair is what an
    // empty slot's border is made of, since Slate has no dash pattern.
    TSharedRef<SWidget> DashedColumn(float Height, const FLinearColor& Color, float Dash = 6.0f, float Gap = 6.0f)
    {
        TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
        const int32 Count = FMath::Clamp(FMath::CeilToInt(Height / (Dash + Gap)), 1, 240);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Gap)
            [
                SNew(SBox).HeightOverride(Dash)[SolidBlock(Color)]
            ];
        }
        return Column;
    }

    // The dashed ring an EMPTY slot keeps: "empty slots keep full geometry with
    // a dashed border and the slot name — the doll never looks broken, only
    // unfinished". Four runs at known pixel sizes, never an allotted one.
    TSharedRef<SWidget> DashedFrame(float Width, float Height, const FLinearColor& Color, const TSharedRef<SWidget>& Inner,
        const FMargin& ContentPadding = FMargin(BreakerUI::Space16, BreakerUI::Space8))
    {
        return SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)[DashedLine(Width, Color)]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)[DashedLine(Width, Color)]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)[DashedColumn(Height, Color)]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)[DashedColumn(Height, Color)]
            + SOverlay::Slot().Padding(ContentPadding)[Inner];
    }

    // The five rarity beams, as the empty backpack draws them: one vertical
    // bar per tier in the same ramp the ground drops use, so the screen and
    // the world teach the same lesson.
    TSharedRef<SWidget> MakeRarityBeams()
    {
        static const EBreakerItemRarity Ramp[] =
        {
            EBreakerItemRarity::Standard,
            EBreakerItemRarity::Uncommon,
            EBreakerItemRarity::Exceptional,
            EBreakerItemRarity::Aberrant,
            EBreakerItemRarity::Anomalous,
        };
        TSharedRef<SHorizontalBox> Beams = SNew(SHorizontalBox);
        for (const EBreakerItemRarity Rarity : Ramp)
        {
            Beams->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space24, 0.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    // The beam itself: a 6px column of the rarity's own colour,
                    // the same value the HUD draws a ground drop's beam with.
                    SNew(SBox).WidthOverride(6.0f).HeightOverride(180.0f)
                    [
                        SolidBlock(BreakerUI::RarityColor(Rarity))
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(RarityName(Rarity)), BreakerUI::TypeCaption, BreakerUI::RarityColor(Rarity), true)
                ]
            ];
        }
        return Beams;
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildInventoryScreen()
{
    UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;

    // The outline handles belong to the widget tree being replaced right now.
    EquipSlotOutlines.Reset();

    // Read the viewport once, the same way the skill matrix does. This screen
    // used to be authored at a hard 1760 with two hard 560/400 columns, so it
    // could only be correct on exactly one window size. The zone arithmetic
    // below derives from the measured panel instead, and the fixed columns
    // give ground before the backpack does — the backpack is where the cards
    // are, so it is the last thing that should be squeezed.
    const FWideScreenMetrics Metrics = MeasureWideScreen();
    // The plate's INTERIOR is what the three zones divide, not the plate: the
    // 1px ring, the 3px identity rail and the 24px content pad on each side
    // are all spent before the first column starts. Dividing the outer width
    // is how a 960 backpack becomes a 900 one and the third card falls off.
    const float PlateInterior = Metrics.PanelWidth - BreakerUI::RailThickness
        - 2.0f * BreakerUI::BorderThin - 2.0f * BreakerUI::Space24;
    // Two 1px dividers between the three zones, and nothing else: the zones
    // tile the interior, as the reference draws them.
    const float ZoneGutters = 2.0f * BreakerUI::BorderThin;
    const BreakerInventoryLayout::FColumns Columns =
        BreakerInventoryLayout::SolveColumns(PlateInterior, ZoneGutters);
    const float CharacterColumnWidth = Columns.Character;
    const float EquipmentColumnWidth = Columns.Equipment;
    const float BackpackZoneWidth = Columns.Backpack;

    // One equipment row, to the reference's CARD ANATOMY for the equipment
    // column: a 44px icon square ringed in the rarity, the slot name over the
    // item name, and the item level with its rarity tag pinned right. Clicking
    // unequips.
    auto MakeEquipRow = [this, Equipment, EquipmentColumnWidth](EBreakerEquipSlot Slot) -> TSharedRef<SWidget>
    {
        FBreakerItemInstance Item;
        const bool bHasItem = Equipment && Equipment->GetEquippedItem(Slot, Item);
        const FLinearColor Accent = bHasItem ? RarityColor(Item.Rarity) : BorderEmphasis;

        // The icon square. Empty for now — there is no item art — but it holds
        // the geometry the reference gives it so the column does not re-space
        // itself the day art lands.
        TSharedRef<SWidget> Icon =
            SNew(SBox).WidthOverride(BreakerUI::MinHitTarget).HeightOverride(BreakerUI::MinHitTarget)
            [
                BorderWrap(
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(BreakerUI::BgRaised)
                    [
                        SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
                    ],
                    Accent)
            ];

        // An EMPTY slot keeps full geometry, a dashed border and its own name.
        if (!bHasItem)
        {
            TSharedRef<SWidget> EmptyBody =
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icon]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(SlotName(Slot)), BreakerUI::TypeCaption, Muted, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(TEXT("EMPTY")), BreakerUI::TypeH2, Disabled, true)
                    ]
                ];

            // The doomed-piece outline is registered for empty slots too: a
            // legendary that claims a second slot can name one that is empty,
            // and an outline that silently did nothing there would be a lie.
            TSharedRef<SBorder> EmptyOutline = SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(Background)
                .Padding(FMargin(BreakerUI::BorderSelected))
                [
                    DashedFrame(EquipmentColumnWidth - 2.0f * BreakerUI::BorderSelected,
                        BreakerInventoryLayout::EquipRowHeight, BorderEmphasis, EmptyBody)
                ];
            EquipSlotOutlines.Add(Slot, EmptyOutline);
            return SNew(SBox).HeightOverride(BreakerInventoryLayout::EquipRowHeight + 2.0f * BreakerUI::BorderSelected)[EmptyOutline];
        }

        // The right stack: item level, and the rarity tag repeated under it for
        // the two capped tiers ("on an equipped card the rarity tag repeats
        // under the item level"). 96 is sized to ANOMALOUS, the longest tag,
        // not to the first one that happened to be on screen.
        constexpr float EquipRightColumn = 96.0f;
        TSharedRef<SVerticalBox> RightStack = SNew(SVerticalBox);
        RightStack->AddSlot().AutoHeight()
        [
            MenuValueColumn(FText::FromString(FString::Printf(TEXT("i%d"), Item.ItemLevel)),
                EquipRightColumn, BreakerUI::TypeCaption, Primary)
        ];
        if (Item.Rarity == EBreakerItemRarity::Aberrant || Item.Rarity == EBreakerItemRarity::Anomalous)
        {
            RightStack->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
            [
                MenuValueColumn(FText::FromString(RarityName(Item.Rarity)),
                    EquipRightColumn, BreakerUI::TypeCaption, RarityColor(Item.Rarity))
            ];
        }

        // The doomed-piece outline. It sits OUTSIDE the card's own ring so the
        // rarity ring is never overwritten, and it rests on the screen field
        // colour, which reads as nothing until a hovered backpack card names
        // this slot.
        TSharedRef<SBorder> Outline = SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Background)
            .Padding(FMargin(BreakerUI::BorderSelected))
            [
                MakeRarityCard(
                SNew(SButton)
                .ButtonColorAndOpacity(PanelRaised)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, Slot]()
                {
                    if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->UnequipSlot(Slot);
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icon]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            MenuText(FText::FromString(SlotName(Slot)), BreakerUI::TypeCaption,
                                Item.IsWeapon() ? BreakerUI::Orange : Muted, true)
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                        [
                            // Rarity on the rail and the NAME ONLY. The face
                            // stays panel/10 at every tier. Wrapped at the room
                            // left after the icon square, the right stack and
                            // the plate's own chrome — "BODY ARMOUR" at H2 is
                            // wider than it looks.
                            MenuWrappedText(FText::FromString(ItemDisplayName(Item)), BreakerUI::TypeH2,
                                RarityColor(Item.Rarity),
                                FMath::Max(80.0f, EquipmentColumnWidth - BreakerUI::MinHitTarget - EquipRightColumn
                                    - BreakerInventoryLayout::CardChrome - BreakerUI::Space16), true)
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[RightStack]
                ],
                Item.Rarity, true)
            ];

        EquipSlotOutlines.Add(Slot, Outline);
        return SNew(SBox).HeightOverride(BreakerInventoryLayout::EquipRowHeight + 2.0f * BreakerUI::BorderSelected)[Outline];
    };

    // ---- Character column, 560 wide (reference, ZONES) --------------------
    // "a full-body render slot (560x660, silhouette placeholder for now) with
    // GEAR TOTALS pinned beneath it, so the numbers are always on screen with
    // the doll". The render slot takes the fill and the totals are AutoHeight
    // under it, which is what "pinned beneath" means in a column that stretches.
    TSharedRef<SVerticalBox> CharacterColumn = SNew(SVerticalBox);
    CharacterColumn->AddSlot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        // 660 is the authored render height and it is a MINIMUM here: the
        // column stretches with the plate and the silhouette centres in
        // whatever it is given. The slot keeps full geometry while empty —
        // the doll never looks broken, only unfinished.
        SNew(SBox).MinDesiredHeight(BreakerInventoryLayout::RenderSlotHeight)
        [
            MakePlate(
                SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(FString::Printf(
                        TEXT("FULL-BODY RENDER SLOT\n%d x %d\nSILHOUETTE PLACEHOLDER"),
                        FMath::RoundToInt(BreakerInventoryLayout::SpecCharacterColumn),
                        FMath::RoundToInt(BreakerInventoryLayout::RenderSlotHeight))),
                        BreakerUI::TypeCaption, Muted)
                ],
                BreakerUI::BgRaised, BorderEmphasis, FMargin(BreakerUI::Space16))
        ]
    ];
    {
        // TWO COLUMNS of label/value, as the reference draws them. One column
        // of twelve rows is 300px of totals under a 660px doll on a 1080 screen,
        // which is the whole reason the reference pairs them up.
        TSharedRef<SVerticalBox> Totals = SNew(SVerticalBox);
        Totals->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(TEXT("GEAR TOTALS")), BreakerUI::TypeCaption, Muted, true)
        ];
        TArray<TSharedRef<SWidget>> TotalCells;
        auto AddTotalRow = [&TotalCells](const FString& Label, const FString& Value, const FLinearColor& ValueColor)
        {
            TotalCells.Add(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    // Fixed value column so the numbers form a straight edge
                    // and never reflow as they tick. Sized to the longest value
                    // any of these rows can print ("+1234.5%"), not to the first
                    // one seen. Same clipping fix as the skill rail's totals
                    // plate — see MenuValueColumn.
                    MenuValueColumn(FText::FromString(Value), 88.0f, BreakerUI::TypeCaption, ValueColor)
                ]);
        };
        if (Equipment)
        {
            const FBreakerEquipmentStats& Stats = Equipment->GetStats();
            // Value colour is the FIELDPLATE function family: player/system
            // survivability and movement cyan, weapon stats orange, reward
            // gold. (There is no gear-granted shield stat yet — health is the
            // cyan survivability row until one exists.)
            AddTotalRow(TEXT("HEALTH"), FString::Printf(TEXT("+%.0f"), Stats.BonusHealth), Cyan);
            AddTotalRow(TEXT("MAX RESOURCE"), FString::Printf(TEXT("+%.0f"), Stats.BonusMaxResource), Cyan);
            AddTotalRow(TEXT("RESOURCE REGEN"), FString::Printf(TEXT("+%.1f/s"), Stats.ResourceRegenPerSecond), Cyan);
            AddTotalRow(TEXT("PHYS DR"), FString::Printf(TEXT("%.1f%%"), Stats.PhysicalDamageReductionPercent), Cyan);
            AddTotalRow(TEXT("MOVE SPEED"), FString::Printf(TEXT("x%.2f"), Stats.MoveSpeedMultiplier), Cyan);
            AddTotalRow(TEXT("SLIDE SPEED"), FString::Printf(TEXT("x%.2f"), Stats.SlideSpeedMultiplier), Cyan);
            AddTotalRow(TEXT("AIR CONTROL"), FString::Printf(TEXT("x%.2f"), Stats.AirControlMultiplier), Cyan);
            AddTotalRow(TEXT("DASH COOLDOWN"), FString::Printf(TEXT("x%.2f"), Stats.DashCooldownMultiplier), Cyan);
            // Read the composed attribute, not the gear-only figure. Gear,
            // skill nodes and the point-spend baseline all land in one additive
            // Increased bucket on DamageMultiplier now, and this row printing
            // only the gear half is precisely how "I spend points and damage
            // never changes" would still look true after it stopped being true.
            {
                const UBreakerAttributeSet* Attributes = Character.IsValid() ? Character->GetAttributes() : nullptr;
                const float ComposedDamage = Attributes ? Attributes->GetDamageMultiplier() : Stats.WeaponDamageMultiplier;
                AddTotalRow(TEXT("WEAPON DMG"), FString::Printf(TEXT("x%.2f"), ComposedDamage), BreakerUI::Orange);
                // O54's other lane. Both rows, for the same reason the row above
                // stopped printing only the gear half: a lane the totals panel
                // does not show is a lane a player cannot tell they are building.
                const float ComposedAbility = Attributes ? Attributes->GetAbilityDamageMultiplier() : 1.0f;
                AddTotalRow(TEXT("ABILITY DMG"), FString::Printf(TEXT("x%.2f"), ComposedAbility), BreakerUI::Orange);
            }
            AddTotalRow(TEXT("CRIT CHANCE"), FString::Printf(TEXT("+%.1f%%"), Stats.CriticalChanceBonus * 100.0f), BreakerUI::Orange);
            AddTotalRow(TEXT("CRIT DAMAGE"), FString::Printf(TEXT("+%.1f%%"), Stats.CriticalMultiplierBonus * 100.0f), BreakerUI::Orange);
            AddTotalRow(TEXT("DROP CHANCE"), FString::Printf(TEXT("+%.1f%%"), Stats.DropChancePercent), Amber);
        }
        else
        {
            AddTotalRow(TEXT("NO EQUIPMENT"), TEXT("—"), Disabled);
        }
        // Pair the cells up. An odd count leaves the last cell alone in the
        // left column rather than stretching it across both, so the label
        // column never moves between builds.
        for (int32 Index = 0; Index < TotalCells.Num(); Index += 2)
        {
            TSharedRef<SHorizontalBox> Pair = SNew(SHorizontalBox);
            Pair->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, BreakerUI::Space24, 0.0f)[TotalCells[Index]];
            Pair->AddSlot().FillWidth(1.0f)
            [
                TotalCells.IsValidIndex(Index + 1) ? TotalCells[Index + 1] : SNullWidget::NullWidget
            ];
            Totals->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)[Pair];
        }
        CharacterColumn->AddSlot().AutoHeight()
        [
            MakePlate(Totals, PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space16))
        ];
    }

    // ---- Equipment column, 400 wide (reference, ZONES) --------------------
    // "eight slots as full-width rows in WEAR ORDER: head to foot, then
    // trinkets, then weapons". The order itself lives in
    // BreakerInventoryLayout::WearOrder so a test can assert it without a
    // widget — see the note there about the mockup contradicting its own prose.
    TSharedRef<SVerticalBox> EquipRows = SNew(SVerticalBox);
    for (const EBreakerEquipSlot Slot : BreakerInventoryLayout::WearOrder())
    {
        EquipRows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[MakeEquipRow(Slot)];
    }
    TSharedRef<SVerticalBox> EquipmentColumn = SNew(SVerticalBox);
    EquipmentColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        MenuText(FText::FromString(TEXT("EQUIPPED · 8 SLOTS · CLICK TO UNEQUIP")), BreakerUI::TypeCaption, Muted, true)
    ];
    EquipmentColumn->AddSlot().FillHeight(1.0f)
    [
        SNew(SScrollBox) + SScrollBox::Slot()[EquipRows]
    ];
    // The standing explainer the reference pins to the foot of this column.
    // It states the rule ONCE, permanently, so the per-card LIMIT FULL tell is
    // a reminder rather than the first the player has heard of a cap.
    EquipmentColumn->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuText(FText::FromString(TEXT("EQUIP LIMITS")), BreakerUI::TypeCaption, Harm, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
            [
                // WRAPPED, not hand-broken. The three hard \n line breaks this
                // replaces were each longer than the column and clipped to
                // "ANOTI" / "RARIT" / "HOVI" — an author-chosen line break is a
                // guess about a width nobody measured. The wrap width is the
                // column's own, minus the plate's chrome.
                //
                // The caps come from the component, never from a literal here:
                // the screen must not hold a second opinion about a rule that
                // decides which of the player's items gets ejected.
                MenuWrappedText(FText::FromString(FString::Printf(
                    TEXT("ABERRANT %d · ANOMALOUS %d. EQUIPPING ANOTHER SWAPS THE LOWEST ITEM LEVEL OF THAT RARITY. THE PIECE IT EJECTS OUTLINES RED ON HOVER."),
                    UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant),
                    UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Anomalous))),
                    BreakerUI::TypeCaption, SoftText,
                    FMath::Max(120.0f, EquipmentColumnWidth - BreakerInventoryLayout::CardChrome))
            ],
            BreakerUI::BgRaised, Harm, FMargin(BreakerUI::Space16, BreakerUI::Space8))
    ];

    // Bottom: backpack grid — best rarity first, optional slot filter,
    // cards grow to fit their affix list so nothing truncates. Fixed rows of
    // three, never a wrap box: SWrapBox measured by allotted width inside a
    // scroll box oscillates between two layouts every frame.
    TSharedRef<SVerticalBox> BackpackGrid = SNew(SVerticalBox);
    TSharedPtr<SHorizontalBox> BackpackRow;
    int32 BackpackCardIndex = 0;
    TArray<FBreakerItemInstance> BackpackItems = Equipment ? Equipment->GetBackpack() : TArray<FBreakerItemInstance>();
    Algo::Reverse(BackpackItems);
    BackpackItems.StableSort([](const FBreakerItemInstance& A, const FBreakerItemInstance& B)
    {
        return static_cast<uint8>(A.Rarity) > static_cast<uint8>(B.Rarity);
    });
    const int32 TotalBackpackCount = BackpackItems.Num();
    // "a 3-across card grid at 16px gaps" — three where three is READABLE, two
    // where it is not. The owner's requirement is that the affixes can be read;
    // the reference's count is a means to it, and the reference was drawn with
    // narrower type than ours. SolveCardsPerRow holds the rule and a test pins
    // it. Both numbers are settled before layout starts, which is what keeps
    // SWrapBox off this screen.
    // What the GRID actually gets, not what the zone is: the 16px gap between
    // the equipment divider and this column, and the scroll bar the card list
    // sits behind. Sizing the cards off the zone is how a third card ends up
    // 32px past the edge it was measured against.
    constexpr float ScrollBarAllowance = 16.0f;
    const float BackpackGridWidth = FMath::Max(160.0f,
        BackpackZoneWidth - BreakerUI::Space16 - ScrollBarAllowance);
    const int32 BackpackCardsPerRow = BreakerInventoryLayout::SolveCardsPerRow(BackpackGridWidth);
    const float BackpackCardWidth = BreakerInventoryLayout::BackpackCardWidth(BackpackGridWidth, BackpackCardsPerRow);
    BackpackItems.RemoveAll([this](const FBreakerItemInstance& Item)
    {
        return !BreakerInventoryLayout::PassesFilter(Item.Slot, BackpackFilter);
    });
    for (const FBreakerItemInstance& Item : BackpackItems)
    {
        const FGuid ItemId = Item.ItemId;

        // Every consequence of clicking this card, answered by the equipment
        // component before the click. The screen states them; it works none of
        // them out itself.
        const FBreakerEquipPreview Preview = Equipment
            ? Equipment->PreviewEquip(Item)
            : UBreakerEquipmentComponent::PreviewEquipAgainst(TArray<FBreakerItemInstance>(), Item);

        // THE FOOTER, and the reference gives it one line with two halves:
        // "the footer line states the consequence of clicking", and when a cap
        // is being spent "the footer reads LIMIT FULL 3/3 beside the name of
        // the piece the swap ejects". The action is never blocked — it is
        // disclosed.
        const bool bLimitTell = BreakerInventoryLayout::ShouldShowLimitTell(Preview);
        FString FooterLead = BreakerInventoryLayout::MakeFooterLead(Preview);
        if (!bLimitTell && Preview.bSlotOccupied)
        {
            FooterLead += FString::Printf(TEXT(" %s i%d"),
                *ItemDisplayName(Preview.SlotDisplaced), Preview.SlotDisplaced.ItemLevel);
        }
        // Harm for a cap being spent, gold for "this costs you something",
        // cyan for a free action.
        const FLinearColor FooterLeadColor = bLimitTell ? Harm : (Preview.bSlotOccupied ? Amber : Cyan);
        // The right half names the ejected piece. Gold, because the eject is
        // the cost, and the cap itself is what the harm-red half states.
        const FString FooterTrail = bLimitTell
            ? FString::Printf(TEXT("SWAPS %s %s i%d"),
                *ItemDisplayName(Preview.LimitDisplaced), *SlotName(Preview.LimitDisplaced.Slot),
                Preview.LimitDisplaced.ItemLevel)
            : FString();
        const EBreakerEquipSlot DoomedSlot = Preview.LimitDisplaced.Slot;

        const FOnClicked DiscardOne = FOnClicked::CreateLambda([this, ItemId]()
        {
            if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->DiscardFromBackpack(ItemId);
            InventoryStatus = FText::FromString(TEXT("Discarded 1 item."));
            Rebuild(EBreakerMenuScreen::Inventory);
            return FReply::Handled();
        });

        // Fixed rows of a KNOWN count, never a wrap box: SWrapBox measured by
        // allotted width inside a scroll box oscillates between two layouts
        // every frame. Three across, at the reference's 16px gap.
        if (BackpackCardIndex % BackpackCardsPerRow == 0)
        {
            BackpackRow = SNew(SHorizontalBox);
            BackpackGrid->AddSlot().AutoHeight()[BackpackRow.ToSharedRef()];
        }
        // Gap on the right of every card but the last in its row, and under
        // every card. A trailing gap on the third card is what pushes a
        // 3-across grid into needing 4-across room.
        const bool bLastInRow = (BackpackCardIndex % BackpackCardsPerRow) == BackpackCardsPerRow - 1;
        ++BackpackCardIndex;
        BackpackRow->AddSlot().AutoWidth()
            .Padding(0.0f, 0.0f, bLastInRow ? 0.0f : BreakerInventoryLayout::BackpackCardGap,
                BreakerInventoryLayout::BackpackCardGap)
        [
            SNew(SBox).WidthOverride(BackpackCardWidth)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    // The wrapper only exists to catch right-click: SButton
                    // leaves non-left buttons unhandled, so they bubble here.
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder")))
                    .Padding(0.0f)
                    .OnMouseButtonDown(FPointerEventHandler::CreateLambda([DiscardOne](const FGeometry&, const FPointerEvent& MouseEvent)
                    {
                        if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton) return DiscardOne.Execute();
                        return FReply::Unhandled();
                    }))
                    [
                        // Card anatomy (UI-Inventory-Spec): line 1 name plus
                        // item level, line 2 rarity and slot, then the affix
                        // list, then a footer stating what clicking costs you.
                        MakeRarityCard(
                            SNew(SButton)
                            .ButtonColorAndOpacity(PanelRaised)
                            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                            .OnClicked(FOnClicked::CreateLambda([this, ItemId]()
                            {
                                if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->EquipFromBackpack(ItemId);
                                Rebuild(EBreakerMenuScreen::Inventory);
                                return FReply::Handled();
                            }))
                            // The hover half of the limit tell. Event-driven on
                            // purpose: this paints one border on enter and
                            // clears it on leave, and never runs on a tick.
                            .OnHovered(FSimpleDelegate::CreateLambda([this, bLimitTell, DoomedSlot]()
                            {
                                if (bLimitTell) SetEquipSlotOutline(DoomedSlot, true);
                            }))
                            .OnUnhovered(FSimpleDelegate::CreateLambda([this, bLimitTell, DoomedSlot]()
                            {
                                if (bLimitTell) SetEquipSlotOutline(DoomedSlot, false);
                            }))
                            [
                                SNew(SVerticalBox)
                                // LINE ONE: name plus item level, "the two
                                // things scanned first". The name carries the
                                // rarity colour; the face stays panel/10.
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SHorizontalBox)
                                    // THE ROW THAT COLLIDED. The name used to
                                    // take a bare FillWidth slot, so "BODY
                                    // ARMOUR" ran into the i50 beside it and
                                    // into the discard X floating above that.
                                    // It now wraps at a width that has both of
                                    // them subtracted out of it.
                                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                                    [
                                        MenuWrappedText(FText::FromString(ItemDisplayName(Item)), BreakerUI::TypeH2,
                                            RarityColor(Item.Rarity),
                                            BreakerInventoryLayout::CardTitleWrapWidth(BackpackCardWidth), true)
                                    ]
                                    // DiscardClearance of right pad clears the
                                    // discard X in the overlay above;
                                    // ItemLevelColumn is sized to "i9999",
                                    // which O29's item-level ceiling allows.
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                        .Padding(BreakerUI::Space8, 0.0f, BreakerInventoryLayout::DiscardClearance, 0.0f)
                                    [
                                        MenuValueColumn(FText::FromString(FString::Printf(TEXT("i%d"), Item.ItemLevel)),
                                            BreakerInventoryLayout::ItemLevelColumn, BreakerUI::TypeCaption, Primary)
                                    ]
                                ]
                                // LINE TWO: rarity and slot.
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                                [
                                    MenuWrappedText(FText::FromString(ItemRarityAndSlot(Item)), BreakerUI::TypeCaption,
                                        RarityTagColor(Item.Rarity),
                                        BreakerInventoryLayout::CardContentWidth(BackpackCardWidth), true)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                                [
                                    // Line 3 of the card anatomy: every affix
                                    // carrying its delta against the equipped
                                    // piece in this slot.
                                    MakeAffixLines(Item, Preview.AffixDeltas, BackpackCardWidth)
                                ]
                                // THE FOOTER, above a 1px divider as the
                                // reference draws it: structure reads off
                                // borders in this system, never off gaps.
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, BreakerUI::Space8)
                                [
                                    SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(BorderEmphasis)]
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()
                                    [
                                        MenuWrappedText(FText::FromString(FooterLead.ToUpper()), BreakerUI::TypeCaption,
                                            FooterLeadColor, BreakerInventoryLayout::CardContentWidth(BackpackCardWidth), true)
                                    ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                                    [
                                        // The ejected piece, and only when there
                                        // is one: an always-present second line
                                        // would stop meaning anything. Stacked
                                        // rather than set beside the lead —
                                        // a 300px card cannot hold two names on
                                        // one row without clipping one of them,
                                        // and clipping is the defect this file
                                        // has been reported for five times.
                                        bLimitTell
                                            ? StaticCastSharedRef<SWidget>(MenuWrappedText(FText::FromString(FooterTrail),
                                                BreakerUI::TypeCaption, Amber,
                                                BreakerInventoryLayout::CardContentWidth(BackpackCardWidth), true))
                                            : SNullWidget::NullWidget
                                    ]
                                ]
                            ],
                            Item.Rarity, true)
                    ]
                ]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(BreakerUI::Space4, BreakerUI::Space4, BreakerUI::Space4, 0.0f)
                [
                    // Discard state: no fill, deep-red ring, harm-red glyph.
                    BorderWrap(
                        SNew(SButton)
                        .ButtonColorAndOpacity(Panel)
                        .ContentPadding(FMargin(BreakerUI::Space8, 1.0f))
                        .ToolTipText(FText::FromString(TEXT("Discard this item (or right-click the card)")))
                        .OnClicked(DiscardOne)
                        [
                            MenuText(FText::FromString(TEXT("X")), BreakerUI::TypeCaption, Harm, true)
                        ],
                        HarmDeep)
                ]
            ]
        ];
    }

    // FILTER CHIPS: ALL / ARMOUR / WEAPONS / TRINKETS, the four the reference
    // names. FOUR CATEGORIES, not nine slots — nine slot chips are what made
    // this bar overflow in the first place, and "which of my nine slots" is a
    // question the equipment column already answers.
    //
    // The chips are still BUILT here and PACKED into rows below, once the bar
    // knows how much width it has. Before that they lived in an SHorizontalBox
    // FillWidth slot beside the input legend, which is two bugs in one place:
    // an overflowing horizontal box does not wrap, it just keeps drawing, so
    // the row ran off the right edge of a 1920 screen AND printed through the
    // legend that shared the slot ("GLOVES / X DISCARD LMB NECKLACE").
    TArray<TSharedRef<SWidget>> FilterChips;
    TArray<float> FilterChipWidths;
    auto AddFilterChip = [this, &FilterChips, &FilterChipWidths](const FString& Label,
        BreakerInventoryLayout::EBackpackFilter FilterValue)
    {
        const bool bSelectedChip = BackpackFilter == FilterValue;
        const float ChipBorder = bSelectedChip ? BreakerUI::BorderSelected : BreakerUI::BorderThin;
        FilterChipWidths.Add(MeasureChipWidth(Label, BreakerUI::Space8, ChipBorder));
        FilterChips.Add(
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bSelectedChip ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
                .OnClicked(FOnClicked::CreateLambda([this, FilterValue]()
                {
                    BackpackFilter = FilterValue;
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bSelectedChip ? Primary : Muted, true)
                ],
                bSelectedChip ? Cyan : BorderEmphasis,
                ChipBorder));
    };
    AddFilterChip(TEXT("ALL"), BreakerInventoryLayout::EBackpackFilter::All);
    AddFilterChip(TEXT("ARMOUR"), BreakerInventoryLayout::EBackpackFilter::Armour);
    AddFilterChip(TEXT("WEAPONS"), BreakerInventoryLayout::EBackpackFilter::Weapons);
    AddFilterChip(TEXT("TRINKETS"), BreakerInventoryLayout::EBackpackFilter::Trinkets);

    // Clean-up chips. First click arms (gold, "CONFIRM"), second click opens
    // the confirmation modal — it never destroys anything directly. Any other
    // interaction rebuilds and disarms.
    TSharedRef<SHorizontalBox> CleanupRow = SNew(SHorizontalBox);
    auto AddCleanupChip = [this, &CleanupRow](const FString& Label, int32 ArmIndex)
    {
        const bool bArmed = CleanupArmedIndex == ArmIndex;
        // Two-step arm: the button turns gold and reads CONFIRM. Armed carries
        // the 2px gold ring, disarmed reads as a destructive control (deep-red
        // ring, harm text).
        CleanupRow->AddSlot().AutoWidth().Padding(BreakerUI::Space4, 0.0f, 0.0f, 0.0f)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bArmed ? PanelHover : Panel)
            .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
            .OnClicked(FOnClicked::CreateLambda([this, ArmIndex, bArmed]()
            {
                if (bArmed)
                {
                    // The modal is the only thing that can destroy: it states
                    // the count and the exclusions first.
                    DiscardModalIndex = ArmIndex;
                    PendingCleanupArm = ArmIndex;
                }
                else
                {
                    PendingCleanupArm = ArmIndex;
                }
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(bArmed ? FString(TEXT("CONFIRM")) : Label), BreakerUI::TypeCaption, bArmed ? Amber : Harm, true)
            ],
            bArmed ? Amber : HarmDeep,
            bArmed ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    };
    AddCleanupChip(TEXT("DISCARD < UNCOMMON"), 0);
    AddCleanupChip(TEXT("DISCARD < EXCEPTIONAL"), 1);

    // Dev gear grants ride the same playtest flag as dev class swap.
    bool bDevTools = false;
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), bDevTools, GGameUserSettingsIni);
    TSharedRef<SHorizontalBox> DevRow = SNew(SHorizontalBox);
    if (bDevTools)
    {
        DevRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("DEV:")), 9, Amber, true)
        ];
        auto AddDevChip = [this, &DevRow](int32 ItemLevel)
        {
            DevRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(PanelRaised)
                .ContentPadding(FMargin(9.0f, 4.0f))
                .OnClicked(FOnClicked::CreateLambda([this, ItemLevel]()
                {
                    if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->DevGrantTestGear(ItemLevel);
                    InventoryStatus = FText::FromString(FString::Printf(TEXT("Granted a full Exceptional set at ilvl %d."), ItemLevel));
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("GRANT TEST GEAR ilvl %d"), ItemLevel)), 9, Primary, true)
                ]
            ];
        };
        AddDevChip(30);
        AddDevChip(50);
        // Reach (Decisions.md O40c): DevGrantLegendaries shipped with zero
        // callers (Items/BreakerEquipmentComponent.cpp:517) — without a UI
        // hook the three authored legendaries (O32) are unreachable outside
        // Blueprint/console/automation. Same dev gate as GRANT TEST GEAR,
        // dropped into the backpack rather than equipped, same reason: only
        // one Anomalous piece can be worn at a time.
        DevRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(PanelRaised)
            .ContentPadding(FMargin(9.0f, 4.0f))
            .OnClicked(FOnClicked::CreateLambda([this]()
            {
                constexpr int32 ItemLevel = 50;
                if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->DevGrantLegendaries(ItemLevel);
                InventoryStatus = FText::FromString(FString::Printf(TEXT("Granted every legendary to the backpack at ilvl %d."), ItemLevel));
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(TEXT("GRANT LEGENDARIES ilvl 50")), 9, Primary, true)
            ]
        ];
    }

    // ---- Backpack zone -----------------------------------------------------
    // Filter bar carrying the count, the input hint and the slot chips, then
    // the card grid. The spec puts the input hints here, which is why the
    // screen has no footer.
    //
    // TWO ROWS, not one. The count and the legend own the first row outright,
    // so the legend can never be drawn over again; the chips get the whole
    // width of the second and wrap into as many rows as they need. The bar is
    // auto-height with the spec's 64 as a FLOOR — a fixed 64 is what forced
    // nine chips onto one line in the first place.
    const float FilterChipRoom = FMath::Max(240.0f,
        BackpackZoneWidth - 2.0f * BreakerUI::Space16 - BreakerUI::RailThickness - 2.0f * BreakerUI::BorderThin);
    TSharedRef<SVerticalBox> BackpackColumn = SNew(SVerticalBox);
    BackpackColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        SNew(SBox).MinDesiredHeight(64.0f)
        [
            MakePlate(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
                    [
                        MenuText(FText::FromString(FString::Printf(TEXT("BACKPACK %d/%d"), BackpackItems.Num(), TotalBackpackCount)), BreakerUI::TypeCaption, Primary, true)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        MenuText(FText::FromString(TEXT("RMB / X DISCARD · LMB EQUIP")), BreakerUI::TypeCaption, Muted, true)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    PackChipRows(FilterChips, FilterChipWidths, FilterChipRoom, BreakerUI::Space4)
                ],
                Panel, BorderEmphasis, FMargin(BreakerUI::Space16, BreakerUI::Space8))
        ]
    ];
    if (bDevTools)
    {
        BackpackColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[DevRow];
    }
    if (!InventoryStatus.IsEmpty())
    {
        BackpackColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(InventoryStatus, BreakerUI::TypeCaption, Amber, true)
        ];
    }
    // The empty backpack is the one place the screen teaches the world: the
    // five rarity beams as vertical bars, and the single line that ties them
    // to what the player sees on the ground.
    //
    // THAT LINE READ "LOOT IS FOUND BY COLOUR", in H2, directly under five bare
    // colour swatches. art-and-ui's rule is "never signal state by colour
    // alone" and UI.Rarity.NonColourCue asserts every rarity is distinguishable
    // without it — so the one screen whose job is to teach the loot vocabulary
    // was teaching the behaviour the invariant exists to forbid, in the largest
    // type on the panel.
    //
    // The copy now describes what the panel actually shows: five rarities in
    // ramp order, each named. The NAME is the non-colour distinction here and
    // it is a reading cue rather than a glance cue, which is honest but is not
    // yet the invariant satisfied. THE BEAMS ARE STILL FIVE IDENTICAL 180px
    // BARS — varying their height is the cheapest real cue on this screen and
    // it is deliberately not done in this pass, because the cue has to be one
    // decision taken across every rarity site at once rather than five
    // different marks arrived at one screen at a time.
    TSharedRef<SWidget> EmptyBackpack =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space40, 0.0f, 0.0f)
        [
            MakeRarityBeams()
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("FIVE RARITIES, WORST TO BEST")), BreakerUI::TypeH2, Primary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("EMPTY · ENEMY KILLS DROP ROLLED ITEMS")), BreakerUI::TypeCaption, Muted, true)
        ];

    BackpackColumn->AddSlot().FillHeight(1.0f)
    [
        BackpackItems.IsEmpty()
            ? EmptyBackpack
            : StaticCastSharedRef<SWidget>(SNew(SScrollBox) + SScrollBox::Slot()[BackpackGrid])
    ];

    // ---- Header band -------------------------------------------------------
    // The two equip-limit counters live here permanently, so the constraint is
    // never a surprise at click time. Both the counts and the caps come from
    // the equipment component: the screen must never hold a second opinion
    // about a rule that decides which of the player's items gets ejected.
    const int32 AberrantEquipped = Equipment ? Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant) : 0;
    const int32 AnomalousEquipped = Equipment ? Equipment->CountEquippedOfRarity(EBreakerItemRarity::Anomalous) : 0;
    const int32 AberrantLimit = UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant);
    const int32 AnomalousLimit = UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Anomalous);

    auto MakeLimitChip = [](const FString& Label, int32 Count, int32 Limit, const FLinearColor& Rail, bool bFullBorder) -> TSharedRef<SWidget>
    {
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, true)]
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuText(FText::FromString(FString::Printf(TEXT("%d/%d"), Count, Limit)), BreakerUI::TypeH2,
                    Count >= Limit ? Rail : Primary, true)
            ],
            PanelRaised, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space4), false,
            bFullBorder ? Rail : BreakerUI::BorderRest);
    };

    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[BuildScreenTabs(EBreakerMenuScreen::Inventory)];
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    // O11: up to three Aberrant equipped, one Anomalous. Aberrant takes the
    // harm rail (it shares that hue by design); Anomalous is the one rarity
    // that is also a world object class, so it takes the teal rail AND the
    // full teal border — the single legal teal on this screen.
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
    [
        MakeLimitChip(TEXT("ABERRANT"), AberrantEquipped, AberrantLimit, BreakerUI::RarityAberrant, false)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
    [
        MakeLimitChip(TEXT("UNWRITTEN"), AnomalousEquipped, AnomalousLimit, BreakerUI::RarityAnomalous, true)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[CleanupRow];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    // Meta line. NO GEAR SCORE, and the absence is the rule rather than an
    // omission: art-and-ui says no screen prints an aggregate item score, and
    // this line printed one — the sum of equipped item levels — in the subtitle
    // of the most-used screen in the game.
    //
    // It is not a small violation. The endgame's whole thesis is that power
    // lives in decisions, and a single number telling the player which item is
    // better deletes the decision the endgame is made of. Worse, a scalar
    // CANNOT be honest here: O54 partitions damage by delivery, and the two
    // lanes measure 0.647x of each other at the cap and 0.388x at endgame, so
    // any one figure is wrong for whichever build the player actually has.
    //
    // What the score was reaching for is answered by the composed delta on the
    // Forge's detail panel, which reports per lane and cannot misprice one.
    // RiorsEdge.UI.NoItemScore holds the absence.
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const FString MetaLine = BreakerInventoryLayout::LoadoutMetaLine(
        ClassDisplayName(Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None),
        Progression ? Progression->GetProgressionState().CharacterLevel : 1);

    // ---- Zones -------------------------------------------------------------
    // 560 | 400 | the rest, separated by 1px DIVIDERS rather than by gutters —
    // the reference tiles the three zones edge to edge and reads the structure
    // off the borders, which is the same rule the rest of FIELDPLATE follows
    // ("depth comes from border value"). It is also why the arithmetic in
    // SolveColumns adds up to the panel exactly: 560 + 400 + 960 = 1920.
    TSharedRef<SHorizontalBox> Body = SNew(SHorizontalBox);
    Body->AddSlot().AutoWidth()
    [
        SNew(SBox).WidthOverride(CharacterColumnWidth).Padding(FMargin(0.0f, 0.0f, BreakerUI::Space16, 0.0f))[CharacterColumn]
    ];
    Body->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(BreakerUI::BorderThin)[SolidBlock(BorderRest)]];
    Body->AddSlot().AutoWidth()
    [
        SNew(SBox).WidthOverride(EquipmentColumnWidth).Padding(FMargin(BreakerUI::Space16, 0.0f))[EquipmentColumn]
    ];
    Body->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(BreakerUI::BorderThin)[SolidBlock(BorderRest)]];
    Body->AddSlot().FillWidth(1.0f).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        BackpackColumn
    ];

    // No footer by design (UI-Inventory-Spec "Zones"): the input hints live in
    // the backpack filter bar and BACK sits in the header band.
    TSharedRef<SWidget> Screen = BuildZonedFrame(
        FText::FromString(TEXT("LOADOUT")),
        FText::FromString(MetaLine),
        HeaderRight,
        Body,
        SNullWidget::NullWidget,
        Metrics.PanelWidth,
        Metrics.PanelHeight,
        // THE JITTER FIX, and it is the one the previous pass missed. A plate
        // whose height is its CONTENT's height, centred, moves every time the
        // body changes size — which on this screen is every equip, every
        // filter, every status line. Fixed height, so the rectangle cannot
        // move. See the long note in BuildZonedFrame.
        /*bFillHeight=*/true);

    if (DiscardModalIndex < 0) return Screen;

    // The confirmation modal sits above the whole screen, not inside a zone:
    // it is the last thing between the player and an irreversible action.
    const EBreakerItemRarity MinimumKept = CleanupThresholdForArm(DiscardModalIndex);
    const int32 DoomedCount = Equipment ? Equipment->CountBackpackBelowRarity(MinimumKept) : 0;
    return SNew(SOverlay)
        + SOverlay::Slot()[Screen]
        + SOverlay::Slot()[BuildDiscardModal(DiscardModalIndex, MinimumKept, DoomedCount)];
}

TSharedRef<SWidget> SBreakerMenu::BuildDiscardModal(int32 ArmIndex, EBreakerItemRarity MinimumKept, int32 Count)
{
    const FString Threshold = RarityName(MinimumKept);
    // The count is the equipment component's own answer, produced by the same
    // predicate the discard uses — the modal cannot promise a different number
    // from the one it destroys.
    TSharedRef<SVerticalBox> Plate = SNew(SVerticalBox);
    Plate->AddSlot().AutoHeight()
    [
        MenuText(FText::FromString(TEXT("DESTROY BACKPACK ITEMS")), BreakerUI::TypeH1, Primary, true)
    ];
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(FString::Printf(TEXT("%d backpack item%s below %s will be destroyed. This cannot be undone."),
            Count, Count == 1 ? TEXT("") : TEXT("s"), *Threshold)), BreakerUI::TypeBody, SoftText)
    ];
    // The exclusions, stated rather than assumed. Both are properties of the
    // component: equipped gear is a separate container, and Aberrant and
    // Anomalous sit above every threshold this screen offers.
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("NEVER INCLUDED\n· EQUIPPED GEAR\n· ABERRANT\n· ANOMALOUS")), BreakerUI::TypeCaption, Muted, true)
    ];

    TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
    Actions->AddSlot().AutoWidth()
    [
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Panel)
            .ContentPadding(FMargin(BreakerUI::Space24, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this]()
            {
                DiscardModalIndex = -1;
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(TEXT("CANCEL")), BreakerUI::TypeCaption, Primary, true)
            ],
            BorderEmphasis)
    ];
    Actions->AddSlot().AutoWidth().Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        // The destructive control: the count in the label, harm-red text on
        // the destructive face, harm-red ring. Nothing else on the screen
        // looks like this.
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(BreakerUI::DestructiveFace)
            .ContentPadding(FMargin(BreakerUI::Space24, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this, ArmIndex]()
            {
                const int32 Removed = Character.IsValid() && Character->GetEquipment()
                    ? Character->GetEquipment()->DiscardBackpackBelowRarity(CleanupThresholdForArm(ArmIndex))
                    : 0;
                InventoryStatus = FText::FromString(FString::Printf(TEXT("Destroyed %d item%s."), Removed, Removed == 1 ? TEXT("") : TEXT("s")));
                DiscardModalIndex = -1;
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(FString::Printf(TEXT("DESTROY %d"), Count)), BreakerUI::TypeCaption, Harm, true)
            ],
            Harm, BreakerUI::BorderSelected)
    ];
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f).HAlign(HAlign_Right)[Actions];

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            // The scrim both dims the screen and swallows clicks, so the
            // controls behind a modal cannot be operated through it.
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BreakerUI::Alpha(BreakerUI::BgVoid, 0.85f))
            .OnMouseButtonDown(FPointerEventHandler::CreateLambda([](const FGeometry&, const FPointerEvent&)
            {
                return FReply::Handled();
            }))
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(560.0f)
            [
                // The destructive face on a harm-red rail: the only plate in
                // the system that is not part of the panel ramp.
                MakePlate(Plate, BreakerUI::DestructiveFace, Harm,
                    FMargin(BreakerUI::Space24, BreakerUI::Space24), false, Harm)
            ]
        ];
}

void SBreakerMenu::SetEquipSlotOutline(EBreakerEquipSlot Slot, bool bDoomed)
{
    if (const TWeakPtr<SBorder>* Found = EquipSlotOutlines.Find(Slot))
    {
        if (const TSharedPtr<SBorder> Outline = Found->Pin())
        {
            // Harm red while a card that would eject this piece is hovered,
            // the screen field otherwise — which reads as no outline at all.
            Outline->SetBorderBackgroundColor(FSlateColor(bDoomed ? Harm : Background));
        }
    }
}

// ---------------------------------------------------------------------------
// CHARACTER SELECT / CREATE
// ---------------------------------------------------------------------------
// Class choice is permanent per character (a locked decision) and the project
// shipped with exactly ONE save slot, so a player who picked Swift could never
// see Caster without a dev override — the permanence rule and the single slot
// together made the class screen a trap rather than a choice. These two
// screens are the other half of UBreakerCharacterRoster: permanence stays,
// and it stops being a cage because there are five slots and a delete.
// ---------------------------------------------------------------------------

namespace
{
    // The five classes, their resource, their branches and a one-line premise.
    // Shared by the create carousel and BuildClassSelectScreen so the two can
    // never describe the same class differently.
    struct FBreakerClassBlurb
    {
        EBreakerClassId ClassId;
        const TCHAR* Name;
        const TCHAR* Resource;
        const TCHAR* Branches;
        const TCHAR* Pitch;
    };

    const FBreakerClassBlurb GBreakerClassBlurbs[] =
    {
        { EBreakerClassId::Swift,    TEXT("SWIFT"),    TEXT("MOMENTUM"), TEXT("Frenzy / Kinetic / Marksman"),
          TEXT("Speed is the build. Movement generates power, and standing still spends it.") },
        { EBreakerClassId::Caster,   TEXT("CASTER"),   TEXT("MANA"),     TEXT("Spellblade / Void Whisperer / Multispell"),
          TEXT("Mana is the cooldown. Statuses, reactions and ability-driven combat.") },
        { EBreakerClassId::Gunsmith, TEXT("GUNSMITH"), TEXT("SCRAP"),    TEXT("Armory / Field Tech / Tinkerer"),
          TEXT("Deployables and weapon mastery. The gun is the character.") },
        { EBreakerClassId::Tank,     TEXT("TANK"),     TEXT("GRIT"),     TEXT("Leech / Bastion / Demolitionist"),
          TEXT("Mitigation becomes fuel. Hold the line and be paid for it.") },
        { EBreakerClassId::Support,  TEXT("SUPPORT"),  TEXT("CHARGE"),   TEXT("Medic / Conductor / Warden"),
          TEXT("Amplify, sustain, control — and solo viable, never a second seat.") },
    };

    const FBreakerClassBlurb* FindClassBlurb(EBreakerClassId ClassId)
    {
        for (const FBreakerClassBlurb& Blurb : GBreakerClassBlurbs)
        {
            if (Blurb.ClassId == ClassId) return &Blurb;
        }
        return nullptr;
    }
}

TSharedRef<SWidget> SBreakerMenu::MakeClassSilhouette(EBreakerClassId ClassId, bool bImplemented, float Scale, bool bShowCaption) const
{
    // A stand-in for the character model, drawn from the same primitives the
    // rest of this front end is built from. Owner's ruling: unimplemented
    // classes show a GREYED SILHOUETTE rather than being hidden, so the shape
    // of what the game intends to be is legible from the first screen while
    // O39 still refuses to let anyone lock into one.
    //
    // The silhouette is deliberately the SAME figure for every class. It is
    // honest about being a placeholder, and five subtly different boxes would
    // imply a distinction the models will actually have to earn later.
    const FLinearColor Body = bImplemented ? SoftText : Disabled;
    const FLinearColor Plate = bImplemented ? PanelRaised : Panel;

    TSharedRef<SVerticalBox> Figure = SNew(SVerticalBox);
    // Head.
    Figure->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 3.0f)
    [
        SNew(SBox).WidthOverride(22.0f * Scale).HeightOverride(22.0f * Scale)[SolidBlock(Body)]
    ];
    // Torso.
    Figure->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 3.0f)
    [
        SNew(SBox).WidthOverride(42.0f * Scale).HeightOverride(52.0f * Scale)[SolidBlock(Body)]
    ];
    // Legs, as two blocks with a gap, so the figure reads as a person at a
    // glance rather than as an icon.
    Figure->AddSlot().AutoHeight().HAlign(HAlign_Center)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SBox).WidthOverride(16.0f * Scale).HeightOverride(38.0f * Scale)[SolidBlock(Body)]
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(16.0f * Scale).HeightOverride(38.0f * Scale)[SolidBlock(Body)]
        ]
    ];

    TSharedRef<SVerticalBox> Inner = SNew(SVerticalBox);
    Inner->AddSlot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Figure];
    // The caption is OPTIONAL, and off at banner scale. The plate scales with
    // Scale but the font does not, so at 0.42 the caption is wider than the
    // plate that clips it and drew as "PLACE" / "NOT B" — the same
    // measure-versus-clip defect the owner reported on the skill board, and a
    // second instance of it caused by scaling a box without scaling what is
    // inside. The banner names the class beneath it anyway, so the caption was
    // redundant there as well as broken.
    if (bShowCaption)
    {
        Inner->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(bImplemented ? TEXT("PLACEHOLDER") : TEXT("NOT BUILT")),
                BreakerUI::TypeCaption, bImplemented ? Muted : Disabled, true)
        ];
    }

    return SNew(SBox).WidthOverride(150.0f * Scale).HeightOverride(190.0f * Scale)
    [
        MakePlate(Inner, Plate, bImplemented ? Cyan : BorderEmphasis,
            FMargin(BreakerUI::Space8, BreakerUI::Space8), false, BreakerUI::BorderRest)
    ];
}

TSharedRef<SWidget> SBreakerMenu::MakeClassTile(EBreakerClassId ClassId, bool bSelected)
{
    const FBreakerClassBlurb* Blurb = FindClassBlurb(ClassId);
    if (!Blurb) return SNullWidget::NullWidget;
    const bool bImplemented = ClassHasImplementedKit(ClassId);

    TSharedRef<SVerticalBox> Text = SNew(SVerticalBox);
    Text->AddSlot().AutoHeight()
    [
        MenuText(FText::FromString(Blurb->Name), BreakerUI::TypeH2,
            bImplemented ? (bSelected ? Cyan : Primary) : Disabled, true)
    ];
    Text->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(FString::Printf(TEXT("RESOURCE: %s"), Blurb->Resource)),
            BreakerUI::TypeCaption, bImplemented ? SoftText : Disabled, true)
    ];
    Text->AddSlot().AutoHeight()
    [
        MenuText(FText::FromString(Blurb->Branches), BreakerUI::TypeCaption,
            bImplemented ? Muted : Disabled, true)
    ];
    Text->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        SNew(STextBlock)
            .Text(FText::FromString(Blurb->Pitch))
            .ColorAndOpacity(bImplemented ? SoftText : Disabled)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
            .AutoWrapText(true)
    ];
    if (!bImplemented)
    {
        // O39, said out loud rather than left as a dead button. A disabled
        // tile with no reason reads as a bug; a disabled tile that says why
        // reads as a roadmap.
        Text->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("NO KIT YET — CANNOT BE CHOSEN")), BreakerUI::TypeCaption, Amber, true)
        ];
    }

    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[MakeClassSilhouette(ClassId, bImplemented)];
    Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)[Text];

    const EBreakerClassId Captured = ClassId;
    return BorderWrap(
        SNew(SButton)
        .ButtonColorAndOpacity(bSelected ? PanelHover : Panel)
        .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space16))
        .HAlign(HAlign_Fill).VAlign(VAlign_Center)
        .OnClicked(FOnClicked::CreateLambda([this, Captured, bImplemented]()
        {
            if (!bImplemented)
            {
                // Refused, and it SAYS so. A click that does nothing is the
                // failure mode this project keeps rediscovering.
                CharacterScreenStatus = FText::FromString(
                    TEXT("That class has no kit yet. Class choice is permanent, so it cannot be chosen."));
            }
            else
            {
                PendingCreateClass = Captured;
                CharacterScreenStatus = FText::GetEmpty();
            }
            Rebuild(EBreakerMenuScreen::CharacterCreate);
            return FReply::Handled();
        }))
        [
            Row
        ],
        bSelected ? Cyan : (bImplemented ? BorderEmphasis : BreakerUI::BorderRest),
        bSelected ? BreakerUI::BorderSelected : BreakerUI::BorderThin);
}

TSharedRef<SWidget> SBreakerMenu::MakeClassBanner(EBreakerClassId ClassId, bool bSelected)
{
    const FBreakerClassBlurb* Blurb = FindClassBlurb(ClassId);
    if (!Blurb) return SNullWidget::NullWidget;
    const bool bImplemented = ClassHasImplementedKit(ClassId);

    // A tall narrow crest, the shape a class banner wants, carrying identity
    // only — the name and the resource. Everything else about the class reads
    // on the detail panel to the right, so scanning the column is a scan of
    // FIVE THINGS rather than five paragraphs.
    // NO THUMBNAIL. A scaled-down figure plus two text lines overran the
    // banner box and spilled its name through the border below it — and the
    // figure it showed was the same one already drawn large on the right, at a
    // size where it read as a grey smudge. The banner carries IDENTITY, the
    // right-hand panel carries the character; duplicating the figure bought
    // nothing and cost the layout. Removing it also removes the caption-
    // clipping problem at its root rather than suppressing the caption.
    TSharedRef<SVerticalBox> Inner = SNew(SVerticalBox);
    Inner->AddSlot().AutoHeight().HAlign(HAlign_Center)
    [
        MenuText(FText::FromString(Blurb->Name), BreakerUI::TypeBody,
            bImplemented ? (bSelected ? Cyan : Primary) : Disabled, true)
    ];
    Inner->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(Blurb->Resource), BreakerUI::TypeCaption,
            bImplemented ? Muted : Disabled, true)
    ];

    const EBreakerClassId Captured = ClassId;
    // 72: name plus resource plus the button's own padding, measured against
    // what actually fits. Five of these plus spacing sit inside the column
    // beside the character with room left, so nothing scrolls and nothing
    // overruns the name field beneath. Sized to the CONTENT rather than
    // picked — the two previous values were picked, and both were wrong.
    return SNew(SBox).HeightOverride(72.0f)
    [
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bSelected ? PanelHover : Panel)
            .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space8))
            .HAlign(HAlign_Fill).VAlign(VAlign_Center)
            .OnClicked(FOnClicked::CreateLambda([this, Captured, bImplemented]()
            {
                // An unbuilt class still SELECTS — it just cannot be created.
                // Letting the player read what Tank is meant to be is the whole
                // reason O39 shows them greyed rather than hiding them; a
                // banner that refuses even to be inspected teaches nothing.
                PendingCreateClass = Captured;
                CharacterScreenStatus = bImplemented
                    ? FText::GetEmpty()
                    : FText::FromString(TEXT("This class has no kit yet and cannot be created."));
                Rebuild(EBreakerMenuScreen::CharacterCreate);
                return FReply::Handled();
            }))
            [
                Inner
            ],
            bSelected ? Cyan : (bImplemented ? BorderEmphasis : BreakerUI::BorderRest),
            bSelected ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
    ];
}

TSharedRef<SWidget> SBreakerMenu::MakeCharacterRow(const FBreakerCharacterSummary& Summary, bool bSelected)
{
    const FBreakerClassBlurb* Blurb = FindClassBlurb(Summary.ClassId);

    TSharedRef<SVerticalBox> Text = SNew(SVerticalBox);
    Text->AddSlot().AutoHeight()
    [
        MenuText(FText::FromString(Summary.CharacterName.ToUpper()), BreakerUI::TypeH2,
            bSelected ? Cyan : Primary, true)
    ];
    Text->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(FString::Printf(TEXT("LEVEL %d  ·  %s"),
            Summary.CharacterLevel, Blurb ? Blurb->Name : TEXT("UNKNOWN"))),
            BreakerUI::TypeCaption, SoftText, true)
    ];

    const FGuid Captured = Summary.CharacterId;
    const bool bArmedForDelete = (PendingDeleteCharacterId == Summary.CharacterId);

    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    // Half scale and no caption: at row size the caption clipped and the
    // figure only needs to read as a figure. Full-size silhouettes are what
    // made three characters taller than the panel.
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        MakeClassSilhouette(Summary.ClassId, true, 0.55f, /*bShowCaption=*/false)
    ];
    Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)[Text];
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
    [
        // 260, not 190. "DELETE FOREVER?" measured wider than the box and was
        // cut off (owner: "the delete forever doesnt actually stretch or fit
        // in the box"). Sized to the LONGEST label the button can ever carry
        // rather than to the shortest, because the box is fixed and the label
        // changes underneath it — sizing to "DELETE" is what created the
        // defect. Same root cause as the skill board's clipped captions.
        SNew(SBox).WidthOverride(260.0f)
        [
            // TWO-STEP DELETE. The same arm/confirm shape the inventory's bulk
            // discard and O37's COMMIT control use. This is the most
            // destructive button in the game — it removes hours of progress and
            // there is no undo — so it does not get a bare single click, and
            // the armed label states the consequence rather than saying
            // "confirm".
            MakeButton(FText::FromString(bArmedForDelete
                    ? TEXT("DELETE FOREVER?")
                    : TEXT("DELETE")),
                FOnClicked::CreateLambda([this, Captured, bArmedForDelete]()
                {
                    EnsureRosterLoaded();
                    if (!Roster.IsValid()) return FReply::Handled();
                    if (!bArmedForDelete)
                    {
                        PendingDeleteCharacterId = Captured;
                        CharacterScreenStatus = FText::FromString(
                            TEXT("Click DELETE FOREVER to destroy this character. There is no undo."));
                    }
                    else
                    {
                        FBreakerCharacterSummary Doomed;
                        const bool bFound = Roster->FindCharacter(Captured, Doomed);
                        Roster->DeleteCharacter(Captured);
                        PendingDeleteCharacterId = FGuid();
                        if (SelectedCharacterId == Captured)
                        {
                            SelectedCharacterId = Roster->Characters.Num() > 0
                                ? Roster->Characters[0].CharacterId : FGuid();
                        }
                        CharacterScreenStatus = FText::FromString(bFound
                            ? FString::Printf(TEXT("%s has been deleted."), *Doomed.CharacterName.ToUpper())
                            : TEXT("Character deleted."));
                    }
                    Rebuild(EBreakerMenuScreen::CharacterSelect);
                    return FReply::Handled();
                }))
        ]
    ];

    return BorderWrap(
        SNew(SButton)
        .ButtonColorAndOpacity(bSelected ? PanelHover : Panel)
        .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space16))
        .HAlign(HAlign_Fill).VAlign(VAlign_Center)
        .OnClicked(FOnClicked::CreateLambda([this, Captured]()
        {
            SelectedCharacterId = Captured;
            // Selecting a different row disarms a pending delete. Otherwise an
            // arm could survive a change of subject and the next click would
            // destroy a character the player had stopped looking at.
            PendingDeleteCharacterId = FGuid();
            Rebuild(EBreakerMenuScreen::CharacterSelect);
            return FReply::Handled();
        }))
        [
            Row
        ],
        bSelected ? Cyan : BorderEmphasis,
        bSelected ? BreakerUI::BorderSelected : BreakerUI::BorderThin);
}

TSharedRef<SWidget> SBreakerMenu::BuildCharacterSelectScreen()
{
    EnsureRosterLoaded();

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    if (!Roster.IsValid())
    {
        // LoadOrCreate returns null only when it refuses a roster written by a
        // newer build. Saying so beats an empty list that reads as data loss.
        Body->AddSlot().AutoHeight()
        [
            MenuText(FText::FromString(
                TEXT("The character roster could not be read — it may have been written by a newer build. ")
                TEXT("No file has been modified.")), BreakerUI::TypeCaption, Harm, true)
        ];
        return BuildFrame(FText::FromString(TEXT("CHARACTERS")), FText::FromString(TEXT("")), Body, 860.0f);
    }

    if (Roster->Characters.Num() == 0)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
        [
            MenuText(FText::FromString(TEXT("No characters yet. Create one to begin.")),
                BreakerUI::TypeCaption, SoftText, true)
        ];
    }
    for (const FBreakerCharacterSummary& Summary : Roster->Characters)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MakeCharacterRow(Summary, Summary.CharacterId == SelectedCharacterId)
        ];
    }

    if (!CharacterScreenStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
        [
            MenuText(CharacterScreenStatus, BreakerUI::TypeCaption, Amber, true)
        ];
    }

    const bool bFull = Roster->IsFull();
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(FString::Printf(TEXT("%d / %d CHARACTERS"),
            Roster->Characters.Num(), UBreakerCharacterRoster::MaxCharacters)),
            BreakerUI::TypeCaption, bFull ? Amber : Muted, true)
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        MakeButton(FText::FromString(bFull ? TEXT("ROSTER FULL — DELETE ONE TO CREATE") : TEXT("CREATE CHARACTER")),
            FOnClicked::CreateLambda([this, bFull]()
            {
                if (bFull)
                {
                    CharacterScreenStatus = FText::FromString(
                        TEXT("You already have five characters. Delete one first."));
                    Rebuild(EBreakerMenuScreen::CharacterSelect);
                    return FReply::Handled();
                }
                PendingCreateClass = EBreakerClassId::None;
                PendingCreateName = FText::GetEmpty();
                CharacterScreenStatus = FText::GetEmpty();
                Rebuild(EBreakerMenuScreen::CharacterCreate);
                return FReply::Handled();
            }))
    ];

    const bool bCanPlay = SelectedCharacterId.IsValid();
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        MakeButton(FText::FromString(bCanPlay ? TEXT("PLAY") : TEXT("SELECT A CHARACTER")),
            FOnClicked::CreateLambda([this]()
            {
                if (!SelectedCharacterId.IsValid() || !Roster.IsValid()) return FReply::Handled();
                Roster->LastPlayedCharacterId = SelectedCharacterId;
                Roster->SaveRoster();
                // Into the world, as this character, in the hub. The gap this
                // replaces was real: LoadGameState was hard-wired to the single
                // legacy slot, so entering here would have played the wrong
                // character rather than the selected one.
                if (Character.IsValid())
                {
                    Character->EnterWorldAsCharacter(SelectedCharacterId);
                }
                return FReply::Handled();
            }), bCanPlay)
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack))
    ];

    return BuildFrame(FText::FromString(TEXT("CHARACTERS")),
        FText::FromString(TEXT("Class choice is permanent per character.")), Body, 860.0f);
}

TSharedRef<SWidget> SBreakerMenu::BuildCharacterCreateScreen()
{
    EnsureRosterLoaded();

    // Two columns, mirrored from the genre convention the owner referenced:
    // the CLASS COLUMN on the LEFT and the CHARACTER on the RIGHT. The shape
    // matters — the thing you are choosing between wants to be a short
    // scannable list, and the thing you are choosing wants to be shown large.
    // The previous stacked version made the player scroll past five paragraphs
    // to compare two classes, and drew the figure five times at thumbnail size.
    const FBreakerClassBlurb* Selected = FindClassBlurb(PendingCreateClass);
    const bool bSelectedImplemented = Selected && ClassHasImplementedKit(Selected->ClassId);

    // ---- LEFT: the five banners ----------------------------------------
    TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    for (const FBreakerClassBlurb& Blurb : GBreakerClassBlurbs)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MakeClassBanner(Blurb.ClassId, Blurb.ClassId == PendingCreateClass)
        ];
    }

    // ---- RIGHT: the character, then what it is --------------------------
    TSharedRef<SVerticalBox> Detail = SNew(SVerticalBox);
    Detail->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        // Nothing selected yet still draws a figure, greyed. An empty right
        // half would read as a broken screen on the one screen a new player
        // cannot avoid.
        MakeClassSilhouette(Selected ? Selected->ClassId : EBreakerClassId::None,
            Selected != nullptr && bSelectedImplemented, 1.55f)
    ];
    Detail->AddSlot().AutoHeight().HAlign(HAlign_Center)
    [
        MenuText(FText::FromString(Selected ? Selected->Name : TEXT("SELECT A CLASS")),
            BreakerUI::TypeH1, Selected ? Primary : Muted, true)
    ];
    if (Selected)
    {
        // The keyword line: the branches, which ARE this class's three
        // identities, in the same slot the reference gives its three-word
        // character summary.
        Detail->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(FString(Selected->Branches).ToUpper().Replace(TEXT("/"), TEXT("."))),
                BreakerUI::TypeCaption, Cyan, true)
        ];
        Detail->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
        [
            SNew(SBox).MaxDesiredWidth(420.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(Selected->Pitch))
                    .Justification(ETextJustify::Center)
                    .ColorAndOpacity(SoftText)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
                    .AutoWrapText(true)
            ]
        ];
        Detail->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("CLASS RESOURCE - %s"), Selected->Resource)),
                BreakerUI::TypeCaption, Muted, true)
        ];
        if (!bSelectedImplemented)
        {
            Detail->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
            [
                MenuText(FText::FromString(TEXT("NO KIT YET - CANNOT BE CREATED")), BreakerUI::TypeCaption, Amber, true)
            ];
        }
    }

    TSharedRef<SHorizontalBox> Columns = SNew(SHorizontalBox);
    Columns->AddSlot().AutoWidth()
    [
        // Scrolled as well as sized: the fix above makes five banners fit, and
        // this is what stops a sixth class silently reintroducing the overrun.
        SNew(SBox).WidthOverride(200.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()[Column]
        ]
    ];
    Columns->AddSlot().FillWidth(1.0f).VAlign(VAlign_Top).Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
    [
        Detail
    ];

    // ---- Below both columns: name, create, back -------------------------
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        MenuText(FText::FromString(
            TEXT("Class choice is PERMANENT for this character - the Forge can respec nodes, never the class.")),
            BreakerUI::TypeCaption, SoftText, true)
    ];
    Body->AddSlot().FillHeight(1.0f)[Columns];

    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
    [
        MenuText(FText::FromString(TEXT("NAME")), BreakerUI::TypeCaption, Muted, true)
    ];
    Body->AddSlot().AutoHeight()
    [
        SNew(SBox).HeightOverride(BreakerUI::MinHitTarget)
        [
            SNew(SEditableTextBox)
            .Text(PendingCreateName)
            .HintText(FText::FromString(TEXT("Name this Breaker")))
            .OnTextChanged(FOnTextChanged::CreateLambda([this](const FText& NewText)
            {
                // Stored WITHOUT rebuilding: rebuilding on every keystroke
                // would destroy the text box mid-word and take focus with it.
                PendingCreateName = NewText;
            }))
        ]
    ];

    if (!CharacterScreenStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
        [
            MenuText(CharacterScreenStatus, BreakerUI::TypeCaption, Amber, true)
        ];
    }

    const bool bReady = PendingCreateClass != EBreakerClassId::None && bSelectedImplemented;
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        MakeButton(FText::FromString(bReady ? TEXT("CREATE") : TEXT("CHOOSE A CLASS")),
            FOnClicked::CreateLambda([this]()
            {
                EnsureRosterLoaded();
                if (!Roster.IsValid() || PendingCreateClass == EBreakerClassId::None)
                {
                    CharacterScreenStatus = FText::FromString(TEXT("Choose a class first."));
                    Rebuild(EBreakerMenuScreen::CharacterCreate);
                    return FReply::Handled();
                }
                FText Failure;
                const FGuid Created = Roster->CreateCharacter(
                    PendingCreateName.ToString(), PendingCreateClass, Failure);
                if (!Created.IsValid())
                {
                    // The roster's own reason, surfaced verbatim rather than
                    // replaced with a generic one - it already distinguishes a
                    // full roster from a bad name from a kitless class.
                    CharacterScreenStatus = Failure;
                    Rebuild(EBreakerMenuScreen::CharacterCreate);
                    return FReply::Handled();
                }
                SelectedCharacterId = Created;
                PendingCreateClass = EBreakerClassId::None;
                PendingCreateName = FText::GetEmpty();
                CharacterScreenStatus = FText::GetEmpty();
                Rebuild(EBreakerMenuScreen::CharacterSelect);
                return FReply::Handled();
            }), bReady)
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateLambda([this]()
        {
            CharacterScreenStatus = FText::GetEmpty();
            Rebuild(EBreakerMenuScreen::CharacterSelect);
            return FReply::Handled();
        }))
    ];

    return BuildFrame(FText::FromString(TEXT("CREATE CHARACTER")),
        FText::FromString(TEXT("Unbuilt classes are shown greyed and cannot be created.")), Body, 900.0f);
}

TSharedRef<SWidget> SBreakerMenu::BuildClassSelectScreen()
{
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const EBreakerClassId CurrentClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;

    struct FClassEntry { EBreakerClassId ClassId; const TCHAR* Name; const TCHAR* Resource; const TCHAR* Branches; const TCHAR* Pitch; };
    static const FClassEntry Classes[] =
    {
        { EBreakerClassId::Swift,    TEXT("SWIFT"),    TEXT("MOMENTUM"), TEXT("Frenzy / Kinetic / Marksman"),          TEXT("Speed is the build. Movement generates power.") },
        { EBreakerClassId::Caster,   TEXT("CASTER"),   TEXT("MANA"),     TEXT("Spellblade / Void Whisperer / Multispell"), TEXT("Statuses, reactions, and ability-driven combat.") },
        { EBreakerClassId::Gunsmith, TEXT("GUNSMITH"), TEXT("SCRAP"),    TEXT("Armory / Field Tech / Tinkerer"),       TEXT("Deployables and weapon mastery.") },
        { EBreakerClassId::Tank,     TEXT("TANK"),     TEXT("GRIT"),     TEXT("Leech / Bastion / Demolitionist"),      TEXT("Mitigation becomes fuel. Hold the line.") },
        { EBreakerClassId::Support,  TEXT("SUPPORT"),  TEXT("CHARGE"),   TEXT("Medic / Conductor / Warden"),           TEXT("Amplify, sustain, control — solo viable.") },
    };

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    // D13: a refused class lock renders its reason here instead of the screen
    // rebuilding as if nothing happened.
    if (!CharacterScreenStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(CharacterScreenStatus, BreakerUI::TypeCaption, Amber, true)
        ];
    }
    if (CurrentClass != EBreakerClassId::None)
    {
        const FClassEntry* Locked = nullptr;
        for (const FClassEntry& Entry : Classes) if (Entry.ClassId == CurrentClass) Locked = &Entry;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("CLASS LOCKED: %s — class selection is permanent per character."), Locked ? Locked->Name : TEXT("UNKNOWN"))), 13, Cyan, true)
        ];
    }
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            MenuText(FText::FromString(TEXT("Selection is PERMANENT for this character. Branches and abilities arrive with the class kits.")), 11, SoftText)
        ];
    }

    for (const FClassEntry& Entry : Classes)
    {
        const bool bImplemented = ClassHasImplementedKit(Entry.ClassId);
        // O39: a class with no kit is LOCKED — choosing it would permanently
        // strand a character on nothing, since class selection is one-way.
        // There is no longer a dev bypass: the swap tool that used to open both
        // this gate and the already-chosen lock is gone, so this screen now
        // states one rule and obeys it. Testing another class is a new
        // character, which is the route the create carousel already offers.
        // UBreakerProgressionComponent::DevForceClass survives as an API --
        // several components document their broadcast behaviour against it --
        // it simply has no button any more.
        const bool bDesignLocked = !bImplemented;
        const bool bIsCurrent = Entry.ClassId == CurrentClass;
        const bool bSelectable = !bDesignLocked && CurrentClass == EBreakerClassId::None;
        const EBreakerClassId CapturedClass = Entry.ClassId;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bSelectable ? PanelRaised : BreakerUI::BgRaised)
            // Deliberately NOT IsEnabled(false). Slate's disabled state fades
            // the whole content, which on this screen dimmed all five class
            // names — INCLUDING the one the character actually is — to an
            // unreadable grey the moment a class was locked. FIELDPLATE 01 is
            // explicit that disabled "keeps geometry, drops text to #3E4C5E,
            // strips the accent entirely — never lowers opacity". So the
            // unavailable state is painted, and the click is refused here.
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space16))
            .OnClicked(FOnClicked::CreateLambda([this, CapturedClass, bSelectable]()
            {
                if (!bSelectable) return FReply::Handled();
                if (Character.IsValid() && Character->GetProgression())
                {
                    UBreakerProgressionComponent* Progression = Character->GetProgression();
                    // D13 FIX: the return is CHECKED and a refusal is
                    // SURFACED. Before this the screen ignored the result and
                    // rebuilt silently, so a refused lock (O39, or any future
                    // refusal reason) was indistinguishable from a successful
                    // one — the exact silent-rebuild the fix names.
                    if (Progression->ChoosePermanentClassById(CapturedClass))
                    {
                        CharacterScreenStatus = FText::GetEmpty();
                        Character->SaveGameState();
                    }
                    else
                    {
                        CharacterScreenStatus = FText::FromString(
                            TEXT("That class was refused: it has no implemented kit yet, or a class is already locked."));
                    }
                }
                Rebuild(EBreakerMenuScreen::ClassSelect);
                return FReply::Handled();
            }))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    // The class you ARE is never disabled copy. It is the one
                    // row on this screen that states a fact about the
                    // character, so it reads at full contrast whether or not
                    // it can be clicked.
                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(Entry.Name), BreakerUI::TypeH2, (bIsCurrent || bSelectable) ? Primary : Disabled, true)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MenuText(FText::FromString(Entry.Resource), BreakerUI::TypeCaption, bIsCurrent ? Cyan : (bSelectable ? Muted : Disabled), true)]
                    // The honesty tag: disabled text token, geometry intact,
                    // shown whether or not a class already happens to be
                    // chosen — it is a fact about the CLASS, not about
                    // whether this particular row is clickable right now.
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
                    [
                        bDesignLocked
                            ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(TEXT("IN DESIGN")), BreakerUI::TypeCaption, Disabled, true))
                            : SNullWidget::NullWidget
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)[MenuText(FText::FromString(Entry.Branches), BreakerUI::TypeCaption, (bIsCurrent || bSelectable) ? Muted : Disabled, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)[MenuText(FText::FromString(Entry.Pitch), BreakerUI::TypeCaption, (bIsCurrent || bSelectable) ? SoftText : Disabled)]
            ],
            // Locked-in class carries the accent ring; everything else keeps
            // the same geometry on the neutral rest border.
            bIsCurrent ? Cyan : BorderRest,
            bIsCurrent ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 0.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    return BuildFrame(FText::FromString(TEXT("BREAKER CLASS")), FText::FromString(TEXT("PERMANENT SELECTION / FIVE DISCIPLINES")), Body, 860.0f);
}

namespace
{
    // ---------------------------------------------------------------------
    // Progression adapter.
    //
    // Every call this screen makes into UBreakerProgressionComponent goes
    // through one of these one-line shims. The progression API is being
    // extended concurrently (fallback tree content, purchase/enumeration
    // helpers); when a signature lands or changes, the fix is one line in
    // this block rather than a sweep through the Slate tree below.
    // ---------------------------------------------------------------------

    TArray<const UBreakerProgressionTree*> ProgressionGatherTrees(UBreakerProgressionComponent* Progression)
    {
        TArray<const UBreakerProgressionTree*> Trees;
        if (!Progression) return Trees;
        // The enumerator landed, so this is now the single call it was always
        // meant to be. It matters: the old body walked ClassDefinition->
        // BranchTrees alone, so a character with no class Data Asset saw an
        // empty screen even though the fallback content had trees for them.
        for (const UBreakerProgressionTree* Tree : Progression->GetAvailableTrees())
        {
            if (Tree) Trees.AddUnique(Tree);
        }
        return Trees;
    }

    int32 ProgressionGetNodeRank(UBreakerProgressionComponent* Progression, FName NodeId, EBreakerPointCurrency Currency)
    {
        return Progression ? Progression->GetNodeRank(NodeId, Currency) : 0;
    }

    int32 ProgressionGetUnspent(UBreakerProgressionComponent* Progression, EBreakerPointCurrency Currency)
    {
        if (!Progression) return 0;
        const FBreakerProgressionState& ProgState = Progression->GetProgressionState();
        // Asks the component rather than re-deriving the wallet: it already
        // switches three ways and a second copy here is how the screen and the
        // rules drift apart. ProgState is still read by the caller for other
        // fields, so this is not a spare lookup.
        (void)ProgState;
        return Progression->GetUnspentPoints(Currency);
    }

    bool ProgressionPurchaseNode(UBreakerProgressionComponent* Progression, const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason)
    {
        if (!Progression || !Tree)
        {
            OutFailureReason = FText::FromString(TEXT("No progression component."));
            return false;
        }
        return Progression->PurchaseNode(Tree, NodeId, OutFailureReason);
    }

    bool ProgressionRespec(UBreakerProgressionComponent* Progression, EBreakerPointCurrency Currency, bool bIsAtForge, FText& OutFailureReason)
    {
        if (!Progression)
        {
            OutFailureReason = FText::FromString(TEXT("No progression component."));
            return false;
        }
        // A REAL ANSWER NOW. This passed true unconditionally, with a comment
        // deferring the gate until the hub existed; the hub exists, and the
        // Forge screen is reachable only through Kess, so arriving here means
        // the player is standing at one. The flag is set by that door and by
        // nothing else, which is what makes the refusal path reachable.
        return Progression->RespecAtForge(Currency, bIsAtForge, OutFailureReason);
    }

    // Points already committed to a tree, and the tree's full cost if every
    // node were maxed. Derived locally from node ranks so it needs no new
    // progression API.
    void ProgressionTreeInvestment(UBreakerProgressionComponent* Progression, const UBreakerProgressionTree* Tree, int32& OutSpent, int32& OutTotal)
    {
        OutSpent = 0;
        OutTotal = 0;
        if (!Tree) return;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            OutSpent += ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) * Node->CostPerRank;
            OutTotal += Node->MaxRank * Node->CostPerRank;
        }
    }

    // Card body is one short generic line: the first sentence, clipped.
    FString ShortSummary(const FString& Description, int32 MaxLength = 60)
    {
        FString Line = Description;
        int32 SentenceEnd = INDEX_NONE;
        if (Line.FindChar(TEXT('.'), SentenceEnd)) Line = Line.Left(SentenceEnd + 1);
        Line.ReplaceInline(TEXT("\n"), TEXT(" "));
        Line.ReplaceInline(TEXT("\r"), TEXT(""));
        Line.TrimStartAndEndInline();
        if (Line.Len() > MaxLength) Line = Line.Left(MaxLength - 1).TrimEnd() + TEXT("…");
        return Line;
    }

    // Short, player-facing stat names. UEnum display names read like code
    // ("DamageOverTime"); the card has room for two words, not a symbol.
    FString StatTargetLabel(EBreakerNodeStatTarget Target)
    {
        switch (Target)
        {
        case EBreakerNodeStatTarget::CriticalChance: return TEXT("CRIT CHANCE");
        case EBreakerNodeStatTarget::CriticalDamage: return TEXT("CRIT DAMAGE");
        case EBreakerNodeStatTarget::MoveSpeed:      return TEXT("MOVE SPEED");
        case EBreakerNodeStatTarget::SlideSpeed:     return TEXT("SLIDE SPEED");
        case EBreakerNodeStatTarget::AirControl:     return TEXT("AIR CONTROL");
        case EBreakerNodeStatTarget::DodgeChance:    return TEXT("DODGE CHANCE");
        case EBreakerNodeStatTarget::BlockChance:    return TEXT("BLOCK CHANCE");
        case EBreakerNodeStatTarget::Health:         return TEXT("HEALTH");
        case EBreakerNodeStatTarget::DamageOverTime: return TEXT("DOT DAMAGE");
        // Damage was missing from this switch, so every damage node on the
        // board printed "+4% STAT" — the one stat the owner most wanted to see
        // was the one with no name.
        case EBreakerNodeStatTarget::Damage:         return TEXT("DAMAGE");
        // ---- The O30 widening. 21 targets landed at once, and this switch's
        // `default` would have printed "STAT" for every one of them — the same
        // defect the Damage line above records, at twenty-one times the scale.
        // Named here in the SAME pass that added them rather than after.
        case EBreakerNodeStatTarget::AbilityDamage:  return TEXT("ABILITY DAMAGE");
        case EBreakerNodeStatTarget::AbilityCost:    return TEXT("ABILITY COST");
        case EBreakerNodeStatTarget::AbilityCooldown:return TEXT("ABILITY COOLDOWN");
        case EBreakerNodeStatTarget::AbilityArea:    return TEXT("ABILITY AREA");
        case EBreakerNodeStatTarget::AbilityDuration:return TEXT("ABILITY DURATION");
        case EBreakerNodeStatTarget::WeaponDamage:   return TEXT("WEAPON DAMAGE");
        case EBreakerNodeStatTarget::MeleeDamage:    return TEXT("MELEE DAMAGE");
        case EBreakerNodeStatTarget::IncomingDamageReduction: return TEXT("DAMAGE TAKEN");
        case EBreakerNodeStatTarget::Armor:          return TEXT("ARMOUR");
        case EBreakerNodeStatTarget::Lifesteal:      return TEXT("LIFESTEAL");
        case EBreakerNodeStatTarget::MaxClassResource:  return TEXT("MAX RESOURCE");
        case EBreakerNodeStatTarget::ClassResourceRegen:return TEXT("RESOURCE REGEN");
        case EBreakerNodeStatTarget::ClassResourceDecay:return TEXT("RESOURCE DECAY");
        case EBreakerNodeStatTarget::FireRate:       return TEXT("FIRE RATE");
        case EBreakerNodeStatTarget::DashCooldown:   return TEXT("DASH COOLDOWN");
        // ---- Swift projectile channels (2026-08-16), named in the same pass
        // that made their lanes live — the four targets nodes can now author
        // and a player can now feel.
        case EBreakerNodeStatTarget::ProjectileCount:return TEXT("PROJECTILES");
        case EBreakerNodeStatTarget::Pierce:         return TEXT("PIERCE");
        case EBreakerNodeStatTarget::ChainCount:     return TEXT("CHAIN");
        case EBreakerNodeStatTarget::RicochetCount:  return TEXT("RICOCHET");
        default:                                     return TEXT("STAT");
        }
    }

    // Board copy only. The marker label has one line for the effect and a
    // fixed pixel width; the full names live on the detail rail, which has
    // room for them.
    FString ShortStatLabel(EBreakerNodeStatTarget Target)
    {
        switch (Target)
        {
        case EBreakerNodeStatTarget::CriticalChance: return TEXT("CRIT");
        case EBreakerNodeStatTarget::CriticalDamage: return TEXT("CRIT DMG");
        case EBreakerNodeStatTarget::MoveSpeed:      return TEXT("MOVE");
        case EBreakerNodeStatTarget::SlideSpeed:     return TEXT("SLIDE");
        case EBreakerNodeStatTarget::AirControl:     return TEXT("AIR");
        case EBreakerNodeStatTarget::DodgeChance:    return TEXT("DODGE");
        case EBreakerNodeStatTarget::BlockChance:    return TEXT("BLOCK");
        case EBreakerNodeStatTarget::Health:         return TEXT("HP");
        case EBreakerNodeStatTarget::DamageOverTime: return TEXT("DOT");
        case EBreakerNodeStatTarget::Damage:         return TEXT("DMG");
        // Same widening, abbreviated: this label sits in a fixed pixel width on
        // the board marker, so every string here is kept to roughly the length
        // of the ones above it. "ABILITY COOLDOWN" would clip; "ABL CD" does
        // not — and clipped text has been reported four times in this file.
        case EBreakerNodeStatTarget::AbilityDamage:  return TEXT("ABL DMG");
        case EBreakerNodeStatTarget::AbilityCost:    return TEXT("ABL COST");
        case EBreakerNodeStatTarget::AbilityCooldown:return TEXT("ABL CD");
        case EBreakerNodeStatTarget::AbilityArea:    return TEXT("ABL AREA");
        case EBreakerNodeStatTarget::AbilityDuration:return TEXT("ABL TIME");
        case EBreakerNodeStatTarget::WeaponDamage:   return TEXT("WPN DMG");
        case EBreakerNodeStatTarget::MeleeDamage:    return TEXT("MELEE");
        case EBreakerNodeStatTarget::IncomingDamageReduction: return TEXT("DMG TAKEN");
        case EBreakerNodeStatTarget::Armor:          return TEXT("ARMOUR");
        case EBreakerNodeStatTarget::Lifesteal:      return TEXT("LEECH");
        case EBreakerNodeStatTarget::MaxClassResource:  return TEXT("MAX RES");
        case EBreakerNodeStatTarget::ClassResourceRegen:return TEXT("RES REGEN");
        case EBreakerNodeStatTarget::ClassResourceDecay:return TEXT("RES DECAY");
        case EBreakerNodeStatTarget::FireRate:       return TEXT("FIRE RATE");
        case EBreakerNodeStatTarget::DashCooldown:   return TEXT("DASH CD");
        // Swift projectile channels, kept to marker width like the rest.
        case EBreakerNodeStatTarget::ProjectileCount:return TEXT("MULTI");
        case EBreakerNodeStatTarget::Pierce:         return TEXT("PIERCE");
        case EBreakerNodeStatTarget::ChainCount:     return TEXT("CHAIN");
        case EBreakerNodeStatTarget::RicochetCount:  return TEXT("RICO");
        default:                                     return TEXT("STAT");
        }
    }

    // "+12% MOVE SPEED" / "+7 CRIT CHANCE". Flat crit/dodge/block values are
    // authored in whole points, so they carry no percent sign here even
    // though they land as chance fractions.
    FString FormatNodeEffect(const FBreakerNodeEffect& Effect)
    {
        const bool bPercent = Effect.StatBucket != EBreakerNodeStatBucket::Flat;
        const FString Number = FString::Printf(TEXT("%+g"), Effect.ValuePerRank);
        return FString::Printf(TEXT("%s%s %s"), *Number, bPercent ? TEXT("%") : TEXT(""), *StatTargetLabel(Effect.StatTarget));
    }

    // The one line on the BOARD that tells a player what they are buying.
    //
    // This used to read "+18 CRIT DAMAGE / RANK  (+1 MORE)" — 33 characters in
    // a 168px box, which wrapped to three lines and pushed the state line down
    // through the tier hairline beneath it. Every effect now fits on one line
    // as "+18 CRIT DMG · +3% DMG"; the unabbreviated version is on the rail.
    FString CompactEffectLine(const UBreakerProgressionNode* Node)
    {
        if (!Node) return FString();
        if (Node->Effects.Num() > 0)
        {
            TArray<FString> Parts;
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                const bool bPercent = Effect.StatBucket != EBreakerNodeStatBucket::Flat;
                Parts.Add(FString::Printf(TEXT("%+g%s %s"), Effect.ValuePerRank,
                    bPercent ? TEXT("%") : TEXT(""), *ShortStatLabel(Effect.StatTarget)));
                if (Parts.Num() == 2) break;
            }
            FString Line = FString::Join(Parts, TEXT(" · "));
            if (Node->Effects.Num() > 2) Line += TEXT(" +");
            return Line;
        }
        if (Node->GrantedAbilityIds.Num() > 0)
        {
            return FString::Printf(TEXT("GRANTS %s"), *Node->GrantedAbilityIds[0].ToString().ToUpper());
        }
        return TEXT("RULE CHANGE");
    }

    // Post-purchase status line: name, every effect at its per-rank value,
    // and the rank the player just reached.
    FString PurchaseFeedback(const UBreakerProgressionNode* Node, int32 NewRank)
    {
        if (!Node) return TEXT("ALLOCATED");
        const FString Name = Node->DisplayName.IsEmpty() ? Node->NodeId.ToString().ToUpper() : Node->DisplayName.ToString().ToUpper();
        TArray<FString> Parts;
        for (const FBreakerNodeEffect& Effect : Node->Effects) Parts.Add(FormatNodeEffect(Effect));
        for (const FName AbilityId : Node->GrantedAbilityIds) Parts.Add(FString::Printf(TEXT("GRANTS %s"), *AbilityId.ToString().ToUpper()));
        if (Parts.Num() == 0) Parts.Add(TEXT("RULE CHANGE ACTIVE"));
        return FString::Printf(TEXT("%s  %s  (RANK %d/%d)"), *Name, *FString::Join(Parts, TEXT("  ")), NewRank, Node->MaxRank);
    }

    // Owned rank reads as a word, not a row of small circles the owner had
    // to squint at.
    FString RankLabel(int32 Rank, int32 MaxRank)
    {
        return FString::Printf(TEXT("RANK %d/%d"), Rank, MaxRank);
    }

    // Selector buttons are 240px wide; long authored tree names clip there
    // ("CORE CONSTELLATIONS (S..."). Short display aliases live here rather
    // than in the content library so authored names stay descriptive.
    FString TreeSelectorLabel(const UBreakerProgressionTree* Tree)
    {
        if (!Tree) return FString();
        if (Tree->TreeId == FName(TEXT("Core.Slice"))) return TEXT("CORE SLICE");
        const FString Name = Tree->DisplayName.IsEmpty() ? Tree->TreeId.ToString() : Tree->DisplayName.ToString();
        return Name.ToUpper();
    }

    FString CurrencyLabel(EBreakerPointCurrency Currency)
    {
        return Currency == EBreakerPointCurrency::DoctrinePoints ? TEXT("DOCTRINE") : TEXT("CORE");
    }

    // INTEGRATION: the progression API is expected to grow
    //   bool UBreakerProgressionComponent::CanPurchase(const UBreakerProgressionTree*, FName, FText& OutReason) const
    // Until it exists this mirrors the purchase rules the component enforces
    // (max rank, unspent points, investment gate, prerequisite ranks) so the
    // cards can render a lock reason without attempting a purchase. Delete
    // this function and forward to CanPurchase when it lands — the returned
    // reason text is already shaped for direct display.
    //
    // OutShortReason is the same failure in board-label width: the fixed
    // marker label has one line for it, and a wrapped four-line lock reason is
    // what used to run into the node beneath. The full sentence still reaches
    // the player on the detail rail and in the status line.
    //
    // OutTierGated says the ONLY thing standing in the way is the tier's entry
    // requirement. The board states that requirement once, in the tier gutter,
    // so a node held only by it must not repeat it — the repetition was the
    // `GATE 0/2` printed under every locked marker, which is uniform noise
    // rather than information, and which collided with the gutter's own `GATE`
    // label meaning something else entirely.
    bool SkillNodeIsPurchasable(UBreakerProgressionComponent* Progression, const UBreakerProgressionTree* Tree, const UBreakerProgressionNode* Node, int32 TreeSpent, FString& OutLockReason, FString* OutShortReason = nullptr, bool* OutTierGated = nullptr)
    {
        OutLockReason.Reset();
        if (OutShortReason) OutShortReason->Reset();
        if (OutTierGated) *OutTierGated = false;
        auto Fail = [&OutLockReason, OutShortReason](const FString& Full, const FString& Short)
        {
            OutLockReason = Full;
            if (OutShortReason) *OutShortReason = Short;
            return false;
        };

        if (!Progression || !Tree || !Node)
        {
            return Fail(TEXT("NO DATA"), TEXT("NO DATA"));
        }
        if (ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) >= Node->MaxRank)
        {
            return Fail(TEXT("MAX RANK"), TEXT("MAXED"));
        }
        // O37 COMMITMENT, and it belongs BEFORE the investment gate because it
        // is the stronger and more surprising of the two — a player who has not
        // committed should be told to commit, not told to invest more into a
        // node that commitment alone will not open.
        //
        // THIS WAS MISSING ENTIRELY, and it made the board lie. The screen
        // keeps its own mirror of the purchase rules, and the mirror checked
        // rank, investment, prerequisites, exclusions and points — but not
        // bCornerstone/CommittedBranch and not the tree's
        // CornerstoneInvestmentGate. So with a keystone's tier gate met the
        // board painted it AMBER, counted it in the footer's "N PURCHASABLE",
        // and the detail card promised the buy — and then the click failed
        // against UBreakerProgressionComponent::CanPurchaseNode, which does
        // check both. Promising a purchase and refusing it is worse than
        // showing it locked.
        // AND THE LABEL SAYS "REQUIRES", NOT "UNLOCKS". It read "COMMIT TO THIS
        // BRANCH TO UNLOCK ITS KEYSTONE", which promises that commitment opens
        // the node -- and the comment directly above already knew better, in the
        // same breath: "not told to invest more into a node that commitment
        // alone will not open". Under O111's benchmark economy the gap is no
        // longer a nuance. A keystone needs six invested plus its own two, the
        // pool pays two points at each of four benchmarks, and eight points do
        // not exist before the last one -- so commitment cannot open a keystone
        // for the entire campaign, and a label saying it does is a promise the
        // screen keeps for nobody below the cap.
        if (Node->bCornerstone && Progression->GetProgressionState().CommittedBranch != Tree->TreeId)
        {
            if (OutTierGated) *OutTierGated = true;
            return Fail(
                TEXT("THIS BRANCH'S KEYSTONE REQUIRES COMMITMENT"),
                TEXT("COMMIT REQUIRED"));
        }
        // The component takes the MAX of the node's own investment requirement
        // and, for a cornerstone, the tree's cornerstone gate — which defaults
        // to 8 against a tier-3 authored requirement of 4. Mirroring only the
        // node's own number was the second half of the same divergence.
        const int32 EffectiveInvestmentGate = FMath::Max(
            Node->RequiredTreeInvestment,
            Node->bCornerstone ? Tree->CornerstoneInvestmentGate : 0);
        if (EffectiveInvestmentGate > TreeSpent)
        {
            if (OutTierGated) *OutTierGated = true;
            return Fail(
                FString::Printf(TEXT("TIER OPENS AT %d INVESTED (%d SO FAR)"), EffectiveInvestmentGate, TreeSpent),
                FString::Printf(TEXT("%d / %d INVESTED"), TreeSpent, EffectiveInvestmentGate));
        }
        for (const FBreakerNodePrerequisite& Prereq : Node->Prerequisites)
        {
            const int32 HeldRank = ProgressionGetNodeRank(Progression, Prereq.NodeId, Node->Currency);
            if (HeldRank < Prereq.RequiredRank)
            {
                const UBreakerProgressionNode* PrereqNode = nullptr;
                for (const UBreakerProgressionNode* Candidate : Tree->Nodes)
                {
                    if (Candidate && Candidate->NodeId == Prereq.NodeId) PrereqNode = Candidate;
                }
                const FString PrereqName = PrereqNode ? PrereqNode->DisplayName.ToString() : Prereq.NodeId.ToString();
                return Fail(
                    FString::Printf(TEXT("NEEDS %s RANK %d"), *PrereqName.ToUpper(), Prereq.RequiredRank),
                    FString::Printf(TEXT("NEEDS %s"), *PrereqName.ToUpper()));
            }
        }
        for (const FName ExclusiveId : Node->MutuallyExclusiveNodeIds)
        {
            if (ProgressionGetNodeRank(Progression, ExclusiveId, Node->Currency) > 0)
            {
                return Fail(
                    FString::Printf(TEXT("LOCKED OUT BY %s"), *ExclusiveId.ToString().ToUpper()),
                    TEXT("LOCKED OUT"));
            }
        }
        if (ProgressionGetUnspent(Progression, Node->Currency) < Node->CostPerRank)
        {
            return Fail(
                FString::Printf(TEXT("NEEDS %d %s POINTS"), Node->CostPerRank, *CurrencyLabel(Node->Currency)),
                FString::Printf(TEXT("NEED %d PT"), Node->CostPerRank));
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Path-board node kinds (UI-Skill-Tree-Spec "Class <-> Core").
    //
    // INTEGRATION: UBreakerProgressionNode carries no node-kind field, so the
    // kind is derived from rules the content already states. When a Kind enum
    // lands on the node asset, delete this and read it directly.
    //   Keystone    — the authored cornerstone flag
    //   Minor       — anything multi-rank (the rank prints inside the marker)
    //   Convergence — single-rank nodes costing 3+ points (the O21 promotion
    //                 tier: Fixate/Necrosis/Reflex all sit here)
    //   Notable     — everything else
    // -----------------------------------------------------------------------
    enum class ESkillMarkerKind : uint8 { Minor, Notable, Convergence, Keystone };

    // A node that hands its owner a `Keystone.*` tag IS a keystone, whatever
    // the authoring flag says.
    //
    // This is why Bloodrhythm rendered wrong. It is Swift/Frenzy's branch
    // keystone — it grants Keystone.Swift.Bloodrhythm, which is a real row in
    // Overdrive's variant table — but the fallback content never set
    // bCornerstone, so ClassifyNode fell through to "single-rank costing 3+"
    // and drew the game's first working keystone as an ordinary Convergence
    // square. The content lives in Progression/, which this screen does not
    // own; reading the tag the content already publishes is both the fix
    // available here and the more truthful test, since the tag is what
    // actually makes a keystone a keystone at runtime.
    bool NodeCarriesKeystoneTag(const UBreakerProgressionNode* Node)
    {
        if (!Node) return false;
        for (const FGameplayTag& Tag : Node->GrantedTags)
        {
            if (Tag.ToString().StartsWith(TEXT("Keystone."))) return true;
        }
        return false;
    }

    ESkillMarkerKind ClassifyNode(const UBreakerProgressionNode* Node)
    {
        if (!Node) return ESkillMarkerKind::Minor;
        if (Node->bCornerstone || NodeCarriesKeystoneTag(Node)) return ESkillMarkerKind::Keystone;
        if (Node->MaxRank > 1) return ESkillMarkerKind::Minor;
        if (Node->CostPerRank >= 3) return ESkillMarkerKind::Convergence;
        return ESkillMarkerKind::Notable;
    }

    // The spec's authored marker geometry (UI-Skill-Tree-Spec "Class <-> Core").
    // It is a FLOOR, not a fixed size: MarkerSizeForLabel below grows it when
    // the rank text inside genuinely needs more room.
    float MarkerBaseSize(ESkillMarkerKind Kind)
    {
        switch (Kind)
        {
            case ESkillMarkerKind::Notable:     return 44.0f;
            case ESkillMarkerKind::Convergence: return 64.0f;
            case ESkillMarkerKind::Keystone:    return 60.0f;
            default:                            return 48.0f;
        }
    }

    bool MarkerIsDiamond(ESkillMarkerKind Kind)
    {
        return Kind == ESkillMarkerKind::Notable || Kind == ESkillMarkerKind::Keystone;
    }

    // Convergence and Keystone label to the RIGHT of the marker so the trunk
    // never runs through their text.
    bool MarkerLabelsRight(ESkillMarkerKind Kind)
    {
        return Kind == ESkillMarkerKind::Convergence || Kind == ESkillMarkerKind::Keystone;
    }

    // How heavy the marker's own ring is. A keystone always carries rail
    // weight, so it reads as the most important shape on the board even
    // locked; everything else uses the ordinary selected/rest border weights.
    float MarkerRingThickness(ESkillMarkerKind Kind, bool bOwnedOrPurchasable)
    {
        if (Kind == ESkillMarkerKind::Keystone) return BreakerUI::RailThickness;
        if (Kind == ESkillMarkerKind::Convergence) return BreakerUI::BorderSelected;
        return bOwnedOrPurchasable ? BreakerUI::BorderSelected : BreakerUI::BorderThin;
    }

    // -----------------------------------------------------------------------
    // The rank text INSIDE a marker, and why it was losing its last glyph.
    //
    // THE CAUSE, found by photographing the board and measuring the pixels
    // rather than by nudging a pad. A marker is BorderWrap(SButton[ text ]),
    // and SButton aligns its content HAlign_Center — which in Slate means the
    // child is arranged at exactly its own DESIRED width, never at the width
    // available. An STextBlock's desired width is its MEASURED width, and
    // Slate then clips the drawn run to that same box (the default
    // ETextOverflowPolicy). Measurement and rasterisation round independently,
    // so a run that lands a fraction wider than its measurement loses the
    // right edge of its final glyph — which is exactly what `0/2` did, and
    // what a `0` did at 30px on the Core chips, where the previous pass read
    // it as "the box is too small" and bumped the box. The box was never the
    // problem: a 48px marker has 40px of content area for 20px of text. The
    // text simply was never GIVEN it.
    //
    // The fix is two-part and holds for `10/10` as well as `0/2`:
    //   1. the text is arranged in a box measured to fit it with slack, so the
    //      overflow clip has nothing to cut;
    //   2. the marker is sized from that same measurement, so the box it needs
    //      always fits inside the marker's chrome.
    // -----------------------------------------------------------------------

    // SButton draws the FCoreStyle button's own 2px NormalPadding underneath
    // whatever ContentPadding we ask for, on every side.
    inline constexpr float MarkerButtonPad = 2.0f;
    // Slack around the measured run. Two pixels a side puts the arranged box
    // outside any rounding difference between measuring a string and drawing
    // it; it is not a fudge for an unknown size, it is the known quantisation.
    inline constexpr float MarkerTextSlack = 4.0f;

    // Measures TEXT against a FONT, which is a pure function of inputs known
    // before layout runs — the sanctioned pattern in this file (see
    // MeasureChipWidth). Nothing here reads an allotted size, so there is no
    // layout feedback loop.
    float MarkerTextWidth(const FString& Label)
    {
        if (Label.IsEmpty()) return 0.0f;
        if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
        {
            const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption);
            return static_cast<float>(Measure->Measure(Label, Font).X);
        }
        // Headless: nothing is on screen, and erring wide only makes a marker
        // a few pixels larger than it needs to be.
        return Label.Len() * (BreakerUI::TypeCaption * 0.72f);
    }

    // The marker edge length that holds this label, never smaller than the
    // authored floor it is given, and kept on the 4px grid. The path board
    // passes the spec's per-kind geometry; the Core map passes its own chip
    // floor, so one rule serves both boards.
    float MarkerSizeForLabel(float Base, float RingThickness, const FString& CentreLabel)
    {
        if (CentreLabel.IsEmpty()) return Base;
        const float Needed = MarkerTextWidth(CentreLabel) + MarkerTextSlack
            + 2.0f * (RingThickness + MarkerButtonPad);
        const float Snapped = FMath::CeilToFloat(Needed / BreakerUI::Space4) * BreakerUI::Space4;
        return FMath::Max(Base, Snapped);
    }

    // The centre label itself, in a box wide enough that the overflow clip can
    // never reach a glyph. HAlign_Fill inside hands the text the whole box;
    // the justification is what centres the run.
    TSharedRef<SWidget> MakeMarkerLabel(const FString& Label, const FLinearColor& Color)
    {
        return SNew(SBox)
            .WidthOverride(FMath::CeilToFloat(MarkerTextWidth(Label) + MarkerTextSlack))
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Label))
                .ColorAndOpacity(Color)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
                .Justification(ETextJustify::Center)
            ];
    }

    // The mark INSIDE the marker.
    //
    // Everything except a multi-rank Minor used to put an SSpacer here, so a
    // Notable, a Convergence and a Keystone were all an empty box — and an
    // empty box on a panel/00 fill behind a 1px rest-colour ring is not a
    // marker, it is a hole. That is precisely how the keystone read on screen:
    // a solid black square, the LEAST distinct thing on a board where it
    // should be the most. Each kind now carries a filled core in its state
    // colour, and the keystone carries a ringed one, so the four kinds are
    // told apart by silhouette AND by centre, not by size alone.
    // MarkerPixels is the marker's own edge length, so the core scales with
    // whatever the caller draws — the path board at 44/64/60 and the Core
    // constellation map at its compact chip size read as the same language.
    TSharedRef<SWidget> MakeMarkerCore(ESkillMarkerKind Kind, const FLinearColor& StateColor, const FLinearColor& Face,
        float MarkerPixels)
    {
        switch (Kind)
        {
            case ESkillMarkerKind::Notable:
            {
                const float Core = FMath::Max(6.0f, MarkerPixels * 0.27f);
                return SNew(SBox).WidthOverride(Core).HeightOverride(Core)[SolidBlock(StateColor)];
            }
            case ESkillMarkerKind::Convergence:
            {
                const float Core = FMath::Max(8.0f, MarkerPixels * 0.31f);
                return SNew(SBox).WidthOverride(Core).HeightOverride(Core)[SolidBlock(StateColor)];
            }
            case ESkillMarkerKind::Keystone:
            {
                // Ring, gap, core — concentric, which nothing else on the
                // board is.
                const float Core = FMath::Max(14.0f, MarkerPixels * 0.50f);
                return SNew(SBox).WidthOverride(Core).HeightOverride(Core)
                [
                    BorderWrap(
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(Face)
                        .Padding(FMargin(BreakerUI::Space4))
                        [
                            SolidBlock(StateColor)
                        ],
                        StateColor, BreakerUI::BorderSelected)
                ];
            }
            default:
                return SNew(SSpacer).Size(FVector2D(1.0f, 1.0f));
        }
    }

    FString MarkerKindLabel(ESkillMarkerKind Kind)
    {
        switch (Kind)
        {
            case ESkillMarkerKind::Notable:     return TEXT("NOTABLE");
            case ESkillMarkerKind::Convergence: return TEXT("CONVERGENCE");
            case ESkillMarkerKind::Keystone:    return TEXT("KEYSTONE");
            default:                            return TEXT("MINOR");
        }
    }

    // Everything the hover rail prints, captured by value at build time. The
    // hover handler therefore touches no live widget tree and no attribute —
    // the rail is event-driven, never polled.
    struct FSkillNodeView
    {
        FString Name;
        FString Kind;
        FString RankLine;
        FString CostLine;
        FString Description;
        FString ActionLine;
        FString GateLine;
        TArray<FString> EffectLines;
        TArray<FString> PrereqLines;
        // The answer to "what does this point actually BUY": composed totals
        // before and after, computed through the character's real aggregator.
        FString BuyHeadline;
        TArray<FString> BuyLines;
        FString MaxHeadline;
        TArray<FString> MaxLines;
        bool bOwned = false;
        bool bPurchasable = false;
        bool bMaxed = false;
        // Identity, so the rail can be restored to the same node after the
        // screen rebuilds. Everything else here is presentation.
        FName NodeId = NAME_None;
    };

    // "DAMAGE   1.06x -> 1.10x   +4%", one per stat the purchase moves.
    TArray<FString> ProjectionLines(const FBreakerSkillSnapshot& Snapshot, const UBreakerProgressionNode* Node, int32 RankDelta)
    {
        TArray<FString> Lines;
        if (!Node || RankDelta <= 0) return Lines;
        for (const FBreakerStatLine& Line : BreakerSkillProjection::ProjectPurchase(Snapshot, Node->NodeId, RankDelta))
        {
            if (!Line.Changed()) continue;
            Lines.Add(FString::Printf(TEXT("%s  %s  %s"),
                *Line.Label,
                *BreakerSkillProjection::FormatTransition(Line),
                *BreakerSkillProjection::FormatDelta(Line)));
        }
        return Lines;
    }

    FSkillNodeView MakeSkillNodeView(const UBreakerProgressionNode* Node, int32 Rank, bool bPurchasable,
        const FString& LockReason, int32 TreeSpent, const FBreakerSkillSnapshot& Snapshot)
    {
        FSkillNodeView View;
        if (!Node) return View;
        const ESkillMarkerKind Kind = ClassifyNode(Node);
        View.NodeId = Node->NodeId;
        View.Name = Node->DisplayName.IsEmpty() ? Node->NodeId.ToString().ToUpper() : Node->DisplayName.ToString().ToUpper();
        View.Kind = MarkerKindLabel(Kind) + (Node->bCornerstone ? TEXT("  ·  CORNERSTONE") : TEXT(""));
        View.bOwned = Rank > 0;
        View.bMaxed = Rank >= Node->MaxRank;
        View.bPurchasable = bPurchasable;
        View.RankLine = View.bMaxed ? FString(TEXT("MAXED")) : RankLabel(Rank, Node->MaxRank);
        View.CostLine = FString::Printf(TEXT("%d %s PER RANK"), Node->CostPerRank, *CurrencyLabel(Node->Currency));
        View.Description = Node->Description.IsEmpty() ? TEXT("—") : Node->Description.ToString();
        View.ActionLine = View.bMaxed
            ? FString(TEXT("MAXED"))
            : (bPurchasable ? FString::Printf(TEXT("%d PT -> RANK %d"), Node->CostPerRank, Rank + 1) : LockReason);
        for (const FBreakerNodeEffect& Effect : Node->Effects) View.EffectLines.Add(FormatNodeEffect(Effect));
        for (const FName AbilityId : Node->GrantedAbilityIds) View.EffectLines.Add(FString::Printf(TEXT("GRANTS %s"), *AbilityId.ToString().ToUpper()));
        for (const FBreakerNodePrerequisite& Prereq : Node->Prerequisites)
        {
            View.PrereqLines.Add(FString::Printf(TEXT("%s RANK %d"), *Prereq.NodeId.ToString().ToUpper(), Prereq.RequiredRank));
        }
        if (Node->RequiredTreeInvestment > 0)
        {
            View.GateLine = FString::Printf(TEXT("TIER GATE %d / %d"), TreeSpent, Node->RequiredTreeInvestment);
        }

        // The projection is offered whenever there is a rank left to buy, even
        // when the node is currently locked: seeing what a node is WORTH is
        // most of the reason to work toward it.
        const int32 RemainingRanks = FMath::Max(0, Node->MaxRank - Rank);
        if (RemainingRanks > 0)
        {
            View.BuyHeadline = FString::Printf(TEXT("ONE POINT (%d PT) BUYS"), Node->CostPerRank);
            View.BuyLines = ProjectionLines(Snapshot, Node, 1);
            if (View.BuyLines.Num() == 0)
            {
                // Honest: some nodes are pure rule rewrites. The point-spend
                // baseline still pays, so an empty list here means the baseline
                // is switched off, not that the node is a trap.
                View.BuyLines.Add(TEXT("NO STAT CHANGE — THIS NODE REWRITES A RULE"));
            }
            if (RemainingRanks > 1)
            {
                View.MaxHeadline = FString::Printf(TEXT("TO MAX (%d PT) BUYS"), RemainingRanks * Node->CostPerRank);
                View.MaxLines = ProjectionLines(Snapshot, Node, RemainingRanks);
            }
        }
        return View;
    }

    // The 420px rail's card. Fixed width comes from the host box, so this can
    // grow vertically without ever moving the board.
    TSharedRef<SWidget> MakeSkillDetailCard(const FSkillNodeView& View)
    {
        const FLinearColor Rail = View.bOwned ? BreakerUI::Cyan
            : (View.bPurchasable ? BreakerUI::Gold : BreakerUI::BorderEmphasis);

        TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
        Column->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(View.Name))
            .ColorAndOpacity(BreakerUI::TextPrimary)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeH2))
            .AutoWrapText(true)
        ];
        Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(View.Kind), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
        ];
        Column->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(View.RankLine), BreakerUI::TypeCaption, View.bOwned ? BreakerUI::Cyan : BreakerUI::TextMuted, true)]
            + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(View.CostLine), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)]
        ];
        Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, BreakerUI::Space8)
        [
            SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(BreakerUI::BorderRest)]
        ];
        Column->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(View.Description))
            .ColorAndOpacity(BreakerUI::TextSecondary)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeBody))
            .AutoWrapText(true)
        ];
        // The before/after block sits ABOVE the per-rank effects on purpose.
        // "+3% damage per rank" is the authoring value; "1.06x -> 1.10x" is
        // the thing the player is actually deciding about, so it reads first.
        auto AddProjectionBlock = [&Column](const FString& Headline, const TArray<FString>& Lines, const FLinearColor& Accent)
        {
            if (Headline.IsEmpty() || Lines.Num() == 0) return;
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
            [
                MenuText(FText::FromString(Headline), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
            for (const FString& Line : Lines)
            {
                Column->AddSlot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Line))
                    .ColorAndOpacity(Accent)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
                    .AutoWrapText(true)
                ];
            }
        };
        AddProjectionBlock(View.BuyHeadline, View.BuyLines, BreakerUI::Gold);
        AddProjectionBlock(View.MaxHeadline, View.MaxLines, BreakerUI::TextSecondary);

        if (View.EffectLines.Num() > 0)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
            [
                MenuText(FText::FromString(TEXT("EFFECTS PER RANK")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
            for (const FString& Line : View.EffectLines)
            {
                Column->AddSlot().AutoHeight()[MenuText(FText::FromString(Line), BreakerUI::TypeCaption, BreakerUI::TextPrimary, true)];
            }
        }
        if (View.PrereqLines.Num() > 0)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
            [
                MenuText(FText::FromString(TEXT("PREREQUISITES")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
            for (const FString& Line : View.PrereqLines)
            {
                Column->AddSlot().AutoHeight()[MenuText(FText::FromString(Line), BreakerUI::TypeCaption, BreakerUI::TextSecondary, true)];
            }
        }
        if (!View.GateLine.IsEmpty())
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
            [
                MenuText(FText::FromString(View.GateLine), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
        }
        if (!View.ActionLine.IsEmpty())
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
            [
                MenuText(FText::FromString(View.ActionLine), BreakerUI::TypeCaption,
                    View.bMaxed ? BreakerUI::Cyan : (View.bPurchasable ? BreakerUI::Gold : BreakerUI::Harm), true)
            ];
        }
        return MakePlate(Column, BreakerUI::Panel10, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space16));
    }

    // -----------------------------------------------------------------------
    // BUILD TOTALS — "what has my spending added up to".
    //
    // Every number here is READ from the character's live aggregation, not
    // recomputed beside it: the composed rows are the same attributes combat
    // rolls against, and the tree rows are the same FBreakerNodeStats the
    // movement and combat components consume. The DAMAGE row additionally
    // splits its additive Increased bucket by layer, because "my tree gave me
    // +14% of this" is the sentence the owner said was missing.
    // -----------------------------------------------------------------------
    TSharedRef<SWidget> MakeBuildTotalsPlate(const FBreakerSkillSnapshot& Snapshot, int32 ClassSpent, int32 CoreSpent)
    {
        TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
        Column->AddSlot().AutoHeight()
        [
            MenuText(FText::FromString(TEXT("BUILD TOTALS")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
        ];

        auto AddRow = [&Column](const FString& Label, const FString& Value, const FLinearColor& ValueColor, const FLinearColor& LabelColor)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, LabelColor, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    // Fixed value column, so the numbers form a straight edge
                    // and a longer label can never push a value off the plate.
                    // See MenuValueColumn for why the alignment lives on the
                    // text rather than on the box.
                    MenuValueColumn(FText::FromString(Value), 96.0f, BreakerUI::TypeCaption, ValueColor)
                ]
            ];
        };

        const int32 Committed = ClassSpent + CoreSpent;
        Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("%d POINTS COMMITTED  ·  CLASS %d · CORE %d"), Committed, ClassSpent, CoreSpent)),
                BreakerUI::TypeCaption, BreakerUI::Gold, true)
        ];

        const TArray<FBreakerStatLine> Totals = BreakerSkillProjection::CurrentTotals(Snapshot);
        for (int32 Index = 0; Index < Totals.Num(); ++Index)
        {
            const FBreakerStatLine& Line = Totals[Index];
            // The five composed rows always show — a player checking their
            // damage wants to see it whether or not the tree moved it. The
            // tree-only rows show when the tree has actually moved them, so
            // the plate does not become a wall of 1.00x.
            const bool bAlwaysShow = Index < 5;
            const bool bAtIdentity = Line.Format == EBreakerStatFormat::Multiplier
                ? FMath::IsNearlyEqual(Line.Before, 1.0f, 0.0005f)
                : FMath::IsNearlyZero(Line.Before, 0.0005f);
            if (!bAlwaysShow && bAtIdentity) continue;

            const FLinearColor ValueColor = Index == 0 ? BreakerUI::Orange
                : (Index < 5 ? BreakerUI::TextPrimary : BreakerUI::Cyan);
            AddRow(Line.bTreeOnly ? Line.Label + TEXT(" (TREE)") : Line.Label,
                BreakerSkillProjection::FormatStat(Line.Before, Line.Format),
                ValueColor, BreakerUI::TextMuted);

            if (Index == 0 && Snapshot.bHasComposedAttributes)
            {
                // The whole point of the one-additive-bucket rule made visible.
                const float FromTree = BreakerSkillProjection::LayerIncreasedPercent(
                    Snapshot, EBreakerAttributeContributor::Progression, EBreakerAggregatedAttribute::DamageMultiplier);
                const float FromGear = BreakerSkillProjection::LayerIncreasedPercent(
                    Snapshot, EBreakerAttributeContributor::Equipment, EBreakerAggregatedAttribute::DamageMultiplier);
                Column->AddSlot().AutoHeight()
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("    TREE +%.0f%%  ·  GEAR +%.0f%%"), FromTree, FromGear)),
                        BreakerUI::TypeCaption, BreakerUI::TextMuted, false)
                ];
            }
        }

        if (!Snapshot.bHasComposedAttributes)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("No attribute set is bound, so these are the tree layer alone — gear is not folded in.")))
                .ColorAndOpacity(BreakerUI::TextMuted)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
                .AutoWrapText(true)
            ];
        }

        return MakePlate(Column, BreakerUI::Panel10, BreakerUI::Gold, FMargin(BreakerUI::Space16, BreakerUI::Space16));
    }

    // Rest state of the rail. It is the same plate geometry as a populated
    // card, so the column never changes shape when a node is hovered.
    TSharedRef<SWidget> MakeSkillDetailPlaceholder()
    {
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(TEXT("NODE DETAIL")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Hover a marker to read its full detail here. This column never changes width, so the board does not move when it fills.")))
                .ColorAndOpacity(BreakerUI::TextSecondary)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeBody))
                .AutoWrapText(true)
            ],
            BreakerUI::Panel00, BreakerUI::BorderEmphasis, FMargin(BreakerUI::Space16, BreakerUI::Space16));
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildSkillTreesScreen()
{
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const UBreakerAttributeSet* SkillAttributes = Character.IsValid() ? Character->GetAttributes() : nullptr;
    const TArray<const UBreakerProgressionTree*> Trees = ProgressionGatherTrees(Progression);

    // Read the viewport once. Everything below is laid out against these
    // numbers rather than against 1920x1080, which is why the screen no longer
    // walks off the edge of a smaller window.
    const FWideScreenMetrics Metrics = MeasureWideScreen();

    // The character's real aggregation, captured once. Every before/after
    // number on this screen is composed from this snapshot, so the screen can
    // never disagree with the attributes combat actually reads.
    const FBreakerSkillSnapshot Snapshot = BreakerSkillProjection::MakeSnapshot(Progression, SkillAttributes);

    // One tab pair, not a mode toggle: the board swaps, the header and the
    // detail rail persist.
    TArray<const UBreakerProgressionTree*> ClassTrees;
    TArray<const UBreakerProgressionTree*> CoreTrees;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        if (!Tree) continue;
        if (Tree->Currency == EBreakerPointCurrency::DoctrinePoints) ClassTrees.Add(Tree);
        else CoreTrees.Add(Tree);
    }
    if (SkillBoardTab == 0 && ClassTrees.IsEmpty() && !CoreTrees.IsEmpty()) SkillBoardTab = 1;
    if (SkillBoardTab == 1 && CoreTrees.IsEmpty() && !ClassTrees.IsEmpty()) SkillBoardTab = 0;
    const bool bCoreBoard = SkillBoardTab == 1;

    const int32 UnspentClass = ProgressionGetUnspent(Progression, EBreakerPointCurrency::DoctrinePoints);
    const int32 UnspentCore = ProgressionGetUnspent(Progression, EBreakerPointCurrency::CorePoints);

    int32 ClassSpent = 0;
    int32 CoreSpent = 0;
    for (const UBreakerProgressionTree* Tree : ClassTrees)
    {
        int32 Spent = 0;
        int32 Total = 0;
        ProgressionTreeInvestment(Progression, Tree, Spent, Total);
        ClassSpent += Spent;
    }
    for (const UBreakerProgressionTree* Tree : CoreTrees)
    {
        int32 Spent = 0;
        int32 Total = 0;
        ProgressionTreeInvestment(Progression, Tree, Spent, Total);
        CoreSpent += Spent;
    }

    // Which branch the class board draws. Clamped here rather than trusted,
    // because the branch list changes with the class and with content.
    if (SkillBranchIndex >= ClassTrees.Num()) SkillBranchIndex = ClassTrees.Num() > 0 ? 0 : -1;
    if (SkillBranchIndex < -1) SkillBranchIndex = -1;

    // The fixed detail rail, built before the board so hover handlers have a
    // target. It is filled through SetContent on hover and never from a
    // per-frame attribute, and its width never changes, so populating it
    // cannot reflow the board.
    // The rail opens on whatever it was last showing, rebuilt from live data
    // rather than restored as a stale widget — so a purchase leaves the card on
    // screen AND updates its rank, cost and before/after to reflect the buy.
    // Previously the host was re-created empty on every rebuild and Slate does
    // not re-fire OnHovered for a stationary cursor, so buying a node blanked
    // the only surface that explained it.
    TSharedRef<SWidget> InitialDetail = MakeSkillDetailPlaceholder();
    if (!SkillDetailNodeId.IsNone())
    {
        for (const UBreakerProgressionTree* Tree : Trees)
        {
            if (!Tree) continue;
            int32 Spent = 0, Total = 0;
            ProgressionTreeInvestment(Progression, Tree, Spent, Total);
            const UBreakerProgressionNode* Found = nullptr;
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (Node && Node->NodeId == SkillDetailNodeId) { Found = Node; break; }
            }
            if (!Found) continue;
            FString LockReason;
            const bool bPurchasable = SkillNodeIsPurchasable(Progression, Tree, Found, Spent, LockReason);
            InitialDetail = MakeSkillDetailCard(MakeSkillNodeView(
                Found, ProgressionGetNodeRank(Progression, Found->NodeId, Found->Currency),
                bPurchasable, LockReason, Spent, Snapshot));
            break;
        }
    }
    SAssignNew(SkillDetailHost, SBox)
    [
        InitialDetail
    ];

    // What the board is actually DRAWING. The class board draws one branch at
    // a time unless COMPARE ALL is chosen, so this is not the same list as
    // ClassTrees — and the footer count below used ClassTrees, which is how it
    // came to read "8 PURCHASABLE" over a branch showing three gold nodes. A
    // number that disagrees with what the player can see is worse than no
    // number, so the count and the board now read the same list.
    TArray<const UBreakerProgressionTree*> VisibleTrees;
    if (bCoreBoard)
    {
        VisibleTrees = CoreTrees;
    }
    else if (ClassTrees.IsValidIndex(SkillBranchIndex))
    {
        VisibleTrees.Add(ClassTrees[SkillBranchIndex]);
    }
    else
    {
        VisibleTrees = ClassTrees;
    }
    // True when the footer's count covers more than the one branch on screen,
    // which is the only case where it needs to say so.
    const bool bCountSpansBoard = bCoreBoard || VisibleTrees.Num() != 1;

    // Live purchasable count for the footer, over the visible board only.
    int32 PurchasableCount = 0;
    {
        for (const UBreakerProgressionTree* Tree : VisibleTrees)
        {
            int32 Spent = 0;
            int32 Total = 0;
            ProgressionTreeInvestment(Progression, Tree, Spent, Total);
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                FString Ignored;
                if (SkillNodeIsPurchasable(Progression, Tree, Node, Spent, Ignored)) ++PurchasableCount;
            }
        }
    }

    // Wires one marker. Markers are never disabled: a locked node still has to
    // explain itself on hover, and a disabled SButton fires no hover events.
    auto WireMarker = [this](const UBreakerProgressionTree* Tree, const UBreakerProgressionNode* Node,
        const FSkillNodeView& View, bool bPurchasable, const FString& LockReason,
        const FLinearColor& Fill, const FLinearColor& Ring, float RingThickness,
        const TSharedRef<SWidget>& Inner) -> TSharedRef<SWidget>
    {
        return BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Fill)
            .ContentPadding(FMargin(0.0f))
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            // Hovering also REMEMBERS which node the rail is showing. The host
            // is re-created by SAssignNew on every rebuild, and a purchase
            // rebuilds — so the detail card was thrown away on every buy and
            // Slate will not re-fire OnHovered for a cursor that has not moved.
            // The rail went blank at the exact moment the player most wanted to
            // read what they had just bought. Owner: "I dont see any layers or
            // details to them."
            .OnHovered(FSimpleDelegate::CreateLambda([this, View]()
            {
                SkillDetailNodeId = View.NodeId;
                if (SkillDetailHost.IsValid()) SkillDetailHost->SetContent(MakeSkillDetailCard(View));
            }))
            .OnClicked(FOnClicked::CreateLambda([this, Tree, Node, bPurchasable, LockReason]()
            {
                if (!Node) return FReply::Handled();
                if (!bPurchasable)
                {
                    // The action is never silently swallowed: it is disclosed.
                    SkillTreeStatus = FText::FromString(LockReason.IsEmpty() ? FString(TEXT("LOCKED")) : LockReason);
                    Rebuild(EBreakerMenuScreen::SkillTrees);
                    return FReply::Handled();
                }
                UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                // SHIFT buys to max. The modifier state is read once, here, on
                // the click itself — never polled from a per-frame attribute,
                // which is the pattern that made this screen jitter before.
                const bool bToMax = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
                FText FailureReason;
                int32 Bought = 0;
                while (ProgressionPurchaseNode(Prog, Tree, Node->NodeId, FailureReason))
                {
                    ++Bought;
                    if (!bToMax || Bought >= Node->MaxRank) break;
                }
                if (Bought > 0)
                {
                    const int32 NewRank = ProgressionGetNodeRank(Prog, Node->NodeId, Node->Currency);
                    SkillTreeStatus = FText::FromString(PurchaseFeedback(Node, NewRank));
                    if (Character.IsValid()) Character->SaveGameState();
                }
                else
                {
                    SkillTreeStatus = FailureReason.IsEmpty() ? FText::FromString(TEXT("PURCHASE FAILED")) : FailureReason;
                }
                Rebuild(EBreakerMenuScreen::SkillTrees);
                return FReply::Handled();
            }))
            [
                Inner
            ],
            Ring, RingThickness);
    };

    // Shared empty-board plate. Every one of these paths exists today and is
    // reachable: no character, no class, no registered content.
    auto MakeEmptyBoard = [](const FString& Message) -> TSharedRef<SWidget>
    {
        return MakePlate(
            SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(Message), BreakerUI::TypeBody, BreakerUI::TextSecondary)
            ],
            BreakerUI::Panel00, BreakerUI::BorderEmphasis, FMargin(BreakerUI::Space24, BreakerUI::Space24));
    };

    // ---- Class board: PATHS, not a card grid -------------------------------
    auto BuildClassBoard = [&]() -> TSharedRef<SWidget>
    {
        if (ClassTrees.IsEmpty())
        {
            return MakeEmptyBoard(TEXT("[ NO CLASS BRANCHES ]\n\nLock a Breaker class, or register class branch trees,\nand the path board draws here."));
        }

        // Only the SELECTED branch is drawn (SkillBranchIndex == -1 draws them
        // all side by side to compare). One branch at a time is what buys the
        // board the width it needs to be readable — the whole reason it felt
        // cramped was three columns of full-detail nodes fighting for 1200px.
        // VisibleTrees is that selection, resolved once above so the footer
        // count cannot disagree with the board.
        const TArray<const UBreakerProgressionTree*>& Drawn = VisibleTrees;

        const float TopPad = 28.0f;

        TArray<int32> Tiers;
        for (const UBreakerProgressionTree* Tree : Drawn)
        {
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (Node) Tiers.AddUnique(Node->Tier);
            }
        }
        Tiers.Sort();
        if (Tiers.Num() == 0)
        {
            return MakeEmptyBoard(TEXT("[ NO NODES AUTHORED ]\n\nThis branch carries no nodes yet."));
        }

        TArray<int32> BranchSpent;
        TArray<int32> BranchTotal;
        TArray<int32> BranchWidest;
        int32 WidestTierRow = 1;
        bool bAnyRightLabel = false;
        for (const UBreakerProgressionTree* Tree : Drawn)
        {
            int32 Spent = 0;
            int32 Total = 0;
            ProgressionTreeInvestment(Progression, Tree, Spent, Total);
            BranchSpent.Add(Spent);
            BranchTotal.Add(Total);

            int32 Widest = 1;
            for (const int32 Tier : Tiers)
            {
                int32 Count = 0;
                for (const UBreakerProgressionNode* Node : Tree->Nodes)
                {
                    if (!Node || Node->Tier != Tier) continue;
                    ++Count;
                    if (MarkerLabelsRight(ClassifyNode(Node))) bAnyRightLabel = true;
                }
                Widest = FMath::Max(Widest, Count);
            }
            BranchWidest.Add(Widest);
            WidestTierRow = FMath::Max(WidestTierRow, Widest);
        }

        // The tier gutter is MEASURED against the longest sentence it will
        // actually print, not set to a number and hoped over. The owner's
        // screenshot read `AT 2` where the label says `OPENS AT 2`: the gutter
        // was 76px against copy that needed more, so the first two words fell
        // outside the slot. Shortening the words again would only move the
        // same cliff, so the width is derived from the widest gate on this
        // board — `OPENS AT 120` costs the gutter nothing on a board whose
        // gates are single digits.
        int32 WidestGate = 0;
        for (const UBreakerProgressionTree* Tree : Drawn)
        {
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (Node) WidestGate = FMath::Max(WidestGate, Node->RequiredTreeInvestment);
            }
        }
        const float GutterWidth = FMath::Max(104.0f,
            FMath::CeilToFloat(MarkerTextWidth(FString::Printf(TEXT("OPENS AT %d"), WidestGate))
                + 3.0f * BreakerUI::Space8));

        // Node pitch is derived from the KNOWN board viewport (the viewport
        // read at the top of this function), never from an allotted size, so
        // there is no layout feedback loop. The clamp is what keeps the label
        // block from ever being narrower than the copy it must hold.
        const float AvailableField = FMath::Max(320.0f, Metrics.BoardViewWidth - GutterWidth);
        const float NodeSpacing = FMath::Clamp(AvailableField / FMath::Max(1, Drawn.Num() * WidestTierRow), 168.0f, 320.0f);
        const float LabelWidth = FMath::Max(150.0f, NodeSpacing - BreakerUI::Space24);
        // 96 of label under a 64px marker plus breathing room, so a label can
        // no longer run down through the tier hairline beneath it — which is
        // exactly what "numbers clip" looked like on the board.
        // Four lines, not three: a keystone now prints a KEYSTONE caption above
        // its name, and a Canvas slot does not clip an overflowing child — it
        // would have overprinted the tier hairline below instead of truncating,
        // which is the harder defect to recognise as a defect.
        const float LabelHeight = 114.0f;
        const float TierHeight = 216.0f;

        TArray<float> ColumnWidth;
        for (int32 Index = 0; Index < Drawn.Num(); ++Index)
        {
            // Column is sized to the widest tier row it must hold, so the
            // labels of neighbouring nodes cannot collide.
            ColumnWidth.Add(FMath::Max(NodeSpacing + LabelWidth, BranchWidest[Index] * NodeSpacing));
        }

        float FieldWidth = GutterWidth;
        for (const float Width : ColumnWidth) FieldWidth += Width;
        // Convergence/Keystone labels sit to the right of their marker, so the
        // board is a label wider than the field — but only when a node on this
        // board actually labels right. It used to reserve the margin
        // unconditionally, which pushed the board wider than the panel and
        // clipped it against the scroll box for no reason at all.
        const float BoardWidth = FieldWidth + (bAnyRightLabel ? LabelWidth + BreakerUI::Space16 : 0.0f);
        const float BoardHeight = TopPad + Tiers.Num() * TierHeight + 40.0f;

        TSharedRef<SCanvas> Canvas = SNew(SCanvas);

        // Tier gates: one dashed hairline across the field, labelled ONCE in
        // the 76px gutter, so a gate label can never land on node copy.
        for (int32 TierIndex = 0; TierIndex < Tiers.Num(); ++TierIndex)
        {
            const float TierTop = TopPad + TierIndex * TierHeight;
            int32 Gate = 0;
            bool bGateMet = true;
            for (int32 BranchIndex = 0; BranchIndex < Drawn.Num(); ++BranchIndex)
            {
                for (const UBreakerProgressionNode* Node : Drawn[BranchIndex]->Nodes)
                {
                    if (!Node || Node->Tier != Tiers[TierIndex]) continue;
                    Gate = FMath::Max(Gate, Node->RequiredTreeInvestment);
                    if (BranchSpent[BranchIndex] < Node->RequiredTreeInvestment) bGateMet = false;
                }
            }

            Canvas->AddSlot()
                .Position(FVector2D(GutterWidth, TierTop))
                .Size(FVector2D(FieldWidth - GutterWidth, 1.0f))
                [
                    DashedLine(FieldWidth - GutterWidth, BorderEmphasis)
                ];
            {
                // The tier's ENTRY REQUIREMENT, stated once for the whole row.
                //
                // This used to read "GATE 2" while every locked marker beneath
                // it read "GATE 0/2" — the same word for the tier's cost and
                // for one node's progress toward it, stacked vertically. The
                // gutter now says what OPENS the tier; the markers say only
                // what is true of themselves.
                TSharedRef<SVerticalBox> GutterLabel = SNew(SVerticalBox);
                GutterLabel->AddSlot().AutoHeight()
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("TIER %d"), Tiers[TierIndex])), BreakerUI::TypeCaption, Muted, true)
                ];
                if (Gate > 0)
                {
                    GutterLabel->AddSlot().AutoHeight()
                    [
                        MenuText(FText::FromString(bGateMet
                                ? FString(TEXT("OPEN"))
                                : FString::Printf(TEXT("OPENS AT %d"), Gate)),
                            BreakerUI::TypeCaption, bGateMet ? Cyan : Amber, true)
                    ];
                }
                Canvas->AddSlot()
                    .Position(FVector2D(0.0f, TierTop + BreakerUI::Space8))
                    .Size(FVector2D(GutterWidth - BreakerUI::Space8, 40.0f))
                    [
                        GutterLabel
                    ];
            }
        }

        float ColumnX = GutterWidth;
        for (int32 BranchIndex = 0; BranchIndex < Drawn.Num(); ++BranchIndex)
        {
            const UBreakerProgressionTree* Tree = Drawn[BranchIndex];
            const float TrunkX = ColumnX + ColumnWidth[BranchIndex] * 0.5f;
            const int32 Spent = BranchSpent[BranchIndex];

            for (int32 TierIndex = 0; TierIndex < Tiers.Num(); ++TierIndex)
            {
                const float TierTop = TopPad + TierIndex * TierHeight;

                TArray<const UBreakerProgressionNode*> TierNodes;
                for (const UBreakerProgressionNode* Node : Tree->Nodes)
                {
                    if (Node && Node->Tier == Tiers[TierIndex]) TierNodes.Add(Node);
                }

                bool bRouteOwned = false;
                for (const UBreakerProgressionNode* Node : TierNodes)
                {
                    if (ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) > 0) bRouteOwned = true;
                }

                // The trunk: one 2px line per branch running the full height,
                // cyan where the route is owned and panel/20 where it is not.
                AddCanvasSegment(Canvas, FVector2D(TrunkX, TierTop), FVector2D(TrunkX, TierTop + TierHeight),
                    bRouteOwned ? Cyan : PanelHover);

                for (int32 NodeIndex = 0; NodeIndex < TierNodes.Num(); ++NodeIndex)
                {
                    const UBreakerProgressionNode* Node = TierNodes[NodeIndex];
                    const ESkillMarkerKind Kind = ClassifyNode(Node);
                    const float NodeX = TrunkX + (NodeIndex - (TierNodes.Num() - 1) * 0.5f) * NodeSpacing;
                    const float NodeY = TierTop + 76.0f;

                    const int32 Rank = ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency);
                    FString LockReason;
                    FString ShortReason;
                    bool bTierGated = false;
                    const bool bPurchasable = SkillNodeIsPurchasable(Progression, Tree, Node, Spent, LockReason, &ShortReason, &bTierGated);
                    const bool bOwned = Rank > 0;
                    const bool bMaxed = Rank >= Node->MaxRank;

                    // The ring is measured before the marker, because the
                    // marker's edge length depends on it: a 2px ring eats 4px
                    // of the box the rank text has to live in, and a 1px ring
                    // eats 2. Two markers of the same kind on the same board
                    // can therefore be different sizes, which is correct — the
                    // spec's numbers are the floor, not the answer.
                    const float RingThickness = MarkerRingThickness(Kind, bOwned || bPurchasable);
                    // Multi-rank Minors carry their rank inside the marker;
                    // every other kind carries a filled core in its state
                    // colour, so no marker is ever an empty box.
                    const FString CentreLabel = (Kind == ESkillMarkerKind::Minor)
                        ? FString::Printf(TEXT("%d/%d"), Rank, Node->MaxRank)
                        : FString();
                    const float Size = MarkerSizeForLabel(MarkerBaseSize(Kind), RingThickness, CentreLabel);

                    // 2px diagonal dropping from the trunk to the marker.
                    AddCanvasSegment(Canvas, FVector2D(TrunkX, TierTop + BreakerUI::Space8),
                        FVector2D(NodeX, NodeY - Size * 0.5f), bOwned ? Cyan : PanelHover);

                    const FLinearColor Fill = (bOwned || bPurchasable) ? PanelHover : PanelRaised;
                    // Gold is the only border colour that means "spend now".
                    // Locked takes border/emphasis rather than border/rest:
                    // rest sits one value step off the plate face, which on a
                    // 44px shape against the board ground is invisible, and an
                    // invisible ring around a dark fill is what made the whole
                    // locked tier read as a row of holes.
                    const FLinearColor Ring = bOwned ? Cyan : (bPurchasable ? Amber : BorderEmphasis);
                    const FLinearColor CoreColor = bOwned ? Cyan : (bPurchasable ? Amber : Muted);

                    TSharedRef<SWidget> Inner = CentreLabel.IsEmpty()
                        ? MakeMarkerCore(Kind, CoreColor, Fill, Size)
                        : MakeMarkerLabel(CentreLabel, bOwned ? Cyan : Muted);

                    const FSkillNodeView View = MakeSkillNodeView(Node, Rank, bPurchasable, LockReason, Spent, Snapshot);
                    TSharedRef<SWidget> Marker = WireMarker(Tree, Node, View, bPurchasable, LockReason, Fill, Ring, RingThickness, Inner);
                    if (MarkerIsDiamond(Kind)) Marker = RotateFortyFive(Marker);

                    Canvas->AddSlot()
                        .Position(FVector2D(NodeX - Size * 0.5f, NodeY - Size * 0.5f))
                        .Size(FVector2D(Size, Size))
                        [
                            Marker
                        ];

                    // Name, number and state sit as plain text near the
                    // marker — not inside a card.
                    const FString NodeName = Node->DisplayName.IsEmpty() ? Node->NodeId.ToString().ToUpper() : Node->DisplayName.ToString().ToUpper();
                    // The SHORT reason on the board; the full sentence is on
                    // the rail. A wrapped four-line lock reason in a fixed
                    // label box is what used to overrun into the tier beneath.
                    //
                    // A node held ONLY by its tier's entry requirement prints
                    // nothing here. The gutter states that requirement once
                    // per tier; repeating it under every marker in the tier
                    // was noise that looked like information, and the muted
                    // marker already says "not yet". Node-specific reasons
                    // (a prerequisite by name, points you cannot afford) still
                    // print, because those ARE about this node.
                    // KEYSTONES ARE THE EXCEPTION to the tier-gate suppression
                    // above. Owner: "i dont see keystones in the skill trees".
                    // A tier-gated keystone printed the EMPTY string, so the
                    // single most important node on a branch was the only one
                    // that said nothing at all about itself — no name for what
                    // it is, no reason it is locked. The tier hairline argument
                    // does not apply here, because the reason is not the shared
                    // tier: it is commitment, which is a per-branch choice the
                    // player has to be told about somewhere on the board.
                    const bool bIsKeystone = (Kind == ESkillMarkerKind::Keystone);
                    const FString StateLine = bMaxed
                        ? FString(TEXT("MAXED"))
                        : (bPurchasable
                            ? FString::Printf(TEXT("%d PT -> RANK %d"), Node->CostPerRank, Rank + 1)
                            : (bOwned ? RankLabel(Rank, Node->MaxRank)
                                      : ((bTierGated && !bIsKeystone) ? FString() : ShortReason)));
                    const FLinearColor StateColor = bMaxed ? Cyan : (bPurchasable ? Amber : (bOwned ? Cyan : Muted));

                    TSharedRef<SVerticalBox> Label = SNew(SVerticalBox);
                    // The word itself, on the board. It existed only inside
                    // MakeSkillDetailCard, which is reachable ONLY by hovering
                    // the marker — so the board never once said "keystone" to a
                    // player who was looking at it rather than pointing at it.
                    // Amber even while locked, deliberately: this caption is
                    // what the eye is meant to find when scanning a branch, and
                    // muting it would restore the problem it exists to solve.
                    if (bIsKeystone)
                    {
                        Label->AddSlot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("KEYSTONE")))
                            .ColorAndOpacity(Amber)
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
                        ];
                    }
                    Label->AddSlot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(NodeName))
                        .ColorAndOpacity((bOwned || bPurchasable) ? Primary : Disabled)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
                        .AutoWrapText(true)
                    ];
                    Label->AddSlot().AutoHeight()
                    [
                        SNew(STextBlock)
                        // The effect as a number, so the board is readable
                        // without hovering anything.
                        .Text(FText::FromString(CompactEffectLine(Node)))
                        .ColorAndOpacity((bOwned || bPurchasable) ? SoftText : Disabled)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
                        .AutoWrapText(true)
                    ];
                    if (!StateLine.IsEmpty())
                    {
                        Label->AddSlot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(StateLine))
                            .ColorAndOpacity(StateColor)
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
                            .AutoWrapText(true)
                        ];
                    }

                    // Convergence and Keystone label to the RIGHT, so the
                    // trunk never runs through their text.
                    const bool bLabelRight = MarkerLabelsRight(Kind);
                    Canvas->AddSlot()
                        .Position(bLabelRight
                            ? FVector2D(NodeX + Size * 0.5f + BreakerUI::Space16, NodeY - LabelHeight * 0.5f)
                            : FVector2D(NodeX - LabelWidth * 0.5f, NodeY + Size * 0.5f + BreakerUI::Space8))
                        .Size(FVector2D(LabelWidth, LabelHeight))
                        [
                            Label
                        ];
                }
            }
            ColumnX += ColumnWidth[BranchIndex];
        }

        // 60px branch header strip above the path field. It rides INSIDE the
        // horizontal scroll with the board, so a scrolled column still knows
        // which branch it belongs to.
        TSharedRef<SHorizontalBox> BranchStrip = SNew(SHorizontalBox);
        BranchStrip->AddSlot().AutoWidth()
        [
            SNew(SBox).WidthOverride(GutterWidth)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))]
        ];
        for (int32 BranchIndex = 0; BranchIndex < Drawn.Num(); ++BranchIndex)
        {
            const UBreakerProgressionTree* Tree = Drawn[BranchIndex];
            const FString BranchName = TreeSelectorLabel(Tree);
            BranchStrip->AddSlot().AutoWidth()
            [
                SNew(SBox).WidthOverride(ColumnWidth[BranchIndex]).Padding(FMargin(0.0f, 0.0f, BreakerUI::Space8, 0.0f))
                [
                    MakePlate(
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(BranchName), BreakerUI::TypeH2, BranchSpent[BranchIndex] > 0 ? Primary : SoftText, true)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(FString::Printf(TEXT("%d / %d INVESTED"), BranchSpent[BranchIndex], BranchTotal[BranchIndex])),
                                BreakerUI::TypeCaption, Muted, true)
                        ],
                        BreakerUI::BgRaised, BranchSpent[BranchIndex] > 0 ? Cyan : BorderEmphasis,
                        FMargin(BreakerUI::Space16, BreakerUI::Space8))
                ]
            ];
        }

        // The board is ZOOMED AND PANNED, not scrolled. The two nested scroll
        // boxes that used to live here are gone: they fought each other for
        // the wheel, which is the most likely reading of "scrolling in the
        // tree is off by a little bit", and neither of them could make a board
        // wider than the panel legible — it could only be dragged past.
        //
        // The branch header strip rides INSIDE the transform with the columns
        // it labels, so a panned column still knows which branch it belongs
        // to, and it zooms with them rather than sliding out of register.
        const float StripHeight = 60.0f + BreakerUI::Space8;
        TSharedPtr<SBreakerBoardViewport> Viewport;
        SAssignNew(Viewport, SBreakerBoardViewport)
            .BoardSize(FVector2D(BoardWidth, BoardHeight + StripHeight))
            .InitialZoom(SkillBoardZoom > 0.0f ? SkillBoardZoom : BoardOpeningZoom)
            .InitialPan(SkillBoardPan)
            .OnViewChanged(FOnBoardViewChanged::CreateSP(this, &SBreakerMenu::HandleBoardViewChanged))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
                [
                    SNew(SBox).HeightOverride(60.0f)[BranchStrip]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBox).HeightOverride(BoardHeight)[Canvas]
                ]
            ];

        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                MakeBoardViewControls(Viewport)
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                Viewport.ToSharedRef()
            ];
    };

    // ---- Core board: the spatial constellation map -------------------------
    auto BuildCoreBoard = [&]() -> TSharedRef<SWidget>
    {
        if (CoreTrees.IsEmpty())
        {
            return MakeEmptyBoard(TEXT("[ NO CORE CONSTELLATIONS ]\n\nNo core-currency tree is registered yet."));
        }

        const UBreakerProgressionTree* CoreTree = CoreTrees[0];
        int32 TreeSpent = 0;
        int32 TreeTotal = 0;
        ProgressionTreeInvestment(Progression, CoreTree, TreeSpent, TreeTotal);

        // ---- Expanded constellation ------------------------------------
        // Owner: "the constellations dont expand like they should I dont see
        // any layers or details to them". The map was the WHOLE surface: seven
        // static plates, each a row of anonymous 36px chips carrying a bare
        // rank integer and one "N NODES · N PURCHASABLE" line. No node name, no
        // tier, no cost, no effect — nothing to read and nothing to open. There
        // was no expand code to be broken; it had never been built.
        //
        // Expanding reuses the class board's vocabulary rather than inventing a
        // second one: the same marker silhouettes, the same name / effect /
        // state label stack, banded by TIER so the depth the data always had is
        // finally visible. The map stays exactly one click away.
        if (!SkillExpandedConstellation.IsNone())
        {
            TArray<const UBreakerProgressionNode*> Members;
            FString ConstellationName = SkillExpandedConstellation.ToString().ToUpper();
            for (const UBreakerProgressionNode* Node : CoreTree->Nodes)
            {
                if (Node && Node->Constellation == SkillExpandedConstellation) Members.Add(Node);
            }
            Members.Sort([](const UBreakerProgressionNode& A, const UBreakerProgressionNode& B)
            {
                if (A.Tier != B.Tier) return A.Tier < B.Tier;
                return A.NodeId.LexicalLess(B.NodeId);
            });

            TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(220.0f)
                    [
                        MakeButton(FText::FromString(TEXT("< ALL CONSTELLATIONS")),
                            FOnClicked::CreateLambda([this]()
                            {
                                SkillExpandedConstellation = NAME_None;
                                Rebuild(EBreakerMenuScreen::SkillTrees);
                                return FReply::Handled();
                            }), false)
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(ConstellationName), BreakerUI::TypeH2, Primary, true)
                ]
            ];

            int32 LastTier = -1;
            for (const UBreakerProgressionNode* Node : Members)
            {
                if (Node->Tier != LastTier)
                {
                    LastTier = Node->Tier;
                    // The layer the flat map never showed. Tier is the axis the
                    // cluster data was already sorted by and never rendered.
                    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space8)
                    [
                        MenuText(FText::FromString(FString::Printf(TEXT("TIER %d"), LastTier)),
                            BreakerUI::TypeCaption, Muted, true)
                    ];
                }

                const int32 Rank = ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency);
                FString LockReason;
                const bool bPurchasable = SkillNodeIsPurchasable(Progression, CoreTree, Node, TreeSpent, LockReason);
                const bool bOwned = Rank > 0;
                const bool bMaxed = Rank >= Node->MaxRank;
                const ESkillMarkerKind Kind = ClassifyNode(Node);
                const FSkillNodeView View = MakeSkillNodeView(Node, Rank, bPurchasable, LockReason, TreeSpent, Snapshot);

                const FLinearColor Fill = (bOwned || bPurchasable) ? PanelHover : PanelRaised;
                const FLinearColor Ring = bOwned ? Cyan : (bPurchasable ? Amber : BorderEmphasis);
                TSharedRef<SWidget> Marker = WireMarker(CoreTree, Node, View, bPurchasable, LockReason, Fill, Ring,
                    MarkerRingThickness(Kind, bOwned || bPurchasable),
                    MakeMarkerCore(Kind, bOwned ? Cyan : (bPurchasable ? Amber : Muted), Fill, 44.0f));
                if (MarkerIsDiamond(Kind)) Marker = RotateFortyFive(Marker);

                const FString StateText = bMaxed
                    ? FString(TEXT("MAXED"))
                    : (bPurchasable ? FString::Printf(TEXT("%d PT -> RANK %d"), Node->CostPerRank, Rank + 1)
                                    : (bOwned ? RankLabel(Rank, Node->MaxRank) : LockReason));

                TSharedRef<SVerticalBox> Text = SNew(SVerticalBox);
                Text->AddSlot().AutoHeight()
                [
                    MenuText(FText::FromString(View.Name), BreakerUI::TypeBody,
                        (bOwned || bPurchasable) ? Primary : Disabled, true)
                ];
                Text->AddSlot().AutoHeight()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(CompactEffectLine(Node)))
                        .ColorAndOpacity((bOwned || bPurchasable) ? SoftText : Disabled)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
                        .AutoWrapText(true)
                ];
                if (!StateText.IsEmpty())
                {
                    Text->AddSlot().AutoHeight()
                    [
                        MenuText(FText::FromString(StateText), BreakerUI::TypeCaption,
                            bMaxed ? Cyan : (bPurchasable ? Amber : (bOwned ? Cyan : Muted)), true)
                    ];
                }

                Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Marker]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        .Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)[Text]
                ];
            }

            if (Members.Num() == 0)
            {
                Body->AddSlot().AutoHeight()
                [
                    MenuText(FText::FromString(TEXT("NO NODES AUTHORED IN THIS CONSTELLATION")),
                        BreakerUI::TypeCaption, Muted, true)
                ];
            }
            return MakePlate(Body, PanelRaised, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space16), false, BreakerUI::BorderRest);
        }

        struct FConstellationCluster
        {
            FString Name;
            FName Constellation;
            FVector2D Centre = FVector2D::ZeroVector;
            bool bHub = false;
            bool bSealed = false;
            TArray<const UBreakerProgressionNode*> Nodes;
        };

        TArray<FConstellationCluster> Clusters;
        auto AddCluster = [&Clusters](const TCHAR* Name, FName Constellation, float X, float Y, bool bHub, bool bSealed)
        {
            FConstellationCluster Cluster;
            Cluster.Name = FString(Name).ToUpper();
            Cluster.Constellation = Constellation;
            Cluster.Centre = FVector2D(X, Y);
            Cluster.bHub = bHub;
            Cluster.bSealed = bSealed;
            Clusters.Add(MoveTemp(Cluster));
        };
        // Kinesis is the hub; the other clusters sit around it. Elements is
        // sealed below centre; Velocity (the O27 addition the prefix map used
        // to sweep into UNMAPPED) takes the centre-north slot.
        AddCluster(TEXT("Kinesis"), TEXT("Kinesis"), 520.0f, 350.0f, true, false);
        AddCluster(TEXT("Precision"), TEXT("Precision"), 190.0f, 130.0f, false, false);
        AddCluster(TEXT("Volley"), TEXT("Volley"), 850.0f, 130.0f, false, false);
        AddCluster(TEXT("Velocity"), TEXT("Velocity"), 520.0f, 130.0f, false, false);
        AddCluster(TEXT("Affliction"), TEXT("Affliction"), 190.0f, 570.0f, false, false);
        AddCluster(TEXT("Bulwark"), TEXT("Bulwark"), 850.0f, 570.0f, false, false);
        AddCluster(TEXT("Elements"), TEXT("Elements"), 520.0f, 660.0f, false, true);

        // Constellation membership is the node's own Constellation field now
        // (populated for every Core node, asserted non-None by
        // RiorsEdge.Progression.CoreConstellationField) — the prefix
        // inference this replaced is what drew VELOCITY under an UNMAPPED
        // heading.
        TSet<FName> Claimed;
        for (FConstellationCluster& Cluster : Clusters)
        {
            for (const UBreakerProgressionNode* Node : CoreTree->Nodes)
            {
                if (!Node || Node->Constellation != Cluster.Constellation) continue;
                Cluster.Nodes.Add(Node);
                Claimed.Add(Node->NodeId);
            }
            Cluster.Nodes.Sort([](const UBreakerProgressionNode& A, const UBreakerProgressionNode& B) { return A.Tier < B.Tier; });
        }
        {
            // A node whose constellation names no cluster above must never
            // silently vanish off the map — it lands here, loudly.
            FConstellationCluster Other;
            Other.Name = TEXT("UNMAPPED");
            Other.Centre = FVector2D(190.0f, 350.0f);
            for (const UBreakerProgressionNode* Node : CoreTree->Nodes)
            {
                if (Node && !Claimed.Contains(Node->NodeId)) Other.Nodes.Add(Node);
            }
            if (Other.Nodes.Num() > 0) Clusters.Add(MoveTemp(Other));
        }

        const float BoardWidth = 1060.0f;
        // Grows with the plates. The plate positions are authored against a
        // fixed canvas, so making plates taller pushed the lowest one (ELEMENTS,
        // at the bottom of the map) through the board's own bottom edge — its
        // chip grid and node count were cut off, which was visible in capture.
        // +96 covers the OPEN CONSTELLATION control's 56 plus headroom for the
        // half-plate that hangs below the lowest authored centre. PlateHeight
        // itself is computed further down (it needs the cluster contents), so
        // this cannot read it directly.
        const float BoardHeight = 800.0f + 96.0f;
        const float PlateWidth = 300.0f;
        // A cluster's chips wrap at six per row inside a 300px plate, so the
        // plate has to be tall enough for however many rows that makes. The
        // old flat 156 assumed one row and a wide cluster's chips ran straight
        // out through the plate's right edge.
        const int32 ChipsPerRow = 6;
        // The chip is MEASURED, not guessed. It was 30 and clipped a `0` into
        // a bracket; the previous pass moved it to 36 and the clip went away
        // by luck rather than by arithmetic — the real defect was the text
        // being arranged at its own measured width and then overflow-clipped
        // (see MarkerSizeForLabel). One chip size serves the whole map, taken
        // from the widest rank string any chip on it will print, so a
        // constellation with a rank-10 node cannot reintroduce the bug.
        float ChipSize = 36.0f;
        for (const UBreakerProgressionNode* Node : CoreTree->Nodes)
        {
            if (!Node || Node->MaxRank <= 1) continue;
            ChipSize = FMath::Max(ChipSize, MarkerSizeForLabel(36.0f,
                BreakerUI::BorderSelected, FString::FromInt(Node->MaxRank)));
        }
        int32 WidestClusterRows = 1;
        for (const FConstellationCluster& Cluster : Clusters)
        {
            WidestClusterRows = FMath::Max(WidestClusterRows,
                FMath::DivideAndRoundUp(FMath::Max(1, Cluster.Nodes.Num()), ChipsPerRow));
        }
        // +56 for the OPEN CONSTELLATION control. A Canvas slot does not clip an
        // overflowing child, so getting this wrong overprints the plate below
        // rather than truncating visibly — the failure mode that is hardest to
        // recognise as one.
        const float PlateHeight = 108.0f + 56.0f + WidestClusterRows * (ChipSize + BreakerUI::Space8);

        TSharedRef<SCanvas> Canvas = SNew(SCanvas);

        auto ClusterHasOwned = [Progression](const FConstellationCluster& Cluster)
        {
            for (const UBreakerProgressionNode* Node : Cluster.Nodes)
            {
                if (ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) > 0) return true;
            }
            return false;
        };

        // Convergence lines radiate from the hub. Drawn first so the plates
        // paint over them.
        const bool bHubOwned = ClusterHasOwned(Clusters[0]);
        for (int32 Index = 1; Index < Clusters.Num(); ++Index)
        {
            const bool bLinked = bHubOwned && ClusterHasOwned(Clusters[Index]);
            AddCanvasSegment(Canvas, Clusters[0].Centre, Clusters[Index].Centre, bLinked ? Cyan : PanelHover);
        }

        for (const FConstellationCluster& Cluster : Clusters)
        {
            int32 ClusterPurchasable = 0;
            // Fixed six-per-row grid, built as rows of an SHorizontalBox. NOT
            // an SWrapBox: a wrap box sized off its allotted width is the
            // pattern that caused the historical layout oscillation, and the
            // row count here is arithmetic on a known chip count instead.
            TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
            TSharedPtr<SHorizontalBox> ChipRow;
            int32 ChipIndex = 0;
            for (const UBreakerProgressionNode* Node : Cluster.Nodes)
            {
                if (ChipIndex % ChipsPerRow == 0)
                {
                    ChipRow = SNew(SHorizontalBox);
                    Grid->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)[ChipRow.ToSharedRef()];
                }
                ++ChipIndex;
                const int32 Rank = ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency);
                FString LockReason;
                const bool bPurchasable = SkillNodeIsPurchasable(Progression, CoreTree, Node, TreeSpent, LockReason);
                if (bPurchasable) ++ClusterPurchasable;
                const bool bOwned = Rank > 0;
                const ESkillMarkerKind Kind = ClassifyNode(Node);

                const FLinearColor Fill = (bOwned || bPurchasable) ? PanelHover : PanelRaised;
                const FLinearColor Ring = bOwned ? Cyan : (bPurchasable ? Amber : BorderEmphasis);
                const float RingThickness = MarkerRingThickness(Kind, bOwned || bPurchasable);
                const FLinearColor CoreColor = bOwned ? Cyan : (bPurchasable ? Amber : Muted);
                const FSkillNodeView View = MakeSkillNodeView(Node, Rank, bPurchasable, LockReason, TreeSpent, Snapshot);

                // The cluster grid is a glance, not the path board: every kind
                // draws at one compact size here, keeping its silhouette AND
                // its centre mark. Without the mark a single-rank chip is an
                // empty black box, which is the same defect the path board's
                // keystone had — the map should not repeat it thirty times.
                TSharedRef<SWidget> Chip = WireMarker(CoreTree, Node, View, bPurchasable, LockReason, Fill, Ring, RingThickness,
                    Node->MaxRank > 1
                        ? MakeMarkerLabel(FString::FromInt(Rank), bOwned ? Cyan : Muted)
                        : MakeMarkerCore(Kind, CoreColor, Fill, ChipSize));
                if (MarkerIsDiamond(Kind)) Chip = RotateFortyFive(Chip);

                ChipRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
                [
                    SNew(SBox).WidthOverride(ChipSize).HeightOverride(ChipSize)[Chip]
                ];
            }

            // These both returned suppression teal for the sealed cluster, on the
            // reasoning that a rift is a world object. The reasoning is the
            // adjectival use the teal law forbids: a rail and a border are chrome
            // describing a panel, and both are named on the forbidden list
            // whatever the panel is about. The word SEALED below is the carrier,
            // and it was already there — the tint was saying nothing twice.
            const FLinearColor Rail = BreakerInventoryLayout::SkillClusterRailColour(Cluster.bSealed, Cluster.bHub);
            const FLinearColor Border = BreakerInventoryLayout::SkillClusterBorderColour(Cluster.bSealed);

            TSharedRef<SVerticalBox> Inner = SNew(SVerticalBox);
            Inner->AddSlot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Cluster.Name), BreakerUI::TypeH2,
                        Cluster.bSealed ? BreakerUI::TealHardware : Primary, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Cluster.bHub ? TEXT("HUB") : TEXT("")), BreakerUI::TypeCaption, Cyan, true)
                ]
            ];
            // SEALED IS A PROPERTY OF THE CONSTELLATION, NOT OF ITS EMPTINESS.
            // Both sealed lines used to live inside the `Num() == 0` branch, and
            // Elements has six authored nodes — so the plate rendered teal, with
            // the teal reserved for sealed hardware, and never once said the
            // word SEALED or named Rift / Entropy / Void. It read as an ordinary
            // constellation coloured differently for no stated reason. O38 makes
            // Elements post-slice, so being sealed is exactly what a player most
            // needs told about it.
            if (Cluster.bSealed)
            {
                Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(TEXT("SEALED")), BreakerUI::TypeCaption,
                        BreakerInventoryLayout::SkillClusterSealedLabelColour(), true)
                ];
                Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(TEXT("RIFT / ENTROPY / VOID")), BreakerUI::TypeCaption, Muted, true)
                ];
            }
            if (Cluster.Nodes.Num() == 0)
            {
                if (!Cluster.bSealed)
                {
                    Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(TEXT("NO NODES AUTHORED")), BreakerUI::TypeCaption, Muted, true)
                    ];
                }
            }
            else
            {
                Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, BreakerUI::Space8)[Grid];
                Inner->AddSlot().AutoHeight()
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("%d NODES · %d PURCHASABLE"), Cluster.Nodes.Num(), ClusterPurchasable)),
                        BreakerUI::TypeCaption, ClusterPurchasable > 0 ? Amber : Muted, true)
                ];
                // The affordance, stated. A plate that opens has to say so —
                // the chips are individually clickable to BUY, so nothing about
                // the plate previously suggested it was itself a way in.
                const FName ClusterId = Cluster.Constellation;
                Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                [
                    SNew(SBox).HeightOverride(BreakerUI::MinHitTarget)
                    [
                        MakeButton(FText::FromString(TEXT("OPEN CONSTELLATION")),
                            FOnClicked::CreateLambda([this, ClusterId]()
                            {
                                SkillExpandedConstellation = ClusterId;
                                Rebuild(EBreakerMenuScreen::SkillTrees);
                                return FReply::Handled();
                            }), false)
                    ]
                ];
            }

            Canvas->AddSlot()
                .Position(FVector2D(Cluster.Centre.X - PlateWidth * 0.5f, Cluster.Centre.Y - PlateHeight * 0.5f))
                .Size(FVector2D(PlateWidth, PlateHeight))
                [
                    MakePlate(Inner, PanelRaised, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space8), false, Border)
                ];
        }

        // Same viewport as the class board — one way for a board to move, on
        // both boards. The constellation map is 1060x800 and the panel is not,
        // on most windows.
        TSharedPtr<SBreakerBoardViewport> Viewport;
        SAssignNew(Viewport, SBreakerBoardViewport)
            .BoardSize(FVector2D(BoardWidth, BoardHeight))
            .InitialZoom(SkillBoardZoom > 0.0f ? SkillBoardZoom : BoardOpeningZoom)
            .InitialPan(SkillBoardPan)
            .OnViewChanged(FOnBoardViewChanged::CreateSP(this, &SBreakerMenu::HandleBoardViewChanged))
            [
                SNew(SBox).WidthOverride(BoardWidth).HeightOverride(BoardHeight)[Canvas]
            ];

        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                SNew(SBox).HeightOverride(60.0f)
                [
                    MakePlate(
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(TEXT("CORE CONSTELLATIONS — KINESIS AT THE HUB")), BreakerUI::TypeH2, Primary, true)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(FString::Printf(TEXT("%d / %d INVESTED"), TreeSpent, TreeTotal)), BreakerUI::TypeCaption, Muted, true)
                        ],
                        BreakerUI::BgRaised, Amber, FMargin(BreakerUI::Space16, BreakerUI::Space8))
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                MakeBoardViewControls(Viewport)
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                Viewport.ToSharedRef()
            ];
    };

    TSharedRef<SWidget> Board = Trees.IsEmpty()
        ? MakeEmptyBoard(TEXT("[ NO TREE CONTENT ]\n\nThe progression component is not serving any trees yet.\nThe class branches and the core constellations appear\nhere once tree content is registered."))
        : (bCoreBoard ? BuildCoreBoard() : BuildClassBoard());

    // ---- Branch selector ---------------------------------------------------
    //
    // "There should be a button to select your subclass and swap between to
    // the others which shows the different trees and what they do."
    //
    // O37 ruled commitment into the data model (FBreakerProgressionState::
    // CommittedBranch, one-way CommitToBranch, Forge respec clears), and it
    // EMPOWERS rather than excludes: committing unlocks the branch's
    // cornerstone keystone tier; every ordinary node of every branch stays
    // freely purchasable (O15 intact). The strip is still the browsing view
    // the owner asked for — the COMMIT control below is two-step (arm, then
    // confirm) because the choice is permanent outside the Forge.
    TSharedRef<SHorizontalBox> BranchChips = SNew(SHorizontalBox);
    if (!bCoreBoard && ClassTrees.Num() > 0)
    {
        auto AddBranchChip = [this, &BranchChips](const FString& Label, const FString& Sub, int32 Index, bool bActive)
        {
            BranchChips->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                BorderWrap(
                    SNew(SButton)
                    .ButtonColorAndOpacity(bActive ? PanelHover : Panel)
                    .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                    .OnClicked(FOnClicked::CreateLambda([this, Index]()
                    {
                        SkillBranchIndex = Index;
                        SkillTreeStatus = FText::GetEmpty();
                        PendingCommitBranch = NAME_None; // switching views disarms O37 commit
                        ResetBoardView();
                        Rebuild(EBreakerMenuScreen::SkillTrees);
                        return FReply::Handled();
                    }))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bActive ? Primary : SoftText, true)
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            MenuText(FText::FromString(Sub), BreakerUI::TypeCaption, bActive ? Cyan : Muted, false)
                        ]
                    ],
                    bActive ? Cyan : BorderEmphasis,
                    bActive ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
            ];
        };

        for (int32 Index = 0; Index < ClassTrees.Num(); ++Index)
        {
            int32 Spent = 0;
            int32 Total = 0;
            ProgressionTreeInvestment(Progression, ClassTrees[Index], Spent, Total);
            AddBranchChip(TreeSelectorLabel(ClassTrees[Index]),
                FString::Printf(TEXT("%d / %d INVESTED"), Spent, Total),
                Index, SkillBranchIndex == Index);
        }
        AddBranchChip(TEXT("COMPARE ALL"), FString::Printf(TEXT("%d BRANCHES"), ClassTrees.Num()), -1, SkillBranchIndex == -1);

        // ---- O37 commit control (two-step: arm, then confirm) --------------
        if (Progression && ClassTrees.IsValidIndex(SkillBranchIndex))
        {
            const UBreakerProgressionTree* SelectedBranch = ClassTrees[SkillBranchIndex];
            const FName Committed = Progression->GetProgressionState().CommittedBranch;
            if (Committed != NAME_None)
            {
                const bool bThisBranch = SelectedBranch && Committed == SelectedBranch->TreeId;
                BranchChips->AddSlot().AutoWidth().Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
                [
                    BorderWrap(
                        SNew(SBox).Padding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                        [
                            // "KEYSTONE TIER OPEN" WAS FALSE FOR EVERY CHARACTER
                            // BELOW THE LEVEL CAP. Commitment clears the O37
                            // check and nothing else; the keystone still wants
                            // six invested plus two, and the benchmark schedule
                            // does not produce eight points until the last one.
                            // O86 splits what commitment delivers: the identity
                            // arrives now, free and immediate, and the mechanics
                            // unfold on the benchmarks. The chip says the half
                            // that is true at the moment it is shown.
                            MenuText(FText::FromString(bThisBranch
                                ? TEXT("COMMITTED — THIS DOCTRINE IS YOURS")
                                : FString::Printf(TEXT("COMMITTED ELSEWHERE: %s"), *Committed.ToString())),
                                BreakerUI::TypeCaption, bThisBranch ? Cyan : Muted, true)
                        ],
                        bThisBranch ? Cyan : BorderEmphasis, BreakerUI::BorderThin)
                ];
            }
            else if (SelectedBranch)
            {
                const bool bArmed = PendingCommitBranch == SelectedBranch->TreeId;
                BranchChips->AddSlot().AutoWidth().Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
                [
                    BorderWrap(
                        SNew(SButton)
                        .ButtonColorAndOpacity(bArmed ? PanelHover : Panel)
                        .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                        .OnClicked(FOnClicked::CreateLambda([this, SelectedBranch]()
                        {
                            if (PendingCommitBranch != SelectedBranch->TreeId)
                            {
                                PendingCommitBranch = SelectedBranch->TreeId;
                                SkillTreeStatus = FText::FromString(TEXT("Committing is permanent outside the Forge. Click CONFIRM to commit."));
                            }
                            else
                            {
                                FText Failure;
                                UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                                if (Prog && Prog->CommitToBranch(SelectedBranch->TreeId, Failure))
                                {
                                    SkillTreeStatus = FText::FromString(TEXT("Committed. The branch keystone tier is now open."));
                                }
                                else
                                {
                                    SkillTreeStatus = Failure;
                                }
                                PendingCommitBranch = NAME_None;
                            }
                            Rebuild(EBreakerMenuScreen::SkillTrees);
                            return FReply::Handled();
                        }))
                        [
                            MenuText(FText::FromString(bArmed
                                ? TEXT("CONFIRM COMMIT — PERMANENT")
                                : TEXT("COMMIT TO THIS BRANCH")),
                                BreakerUI::TypeCaption, bArmed ? Primary : SoftText, true)
                        ],
                        bArmed ? Cyan : BorderEmphasis,
                        bArmed ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
                ];
            }
        }
    }

    // ---- Header zone -------------------------------------------------------
    const EBreakerClassId PermanentClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    const FString MetaLine = FString::Printf(TEXT("BREAKER · %s · LV %d"),
        *ClassDisplayName(PermanentClass),
        Progression ? Progression->GetProgressionState().CharacterLevel : 1);

    // Below this width the header band's controls stop fitting on one 88px
    // row, so the labels compact rather than running off the plate. Sampled
    // from the viewport, once, like every other number on this screen.
    // 1900, not 1500. Measured by capture at a 1920 viewport (PanelWidth 1840,
    // comfortably above the old threshold) where RESPEC was still drawn off the
    // right edge as "RESPEC C". The header packs a title, four screen tabs, two
    // board tabs, two point chips, DEV, RESPEC and BACK into one row of
    // AutoWidth slots, and an SHorizontalBox does not shrink an oversized
    // AutoWidth child — it draws it straight through the panel edge. The old
    // threshold was set by reasoning; this one was set by looking.
    const bool bCompactHeader = Metrics.PanelWidth < 1900.0f;

    TSharedRef<SHorizontalBox> BoardTabs = SNew(SHorizontalBox);
    auto AddBoardTab = [this, &BoardTabs, bCoreBoard](const FString& Label, int32 TabIndex)
    {
        const bool bActive = (bCoreBoard ? 1 : 0) == TabIndex;
        BoardTabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bActive ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, TabIndex]()
                {
                    SkillBoardTab = TabIndex;
                    SkillTreeStatus = FText::GetEmpty();
                    ResetBoardView();
                    Rebuild(EBreakerMenuScreen::SkillTrees);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bActive ? Primary : Muted, true)
                ],
                bActive ? Cyan : BorderEmphasis,
                bActive ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    };
    AddBoardTab(bCompactHeader ? TEXT("CLASS") : FString::Printf(TEXT("CLASS · %s"), *ClassDisplayName(PermanentClass)), 0);
    AddBoardTab(TEXT("CORE"), 1);

    // Both numbers, each one NAMED.
    //
    // The counter used to stack a large unspent count over a small "/ N SPENT",
    // which at real values read as "108 / 32" — a fraction, and a nonsensical
    // one, since unspent is not out of spent. The big number now carries the
    // word UNSPENT on its own baseline and the spent count is a separate
    // labelled line, so no reading of the two as one ratio survives.
    auto MakePointChip = [bCompactHeader](const FString& Label, int32 Unspent, int32 Spent, const FLinearColor& Rail) -> TSharedRef<SWidget>
    {
        TSharedRef<SWidget> Headline =
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
            [
                MenuText(FText::FromString(FString::FromInt(Unspent)), BreakerUI::TypeH2, Rail, true)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(BreakerUI::Space4, 0.0f, 0.0f, 0.0f)
            [
                MenuText(FText::FromString(TEXT("UNSPENT")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];

        if (bCompactHeader)
        {
            // Two lines instead of three. Every word still present; the plate
            // just stops claiming a header row that has none left.
            return MakePlate(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Headline]
                + SVerticalBox::Slot().AutoHeight()
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("%s · %d SPENT"), *Label.Replace(TEXT(" POINTS"), TEXT("")), Spent)),
                        BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
                ],
                BreakerUI::Panel10, Rail, FMargin(BreakerUI::Space8, BreakerUI::Space4));
        }
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(Label), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)]
            + SVerticalBox::Slot().AutoHeight()[Headline]
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(FString::Printf(TEXT("%d SPENT"), Spent)), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)],
            BreakerUI::Panel10, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space4));
    };

    // Dev-only recovery row: saves made before the slice seeding rule relaxed
    // can land here with a class chosen and zero points. Same
    // RiorsEdge.Playtest/DevClassSwap gate the class screen uses.
    bool bDevTools = false;
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), bDevTools, GGameUserSettingsIni);

    const EBreakerPointCurrency BoardCurrency = bCoreBoard ? EBreakerPointCurrency::CorePoints : EBreakerPointCurrency::DoctrinePoints;

    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[BuildScreenTabs(EBreakerMenuScreen::SkillTrees)];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)[BoardTabs];
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    // Two counters as separate railed chips — class cyan, core gold — so the
    // two currencies are never read as one pool.
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
    [
        MakePointChip(TEXT("CLASS POINTS"), UnspentClass, ClassSpent, Cyan)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
    [
        MakePointChip(TEXT("CORE POINTS"), UnspentCore, CoreSpent, Amber)
    ];
    if (bDevTools)
    {
        HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this]()
                {
                    UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                    if (Prog)
                    {
                        // O2 PLACEHOLDER: same 10 Class / 12 Core slice budget
                        // ApplySliceDefaultsIfFresh seeds.
                        Prog->GrantPlaytestPoints(10, 12);
                        SkillTreeStatus = FText::FromString(TEXT("DEV: GRANTED 10 CLASS / 12 CORE"));
                        if (Character.IsValid()) Character->SaveGameState();
                    }
                    else
                    {
                        SkillTreeStatus = FText::FromString(TEXT("DEV: NO PROGRESSION COMPONENT"));
                    }
                    Rebuild(EBreakerMenuScreen::SkillTrees);
                    return FReply::Handled();
                }))
                [
                    // Shortened when the header is compact. The header is one
                    // row of AutoWidth slots that an SHorizontalBox will not
                    // shrink, so the only way to keep BACK on screen is to
                    // spend fewer pixels earlier in the row — and a dev-only
                    // control is the right place to spend them.
                    MenuText(FText::FromString(bCompactHeader ? TEXT("DEV: POINTS") : TEXT("DEV: GRANT POINTS")),
                        BreakerUI::TypeCaption, Amber, true)
                ],
                Amber)
        ];
    }
    // Respec is per-tree and destructive: discard styling, and the label
    // states which tree it will clear.
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this, BoardCurrency]()
            {
                UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                FText FailureReason;
                if (ProgressionRespec(Prog, BoardCurrency, bAtForge, FailureReason))
                {
                    SkillTreeStatus = FText::FromString(FString::Printf(TEXT("%s POINTS REFUNDED"), *CurrencyLabel(BoardCurrency)));
                    if (Character.IsValid()) Character->SaveGameState();
                }
                else
                {
                    SkillTreeStatus = FailureReason.IsEmpty() ? FText::FromString(TEXT("RESPEC FAILED")) : FailureReason;
                }
                Rebuild(EBreakerMenuScreen::SkillTrees);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(bCompactHeader
                    ? FString(TEXT("RESPEC"))
                    : FString::Printf(TEXT("RESPEC %s"), *CurrencyLabel(BoardCurrency))), BreakerUI::TypeCaption, Harm, true)
            ],
            HarmDeep)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(bCompactHeader ? 88.0f : 120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    // ---- Body: board plus the fixed 420px detail rail ----------------------
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    if (!SkillTreeStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(SkillTreeStatus, BreakerUI::TypeCaption, Cyan, true)
        ];
    }
    // Board column: the branch selector sits above the board, outside its
    // scroll, so swapping branches never requires scrolling to find the chips.
    TSharedRef<SVerticalBox> BoardColumn = SNew(SVerticalBox);
    if (!bCoreBoard && ClassTrees.Num() > 0)
    {
        BoardColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MakePlate(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(TEXT("BRANCH")), BreakerUI::TypeCaption, Muted, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
                [
                    BranchChips
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).HAlign(HAlign_Right)
                [
                    // Said out loud, because the screen must not imply a
                    // commitment the save has no field for. Clipped for the
                    // same reason the footer is: a class with more branches
                    // than Swift's three must push this note off rather than
                    // print it through the chips.
                    SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)
                    .HAlign(HAlign_Right)
                    [
                        MenuText(FText::FromString(TEXT("BROWSING — NO SUBCLASS IS COMMITTED")), BreakerUI::TypeCaption, Muted, true)
                    ]
                ],
                BreakerUI::BgRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space8))
        ];
    }
    BoardColumn->AddSlot().FillHeight(1.0f)[Board];

    // Rail column: the build summary pinned at the top, the hover detail
    // beneath it in its own scroll so a long node card can never push the
    // totals off the plate. The column's width is fixed by this box, so
    // populating the detail cannot reflow the board.
    TSharedRef<SWidget> RailColumn = SNew(SBox).WidthOverride(Metrics.RailWidth)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MakeBuildTotalsPlate(Snapshot, ClassSpent, CoreSpent)
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SkillDetailHost.ToSharedRef()
            ]
        ]
    ];

    Body->AddSlot().FillHeight(1.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f)[BoardColumn]
        + SHorizontalBox::Slot().AutoWidth().Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
        [
            RailColumn
        ]
    ];

    // ---- Footer: input legend plus the live purchasable count --------------
    TSharedRef<SBox> Footer = SNew(SBox).HeightOverride(56.0f)
    [
        MakePlate(
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space24, 0.0f)
            [
                // Clipped, not merely filled. An SHorizontalBox does not
                // shrink an oversized child to its slot — it draws it at full
                // width straight through whatever shares the row, which is
                // exactly how the count and the legend came to overprint each
                // other. The legend is also cut back to the bindings a player
                // cannot guess; the rest was documentation, not a legend.
                // O2: node numbers are not balanced yet.
                SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)
                [
                    MenuText(FText::FromString(TEXT("LMB BUY 1 RANK · SHIFT+LMB BUY TO MAX · HOVER FOR BEFORE / AFTER · WHEEL ZOOM · DRAG PAN · ESC BACK")),
                        BreakerUI::TypeCaption, Muted, true)
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                // Scoped to the board and LABELLED with its scope. The count
                // is only useful if the player can look up and find that many
                // gold nodes.
                MenuText(FText::FromString(FString::Printf(TEXT("%d PURCHASABLE %s"),
                        PurchasableCount, bCountSpansBoard ? TEXT("ON THIS BOARD") : TEXT("ON THIS BRANCH"))),
                    BreakerUI::TypeH2, PurchasableCount > 0 ? Amber : Muted, true)
            ],
            BreakerUI::BgRaised, Amber, FMargin(BreakerUI::Space24, BreakerUI::Space8))
    ];

    return BuildZonedFrame(
        FText::FromString(TEXT("SKILL MATRIX")),
        FText::FromString(MetaLine),
        HeaderRight,
        Body,
        Footer,
        Metrics.PanelWidth,
        Metrics.PanelHeight,
        /*bFillHeight=*/true);
}

namespace
{
    FString DescribeForgeCost(const FBreakerForgeCost& Cost)
    {
        // The ONE currency's name comes from BreakerForge::CurrencyDisplayName
        // (Items/BreakerForgeLibrary.h) so a cost line, the wallet chip and
        // the salvage preview can never disagree about what it is called.
        return Cost.IsFree() ? FString(TEXT("FREE")) : FString::Printf(TEXT("%d %s"), Cost.Amount, BreakerForge::CurrencyDisplayName);
    }

    // EBreakerForgeResult (Items/BreakerForgeLibrary.h) carries no FText, unlike
    // the ability selection API's DescribeSelectionResult — this is that
    // mapping's Forge-side twin, kept in one place so the tab's copy cannot
    // drift per button.
    FString DescribeForgeResult(EBreakerForgeResult Result)
    {
        switch (Result)
        {
            case EBreakerForgeResult::Success:      return TEXT("DONE.");
            case EBreakerForgeResult::NotAtForge:    return TEXT("NOT AT A FORGE.");
            case EBreakerForgeResult::InvalidItem:   return TEXT("ITEM NOT FOUND.");
            case EBreakerForgeResult::InvalidAffix:  return TEXT("NO SUCH AFFIX LINE.");
            case EBreakerForgeResult::AtTierCeiling: return TEXT("ALREADY AT ITS TIER CEILING.");
            case EBreakerForgeResult::Unaffordable:  return TEXT("CANNOT AFFORD THE COST.");
            default:                                 return TEXT("UNKNOWN RESULT.");
        }
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildForgeScreen()
{
    // Reach (Decisions.md O37/O40c): Items/BreakerForgeLibrary.h's three verbs
    // plus salvage and the wallet had zero UI callers — CONTEXT.md called this
    // "the biggest built-but-unreachable item in the project." Utilitarian
    // FIELDPLATE per the brief: plates, rails, the existing type scale: no new
    // tokens invented for this tab.
    UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;
    const FWideScreenMetrics Metrics = MeasureWideScreen();

    const FBreakerForgeWallet EmptyWallet;
    const FBreakerForgeWallet& Wallet = Equipment ? Equipment->GetForgeWallet() : EmptyWallet;

    // Resolve the selection by walking both containers fresh every rebuild —
    // no separate cache, so an id whose item was just salvaged, discarded, or
    // consumed by a previous Forge op simply resolves to "nothing selected"
    // on the very next rebuild instead of needing an explicit clear anywhere.
    FBreakerItemInstance SelectedItem;
    bool bSelectedFound = false;
    bool bSelectedEquipped = false;
    if (Equipment && ForgeSelectedItemId.IsValid())
    {
        for (const FBreakerItemInstance& HeldItem : Equipment->GetEquipped())
        {
            if (HeldItem.IsValid() && HeldItem.ItemId == ForgeSelectedItemId)
            {
                SelectedItem = HeldItem; bSelectedFound = true; bSelectedEquipped = true; break;
            }
        }
        if (!bSelectedFound)
        {
            for (const FBreakerItemInstance& HeldItem : Equipment->GetBackpack())
            {
                if (HeldItem.ItemId == ForgeSelectedItemId) { SelectedItem = HeldItem; bSelectedFound = true; break; }
            }
        }
    }

    // ---- Header: tab strip, wallet, BACK -----------------------------------
    // The ONE wallet chip: the currency's name over its amount. Imperative
    // rebuild like everything else on this screen — the amount is re-read on
    // the next rebuild, never a per-frame attribute. AutoWidth, so the chip
    // always fits its widest content (the RIFTGLASS caption).
    auto MakeCurrencyChip = [](int32 Amount) -> TSharedRef<SWidget>
    {
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(BreakerForge::CurrencyDisplayName), BreakerUI::TypeCaption, Muted, true)]
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(BreakerUI::FormatTicker(static_cast<float>(Amount))), BreakerUI::TypeH2, Primary, true)],
            PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space4));
    };
    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[BuildScreenTabs(EBreakerMenuScreen::Forge)];
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)[MakeCurrencyChip(Wallet.Get())];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        SNew(SBox).WidthOverride(120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    // ---- Left: pick a held item (equipped or backpack) ---------------------
    auto MakeSelectRow = [this](const FBreakerItemInstance& Item) -> TSharedRef<SWidget>
    {
        const bool bRowSelected = ForgeSelectedItemId == Item.ItemId;
        const FGuid CapturedId = Item.ItemId;
        return SNew(SBox).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            MakeRarityCard(
                SNew(SButton)
                .ButtonColorAndOpacity(bRowSelected ? PanelHover : PanelRaised)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, CapturedId]()
                {
                    ForgeSelectedItemId = CapturedId;
                    ForgeStatus = FText::GetEmpty();
                    Rebuild(EBreakerMenuScreen::Forge);
                    return FReply::Handled();
                }))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
                    [
                        MenuText(FText::FromString(RarityName(Item.Rarity)), BreakerUI::TypeCaption, RarityColor(Item.Rarity), true)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(ItemSlotLabel(Item)), BreakerUI::TypeCaption, bRowSelected ? Primary : Muted, true)]
                    + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(FString::Printf(TEXT("i%d"), Item.ItemLevel)), BreakerUI::TypeCaption, SoftText, true)]
                ],
                Item.Rarity, true)
        ];
    };

    TSharedRef<SVerticalBox> PickerList = SNew(SVerticalBox);
    PickerList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[MenuText(FText::FromString(TEXT("EQUIPPED")), BreakerUI::TypeCaption, Muted, true)];
    static const EBreakerEquipSlot ForgeWearOrder[] =
    {
        EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist,
        EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary,
    };
    bool bAnyEquipped = false;
    for (const EBreakerEquipSlot Slot : ForgeWearOrder)
    {
        FBreakerItemInstance Item;
        if (Equipment && Equipment->GetEquippedItem(Slot, Item))
        {
            bAnyEquipped = true;
            PickerList->AddSlot().AutoHeight()[MakeSelectRow(Item)];
        }
    }
    if (!bAnyEquipped) PickerList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[MenuText(FText::FromString(TEXT("NOTHING EQUIPPED")), BreakerUI::TypeCaption, Disabled)];

    PickerList->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space8)[MenuText(FText::FromString(TEXT("BACKPACK")), BreakerUI::TypeCaption, Muted, true)];
    TArray<FBreakerItemInstance> BackpackItems = Equipment ? Equipment->GetBackpack() : TArray<FBreakerItemInstance>();
    Algo::Reverse(BackpackItems);
    BackpackItems.StableSort([](const FBreakerItemInstance& A, const FBreakerItemInstance& B)
    {
        return static_cast<uint8>(A.Rarity) > static_cast<uint8>(B.Rarity);
    });
    if (BackpackItems.IsEmpty()) PickerList->AddSlot().AutoHeight()[MenuText(FText::FromString(TEXT("BACKPACK EMPTY")), BreakerUI::TypeCaption, Disabled)];
    for (const FBreakerItemInstance& Item : BackpackItems)
    {
        PickerList->AddSlot().AutoHeight()[MakeSelectRow(Item)];
    }

    // ---- Right: the selected item's Forge operations -----------------------
    TSharedRef<SVerticalBox> OpsColumn = SNew(SVerticalBox);
    if (!bSelectedFound)
    {
        OpsColumn->AddSlot().AutoHeight()
        [
            MakePlate(
                MenuText(FText::FromString(TEXT("SELECT A HELD ITEM ON THE LEFT TO SALVAGE OR CRAFT IT.")), BreakerUI::TypeCaption, Muted),
                PanelRaised, BorderEmphasis, FMargin(BreakerUI::Space16, BreakerUI::Space16))
        ];
    }
    else
    {
        TSharedRef<SVerticalBox> ForgePanel = SNew(SVerticalBox);
        ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                MenuText(FText::FromString(FString::Printf(TEXT("%s %s"), *RarityName(SelectedItem.Rarity), *ItemSlotLabel(SelectedItem))), BreakerUI::TypeH2, RarityColor(SelectedItem.Rarity), true)
            ]
            + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(FString::Printf(TEXT("i%d"), SelectedItem.ItemLevel)), BreakerUI::TypeCaption, Primary, true)]
        ];
        ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
        [
            MenuText(FText::FromString(bSelectedEquipped ? TEXT("CURRENTLY EQUIPPED") : TEXT("IN BACKPACK")), BreakerUI::TypeCaption, Muted, true)
        ];

        // ---- IF EQUIPPED: the composed delta, per lane ---------------------
        // This is what the printed gear score was reaching for, and the reason
        // a score could not do it: damage is partitioned by DELIVERY (O54), so
        // one number has to pick a lane and is then wrong for whoever built the
        // other one. The two lanes measure 0.647x of each other at the cap.
        //
        // Only for a piece NOT already worn — the delta against itself is zero,
        // and five rows of nothing is worse than no rows.
        //
        // VERTICAL furniture on purpose. Every clipped-text report on this file
        // has been horizontal overflow against the card width solve, and this
        // adds no column: it is stacked rows in a panel that already scrolls.
        UBreakerProgressionComponent* EquipProgression =
            Character.IsValid() ? Character->GetProgression() : nullptr;
        const UBreakerAttributeSet* EquipAttributes =
            Character.IsValid() ? Character->GetAttributes() : nullptr;
        if (!bSelectedEquipped && EquipProgression)
        {
            const FBreakerSkillSnapshot EquipSnapshot =
                BreakerSkillProjection::MakeSnapshot(EquipProgression, EquipAttributes);
            const TArray<FBreakerItemInstance> Worn =
                Equipment ? Equipment->GetEquipped() : TArray<FBreakerItemInstance>();
            ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
            [
                MenuText(FText::FromString(TEXT("IF EQUIPPED")), BreakerUI::TypeCaption, Cyan, true)
            ];
            for (const FBreakerStatLine& Line : BreakerSkillProjection::ProjectEquip(EquipSnapshot, Worn, SelectedItem))
            {
                // The same three colours the per-affix delta marks already use,
                // so one screen does not teach two vocabularies for "better".
                const FLinearColor RowColor = !Line.Changed() ? Muted
                    : (Line.After > Line.Before ? Cyan : Harm);
                ForgePanel->AddSlot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        MenuText(FText::FromString(Line.Label), BreakerUI::TypeCaption, Muted, true)
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        MenuText(FText::FromString(Line.Changed()
                            ? BreakerSkillProjection::FormatTransition(Line)
                            : BreakerSkillProjection::FormatStat(Line.Before, Line.Format)),
                            BreakerUI::TypeCaption, RowColor, true)
                    ]
                ];
            }
            ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
            [
                MenuText(FText::FromString(TEXT("EFFECTIVE HEALTH EXCLUDES BLOCK AND DODGE — BOTH ARE CHANCE")),
                    BreakerUI::TypeCaption, Disabled, true)
            ];
        }

        // ---- TEMPER: one row per affix line --------------------------------
        ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            MenuText(FText::FromString(TEXT("TEMPER — ONE AFFIX, ONE TIER BETTER")), BreakerUI::TypeCaption, Cyan, true)
        ];
        if (SelectedItem.Affixes.IsEmpty())
        {
            ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[MenuText(FText::FromString(TEXT("NO AFFIXES TO TEMPER.")), BreakerUI::TypeCaption, Disabled)];
        }
        for (int32 AffixIndex = 0; AffixIndex < SelectedItem.Affixes.Num(); ++AffixIndex)
        {
            const FBreakerRolledAffix& Affix = SelectedItem.Affixes[AffixIndex];
            const int32 TargetTier = Affix.Tier - 1;
            const int32 Ceiling = UBreakerForgeLibrary::TemperCeilingForItem(SelectedItem);
            // TemperCost returns a deceptive zero — indistinguishable from
            // free — both for a bad index AND for a line already at its tier
            // ceiling, so the ceiling has to be checked independently rather
            // than trusted from Cost.IsFree() alone.
            const bool bAtCeiling = TargetTier < Ceiling;
            const FBreakerForgeCost CostTemper = UBreakerForgeLibrary::TemperCost(SelectedItem, AffixIndex);
            const bool bAffordableTemper = Wallet.CanAfford(CostTemper);
            const FGuid CapturedId = SelectedItem.ItemId;
            const int32 CapturedAffixIndex = AffixIndex;

            ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[MenuText(FText::FromString(DescribeAffix(Affix)), BreakerUI::TypeCaption, SoftText)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(150.0f)
                    [
                        bAtCeiling
                            ? StaticCastSharedRef<SWidget>(SNew(SBox).HAlign(HAlign_Right)[MenuText(FText::FromString(TEXT("MAXED")), BreakerUI::TypeCaption, Disabled, true)])
                            : StaticCastSharedRef<SWidget>(BorderWrap(
                                SNew(SButton)
                                .ButtonColorAndOpacity(PanelRaised)
                                .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
                                .OnClicked(FOnClicked::CreateLambda([this, CapturedId, CapturedAffixIndex]()
                                {
                                    if (Character.IsValid() && Character->GetEquipment())
                                    {
                                        // Forge-proximity gating arrives with the
                                        // hub, same interim rule as Progression::
                                        // RespecAtForge's menu-side shim above:
                                        // pass true unconditionally so the verb is
                                        // testable from the menu today.
                                        const EBreakerForgeResult Result = Character->GetEquipment()->TemperItem(CapturedId, CapturedAffixIndex, /*bIsAtForge=*/true);
                                        ForgeStatus = FText::FromString(FString::Printf(TEXT("TEMPER: %s"), *DescribeForgeResult(Result)));
                                    }
                                    Rebuild(EBreakerMenuScreen::Forge);
                                    return FReply::Handled();
                                }))
                                [
                                    MenuText(FText::FromString(FString::Printf(TEXT("TEMPER · %s"), *DescribeForgeCost(CostTemper))), BreakerUI::TypeCaption, bAffordableTemper ? Amber : Harm, true)
                                ],
                                bAffordableTemper ? Amber : HarmDeep))
                    ]
                ]
            ];
        }

        // ---- REFORGE / ATTUNE: whole-item verbs -----------------------------
        const FBreakerForgeCost CostReforge = UBreakerForgeLibrary::ReforgeCost(SelectedItem);
        const bool bAffordableReforge = Wallet.CanAfford(CostReforge);
        const FBreakerForgeCost CostAttune = UBreakerForgeLibrary::AttuneCost(SelectedItem);
        const bool bAffordableAttune = Wallet.CanAfford(CostAttune);
        const FGuid CapturedSelectedId = SelectedItem.ItemId;

        ForgePanel->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(PanelRaised)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, CapturedSelectedId]()
                {
                    if (Character.IsValid() && Character->GetEquipment())
                    {
                        const EBreakerForgeResult Result = Character->GetEquipment()->ReforgeItem(CapturedSelectedId, /*bIsAtForge=*/true);
                        ForgeStatus = FText::FromString(FString::Printf(TEXT("REFORGE: %s"), *DescribeForgeResult(Result)));
                    }
                    Rebuild(EBreakerMenuScreen::Forge);
                    return FReply::Handled();
                }))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(TEXT("REFORGE — REROLL EVERY AFFIX VALUE")), BreakerUI::TypeBody, Primary, true)]
                    + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(DescribeForgeCost(CostReforge)), BreakerUI::TypeBody, bAffordableReforge ? Amber : Harm, true)]
                ],
                BorderEmphasis)
        ];
        ForgePanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(PanelRaised)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, CapturedSelectedId]()
                {
                    if (Character.IsValid() && Character->GetEquipment())
                    {
                        const EBreakerForgeResult Result = Character->GetEquipment()->AttuneItem(CapturedSelectedId, /*bIsAtForge=*/true);
                        ForgeStatus = FText::FromString(FString::Printf(TEXT("ATTUNE: %s"), *DescribeForgeResult(Result)));
                    }
                    Rebuild(EBreakerMenuScreen::Forge);
                    return FReply::Handled();
                }))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(TEXT("ATTUNE — REROLL WHICH AFFIXES IT CARRIES")), BreakerUI::TypeBody, Primary, true)]
                    + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(DescribeForgeCost(CostAttune)), BreakerUI::TypeBody, bAffordableAttune ? Amber : Harm, true)]
                ],
                BorderEmphasis)
        ];

        // ---- SALVAGE: backpack only, destroys the item ----------------------
        if (!bSelectedEquipped)
        {
            const FBreakerForgeWallet SalvagePreview = UBreakerForgeLibrary::SalvageValue(SelectedItem);
            ForgePanel->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
            [
                MenuText(FText::FromString(FString::Printf(TEXT("SALVAGE — DESTROYS THE ITEM · PAYS %d %s"),
                    SalvagePreview.Get(), BreakerForge::CurrencyDisplayName)),
                    BreakerUI::TypeCaption, Harm, true)
            ];
            ForgePanel->AddSlot().AutoHeight()
            [
                BorderWrap(
                    SNew(SButton)
                    .ButtonColorAndOpacity(Panel)
                    .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                    .ToolTipText(FText::FromString(TEXT("Destroys this item and pays its salvage value into the wallet.")))
                    .OnClicked(FOnClicked::CreateLambda([this, CapturedSelectedId]()
                    {
                        if (Character.IsValid() && Character->GetEquipment())
                        {
                            const bool bOk = Character->GetEquipment()->SalvageFromBackpack(CapturedSelectedId);
                            ForgeStatus = FText::FromString(bOk ? TEXT("SALVAGE: DONE.") : TEXT("SALVAGE: ITEM NOT FOUND."));
                            if (bOk) ForgeSelectedItemId.Invalidate();
                        }
                        Rebuild(EBreakerMenuScreen::Forge);
                        return FReply::Handled();
                    }))
                    [
                        MenuText(FText::FromString(TEXT("SALVAGE")), BreakerUI::TypeCaption, Harm, true)
                    ],
                    HarmDeep)
            ];
        }

        if (!ForgeStatus.IsEmpty())
        {
            ForgePanel->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)[MenuText(ForgeStatus, BreakerUI::TypeCaption, Amber, true)];
        }

        OpsColumn->AddSlot().AutoHeight()[MakePlate(ForgePanel, PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space16))];
    }

    TSharedRef<SHorizontalBox> Body = SNew(SHorizontalBox);
    Body->AddSlot().AutoWidth()
    [
        SNew(SBox).WidthOverride(420.0f)[SNew(SScrollBox) + SScrollBox::Slot()[PickerList]]
    ];
    Body->AddSlot().FillWidth(1.0f).Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
    [
        SNew(SScrollBox) + SScrollBox::Slot()[OpsColumn]
    ];

    return BuildZonedFrame(
        FText::FromString(TEXT("FORGE")),
        FText::FromString(TEXT("SALVAGE · TEMPER · REFORGE · ATTUNE")),
        HeaderRight,
        Body,
        SNullWidget::NullWidget,
        Metrics.PanelWidth,
        Metrics.PanelHeight,
        // Fixed height, never content height — see the shrink-wrap note in
        // BuildZonedFrame. A forge selection changes the body's height, and a
        // centred plate that changes height jumps.
        /*bFillHeight=*/true);
}


// ---------------------------------------------------------------------------
// O100: THE QUARTERMASTER
// ---------------------------------------------------------------------------
// Where a token is spent. An ANCHOR interaction, which in this file means one
// concrete thing: this screen has no BuildScreenTabs call and no AddTab entry,
// so the only way in is the quartermaster's dialogue. That is not decoration —
// the tab strip is precisely how the Forge became reachable from the pause menu
// two clicks deep, which content-and-modes forbids, and a quartermaster added
// as a fifth tab would inherit the same defect on day one.
//
// The screen AUTHORS NO RULE. It reads GetUnlockableAbilityIds,
// GetUnspentAbilityTokens and IsAbilityUnlocked, and it spends through
// SpendAbilityToken. Every refusal the player sees is progression's own text,
// for the reason TryEquipAbility's "ONE writer" comment already gives: a second
// copy of an unlock rule drifts from the first.
TSharedRef<SWidget> SBreakerMenu::BuildQuartermasterScreen()
{
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const FWideScreenMetrics Metrics = MeasureWideScreen();

    const int32 Tokens = Progression ? Progression->GetUnspentAbilityTokens() : 0;

    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    // Deliberately NO BuildScreenTabs here. See the block comment above.
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
    [
        MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(TEXT("TOKENS")), BreakerUI::TypeCaption, Muted, true)]
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::AsNumber(Tokens), BreakerUI::TypeH2, Primary, true)],
            PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space4))
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        SNew(SBox).WidthOverride(120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    if (!QuartermasterStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(QuartermasterStatus, BreakerUI::TypeCaption, SoftText)
        ];
    }

    const TArray<FName> Offered = Progression ? Progression->GetUnlockableAbilityIds() : TArray<FName>();
    if (Offered.Num() == 0)
    {
        // A class with nothing left to sell is a real state, not an error: the
        // stock is finite by design (one token per unlockable), so it empties
        // once the last one is bought. Say so rather than showing a blank plate.
        Body->AddSlot().AutoHeight()
        [
            MenuText(FText::FromString(TEXT("NOTHING IN STOCK FOR YOUR CLASS.")), BreakerUI::TypeCaption, Disabled)
        ];
    }

    for (const FName AbilityId : Offered)
    {
        const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(AbilityId);
        const bool bOwned = Progression && Progression->IsAbilityUnlocked(AbilityId);
        const bool bAffordable = Tokens > 0;
        const FName CapturedId = AbilityId;

        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuText(Definition ? Definition->DisplayName : FText::FromName(AbilityId),
                    BreakerUI::TypeBody, bOwned ? Muted : Primary, true)
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuText(Definition ? Definition->Description : FText::GetEmpty(), BreakerUI::TypeCaption, SoftText)
            ]
        ];
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(160.0f)
            [
                bOwned
                ? MenuText(FText::FromString(TEXT("UNLOCKED")), BreakerUI::TypeCaption, Muted, true)
                : MakeButton(FText::FromString(TEXT("UNLOCK · 1 TOKEN")),
                    FOnClicked::CreateLambda([this, CapturedId]()
                    {
                        UBreakerProgressionComponent* Target = Character.IsValid() ? Character->GetProgression() : nullptr;
                        if (!Target) return FReply::Handled();
                        FText Failure;
                        // The screen never decides. Progression refuses or
                        // spends, and its reason is what the player reads.
                        if (Target->SpendAbilityToken(CapturedId, Failure))
                        {
                            QuartermasterStatus = FText::FromString(TEXT("UNLOCKED. EQUIP IT FROM THE ABILITIES TAB."));
                        }
                        else
                        {
                            QuartermasterStatus = Failure;
                        }
                        Rebuild(EBreakerMenuScreen::Quartermaster);
                        return FReply::Handled();
                    }),
                    // PAINTED, NEVER FADED (the banned-patterns rule): an
                    // unaffordable unlock renders as a secondary button rather
                    // than a dimmed one, and the click is refused in the
                    // handler by progression. Opacity on a subtree shows the
                    // plate seams behind it.
                    /*bPrimary=*/bAffordable)
            ]
        ];

        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MakePlate(Row, PanelRaised, BorderRest, FMargin(BreakerUI::Space16, BreakerUI::Space8))
        ];
    }

    return BuildZonedFrame(
        FText::FromString(TEXT("QUARTERMASTER")),
        FText::FromString(TEXT("ABILITY UNLOCKS")),
        HeaderRight,
        Body,
        SNullWidget::NullWidget,
        Metrics.PanelWidth,
        Metrics.PanelHeight,
        /*bFillHeight=*/true);
}

TSharedRef<SWidget> SBreakerMenu::BuildAbilitiesScreen()
{
    // Reach (Decisions.md O37): a picker over UBreakerAbilityComponent's
    // selection API (GetSelectableAbilityIds / GetEquippedAbilityId /
    // PreviewSelection / TryEquipAbility), which shipped with zero callers
    // anywhere in the project. "WHAT A UI MUST CALL, in order" is documented
    // at Abilities/BreakerAbilityComponent.h:99-122; this screen is that
    // caller.
    UBreakerAbilityComponent* Abilities = Character.IsValid() ? Character->GetAbilities() : nullptr;
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const EBreakerClassId PermanentClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    const FWideScreenMetrics Metrics = MeasureWideScreen();

    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[BuildScreenTabs(EBreakerMenuScreen::Abilities)];
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        SNew(SBox).WidthOverride(120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    if (!Abilities || PermanentClass == EBreakerClassId::None)
    {
        Body->AddSlot().AutoHeight()
        [
            MakePlate(
                MenuText(FText::FromString(TEXT("CHOOSE A CLASS ON THE BREAKER CLASS SCREEN FIRST — THERE IS NOTHING TO EQUIP WITHOUT ONE.")), BreakerUI::TypeCaption, Muted),
                PanelRaised, BorderEmphasis, FMargin(BreakerUI::Space16, BreakerUI::Space16))
        ];
    }
    else
    {
        struct FSlotEntry { EBreakerAbilitySlot Slot; const TCHAR* Label; };
        static const FSlotEntry SlotEntries[] =
        {
            { EBreakerAbilitySlot::ClassAbilityOne, TEXT("CLASS ABILITY 1") },
            { EBreakerAbilitySlot::ClassAbilityTwo, TEXT("CLASS ABILITY 2") },
            { EBreakerAbilitySlot::Ultimate,        TEXT("ULTIMATE") },
        };

        for (const FSlotEntry& SlotEntry : SlotEntries)
        {
            const EBreakerAbilitySlot Slot = SlotEntry.Slot;
            // GetEquippedAbilityId is the PLAYER'S raw pick (what the picker
            // must mark as selected); GetAbilityIdForSlot is what actually
            // resolves once fallback is applied (what the HUD activates). The
            // header is explicit that a picker needs the former, not the
            // latter, "or the player cannot tell 'I picked Cleave' from
            // 'Cleave is what you get when you pick nothing'."
            const FName EquippedChoiceId = Abilities->GetEquippedAbilityId(Slot);
            const FName GrantedId = Abilities->GetAbilityIdForSlot(Slot);
            const UBreakerAbilityDefinition* GrantedDefinition = GrantedId.IsNone() ? nullptr : UBreakerAbilityDefinition::FindFallback(GrantedId);

            TSharedRef<SVerticalBox> SlotBody = SNew(SVerticalBox);
            SlotBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(SlotEntry.Label), BreakerUI::TypeH2, Primary, true)]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("ACTIVE: %s"), GrantedDefinition ? *GrantedDefinition->DisplayName.ToString() : TEXT("NONE"))),
                        BreakerUI::TypeCaption, GrantedDefinition ? Cyan : Muted, true)
                ]
            ];

            const TArray<FName> Catalogue = Abilities->GetSelectableAbilityIds(Slot);
            if (Catalogue.IsEmpty())
            {
                SlotBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
                [
                    MenuText(FText::FromString(TEXT("NO ABILITIES REGISTERED FOR THIS SLOT YET.")), BreakerUI::TypeCaption, Disabled)
                ];
            }
            for (const FName AbilityId : Catalogue)
            {
                const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(AbilityId);
                const bool bIsEquippedChoice = EquippedChoiceId == AbilityId;
                // Cheap, definite blockers only (already sitting in the other
                // slot, wrong class/slot, unknown id). What this CANNOT see is
                // UBreakerProgressionComponent::IsAbilityUnlocked, which only
                // runs inside TryEquipAbility — so a Caster pick previews as
                // fine here and the true "not unlocked" reason only appears
                // after the click. That is the API's own documented seam
                // (Abilities/BreakerAbilityComponent.h), not a bug in this
                // screen: "verify with Swift, and show refusal reasons
                // honestly for Caster."
                const EBreakerAbilitySelectionResult Preview = Abilities->PreviewSelection(Slot, AbilityId);
                const bool bPreviewBlocked = !bIsEquippedChoice && Preview != EBreakerAbilitySelectionResult::Allowed;
                const bool bImplemented = Definition && Definition->IsImplemented();
                const FName CapturedId = AbilityId;

                SlotBody->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
                [
                    BorderWrap(
                        SNew(SButton)
                        .ButtonColorAndOpacity(bIsEquippedChoice ? PanelHover : PanelRaised)
                        .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                        .OnClicked(FOnClicked::CreateLambda([this, Slot, CapturedId]()
                        {
                            if (Character.IsValid() && Character->GetAbilities())
                            {
                                FText FailureReason;
                                const bool bOk = Character->GetAbilities()->TryEquipAbility(Slot, CapturedId, FailureReason);
                                // Surfaced verbatim: FailureReason is the exact
                                // player-facing text TryEquipAbility fills in,
                                // e.g. a Caster's "That ability has not been
                                // unlocked." — this screen states it rather
                                // than substituting its own copy.
                                AbilityStatus = bOk ? FText::FromString(TEXT("EQUIPPED.")) : FailureReason;
                            }
                            Rebuild(EBreakerMenuScreen::Abilities);
                            return FReply::Handled();
                        }))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1.0f)
                                [
                                    MenuText(Definition ? Definition->DisplayName : FText::FromName(CapturedId),
                                        BreakerUI::TypeBody, bIsEquippedChoice ? Primary : SoftText, true)
                                ]
                                + SHorizontalBox::Slot().AutoWidth()
                                [
                                    bIsEquippedChoice
                                        ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(TEXT("EQUIPPED")), BreakerUI::TypeCaption, Cyan, true))
                                        : SNullWidget::NullWidget
                                ]
                                + SHorizontalBox::Slot().AutoWidth().Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
                                [
                                    // "AbilityId is recorded even when the
                                    // ability has no implementation yet" is
                                    // already the HUD's rule for a granted
                                    // slot (BreakerAbilityComponent.h); the
                                    // catalogue gets the same honesty rather
                                    // than quietly offering a pick that will
                                    // do nothing in combat.
                                    !bImplemented
                                        ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(TEXT("NOT IMPLEMENTED")), BreakerUI::TypeCaption, Disabled, true))
                                        : SNullWidget::NullWidget
                                ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                            [
                                MenuText(Definition ? Definition->Description : FText::GetEmpty(), BreakerUI::TypeCaption, Muted)
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                            [
                                bPreviewBlocked
                                    ? StaticCastSharedRef<SWidget>(MenuText(UBreakerAbilityComponent::DescribeSelectionResult(Preview), BreakerUI::TypeCaption, Harm, true))
                                    : SNullWidget::NullWidget
                            ]
                        ],
                        bIsEquippedChoice ? Cyan : BorderRest,
                        bIsEquippedChoice ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
                ];
            }

            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
            [
                MakePlate(SlotBody, PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space16))
            ];
        }

        if (!AbilityStatus.IsEmpty())
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[MenuText(AbilityStatus, BreakerUI::TypeCaption, Amber, true)];
        }
    }

    const FString MetaLine = FString::Printf(TEXT("BREAKER · %s · TWO CLASS ABILITIES + ONE ULTIMATE"), *ClassDisplayName(PermanentClass));

    return BuildZonedFrame(
        FText::FromString(TEXT("ABILITIES")),
        FText::FromString(MetaLine),
        HeaderRight,
        SNew(SScrollBox) + SScrollBox::Slot()[Body],
        SNullWidget::NullWidget,
        Metrics.PanelWidth,
        Metrics.PanelHeight,
        // Fixed height — see the shrink-wrap note in BuildZonedFrame.
        /*bFillHeight=*/true);
}

TSharedRef<SWidget> SBreakerMenu::BuildDialogueScreen()
{
    ABreakerNPC* NPC = DialogueNPC.Get();
    FBreakerDialogueNode Node;
    if (!NPC || !NPC->FindDialogueNode(DialogueNodeId, Node))
    {
        if (Character.IsValid()) Character->ResumeFromMenu();
        return SNew(SBox);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[MenuText(NPC->GetDisplayName(), BreakerUI::TypeCaption, Muted, true)]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(Node.SpeakerLine))
                .ColorAndOpacity(SoftText)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeBody))
                .AutoWrapText(true)
            ],
            PanelRaised, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space16))
    ];

    // Gated entries: a choice can require a flag or be hidden by one. Iterating
    // Node.Choices unconditionally is what made flags invisible to the player.
    const UBreakerQuestJournal* Journal = Character.IsValid() ? Character->GetQuestJournal() : nullptr;
    static const FBreakerQuestFlagSet EmptyFlags;
    TArray<FBreakerDialogueChoice> VisibleChoices;
    NPC->GetVisibleChoices(Node, Journal ? Journal->GetState() : EmptyFlags, VisibleChoices);

    int32 ChoiceNumber = 0;
    for (const FBreakerDialogueChoice& Choice : VisibleChoices)
    {
        ++ChoiceNumber;
        const FName NextNodeId = Choice.NextNodeId;
        const FName QuestFlag = Choice.SetsQuestFlag;
        const EBreakerDialogueAction Action = Choice.Action;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this, NextNodeId, QuestFlag, Action]()
            {
                if (Character.IsValid())
                {
                    Character->AddQuestFlag(QuestFlag);
                    // The action runs after the flag and BEFORE the end-of-
                    // conversation exit, so a choice can both close the
                    // dialogue and open a screen. This is the quartermaster's
                    // only door (O100).
                    if (Action == EBreakerDialogueAction::OpenQuartermaster)
                    {
                        QuartermasterStatus = FText::GetEmpty();
                        Rebuild(EBreakerMenuScreen::Quartermaster);
                        return FReply::Handled();
                    }
                    if (Action == EBreakerDialogueAction::OpenForge)
                    {
                        // Reached through Kess, so the player IS at the Forge.
                        // This is what makes bIsAtForge a real answer rather
                        // than a hardcoded true.
                        bAtForge = true;
                        ForgeStatus = FText::GetEmpty();
                        Rebuild(EBreakerMenuScreen::Forge);
                        return FReply::Handled();
                    }
                    if (NextNodeId == NAME_None)
                    {
                        Character->ResumeFromMenu();
                        return FReply::Handled();
                    }
                }
                DialogueNodeId = NextNodeId;
                Rebuild(EBreakerMenuScreen::Dialogue);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(FString::Printf(TEXT("%d.  %s"), ChoiceNumber, *Choice.Text)), BreakerUI::TypeBody, SoftText, true)
            ],
            BorderEmphasis)
        ];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("Choices marked [Leave] end the conversation  |  ESC to walk away")), 9, SoftText)
    ];
    return BuildFrame(FText::FromString(TEXT("CONVERSATION")), NPC->GetDisplayName(), Body, 780.0f);
}

TSharedRef<SWidget> SBreakerMenu::BuildTravelScreen()
{
    ABreakerTravelPoint* Point = TravelPoint.Get();
    if (!Point)
    {
        // The interactable went away while its screen was up. Same answer the
        // dialogue screen gives to a missing node: leave, rather than draw an
        // empty picker the player can only escape from.
        if (Character.IsValid()) Character->ResumeFromMenu();
        return SNew(SBox);
    }

    // The FILTERED list, never the raw registry. GetAvailableDestinations drops
    // disabled entries and the point's own ExcludedDestinationId, so a card can
    // never exist for a place SelectDestination would refuse — or for the place
    // the player is already standing in.
    const TArray<FBreakerTravelDestination> Destinations = Point->GetAvailableDestinations();

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    if (Destinations.Num() == 0)
    {
        // Said out loud rather than drawn as an empty list, which reads as a
        // broken screen. Reachable today only if a point excludes the sole
        // enabled destination.
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
        [
            MakePlate(
                MenuText(FText::FromString(TEXT("NOWHERE TO GO FROM HERE YET.")), BreakerUI::TypeBody, Muted, true),
                PanelRaised, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space16))
        ];
    }

    for (const FBreakerTravelDestination& Destination : Destinations)
    {
        const FName DestinationId = Destination.Id;
        const bool bSelected = SelectedTravelDestinationId == DestinationId;

        TSharedRef<SVerticalBox> Card = SNew(SVerticalBox);
        Card->AddSlot().AutoHeight()
        [
            MenuText(Destination.DisplayName, BreakerUI::TypeH2, bSelected ? Primary : SoftText, true)
        ];
        Card->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(Destination.Description))
                .ColorAndOpacity(bSelected ? SoftText : Muted)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
                // Wraps rather than clips: a description is authored prose of
                // unknown length, and this is the one place on the screen whose
                // width the text does not get to decide.
                .AutoWrapText(true)
        ];

        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bSelected ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space16))
                .HAlign(HAlign_Fill)
                .OnClicked(FOnClicked::CreateLambda([this, DestinationId]()
                {
                    // The selection moves FIRST, so a refused travel leaves the
                    // ring on the card the player actually chose rather than on
                    // whatever was marked before.
                    SelectedTravelDestinationId = DestinationId;
                    ABreakerTravelPoint* Live = TravelPoint.Get();
                    if (!Live) { if (Character.IsValid()) Character->ResumeFromMenu(); return FReply::Handled(); }

                    // The travel point does not teleport anyone — it broadcasts
                    // OnDestinationSelected and whoever bound it decides what
                    // travel means. So a TRUE here means the request was
                    // accepted, which is the menu's cue to get out of the way.
                    if (Live->SelectDestination(DestinationId, Character.Get()))
                    {
                        if (Character.IsValid()) Character->ResumeFromMenu();
                        return FReply::Handled();
                    }
                    // FALSE means unknown or disabled — a card built before the
                    // destination went away. The menu stays open and says so
                    // instead of closing on a departure that never happened.
                    TravelStatus = FText::FromString(TEXT("THAT DESTINATION IS NO LONGER AVAILABLE."));
                    Rebuild(EBreakerMenuScreen::Travel);
                    return FReply::Handled();
                }))
                [
                    Card
                ],
                // Selected carries the 2px accent ring, unselected the neutral
                // 1px one — the same selected-state vocabulary the tab strip
                // and the class banners use. Never a colour-only difference.
                bSelected ? Cyan : BorderEmphasis,
                bSelected ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    }

    // Always present, so the list cannot change height when a refusal lands.
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        SNew(SBox).HeightOverride(20.0f)
        [
            MenuText(TravelStatus, BreakerUI::TypeCaption, Harm, true)
        ]
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("CLICK A DESTINATION TO TRAVEL  |  ESC STAYS HERE")), BreakerUI::TypeCaption, SoftText)
    ];

    // BuildFrame, at the dialogue screen's width — the two are the same kind of
    // screen and should not be two sizes. Fixed-height plate with a scrolling
    // body, which is what BuildFrame now IS: a content-sized panel here would
    // resize the plate every time the destination count changed, which is the
    // jitter this frame was rebuilt to remove.
    // The subtitle counts rather than naming the place: the point knows where
    // it does NOT go (ExcludedDestinationId) but carries no display name for
    // where it IS, and inventing one here would be a second source of truth for
    // location names.
    const FString Subtitle = FString::Printf(TEXT("%d DESTINATION%s"),
        Destinations.Num(), Destinations.Num() == 1 ? TEXT("") : TEXT("S"));
    return BuildFrame(FText::FromString(TEXT("TRAVEL")), FText::FromString(Subtitle), Body, 780.0f);
}

// ---------------------------------------------------------------------------
// THE BREAKPOINT SANDBOX.
//
// Owner: "a way for me as a player/dev to test different breakpoints and
// strength throughout the progression of the game". Every control on this
// screen is PLUMBING to a call that already exists — AwardExperience /
// LoadProgressionState for level, the game mode's GymAreaLevel tunable for
// area difficulty, DevForceClass for class, RollDropSlot/RollItem +
// AddToBackpack for seeded gear — so the sandbox can never disagree with the
// game about what a level-30 character with area-40 loot IS. No game rule
// lives here.
//
// It keeps the settings screen's PREVIOUS idiom: SettingsSectionHeader /
// SettingsRow rows, BuildFrame's fixed-height scrolling plate clamped to the
// viewport, MakeButton chrome, and readouts that are plain text rebuilt by the
// click that changed them — never a per-frame attribute polling a component.
// (The settings screen itself moved onto the Fieldplate sidebar layout; this
// dev screen is not part of that pass and follows it whenever its turn comes.)
// ---------------------------------------------------------------------------
TSharedRef<SWidget> SBreakerMenu::BuildDevSandboxScreen()
{
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;
    UBreakerAttributeSet* Attributes = Character.IsValid() ? Character->GetAttributes() : nullptr;
    ABreakerGameMode* GameMode = (Character.IsValid() && Character->GetWorld())
        ? Character->GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;

    const int32 CurrentLevel = Progression ? Progression->GetCharacterLevel() : 1;
    const int32 ShownTargetLevel = FMath::Clamp(DevTargetLevel > 0 ? DevTargetLevel : CurrentLevel,
        1, UBreakerExperienceLibrary::MaxCharacterLevel);

    // A small selectable chip: the tab strip's selected-state vocabulary (2px
    // cyan ring on selected, neutral 1px otherwise), sized by its content.
    // A function-local helper rather than an anonymous-namespace one, so the
    // unity-build prefix rule has nothing to collide.
    auto MakeChip = [this](const FString& Label, bool bSelected, const FLinearColor& LabelColor, const FOnClicked& OnClicked)
    {
        return BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bSelected ? PanelHover : Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .OnClicked(OnClicked)
            [
                MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bSelected ? LabelColor : Muted, true)
            ],
            bSelected ? Cyan : BorderEmphasis,
            bSelected ? BreakerUI::BorderSelected : BreakerUI::BorderThin);
    };

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        MenuText(FText::FromString(
            TEXT("DEV TOOLING. Everything below drives the game's own progression, class and loot calls — and SAVES, so what you set here is what the character IS.")),
            BreakerUI::TypeCaption, Amber, true)
    ];

    // ---- 1. Character level ---------------------------------------------
    Body->AddSlot().AutoHeight()[SettingsSectionHeader(TEXT("CHARACTER LEVEL"))];
    if (Progression)
    {
        // Readout built FIRST, handle captured BY VALUE — the settings screen's
        // own note on argument evaluation order applies here verbatim.
        TSharedPtr<STextBlock> LevelReadout;
        const TSharedRef<SWidget> LevelValue =
            SNew(SBox).WidthOverride(SettingsValueWidth).HAlign(HAlign_Fill)
            [
                SAssignNew(LevelReadout, STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%d"), ShownTargetLevel)))
                    .Justification(ETextJustify::Right)
                    .ColorAndOpacity(Cyan)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
            ];

        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            SettingsRow(TEXT("TARGET LEVEL"),
                SNew(SSlider)
                // 1..50, the ladder's hard cap. The readout is written from
                // OnValueChanged — an event — never a Text_Lambda.
                .Value(static_cast<float>(ShownTargetLevel - 1) / static_cast<float>(UBreakerExperienceLibrary::MaxCharacterLevel - 1))
                .OnValueChanged_Lambda([this, LevelReadout](float Value)
                {
                    DevTargetLevel = 1 + FMath::RoundToInt(Value * (UBreakerExperienceLibrary::MaxCharacterLevel - 1));
                    if (LevelReadout.IsValid())
                    {
                        LevelReadout->SetText(FText::FromString(FString::Printf(TEXT("%d"), DevTargetLevel)));
                    }
                }),
                LevelValue)
        ];
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("NOW: LEVEL %d  ·  %d TOTAL XP  ·  %d CLASS / %d CORE UNSPENT"),
                CurrentLevel, Progression->GetTotalExperience(),
                Progression->GetProgressionState().UnspentClassPoints,
                Progression->GetProgressionState().UnspentCorePoints)),
                BreakerUI::TypeCaption, SoftText, true)
        ];
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            SNew(SBox).WidthOverride(240.0f)
            [
                MakeButton(FText::FromString(TEXT("SET LEVEL")), FOnClicked::CreateLambda([this]()
                {
                    UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                    if (!Prog)
                    {
                        DevSandboxStatus = FText::FromString(TEXT("NO PROGRESSION COMPONENT."));
                        Rebuild(EBreakerMenuScreen::DevSandbox);
                        return FReply::Handled();
                    }
                    const int32 Target = FMath::Clamp(DevTargetLevel > 0 ? DevTargetLevel : Prog->GetCharacterLevel(),
                        1, UBreakerExperienceLibrary::MaxCharacterLevel);
                    // The level is DRIVEN THROUGH XP, exactly the way play
                    // does it: the target total comes off the live curve, and
                    // the delta is paid with AwardExperience so the per-level
                    // point entitlement, the level-up event and the HUD tell
                    // all fire as they would in the field.
                    const int32 TargetXp = UBreakerExperienceLibrary::TotalXpToReachLevel(Target, Prog->ExperienceCurve);
                    const int32 CurrentXp = Prog->GetTotalExperience();
                    if (TargetXp > CurrentXp)
                    {
                        Prog->AwardExperience(TargetXp - CurrentXp);
                    }
                    else if (TargetXp < CurrentXp)
                    {
                        // Down-levelling has no play-path verb, so it goes
                        // through the save-load seam instead of a new one:
                        // rewrite TotalExperience in a copy of the state and
                        // LoadProgressionState re-derives the level exactly as
                        // a save load would. Granted points are NOT clawed
                        // back — the entitlement is monotonic by design.
                        FBreakerProgressionState NewState = Prog->GetProgressionState();
                        NewState.TotalExperience = TargetXp;
                        Prog->LoadProgressionState(NewState);
                    }
                    if (Character.IsValid()) Character->SaveGameState();
                    DevSandboxStatus = FText::FromString(FString::Printf(
                        TEXT("LEVEL SET TO %d (%d TOTAL XP). DOWN-LEVELS NEVER RECLAIM GRANTED POINTS."),
                        Prog->GetCharacterLevel(), Prog->GetTotalExperience()));
                    Rebuild(EBreakerMenuScreen::DevSandbox);
                    return FReply::Handled();
                }), true)
            ]
        ];
    }
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(TEXT("NO PROGRESSION COMPONENT ON THIS PAWN.")), BreakerUI::TypeCaption, Harm, true)
        ];
    }

    // ---- 2. Gym area level ----------------------------------------------
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)[SettingsSectionHeader(TEXT("GYM AREA LEVEL"))];
    if (GameMode)
    {
        TSharedRef<SHorizontalBox> AreaRow = SNew(SHorizontalBox);
        AreaRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
        [
            MenuValueColumn(FText::FromString(FString::Printf(TEXT("%d"), GameMode->GymAreaLevel)),
                SettingsValueWidth, BreakerUI::TypeH2, Cyan)
        ];
        // Stepped, not slid: the area ladder's interesting places are exact
        // integers (the rarity gates sit at 25 and 40), and a slider lands
        // beside them.
        const int32 Steps[] = { -10, -1, +1, +10 };
        for (const int32 Step : Steps)
        {
            AreaRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                MakeChip(FString::Printf(TEXT("%+d"), Step), false, Primary,
                    FOnClicked::CreateLambda([this, Step]()
                    {
                        ABreakerGameMode* Mode = (Character.IsValid() && Character->GetWorld())
                            ? Character->GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;
                        if (Mode)
                        {
                            // Direct write to the same BlueprintReadWrite
                            // tunable the details panel edits, clamped to its
                            // own declared 1..100 range.
                            Mode->GymAreaLevel = FMath::Clamp(Mode->GymAreaLevel + Step, 1, 100);
                            DevSandboxStatus = FText::FromString(FString::Printf(
                                TEXT("GYM AREA LEVEL %d — APPLIES TO ENEMIES SPAWNED FROM NOW ON, NOT ONES ALREADY STANDING."),
                                Mode->GymAreaLevel));
                        }
                        Rebuild(EBreakerMenuScreen::DevSandbox);
                        return FReply::Handled();
                    }))
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[AreaRow];
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(TEXT("DRIVES MONSTER STRENGTH AND DROP ITEM LEVEL. RARITY GATES: ABERRANT NEEDS ILVL 25, ANOMALOUS 40.")),
                BreakerUI::TypeCaption, Muted)
        ];
    }
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(TEXT("NO BREAKER GAME MODE IN THIS WORLD — AREA LEVEL LIVES ON THE GYM.")),
                BreakerUI::TypeCaption, Muted, true)
        ];
    }

    // ---- 3. Force class --------------------------------------------------
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)[SettingsSectionHeader(TEXT("FORCE CLASS"))];
    {
        const EBreakerClassId CurrentClass = Progression
            ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
        TSharedRef<SHorizontalBox> ClassRow = SNew(SHorizontalBox);
        for (const FBreakerClassBlurb& Blurb : GBreakerClassBlurbs)
        {
            const EBreakerClassId Captured = Blurb.ClassId;
            const bool bIsCurrent = Captured == CurrentClass;
            ClassRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                MakeChip(Blurb.Name, bIsCurrent, ClassHasImplementedKit(Captured) ? Primary : Disabled,
                    FOnClicked::CreateLambda([this, Captured]()
                    {
                        UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                        if (Prog)
                        {
                            // The existing dev path, verbatim from the old
                            // class-select checkbox flow: DevForceClass swaps
                            // the class (clearing the definition for kitless
                            // ones, its own documented behaviour) and the
                            // save makes it stick.
                            Prog->DevForceClass(Captured);
                            if (Character.IsValid()) Character->SaveGameState();
                            DevSandboxStatus = FText::FromString(TEXT("CLASS FORCED. KITLESS CLASSES RUN WITH NO KIT — THAT IS THE POINT OF LOOKING."));
                        }
                        Rebuild(EBreakerMenuScreen::DevSandbox);
                        return FReply::Handled();
                    }))
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[ClassRow];
    }
    // The class-swap toggle used to live here. It is gone: class selection is
    // permanent per character, the class screen now states that rule without an
    // exception, and testing another class is a new character. The GConfig key
    // survives because two OTHER dev affordances still read it -- the gear
    // grants below and the skill screen's point-recovery row -- so its name is
    // now historical rather than descriptive.

    // ---- 4. Seeded gear --------------------------------------------------
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)[SettingsSectionHeader(TEXT("SEEDED GEAR"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        SettingsRow(TEXT("SEED"),
            SNew(SBox).HeightOverride(BreakerUI::MinHitTarget)
            [
                SNew(SEditableTextBox)
                .Text(FText::FromString(DevSeedString))
                .HintText(FText::FromString(TEXT("Empty rolls a fresh seed")))
                .OnTextChanged(FOnTextChanged::CreateLambda([this](const FText& NewText)
                {
                    // Stored WITHOUT rebuilding, same as the create screen's
                    // name field: a rebuild would destroy the box mid-word.
                    DevSeedString = NewText.ToString();
                }))
            ],
            MenuText(FText::FromString(TEXT("SAME SEED, SAME ITEM")), BreakerUI::TypeCaption, Muted, true))
    ];
    {
        TSharedRef<SHorizontalBox> RarityRow = SNew(SHorizontalBox);
        for (int32 RarityIndex = 0; RarityIndex <= static_cast<int32>(EBreakerItemRarity::Anomalous); ++RarityIndex)
        {
            const EBreakerItemRarity Rarity = static_cast<EBreakerItemRarity>(RarityIndex);
            RarityRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                MakeChip(RarityName(Rarity), DevGrantRarity == Rarity, BreakerUI::RarityColor(Rarity),
                    FOnClicked::CreateLambda([this, Rarity]()
                    {
                        DevGrantRarity = Rarity;
                        Rebuild(EBreakerMenuScreen::DevSandbox);
                        return FReply::Handled();
                    }))
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[RarityRow];
    }
    {
        // Nine chips: the production slot draw ("FROM SEED", which is what a
        // real kill does) plus each slot pinned. Packed with the measured chip
        // rows the inventory filter bar uses, so nine chips cannot run off the
        // plate the way the loadout's nine once did.
        TArray<TSharedRef<SWidget>> SlotChips;
        TArray<float> SlotChipWidths;
        auto AddSlotChip = [this, &SlotChips, &SlotChipWidths, &MakeChip](const FString& Label, int32 SlotValue)
        {
            SlotChips.Add(MakeChip(Label, DevGrantSlot == SlotValue, Primary,
                FOnClicked::CreateLambda([this, SlotValue]()
                {
                    DevGrantSlot = SlotValue;
                    Rebuild(EBreakerMenuScreen::DevSandbox);
                    return FReply::Handled();
                })));
            SlotChipWidths.Add(MeasureChipWidth(Label, BreakerUI::Space16, BreakerUI::BorderThin));
        };
        AddSlotChip(TEXT("FROM SEED"), -1);
        for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
        {
            AddSlotChip(SlotName(static_cast<EBreakerEquipSlot>(SlotIndex)), SlotIndex);
        }
        const float ChipRowWidth = FMath::Min(1040.0f, MeasureWideScreen().PanelWidth) - 2.0f * BreakerUI::Space24;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            PackChipRows(SlotChips, SlotChipWidths, ChipRowWidth, BreakerUI::Space8)
        ];
    }
    {
        // Item level the grant rolls at — the sandbox's own control, not the
        // gym's. O48 froze the rarity gates and named dev spawning THE way to
        // test chase items, and the second half of the affix ladder (T6 at
        // ilvl 50 down to T1 at 120) sits past any reachable area level, so
        // without this stepper it is untestable. 0 (untouched) preserves the
        // old behaviour: the gym's area level, character level outside a gym.
        // Stepped, not slid, same reasoning as the area-level row above.
        const int32 FallbackItemLevel = GameMode ? GameMode->GymAreaLevel
            : (Progression ? Progression->GetCharacterLevel() : 1);
        const int32 ShownItemLevel = FMath::Clamp(
            DevGrantItemLevel > 0 ? DevGrantItemLevel : FallbackItemLevel,
            1, UBreakerAffixLibrary::MaxItemLevel);
        TSharedRef<SHorizontalBox> ItemLevelRow = SNew(SHorizontalBox);
        ItemLevelRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
        [
            MenuValueColumn(FText::FromString(FString::Printf(TEXT("%d"), ShownItemLevel)),
                SettingsValueWidth, BreakerUI::TypeH2, Cyan)
        ];
        const int32 ItemLevelSteps[] = { -10, -1, +1, +10 };
        for (const int32 Step : ItemLevelSteps)
        {
            ItemLevelRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
            [
                MakeChip(FString::Printf(TEXT("%+d"), Step), false, Primary,
                    FOnClicked::CreateLambda([this, Step, ShownItemLevel]()
                    {
                        // Step from the SHOWN value, so the first click on an
                        // untouched stepper moves off the fallback rather than
                        // off zero.
                        DevGrantItemLevel = FMath::Clamp(ShownItemLevel + Step, 1, UBreakerAffixLibrary::MaxItemLevel);
                        DevSandboxStatus = FText::FromString(FString::Printf(
                            TEXT("GRANT ITEM LEVEL %d — AFFIX TIERS FOLLOW ITEM LEVEL: T6 AT 50, T1 AT 120."),
                            DevGrantItemLevel));
                        Rebuild(EBreakerMenuScreen::DevSandbox);
                        return FReply::Handled();
                    }))
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[ItemLevelRow];
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(TEXT("ITEM LEVEL OF THE NEXT GRANT (1-120). UNTOUCHED = GYM AREA LEVEL, CHARACTER LEVEL OUTSIDE A GYM.")),
                BreakerUI::TypeCaption, Muted)
        ];
    }
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        SNew(SBox).WidthOverride(240.0f)
        [
            MakeButton(FText::FromString(TEXT("GRANT TO BACKPACK")), FOnClicked::CreateLambda([this]()
            {
                UBreakerEquipmentComponent* Equip = Character.IsValid() ? Character->GetEquipment() : nullptr;
                if (!Equip)
                {
                    DevSandboxStatus = FText::FromString(TEXT("NO EQUIPMENT COMPONENT."));
                    Rebuild(EBreakerMenuScreen::DevSandbox);
                    return FReply::Handled();
                }
                // Empty seed field rolls a fresh one and PRINTS it back into
                // the field, so an interesting roll can be re-rolled at will —
                // that is the whole reason the seed is a field and not hidden.
                const FString Trimmed = DevSeedString.TrimStartAndEnd();
                const int32 Seed = Trimmed.IsEmpty() ? FMath::Rand() : FCString::Atoi(*Trimmed);
                DevSeedString = FString::FromInt(Seed);
                // Slot pinned, or drawn from the seed by THE production draw —
                // the same salted RollDropSlot every kill uses, so "FROM SEED"
                // reproduces a drop rather than approximating one.
                const EBreakerEquipSlot Slot = DevGrantSlot >= 0
                    ? static_cast<EBreakerEquipSlot>(DevGrantSlot)
                    : UBreakerLootLibrary::RollDropSlot(Seed);
                // Item level: the stepper's value if the tester moved it (O48
                // — the sandbox is the route to ilvl-120 chase items no area
                // can pay out); untouched, the AREA's exactly as a kill pays
                // it, character level outside a gym so the button still works
                // in the Anchor.
                ABreakerGameMode* Mode = (Character.IsValid() && Character->GetWorld())
                    ? Character->GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;
                UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                const int32 ItemLevel = FMath::Clamp(
                    DevGrantItemLevel > 0 ? DevGrantItemLevel
                        : (Mode ? Mode->GymAreaLevel : (Prog ? Prog->GetCharacterLevel() : 1)),
                    1, UBreakerAffixLibrary::MaxItemLevel);
                const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(
                    TEXT("DevSandbox"), Slot, DevGrantRarity, ItemLevel, Seed);
                Equip->AddToBackpack(Item);
                if (Character.IsValid()) Character->SaveGameState();
                DevSandboxStatus = FText::FromString(FString::Printf(
                    TEXT("GRANTED %s %s (ILVL %d, SEED %d) TO BACKPACK — SEE GEAR SCREEN."),
                    *RarityName(Item.Rarity), *SlotName(Item.Slot), ItemLevel, Seed));
                Rebuild(EBreakerMenuScreen::DevSandbox);
                return FReply::Handled();
            }), true)
        ]
    ];

    // ---- 5. Aggregate readout -------------------------------------------
    // Read-only, and rebuilt by the click that changed it — every mutating
    // control on this screen ends in Rebuild, so these lines are exactly as
    // fresh as the state they describe with nothing polling per frame.
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)[SettingsSectionHeader(TEXT("COMPOSED STATS"))];
    if (Attributes)
    {
        auto AddStatRow = [&Body](const FString& Label, const FString& Value)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(SettingsLabelWidth).HAlign(HAlign_Fill)
                    [
                        MenuText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, true)
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    MenuValueColumn(FText::FromString(Value), 140.0f, BreakerUI::TypeCaption, BreakerUI::TextPrimary)
                ]
            ];
        };
        // O54: two lanes, two rows. One row printing the weapon lane and calling
        // it "damage" is what the sheet said before the split, and it would now
        // be a screen that hides half of what a build composes — an ability
        // build's whole offence would read as whatever its weapon lane happened
        // to inherit from the shared pool.
        AddStatRow(TEXT("WEAPON DAMAGE"), FString::Printf(TEXT("x%.3f"), Attributes->GetDamageMultiplier()));
        AddStatRow(TEXT("ABILITY DAMAGE"), FString::Printf(TEXT("x%.3f"), Attributes->GetAbilityDamageMultiplier()));
        AddStatRow(TEXT("CRIT CHANCE"), FString::Printf(TEXT("%.1f%%"), Attributes->GetCriticalChance() * 100.0f));
        AddStatRow(TEXT("CRIT MULTIPLIER"), FString::Printf(TEXT("x%.2f"), Attributes->GetCriticalMultiplier()));
        AddStatRow(TEXT("MOVE SPEED"), FString::Printf(TEXT("%.0f cm/s"), Attributes->GetMoveSpeed()));
        AddStatRow(TEXT("ARMOR"), FString::Printf(TEXT("%.0f"), Attributes->GetArmor()));
        AddStatRow(TEXT("HEALTH"), FString::Printf(TEXT("%.0f / %.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth()));
        AddStatRow(TEXT("SHIELD"), FString::Printf(TEXT("%.0f / %.0f"), Attributes->GetShield(), Attributes->GetMaxShield()));
        AddStatRow(TEXT("FIRE RATE MULT"), FString::Printf(TEXT("x%.2f"), Attributes->GetFireRateMultiplier()));
    }
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            MenuText(FText::FromString(TEXT("NO ATTRIBUTE SET ON THIS PAWN.")), BreakerUI::TypeCaption, Harm, true)
        ];
    }

    // ---- Status + back ---------------------------------------------------
    // Fixed-height status slot so a result landing cannot reflow the plate.
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        SNew(SBox).HeightOverride(20.0f)
        [
            MenuText(DevSandboxStatus, BreakerUI::TypeCaption, Amber, true)
        ]
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(240.0f)
        [
            MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)
        ]
    ];

    // Settings-width plate, clamped to the viewport the same way — the widest
    // row here is the nine-slot chip pack, which is measured against exactly
    // this width above.
    const float PanelWidth = FMath::Min(1040.0f, MeasureWideScreen().PanelWidth);
    return BuildFrame(FText::FromString(TEXT("BREAKPOINT SANDBOX")),
        FText::FromString(TEXT("DEV — LEVEL / AREA / CLASS / SEEDED GEAR / COMPOSED STATS")), Body, PanelWidth);
}

FReply SBreakerMenu::GoBack()
{
    Rebuild(RootScreen);
    return FReply::Handled();
}

// ==========================================================================
// THE CHARACTER SHEET (C).
//
// Owner ask: a place to gauge current DPS by weapon or by ability, the
// miscellaneous modifiers acting on a character, and the defensive picture --
// effective health pool and what the character is currently afflicted by.
//
// EVERY DERIVED NUMBER ON THIS SCREEN COMES FROM BreakerSheet, which is
// world-free and asserted by RiorsEdge.UI.CharacterSheet.Math. The builder
// below reads components and formats strings; it computes nothing. A sheet
// that prints a wrong number is worse than no sheet -- it is an instrument
// returning a false negative, and the next reader files a bug against working
// code.
//
// WHAT IS NOT HERE IS STATED, NOT OMITTED. Per-element resistance, block and
// dodge are designed and are not attributes yet, so the defence panel says so
// rather than printing a zero that reads as "you have none".
// ==========================================================================
TSharedRef<SWidget> SBreakerMenu::BuildCharacterSheetScreen()
{
    const ABreakerCharacter* Pawn = Character.Get();
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    // THE WRAP WIDTH IS COMPUTED, NOT CHOSEN. art-and-ui bans auto-wrapping
    // where the width matters and bans sizing a fixed box to its shortest
    // label; the first pass of this screen broke both, clipping MODIFIERS to
    // "MODIFIER", AFFLICTIONS to "AFFLICTIO" and two notes mid-word. This is
    // BuildFrame's own arithmetic -- panel width, less the identity rail, less
    // MakePlate's two margins -- so it cannot drift from the frame it sits in.
    constexpr float SheetPanelWidth = 900.0f;
    const float SheetContentWidth = SheetPanelWidth - BreakerUI::RailThickness - BreakerUI::Space24 * 2.0f;
    auto WrapNote = [SheetContentWidth](const TSharedRef<STextBlock>& Text)
    {
        Text->SetWrapTextAt(SheetContentWidth);
        return StaticCastSharedRef<SWidget>(Text);
    };

    // --- Panel picker -----------------------------------------------------
    static const TCHAR* TabNames[] = { TEXT("OFFENCE"), TEXT("DEFENCE"), TEXT("MODIFIERS"), TEXT("AFFLICTIONS") };
    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const int32 Captured = Index;
        const bool bActive = CharacterSheetTab == Index;
        Tabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            SNew(SBox).HeightOverride(BreakerUI::MinHitTarget)
            [
                MakeButton(FText::FromString(TabNames[Index]),
                    FOnClicked::CreateLambda([this, Captured]()
                    {
                        CharacterSheetTab = Captured;
                        Rebuild(EBreakerMenuScreen::CharacterSheet);
                        return FReply::Handled();
                    }), bActive)
            ]
        ];
    }
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)[Tabs];

    // One row shape for the whole screen: label left, value right, muted
    // caption underneath when the number needs a qualifier. Everything is a
    // row, so nothing on this screen can invent its own alignment.
    auto Row = [this, SheetContentWidth](const FString& Label, const FString& Value, const FLinearColor& ValueColor, const FString& Note)
    {
        TSharedRef<SVerticalBox> Cell = SNew(SVerticalBox);
        Cell->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(Label), BreakerUI::TypeCaption, BreakerUI::TextSecondary, false)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(Value), BreakerUI::TypeBody, ValueColor, true)
            ]
        ];
        if (!Note.IsEmpty())
        {
            TSharedRef<STextBlock> NoteText = MenuText(FText::FromString(Note), BreakerUI::TypeCaption, BreakerUI::TextMuted, false);
            NoteText->SetWrapTextAt(SheetContentWidth);
            Cell->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)[NoteText];
        }
        return StaticCastSharedRef<SWidget>(Cell);
    };

    if (!Pawn)
    {
        Body->AddSlot().AutoHeight()
        [
            MenuText(FText::FromString(TEXT("NO CHARACTER.")), BreakerUI::TypeBody, BreakerUI::Orange, true)
        ];
        return BuildFrame(FText::FromString(TEXT("CHARACTER")),
            FText::FromString(TEXT("WHAT THIS BUILD IS ACTUALLY DOING")), Body, 900.0f);
    }

    const UBreakerAttributeSet* Attributes = Pawn->GetAttributes();
    const UBreakerWeaponComponent* Weapon = Pawn->GetWeapon();
    // Pellets, magazine and reload are the DEFINITION's, not the component's:
    // the component reports live state, the definition reports the weapon.
    const UBreakerWeaponDefinition* WeaponDef = Weapon ? Weapon->GetActiveDefinition() : nullptr;

    // ---------------------------------------------------------------- OFFENCE
    if (CharacterSheetTab == 0)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[SettingsSectionHeader(TEXT("WEAPON"))];
        if (Weapon && Attributes)
        {
            const float Base = Weapon->GetScaledBaseDamage();
            const int32 Pellets = WeaponDef ? FMath::Max(1, WeaponDef->PelletsPerShot) : 1;
            const float Increased = Attributes->GetDamageMultiplier();
            const float Rpm = Weapon->GetEffectiveRoundsPerMinute(WeaponDef);
            const float CritChance = Attributes->GetCriticalChance() * 0.01f;
            const float CritMulti = Attributes->GetCriticalMultiplier();
            const float Shot = BreakerSheet::ShotDamage(Base, Pellets, Increased);
            const float Crit = BreakerSheet::CritFactor(CritChance, CritMulti);
            const float Burst = BreakerSheet::BurstDps(Shot, Crit, Rpm);
            const int32 Magazine = FMath::Max(1, Weapon->GetEffectiveMagazineSize());
            const float Sustained = BreakerSheet::SustainedDps(Shot, Crit, Rpm, Magazine,
                WeaponDef ? WeaponDef->ReloadDuration : 0.0f);

            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space12)
            [
                Row(TEXT("SUSTAINED DPS"), BreakerUI::FormatDamage(Sustained), BreakerUI::Gold,
                    TEXT("A magazine, then a reload, forever. This is the one that describes play."))
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space12)
            [
                Row(TEXT("BURST DPS"), BreakerUI::FormatDamage(Burst), BreakerUI::TextPrimary,
                    TEXT("The gun never stops firing. Flatters the build; kept beside sustained so it cannot be mistaken for it."))
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(*Weapon->GetArchetypeName().ToUpper(),
                    FString::Printf(TEXT("i%d"), Weapon->GetEquippedItemLevel()), BreakerUI::TextSecondary, FString())
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(TEXT("DAMAGE PER SHOT"),
                    Pellets > 1
                        ? FString::Printf(TEXT("%s  (%d x %s)"), *BreakerUI::FormatDamage(Shot), Pellets, *BreakerUI::FormatDamage(Base * Increased))
                        : BreakerUI::FormatDamage(Shot),
                    BreakerUI::TextPrimary, FString())
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(TEXT("ROUNDS PER MINUTE"), FString::Printf(TEXT("%.0f"), Rpm), BreakerUI::TextPrimary, FString())
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(TEXT("CRITICAL"),
                    FString::Printf(TEXT("%.1f%%  x%.2f"), Attributes->GetCriticalChance(), CritMulti),
                    BreakerUI::TextPrimary,
                    FString::Printf(TEXT("Expected contribution x%.3f. Every DPS figure above is the EXPECTATION, never the crit."), Crit))
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
            [
                Row(TEXT("INCREASED WEAPON DAMAGE"), FString::Printf(TEXT("x%.3f"), Increased), BreakerUI::TextPrimary, FString())
            ];
        }
        else
        {
            Body->AddSlot().AutoHeight()[MenuText(FText::FromString(TEXT("NO WEAPON EQUIPPED.")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)];
        }

        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[SettingsSectionHeader(TEXT("ABILITY"))];
        if (Attributes)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(TEXT("INCREASED ABILITY DAMAGE"),
                    FString::Printf(TEXT("x%.3f"), Attributes->GetAbilityDamageMultiplier()), BreakerUI::TextPrimary,
                    TEXT("Abilities compose their own additive pool plus the shared one, and ride gear depth through the equipped weapon's item level."))
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(TEXT("DAMAGE OVER TIME"),
                    FString::Printf(TEXT("x%.3f"), Attributes->GetDamageOverTimeMultiplier()), BreakerUI::TextPrimary, FString())
            ];
            // A per-ability DPS row needs each ability to publish a base and a
            // cadence, which no ability does today. Recorded rather than faked:
            // a plausible number here would be the exact false negative this
            // screen exists to prevent.
            Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
            [
                WrapNote(MenuText(FText::FromString(TEXT("PER-ABILITY DPS IS NOT BUILT. No ability publishes a base damage and a cadence, so a number here would be invented. The multipliers above are real.")),
                    BreakerUI::TypeCaption, BreakerUI::Orange, false))
            ];
        }
    }
    // ---------------------------------------------------------------- DEFENCE
    else if (CharacterSheetTab == 1)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[SettingsSectionHeader(TEXT("EFFECTIVE HEALTH"))];
        if (Attributes)
        {
            const float Armor = Attributes->GetArmor();
            const float Mitigation = BreakerSheet::ArmourMitigation(Armor);
            const float Health = Attributes->GetHealth();
            const float Shield = Attributes->GetShield();
            const float Ehp = BreakerSheet::EffectiveHealthPool(Health, Shield, Mitigation);

            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space12)
            [
                Row(TEXT("EFFECTIVE HEALTH POOL"), BreakerUI::FormatDamage(Ehp), BreakerUI::Cyan,
                    TEXT("Raw incoming damage the pool absorbs once armour is applied. Health plus shield, both behind the same mitigation step."))
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(TEXT("HEALTH"), FString::Printf(TEXT("%s / %s"), *BreakerUI::FormatTicker(Health), *BreakerUI::FormatTicker(Attributes->GetMaxHealth())), BreakerUI::RarityStandard, FString())
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(TEXT("SHIELD"), FString::Printf(TEXT("%s / %s"), *BreakerUI::FormatTicker(Shield), *BreakerUI::FormatTicker(Attributes->GetMaxShield())), BreakerUI::Cyan, FString())
            ];
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
            [
                Row(TEXT("ARMOUR"), FString::Printf(TEXT("%.0f   %.1f%%"), Armor, Mitigation * 100.0f), BreakerUI::TextPrimary,
                    TEXT("Mitigation is capped at 80%. Facing selects the base value, so a rear arc reads differently from a frontal one."))
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[SettingsSectionHeader(TEXT("NOT BUILT"))];
        Body->AddSlot().AutoHeight()
        [
            WrapNote(MenuText(FText::FromString(TEXT("Per-element resistance, ailment avoidance, passive block and passive dodge are designed and are not attributes yet. They are named here rather than printed as zero, because a zero on this screen reads as \"you have none\" instead of \"this does not exist\".")),
                BreakerUI::TypeCaption, BreakerUI::Orange, false))
        ];
    }
    // -------------------------------------------------------------- MODIFIERS
    else if (CharacterSheetTab == 2)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[SettingsSectionHeader(TEXT("EVERYTHING ELSE ACTING ON THIS CHARACTER"))];
        if (Attributes)
        {
            struct FModRow { const TCHAR* Label; float Value; bool bMultiplier; };
            const FModRow Rows[] = {
                { TEXT("MOVE SPEED"),            Attributes->GetMoveSpeed(),              false },
                { TEXT("SLIDE SPEED"),           Attributes->GetSlideSpeedMultiplier(),   true  },
                { TEXT("AIR CONTROL"),           Attributes->GetAirControlMultiplier(),   true  },
                { TEXT("DASH COOLDOWN REDUCTION"), Attributes->GetDashCooldownReduction(), false },
                { TEXT("FIRE RATE"),             Attributes->GetFireRateMultiplier(),     true  },
                { TEXT("RESOURCE COST"),         Attributes->GetResourceCostMultiplier(), true  },
                { TEXT("RESOURCE REGEN"),        Attributes->GetClassResourceRegen(),     false },
                { TEXT("MAX CLASS RESOURCE"),    Attributes->GetMaxClassResource(),       false },
            };
            for (const FModRow& Mod : Rows)
            {
                // A multiplier at exactly 1.000 is doing nothing, and saying so
                // is the point of the panel: the owner's question is "what is
                // affecting this character", and an inert line answers it.
                const bool bInert = Mod.bMultiplier && FMath::IsNearlyEqual(Mod.Value, 1.0f, 0.0005f);
                Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
                [
                    Row(Mod.Label,
                        Mod.bMultiplier ? FString::Printf(TEXT("x%.3f"), Mod.Value) : FString::Printf(TEXT("%.1f"), Mod.Value),
                        bInert ? BreakerUI::TextMuted : BreakerUI::TextPrimary,
                        bInert ? FString(TEXT("inert")) : FString())
                ];
            }
        }
    }
    // ------------------------------------------------------------ AFFLICTIONS
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[SettingsSectionHeader(TEXT("ACTIVE ON THIS CHARACTER"))];
        const UBreakerStatusComponent* Status = Pawn->FindComponentByClass<UBreakerStatusComponent>();
        const TArray<FBreakerActiveStatus> Empty;
        const TArray<FBreakerActiveStatus>& Active = Status ? Status->GetActiveStatuses() : Empty;
        if (Active.Num() == 0)
        {
            Body->AddSlot().AutoHeight()
            [
                MenuText(FText::FromString(TEXT("NOTHING. The character is carrying no statuses.")),
                    BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
        }
        for (const FBreakerActiveStatus& Entry : Active)
        {
            FString Name = Entry.Spec.StatusTag.IsValid() ? Entry.Spec.StatusTag.GetTagName().ToString() : TEXT("STATUS");
            int32 Separator = INDEX_NONE;
            if (Name.FindLastChar(TEXT('.'), Separator)) Name = Name.RightChop(Separator + 1);
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                Row(*Name.ToUpper(),
                    FString::Printf(TEXT("%d  %.1fs"), Entry.Stacks, FMath::Max(Entry.RemainingDuration, 0.0f)),
                    BreakerUI::Harm, FString())
            ];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
    [
        MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)
    ];
    return BuildFrame(FText::FromString(TEXT("CHARACTER")),
        FText::FromString(TEXT("WHAT THIS BUILD IS ACTUALLY DOING")), Body, SheetPanelWidth);
}
