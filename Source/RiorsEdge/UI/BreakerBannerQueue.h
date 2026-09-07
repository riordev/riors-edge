#pragma once

#include "CoreMinimal.h"

// The event banners, to Assets/design/04-death-banners/spec.md (O202): three
// kinds, three disjoint rectangles, one queue. World-free: the HUD enqueues
// and draws, everything about WHEN a banner arrives, how long it holds and
// where it slides from is decided here and tested on a bare array.
//
// The out is a slide, never a fade. A plate leaves the way it came.

// Priority is enum order: a rift completing outranks the level it paid,
// which outranks the wave that ended it. Never serialized; not append-only.
enum class EBreakerBannerKind : uint8
{
    RiftComplete,
    LevelUp,
    WaveClear
};

struct FBreakerBanner
{
    EBreakerBannerKind Kind = EBreakerBannerKind::WaveClear;
    FString Title;
    FString Line;
    // World seconds at which the plate starts its slide in. Negative means
    // not yet scheduled: ScheduleArrival assigns it.
    double ArriveAt = -1.0;
};

namespace BreakerBannerQueue
{
    // The stagger between arrivals: 04-death-banners.
    inline constexpr float StaggerSeconds = 0.3f;
    // The slide out, every kind: the sheet's 120 ms.
    inline constexpr float OutSeconds = 0.12f;
    // How far a plate travels on its slide. The wave banner's 16 px is the
    // sheet's; the other two take the same figure. O2 PLACEHOLDER.
    inline constexpr float SlidePixels = 16.0f;

    inline int32 PriorityFor(EBreakerBannerKind Kind)
    {
        return static_cast<int32>(Kind);
    }

    inline float HoldSecondsFor(EBreakerBannerKind Kind)
    {
        switch (Kind)
        {
        case EBreakerBannerKind::RiftComplete: return 2.4f;   // 04-death-banners
        case EBreakerBannerKind::LevelUp:      return 2.0f;   // 04-death-banners
        default:                               return 1.6f;   // 04-death-banners
        }
    }

    // The slide in. WaveClear's 160 ms is the sheet's; the sheet names no
    // in-time for the other two and they take the same. O2 PLACEHOLDER.
    inline float InSecondsFor(EBreakerBannerKind Kind)
    {
        switch (Kind)
        {
        case EBreakerBannerKind::WaveClear: return 0.16f;
        default:                            return 0.16f;   // O2 PLACEHOLDER
        }
    }

    inline float TotalSecondsFor(EBreakerBannerKind Kind)
    {
        return InSecondsFor(Kind) + HoldSecondsFor(Kind) + OutSeconds;
    }

    // Spec pixels at 1920x1080: X, Y, W, H.
    struct FRect
    {
        float X = 0.0f;
        float Y = 0.0f;
        float W = 0.0f;
        float H = 0.0f;
        float Right() const { return X + W; }
        float Bottom() const { return Y + H; }
    };

    inline FRect RectFor(EBreakerBannerKind Kind)
    {
        switch (Kind)
        {
        case EBreakerBannerKind::RiftComplete: return FRect{ 0.0f, 440.0f, 1920.0f, 200.0f };  // 04-death-banners
        case EBreakerBannerKind::LevelUp:      return FRect{ 1480.0f, 200.0f, 400.0f, 88.0f }; // 04-death-banners
        default:                               return FRect{ 760.0f, 96.0f, 400.0f, 64.0f };   // 04-death-banners
        }
    }

    inline bool RectsDisjoint(const FRect& A, const FRect& B)
    {
        return A.Right() <= B.X || B.Right() <= A.X || A.Bottom() <= B.Y || B.Bottom() <= A.Y;
    }

    // The unit direction the plate TRAVELS on its way in. Wave clear slides
    // down from above, rift complete comes in from the left, level up from
    // the right. The out reverses it.
    inline FVector2D SlideAxisFor(EBreakerBannerKind Kind)
    {
        switch (Kind)
        {
        case EBreakerBannerKind::RiftComplete: return FVector2D(1.0f, 0.0f);
        case EBreakerBannerKind::LevelUp:      return FVector2D(-1.0f, 0.0f);
        default:                               return FVector2D(0.0f, 1.0f);
        }
    }

    // The identity rail: system bone for every plate, reward gold only for
    // the level, which is the one banner that IS a payout.
    inline bool RailIsGold(EBreakerBannerKind Kind)
    {
        return Kind == EBreakerBannerKind::LevelUp;
    }

    inline bool IsShowing(EBreakerBannerKind Kind, float Age)
    {
        return Age >= 0.0f && Age < TotalSecondsFor(Kind);
    }

    // Pixels the plate sits BEHIND its rest position along the slide axis:
    // SlidePixels on frame zero, zero through the hold, back to SlidePixels
    // on the last frame of the out. Linear both ways, no ease, no fade.
    inline float SlideDisplacementFor(EBreakerBannerKind Kind, float Age)
    {
        const float In = InSecondsFor(Kind);
        const float Hold = HoldSecondsFor(Kind);
        if (Age <= 0.0f) return SlidePixels;
        if (Age < In) return SlidePixels * (1.0f - Age / In);
        if (Age < In + Hold) return 0.0f;
        const float OutT = FMath::Clamp((Age - In - Hold) / OutSeconds, 0.0f, 1.0f);
        return SlidePixels * OutT;
    }

    // Assigns ArriveAt to every unscheduled banner in Pending: each lands at
    // max(Now, the latest scheduled arrival + StaggerSeconds), and among the
    // banners waiting for a time the higher priority takes the earlier slot.
    // Called once per frame before drawing, so every event that fired since
    // the last frame is ordered by priority rather than by which delegate
    // happened to broadcast first. Returns the latest arrival scheduled.
    inline double ScheduleArrival(TArray<FBreakerBanner>& Pending, double Now)
    {
        double Latest = -1.0;
        TArray<int32> Unscheduled;
        for (int32 Index = 0; Index < Pending.Num(); ++Index)
        {
            if (Pending[Index].ArriveAt < 0.0) Unscheduled.Add(Index);
            else Latest = FMath::Max(Latest, Pending[Index].ArriveAt);
        }
        Unscheduled.StableSort([&Pending](int32 A, int32 B)
        {
            return PriorityFor(Pending[A].Kind) < PriorityFor(Pending[B].Kind);
        });
        for (const int32 Index : Unscheduled)
        {
            const double Slot = Latest < 0.0 ? Now : FMath::Max(Now, Latest + StaggerSeconds);
            Pending[Index].ArriveAt = Slot;
            Latest = Slot;
        }
        return Latest;
    }

    // Drops every banner whose out has finished.
    inline void Prune(TArray<FBreakerBanner>& Pending, double Now)
    {
        Pending.RemoveAll([Now](const FBreakerBanner& Banner)
        {
            return Banner.ArriveAt >= 0.0
                && static_cast<float>(Now - Banner.ArriveAt) >= TotalSecondsFor(Banner.Kind);
        });
    }
}
