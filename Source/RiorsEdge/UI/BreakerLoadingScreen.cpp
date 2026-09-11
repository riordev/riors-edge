#include "UI/BreakerLoadingScreen.h"
#include "Combat/BreakerMonsterChassis.h"

#include "Data/BreakerStrings.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "UI/BreakerTypeRoles.h"
#include "UI/BreakerUIStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    // The pack's loading spec, on the tokens' 64px margin (the README's own
    // sizing sheet says 96; the tokens are the named source for every size
    // and the owner has the discrepancy — 64 until his answer says otherwise).
    constexpr float BreakerDeployMargin = 64.0f;
    constexpr float BreakerDeployContentWidth = 1240.0f;
    constexpr int32 BreakerDeployNamePixels = 104;
    constexpr int32 BreakerDeployLinePixels = 19;
    // Lattice: 7 cells, 26x26, 5px gaps; each cell breathes to cyan and back
    // over 1.6s, staggered 120ms.
    constexpr int32 BreakerLatticeCells = 7;
    constexpr float BreakerLatticeCellSize = 26.0f;
    constexpr float BreakerLatticeGap = 5.0f;
    constexpr float BreakerLatticePeriod = 1.6f;
    constexpr float BreakerLatticeStagger = 0.12f;
    // Bottom crawl: a 4px track, the fill 22% of the authored width,
    // travelling -100%..340% of its own width over 2.4s, ease-in-out.
    constexpr float BreakerCrawlHeight = 4.0f;
    constexpr float BreakerCrawlFillFraction = 0.22f;
    constexpr float BreakerCrawlPeriod = 2.4f;

    TSharedRef<SWidget> BreakerDeploySolid(const FLinearColor& Color)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Color)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    float BreakerEaseInOut(float T)
    {
        return 0.5f - 0.5f * FMath::Cos(T * PI);
    }
}

FBreakerDeploymentBriefing SBreakerLoadingScreen::MakeBriefing(const FBreakerRiftDefinition& Rift,
    int32 EliteBonus, int32 EndgameDeathsRemaining, int32 PlayerItemLevel)
{
    FBreakerDeploymentBriefing Briefing;
    Briefing.AreaName = Rift.AreaName;
    Briefing.AreaLine = Rift.AreaLine;
    Briefing.AreaLevel = Rift.EffectiveAreaLevel();
    UBreakerRiftLibrary::GetDropItemLevelRange(Briefing.AreaLevel, EliteBonus,
        Briefing.ItemLevelMin, Briefing.ItemLevelMax);
    // The multipliers are properties of the CURVE (the library's own note:
    // BaseHealth cancels), so default-constructed params carry them for every
    // chassis sharing the growth constants.
    const FBreakerMonsterChassisParams Params;
    Briefing.HealthMultiplier = UBreakerRiftLibrary::GetMonsterHealthMultiplier(Briefing.AreaLevel, Params);
    Briefing.DamageMultiplier = UBreakerRiftLibrary::GetMonsterDamageMultiplier(Briefing.AreaLevel, Params);
    Briefing.DeathAllowance = UBreakerRiftLibrary::GetDeathAllowanceReadout(Rift.Tier, EndgameDeathsRemaining);
    Briefing.TierKicker = BreakerStrings::Get(Rift.Tier == EBreakerRiftTier::Campaign
        ? EBreakerStringKey::LoadingTitleCampaign
        : EBreakerStringKey::LoadingTitleEndgame);

    // THE READINESS READ. Compared against the item level this area DROPS
    // rather than against its area level: what decides a fight is the gear the
    // player is holding against the gear the content assumes, and those are
    // the two numbers the power curve is built to cancel.
    Briefing.PlayerItemLevel = FMath::Max(PlayerItemLevel, 0);
    if (Briefing.PlayerItemLevel > 0)
    {
        const int32 Expected = UBreakerMonsterChassisLibrary::GetDropItemLevel(Briefing.AreaLevel);
        const int32 Behind = Expected - Briefing.PlayerItemLevel;
        // The threshold is a WHOLE TIER of gear, not a point: a level or two
        // under is ordinary play and saying so every time would make the line
        // noise, which is how a warning stops being read.
        constexpr int32 UnderLevelledBy = 4;   // O2 PLACEHOLDER
        if (Behind >= UnderLevelledBy)
        {
            Briefing.ReadinessLine = FString::Printf(
                TEXT("YOUR GEAR IS i%d — %d LEVELS UNDER THIS AREA"),
                Briefing.PlayerItemLevel, Behind);
        }
        else
        {
            Briefing.ReadinessLine = FString::Printf(TEXT("YOUR GEAR IS i%d"), Briefing.PlayerItemLevel);
        }
    }
    return Briefing;
}

// ===========================================================================
// SBreakerRiftLattice
// ===========================================================================

void SBreakerRiftLattice::Construct(const FArguments&)
{
    // The crawl fill is sized from the viewport ONCE, at construction — the
    // derived-width rule; the widget lives seconds, so a mid-beat resize is
    // not a case worth a re-layout path.
    FVector2D Viewport(1920.0f, 1080.0f);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(Viewport);
    }
    if (Viewport.X < 640.0f) Viewport = FVector2D(1920.0f, 1080.0f);
    CrawlFillWidth = BreakerCrawlFillFraction * static_cast<float>(Viewport.X);

    // ---- The lattice -------------------------------------------------------
    TSharedRef<SHorizontalBox> Lattice = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < BreakerLatticeCells; ++Index)
    {
        TSharedPtr<SBorder> Cell;
        Lattice->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Index + 1 < BreakerLatticeCells ? BreakerLatticeGap : 0.0f, 0.0f)
        [
            SNew(SBox).WidthOverride(BreakerLatticeCellSize).HeightOverride(BreakerLatticeCellSize)
            [
                SAssignNew(Cell, SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(BreakerUI::Panel10)
                [
                    SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
                ]
            ]
        ];
        LatticeCells.Add(Cell);
    }

    // ---- The stage line ----------------------------------------------------
    const TSharedRef<SWidget> StageRow = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SAssignNew(StageText, STextBlock)
                .Text(FText::GetEmpty())
                .ColorAndOpacity(BreakerUI::TextSecondary)
                .Font(BreakerMonoFont(13, 0.16f))
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
        [
            SNew(SBox).WidthOverride(8.0f).HeightOverride(14.0f)
            [
                SAssignNew(BlinkBlock, SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(BreakerUI::System)
                [
                    SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
                ]
            ]
        ];

    // ---- The crawl ---------------------------------------------------------
    SAssignNew(CrawlFill, SBox).WidthOverride(CrawlFillWidth).HeightOverride(BreakerCrawlHeight)
    [
        BreakerDeploySolid(BreakerUI::Orange)
    ];
    Crawl = SNew(SBox).HeightOverride(BreakerCrawlHeight)
        .Clipping(EWidgetClipping::ClipToBounds)
    [
        SNew(SOverlay)
        + SOverlay::Slot()[BreakerDeploySolid(BreakerUI::Panel00)]
        + SOverlay::Slot().HAlign(HAlign_Left)[CrawlFill.ToSharedRef()]
    ];

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Lattice]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)[StageRow]
    ];

    RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBreakerRiftLattice::Animate));
}

void SBreakerRiftLattice::SetStage(const FText& StageWords)
{
    if (StageText.IsValid())
    {
        StageText->SetText(StageWords);
    }
}

EActiveTimerReturnType SBreakerRiftLattice::Animate(double CurrentTime, float)
{
    if (StartSeconds <= 0.0) StartSeconds = CurrentTime;
    const float Elapsed = static_cast<float>(CurrentTime - StartSeconds);

    // Lattice: each cell breathes idle -> cyan -> idle on the shared period,
    // offset by the stagger.
    for (int32 Index = 0; Index < LatticeCells.Num(); ++Index)
    {
        if (!LatticeCells[Index].IsValid()) continue;
        const float Phase = FMath::Fmod(Elapsed - BreakerLatticeStagger * Index + BreakerLatticePeriod * 4.0f,
            BreakerLatticePeriod) / BreakerLatticePeriod;
        const float Breathe = BreakerEaseInOut(1.0f - FMath::Abs(Phase * 2.0f - 1.0f));
        LatticeCells[Index]->SetBorderBackgroundColor(
            FMath::Lerp(BreakerUI::Panel10, BreakerUI::System, Breathe));
    }

    // The cursor block: the 1s step — visible for the first half-second.
    if (BlinkBlock.IsValid())
    {
        const bool bLit = FMath::Fmod(Elapsed, 1.0f) < 0.5f;
        BlinkBlock->SetBorderBackgroundColor(bLit ? BreakerUI::System
            : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
    }

    // The crawl: its own width, -100% to 340%, ease-in-out, repeating.
    if (CrawlFill.IsValid())
    {
        const float T = BreakerEaseInOut(FMath::Fmod(Elapsed, BreakerCrawlPeriod) / BreakerCrawlPeriod);
        const float Offset = (-1.0f + 4.4f * T) * CrawlFillWidth;
        CrawlFill->SetRenderTransform(TOptional<FSlateRenderTransform>(
            FSlateRenderTransform(FVector2D(Offset, 0.0f))));
    }
    return EActiveTimerReturnType::Continue;
}

// ===========================================================================
// SBreakerLoadingScreen — the card's builders, then the deployment card
// ===========================================================================

TSharedRef<SWidget> SBreakerLoadingScreen::MakeCardFrame(TSharedRef<SWidget> Content, TSharedRef<SWidget> Crawl)
{
    return SNew(SOverlay)
        + SOverlay::Slot()[BreakerDeploySolid(BreakerUI::BgBase)]
        // The top edge: a 2px rule, full bleed.
        + SOverlay::Slot().VAlign(VAlign_Top)
        [
            SNew(SBox).HeightOverride(2.0f)[BreakerDeploySolid(BreakerUI::BorderRest)]
        ]
        // The corner identity: the breakers mark and its caption.
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
            .Padding(BreakerDeployMargin, 80.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                BreakerMark(TEXT("/Game/Breaker/UI/Marks/T_InsigniaBreakers.T_InsigniaBreakers"), 34.0f)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(18.0f, 0.0f, 0.0f, 0.0f)
            [
                BreakerMonoText(FText::FromString(BreakerStrings::Get(EBreakerStringKey::LoadingInsignia)), 11, BreakerUI::TextMuted, 0.22f)
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(BreakerDeployContentWidth)[Content]
        ]
        + SOverlay::Slot().VAlign(VAlign_Bottom)[Crawl];
}

TSharedRef<SWidget> SBreakerLoadingScreen::MakeHeadline(const FString& Name, const FText& Line)
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            // WRAPPED at the name column's derived width — the content
            // block minus the side block and the gap. The spec's own
            // 0.92 line height is the tell that long names are MEANT to
            // break: the first capture photographed "FERNHALL SU" clipped
            // against the level rail instead.
            SNew(STextBlock)
                .Text(FText::FromString(Name.ToUpper()))
                .ColorAndOpacity(BreakerUI::TextPrimary)
                .WrapTextAt(BreakerDeployContentWidth - 320.0f)
                .LineHeightPercentage(0.92f)
                .Font(BreakerDisplayFont(BreakerDeployNamePixels, /*bHeavy=*/true))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
        [
            SNew(STextBlock)
                .Text(Line)
                .ColorAndOpacity(BreakerUI::TextSecondary)
                .WrapTextAt(760.0f)
                .Font(BreakerBodyFont(BreakerDeployLinePixels))
        ];
}

TSharedRef<SWidget> SBreakerLoadingScreen::MakeRailedBlock(TSharedRef<SWidget> Body)
{
    // 3px gold rail on the block's edge, the body beside it.
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(BreakerUI::RailThickness)[BreakerDeploySolid(BreakerUI::Gold)]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(28.0f, 0.0f, 0.0f, 0.0f)
        [
            Body
        ];
}

TSharedRef<SWidget> SBreakerLoadingScreen::MakeStatRow(const TArray<TPair<FString, FString>>& Stats)
{
    // Readouts split by 1px dividers.
    auto MakeStat = [](const FString& Caption, const FString& Value) -> TSharedRef<SWidget>
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                BreakerMonoText(FText::FromString(Caption), BreakerUI::TypeCaption, BreakerUI::TextMuted, 0.16f)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
            [
                BreakerMonoText(FText::FromString(Value), BreakerUI::TypeBody, BreakerUI::TextPrimary, 0.0f)
            ];
    };
    auto MakeStatDivider = []() -> TSharedRef<SWidget>
    {
        return SNew(SBox).WidthOverride(BreakerUI::BorderThin).Padding(0.0f)
        [
            BreakerDeploySolid(BreakerUI::BorderRest)
        ];
    };
    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < Stats.Num(); ++Index)
    {
        if (Index > 0)
        {
            Row->AddSlot().AutoWidth().Padding(BreakerUI::Space24, 0.0f)[MakeStatDivider()];
        }
        Row->AddSlot().AutoWidth()[MakeStat(Stats[Index].Key, Stats[Index].Value)];
    }
    return Row;
}

void SBreakerLoadingScreen::Construct(const FArguments& InArgs)
{
    const FBreakerDeploymentBriefing& Briefing = InArgs._Briefing;

    // ---- The level block ---------------------------------------------------
    // The area level at the display scale in reward gold, the item-level
    // range under it, on the gold rail. Numbers ride the mono role's Medium —
    // the closest weight the pack's own family carries to the spec's mono 700.
    const TSharedRef<SWidget> LevelBlock = MakeRailedBlock(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
        [
            BreakerMonoText(FText::FromString(BreakerStrings::Get(EBreakerStringKey::LoadingAreaLevel)), BreakerUI::TypeCaption,
                BreakerUI::TextMuted, 0.16f)
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
        [
            SNew(STextBlock)
                .Text(FText::AsNumber(Briefing.AreaLevel))
                .ColorAndOpacity(BreakerUI::Gold)
                .Font(BreakerMonoFont(BreakerDeployNamePixels, 0.0f, /*bMedium=*/true))
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
        [
            BreakerMonoText(FText::FromString(BreakerStrings::Format(EBreakerStringKey::LoadingItemLevel,
                Briefing.ItemLevelMin, Briefing.ItemLevelMax)), 12, BreakerUI::TextSecondary, 0.16f)
        ]
        // WHAT YOU ARE BRINGING, directly under what the area drops, so
        // the comparison is made by the layout rather than by the player.
        // Harm-coloured only when it is a whole tier of gear behind: a
        // warning that fires on every deployment is not read.
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
        [
            BreakerMonoText(FText::FromString(Briefing.ReadinessLine), 12,
                Briefing.ReadinessLine.Contains(TEXT("UNDER")) ? BreakerUI::Harm : BreakerUI::TextMuted, 0.16f)
        ]);

    // ---- The stat row ------------------------------------------------------
    // Three readouts. The third is O123's field: always present, only the
    // value moves.
    TArray<TPair<FString, FString>> Stats;
    Stats.Emplace(BreakerStrings::Get(EBreakerStringKey::LoadingStatMonsterHealth),
        BreakerStrings::Format(EBreakerStringKey::LoadingStatMultiplier, Briefing.HealthMultiplier));
    Stats.Emplace(BreakerStrings::Get(EBreakerStringKey::LoadingStatMonsterDamage),
        BreakerStrings::Format(EBreakerStringKey::LoadingStatMultiplier, Briefing.DamageMultiplier));
    Stats.Emplace(BreakerStrings::Get(EBreakerStringKey::LoadingStatDeaths), Briefing.DeathAllowance);
    const TSharedRef<SWidget> StatRow = MakeStatRow(Stats);

    // ---- The content block -------------------------------------------------
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()
    [
        BreakerMonoText(FText::FromString(Briefing.TierKicker), 11, BreakerUI::TextMuted, 0.22f)
    ];
    Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
        [
            MakeHeadline(Briefing.AreaName.ToString(), Briefing.AreaLine)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(BreakerUI::Space40, 0.0f, 0.0f, 0.0f)
        [
            LevelBlock
        ]
    ];
    Content->AddSlot().AutoHeight().Padding(0.0f, 56.0f, 0.0f, 0.0f)[StatRow];
    Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space40, 0.0f, 0.0f)
    [
        SAssignNew(Lattice, SBreakerRiftLattice)
    ];

    ChildSlot
    [
        MakeCardFrame(Content, Lattice->GetCrawl())
    ];
}

void SBreakerLoadingScreen::SetStage(const FText& StageWords)
{
    if (Lattice.IsValid())
    {
        Lattice->SetStage(StageWords);
    }
}
