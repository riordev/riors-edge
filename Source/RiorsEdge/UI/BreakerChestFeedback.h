#pragma once
#include "CoreMinimal.h"
class AActor;
// O275: opening a supply chest is a director verb. This is GLASS's half —
// the seam the chest's open path calls, on BreakerRiftFeedback's pattern:
// only the LOCAL opener hears the latch, and the director is found or
// spawned for their world. The chest actor owns no sound of its own. The
// caller (the chest's interaction path, GROUND's half) is not wired here.
namespace BreakerChestFeedback
{
    RIORSEDGE_API void PlayOpen(AActor* Chest, AActor* Opener);
}
