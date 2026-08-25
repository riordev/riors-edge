#pragma once

#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

// ---------------------------------------------------------------------------
// FIELDPLATE TYPE ROLES AND MARKS — shared by every Slate surface.
//
// Archivo (display), IBM Plex Sans (body), IBM Plex Mono (numbers and chrome
// captions): the three role fonts Scripts/import_fonts.py builds from
// Assets/fonts into /Game/Breaker/UI/Fonts, and the mark textures
// Scripts/import_marks.py imports into /Game/Breaker/UI/Marks. This header
// exists because the deployment briefing draws the same roles as the menu,
// and a font loader that exists twice drifts exactly the way a colour does
// (BreakerUIStyle.h's founding rule).
//
// Everything loads once and is ROOTED — the caches outlive every widget, and
// a collected font or brush under live Slate is a crash on the next paint.
// Every helper falls back when its asset is absent (a clone before its LFS
// pull, a suite run with no content): fonts degrade to the engine face, marks
// to an empty box of the same size — the pre-import look, never tofu.
// ---------------------------------------------------------------------------

inline FSlateFontInfo BreakerRoleFont(const TCHAR* AssetPath, const FName& Typeface, int32 Size,
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
inline FSlateFontInfo BreakerDisplayFont(int32 Size, bool bHeavy = false)
{
    return BreakerRoleFont(TEXT("/Game/Breaker/UI/Fonts/F_BreakerDisplay.F_BreakerDisplay"),
        bHeavy ? FName(TEXT("Bold")) : FName(TEXT("SemiBold")), Size, TEXT("Bold"), 0.01f);
}

// Body: mixed-case copy, 400/500/600. The bold flag maps to SemiBold — the
// pack's body family carries no 700.
inline FSlateFontInfo BreakerBodyFont(int32 Size, bool bSemiBold = false)
{
    return BreakerRoleFont(TEXT("/Game/Breaker/UI/Fonts/F_BreakerBody.F_BreakerBody"),
        bSemiBold ? FName(TEXT("SemiBold")) : FName(TEXT("Regular")), Size,
        bSemiBold ? TEXT("Bold") : TEXT("Regular"));
}

// Mono: every number, key cap and tracked chrome caption. Tabular figures
// come with the face; the tracking parameter is the caption's 0.16em. The
// family carries Regular and Medium — bMedium is the emphasis weight where a
// spec asks for mono 700, the closest the pack's own weights come.
inline FSlateFontInfo BreakerMonoFont(int32 Size, float TrackingEm = 0.0f, bool bMedium = false)
{
    return BreakerRoleFont(TEXT("/Game/Breaker/UI/Fonts/F_BreakerMono.F_BreakerMono"),
        bMedium ? FName(TEXT("Medium")) : FName(TEXT("Regular")), Size, TEXT("Mono"), TrackingEm);
}

inline TSharedRef<STextBlock> BreakerMonoText(const FText& Text, int32 Size, const FLinearColor& Color,
    float TrackingEm = 0.0f)
{
    return SNew(STextBlock)
        .Text(Text)
        .ColorAndOpacity(Color)
        .Font(BreakerMonoFont(Size, TrackingEm));
}

// ---- Mark brushes ---------------------------------------------------------
// The pack's stroke-only marks. Insignia never tint (they stay #DCE4EE at
// every state); rarity marks tint to the rarity hex at the call site.
inline const FSlateBrush* BreakerMarkBrush(const TCHAR* AssetPath, const FVector2D& Size)
{
    static TMap<FString, TUniquePtr<FSlateBrush>> Cache;
    TUniquePtr<FSlateBrush>& Slot = Cache.FindOrAdd(FString(AssetPath));
    if (!Slot)
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath);
        if (!Texture) return nullptr;   // absent stays retryable; menu-rate cost
        Texture->AddToRoot();
        Slot = MakeUnique<FSlateBrush>();
        Slot->SetResourceObject(Texture);
        Slot->ImageSize = Size;
        Slot->DrawAs = ESlateBrushDrawType::Image;
    }
    return Slot.Get();
}

inline TSharedRef<SWidget> BreakerMark(const TCHAR* AssetPath, float Size,
    const FLinearColor& Tint = FLinearColor::White)
{
    const FSlateBrush* Brush = BreakerMarkBrush(AssetPath, FVector2D(Size, Size));
    return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
    [
        Brush
            ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Brush).ColorAndOpacity(Tint))
            : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(1.0f, 1.0f)))
    ];
}
