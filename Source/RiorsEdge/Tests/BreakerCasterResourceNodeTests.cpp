#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Resonance.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Data/BreakerDataFile.h"
#include "Engine/World.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BreakerCasterResourceNodeTest
{
    struct FVictim
    {
        AActor* Actor;
        UBreakerCombatComponent* Combat;
        UBreakerStatusComponent* Status;
    };
    FVictim MakeVictim(UWorld* World, FVector Location = FVector(500, 0, 0))
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USphereComponent* Body = NewObject<USphereComponent>(Actor);
        Actor->AddInstanceComponent(Body);
        Actor->SetRootComponent(Body);
        Body->SetSphereRadius(40);
        Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Body->SetCollisionResponseToAllChannels(ECR_Ignore);
        Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
        Body->RegisterComponent();
        Actor->SetActorLocation(Location);
        UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat);
        Combat->RegisterComponent();
        UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Actor);
        Attributes->ApplyMaxHealth(10000);
        Attributes->ApplyHealth(10000);
        Combat->BindAttributes(Attributes);
        UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Actor);
        Actor->AddInstanceComponent(Status);
        Status->RegisterComponent();
        return {Actor, Combat, Status};
    }
    ABreakerCharacter* MakeCaster(UWorld* World, int32 DoctrineBudget = 2)
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ABreakerCharacter* Caster = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Caster, Caster);
        ASC->AddAttributeSetSubobject(Caster->GetAttributes());
        Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
        Caster->GetProgression()->BindAttributes(Caster->GetAttributes());
        FBreakerProgressionState State;
        State.PermanentClass = EBreakerClassId::Caster;
        State.UnspentDoctrinePoints = DoctrineBudget;
        Caster->GetProgression()->LoadProgressionState(State);
        Caster->GetMana()->PassiveRegenPerSecond = 0;
        Caster->GetMana()->BindAttributes(Caster->GetAttributes());
        Caster->GetAttributes()->ApplyClassResource(0);
        return Caster;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterStatusResourceRuntimeTest,
    "RiorsEdge.Classes.Mana.StatusResourceNodesRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterStatusResourceRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterResourceNodeTest;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    // This fixture's subject is purchased-node wiring across several trees.
    // Eight is the authored eventual pool, not today's two-point campaign
    // entitlement. The separate Seep fixture below uses that live entitlement.
    ABreakerCharacter* Caster = MakeCaster(World, 8);
    UBreakerManaComponent* Mana = Caster->GetMana();
    auto Buy = [&](const UBreakerProgressionTree* Tree, const TCHAR* Id)
    {
        FText Error;
        return TestTrue(Id, Caster->GetProgression()->PurchaseNode(Tree, Id, Error));
    };
    const UBreakerProgressionTree* Void = UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    const UBreakerProgressionTree* Blade = UBreakerProgressionLibrary::GetCasterSpellbladeTree();
    FVictim Victim = MakeVictim(World);
    FBreakerStatusApplicationSpec Poison;
    Poison.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Poison.BaseDamagePerTick = 1;
    Poison.Duration = 10;
    Poison.TickInterval = 1;
    auto ApplyPoison = [&] { Victim.Status->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Caster); };
    ApplyPoison();
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("New status earns independent base income"), Mana->GetMana(), 2.0f);
    ApplyPoison();
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Ordinary refresh pays nothing"), Mana->GetMana(), 2.0f);
    for (int32 Rank = 1; Rank <= 2; ++Rank)
    {
        if (!Buy(Void, TEXT("Caster.VoidWhisperer.Seep"))) return false;
        Victim.Status->ConsumeAllStatuses();
        const float Before = Mana->GetMana();
        ApplyPoison();
        Mana->AdvanceLoop(1);
        TestEqual(TEXT("Purchased Seep rank scales real status income"), Mana->GetMana() - Before, Rank == 1 ? 3.0f : 4.0f);
    }
    if (!Buy(Blade, TEXT("Caster.Spellblade.FollowThrough"))) return false;
    FBreakerStatusApplicationSpec Bleed = Poison;
    Bleed.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    Bleed.Snapshot.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
    Victim.Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Caster);
    Mana->AdvanceLoop(1);
    const float BeforeRefresh = Mana->GetMana();
    Victim.Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Caster);
    Victim.Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Caster);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Follow Through refreshes share six-per-second cap"), Mana->GetMana() - BeforeRefresh, 6.0f);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Remaining refresh income is metered next second"), Mana->GetMana() - BeforeRefresh, 8.0f);

    const float BeforeZeroProc = Mana->GetMana();
    FBreakerStatusApplicationSpec Secondary = Bleed;
    Secondary.ProcCoefficient = 0;
    Victim.Status->ApplyStatus(Secondary, EBreakerDamageFamily::Physical, Caster);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Zero-proc refresh cannot generate Mana"), Mana->GetMana(), BeforeZeroProc);
    Mana->PushGenerationSuspension(TEXT("Test.Unmake"));
    Victim.Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Caster);
    Mana->PopGenerationSuspension(TEXT("Test.Unmake"));
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Suspended status income is discarded"), Mana->GetMana(), BeforeZeroProc);

    AActor* OtherKiller = World->SpawnActor<AActor>();
    FBreakerDamageRequest Kill;
    Kill.BaseDamage = 1000000;
    Kill.bCanCritical = false;
    Kill.SetInstigator(OtherKiller);
    for (int32 Rank = 1; Rank <= 2; ++Rank)
    {
        if (!Buy(Void, TEXT("Caster.VoidWhisperer.Attrition"))) return false;
        FVictim Afflicted = MakeVictim(World, FVector(1000 + Rank * 100, 0, 0));
        Afflicted.Status->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Caster);
        Afflicted.Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Caster);
        Mana->AdvanceLoop(2);
        const float Before = Mana->GetMana();
        TestTrue(TEXT("Another attacker really kills afflicted victim"), Afflicted.Combat->ReceiveDamage(Kill).bKilled);
        Afflicted.Combat->ReceiveDamage(Kill);
        TestEqual(TEXT("Attrition pays once per victim, not per status or corpse hit"), Mana->GetMana() - Before, Rank == 1 ? 4.0f : 8.0f);
    }
    FVictim ProcVictim = MakeVictim(World, FVector(1500, 0, 0));
    ProcVictim.Status->ApplyStatus(Secondary, EBreakerDamageFamily::Physical, Caster);
    const float BeforeProcKill = Mana->GetMana();
    ProcVictim.Combat->ReceiveDamage(Kill);
    TestEqual(TEXT("A secondary-only status earns no Attrition refund"), Mana->GetMana(), BeforeProcKill);
    ProcVictim.Status->ApplyStatus(Bleed, EBreakerDamageFamily::Physical, Caster);
    FBreakerStatusApplicationSpec Invalid = Poison;
    Invalid.StatusTag = FGameplayTag();
    Victim.Status->ApplyStatus(Invalid, EBreakerDamageFamily::Physical, Caster);
    Mana->AdvanceLoop(1);
    TestEqual(TEXT("Corpse and invalid-tag applications generate no Mana"), Mana->GetMana(), BeforeProcKill);

    FVictim LastTickVictim = MakeVictim(World, FVector(1600, 0, 0));
    FBreakerStatusApplicationSpec LastTick = Poison;
    LastTick.BaseDamagePerTick = 1000000;
    LastTick.Duration = LastTick.TickInterval = 1;
    LastTickVictim.Status->ApplyStatus(LastTick, EBreakerDamageFamily::Physical, Caster);
    Mana->AdvanceLoop(1);
    const float BeforeLastTick = Mana->GetMana();
    LastTickVictim.Status->TickComponent(1, LEVELTICK_All, nullptr);
    TestTrue(TEXT("The final owed status tick actually kills"), LastTickVictim.Combat->IsDead());
    TestEqual(TEXT("A lethal expiry tick still pays Attrition once"), Mana->GetMana() - BeforeLastTick, 8.0f);
    LastTickVictim.Status->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("An expired corpse cannot pay again"), Mana->GetMana() - BeforeLastTick, 8.0f);

    Kill.SetInstigator(Caster);
    Kill.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
    for (int32 Rank = 1; Rank <= 2; ++Rank)
    {
        if (Rank == 2 && !Buy(Blade, TEXT("Caster.Spellblade.FollowThrough"))) return false;
        FVictim Cleaved = MakeVictim(World, FVector(1700 + Rank * 100, 0, 0));
        const float Before = Mana->GetMana();
        TestTrue(TEXT("Real direct Cleave kill"), Cleaved.Combat->ReceiveDamage(Kill).bKilled);
        TestEqual(TEXT("Follow Through rank kill refund"), Mana->GetMana() - Before, Rank == 1 ? 3.0f : 6.0f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterSeepCurrentBudgetTest,
    "RiorsEdge.Classes.Mana.SeepCurrentCampaignBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterSeepCurrentBudgetTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterResourceNodeTest;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated current-budget world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    ABreakerCharacter* Caster = MakeCaster(World);
    FVictim Victim = MakeVictim(World);
    FBreakerStatusApplicationSpec Poison;
    Poison.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Poison.BaseDamagePerTick = 1;
    Poison.Duration = 4;
    Poison.TickInterval = 1;
    FText Error;
    for (int32 Rank = 1; Rank <= 2; ++Rank)
    {
        if (!TestTrue(TEXT("Seep rank fits today's two-point entitlement"), Caster->GetProgression()->PurchaseNode(
            UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), TEXT("Caster.VoidWhisperer.Seep"), Error))) return false;
        Victim.Status->ConsumeAllStatuses();
        const float Before = Caster->GetMana()->GetMana();
        Victim.Status->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Caster);
        Caster->GetMana()->AdvanceLoop(1);
        TestEqual(TEXT("Current-budget Seep purchase changes real application income"),
            Caster->GetMana()->GetMana() - Before, Rank == 1 ? 3.0f : 4.0f);
    }
    TestEqual(TEXT("Both earned points were spent"), Caster->GetProgression()->GetProgressionState().UnspentDoctrinePoints, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterPaymentRuntimeTest,
    "RiorsEdge.Classes.Mana.PaymentRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterPaymentRuntimeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterResourceNodeTest;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated payment world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    // Payment rank two requires four doctrine points including prerequisites.
    // This tests its authored purchase/cast wiring, not campaign reachability;
    // the current campaign still grants only two and that gap remains pinned.
    ABreakerCharacter* Caster = MakeCaster(World, 4);
    UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Ability = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Resonance::StaticClass(), 1));
    FVictim Victim = MakeVictim(World);
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetCasterMultispellTree();
    FText Error;
    if (!TestTrue(TEXT("Cycle prerequisite rank1"), Caster->GetProgression()->PurchaseNode(Tree, TEXT("Caster.Multispell.Cycle"), Error))
        || !TestTrue(TEXT("Cycle investment rank2"), Caster->GetProgression()->PurchaseNode(Tree, TEXT("Caster.Multispell.Cycle"), Error))) return false;
    for (int32 Rank = 0; Rank <= 2; ++Rank)
    {
        if (Rank && !TestTrue(TEXT("Real Payment purchase"), Caster->GetProgression()->PurchaseNode(Tree, TEXT("Caster.Multispell.Payment"), Error))) return false;
        for (const TCHAR* Tag : {TEXT("Status.Bleed"), TEXT("Status.Poison")})
        {
            FBreakerStatusApplicationSpec Spec;
            Spec.StatusTag = FGameplayTag::RequestGameplayTag(Tag);
            Spec.BaseDamagePerTick = 1;
            Spec.Duration = 10;
            Spec.TickInterval = 1;
            // One ordinary type and one secondary type: both detonate, only
            // the ordinary application is eligible for a resource refund.
            Spec.ProcCoefficient = Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Poison")) ? 0 : 1;
            Victim.Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, Caster);
        }
        Caster->GetMana()->AdvanceLoop(1);
        Caster->GetAttributes()->ApplyClassResource(80);
        TestTrue(TEXT("Real Resonance GAS cast"), ASC->TryActivateAbility(Ability));
        BreakerResolvePendingCast(World, Caster);
        TestEqual(TEXT("Payment pays only eligible distinct type after forty-Mana cost"), Caster->GetMana()->GetMana(), 40.0f + Rank * 2.0f);
        TestEqual(TEXT("Baseline consumption still removes all statuses"), Victim.Status->GetDistinctStatusTypeCount(), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterResourceTuningTest,
    "RiorsEdge.Data.CasterResourceTuning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterResourceTuningTest::RunTest(const FString& Parameters)
{
    BreakerDataFile::FBreakerDataErrors Errors;
    TSharedPtr<FJsonObject> Object = BreakerDataFile::Load(TEXT("Data/caster-resource.json"), Errors);
    if (!TestTrue(TEXT("shipped resource JSON loads"), Object.IsValid() && Errors.IsClean())) return false;
    FBreakerCasterResourceTuning Tuning;
    FString Error;
    TestTrue(TEXT("all shipped fields validate"), UBreakerManaComponent::ParseResourceTuning(*Object, Tuning, Error));
    TestEqual(TEXT("runtime loader uses authored status rate"), UBreakerManaComponent::GetResourceTuning().StatusApplicationMana, Tuning.StatusApplicationMana);
    const float Before = Tuning.StatusApplicationMana;
    Object->SetNumberField(TEXT("StatusApplicationMana"), -1);
    TestFalse(TEXT("negative rate refused"), UBreakerManaComponent::ParseResourceTuning(*Object, Tuning, Error));
    TestEqual(TEXT("invalid data does not partially overwrite output"), Tuning.StatusApplicationMana, Before);
    Object->RemoveField(TEXT("StatusApplicationMana"));
    TestFalse(TEXT("missing field refused"), UBreakerManaComponent::ParseResourceTuning(*Object, Tuning, Error));
    return true;
}
#endif
