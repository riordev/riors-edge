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
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SCanvas.h"
#include "UI/BreakerSkillProjection.h"
#include "UI/BreakerUIStyle.h"
#include "Engine/Engine.h"
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

    TSharedRef<STextBlock> MenuText(const FText& Text, int32 Size, const FLinearColor& Color = BreakerUI::TextPrimary, bool bBold = false)
    {
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Color)
            .Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size));
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
        return SNew(SBox).WidthOverride(Width).HAlign(HAlign_Fill)
        [
            SNew(STextBlock)
                .Text(Text)
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Color)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size))
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
    if (CurrentScreen == EBreakerMenuScreen::Settings || CurrentScreen == EBreakerMenuScreen::Loadout || CurrentScreen == EBreakerMenuScreen::Inventory || CurrentScreen == EBreakerMenuScreen::ClassSelect || CurrentScreen == EBreakerMenuScreen::SkillTrees || CurrentScreen == EBreakerMenuScreen::Forge || CurrentScreen == EBreakerMenuScreen::Abilities || CurrentScreen == EBreakerMenuScreen::CharacterSelect)
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
        case EBreakerMenuScreen::Loadout: ContentHost->SetContent(BuildLoadoutScreen()); break;
        case EBreakerMenuScreen::Inventory: ContentHost->SetContent(BuildInventoryScreen()); break;
        case EBreakerMenuScreen::ClassSelect: ContentHost->SetContent(BuildClassSelectScreen()); break;
        case EBreakerMenuScreen::CharacterSelect: ContentHost->SetContent(BuildCharacterSelectScreen()); break;
        case EBreakerMenuScreen::CharacterCreate: ContentHost->SetContent(BuildCharacterCreateScreen()); break;
        case EBreakerMenuScreen::SkillTrees: ContentHost->SetContent(BuildSkillTreesScreen()); break;
        case EBreakerMenuScreen::Forge: ContentHost->SetContent(BuildForgeScreen()); break;
        case EBreakerMenuScreen::Abilities: ContentHost->SetContent(BuildAbilitiesScreen()); break;
        case EBreakerMenuScreen::Dialogue: ContentHost->SetContent(BuildDialogueScreen()); break;
        case EBreakerMenuScreen::Travel: ContentHost->SetContent(BuildTravelScreen()); break;
        default: ContentHost->SetContent(BuildMainScreen()); break;
    }
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
            SNew(SBox).WidthOverride(PanelWidth).HeightOverride(MeasureWideScreen().PanelHeight)
            [
                // The screen plate carries the cyan identity rail: the front
                // end belongs to the player/system family.
                MakePlate(PanelContent, Panel, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space24))
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

    // bFillHeight claims the whole PanelHeight instead of shrinking to the
    // body's desired height. The skill matrix needs it: its board is a
    // VIEWPORT onto a larger surface, so it deliberately has no desired height
    // of its own, and a plate that shrink-wrapped it would collapse to the
    // height of the branch strip. The loadout still shrink-wraps, which is why
    // this is a parameter and not a change of rule.
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
            Plate
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
    AddTab(TEXT("FORGE"), EBreakerMenuScreen::Forge);
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
            MenuText(FText::FromString(TEXT("MOVEMENT-DRIVEN ARPG LOOTER SHOOTER")), 11, SoftText)
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
        MenuText(FText::FromString(TEXT("MOVEMENT-DRIVEN ARPG LOOTER SHOOTER")), 11, SoftText)
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
    AddButton(MakeButton(FText::FromString(TEXT("LOADOUT")), FOnClicked::CreateLambda([this]()
    {
        Rebuild(EBreakerMenuScreen::Loadout);
        return FReply::Handled();
    })));
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
    // A section header: cyan caption over a 1px divider. Structure comes off
    // borders in this system, not off gaps.
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

    // Fixed label column, so every control on the screen starts on the same
    // vertical edge whatever its label says. 210 clears the longest label the
    // screen carries ("SENSITIVITY DOWN", "SCOPED SENSITIVITY") at TypeBody
    // bold, and is chosen against the KEYBIND row's total width rather than
    // against the setting rows — the keybind row is the one with five columns
    // and therefore the one that decides how much anything else may have.
    constexpr float SettingsLabelWidth = 210.0f;
    // Fixed readout column. HAlign_Fill + right justification inside, via
    // MenuValueColumn, which is the project's fix for the clipped-number
    // defect.
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

    // A choice strip: N fixed-width chips, exactly one selected. Used by window
    // mode and by the frame cap, which are both small closed sets — a
    // frame-cap SLIDER would have to represent "uncapped" as a magic zero at
    // one end of a continuum, and a player dragging past 30 would land on it by
    // accident. ClampFrameRateCap treats 0 as a sentinel precisely because it
    // is a separate CHOICE, so it is offered as one.
    constexpr float SettingsChipWidth = 132.0f;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsInputSection()
{
    UBreakerGameSettings* Model = GameSettings.Get();
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    Section->AddSlot().AutoHeight()[SettingsSectionHeader(TEXT("INPUT"))];
    if (!Model) return Section;

    // Live readouts are held as widget handles and written imperatively from
    // the slider's own OnValueChanged. NOT a Text_Lambda: a per-frame text
    // attribute is banned in this file, and it is not needed — a slider that
    // moves fires an event, and an event is exactly when the number changes.
    // Each one sits in a fixed-width column so a value going from "1.0" to
    // "1.25" cannot reflow the row.
    //
    // The readout widget is built FIRST and the handle captured BY VALUE. Both
    // matter: the handle is captured into a lambda that outlives this function,
    // and the order in which a compiler evaluates two arguments to the same
    // call is unspecified — a by-reference capture of a handle SAssignNew has
    // not filled in yet would be a dangling reference on some builds and fine
    // on others, which is the worst kind of both.
    TSharedPtr<STextBlock> SensitivityReadout;
    const TSharedRef<SWidget> SensitivityValue =
        SNew(SBox).WidthOverride(SettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(SensitivityReadout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.2f"), Model->MouseSensitivity)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Cyan)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
        ];

    TSharedPtr<STextBlock> ScopedReadout;
    const TSharedRef<SWidget> ScopedValue =
        SNew(SBox).WidthOverride(SettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(ScopedReadout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.2fx"), Model->ScopedSensitivityMultiplier)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Cyan)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
        ];

    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        SettingsRow(TEXT("LOOK SENSITIVITY"),
            SNew(SSlider)
            // 0.2 .. 2.0, the range ClampMouseSensitivity enforces
            // (Settings/BreakerGameSettings.cpp:12-15) and the range the
            // previous screen's slider already spanned.
            .Value((Model->MouseSensitivity - 0.2f) / 1.8f)
            .OnValueChanged_Lambda([this, SensitivityReadout](float Value)
            {
                UBreakerGameSettings* Live = GameSettings.Get();
                if (!Live) return;
                Live->MouseSensitivity = UBreakerGameSettingsLibrary::ClampMouseSensitivity(0.2f + Value * 1.8f);
                // The live pawn keeps its own copy of these three; pushing them
                // through ApplyMenuSettings is what makes the slider FELT
                // rather than merely stored.
                if (Character.IsValid())
                {
                    Character->ApplyMenuSettings(Live->MouseSensitivity, Live->FieldOfView, Live->bInvertVerticalLook);
                }
                if (SensitivityReadout.IsValid())
                {
                    SensitivityReadout->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Live->MouseSensitivity)));
                }
            })
            // Saved on RELEASE, not per drag frame: a drag fires this handler
            // dozens of times a second and Save() flushes an ini file.
            .OnMouseCaptureEnd_Lambda([this]() { if (GameSettings.IsValid()) GameSettings->Save(); }),
            SensitivityValue)
    ];

    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
    [
        SettingsRow(TEXT("SCOPED SENSITIVITY"),
            SNew(SSlider)
            // 0.1 .. 3.0 (ClampScopedSensitivityMultiplier,
            // BreakerGameSettings.cpp:17-20).
            .Value((Model->ScopedSensitivityMultiplier - 0.1f) / 2.9f)
            .OnValueChanged_Lambda([this, ScopedReadout](float Value)
            {
                UBreakerGameSettings* Live = GameSettings.Get();
                if (!Live) return;
                Live->ScopedSensitivityMultiplier =
                    UBreakerGameSettingsLibrary::ClampScopedSensitivityMultiplier(0.1f + Value * 2.9f);
                if (ScopedReadout.IsValid())
                {
                    ScopedReadout->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Live->ScopedSensitivityMultiplier)));
                }
            })
            .OnMouseCaptureEnd_Lambda([this]() { if (GameSettings.IsValid()) GameSettings->Save(); }),
            ScopedValue)
    ];
    // Said plainly on the screen rather than left for a player to discover by
    // dragging it and feeling nothing. The multiplier persists and clamps; the
    // aiming path does not read it yet.
    Section->AddSlot().AutoHeight().Padding(SettingsLabelWidth, 0.0f, 0.0f, BreakerUI::Space16)
    [
        MenuText(FText::FromString(TEXT("MULTIPLIER OF LOOK SENSITIVITY WHILE AIMING. SAVED; NOT YET READ BY THE AIM PATH.")),
            BreakerUI::TypeCaption, Muted)
    ];

    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        SNew(SCheckBox)
        .IsChecked(Model->bInvertVerticalLook ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
        {
            UBreakerGameSettings* Live = GameSettings.Get();
            if (!Live) return;
            Live->bInvertVerticalLook = State == ECheckBoxState::Checked;
            if (Character.IsValid())
            {
                Character->ApplyMenuSettings(Live->MouseSensitivity, Live->FieldOfView, Live->bInvertVerticalLook);
            }
            Live->Save();
        })
        [
            SNew(SBox).Padding(FMargin(BreakerUI::Space8, 0.0f, 0.0f, 0.0f))
            [
                MenuText(FText::FromString(TEXT("INVERT VERTICAL LOOK")), BreakerUI::TypeBody, Primary, true)
            ]
        ]
    ];
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

    // What the key column says.
    FString KeyLabel;
    if (bComposite)
    {
        // Only the keyboard/mouse half, which is also what stopped this column
        // overflowing: joining every default including the gamepad names
        // produced strings like "GAMEPAD LEFT THUMBSTICK 2D-AXIS  MOUSE XY
        // 2D-AXIS" and the row clipped them on the left.
        for (const FKey& Key : DeskDefaults)
        {
            if (!KeyLabel.IsEmpty()) KeyLabel += TEXT("  ");
            KeyLabel += Key.GetDisplayName(false).ToString().ToUpper();
        }
    }
    else if (Resolved.IsValid())
    {
        KeyLabel = Resolved.GetDisplayName().ToString().ToUpper();
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

    // ---- THE KEY CONTROL, and its width budget ---------------------------
    // The key display IS the button now. There is no separate BIND column:
    // owner, "this should show the current bind you click and replace it this
    // is ugly". So one control per row carries three states in the same box —
    // the current key at rest, "PRESS A KEY…" while listening, and the conflict
    // confirm — and the box is sized to the longest of them.
    //
    // TRAP 1, reported FOUR times in this file: a fixed box carrying a CHANGING
    // label is sized to the LONGEST string it can ever hold, never to the one
    // it happens to show first. Every candidate, measured at its own type size
    // against Roboto Bold (~0.65em per uppercase glyph, so ~9.1px at TypeBody
    // and ~7.2px at TypeCaption):
    //
    //   "MIDDLE MOUSE BUTTON"  19 glyphs, TypeBody    ~173 + 34 chrome = 207
    //   "RIGHT MOUSE BUTTON"   18 glyphs, TypeBody    ~164 + 34 chrome = 198
    //   "BIND ANYWAY"          11 glyphs, TypeBody    ~100 + 34 chrome = 134
    //   "PRESS A KEY…"         12 glyphs, TypeBody    ~110 + 34 chrome = 144
    //   "W  S  A  D  UP  DOWN  RIGHT  LEFT"
    //                          33 glyphs, TypeCaption ~237, no chrome  = 237
    //
    // The chrome is the control's 16px content padding on each side plus its
    // 1px ring on each side. The COMPOSITE string is the widest thing this
    // column ever holds — it is plain text, not a control, so it pays no
    // padding, and it is still 30px wider than the longest single key name.
    // Sizing to the longest SINGLE key is the exact mistake that produced the
    // four clipping reports; 260 clears the composite string with ~23px spare.
    constexpr float KeyControlWidth = 260.0f;
    constexpr float DefaultButtonWidth = 110.0f;
    // ROW BUDGET, re-measured for the four-column layout (the old note said
    // five columns totalling 738 and was stale the moment BIND was deleted):
    //
    //   210 label + 260 key + 16 gap + [badge fills] + 8 gap + 110 default
    //   = 604 of fixed width.
    //
    // The plate is 1040 wide; its interior is roughly 1040 - 3 rail - 2 border
    // - 48 padding - 16 scroll bar = 971. That leaves ~367 for the badge, whose
    // longest string is "SHARED: " plus the longest action label
    // ("SCOPED SENSITIVITY", 18) = 26 glyphs at TypeCaption ~187. Comfortable,
    // and 134px MORE headroom than the five-column layout had — which is the
    // point of deleting a column.

    // What the control says. Rest is the key itself; listening replaces it in
    // place; a pending conflict replaces it with its own confirm.
    FString KeyControlLabel = KeyLabel;
    if (bListening) KeyControlLabel = TEXT("PRESS A KEY…");
    else if (bPending) KeyControlLabel = TEXT("BIND ANYWAY");

    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        SNew(SBox).WidthOverride(SettingsLabelWidth).HAlign(HAlign_Fill)
        [
            MenuText(UBreakerGameSettingsLibrary::DescribeAction(Action), BreakerUI::TypeBody,
                bListening ? Cyan : Primary, true)
        ]
    ];
    // ---- The key column --------------------------------------------------
    // Composite rows are PLAIN TEXT and not clickable: there is nothing a
    // single FKey could replace a two-dimensional axis or four movement keys
    // with, so the row shows what it is bound to and the badge beside it says
    // which of the two reasons applies. Boxed to the control's own height so
    // the rows keep a straight edge either way.
    if (bComposite)
    {
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(KeyControlWidth)
            .HeightOverride(BreakerUI::MinHitTarget + BreakerUI::Space8)
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Center)
            [
                // HAlign_Fill on the box plus justification inside, never
                // HAlign_Left — see MenuValueColumn for why an alignment other
                // than Fill shaves the boundary glyph.
                SNew(STextBlock)
                    .Text(FText::FromString(KeyLabel))
                    .Justification(ETextJustify::Left)
                    .ColorAndOpacity(SoftText)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
            ]
        ];
    }
    else
    {
        // THE CONTROL. Built from BorderWrap + SButton — the same two-part
        // vocabulary MakeButton itself is made of — rather than through
        // MakeButton, because this control needs a THIRD colour state that
        // MakeButton's primary/secondary pair cannot express: an overridden key
        // reads cyan at rest, which is the only tell that a row is off its
        // default other than the DEFAULT button appearing next to it.
        const bool bArmed = bListening || bPending;
        const FLinearColor LabelColor = bArmed
            ? (bPending ? Amber : Cyan)
            : (Resolved.IsValid() ? (bOverridden ? Cyan : Primary) : Disabled);
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(KeyControlWidth)
            .HeightOverride(BreakerUI::MinHitTarget + BreakerUI::Space8)
            [
                BorderWrap(
                    SNew(SButton)
                    .ButtonColorAndOpacity(bArmed ? PanelHover : PanelRaised)
                    .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                    // HAlign_Fill for the clipping reason MakeButton documents.
                    .HAlign(HAlign_Fill)
                    .VAlign(VAlign_Center)
                    .OnClicked(FOnClicked::CreateLambda([this, Action]()
                    {
                        if (PendingKeybindAction == Action && PendingKeybindKey.IsValid())
                        {
                            // CONFLICT CONFIRM, rehomed onto this control. It
                            // used to be the BIND button's third label, and
                            // that button no longer exists; putting it here
                            // rather than on a new adjacent affordance keeps
                            // the row at one clickable thing and matches the
                            // armed-chip pattern already in this file, where a
                            // control swaps its own label to CONFIRM rather
                            // than growing a neighbour.
                            const FKey Confirmed = PendingKeybindKey;
                            PendingKeybindAction = NAME_None;
                            PendingKeybindKey = FKey();
                            if (UBreakerGameSettings* Live = GameSettings.Get())
                            {
                                Live->SetKeybindOverride(Action, Confirmed);
                                Live->Save();
                                KeybindStatus = FText::FromString(FString::Printf(TEXT("%s BOUND TO %s — THE KEY IS NOW SHARED."),
                                    *UBreakerGameSettingsLibrary::DescribeAction(Action).ToString(),
                                    *Confirmed.GetDisplayName().ToString().ToUpper()));
                                bKeybindStatusIsClash = true;
                            }
                            Rebuild(EBreakerMenuScreen::Settings);
                            return FReply::Handled();
                        }
                        BeginKeybindListen(Action);
                        return FReply::Handled();
                    }))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(KeyControlLabel))
                            .Justification(ETextJustify::Left)
                            .ColorAndOpacity(LabelColor)
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
                    ],
                    bArmed ? (bPending ? Amber : Cyan) : BorderEmphasis,
                    bArmed ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
            ]
        ];
    }
    Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
    [
        MenuText(
            bComposite
                // Short, because widening the key column squeezed this trailing slot and
                // the badge clipped in turn. "NOT REBINDABLE" was redundant anyway: the
                // key is drawn as plain text rather than as a control, which says it
                // louder than the words did.
                ? FText::FromString(bAxisBound ? TEXT("ANALOG AXIS") : TEXT("MULTI-KEY"))
                : (ClashWith != NAME_None
                    ? FText::FromString(FString::Printf(TEXT("SHARED: %s"),
                        *UBreakerGameSettingsLibrary::DescribeAction(ClashWith).ToString()))
                    : FText::GetEmpty()),
            BreakerUI::TypeCaption, bComposite ? Muted : Harm, true)
    ];

    if (bComposite)
    {
        // Keeps the column edges straight without pretending a control is
        // there and disabled — there is nothing to disable, and an action with
        // no override has nothing to reset either.
        Row->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(BreakerUI::Space8 + DefaultButtonWidth)];
        return Row;
    }

    Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(DefaultButtonWidth)
        [
            bOverridden
                ? MakeButton(FText::FromString(TEXT("DEFAULT")), FOnClicked::CreateLambda([this, Action]()
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
                  }))
                // An action with no override has nothing to reset, and a live
                // button that does nothing is worse than no button.
                : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(1.0f, 1.0f)))
        ]
    ];
    return Row;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsKeybindSection()
{
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    Section->AddSlot().AutoHeight()[SettingsSectionHeader(TEXT("KEYBINDS"))];

    const TMap<FName, FKey> FlatDefaults = UBreakerGameSettingsLibrary::FirstKeyPerAction(DefaultKeybinds);

    if (DefaultKeybinds.Num() == 0)
    {
        // Said out loud rather than drawn as a list of UNBOUND rows that would
        // read as "this game has no controls".
        Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
        [
            MenuText(FText::FromString(TEXT("NO INPUT CONFIG COULD BE LOADED — DEFAULT KEYS ARE UNKNOWN. REBINDS STILL SAVE.")),
                BreakerUI::TypeCaption, Harm, true)
        ];
    }

    for (const FName& Action : UBreakerGameSettingsLibrary::BindableActionNames())
    {
        Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            MakeKeybindRow(Action, DefaultKeybinds, FlatDefaults)
        ];
    }

    // The status line. One line, always present so the section cannot change
    // height when it populates — an empty FText still reserves the row.
    Section->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, BreakerUI::Space8)
    [
        SNew(SBox).HeightOverride(20.0f)
        [
            MenuText(KeybindStatus, BreakerUI::TypeCaption, bKeybindStatusIsClash ? Harm : SoftText, true)
        ]
    ];

    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(240.0f)
            [
                MakeButton(FText::FromString(TEXT("RESET ALL KEYBINDS")), FOnClicked::CreateLambda([this]()
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
                }))
            ]
        ]
    ];
    // The honest line. The override map is written, clamped, persisted and
    // read back correctly — and nothing in the running game consumes it yet:
    // as of this pass UBreakerGameSettings has no callers outside the settings
    // screen and its own tests. Applying an override live means rewriting the
    // mapping context's entries and asking Enhanced Input to rebuild, which is
    // a change to ABreakerCharacter's input setup
    // (Characters/BreakerCharacter.cpp:239-240, 388-418) and not this screen's
    // to make. Saying so beats a screen that appears to rebind and does not.
    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        MenuText(FText::FromString(TEXT("CLICK A KEY, THEN PRESS THE ONE YOU WANT INSTEAD.  ESC CANCELS.\nREBINDS SAVE TO YOUR PROFILE. LIVE INPUT STILL USES THE DEFAULT KEYS UNTIL THE INPUT PASS READS THEM.")),
            BreakerUI::TypeCaption, Muted)
    ];
    return Section;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsVideoSection()
{
    UBreakerGameSettings* Model = GameSettings.Get();
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    Section->AddSlot().AutoHeight()[SettingsSectionHeader(TEXT("VIDEO"))];
    if (!Model) return Section;

    // Built first, captured by value — see the note in BuildSettingsInputSection.
    TSharedPtr<STextBlock> FOVReadout;
    const TSharedRef<SWidget> FOVValue =
        SNew(SBox).WidthOverride(SettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(FOVReadout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.0f"), Model->FieldOfView)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(Cyan)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
        ];

    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        SettingsRow(TEXT("FIELD OF VIEW"),
            // 70 .. 120 (ClampFOV, BreakerGameSettings.cpp:22-25), the same
            // span the previous screen used and the same one
            // ABreakerCharacter::BaseFieldOfView enforces.
            SNew(SSlider)
            .Value((Model->FieldOfView - 70.0f) / 50.0f)
            .OnValueChanged_Lambda([this, FOVReadout](float Value)
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
            })
            .OnMouseCaptureEnd_Lambda([this]() { if (GameSettings.IsValid()) GameSettings->Save(); }),
            FOVValue)
    ];

    // ---- Window mode -------------------------------------------------------
    TSharedRef<SHorizontalBox> ModeStrip = SNew(SHorizontalBox);
    auto AddModeChip = [this, &ModeStrip, Model](const FString& Label, EBreakerWindowMode Mode)
    {
        const bool bSelected = Model->WindowMode == Mode;
        ModeStrip->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            SNew(SBox).WidthOverride(SettingsChipWidth)
            [
                MakeButton(FText::FromString(Label), FOnClicked::CreateLambda([this, Mode]()
                {
                    if (UBreakerGameSettings* Live = GameSettings.Get())
                    {
                        Live->WindowMode = Mode;
                        Live->Save();
                        // Video is the one group that reaches the engine, and
                        // it does so IMMEDIATELY: a window-mode chip that only
                        // took effect on restart would be indistinguishable
                        // from a broken one.
                        Live->ApplyToEngine();
                    }
                    Rebuild(EBreakerMenuScreen::Settings);
                    return FReply::Handled();
                }), bSelected)
            ]
        ];
    };
    AddModeChip(TEXT("FULLSCREEN"), EBreakerWindowMode::Fullscreen);
    AddModeChip(TEXT("BORDERLESS"), EBreakerWindowMode::BorderlessWindowed);
    AddModeChip(TEXT("WINDOWED"), EBreakerWindowMode::Windowed);
    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        SettingsRow(TEXT("WINDOW MODE"), ModeStrip, SNew(SBox).WidthOverride(SettingsValueWidth))
    ];

    // ---- Frame cap ---------------------------------------------------------
    TSharedRef<SHorizontalBox> CapStrip = SNew(SHorizontalBox);
    auto AddCapChip = [this, &CapStrip, Model](const FString& Label, float Cap)
    {
        const bool bSelected = FMath::IsNearlyEqual(Model->FrameRateCapFPS, Cap);
        CapStrip->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            SNew(SBox).WidthOverride(96.0f)
            [
                MakeButton(FText::FromString(Label), FOnClicked::CreateLambda([this, Cap]()
                {
                    if (UBreakerGameSettings* Live = GameSettings.Get())
                    {
                        Live->FrameRateCapFPS = UBreakerGameSettingsLibrary::ClampFrameRateCap(Cap);
                        Live->Save();
                        Live->ApplyToEngine();
                    }
                    Rebuild(EBreakerMenuScreen::Settings);
                    return FReply::Handled();
                }), bSelected)
            ]
        ];
    };
    // Every offered value is inside ClampFrameRateCap's [30, 360] band, or the
    // 0 sentinel (BreakerGameSettings.cpp:27-33), so no chip on this strip can
    // be silently rewritten by the clamp into a different chip's value.
    AddCapChip(TEXT("NONE"), 0.0f);
    AddCapChip(TEXT("60"), 60.0f);
    AddCapChip(TEXT("120"), 120.0f);
    AddCapChip(TEXT("144"), 144.0f);
    AddCapChip(TEXT("240"), 240.0f);
    AddCapChip(TEXT("360"), 360.0f);
    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        SettingsRow(TEXT("FRAME RATE CAP"), CapStrip, SNew(SBox).WidthOverride(SettingsValueWidth))
    ];

    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        SNew(SCheckBox)
        .IsChecked(Model->bVSyncEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
        {
            if (UBreakerGameSettings* Live = GameSettings.Get())
            {
                Live->bVSyncEnabled = State == ECheckBoxState::Checked;
                Live->Save();
                Live->ApplyToEngine();
            }
        })
        [
            SNew(SBox).Padding(FMargin(BreakerUI::Space8, 0.0f, 0.0f, 0.0f))
            [
                MenuText(FText::FromString(TEXT("VERTICAL SYNC")), BreakerUI::TypeBody, Primary, true)
            ]
        ]
    ];
    return Section;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsAudioSection()
{
    UBreakerGameSettings* Model = GameSettings.Get();
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    Section->AddSlot().AutoHeight()[SettingsSectionHeader(TEXT("AUDIO"))];
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
        const TSharedRef<SWidget> ValueColumn = SNew(SBox).WidthOverride(SettingsValueWidth).HAlign(HAlign_Fill)
        [
            SAssignNew(Readout, STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.0f%%"), Initial * 100.0f)))
                .Justification(ETextJustify::Right)
                .ColorAndOpacity(BreakerUI::Cyan)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
        ];
        Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
        [
            SettingsRow(Label,
                SNew(SSlider)
                .Value(Initial)   // volumes are already 0..1, so no remap
                .OnValueChanged_Lambda([this, Field, Readout](float Value)
                {
                    UBreakerGameSettings* Live = GameSettings.Get();
                    if (!Live) return;
                    Live->*Field = UBreakerGameSettingsLibrary::ClampVolume(Value);
                    if (Readout.IsValid())
                    {
                        Readout->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Live->*Field * 100.0f)));
                    }
                })
                .OnMouseCaptureEnd_Lambda([this]()
                {
                    if (UBreakerGameSettings* Live = GameSettings.Get())
                    {
                        Live->Save();
                        Live->ApplyToEngine();
                    }
                }),
                ValueColumn)
        ];
    };
    AddVolumeRow(TEXT("MASTER VOLUME"), &UBreakerGameSettings::MasterVolume, Model->MasterVolume);
    AddVolumeRow(TEXT("EFFECTS VOLUME"), &UBreakerGameSettings::EffectsVolume, Model->EffectsVolume);
    AddVolumeRow(TEXT("MUSIC VOLUME"), &UBreakerGameSettings::MusicVolume, Model->MusicVolume);

    // The same honesty the model's own header already carries
    // (BreakerGameSettings.cpp:250-261): the project has no audio pipeline, so
    // all three values persist and route nowhere. A player who drags these and
    // hears nothing should be told why on the screen, not left to conclude the
    // sliders are broken.
    Section->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        MenuText(FText::FromString(TEXT("SAVED. THE GAME HAS NO AUDIO YET — THESE TAKE EFFECT THE DAY SOUND EXISTS.")),
            BreakerUI::TypeCaption, Muted)
    ];
    return Section;
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsScreen()
{
    EnsureSettingsLoaded();

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight()[BuildSettingsInputSection()];
    Body->AddSlot().AutoHeight()[BuildSettingsKeybindSection()];
    Body->AddSlot().AutoHeight()[BuildSettingsVideoSection()];
    Body->AddSlot().AutoHeight()[BuildSettingsAudioSection()];

    Body->AddSlot().AutoHeight()
    [
        SNew(SBox).WidthOverride(240.0f)
        [
            MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)
        ]
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("CHANGES SAVE IMMEDIATELY  |  ESC BACK")), BreakerUI::TypeCaption, SoftText)
    ];

    // Wider than the 720 default: the keybind rows carry four columns
    // (action, the key control, badge, DEFAULT) whose fixed widths and gaps
    // total 604, and the badge needs the rest — see the budget note in
    // MakeKeybindRow. Deleting the BIND column bought back 134px of that
    // budget; the plate stays at 1040 because the badge is what was tight, not
    // the plate. BuildFrame
    // is still the fixed-height plate with the scrolling body: the four
    // sections are far taller than any viewport, and a plate that grew to fit
    // them is trap 2 in this file, not a fix.
    //
    // Clamped to the viewport the same way the wide screens are: 1040 is the
    // width the rows were laid out against, not a width the window is promised
    // to have, and a plate wider than the window runs its right-hand columns
    // off the edge — which is exactly how the tab strip's labels were lost
    // once already (see BuildScreenTabs).
    const float SettingsPanelWidth = FMath::Min(1040.0f, MeasureWideScreen().PanelWidth);
    return BuildFrame(FText::FromString(TEXT("SETTINGS")),
        FText::FromString(TEXT("INPUT / KEYBINDS / VIDEO / AUDIO")), Body, SettingsPanelWidth);
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
    ListeningKeybindAction = NAME_None;
    if (Action == NAME_None) return;

    UBreakerGameSettings* Model = GameSettings.Get();
    if (!Model || !Key.IsValid())
    {
        KeybindStatus = FText::FromString(TEXT("THAT INPUT CANNOT BE BOUND."));
        bKeybindStatusIsClash = true;
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
        // So the clash is reported with the action that owns the key, the row
        // arms, and the player's second click is what decides.
        PendingKeybindAction = Action;
        PendingKeybindKey = Key;
        KeybindStatus = FText::FromString(FString::Printf(TEXT("%s IS ALREADY %s. CLICK BIND ANYWAY TO SHARE IT, OR REBIND %s FIRST."),
            *Key.GetDisplayName().ToString().ToUpper(),
            *UBreakerGameSettingsLibrary::DescribeAction(ClashWith).ToString(),
            *UBreakerGameSettingsLibrary::DescribeAction(ClashWith).ToString()));
        bKeybindStatusIsClash = true;
        Rebuild(EBreakerMenuScreen::Settings);
        return;
    }

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

TSharedRef<SWidget> SBreakerMenu::BuildLoadoutScreen()
{
    UBreakerWeaponComponent* Weapon = Character.IsValid() ? Character->GetWeapon() : nullptr;

    struct FArchetypeEntry { EBreakerWeaponArchetype Archetype; const TCHAR* Name; const TCHAR* Details; };
    static const FArchetypeEntry Archetypes[] =
    {
        { EBreakerWeaponArchetype::Rifle,   TEXT("RIFLE"),   TEXT("AUTOMATIC  |  30 ROUNDS  |  MID-RANGE") },
        { EBreakerWeaponArchetype::SMG,     TEXT("SMG"),     TEXT("AUTOMATIC  |  35 ROUNDS  |  CLOSE-MID, HIGH CADENCE") },
        { EBreakerWeaponArchetype::Sniper,  TEXT("SNIPER"),  TEXT("SEMI-AUTOMATIC  |  8 ROUNDS  |  LONG-RANGE") },
        { EBreakerWeaponArchetype::Shotgun, TEXT("SHOTGUN"), TEXT("SEMI-AUTOMATIC  |  8 SHELLS  |  CLOSE-RANGE") },
        { EBreakerWeaponArchetype::Rocket,  TEXT("ROCKET"),  TEXT("PROJECTILE  |  4 ROCKETS  |  AREA DAMAGE") },
        // O27 breadth pass. A row here is the ONLY way an archetype is
        // reachable from the loadout screen; a new weapon with no row is
        // content that exists and cannot be picked.
        { EBreakerWeaponArchetype::BurstRifle, TEXT("BURST RIFLE"),  TEXT("3-ROUND BURST  |  27 ROUNDS  |  MID-LONG, DISCIPLINE") },
        { EBreakerWeaponArchetype::Machinegun, TEXT("MACHINEGUN"), TEXT("AUTOMATIC  |  120 ROUNDS  |  SUSTAINED, PLANTED") },
        { EBreakerWeaponArchetype::Sidearm,    TEXT("SIDEARM"),    TEXT("SEMI-AUTOMATIC  |  14 ROUNDS  |  FAST SWAP, DEEP RESERVE") },
    };

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    for (int32 SlotNumber = 1; SlotNumber <= 2; ++SlotNumber)
    {
        const EBreakerWeaponArchetype Assigned = Weapon ? Weapon->GetSlotArchetype(SlotNumber) : EBreakerWeaponArchetype::Rifle;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("SLOT %d — click an archetype to assign"), SlotNumber)), 11, Cyan, true)
        ];
        // FOUR per row, not eight. The O27 breadth pass took this row from five
        // archetypes to eight while every tile still split one 880px panel, so
        // "BURST RIFLE" and "MACHINEGUN" were drawn into ~90px and came out as
        // "BURS" and "MACI" — the screen was clipping its own weapon names.
        constexpr int32 ArchetypesPerRow = 4;
        TSharedRef<SVerticalBox> RowStack = SNew(SVerticalBox);
        TSharedPtr<SHorizontalBox> RowBox;
        int32 ArchetypeIndex = 0;
        for (const FArchetypeEntry& Entry : Archetypes)
        {
            if (ArchetypeIndex % ArchetypesPerRow == 0)
            {
                RowBox = SNew(SHorizontalBox);
                RowStack->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[RowBox.ToSharedRef()];
            }
            ++ArchetypeIndex;
            const bool bAssigned = Entry.Archetype == Assigned;
            const EBreakerWeaponArchetype CapturedArchetype = Entry.Archetype;
            const int32 CapturedSlot = SlotNumber;
            RowBox->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(SBox).HeightOverride(64.0f)
                [
                    // Assigned carries the accent ring, not an accent fill:
                    // a solid cyan tile would outrank the screen title.
                    BorderWrap(
                        SNew(SButton)
                        .ButtonColorAndOpacity(bAssigned ? PanelHover : Panel)
                        .HAlign(HAlign_Center).VAlign(VAlign_Center)
                        .OnClicked(FOnClicked::CreateLambda([this, CapturedSlot, CapturedArchetype]()
                        {
                            if (Character.IsValid() && Character->GetWeapon()) Character->GetWeapon()->SetSlotArchetype(CapturedSlot, CapturedArchetype);
                            Rebuild(EBreakerMenuScreen::Loadout);
                            return FReply::Handled();
                        }))
                        [
                            MenuText(FText::FromString(Entry.Name), BreakerUI::TypeH2, bAssigned ? Primary : SoftText, true)
                        ],
                        // Weapons are the orange family; the assigned slot says so.
                        bAssigned ? BreakerUI::Orange : BorderEmphasis,
                        bAssigned ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
                ]
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)[RowStack];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("ARMORY REFERENCE")), 12, SoftText, true)];
    {
        FString Reference;
        for (const FArchetypeEntry& Entry : Archetypes)
        {
            Reference += FString::Printf(TEXT("%-8s  %s\n"), Entry.Name, Entry.Details);
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[MenuText(FText::FromString(Reference), 10, SoftText)];
    }
    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[MenuText(FText::FromString(TEXT("Two equipped weapons maximum  |  ESC Back")), 9, SoftText)];
    // 1040, not 880: four H2 tiles per row need ~240 each before MACHINEGUN
    // loses its last letter, and a weapon that cannot say its own name is not
    // pickable in any useful sense.
    return BuildFrame(FText::FromString(TEXT("LOADOUT")), FText::FromString(TEXT("WEAPON SLOTS / ARMORY")), Body, 1040.0f);
}

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
            case EBreakerItemRarity::Anomalous: return TEXT("ANOMALOUS");
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

    // One affix as the player reads it: "Movement Speed  +5.0%  T4".
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
        return FString::Printf(TEXT("%s  +%.1f%s  %s"), *Name, Affix.Value,
            bPercent || bPercentStyleFlat ? TEXT("%") : TEXT(""), *TierLabel(Affix.Tier));
    }

    FString DescribeItem(const FBreakerItemInstance& Item)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("ITEM LEVEL %d"), Item.ItemLevel));
        for (const FBreakerRolledAffix& Affix : Item.Affixes)
        {
            Lines.Add(DescribeAffix(Affix));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    // The affix list with per-affix deltas (UI-Inventory-Spec "Card anatomy"
    // line 3): the glyph sits in a fixed column so the affix names keep a
    // straight left edge whether or not a card is being compared.
    //
    // Deltas is UBreakerEquipmentComponent's answer, one row per affix in the
    // same order as Item.Affixes — this function decides nothing about better
    // or worse, it only picks a glyph and a colour. Pass an empty array for a
    // card with nothing to compare against (an equipped piece).
    TSharedRef<SWidget> MakeAffixLines(const FBreakerItemInstance& Item, const TArray<FBreakerAffixComparison>& Deltas)
    {
        TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
        Lines->AddSlot().AutoHeight()
        [
            MenuText(FText::FromString(FString::Printf(TEXT("ITEM LEVEL %d"), Item.ItemLevel)), BreakerUI::TypeCaption, SoftText)
        ];
        for (int32 Index = 0; Index < Item.Affixes.Num(); ++Index)
        {
            FString Glyph;
            FLinearColor GlyphColor = Muted;
            if (Deltas.IsValidIndex(Index))
            {
                switch (Deltas[Index].Delta)
                {
                    case EBreakerAffixDelta::Better: Glyph = BreakerUI::DeltaBetterGlyph; GlyphColor = Cyan; break;
                    case EBreakerAffixDelta::Worse:  Glyph = BreakerUI::DeltaWorseGlyph;  GlyphColor = Harm; break;
                    default:                         Glyph = BreakerUI::DeltaParityGlyph; GlyphColor = Muted; break;
                }
            }
            Lines->AddSlot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(BreakerUI::DeltaGlyphColumn)
                    [
                        MenuText(FText::FromString(Glyph), BreakerUI::TypeCaption, GlyphColor, true)
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    MenuText(FText::FromString(DescribeAffix(Item.Affixes[Index])), BreakerUI::TypeCaption, SoftText)
                ]
            ];
        }
        return Lines;
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
    const float ZoneGutters = 2.0f * BreakerUI::Space24;
    // Spec widths at 1760; scaled down together once the panel cannot hold
    // them plus a usable backpack, floored where the copy stops fitting.
    float CharacterColumnWidth = 560.0f;
    float EquipmentColumnWidth = 400.0f;
    {
        const float MinimumBackpack = 640.0f;   // two 300px cards plus gutters
        const float FixedRoom = Metrics.PanelWidth - ZoneGutters - MinimumBackpack;
        const float SpecFixed = CharacterColumnWidth + EquipmentColumnWidth;
        if (FixedRoom < SpecFixed)
        {
            const float Scale = FMath::Max(0.55f, FixedRoom / SpecFixed);
            CharacterColumnWidth = FMath::Max(320.0f, CharacterColumnWidth * Scale);
            EquipmentColumnWidth = FMath::Max(280.0f, EquipmentColumnWidth * Scale);
        }
    }
    const float BackpackZoneWidth = FMath::Max(320.0f,
        Metrics.PanelWidth - CharacterColumnWidth - EquipmentColumnWidth - ZoneGutters);

    // One-click cards: an equipped slot unequips on click, a backpack item
    // equips on click.
    auto MakeSlotCard = [this, Equipment](EBreakerEquipSlot Slot) -> TSharedRef<SWidget>
    {
        FBreakerItemInstance Item;
        const bool bHasItem = Equipment && Equipment->GetEquippedItem(Slot, Item);
        const FLinearColor Accent = bHasItem ? RarityColor(Item.Rarity) : Disabled;
        // An empty slot keeps its full geometry and its name: the doll never
        // looks broken, only unfinished.
        const FString Name = bHasItem ? RarityName(Item.Rarity) : TEXT("EMPTY");
        const FString Details = bHasItem ? DescribeItem(Item) : TEXT("—");

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
                .ButtonColorAndOpacity(bHasItem ? PanelRaised : Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, Slot]()
                {
                    if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->UnequipSlot(Slot);
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(SlotName(Slot)), BreakerUI::TypeCaption, Muted, true)]
                        + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(Name), BreakerUI::TypeCaption, Accent, true)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(Details), BreakerUI::TypeCaption, bHasItem ? SoftText : Disabled)
                    ]
                ],
                bHasItem ? Item.Rarity : EBreakerItemRarity::Standard, bHasItem)
            ];

        EquipSlotOutlines.Add(Slot, Outline);
        return SNew(SBox).MinDesiredHeight(72.0f).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[Outline];
    };

    // ---- Character column, 560 wide (UI-Inventory-Spec "Zones") -----------
    // Render slot on top, gear totals pinned beneath it so the numbers are
    // always on screen with the doll. The old single printf blob is gone:
    // the spec wants aligned label/value rows with the value coloured by its
    // function family.
    TSharedRef<SVerticalBox> CharacterColumn = SNew(SVerticalBox);
    CharacterColumn->AddSlot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        SNew(SBox).MinDesiredHeight(300.0f)
        [
            // The render slot keeps full geometry while empty: the doll never
            // looks broken, only unfinished.
            MakePlate(
                SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(TEXT("FULL-BODY RENDER SLOT\n\nSILHOUETTE PLACEHOLDER")), BreakerUI::TypeCaption, Muted)
                ],
                BreakerUI::BgRaised, BorderEmphasis, FMargin(BreakerUI::Space16))
        ]
    ];
    {
        TSharedRef<SVerticalBox> Totals = SNew(SVerticalBox);
        Totals->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(TEXT("GEAR TOTALS")), BreakerUI::TypeCaption, Muted, true)
        ];
        auto AddTotalRow = [&Totals](const FString& Label, const FString& Value, const FLinearColor& ValueColor)
        {
            Totals->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    // Fixed value column so the numbers form a straight edge
                    // and never reflow as they tick. Same clipping fix as the
                    // skill rail's totals plate — see MenuValueColumn.
                    MenuValueColumn(FText::FromString(Value), 104.0f, BreakerUI::TypeCaption, ValueColor)
                ]
            ];
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
                AddTotalRow(TEXT("DAMAGE"), FString::Printf(TEXT("x%.2f"), ComposedDamage), BreakerUI::Orange);
            }
            AddTotalRow(TEXT("CRIT CHANCE"), FString::Printf(TEXT("+%.1f%%"), Stats.CriticalChanceBonus * 100.0f), BreakerUI::Orange);
            AddTotalRow(TEXT("CRIT DAMAGE"), FString::Printf(TEXT("+%.1f%%"), Stats.CriticalMultiplierBonus * 100.0f), BreakerUI::Orange);
            AddTotalRow(TEXT("DROP CHANCE"), FString::Printf(TEXT("+%.1f%%"), Stats.DropChancePercent), Amber);
        }
        else
        {
            Totals->AddSlot().AutoHeight()
            [
                MenuText(FText::FromString(TEXT("NO EQUIPMENT COMPONENT")), BreakerUI::TypeCaption, Disabled, true)
            ];
        }
        CharacterColumn->AddSlot().AutoHeight()
        [
            MakePlate(Totals, PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space16))
        ];
    }

    // ---- Equipment column, 400 wide ---------------------------------------
    // Eight slots as full-width rows in wear order — head to foot, then
    // trinkets, then weapons — rather than the old two-column split.
    static const EBreakerEquipSlot WearOrder[] =
    {
        EBreakerEquipSlot::Helmet,
        EBreakerEquipSlot::BodyArmour,
        EBreakerEquipSlot::Gloves,
        EBreakerEquipSlot::Waist,
        EBreakerEquipSlot::Boots,
        EBreakerEquipSlot::Necklace,
        EBreakerEquipSlot::Primary,
        EBreakerEquipSlot::Secondary,
    };
    TSharedRef<SVerticalBox> EquipRows = SNew(SVerticalBox);
    for (const EBreakerEquipSlot Slot : WearOrder)
    {
        EquipRows->AddSlot().AutoHeight()[MakeSlotCard(Slot)];
    }
    TSharedRef<SVerticalBox> EquipmentColumn = SNew(SVerticalBox);
    EquipmentColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        MenuText(FText::FromString(TEXT("EQUIPPED — CLICK TO UNEQUIP")), BreakerUI::TypeCaption, Muted, true)
    ];
    EquipmentColumn->AddSlot().FillHeight(1.0f)
    [
        SNew(SScrollBox) + SScrollBox::Slot()[EquipRows]
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
    // Cards per row is arithmetic on the measured zone, not a frozen 2: a
    // wider window should show more loot, and a narrower one must not push the
    // second card out through the panel edge.
    const float BackpackCardWidth = 300.0f;
    const int32 BackpackCardsPerRow = FMath::Clamp(
        FMath::FloorToInt((BackpackZoneWidth + BreakerUI::Space8) / (BackpackCardWidth + BreakerUI::Space8)), 1, 4);
    if (BackpackSlotFilter >= 0)
    {
        BackpackItems.RemoveAll([this](const FBreakerItemInstance& Item)
        {
            return static_cast<int32>(Item.Slot) != BackpackSlotFilter;
        });
    }
    for (const FBreakerItemInstance& Item : BackpackItems)
    {
        const FGuid ItemId = Item.ItemId;

        // Every consequence of clicking this card, answered by the equipment
        // component before the click. The screen states them; it works none of
        // them out itself.
        const FBreakerEquipPreview Preview = Equipment
            ? Equipment->PreviewEquip(Item)
            : UBreakerEquipmentComponent::PreviewEquipAgainst(TArray<FBreakerItemInstance>(), Item);

        // Footer line one: the ordinary slot swap. Gold means "this costs you
        // something", cyan means the action is free.
        const FString DeltaLine = Preview.bSlotOccupied
            ? FString::Printf(TEXT("EQUIP · REPLACES %s i%d"), *RarityName(Preview.SlotDisplaced.Rarity), Preview.SlotDisplaced.ItemLevel)
            : FString(TEXT("EQUIP · SLOT EMPTY"));
        const FLinearColor DeltaColor = Preview.bSlotOccupied ? Amber : Cyan;

        // Footer line two, only when the rarity cap is already met: a SECOND
        // consequence, so it gets a second line. The action is never blocked —
        // it is disclosed (UI-Inventory-Spec "Limit tells"). Items carry no
        // display name yet, so the ejected piece is named by rarity and slot,
        // which is exactly how its own card is titled.
        const bool bLimitTell = Preview.bExceedsRarityLimit && Preview.LimitDisplaced.IsValid();
        const FString LimitLine = bLimitTell
            ? FString::Printf(TEXT("LIMIT FULL %d/%d · EJECTS %s %s i%d"),
                Preview.RarityCount, Preview.RarityLimit,
                *RarityName(Preview.LimitDisplaced.Rarity), *SlotName(Preview.LimitDisplaced.Slot), Preview.LimitDisplaced.ItemLevel)
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
        // every frame. The count comes from the measured backpack zone above,
        // which is arithmetic done before layout rather than during it.
        if (BackpackCardIndex % BackpackCardsPerRow == 0)
        {
            BackpackRow = SNew(SHorizontalBox);
            BackpackGrid->AddSlot().AutoHeight()[BackpackRow.ToSharedRef()];
        }
        ++BackpackCardIndex;
        BackpackRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 8.0f)
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
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(RarityName(Item.Rarity)), BreakerUI::TypeH2, RarityColor(Item.Rarity), true)]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(BreakerUI::Space8, 0.0f, 22.0f, 0.0f)[MenuText(FText::FromString(FString::Printf(TEXT("i%d"), Item.ItemLevel)), BreakerUI::TypeCaption, Primary, true)]
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                                [
                                    MenuText(FText::FromString(ItemSlotLabel(Item)), BreakerUI::TypeCaption, Muted, true)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                                [
                                    // Line 3 of the card anatomy: every affix
                                    // carrying its delta against the equipped
                                    // piece in this slot.
                                    MakeAffixLines(Item, Preview.AffixDeltas)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                                [
                                    MenuText(FText::FromString(DeltaLine.ToUpper()), BreakerUI::TypeCaption, DeltaColor, true)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                                [
                                    // Harm red, and only present when it is
                                    // true: an always-visible limit line would
                                    // stop meaning anything.
                                    bLimitTell
                                        ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(LimitLine), BreakerUI::TypeCaption, Harm, true))
                                        : SNullWidget::NullWidget
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

    // Slot filter row: ALL plus one chip per equipment slot.
    //
    // The chips are BUILT here and PACKED into rows below, once the bar knows
    // how much width it has. Before this they lived in an SHorizontalBox
    // FillWidth slot beside the input legend, which is two bugs in one place:
    // an overflowing horizontal box does not wrap, it just keeps drawing, so
    // the row ran off the right edge of a 1920 screen AND printed through the
    // legend that shared the slot ("GLOVES / X DISCARD LMB NECKLACE").
    TArray<TSharedRef<SWidget>> FilterChips;
    TArray<float> FilterChipWidths;
    auto AddFilterChip = [this, &FilterChips, &FilterChipWidths](const FString& Label, int32 FilterValue)
    {
        const bool bSelectedChip = BackpackSlotFilter == FilterValue;
        const float ChipBorder = bSelectedChip ? BreakerUI::BorderSelected : BreakerUI::BorderThin;
        FilterChipWidths.Add(MeasureChipWidth(Label, BreakerUI::Space8, ChipBorder));
        FilterChips.Add(
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bSelectedChip ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
                .OnClicked(FOnClicked::CreateLambda([this, FilterValue]()
                {
                    BackpackSlotFilter = FilterValue;
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bSelectedChip ? Primary : Muted, true)
                ],
                bSelectedChip ? Cyan : BorderEmphasis,
                ChipBorder));
    };
    AddFilterChip(TEXT("ALL"), -1);
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        AddFilterChip(SlotName(static_cast<EBreakerEquipSlot>(SlotIndex)), SlotIndex);
    }

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
    TSharedRef<SWidget> EmptyBackpack =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space40, 0.0f, 0.0f)
        [
            MakeRarityBeams()
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("LOOT IS FOUND BY COLOUR")), BreakerUI::TypeH2, Primary, true)
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
        MakeLimitChip(TEXT("ANOMALOUS"), AnomalousEquipped, AnomalousLimit, BreakerUI::RarityAnomalous, true)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[CleanupRow];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    // Meta line. Gear score is the sum of equipped item levels — O2
    // PLACEHOLDER, the shipping formula is not authored yet.
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    int32 GearScore = 0;
    if (Equipment)
    {
        for (const FBreakerItemInstance& EquippedItem : Equipment->GetEquipped())
        {
            if (EquippedItem.IsValid()) GearScore += EquippedItem.ItemLevel;
        }
    }
    const FString MetaLine = FString::Printf(TEXT("BREAKER · %s · LV %d · GEAR SCORE %s"),
        *ClassDisplayName(Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None),
        Progression ? Progression->GetProgressionState().CharacterLevel : 1,
        *BreakerUI::FormatTicker(static_cast<float>(GearScore)));

    // ---- Zones -------------------------------------------------------------
    TSharedRef<SHorizontalBox> Body = SNew(SHorizontalBox);
    Body->AddSlot().AutoWidth()
    [
        SNew(SBox).WidthOverride(CharacterColumnWidth)[CharacterColumn]
    ];
    Body->AddSlot().AutoWidth().Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(EquipmentColumnWidth)[EquipmentColumn]
    ];
    Body->AddSlot().FillWidth(1.0f).Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
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
        Metrics.PanelHeight);

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

    bool bDevClassSwap = false;
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), bDevClassSwap, GGameUserSettingsIni);

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
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
        // O39: outside dev mode a class with no kit is LOCKED — choosing it
        // would permanently strand a character on nothing, since class
        // selection is one-way. Dev mode bypasses this the same way it
        // already bypasses the already-chosen-class lock below.
        const bool bDesignLocked = !bImplemented && !bDevClassSwap;
        const bool bIsCurrent = Entry.ClassId == CurrentClass;
        const bool bSelectable = !bDesignLocked && (CurrentClass == EBreakerClassId::None || bDevClassSwap);
        const EBreakerClassId CapturedClass = Entry.ClassId;
        const bool bCapturedDevSwap = bDevClassSwap;
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
            .OnClicked(FOnClicked::CreateLambda([this, CapturedClass, bCapturedDevSwap, bSelectable]()
            {
                if (!bSelectable) return FReply::Handled();
                if (Character.IsValid() && Character->GetProgression())
                {
                    UBreakerProgressionComponent* Progression = Character->GetProgression();
                    if (bCapturedDevSwap) Progression->DevForceClass(CapturedClass);
                    if (bCapturedDevSwap || Progression->ChoosePermanentClassById(CapturedClass)) Character->SaveGameState();
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

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 10.0f)
    [
        SNew(SCheckBox)
        .IsChecked(bDevClassSwap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
        {
            GConfig->SetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), State == ECheckBoxState::Checked, GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);
            Rebuild(EBreakerMenuScreen::ClassSelect);
        })
        [
            MenuText(FText::FromString(TEXT("DEV MODE — allow class swap (playtest only)")), 11, SoftText, true)
        ]
    ];
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
        return Currency == EBreakerPointCurrency::ClassPoints ? ProgState.UnspentClassPoints : ProgState.UnspentCorePoints;
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

    bool ProgressionRespec(UBreakerProgressionComponent* Progression, EBreakerPointCurrency Currency, FText& OutFailureReason)
    {
        if (!Progression)
        {
            OutFailureReason = FText::FromString(TEXT("No progression component."));
            return false;
        }
        // bIsAtForge is passed true unconditionally: Forge-proximity gating
        // arrives with the hub. Until then respec is always available from
        // the menu so the flow is testable. UI-UX-Spec 6.6 wants the button
        // visible-but-disabled away from a Forge — wire that here when the
        // hub exists.
        return Progression->RespecAtForge(Currency, /*bIsAtForge=*/true, OutFailureReason);
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
        return Currency == EBreakerPointCurrency::ClassPoints ? TEXT("CLASS") : TEXT("CORE");
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
        if (Node->bCornerstone && Progression->GetProgressionState().CommittedBranch != Tree->TreeId)
        {
            if (OutTierGated) *OutTierGated = true;
            return Fail(
                TEXT("COMMIT TO THIS BRANCH TO UNLOCK ITS KEYSTONE"),
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
        if (Tree->Currency == EBreakerPointCurrency::ClassPoints) ClassTrees.Add(Tree);
        else CoreTrees.Add(Tree);
    }
    if (SkillBoardTab == 0 && ClassTrees.IsEmpty() && !CoreTrees.IsEmpty()) SkillBoardTab = 1;
    if (SkillBoardTab == 1 && CoreTrees.IsEmpty() && !ClassTrees.IsEmpty()) SkillBoardTab = 0;
    const bool bCoreBoard = SkillBoardTab == 1;

    const int32 UnspentClass = ProgressionGetUnspent(Progression, EBreakerPointCurrency::ClassPoints);
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

            // Elements is sealed, and reads in suppression teal: the one place
            // teal is legal on this screen, because a rift is a world object,
            // not chrome.
            const FLinearColor Rail = Cluster.bSealed ? BreakerUI::TealHardware : (Cluster.bHub ? Cyan : BorderEmphasis);
            const FLinearColor Border = Cluster.bSealed ? BreakerUI::TealHardware : BreakerUI::BorderRest;

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
                    MenuText(FText::FromString(TEXT("SEALED")), BreakerUI::TypeCaption, BreakerUI::TealHardware, true)
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
                            MenuText(FText::FromString(bThisBranch
                                ? TEXT("COMMITTED — KEYSTONE TIER OPEN")
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

    const EBreakerPointCurrency BoardCurrency = bCoreBoard ? EBreakerPointCurrency::CorePoints : EBreakerPointCurrency::ClassPoints;

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
                if (ProgressionRespec(Prog, BoardCurrency, FailureReason))
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
    FString ForgeCurrencyName(EBreakerForgeCurrency Currency)
    {
        switch (Currency)
        {
            case EBreakerForgeCurrency::Slag:  return TEXT("SLAG");
            case EBreakerForgeCurrency::Flux:  return TEXT("FLUX");
            case EBreakerForgeCurrency::Sigil: return TEXT("SIGIL");
            default:                           return TEXT("?");
        }
    }

    FString DescribeForgeCost(const FBreakerForgeCost& Cost)
    {
        return Cost.IsFree() ? FString(TEXT("FREE")) : FString::Printf(TEXT("%d %s"), Cost.Amount, *ForgeCurrencyName(Cost.Currency));
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
    auto MakeCurrencyChip = [](EBreakerForgeCurrency Currency, int32 Amount) -> TSharedRef<SWidget>
    {
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(ForgeCurrencyName(Currency)), BreakerUI::TypeCaption, Muted, true)]
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(BreakerUI::FormatTicker(static_cast<float>(Amount))), BreakerUI::TypeH2, Primary, true)],
            PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space4));
    };
    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[BuildScreenTabs(EBreakerMenuScreen::Forge)];
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)[MakeCurrencyChip(EBreakerForgeCurrency::Slag, Wallet.Get(EBreakerForgeCurrency::Slag))];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)[MakeCurrencyChip(EBreakerForgeCurrency::Flux, Wallet.Get(EBreakerForgeCurrency::Flux))];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)[MakeCurrencyChip(EBreakerForgeCurrency::Sigil, Wallet.Get(EBreakerForgeCurrency::Sigil))];
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
            // TemperCost returns a deceptive {Slag, 0} — indistinguishable from
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
                MenuText(FText::FromString(FString::Printf(TEXT("SALVAGE — DESTROYS THE ITEM · PAYS %d SLAG · %d FLUX · %d SIGIL"),
                    SalvagePreview.Get(EBreakerForgeCurrency::Slag), SalvagePreview.Get(EBreakerForgeCurrency::Flux), SalvagePreview.Get(EBreakerForgeCurrency::Sigil))),
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
        Metrics.PanelHeight);
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
        Metrics.PanelHeight);
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
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this, NextNodeId, QuestFlag]()
            {
                if (Character.IsValid())
                {
                    Character->AddQuestFlag(QuestFlag);
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

FReply SBreakerMenu::GoBack()
{
    Rebuild(RootScreen);
    return FReply::Handled();
}
