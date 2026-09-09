#include "Progression/BreakerWorldPoints.h"
#include "Save/BreakerMissionContent.h"

namespace
{
    // Prefixed for the unity build, per the twice-shipped rule about helpers
    // that collide once translation units are merged.
    FBreakerWorldPointSource BreakerWorldPointMake(
        const TCHAR* Id, const TCHAR* Display, int32 Act,
        EBreakerWorldPointDelivery Delivery)
    {
        FBreakerWorldPointSource Source;
        Source.SourceId = Id;
        Source.Display = FText::FromString(Display);
        Source.Act = Act;
        Source.Delivery = Delivery;
        // Both design rules are properties of the LIST, so no entry sets them
        // and the test asserts them over everything. An entry that needs to
        // break one has to say so at its own site, which is the point.
        Source.bRequiresParty = false;
        Source.bMissable = false;
        return Source;
    }
}

const TArray<FBreakerWorldPointSource>& UBreakerWorldPointLibrary::GetSources()
{
    using EDelivery = EBreakerWorldPointDelivery;
    static const TArray<FBreakerWorldPointSource> Sources = {
        // --- Act I ----------------------------------------------------------
        BreakerWorldPointMake(TEXT("TutorialRift"), TEXT("Complete the tutorial rift"),
            1, EDelivery::MainPath),
        BreakerWorldPointMake(TEXT("FirstForge"), TEXT("First Forge interaction"),
            1, EDelivery::MainPath),
        // O117: Act I and the remaining archetypes each pay one grouped grant.
        // Eight individual grants would violate the canon fifteen-source list.
        BreakerWorldPointMake(TEXT("ArchetypesActOne"), TEXT("First-clear of each Act I rift archetype"),
            1, EDelivery::Archetype),
        BreakerWorldPointMake(TEXT("ActOneBoss"), TEXT("Act I boss"),
            1, EDelivery::MainPath),
        BreakerWorldPointMake(TEXT("FragmentOne"), TEXT("Rior fragment #1 reconstructed"),
            1, EDelivery::Fragment),

        // --- Act II ---------------------------------------------------------
        BreakerWorldPointMake(TEXT("BreachFirstEntry"), TEXT("The Breach, first entry"),
            2, EDelivery::MainPath),
        BreakerWorldPointMake(TEXT("ArchetypesRemaining"), TEXT("First-clear of the remaining rift archetypes"),
            2, EDelivery::Archetype),
        BreakerWorldPointMake(TEXT("FragmentTwo"), TEXT("Rior fragment #2 reconstructed"),
            2, EDelivery::Fragment),
        BreakerWorldPointMake(TEXT("AlteredCommander"), TEXT("Defeat the Altered commander"),
            2, EDelivery::MainPath),
        BreakerWorldPointMake(TEXT("ActTwoBoss"), TEXT("Act II boss"),
            2, EDelivery::MainPath),

        // --- Act III --------------------------------------------------------
        BreakerWorldPointMake(TEXT("EarthOne"), TEXT("Erased Earth 1, zone completion"),
            3, EDelivery::Zone),
        BreakerWorldPointMake(TEXT("EarthTwo"), TEXT("Erased Earth 2, zone completion"),
            3, EDelivery::Zone),
        BreakerWorldPointMake(TEXT("FragmentThree"), TEXT("Rior fragment #3 reconstructed"),
            3, EDelivery::Fragment),
        BreakerWorldPointMake(TEXT("SurvivorToAnchor"), TEXT("Bring the Survivor to an Anchor"),
            3, EDelivery::MainPath),
        BreakerWorldPointMake(TEXT("EarthThree"), TEXT("Erased Earth 3, the Earth where Rior lost"),
            3, EDelivery::Zone),
    };
    return Sources;
}

FName UBreakerWorldPointLibrary::FlagForSource(FName SourceId)
{
    if (SourceId.IsNone()) return NAME_None;
    return FName(*FString::Printf(TEXT("World.%s"), *SourceId.ToString()));
}

bool UBreakerWorldPointLibrary::IsKnownSource(FName SourceId)
{
    for (const FBreakerWorldPointSource& Source : GetSources())
    {
        if (Source.SourceId == SourceId) return true;
    }
    return false;
}

int32 UBreakerWorldPointLibrary::CountWithBuiltTrigger()
{
    // Derive authoring coverage from the same mission beats consulted by
    // SettleWorldCorePoints. This counts references, not runtime completion
    // evidence, and needs no separately maintained per-source status field.
    TSet<FName> Triggered;
    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
    {
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
        {
            if (Beat.Kind == EBreakerMissionBeatKind::Unlock && !Beat.CorePoint.IsNone())
            {
                Triggered.Add(Beat.CorePoint);
            }
        }
    }

    int32 Count = 0;
    for (const FBreakerWorldPointSource& Source : GetSources())
    {
        if (Triggered.Contains(Source.SourceId)) ++Count;
    }
    return Count;
}
