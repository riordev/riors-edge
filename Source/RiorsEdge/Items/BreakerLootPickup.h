#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerLootPickup.generated.h"

class ABreakerCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

// A dropped item lying on the ground: a small floating rarity-tinted box that
// bobs and spins, picked up with F. Despawns after five minutes so a long
// session cannot carpet the arena with junk.
//
// LOOT DRAMA scales with the tier, because the drop is the paycheck and the
// paycheck must be readable from across the arena BEFORE the player walks
// over: Standard is just the tinted box; Uncommon adds a modest column;
// Exceptional+ get a genuine vertical light beam (emissive, taller and
// brighter by tier) plus a rarity-coloured point light; Aberrant and Anomalous
// pulse. Every magnitude O2 PLACEHOLDER.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerLootPickup : public AActor
{
    GENERATED_BODY()

public:
    ABreakerLootPickup();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Set server-side right after spawn; drives the tint and the label.
    UFUNCTION(BlueprintCallable, Category="Loot") void SetItem(const FBreakerItemInstance& NewItem);
    UFUNCTION(BlueprintPure, Category="Loot") const FBreakerItemInstance& GetItem() const { return Item; }
    // HUD popup contract: "RARITY SLOTNAME", e.g. "EXCEPTIONAL BODYARMOUR".
    UFUNCTION(BlueprintPure, Category="Loot") FText GetDisplayLabel() const;
    UFUNCTION(BlueprintPure, Category="Loot") float GetInteractionRange() const { return InteractionRange; }

    // Authority-only: moves the item into the character's backpack and
    // destroys this actor. Returns false when nothing was transferred.
    UFUNCTION(BlueprintCallable, Category="Loot") bool TryPickup(ABreakerCharacter* Character);

    // Rarity chroma shared with the HUD. Anomalous keeps its rift teal — it is
    // a rift-class object, so the object-chroma law permits it.
    static FLinearColor ColorForRarity(EBreakerItemRarity Rarity);
    // 0 (Standard) .. 4 (Anomalous): the one ladder every drama knob below
    // scales from, so a new rarity cannot get a beam without getting a light.
    static int32 TierForRarity(EBreakerItemRarity Rarity);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot", meta=(ClampMin="50")) float InteractionRange = 350.0f;

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void OnRep_Item();
    void ApplyRarityVisuals();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> PickupSphere;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> ItemVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RarityBeam;
    // Rarity-coloured glow around the drop, Exceptional and above. What makes
    // an Aberrant on the far side of a pillar announce itself off the walls.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPointLightComponent> RarityLight;

private:
    UPROPERTY(ReplicatedUsing=OnRep_Item) FBreakerItemInstance Item;
    float BobTime = 0.0f;
    FVector VisualBaseLocation = FVector::ZeroVector;
    // The beam's emissive dynamic material and this rarity's resolved drama
    // numbers, cached by ApplyRarityVisuals so Tick's pulse never re-derives.
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;
    int32 CachedTier = 0;
    FLinearColor CachedColor = FLinearColor::White;
    float CachedBeamIntensity = 0.0f;
    float CachedLightIntensity = 0.0f;
};
