#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
#include "UI/BreakerHUDMath.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPaidCooldownHUDTest,"RiorsEdge.UI.PaidCooldownSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPaidCooldownHUDTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter=Frame; };
    auto* Player=World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    auto* Movement=Player->GetBreakerMovement(); Movement->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent(); auto* Attr=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr);
    auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Swift)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50,Progression->ExperienceCurve));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission:UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat:Mission.Beats)
            for (FName Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    FText Reason;
    // O272: Redirect opens its pair; no prerequisite, one point.
    auto* Tree=UBreakerProgressionLibrary::GetSwiftKineticTree();
    if (!TestTrue(TEXT("actual Redirect cooldown purchase"),Progression->PurchaseNode(Tree,TEXT("Swift.Kinetic.Redirect"),Reason))) return false;
    TestEqual(TEXT("Redirect alone spends one of the eight-point wallet"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),7);
    if (!Progression->IsAbilityUnlocked(TEXT("Swift.Sightline")) && !Progression->SpendAbilityToken(TEXT("Swift.Sightline"),Reason)) return false;
    constexpr auto Slot=EBreakerAbilitySlot::ClassAbilityOne;
    if (!TestTrue(TEXT("normal Sightline slot assignment"),Progression->EquipAbility(Slot,TEXT("Swift.Sightline"),Reason))) return false;
    auto* Abilities=Player->GetAbilities(); Abilities->RefreshGrants();
    auto* Momentum=Player->GetMomentum(); Momentum->BindAttributes(Attr); Momentum->BeginPlay(); Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking); Movement->Velocity=FVector(Movement->WalkSpeed,0,0);
    for (int32 Second=0;Second<40;++Second)
    { Player->SetActorLocation(Player->GetActorLocation()+Movement->Velocity); Momentum->AdvanceLoop(1); }
    Movement->StopMovementImmediately();
    Movement->SetMovementMode(MOVE_Falling); Progression->RefreshBuildConditions();
    const float Before=Momentum->GetMomentum();
    if (!TestTrue(TEXT("actual paid slot cast"),Abilities->TryActivateSlot(Slot))) return false;
    TestTrue(TEXT("cast spends earned Momentum"),Momentum->GetMomentum()<Before);
    FGameplayEffectQuery Query; Query.EffectDefinition=UBreakerAbilityCooldownEffect::StaticClass();
    const auto Handles=ASC->GetActiveEffects(Query);
    if (!TestEqual(TEXT("one actual cooldown"),Handles.Num(),1)) return false;
    const auto* Effect=ASC->GetActiveGameplayEffect(Handles[0]);
    if (!Effect) return false;
    const float Duration=Effect->Spec.GetDuration();
    TestTrue(TEXT("purchased airborne reduction affects actual snapshot"),Duration<Abilities->GetDefinitionForSlot(Slot)->CooldownSeconds);
    TestEqual(TEXT("HUD duration matches active snapshot"),Abilities->GetCooldownDuration(Slot),Duration,.001f);
    TestEqual(TEXT("fresh reduced cooldown starts fully unrecovered"),BreakerHUDMath::AbilityRecoveryFraction(Abilities->GetCooldownRemaining(Slot),Abilities->GetCooldownDuration(Slot)),0.f,.001f);
    Movement->SetMovementMode(MOVE_Walking); Progression->RefreshBuildConditions();
    TestEqual(TEXT("leaving airborne does not rewrite active duration"),Abilities->GetCooldownDuration(Slot),Duration,.001f);
    for (int32 Tick=0;Tick<20;++Tick) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); }
    TestEqual(TEXT("live radial uses elapsed time against snapshot"),BreakerHUDMath::AbilityRecoveryFraction(Abilities->GetCooldownRemaining(Slot),Abilities->GetCooldownDuration(Slot)),1.f/Duration,.01f);
    return true;
}
#endif
