#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NativeGameplayTags.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerProgressionLibrary.generated.h"

class UBreakerClassDefinition;
class UBreakerProgressionNode;
class UBreakerProgressionTree;

// Tags published by rule-rewrite and verb-grant nodes. Native so the
// fallback trees work with no .ini content, exactly like the weapon
// prototype archetypes work with no Data Assets.
namespace BreakerNodeTags
{
    // Core constellation slice
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Fixate);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TunnelVision);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_TriggerDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Cyclic);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_LastRound);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_OpenWound);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SetStance);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Read);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Loft);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_PhantomStep);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_Parry);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Verb_AirJump);

    // Swift / Kinetic
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_ReadTheRoom);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Contact);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Carry);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Redirect);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_EvadeConversion);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Landing);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_SkimDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_AirWork);

    // Swift / Marksman
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_LongLens);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Steady);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Ledger);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Angle);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_MarkEconomy);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_PierceDiscipline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Sightline);
    RIORSEDGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Node_Lead);
}

// Zero-setup fallback skill-tree content, the same convention the weapon
// prototypes use: real runtime trees exist in C++ so the slice is playable
// before any Data Asset is authored, and the Data Assets replace them
// one-for-one later (Core-Constellations §10.3 criterion 1 is a shipping
// requirement, not a slice-prototype requirement).
//
// EVERY MAGNITUDE HERE IS AN O2 PLACEHOLDER. Wave-mode instrumentation has
// not reported; no value below is tuned and none may be signed off.
UCLASS()
class RIORSEDGE_API UBreakerProgressionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The ~15-node Core constellation slice subset (Core-Constellations §10.1).
    static UBreakerProgressionTree* GetCoreSliceTree();
    // Swift KINETIC branch, tiers 1-3 (Class-Kits §1.4).
    static UBreakerProgressionTree* GetSwiftKineticTree();
    // Swift MARKSMAN branch, tiers 1-3 (Class-Kits §1.5).
    static UBreakerProgressionTree* GetSwiftMarksmanTree();

    // Core tree plus every class tree. The UI enumerates from here.
    UFUNCTION(BlueprintPure, Category="Progression|Content")
    static const TArray<UBreakerProgressionTree*>& GetAllFallbackTrees();

    // Trees a character of this class can spend in (core tree included).
    UFUNCTION(BlueprintPure, Category="Progression|Content")
    static TArray<UBreakerProgressionTree*> GetTreesForClass(EBreakerClassId ClassId);

    // Fallback class definition so a component with no Data Asset can still
    // resolve branch trees, starting abilities, and the ultimate.
    static UBreakerClassDefinition* GetFallbackClassDefinition(EBreakerClassId ClassId);

    UFUNCTION(BlueprintPure, Category="Progression|Content")
    static const UBreakerProgressionNode* FindFallbackNode(FName NodeId);

    // XP-And-Pacing §9: 10 Class Points and 12 Core Points at slice cap 10.
    // O2 PLACEHOLDER — the shipping values come from the curve Data Asset.
    static constexpr int32 SliceClassPointGrant = 10;
    static constexpr int32 SliceCorePointGrant = 12;
};
