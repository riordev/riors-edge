#include "Progression/BreakerWorldPoints.h"

namespace
{
    // Prefixed for the unity build, per the twice-shipped rule about helpers
    // that collide once translation units are merged.
    FBreakerWorldPointSource BreakerWorldPointMake(
        const TCHAR* Id, const TCHAR* Display, int32 Act,
        EBreakerWorldPointDelivery Delivery, bool bTriggerBuilt)
    {
        FBreakerWorldPointSource Source;
        Source.SourceId = Id;
        Source.Display = FText::FromString(Display);
        Source.Act = Act;
        Source.Delivery = Delivery;
        Source.bTriggerBuilt = bTriggerBuilt;
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
            1, EDelivery::MainPath, false),
        // The Forge interaction is the one Act I entry whose trigger EXISTS:
        // Kess is spawned, her dialogue ships, and MetForgeKeeper is already a
        // registered flag written from a real conversation.
        BreakerWorldPointMake(TEXT("FirstForge"), TEXT("First Forge interaction"),
            1, EDelivery::MainPath, true),
        // GROUPED, and the grouping is a live owner question rather than a
        // settled shape: this list allots the archetype first-clears two slots
        // while the modes design budgets them at eight, one per archetype, and
        // both cannot be true inside a total of fifteen. O7 makes the slot
        // count canon and leaves the reconciliation open, so it stays two here
        // and the ledger carries the question.
        BreakerWorldPointMake(TEXT("ArchetypesActOne"), TEXT("First-clear of each Act I rift archetype"),
            1, EDelivery::Archetype, false),
        BreakerWorldPointMake(TEXT("ActOneBoss"), TEXT("Act I boss"),
            1, EDelivery::MainPath, false),
        BreakerWorldPointMake(TEXT("FragmentOne"), TEXT("Rior fragment #1 reconstructed"),
            1, EDelivery::Fragment, false),

        // --- Act II ---------------------------------------------------------
        BreakerWorldPointMake(TEXT("BreachFirstEntry"), TEXT("The Breach, first entry"),
            2, EDelivery::MainPath, false),
        BreakerWorldPointMake(TEXT("ArchetypesRemaining"), TEXT("First-clear of the remaining rift archetypes"),
            2, EDelivery::Archetype, false),
        BreakerWorldPointMake(TEXT("FragmentTwo"), TEXT("Rior fragment #2 reconstructed"),
            2, EDelivery::Fragment, false),
        BreakerWorldPointMake(TEXT("AlteredCommander"), TEXT("Defeat the Altered commander"),
            2, EDelivery::MainPath, false),
        BreakerWorldPointMake(TEXT("ActTwoBoss"), TEXT("Act II boss"),
            2, EDelivery::MainPath, false),

        // --- Act III --------------------------------------------------------
        BreakerWorldPointMake(TEXT("EarthOne"), TEXT("Erased Earth 1, zone completion"),
            3, EDelivery::Zone, false),
        BreakerWorldPointMake(TEXT("EarthTwo"), TEXT("Erased Earth 2, zone completion"),
            3, EDelivery::Zone, false),
        BreakerWorldPointMake(TEXT("FragmentThree"), TEXT("Rior fragment #3 reconstructed"),
            3, EDelivery::Fragment, false),
        BreakerWorldPointMake(TEXT("SurvivorToAnchor"), TEXT("Bring the Survivor to an Anchor"),
            3, EDelivery::MainPath, false),
        BreakerWorldPointMake(TEXT("EarthThree"), TEXT("Erased Earth 3, the Earth where Rior lost"),
            3, EDelivery::Zone, false),
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
    int32 Count = 0;
    for (const FBreakerWorldPointSource& Source : GetSources())
    {
        if (Source.bTriggerBuilt) ++Count;
    }
    return Count;
}
