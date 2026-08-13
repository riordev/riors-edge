#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCombatComponent.generated.h"

class UBreakerAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerDamageReceived, const FBreakerDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerDeathEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerHitDealt, const FBreakerHitContext&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerKillDealt, const FBreakerHitContext&, Hit);

// One push/pop-able outgoing damage modifier. Keyed so the pusher (an ability
// window, a node) can remove exactly its own entry; expiry is a safety net for
// windows whose owner never gets to pop.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerOutgoingModifier
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName Key;
    UPROPERTY(BlueprintReadOnly) float FlatBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MoreMultiplier = 1.0f;
    // Negative means "never expires on its own".
    UPROPERTY(BlueprintReadOnly) float ExpiryTime = -1.0f;
};

UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerCombatComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") FBreakerDamageResult ReceiveDamage(const FBreakerDamageRequest& Request);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool SpendClassResource(float Cost);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void AddClassResource(float Amount);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void RestoreVitals();
    UFUNCTION(BlueprintPure, Category="Combat") bool IsDead() const;
    UFUNCTION(BlueprintPure, Category="Combat") float GetSecondsSinceDamage() const;

    // Outgoing-damage modifier chain (SI-7 partial). Pushed by ability windows
    // and class nodes; applied to a request just before it is submitted.
    UFUNCTION(BlueprintCallable, Category="Combat|Outgoing")
    void PushOutgoingModifier(FName Key, float FlatBonus, float MoreMultiplier, float ExpirySeconds);

    UFUNCTION(BlueprintCallable, Category="Combat|Outgoing")
    void RemoveOutgoingModifier(FName Key);

    // Adds the composed flat bonus to BaseDamage and folds the More product
    // into SourceDamageMultiplier. The product is clamped at the Damage-Pipeline
    // §4 ceiling (2.20x).
    UFUNCTION(BlueprintCallable, Category="Combat|Outgoing")
    void ApplyOutgoingModifiers(UPARAM(ref) FBreakerDamageRequest& Request);

    UFUNCTION(BlueprintPure, Category="Combat|Outgoing") float GetComposedMoreMultiplier() const;

    // Incoming-damage modifier chain (Ability-Implementation-Spec §4.4). Keyed
    // push/remove, composed multiplicatively into FBreakerDefenseState::
    // IncomingDamageMultiplier at the top of ReceiveDamage, so it lands before
    // armour, shields, and the passive rolls — the same stage gear-rolled
    // physical reduction already occupies.
    //   1.0 = no change, 1.15 = takes 15% more, 0.0 = immune.
    // First consumer: Caster's Overcast penalty (Class-Kits §2.1).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Incoming")
    void PushIncomingDamageModifier(FName Key, float Multiplier);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Incoming")
    void RemoveIncomingDamageModifier(FName Key);

    UFUNCTION(BlueprintPure, Category="Combat|Incoming") float GetComposedIncomingDamageMultiplier() const;

    // Damage-Pipeline §4: at most three More multipliers, each capped at 1.30x.
    static constexpr float ComposedMoreCeiling = 2.20f;

    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDamageReceived OnDamageReceived;
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDeathEvent OnDeath;
    // Attacker-side (SI-8): raised on the component of whoever dealt the hit.
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHitDealt OnHitDealt;
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerKillDealt OnKillDealt;
    // Passive defensive layers: classes and gear raise these; no inputs.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float BlockChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float BlockMitigation = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float DodgeChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0")) float DodgeResourceRefund = 5.0f;

private:
    void PruneExpiredOutgoingModifiers();
    void DispatchHitDealt(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result) const;

    UPROPERTY() TArray<FBreakerOutgoingModifier> OutgoingModifiers;
    // Keyed so a pusher removes exactly its own entry. No expiry: an incoming
    // modifier reflects a state (Overcast, a defensive window) whose owner is
    // responsible for removing it, and a silently expiring defence is worse
    // than one that is visibly stuck.
    TMap<FName, float> IncomingDamageModifiers;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    bool bDeathBroadcast = false;
    double LastDamageTime = -1000.0;
};
