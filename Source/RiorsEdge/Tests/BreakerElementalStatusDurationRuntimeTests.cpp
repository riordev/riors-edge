#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerElementSourceMath.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerElementalStatusDurationRuntimeTest,
    "RiorsEdge.Combat.Elements.StatusDurationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerElementalStatusDurationRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Native duration world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("Actual Caster"), Player)) return false;
    Player->bRefuseSavesForPendingCharacter = true;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr = Player->GetAttributes(); auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10, Progression->ExperienceCurve));
    auto* Tree = UBreakerProgressionLibrary::GetCoreSliceTree(); FText Reason;
    auto Buy = [&](const TCHAR* Id)
    {
        const bool Purchased = Progression->PurchaseNode(Tree, Id, Reason);
        return TestTrue(FString::Printf(TEXT("Earned purchase %s: %s"), Id, *Reason.ToString()), Purchased);
    };
    // Two adjacent real wedges; eight setup points, then each one-point link.
    for (const TCHAR* Id : {TEXT("Core.Entropy.Attunement"), TEXT("Core.Entropy.Conductive"), TEXT("Core.Entropy.Sympathy"),
        TEXT("Core.Reaction.Catalysis"), TEXT("Core.Reaction.Residue"), TEXT("Core.Reaction.SecondOrder")})
        if (!Buy(Id)) return false;
    TestEqual(TEXT("Both paid link paths leave two points"), Progression->GetProgressionState().UnspentCorePoints, 2);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Target = World->SpawnActor<ABreakerEnemy>(FVector(5000, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Native level-forty chassis"), Target)) return false;
    Target->ConfigureCrowdProbe(); Target->DispatchBeginPlay(); Target->SetAreaLevel(40); Target->SetActorTickEnabled(false);
    if (auto* Movement = Target->FindComponentByClass<UBreakerEnemyMovementComponent>()) Movement->SetComponentTickEnabled(false);
    auto* Combat = Target->FindComponentByClass<UBreakerCombatComponent>();
    Combat->BindAttributes(FindObject<UBreakerAttributeSet>(Target, TEXT("Attributes")));
    auto* Status = Target->FindComponentByClass<UBreakerStatusComponent>(); Status->SetComponentTickEnabled(false);
    auto* Observer = NewObject<UBreakerReactionRuntimeObserver>(Target);
    Combat->OnDamageTaken.AddDynamic(Observer, &UBreakerReactionRuntimeObserver::OnHit);
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    const auto Unstable = FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
    FBreakerDamageResult ApplyingResult;
    auto Earn = [&](EBreakerElement Element, FGameplayTag Tag, FBreakerActiveStatus& Out)
    {
        Status->ConsumeAllStatuses(); Status->AdvanceStatuses(30); Combat->RestoreVitals(); Observer->Hits.Reset();
        FBreakerDamageRequest Hit; Hit.BaseDamage = 1; Hit.Element = Element; Hit.ElementalFraction = 1;
        Hit.DamageFamily = EBreakerDamageFamily::Elemental; Hit.bCanCritical = false; Hit.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Attr, EBreakerDamageDelivery::Ability, Hit);
        const float Threshold = Element == EBreakerElement::Entropy ? Status->GetEntropyThreshold()
            : Element == EBreakerElement::Void ? Status->GetVoidThreshold() : Status->GetRiftThreshold();
        const float Resistance = Element == EBreakerElement::Entropy ? Status->GetEntropyResistancePercent()
            : Element == EBreakerElement::Void ? Status->GetVoidResistancePercent() : Status->GetRiftResistancePercent();
        const float Factor = BreakerElementSource::BuildupMultiplier(Hit) * BreakerElementSource::ResistanceFactor(Hit, Resistance);
        if (!TestTrue(TEXT("Authored source can build this element"), Factor > 0)) return false;
        Hit.BaseDamage = BreakerElementSource::Threshold(Hit, Threshold) / (4 * Factor * Attr->GetAbilityDamageMultiplier());
        for (int32 I = 0; I < 20 && !Status->HasStatus(Tag); ++I) ApplyingResult = Combat->ReceiveDamage(Hit);
        const auto* Active = Status->GetActiveStatuses().FindByPredicate([&](const auto& Entry) { return Entry.Spec.StatusTag == Tag; });
        if (!TestNotNull(TEXT("Native elemental hits create authored status"), Active)) return false;
        Out = *Active; return true;
    };
    FBreakerActiveStatus BaseRot, BaseVoid, BaseRift;
    if (!Earn(EBreakerElement::Entropy, Rot, BaseRot)) return false;
    const float BaseBudgetPerHit = BaseRot.InitialDamageBudget / ApplyingResult.RawDamage;
    const float BaseReactionPerHit = BaseRot.InitialReactionBudget / ApplyingResult.RawDamage;
    if (!Earn(EBreakerElement::Void, Erased, BaseVoid) || !Earn(EBreakerElement::Rift, Unstable, BaseRift)) return false;
    if (!Buy(TEXT("Core.Entropy.Sequence"))) return false;
    FBreakerActiveStatus SequenceRot;
    if (!Earn(EBreakerElement::Entropy, Rot, SequenceRot)) return false;
    TestEqual(TEXT("Paid Sequence extends periodic window by twelve percent"), SequenceRot.RemainingDuration, BaseRot.RemainingDuration * 1.12f, .001f);
    TestEqual(TEXT("Sequence redistributes unchanged per-hit ordinary funding"), SequenceRot.InitialDamageBudget / ApplyingResult.RawDamage, BaseBudgetPerHit, .001f);
    TestEqual(TEXT("Sequence preserves per-hit reaction funding"), SequenceRot.InitialReactionBudget / ApplyingResult.RawDamage, BaseReactionPerHit, .001f);
    Observer->Hits.Reset();
    Status->AdvanceStatuses(BaseRot.RemainingDuration);
    float SequencePaidAtBase = 0;
    for (const auto& Hit : Observer->Hits) if (Hit.bFromDoT) SequencePaidAtBase += Hit.Result.RawDamage;
    TestTrue(TEXT("Sequence spreads payment beyond the original expiry, not just the marker"), SequencePaidAtBase < SequenceRot.InitialDamageBudget);
    const auto* SequenceTail = Status->GetActiveStatuses().FindByPredicate([&](const auto& Entry) { return Entry.Spec.StatusTag == Rot; });
    if (!TestNotNull(TEXT("Sequence still has a funded final tick after original expiry"), SequenceTail)) return false;
    TestTrue(TEXT("Sequence tail retains unpaid finite damage"), SequenceTail->UnpaidDamageBudget > 0);
    Status->AdvanceStatuses(SequenceRot.RemainingDuration - BaseRot.RemainingDuration + .01f);
    float SequencePaidAtEnd = 0;
    for (const auto& Hit : Observer->Hits) if (Hit.bFromDoT) SequencePaidAtEnd += Hit.Result.RawDamage;
    TestEqual(TEXT("Sequence pays its full original budget at extended expiry"), SequencePaidAtEnd, SequenceRot.InitialDamageBudget, .001f);
    TestFalse(TEXT("Sequence finite application ends after final payment"), Status->HasStatus(Rot));
    if (!Buy(TEXT("Core.Reaction.Echo"))) return false;
    TestEqual(TEXT("Two status-duration links add to eighteen percent"), Progression->GetNodeStats().StatusDurationMultiplier, 1.18f, .0001f);
    TestEqual(TEXT("Real ten-point route exhausted"), Progression->GetProgressionState().UnspentCorePoints, 0);
    FBreakerActiveStatus CombinedRot, ExtendedVoid, ExtendedRift;
    if (!Earn(EBreakerElement::Void, Erased, ExtendedVoid) || !Earn(EBreakerElement::Rift, Unstable, ExtendedRift)) return false;
    TestEqual(TEXT("Erased's authored delayed payout is excluded"), ExtendedVoid.RemainingDuration, BaseVoid.RemainingDuration);
    TestEqual(TEXT("Unstable's authored marker is excluded"), ExtendedRift.RemainingDuration, BaseRift.RemainingDuration);
    if (!Earn(EBreakerElement::Entropy, Rot, CombinedRot)) return false;
    TestEqual(TEXT("Sequence plus Echo extend once, additively"), CombinedRot.RemainingDuration, BaseRot.RemainingDuration * 1.18f, .001f);
    TestEqual(TEXT("Combined duration does not mint ordinary damage"), CombinedRot.InitialDamageBudget / ApplyingResult.RawDamage, BaseBudgetPerHit, .001f);
    TestEqual(TEXT("Combined duration does not mint reaction damage"), CombinedRot.InitialReactionBudget / ApplyingResult.RawDamage, BaseReactionPerHit, .001f);
    Observer->Hits.Reset();
    if (!TestTrue(TEXT("Real low-level Core respec is free"), Progression->RespecCore(Reason))) return false;
    Status->AdvanceStatuses(BaseRot.RemainingDuration + .01f);
    TestTrue(TEXT("Purchased application remains active beyond original lifetime after respec"), Status->HasStatus(Rot));
    Status->AdvanceStatuses(CombinedRot.RemainingDuration - BaseRot.RemainingDuration + .02f);
    TestFalse(TEXT("Extended finite window still ends"), Status->HasStatus(Rot));
    float Paid = 0; for (const auto& Hit : Observer->Hits) if (Hit.bFromDoT) Paid += Hit.Result.RawDamage;
    TestEqual(TEXT("Extended ticks pay exactly the finite applying-hit budget"), Paid, CombinedRot.InitialDamageBudget, .001f);
    const float AlreadyPaid = Paid; Status->AdvanceStatuses(5);
    Paid = 0; for (const auto& Hit : Observer->Hits) if (Hit.bFromDoT) Paid += Hit.Result.RawDamage;
    TestEqual(TEXT("Expired status pays no second budget"), Paid, AlreadyPaid, .001f);
    FBreakerActiveStatus Fresh;
    if (!Earn(EBreakerElement::Entropy, Rot, Fresh)) return false;
    TestEqual(TEXT("Fresh post-respec status returns to authored window"), Fresh.RemainingDuration, BaseRot.RemainingDuration, .001f);
    return true;
}
#endif
