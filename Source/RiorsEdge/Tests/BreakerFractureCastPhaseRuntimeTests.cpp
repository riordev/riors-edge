#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFractureCastPhaseRuntimeTest, "RiorsEdge.Abilities.FractureCastPhaseRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFractureCastPhaseRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    auto* Combat = Player->GetCombat(); Combat->BindAttributes(Attr); Combat->SetComponentTickEnabled(false);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    auto* Mana = Player->GetMana(); Mana->BindAttributes(Attr); Mana->SetComponentTickEnabled(false); Mana->AdvanceLoop(30);
    auto* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Player);
    if (!TestTrue(TEXT("Starter has real cycle"), Cycle && Cycle->GetCycleLength() > 1)) return false;
    auto Tick = [&](float Seconds) { for (float Elapsed = 0; Elapsed < Seconds; Elapsed += .005f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .005f); } };
    auto Shots = [&]() { TArray<ABreakerProjectileBase*> Result; for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) if (It->GetOwner() == Player && !It->IsActorBeingDestroyed()) Result.Add(*It); return Result; };
    auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    auto Active = [&]() { const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle); return Spec && Spec->IsActive(); };
    const float Base = GetDefault<UBreakerAbility_Fracture>()->BaseCastSeconds;
    const int32 Cursor = Cycle->GetCursor(); const float Before = Mana->GetMana();
    if (!TestTrue(TEXT("Paid starter cast accepted"), ASC->TryActivateAbility(Handle))) return false;
    const float Paid = Mana->GetMana();
    TestTrue(TEXT("Cost paid before windup"), Paid < Before);
    TestTrue(TEXT("GAS remains active"), Active()); TestEqual(TEXT("No instant projectile"), Shots().Num(), 0);
    TestFalse(TEXT("Repeated input cannot overlap same cast"), ASC->TryActivateAbility(Handle));
    TestEqual(TEXT("Repeated input costs nothing"), Mana->GetMana(), Paid, .001f);
    Tick(Base * .45f); TestEqual(TEXT("No early emission"), Shots().Num(), 0); TestEqual(TEXT("Windup reserves but does not advance cycle"), Cycle->GetCursor(), Cursor);
    Tick(Base * .6f); auto Emitted = Shots();
    if (!TestEqual(TEXT("One emission at completion"), Emitted.Num(), 1)) return false;
    TestFalse(TEXT("Completion ends GAS"), Active()); TestEqual(TEXT("Successful emission advances once"), Cycle->GetCursor(), (Cursor + 1) % Cycle->GetCycleLength());
    Emitted[0]->Destroy();
    Mana->AdvanceLoop(30); const int32 CancelCursor = Cycle->GetCursor();
    if (!ASC->TryActivateAbility(Handle)) return false;
    ASC->CancelAbilityHandle(Handle); Tick(Base * 1.1f);
    TestEqual(TEXT("Cancel discards projectile"), Shots().Num(), 0); TestEqual(TEXT("Cancel does not advance"), Cycle->GetCursor(), CancelCursor);

    Mana->AdvanceLoop(30);
    if (!ASC->TryActivateAbility(Handle)) return false;
    ASC->ClearAbility(Handle); Tick(Base * 1.1f);
    TestEqual(TEXT("Removing ability discards pending cast"), Shots().Num(), 0);
    TestEqual(TEXT("Removing ability preserves cursor"), Cycle->GetCursor(), CancelCursor);
    Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));

    auto* Definition = DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition, Player);
    auto* Tree = NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId = TEXT("Test.Fracture.Tempo"); Tree->Currency = EBreakerPointCurrency::CorePoints;
    Definition->BranchTrees.Add(Tree); Progression->ClassDefinition = Definition;
    auto* Rate = NewObject<UBreakerProgressionNode>(Tree); Rate->NodeId = TEXT("Test.Fracture.Metronome"); Rate->Currency = Tree->Currency;
    // Supplied Core gateway magnitudes, bought separately with earned points.
    FBreakerNodeEffect Effect; Effect.StatTarget = EBreakerNodeStatTarget::AbilityCastRate;
    Effect.StatBucket = EBreakerNodeStatBucket::IncreasedPercent; Effect.ValuePerRank = 6; Rate->Effects.Add(Effect); Tree->Nodes.Add(Rate);
    auto* Prime = NewObject<UBreakerProgressionNode>(Tree); Prime->NodeId = TEXT("Test.Fracture.Prime"); Prime->Currency = Tree->Currency;
    Effect.StatTarget = EBreakerNodeStatTarget::AbilityDamage; Effect.ValuePerRank = 8; Prime->Effects.Add(Effect); Tree->Nodes.Add(Prime);
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(3, Progression->ExperienceCurve));
    FText Reason; if (!Progression->PurchaseNode(Tree, Rate->NodeId, Reason) || !Progression->PurchaseNode(Tree, Prime->NodeId, Reason)) return false;
    Mana->AdvanceLoop(30);
    const float PaidPower = Attr->GetAbilityDamageMultiplier();
    const float PaidDuration = Base / UBreakerGameplayAbility::AbilityCastRateMultiplierFor(Player);
    const float PaidStart = World->GetTimeSeconds();
    if (!ASC->TryActivateAbility(Handle)) return false;
    if (!Progression->RespecCore(Reason)) return false;
    Tick(PaidDuration - .015f); TestTrue(TEXT("Purchased rate still has actual windup"), Active());
    Tick(.02f); TestFalse(TEXT("Paid cast retains snapshotted rate through respec"), Active());
    TestTrue(TEXT("Supplied Metronome emits before ordinary base duration"), World->GetTimeSeconds() - PaidStart < Base);
    Emitted = Shots(); if (!TestEqual(TEXT("Faster paid cast emits once"), Emitted.Num(), 1)) return false;
    TestEqual(TEXT("Emitted damage preserves paid source through respec"), Emitted[0]->GetProjectileDamage().SourceDamageMultiplier, PaidPower, .001f); Emitted[0]->Destroy();
    auto* Conduction = NewObject<UBreakerProgressionNode>(Tree); Conduction->NodeId = TEXT("Test.Fracture.Conduction"); Conduction->Currency = Tree->Currency;
    Conduction->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Conduction"))); Tree->Nodes.Add(Conduction);
    if (!Progression->PurchaseNode(Tree, Conduction->NodeId, Reason)) return false;
    Mana->AdvanceLoop(30);
    if (!ASC->TryActivateAbility(Handle)) return false;
    TestTrue(TEXT("Conduction leaves cast phase active"), Active()); TestEqual(TEXT("Conduction cannot emit instantly"), Shots().Num(), 0);
    TestFalse(TEXT("Conduction cannot bypass in-flight GAS cast"), ASC->TryActivateAbility(Handle));
    ASC->CancelAbilityHandle(Handle); Tick(Base * 1.1f);
    if (!Progression->RespecCore(Reason)) return false;
    Mana->AdvanceLoop(30); const int32 DeathCursor = Cycle->GetCursor();
    if (!ASC->TryActivateAbility(Handle)) return false;
    auto* Enemy = World->SpawnActor<AActor>(); FBreakerDamageRequest Kill; Kill.BaseDamage = 100000; Kill.DamageFamily = EBreakerDamageFamily::TrueDamage; Kill.bCanCritical = false; Kill.SetInstigator(Enemy); Combat->ReceiveDamage(Kill);
    TestTrue(TEXT("Real hostile damage kills caster"), Combat->IsDead()); Tick(Base * 1.1f);
    TestFalse(TEXT("Death ends active cast"), Active()); TestEqual(TEXT("Death discards emission"), Shots().Num(), 0); TestEqual(TEXT("Death preserves cycle"), Cycle->GetCursor(), DeathCursor);
    return true;
}
#endif
