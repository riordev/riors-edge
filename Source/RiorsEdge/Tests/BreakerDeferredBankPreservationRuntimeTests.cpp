#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerElementSourceMath.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDeferredBankPreservationRuntimeTest,
    "RiorsEdge.Combat.Elements.DeferredBankPreservationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDeferredBankPreservationRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated native world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto MakePlayer = [&]()
    {
        auto* Player = World->SpawnActor<ABreakerCharacter>();
        if (!Player) return Player;
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr);
        Player->GetCombat()->BindAttributes(Attr); Player->GetProgression()->BindAttributes(Attr);
        Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Swift);
        Player->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(30, Player->GetProgression()->ExperienceCurve));
        return Player;
    };
    auto* Player = MakePlayer(); auto* Ally = MakePlayer();
    if (!TestNotNull(TEXT("Owner"), Player) || !TestNotNull(TEXT("Independent allied applier"), Ally)) return false;
    auto* Progression = Player->GetProgression();
    auto* Tree = UBreakerProgressionLibrary::GetCoreSliceTree(); FText Reason;
    auto Buy = [&](const TCHAR* Id, int32 Ranks = 1)
    {
        for (int32 Rank = 0; Rank < Ranks; ++Rank)
            if (!TestTrue(*FString::Printf(TEXT("Earned native Core purchase %s: %s"), Id, *Reason.ToString()), Progression->PurchaseNode(Tree, Id, Reason))) return false;
        return true;
    };
    auto Enemy = [&](float X)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E = World->SpawnActor<ABreakerEnemy>(FVector(X, 0, 100), FRotator::ZeroRotator, Spawn);
        if (!E) return E;
        E->ConfigureCrowdProbe(); E->SetAreaLevel(100); E->DispatchBeginPlay(); E->SetActorTickEnabled(false);
        if (auto* Move = E->FindComponentByClass<UBreakerEnemyMovementComponent>()) Move->SetComponentTickEnabled(false);
        E->FindComponentByClass<UBreakerCombatComponent>()->BindAttributes(FindObject<UBreakerAttributeSet>(E, TEXT("Attributes")));
        return E;
    };
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    auto Request = [&](ABreakerCharacter* Source, float Raw, EBreakerElement Element)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = Raw / Source->GetAttributes()->GetAbilityDamageMultiplier();
        Hit.Element = Element; Hit.ElementalFraction = 1; Hit.DamageFamily = EBreakerDamageFamily::Elemental;
        Hit.bCanCritical = false; Hit.SetInstigator(Source);
        UBreakerDamageLibrary::FillSourcePools(Source->GetAttributes(), EBreakerDamageDelivery::Ability, Hit);
        return Hit;
    };
    // Native Vestiges resist Entropy buildup. Fund the actual threshold after
    // source buildup modifiers and target resistance; do not change the chassis.
    auto ActivationRaw = [&](ABreakerCharacter* Source, UBreakerStatusComponent* Status, EBreakerElement Element)
    {
        const auto Probe = Request(Source, 1.f, Element);
        const float Threshold = Element == EBreakerElement::Entropy ? Status->GetEntropyThreshold() : Status->GetVoidThreshold();
        const float Resistance = Element == EBreakerElement::Entropy ? Status->GetEntropyResistancePercent() : Status->GetVoidResistancePercent();
        const float Factor = BreakerElementSource::BuildupMultiplier(Probe) * BreakerElementSource::ResistanceFactor(Probe, Resistance);
        TestTrue(TEXT("Native activation witness has nonzero buildup acceptance"), Factor > 0);
        return 1.1f * BreakerElementSource::Threshold(Probe, Threshold) / FMath::Max(Factor, UE_SMALL_NUMBER);
    };
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
        if (Node->Constellation == FName(TEXT("Reaction")))
            if (!Buy(*Node->NodeId.ToString(), Node->MaxRank)) return false;
    TestEqual(TEXT("Actual complete major price"),Progression->GetConstellationInvestment(Tree,TEXT("Reaction")),26);
    auto* Target=Enemy(17000);if(!Target)return false;
    auto* Status=Target->FindComponentByClass<UBreakerStatusComponent>();
    auto* Sink=Target->FindComponentByClass<UBreakerCombatComponent>();
    auto LockSource=[&](ABreakerCharacter* Source)
    {
        Sink->ReceiveDamage(Request(Source,ActivationRaw(Source,Status,EBreakerElement::Entropy),EBreakerElement::Entropy));
        if(!Status->HasStatus(Rot))return false;
        Sink->ReceiveDamage(Request(Source,1,EBreakerElement::Rift));
        bool Found=false;Status->ConsumeStatus(Rot,Found);Status->ConsumeStatus(Erased,Found);
        return true;
    };
    if(!LockSource(Player))return false;
    // Fresh late-lock deposits survive the normal buildup grace until unlock.
    Status->AdvanceStatuses(2.5f);
    Sink->ReceiveDamage(Request(Player,ActivationRaw(Player,Status,EBreakerElement::Entropy)*.4f,EBreakerElement::Entropy));
    TestFalse(TEXT("Locked owner's buildup does not apply Rot"),Status->HasStatus(Rot));
    const float Deferred=Status->GetEntropyBuildup();TestTrue(TEXT("Owner funded a positive bank"),Deferred>0);
    Sink->ReceiveDamage(Request(Ally,ActivationRaw(Ally,Status,EBreakerElement::Entropy),EBreakerElement::Entropy));
    if(!TestTrue(TEXT("Other source applies same status during lock"),Status->HasStatus(Rot)))return false;
    Status->AdvanceStatuses(.55f);
    const float BeforeRefusal=Status->GetEntropyBuildup();
    Sink->ReceiveDamage(Request(Player,1,EBreakerElement::Entropy));
    TestEqual(TEXT("Unlocked owner hit refused by ally status preserves bank"),Status->GetEntropyBuildup(),BeforeRefusal,.001f);
    bool Found=false;Status->ConsumeStatus(Rot,Found);
    Sink->ReceiveDamage(Request(Player,ActivationRaw(Player,Status,EBreakerElement::Entropy),EBreakerElement::Entropy));
    TestTrue(TEXT("Later accepted owner hit can claim preserved bank"),Status->HasStatus(Rot));
    Status->ConsumeStatus(Rot,Found);

    // A second earned owner creates a separate lock and bank on one target.
    auto* AllyProgression=Ally->GetProgression();
    for(const UBreakerProgressionNode* Node:Tree->Nodes)
        if(Node->Constellation==FName(TEXT("Reaction")))for(int32 Rank=0;Rank<Node->MaxRank;++Rank)
            if(!TestTrue(TEXT("Second source purchases the real major"),AllyProgression->PurchaseNode(Tree,Node->NodeId,Reason)))return false;
    auto* Pair=Enemy(20000);if(!Pair)return false;
    Status=Pair->FindComponentByClass<UBreakerStatusComponent>();Sink=Pair->FindComponentByClass<UBreakerCombatComponent>();
    if(!LockSource(Player)||!LockSource(Ally))return false;
    Status->AdvanceStatuses(2.5f);
    Sink->ReceiveDamage(Request(Player,ActivationRaw(Player,Status,EBreakerElement::Entropy),EBreakerElement::Entropy));
    Sink->ReceiveDamage(Request(Ally,ActivationRaw(Ally,Status,EBreakerElement::Entropy),EBreakerElement::Entropy));
    TestFalse(TEXT("Both same-element applications initially deferred"),Status->HasStatus(Rot));
    Status->AdvanceStatuses(.55f);
    if(!TestTrue(TEXT("First ready source obtains status slot"),Status->HasStatus(Rot)))return false;
    TestTrue(TEXT("Second source funding survives first replay's application"),Status->GetEntropyBuildup()>0);
    const auto* First=Status->GetActiveStatuses().FindByPredicate([&](const auto& A){return A.Spec.StatusTag==Rot;});
    const TWeakObjectPtr<AActor> FirstSource=First->Instigator;
    Status->ConsumeStatus(Rot,Found);
    Status->AdvanceStatuses(.01f);
    const auto* Second=Status->GetActiveStatuses().FindByPredicate([&](const auto& A){return A.Spec.StatusTag==Rot;});
    if(!TestNotNull(TEXT("Second funded replay remains available after slot clears"),Second))return false;
    TestTrue(TEXT("Deferred replay preserves distinct source ownership"),Second->Instigator!=FirstSource);
    return true;
}
#endif
