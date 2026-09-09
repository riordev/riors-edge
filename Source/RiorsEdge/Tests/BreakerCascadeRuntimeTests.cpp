#include "Tests/BreakerFractureTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCascadeRuntimeTest,
    "RiorsEdge.Abilities.CascadePurchasedRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCascadeRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated Cascade world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT { GFrameCounter = SavedFrame; World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto Advance = [&]()
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, .05f);
        if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
    };
    const auto Bleed = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    const auto Poison = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Each scenario starts from a new ordinary Mana pool. The restored final
    // benchmark/XP fixture proves entitlement spending, not campaign acquisition.
    // Projectile Impact is the real server collision seam, not a flight claim.
    for (int32 Scenario = 0; Scenario < 5; ++Scenario)
    {
        const FVector Origin(Scenario * 10000.0f, 0, 200);
        auto* Caster = World->SpawnActor<ABreakerCharacter>(Origin, FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("native caster"), Caster)) return false;
        Caster->SetActorTickEnabled(false);
        Caster->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Caster->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Caster, Caster);
        ASC->AddAttributeSetSubobject(Caster->GetAttributes());
        Caster->GetCombat()->BindAttributes(Caster->GetAttributes());
        auto* Progression = Caster->GetProgression();
        Progression->BindAttributes(Caster->GetAttributes());
        if (!TestTrue(TEXT("permanent Caster class"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal"));
        Progression->SettleDoctrineEntitlement(Flags);
        TestEqual(TEXT("shipped final entitlement is eight points"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
        const auto* Tree = UBreakerProgressionLibrary::GetCasterMultispellTree();
        FText Reason;
        if (!Progression->IsAbilityUnlocked(TEXT("Caster.Fracture")))
        {
            const int32 Tokens = Progression->GetUnspentAbilityTokens();
            if (!TestTrue(TEXT("actual level-earned token unlocks Fracture"), Progression->SpendAbilityToken(TEXT("Caster.Fracture"), Reason))) return false;
            TestEqual(TEXT("Fracture acquisition spends one earned token"), Progression->GetUnspentAbilityTokens(), Tokens - 1);
        }
        if (!TestTrue(TEXT("Unmake is an unlocked class ultimate"), Progression->IsAbilityUnlocked(TEXT("Caster.Unmake")))) return false;
        for (const TCHAR* Node : { TEXT("Caster.Multispell.Reservoir"), TEXT("Caster.Multispell.Reservoir"),
            TEXT("Caster.Multispell.Variance"), TEXT("Caster.Multispell.Variance"),
            TEXT("Caster.Multispell.Chain"), TEXT("Caster.Multispell.Sequence") })
            if (!TestTrue(Node, Progression->PurchaseNode(Tree, Node, Reason))) return false;
        if (!TestTrue(TEXT("actual branch commitment"), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
        if (!TestTrue(TEXT("actual Cascade purchase"), Progression->PurchaseNode(Tree, TEXT("Caster.Multispell.Cascade"), Reason))) return false;
        TestEqual(TEXT("Cascade path spends exactly eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
        if (!TestTrue(TEXT("purchased keystone reaches GAS"), ASC->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_Cascade.GetTag()))) return false;
        auto* Mana = Caster->GetMana();
        Mana->BindAttributes(Caster->GetAttributes());
        Mana->SetComponentTickEnabled(false);
        Mana->AdvanceLoop(30); // Normal passive recovery before the encounter only.
        const float StartingMana = Mana->GetMana();
        if (!TestTrue(TEXT("normal Mana plus legal Overcast funds two Fractures and Unmake"),
            StartingMana - Caster->GetAttributes()->GetClassResourceFloor() >= 140)) return false;
        auto* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Caster);
        Cycle->BeginPlay();
        const auto Fracture = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
        const auto Unmake = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Unmake::StaticClass(), 1));
        auto CastProjectile = [&]() -> ABreakerProjectileBase*
        {
            TSet<ABreakerProjectileBase*> Existing;
            for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) Existing.Add(*It);
            const float Before = Mana->GetMana();
            if (!TestTrue(TEXT("real Fracture activation"), ASC->TryActivateAbility(Fracture))) return nullptr;
            BreakerResolvePendingCast(World, Caster);
            TestTrue(TEXT("Fracture pays before the ultimate"), Mana->GetMana() < Before);
            if (!BreakerWaitForFractureCast(World, ASC, Fracture)) return nullptr;
            for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It)
                if (!Existing.Contains(*It)) return *It;
            return nullptr;
        };
        auto* Miss = CastProjectile();
        if (!TestNotNull(TEXT("first real projectile"), Miss)) return false;
        Miss->Impact(nullptr, Origin + FVector(300, 0, 0));
        auto* Paid = CastProjectile();
        if (!TestNotNull(TEXT("second paid projectile"), Paid)) return false;
        if (!TestEqual(TEXT("two actual casts reach Entropy"), Cycle->PeekNextEntry().Element, EBreakerElement::Entropy)) return false;

        auto SpawnTarget = [&](const FVector& Location) -> ABreakerEnemy*
        {
            auto* Enemy = World->SpawnActor<ABreakerEnemy>(Location, FRotator::ZeroRotator, Spawn);
            if (!Enemy) return nullptr;
            Enemy->SetActorTickEnabled(false);
            auto* Health = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
            auto* Combat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
            auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>();
            if (!Health || !Combat || !Status) return nullptr;
            auto* EnemyASC = Enemy->GetAbilitySystemComponent();
            EnemyASC->InitAbilityActorInfo(Enemy, Enemy);
            EnemyASC->AddAttributeSetSubobject(Health);
            // A durable observation target isolates echoes from lethal damage;
            // player offense, costs, node values and status payloads stay shipped.
            Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000);
            Combat->BindAttributes(Health);
            Status->SetComponentTickEnabled(false);
            return Enemy;
        };
        ABreakerEnemy* Target = Scenario == 0 ? SpawnTarget(Origin + FVector(600, 0, 0)) : nullptr;
        auto* Neighbor = SpawnTarget(Origin + FVector(600, 150, 0));
        if (!TestNotNull(TEXT("nearby potential Chain recipient"), Neighbor)) return false;
        const float BeforeUltimate = Mana->GetMana();
        if (!TestTrue(TEXT("actual paid Unmake activates"), ASC->TryActivateAbility(Unmake))) return false;
        BreakerResolvePendingCast(World, Caster);
        TestTrue(TEXT("Unmake consumes its real resource cost"), Mana->GetMana() < BeforeUltimate);
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Caster);
        if (!TestTrue(TEXT("actual ultimate window opens"), State->IsWindowActive(UBreakerCasterAbility::UnmakeWindowKey()))) return false;
        if (!Target) Target = SpawnTarget(Origin + FVector(600, 0, 0));
        if (!TestNotNull(TEXT("native status target exists"), Target)) return false;
        auto* Status = Target->FindComponentByClass<UBreakerStatusComponent>();
        Paid->Impact(Target, Target->GetActorLocation());
        if (!TestTrue(TEXT("paid impact applies positive-proc Poison"), Status->HasStatus(Poison))) return false;
        const auto* Original = Status->GetActiveStatuses().FindByPredicate([Poison](const FBreakerActiveStatus& Active)
        { return Active.Spec.StatusTag == Poison; });
        if (!TestTrue(TEXT("paid original application can trigger Cascade"), Original && Original->Spec.ProcCoefficient > 0)) return false;
        TestFalse(TEXT("echo is deferred beyond original application"), Status->HasStatus(Bleed));
        TestEqual(TEXT("Cascade skips element positions and draws Bleed"), Cycle->PeekNext(), Poison);

        if (Scenario == 2) ASC->CancelAbilityHandle(Unmake);
        if (Scenario == 3)
        {
            FBreakerDamageRequest Lethal;
            Lethal.BaseDamage = 100000; Lethal.bCanCritical = false;
            Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Caster->GetCombat()->ReceiveDamage(Lethal);
            TestTrue(TEXT("caster really dies before queued echo"), Caster->GetCombat()->IsDead());
        }
        if (Scenario == 4)
        {
            TestTrue(TEXT("actual Doctrine respec removes keystone"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Reason));
            TestFalse(TEXT("Cascade tag actually removed"), ASC->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_Cascade.GetTag()));
        }
        const double BeforeTick = World->GetTimeSeconds();
        Advance(); Advance();
        TestTrue(TEXT("real world and timer frames advance"), World->GetTimeSeconds() >= BeforeTick + .09);
        if (Scenario < 2)
        {
            if (!TestTrue(TEXT("physical echo lands on initial or late-spawned target"), Status->HasStatus(Bleed))) return false;
            const auto* Echo = Status->GetActiveStatuses().FindByPredicate([Bleed](const FBreakerActiveStatus& Active)
            { return Active.Spec.StatusTag == Bleed; });
            if (!Echo) return false;
            TestEqual(TEXT("echo proc coefficient is zero"), Echo->Spec.ProcCoefficient, 0.0f);
            TestEqual(TEXT("echo retains caster ownership"), Echo->Instigator.Get(), static_cast<AActor*>(Caster));
            TestEqual(TEXT("echo cannot recursively advance cycle"), Cycle->PeekNext(), Poison);
        }
        else TestFalse(TEXT("cancel, death or keystone removal cancels queued echo"), Status->HasStatus(Bleed));
        TestEqual(TEXT("zero-proc second status cannot Chain to neighbor"), Neighbor->FindComponentByClass<UBreakerStatusComponent>()->GetDistinctStatusTypeCount(), 0);
        ASC->CancelAbilityHandle(Unmake);
        Target->Destroy(); Neighbor->Destroy(); Caster->Destroy();
    }
    return true;
}
#endif
