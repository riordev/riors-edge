#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "UObject/Object.h"
#include "BreakerCasterDamageSplitObserver.generated.h"

// Read-only victim-side measurement: accepted health only, never requested
// damage, shield loss or overkill. Element/status categories overlap timing.
UCLASS()
class UBreakerCasterDamageSplitObserver : public UObject
{
    GENERATED_BODY()
public:
    double Direct=0,Periodic=0,Bleed=0,Entropy=0,Void=0,Rift=0;
    int32 DirectHits=0,PeriodicHits=0,DirectCriticalHits=0;
    UFUNCTION() void Observe(const FBreakerHitContext& Hit)
    {
        const double Paid=Hit.Result.HealthDamage;
        if(Paid<=0)return;
        if(Hit.bFromDoT){Periodic+=Paid;++PeriodicHits;}
        else{Direct+=Paid;++DirectHits;if(Hit.Result.bCritical)++DirectCriticalHits;}
        if(Hit.DamageTypeTag.GetTagName()==FName(TEXT("Status.Bleed")))Bleed+=Paid;
        if(Hit.Element==EBreakerElement::Entropy)Entropy+=Paid;
        else if(Hit.Element==EBreakerElement::Void)Void+=Paid;
        else if(Hit.Element==EBreakerElement::Rift)Rift+=Paid;
    }
};
