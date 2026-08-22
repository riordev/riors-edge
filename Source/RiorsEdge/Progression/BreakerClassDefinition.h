#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerClassDefinition.generated.h"

class UBreakerProgressionTree;

UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerClassDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("ClassDefinition"), ClassAssetId);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName ClassAssetId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") EBreakerClassId ClassId = EBreakerClassId::None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity", meta=(MultiLine="true")) FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource") FGameplayTag ResourceTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression") TArray<TObjectPtr<UBreakerProgressionTree>> BranchTrees;
    // O100: THE KIT IS A PARTITION, and these three fields are it.
    //
    // Exactly two starters, free at level one, and what seeds a fresh loadout.
    // Every other class ability is unlockable, one at a time, per character,
    // bought with a token at the quartermaster. The ultimate is free at level
    // one and never unlocks.
    //
    // This replaced a single StartingClassAbilityIds list that four classes
    // filled with their WHOLE kit — so everything was unlocked at level one and
    // there was no acquisition system at all — while Swift filled it with two,
    // which is why Swift.CadenceBreak was registered, offered by the picker and
    // permanently refusable. One list doing both jobs is what allowed those two
    // states to coexist and look alike.
    //
    // THE THREE MUST PARTITION THE CLASS'S REGISTERED SET EXACTLY: disjoint,
    // and together equal to what UBreakerAbilityDefinition::GetClassAbilities
    // offers. An omission is an ability nobody can ever reach; an overlap is an
    // ability that is free and also for sale. RiorsEdge.Abilities.Catalogue.
    // Partition asserts both, because these lists are hand-authored data and
    // nothing else in the pass is.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout") TArray<FName> StarterAbilityIds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout") TArray<FName> UnlockableAbilityIds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout") FName BaseUltimateId = NAME_None;
};
