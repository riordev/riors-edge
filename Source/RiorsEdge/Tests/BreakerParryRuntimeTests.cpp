#include "Progression/BreakerExperience.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/BreakerInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Settings/BreakerGameSettings.h"
#include "Net/UnrealNetwork.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerParryRuntimeTest, "RiorsEdge.Combat.ParryPurchasedRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerParryRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated parry world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("Actual player"), Player)) return false;
    Player->GetCharacterMovement()->SetComponentTickEnabled(false);
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    UBreakerAttributeSet* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    // Set fixture chassis before the aggregator captures its base values.
    Attributes->ApplyMaxHealth(10000); Attributes->ApplyHealth(10000);
    UBreakerProgressionComponent* Progression = Player->GetProgression();
    UBreakerCombatComponent* Combat = Player->GetCombat();
    Progression->BindAttributes(Attributes);
    // Only this component begins; the character never begins or reads saves.
    Combat->BeginPlay();
    TestFalse(TEXT("Unowned parry refuses"), Combat->TryParry());
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(7, Progression->ExperienceCurve)); // Earned seven-point Parry and Counterweight route.
    FText Failure;
    const UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    for (const TCHAR* Node : { TEXT("Core.Bulwark.Read"), TEXT("Core.Bulwark.Guard"), TEXT("Core.Bulwark.Parry"),
        TEXT("Core.Bulwark.Evade"), TEXT("Core.Bulwark.Counterweight") })
        if (!TestTrue(*FString::Printf(TEXT("Actual node purchase %s"), Node), Progression->PurchaseNode(Core, FName(Node), Failure))) return false;
    Attributes->ApplyShield(0); Attributes->ApplyMaxShield(0);
    Combat->DodgeChance = 0;
    Combat->BlockChance = 0;
    Attributes->ApplyClassResource(0);
    const float BaselineWeapon = Attributes->GetDamageMultiplier();
    const float BaselineAbility = Attributes->GetAbilityDamageMultiplier();
    TestFalse(TEXT("Counter opportunity is absent before success"), Combat->IsParryCounterActive());
    AActor* Attacker = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("External hit source"), Attacker)) return false;
    FBreakerDamageRequest Hit;
    Hit.Instigator = Attacker; Hit.BaseDamage = 10; Hit.bBypassShield = true;
    Hit.SourceLocation = Player->GetActorLocation() + FVector(100, 0, 0); Hit.bHasSourceLocation = true;
    auto Advance = [&](float Seconds)
    {
        const float Start = World->GetTimeSeconds();
        for (float Elapsed = 0; Elapsed + KINDA_SMALL_NUMBER < Seconds;)
        {
            const float Step = FMath::Min(0.05f, Seconds - Elapsed);
            ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Elapsed += Step;
        }
        TestTrue(TEXT("Actual world clock advanced"), World->GetTimeSeconds() >= Start + Seconds - 0.001f);
        Progression->RefreshBuildConditions();
    };
    TestTrue(TEXT("Purchased parry activates"), Combat->TryParry());
    TestEqual(TEXT("Counterweight widens the authored base window"), Combat->GetParryWindowRemaining(), 0.35f, 0.001f);
    TestEqual(TEXT("Counterweight reduces actual cooldown by half a second"), Combat->GetParryCooldownRemaining(), 1.5f, .001f);
    TestFalse(TEXT("Repress cannot extend window or reset cooldown"), Combat->TryParry());
    const float WindowBefore = Combat->GetParryWindowRemaining();
    FBreakerDamageRequest Ineligible = Hit; Ineligible.bIsDamageOverTime = true;
    TestFalse(TEXT("DoT cannot parry"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible = Hit; Ineligible.SourceLocation = Player->GetActorLocation() - FVector(100, 0, 0);
    TestFalse(TEXT("Rear hit cannot parry"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible = Hit; Ineligible.bHasSourceLocation = false;
    TestFalse(TEXT("Unlocated hit cannot parry"), Combat->ReceiveDamage(Ineligible).bParried);
    Ineligible = Hit; Ineligible.Instigator = Player;
    const FBreakerDamageResult Self = Combat->ReceiveDamage(Ineligible);
    TestFalse(TEXT("Mandatory self-damage cannot parry"), Self.bParried);
    TestTrue(TEXT("Self-damage still lands"), Self.HealthDamage > 0);
    Ineligible = Hit; Ineligible.BaseDamage = 0;
    TestFalse(TEXT("Zero hit cannot earn a counter"), Combat->ReceiveDamage(Ineligible).bParried);
    TestEqual(TEXT("Ineligible hits leave window intact"), Combat->GetParryWindowRemaining(), WindowBefore, 0.001f);
    TestFalse(TEXT("Ineligible hits earn no counter"), Combat->IsParryCounterActive());
    Advance(0.30f); // Outside base .25, inside the actually purchased Counterweight extension.
    const float HealthBefore = Attributes->GetHealth();
    const float ResourceBefore = Attributes->GetClassResource();
    Combat->DodgeChance = 1.0f; // If Parry incorrectly routes through dodge, the guaranteed refund exposes it.
    const FBreakerDamageResult Parried = Combat->ReceiveDamage(Hit);
    Combat->DodgeChance = 0.0f;
    TestTrue(TEXT("Counterweight-extended frontal hit is actually parried"), Parried.bParried);
    TestEqual(TEXT("Successful defense resets the real recovery combat clock"), Player->GetSecondsSinceCombat(), 0.0f, 0.001f);
    TestFalse(TEXT("Parry is not a dodge"), Parried.bDodged);
    TestFalse(TEXT("Parry is not a block"), Parried.bBlocked);
    TestEqual(TEXT("Parry loses no health"), Attributes->GetHealth(), HealthBefore);
    TestEqual(TEXT("Parry earns no dodge resource refund"), Attributes->GetClassResource(), ResourceBefore);
    TestFalse(TEXT("Success consumes the defense window"), Combat->IsParryActive());
    TestTrue(TEXT("Success starts counter"), Combat->IsParryCounterActive());
    TestEqual(TEXT("Counterweight changes clocks without adding weapon power"), Attributes->GetDamageMultiplier(), BaselineWeapon, 0.0001f);
    TestEqual(TEXT("Counter adds no ability power"), Attributes->GetAbilityDamageMultiplier(), BaselineAbility, 0.0001f);
    FBreakerDamageRequest Followup = Hit; Followup.bCanBeAvoided = false; // Evade now authors passive dodge; isolate consumed Parry.
    const FBreakerDamageResult Second = Combat->ReceiveDamage(Followup);
    TestFalse(TEXT("Second hit is not parried"), Second.bParried);
    TestTrue(TEXT("Second hit deals actual damage"), Second.HealthDamage > 0);
    // Actual outgoing damage uses the just-published source pool, not a mirrored formula.
    UBreakerCombatComponent* TargetCombat = NewObject<UBreakerCombatComponent>(Attacker);
    Attacker->AddInstanceComponent(TargetCombat); TargetCombat->RegisterComponent();
    UBreakerAttributeSet* TargetAttributes = NewObject<UBreakerAttributeSet>(Attacker);
    TargetAttributes->ApplyMaxHealth(10000); TargetAttributes->ApplyHealth(10000); TargetCombat->BindAttributes(TargetAttributes);
    auto Shot = [&]()
    {
        FBreakerDamageRequest Request; Request.Instigator = Player; Request.BaseDamage = 100;
        Request.bCanCritical = false;
        UBreakerDamageLibrary::FillSourcePools(Attributes, EBreakerDamageDelivery::Weapon, Request);
        return TargetCombat->ReceiveDamage(Request).HealthDamage;
    };
    const float CounterDamage = Shot();
    Advance(2.05f);
    TestFalse(TEXT("Counter expires"), Combat->IsParryCounterActive());
    TestEqual(TEXT("Counter expiry leaves ordinary weapon power unchanged"), Attributes->GetDamageMultiplier(), BaselineWeapon, 0.0001f);
    TestEqual(TEXT("Counterweight no longer authors legacy conditional damage"), CounterDamage - Shot(), 0.0f, 0.001f);
    TestTrue(TEXT("Cooldown recovers"), Combat->TryParry());
    Advance(0.36f);
    TestFalse(TEXT("Whiffed window expires"), Combat->IsParryActive());
    TestFalse(TEXT("Whiff never starts counter"), Combat->IsParryCounterActive());
    TestFalse(TEXT("Whiff still pays cooldown"), Combat->TryParry());
    Advance(1.7f);
    TestTrue(TEXT("Parry before lethal hit"), Combat->TryParry());
    Ineligible = Hit; Ineligible.BaseDamage = 1000000; Ineligible.bHasSourceLocation = false;
    Combat->ReceiveDamage(Ineligible);
    TestTrue(TEXT("Lethal hit kills"), Combat->IsDead());
    TestFalse(TEXT("Dead owner cannot parry"), Combat->TryParry());
    Combat->RestoreVitals();
    TestFalse(TEXT("Revive cannot restore old window"), Combat->IsParryActive());
    TestTrue(TEXT("Fresh parry after revive"), Combat->TryParry());
    Combat->ReceiveDamage(Hit);
    if (!TestTrue(TEXT("Actual Core respec"), Progression->RespecCore(Failure))) return false;
    TestFalse(TEXT("Respec cancels permission"), Combat->TryParry());
    TestFalse(TEXT("Respec cancels counter"), Combat->IsParryCounterActive());
    Combat->GetClass()->SetUpRuntimeReplicationData();
    TArray<FLifetimeProperty> Replicated; Combat->GetLifetimeReplicatedProps(Replicated);
    for (const TCHAR* Name : { TEXT("bParryOwned"), TEXT("ParryWindowEnd"), TEXT("ParryCooldownEnd"), TEXT("ParryCounterEnd") })
    {
        const FProperty* Property = FindFProperty<FProperty>(Combat->GetClass(), Name);
        if (!TestNotNull(Name, Property)) return false;
        TestTrue(*FString::Printf(TEXT("Owner-only replicated %s"), Name), Replicated.ContainsByPredicate(
            [Property](const FLifetimeProperty& Entry) { return Entry.RepIndex == Property->RepIndex && Entry.Condition == COND_OwnerOnly; }));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerParryMappingTest, "RiorsEdge.Settings.ParryShippedMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerParryMappingTest::RunTest(const FString& Parameters)
{
    const UBreakerInputConfig* Config = LoadObject<UBreakerInputConfig>(nullptr, TEXT("/Game/ProjectBreaker/Input/DA_PlayerInputConfig.DA_PlayerInputConfig"));
    if (!TestNotNull(TEXT("Shipped input config"), Config) || !TestNotNull(TEXT("Code-supplied action on old asset"), Config->Parry.Get())) return false;
    const auto Defaults = UBreakerGameSettingsLibrary::ProjectDefaultKeybinds();
    const TArray<FKey>* Keys = Defaults.Find(TEXT("Parry"));
    if (!TestTrue(TEXT("V is a visible project default"), Keys && Keys->Contains(EKeys::V))) return false;
    TestTrue(TEXT("Parry appears in editable settings"), UBreakerGameSettingsLibrary::BindableActionNames().Contains(TEXT("Parry")));
    auto Check = [&](const TMap<FName, FKey>& Overrides, FKey Expected)
    {
        UInputMappingContext* Context = UBreakerGameSettingsLibrary::BuildRuntimeMappingContext(Config, Overrides, GetTransientPackage());
        if (!TestNotNull(TEXT("Runtime context includes fallback"), Context)) return;
        int32 ParryRows = 0;
        for (const FEnhancedActionKeyMapping& Row : Context->GetMappings())
            if (Row.Action == Config->Parry) { ++ParryRows; TestEqual(TEXT("Actual parry mapping key"), Row.Key, Expected); }
        TestEqual(TEXT("Exactly one Parry binding"), ParryRows, 1);
    };
    Check({}, EKeys::V);
    TMap<FName, FKey> Rebound; Rebound.Add(TEXT("Parry"), EKeys::B);
    Check(Rebound, EKeys::B);
    Check({}, EKeys::V);
    TestFalse(TEXT("Shipped mapping asset was not mutated"), Config->DefaultMappingContext->GetMappings().ContainsByPredicate(
        [Config](const FEnhancedActionKeyMapping& Row) { return Row.Action == Config->Parry; }));
    return true;
}
#endif
