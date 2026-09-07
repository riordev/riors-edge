#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Combat/BreakerZoneMath.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBlackoutProtocolRuntimeTest, "RiorsEdge.Abilities.BlackoutProtocolRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerBlackoutProtocolRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    Player->SetActorLocation(FVector(0, 0, 200));
    Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), FRotator::ZeroRotator);
    Controller->Possess(Player);
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    UBreakerProgressionComponent* Progression = Player->GetProgression();
    Progression->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("actual Support selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Support))) return false;
    Progression->GrantPlaytestPoints(8, 0); // Full authored budget wiring, not current campaign entitlement.
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetSupportWardenTree();
    for (const TCHAR* Node : { TEXT("Support.Warden.FieldOfView"), TEXT("Support.Warden.FieldOfView"),
        TEXT("Support.Warden.Pressure"), TEXT("Support.Warden.Pressure"), TEXT("Support.Warden.Suppression") })
    { FText Reason; if (!TestTrue(Node, Progression->PurchaseNode(Tree, Node, Reason))) return false; }
    UBreakerChargeComponent* Charge = Player->FindComponentByClass<UBreakerChargeComponent>();
    Charge->BindAttributes(Player->GetAttributes());
    auto SetCharge = [&](float Amount) { Player->GetAttributes()->ApplyClassResource(Amount); Charge->AdvanceLoop(0.001f); };
    ABreakerEnemy* Target = World->SpawnActor<ABreakerEnemy>();
    if (!TestNotNull(TEXT("actual enemy"), Target)) return false;
    // Enemy hitboxes keep their shipped channel responses for both casts.
    Target->SetActorLocation(FVector(2800, 0, 100));
    UBreakerCombatComponent* Combat = Target->FindComponentByClass<UBreakerCombatComponent>();
    UBreakerAttributeSet* Health = Cast<UBreakerAttributeSet>(Target->GetDefaultSubobjectByName(TEXT("Attributes")));
    Target->GetAbilitySystemComponent()->InitAbilityActorInfo(Target, Target);
    Target->GetAbilitySystemComponent()->AddAttributeSetSubobject(Health);
    Combat->BindAttributes(Health); Health->ApplyMaxHealth(1000); Health->ApplyHealth(500);
    FVector Eye; FRotator View; Controller->GetPlayerViewPoint(Eye, View);
    Controller->SetControlRotation((Target->GetActorLocation() - Eye).Rotation());
    SetCharge(100);
    const FGameplayAbilitySpecHandle Mark = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Mark::StaticClass(), 1));
    if (!TestTrue(TEXT("actual GAS Mark"), ASC->TryActivateAbility(Mark))) return false;
    UBreakerAbilityStateComponent* State = Player->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!TestTrue(TEXT("cast actually marked enemy"), State && State->IsMarked(Target))) return false;
    AActor* Ground = World->SpawnActor<AActor>();
    UBoxComponent* Floor = NewObject<UBoxComponent>(Ground);
    Ground->AddInstanceComponent(Floor); Ground->SetRootComponent(Floor);
    Floor->SetBoxExtent(FVector(4000, 2000, 10));
    Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Floor->SetCollisionResponseToAllChannels(ECR_Block);
    Floor->RegisterComponent(); Ground->SetActorLocation(FVector(0, 0, -10));
    Controller->GetPlayerViewPoint(Eye, View);
    Controller->SetControlRotation((FVector(2800, 0, 0) - Eye).Rotation());
    SetCharge(100);
    const FGameplayAbilitySpecHandle Suppress = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Suppress::StaticClass(), 1));
    if (!TestTrue(TEXT("actual GAS Suppress"), ASC->TryActivateAbility(Suppress))) return false;
    ABreakerZoneActor* Zone = nullptr;
    for (TActorIterator<ABreakerZoneActor> It(World); It; ++It) if (It->GetZoneInstigator() == Player) Zone = *It;
    if (!TestNotNull(TEXT("cast actually created field"), Zone)) return false;
    TestEqual(TEXT("Suppress uses actual ground aim"), Zone->GetActorLocation().Z, 0.0, 0.1);
    Zone->AdvanceZone(0.01f); SetCharge(100);
    if (!TestTrue(FString::Printf(TEXT("actual field contains target: field %s target %s"),
        *Zone->GetActorLocation().ToString(), *Target->GetActorLocation().ToString()),
        UBreakerZoneMath::IsInsideZone(Zone->GetActorLocation(), Zone->GetSpec().RadiusCm,
            Zone->GetSpec().HalfHeightCm, Target->GetActorLocation()))
        || !TestTrue(TEXT("actual field membership installed"), Zone->GetOccupantCount() > 0)) return false;
    TestFalse(TEXT("unowned rewrite does nothing"), Combat->IsBeneficialEffectSuppressed());
    FText Reason;
    if (!TestTrue(TEXT("actual final node purchase"), Progression->PurchaseNode(Tree, TEXT("Support.Warden.BlackoutProtocol"), Reason))) return false;
    if (!TestTrue(TEXT("purchased rewrite tag is active"), Progression->HasNodeTag(BreakerNodeTags::Node_WA_BlackoutProtocol.GetTag()))
        || !TestTrue(TEXT("Charge source is active Support"), Charge->IsActiveForOwner())
        || !TestEqual(TEXT("live Charge is Resonant"), Charge->GetChargeBand(), EBreakerChargeBand::Resonant)) return false;
    TestTrue(TEXT("live purchase enables existing marked field"), Combat->IsBeneficialEffectSuppressed());
    const float Before = Health->GetHealth();
    Combat->ApplyHealingAmount(100, Player, FGameplayTag());
    TestEqual(TEXT("actual healing refused"), Health->GetHealth(), Before);
    const float MarkVulnerability = Combat->GetComposedIncomingDamageMultiplier();
    TestTrue(TEXT("actual Mark vulnerability remains a debuff"), MarkVulnerability > 1);
    Combat->PushIncomingDamageModifier(TEXT("Test.BossGate"), 0.5f);
    ABreakerEnemy* Warder = World->SpawnActor<ABreakerEnemy>();
    Warder->SetActorLocation(Target->GetActorLocation() + FVector(100, 0, 0));
    UBreakerEnemyModifierComponent* Aura = Warder->FindComponentByClass<UBreakerEnemyModifierComponent>();
    if (!TestTrue(TEXT("actual warding aura modifier"), Aura && Aura->SetModifiers({ EBreakerEnemyModifier::WardingAura }))) return false;
    Aura->AdvanceModifiers(0.3f);
    const float AuraFactor = 1.0f - Aura->Params.AuraDamageReduction;
    TestEqual(TEXT("buff suppressed while mechanical boss gate and Mark survive"), Combat->GetComposedIncomingDamageMultiplier(), MarkVulnerability * 0.5f);
    SetCharge(50);
    TestFalse(TEXT("Attuned immediately releases"), Combat->IsBeneficialEffectSuppressed());
    TestEqual(TEXT("existing live aura restores without repush"), Combat->GetComposedIncomingDamageMultiplier(), MarkVulnerability * 0.5f * AuraFactor, 0.001f);
    Combat->ApplyHealingAmount(100, Player, FGameplayTag());
    TestTrue(TEXT("real healing works again"), Health->GetHealth() > Before);
    SetCharge(0); TestFalse(TEXT("Cold excluded"), Combat->IsBeneficialEffectSuppressed());
    SetCharge(100); State->ClearMark();
    TestFalse(TEXT("mark removal immediately releases"), Combat->IsBeneficialEffectSuppressed());
    State->SetMark(Target, 10); // Subsequent live mark-state edges isolate the lease predicate.
    TestTrue(TEXT("mark restoration reactivates"), Combat->IsBeneficialEffectSuppressed());
    UBreakerEnemyModifierComponent* Ward = Target->FindComponentByClass<UBreakerEnemyModifierComponent>();
    if (!TestTrue(TEXT("actual rechargeable ward"), Ward && Ward->SetModifiers({ EBreakerEnemyModifier::Warded }))) return false;
    Ward->Params.WardRechargeDelaySeconds = 0;
    Health->ApplyShield(0);
    Ward->AdvanceModifiers(1);
    TestEqual(TEXT("new ward recharge is blocked"), Health->GetShield(), 0.0f);
    SetCharge(50); Ward->AdvanceModifiers(1);
    TestTrue(TEXT("ward recharge resumes outside Resonant"), Health->GetShield() > 0);
    SetCharge(100);
    FBreakerDamageRequest Lethal;
    Lethal.BaseDamage = 1000000; Lethal.bCanCritical = false; Lethal.bBypassShield = true;
    Player->GetCombat()->ReceiveDamage(Lethal);
    if (!TestTrue(TEXT("source actually died"), Player->GetCombat()->IsDead())) return false;
    TestFalse(TEXT("dead caster cannot retain suppression"), Combat->IsBeneficialEffectSuppressed());
    Player->GetCombat()->RestoreVitals(); SetCharge(100);
    const FVector Inside = Target->GetActorLocation(); Target->SetActorLocation(Inside + FVector(0, 0, 1000));
    TestFalse(TEXT("vertical exit releases before next membership tick"), Combat->IsBeneficialEffectSuppressed());
    Target->SetActorLocation(Inside);
    ABreakerZoneActor* Second = World->SpawnActor<ABreakerZoneActor>();
    Second->SetActorLocation(Zone->GetActorLocation()); Second->ConfigureZone(Zone->GetSpec(), Player);
    Second->AdvanceZone(0.01f); // Actual second zone membership, no second GAS cast claim.
    Zone->AdvanceZone(100);
    TestTrue(TEXT("other live zone lease survives first expiry"), Combat->IsBeneficialEffectSuppressed());
    Second->ReleaseAllOccupants();
    TestTrue(TEXT("explicit release can retain positive lifetime"), Second->GetRemainingDuration() > 0);
    TestFalse(TEXT("explicit release immediately ends suppression"), Combat->IsBeneficialEffectSuppressed());
    Second->Destroy();
    TestFalse(TEXT("destroyed lease cannot retain block"), Combat->IsBeneficialEffectSuppressed());
    return true;
}
#endif
