#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBreakerProgressionTree;

// The census: every built tree and the progression vocabulary, as JSON.
//
// Docs/STATE.md used to be measured by regex over the node library's C++
// authoring shape: a MakeNode( pattern, a sixty-line forward walk, a helper
// read by name. This is the same census read off the objects the game
// actually builds, so a magnitude that moves in data moves in the report
// without a compile (O186). The commandlet writes it; the freshness test
// pins the committed file to a live export; status.py --from-data reads it.
//
// World-free: takes the trees, returns the object. Nothing here writes.
namespace BreakerCensus
{
    // The repo-relative path of the export. One place, so the commandlet, the
    // test and the reporter cannot disagree about where the census lives.
    RIORSEDGE_API FString RelativePath();

    RIORSEDGE_API TSharedRef<FJsonObject> Export(const TArray<UBreakerProgressionTree*>& Trees);

    // Pretty-printed, "\n" line endings whatever the platform, no BOM: the
    // file is committed and diffed, so it must serialise byte-identically on
    // every seat.
    RIORSEDGE_API FString Serialize(const TSharedRef<FJsonObject>& Census);
}
