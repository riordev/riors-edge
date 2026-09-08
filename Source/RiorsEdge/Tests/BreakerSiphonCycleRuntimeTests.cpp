#include "Tests/BreakerFractureTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerElementSharesMath.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSiphonCycleRuntimeTest,
    "RiorsEdge.Abilities.SiphonUnlockPaidCycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSiphonCycleRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto MakeCaster = [&]()
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        if (!Player) return Player;
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        auto* Progression = Player->GetProgression(); Progression->BindAttributes(Player->GetAttributes());
        Progression->ChoosePermanentClassById(EBreakerClassId::Caster);
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, Progression->ExperienceCurve));
        return Player;
    };
    auto* Player = MakeCaster();
    if (!TestNotNull(TEXT("native Caster without save-loading BeginPlay"), Player)) return false;
    auto* Progression = Player->GetProgression();
    auto* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Player);
    if (!TestNotNull(TEXT("actual cycle component"), Cycle)) return false;
    TestEqual(TEXT("starter cycle contains three earned delivery types"), Cycle->GetCycleLength(), 3);
    FText Reason;
    const int32 BeforeTokens = Progression->GetUnspentAbilityTokens();
    if (!TestTrue(TEXT("level-earned token buys Siphon"), Progression->SpendAbilityToken(TEXT("Caster.Siphon"), Reason))) return false;
    TestEqual(TEXT("unlock debits one token"), Progression->GetUnspentAbilityTokens(), BeforeTokens - 1);
    TestEqual(TEXT("actual unlock appends fourth position immediately"), Cycle->GetCycleLength(), 4);
    TestEqual(TEXT("appended position is Void"), Cycle->PeekNextEntry(3).Element, EBreakerElement::Void);
    TestEqual(TEXT("appended preview is Erased"), Cycle->PeekNext(3), FGameplayTag::RequestGameplayTag(TEXT("Status.Erased")));
    UBreakerStatusCycleComponent::FindOrAdd(Player); UBreakerStatusCycleComponent::FindOrAdd(Player);
    TestEqual(TEXT("repeated synchronization cannot duplicate unlock"), Cycle->GetCycleLength(), 4);
    TestEqual(TEXT("synchronization preserves starter cursor"), Cycle->GetCursor(), 0);
    auto* Late = MakeCaster();
    if (!Late) return false;
    if (!TestTrue(TEXT("unlock succeeds before component creation"), Late->GetProgression()->SpendAbilityToken(TEXT("Caster.Siphon"), Reason))) return false;
    TestEqual(TEXT("late component reconstructs unlocked fourth position"), UBreakerStatusCycleComponent::FindOrAdd(Late)->GetCycleLength(), 4);

    // Actual restored benchmark entitlement, not a campaign acquisition claim.
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("benchmark entitlement is eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    const auto* Tree = UBreakerProgressionLibrary::GetCasterMultispellTree();
    for (const TCHAR* Node : { TEXT("Caster.Multispell.Variance"), TEXT("Caster.Multispell.Variance"),
        TEXT("Caster.Multispell.Cycle"), TEXT("Caster.Multispell.Cycle"), TEXT("Caster.Multispell.Chain"),
        TEXT("Caster.Multispell.Chain"), TEXT("Caster.Multispell.Fracture") })
    {
        const bool bBought = Progression->PurchaseNode(Tree, Node, Reason);
        if (!TestTrue(FString::Printf(TEXT("purchase %s: %s"), Node, *Reason.ToString()), bBought)) return false;
    }
    TestEqual(TEXT("MS7 path spends exactly eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    if (!Progression->IsAbilityUnlocked(TEXT("Caster.Fracture")))
        if (!TestTrue(TEXT("earned token unlocks Fracture"), Progression->SpendAbilityToken(TEXT("Caster.Fracture"), Reason))) return false;
    auto* Mana = Player->GetMana(); Mana->BindAttributes(Player->GetAttributes()); Mana->SetComponentTickEnabled(false); Mana->AdvanceLoop(30);
    const auto Handle = Player->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Fracture::StaticClass(), 1));
    auto* Target = World->SpawnActor<AActor>();
    if (!Target) return false;
    auto* Combat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target); Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->SetComponentTickEnabled(false);
    for (int32 Cast = 0; Cast < 2; ++Cast)
    {
        if (Cast == 1) TestEqual(TEXT("first two-position impact reaches Entropy without cursor override"), Cycle->PeekNextEntry().Element, EBreakerElement::Entropy);
        TSet<ABreakerProjectileBase*> Existing;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) Existing.Add(*It);
        const float Before = Mana->GetMana();
        if (!TestTrue(TEXT("actual paid Fracture cast"), Player->GetAbilitySystemComponent()->TryActivateAbility(Handle))) return false;
        TestTrue(TEXT("each projectile consumes normal Mana"), Mana->GetMana() < Before);
        if (!BreakerWaitForFractureCast(World, Player->GetAbilitySystemComponent(), Handle)) return false;
        ABreakerProjectileBase* Projectile = nullptr;
        for (TActorIterator<ABreakerProjectileBase> It(World); It; ++It) if (!Existing.Contains(*It)) { Projectile = *It; break; }
        if (!TestNotNull(TEXT("paid cast spawned projectile"), Projectile)) return false;
        if (Cast == 1)
        {
            const auto Shares = BreakerElementShares::Resolve(Projectile->GetProjectileDamage());
            if (!TestEqual(TEXT("MS7 carries two earned elemental shares"), Shares.Num(), 2)) return false;
            TestEqual(TEXT("first share is actual Entropy cursor"), Shares[0].Element, EBreakerElement::Entropy);
            TestEqual(TEXT("second share is unlocked Void"), Shares[1].Element, EBreakerElement::Void);
        }
        // Actual server impact seam; this fixture does not claim natural flight.
        Projectile->Impact(Target, Target->GetActorLocation());
    }
    TestTrue(TEXT("paid dual-element impact reaches Entropy consumer"), Status->GetEntropyBuildup() > 0);
    TestTrue(TEXT("paid dual-element impact reaches Void consumer"), Status->GetVoidBuildup() > 0);
    return true;
}
#endif
