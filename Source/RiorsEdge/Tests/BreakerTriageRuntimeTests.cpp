#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerTriageRuntimeTest, "RiorsEdge.Abilities.TriageRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerTriageRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated real Triage world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto MakePlayer = [&](FVector Location)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ABreakerCharacter>(Location, FRotator::ZeroRotator, Spawn);
        if (!Player) return Player;
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        TestTrue(TEXT("actual permanent Support class"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Support));
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* Charge = Player->FindComponentByClass<UBreakerChargeComponent>();
        Charge->SetComponentTickEnabled(false); Charge->BindAttributes(Player->GetAttributes());
        return Player;
    };
    auto PurchaseTriage = [&](ABreakerCharacter* Player)
    {
        // Restored authored campaign fixture proves spending/wiring, not mission acquisition.
        auto* Progression = Player->GetProgression();
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, FBreakerExperienceCurve()));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat : Mission.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        TestEqual(TEXT("real authored eight-point entitlement"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
        const auto* Tree = UBreakerProgressionLibrary::GetSupportMedicTree(); FText Reason;
        // The keystone walk (O272): Steady Hands is Triage's travel; two whole
        // pairs and a third travel open the six-invested gate. Sustained
        // Care, No Triage and Triage Priority are read only by Patch and
        // Purge, which no Triage caster in this fixture casts; Field Dressing
        // and Clean Hands are the travels it always held.
        for (const TCHAR* Node : { TEXT("Support.Medic.SteadyHands"),
            TEXT("Support.Medic.FieldDressing"), TEXT("Support.Medic.SustainedCare"),
            TEXT("Support.Medic.CleanHands"), TEXT("Support.Medic.NoTriage"),
            TEXT("Support.Medic.TriagePriority") })
            if (!TestTrue(Node, Progression->PurchaseNode(Tree, Node, Reason))) return false;
        TestEqual(TEXT("six invested opens the keystone gate"), Progression->GetTreeInvestment(Tree), 6);
        if (!TestTrue(TEXT("actual Medic commitment"), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
        if (!TestTrue(TEXT("actual Triage keystone purchase"), Progression->PurchaseNode(Tree, TEXT("Support.Medic.Triage"), Reason))) return false;
        TestEqual(TEXT("seven nodes leave one point of the eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), UBreakerProgressionLibrary::DoctrinePointGrant - 7);
        Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Player->GetAttributes());
        return true;
    };
    auto Advance = [&](int32 Steps)
    {
        for (int32 I = 0; I < Steps; ++I)
        {
            TArray<UBreakerAbilityStateComponent*> States;
            for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                if (auto* State = It->FindComponentByClass<UBreakerAbilityStateComponent>())
                { State->SetComponentTickEnabled(false); States.Add(State); }
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            for (auto* State : States) State->AdvanceTime(.05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
    };
    auto CastTriage = [&](ABreakerCharacter* Player)
    {
        Player->GetAttributes()->ApplyClassResource(100);
        auto* ASC = Player->GetAbilitySystemComponent();
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Conduit::StaticClass(), 1));
        TestTrue(TEXT("actual purchased Triage Conduit activates"), ASC->TryActivateAbility(Handle));
        return Handle;
    };
    auto Damage = [&](ABreakerCharacter* Player, float Amount, bool bDot = false)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = Amount; Hit.bCanCritical = false;
        Hit.bIsDamageOverTime = bDot;
        Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
        return Player->GetCombat()->ReceiveDamage(Hit);
    };
    auto* Source = MakePlayer(FVector(0, 0, 100));
    auto* Ally = MakePlayer(FVector(500, 0, 100));
    auto* Outside = MakePlayer(FVector(2000, 0, 100));
    if (!Source || !Ally || !Outside || !PurchaseTriage(Source)) return false;
    Ally->GetAttributes()->ApplyHealth(Ally->GetAttributes()->GetMaxHealth() * .5f);
    Outside->GetAttributes()->ApplyHealth(Outside->GetAttributes()->GetMaxHealth() * .5f);
    Advance(1);
    const auto FirstCast = CastTriage(Source);
    const float BeforeHeal = Ally->GetAttributes()->GetHealth();
    const float OutsideBefore = Outside->GetAttributes()->GetHealth();
    Advance(22);
    TestEqual(TEXT("actual 15m field heals living ally four percent per second"), Ally->GetAttributes()->GetHealth() - BeforeHeal,
        Ally->GetAttributes()->GetMaxHealth() * .04f, .01f);
    TestEqual(TEXT("outside ally gets no field healing"), Outside->GetAttributes()->GetHealth(), OutsideBefore);
    Ally->GetAttributes()->ApplyHealth(10); Ally->GetAttributes()->ApplyMaxShield(100); Ally->GetAttributes()->ApplyShield(50);
    const auto ShieldHit = Damage(Ally, 20);
    TestFalse(TEXT("shield-only damage is nonlethal"), ShieldHit.bKilled);
    TestEqual(TEXT("shield absorbs actual damage before save"), Ally->GetAttributes()->GetHealth(), 10.0f);
    Ally->GetCombat()->DodgeChance = 1;
    TestTrue(TEXT("actual dodge precedes lethal save"), Damage(Ally, 10000).bDodged);
    Ally->GetCombat()->DodgeChance = 0; Ally->GetAttributes()->ApplyShield(0);
    const auto Saved = Damage(Ally, 10000);
    TestFalse(TEXT("first real lethal hit is saved without fake death"), Saved.bKilled);
    TestFalse(TEXT("saved recipient stays alive"), Ally->GetCombat()->IsDead());
    TestTrue(TEXT("save leaves at most one HP"), Ally->GetAttributes()->GetHealth() > 0 && Ally->GetAttributes()->GetHealth() <= 1);
    Ally->SetActorLocation(FVector(3000, 0, 100)); Advance(2);
    Ally->SetActorLocation(FVector(500, 0, 100)); Advance(2);
    TestTrue(TEXT("exit and reentry cannot refresh consumed save"), Damage(Ally, 10000).bKilled);
    Ally->GetCombat()->RestoreVitals(); Advance(2);
    TestTrue(TEXT("revival within same cast cannot refresh consumed save"), Damage(Ally, 10000).bKilled);
    TestTrue(TEXT("outside player is not saved by distant source"), Damage(Outside, 10000).bKilled);

    auto* Overlap = MakePlayer(FVector(500, 200, 100));
    auto* Second = MakePlayer(FVector(0, 200, 100));
    if (!Overlap || !Second || !PurchaseTriage(Second)) return false;
    Ally->GetCombat()->RestoreVitals();
    const auto SecondCast = CastTriage(Second);
    TestFalse(TEXT("genuinely new cast grants revived target a new save"), Damage(Ally, 10000).bKilled);
    TestTrue(TEXT("new cast save is also single-use"), Damage(Ally, 10000).bKilled);
    Advance(22);
    TestFalse(TEXT("first overlapping lease saves a lethal hit"), Damage(Overlap, 10000).bKilled);
    TestFalse(TEXT("second independent lease saves actual lethal DoT"), Damage(Overlap, 10000, true).bKilled);
    TestTrue(TEXT("two leases cannot save third lethal hit"), Damage(Overlap, 10000).bKilled);
    Source->GetAbilitySystemComponent()->CancelAbilityHandle(FirstCast);
    auto* Canceled = MakePlayer(FVector(0, -1000, 100));
    if (!Canceled) return false;
    Advance(22);
    Second->GetAbilitySystemComponent()->CancelAbilityHandle(SecondCast);
    TestTrue(TEXT("cancel removes unspent actual field protection"), Damage(Canceled, 10000).bKilled);
    auto* Third = MakePlayer(FVector(10000, 0, 100));
    auto* Leaving = MakePlayer(FVector(10500, 0, 100));
    auto* Orphan = MakePlayer(FVector(10500, 200, 100));
    if (!Third || !Leaving || !Orphan || !PurchaseTriage(Third)) return false;
    const auto ThirdCast = CastTriage(Third); Advance(22);
    Leaving->SetActorLocation(FVector(13000, 0, 100));
    TestTrue(TEXT("hit-time radius rejects stale lease before next field poll"), Damage(Leaving, 10000).bKilled);
    TestFalse(TEXT("Triage also grants source its first lethal save"), Damage(Third, 10000).bKilled);
    TestTrue(TEXT("second actual lethal hit kills field source"), Damage(Third, 10000).bKilled);
    TestFalse(TEXT("source death ends active Conduit"), Third->GetAbilitySystemComponent()->FindAbilitySpecFromHandle(ThirdCast)->IsActive());
    TestTrue(TEXT("dead source cannot save another recipient"), Damage(Orphan, 10000).bKilled);
    auto* Expiring = MakePlayer(FVector(20000, 0, 100));
    auto* ExpiredRecipient = MakePlayer(FVector(20500, 0, 100));
    if (!Expiring || !ExpiredRecipient || !PurchaseTriage(Expiring)) return false;
    const auto ExpiringCast = CastTriage(Expiring); Advance(500);
    TestFalse(TEXT("actual Conduit duration expires"), Expiring->GetAbilitySystemComponent()->FindAbilitySpecFromHandle(ExpiringCast)->IsActive());
    TestTrue(TEXT("expired lease cannot save lethal damage"), Damage(ExpiredRecipient, 10000).bKilled);
    auto* Healer = MakePlayer(FVector(30000, 0, 100));
    if (!Healer) return false;
    auto* Progression = Healer->GetProgression();
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50, FBreakerExperienceCurve()));
    FBreakerQuestFlagSet Completed;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Completed.Add(Flag);
    Completed.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Completed);
    const auto* Medic = UBreakerProgressionLibrary::GetSupportMedicTree(); FText Failure;
    // Single-rank travel roots (O272): one buy each.
    for (const TCHAR* Node : { TEXT("Support.Medic.FieldDressing"), TEXT("Support.Medic.Attending") })
        if (!TestTrue(Node, Progression->PurchaseNode(Medic, Node, Failure))) return false;
    auto* Charge = Healer->FindComponentByClass<UBreakerChargeComponent>();
    Charge->BindAttributes(Healer->GetAttributes()); Charge->SetInCombat(true);
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(30500, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("actual Attending marked enemy"), Enemy)) return false;
    auto* EnemyAttributes = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
    if (!TestNotNull(TEXT("actual enemy attribute subobject"), EnemyAttributes)) return false;
    Enemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(EnemyAttributes);
    Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
    EnemyAttributes->ApplyMaxHealth(10000); EnemyAttributes->ApplyHealth(10000);
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Controller) return false;
    Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
    Controller->Possess(Healer); Controller->SetViewTarget(Healer);
    Controller->SetInitialLocationAndRotation(Healer->GetActorLocation(), FRotator::ZeroRotator);
    for (int32 I = 0; I < 3; ++I)
    {
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0);
        FVector Origin; FRotator Facing; Controller->GetPlayerViewPoint(Origin, Facing);
        Controller->SetControlRotation((Enemy->GetActorLocation() - Origin).Rotation());
    }
    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0);
    auto* ASC = Healer->GetAbilitySystemComponent();
    const auto Patch = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Patch::StaticClass(), 1));
    auto Heal = [&]
    {
        FGameplayTagContainer Cooldown; Cooldown.AddTag(GetDefault<UBreakerAbility_Patch>()->GetAbilityDefinition()->CooldownTag);
        ASC->RemoveActiveEffectsWithGrantedTags(Cooldown);
        Healer->GetAttributes()->ApplyHealth(Healer->GetAttributes()->GetMaxHealth() - 20);
        Healer->GetAttributes()->ApplyClassResource(GetDefault<UBreakerAbility_Patch>()->GetAbilityDefinition()->ResourceCost + 1);
        Charge->BindAttributes(Healer->GetAttributes()); Charge->SetInCombat(true);
        TestTrue(TEXT("actual Patch restores health for Attending"), ASC->TryActivateAbility(Patch));
        Charge->AdvanceLoop(1);
        return Healer->GetAttributes()->GetClassResource() - 1;
    };
    // The real self-heal token bucket starts empty in this isolated fixture.
    // Fill one second before the control heal, matching the budget available
    // to later heals after their normal AdvanceLoop calls.
    Charge->AdvanceLoop(1);
    const float OrdinaryHealCharge = Heal();
    Healer->GetAttributes()->ApplyClassResource(100);
    const auto Mark = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Mark::StaticClass(), 1));
    TestTrue(TEXT("actual locally aimed Mark for Attending"), ASC->TryActivateAbility(Mark));
    auto* MarkState = Healer->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!TestTrue(TEXT("real Mark selected intended enemy"), MarkState && MarkState->GetMarkedTarget() == Enemy)) return false;
    Advance(40);
    TestEqual(TEXT("Attending pays actual healed amount at marked health-fraction rate"), Heal() - OrdinaryHealCharge, .1f, .001f);
    Advance(80); Heal();
    TestTrue(TEXT("Attending refreshes live mark to ten seconds"), MarkState->GetMarkRemaining() >= 9.99f);
    Advance(100);
    TestTrue(TEXT("actual Mark survives original expiry after real heal"), ASC->FindAbilitySpecFromHandle(Mark)->IsActive());
    FBreakerDamageRequest Probe; Probe.BaseDamage = 100; Probe.bCanCritical = false; Probe.DamageFamily = EBreakerDamageFamily::TrueDamage;
    auto* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    const float AuthoredMarkMultiplier = GetDefault<UBreakerAbility_Mark>()->MarkedDamageMultiplier;
    TestEqual(TEXT("shipped Mark vulnerability is 1.15"), AuthoredMarkMultiplier, 1.15f);
    TestEqual(TEXT("refreshed timer preserves actual authored vulnerability"), EnemyCombat->ReceiveDamage(Probe).HealthDamage,
        Probe.BaseDamage * AuthoredMarkMultiplier, .01f);
    Advance(120);
    TestFalse(TEXT("refreshed actual timer eventually ends"), ASC->FindAbilitySpecFromHandle(Mark)->IsActive());
    TestEqual(TEXT("expired refreshed timer removes actual vulnerability"), EnemyCombat->ReceiveDamage(Probe).HealthDamage, 100.0f, .01f);
    auto* ProcHealer = MakePlayer(FVector(40000, 0, 100));
    auto* ProcRecipient = MakePlayer(FVector(40500, 0, 100));
    if (!ProcHealer || !ProcRecipient) return false;
    // Bind the same native handler Character BeginPlay normally installs,
    // without invoking its unrelated owner-save loading in this fixture.
    FScriptDelegate HealingHandler;
    HealingHandler.BindUFunction(ProcHealer, TEXT("HandleClassResourceHealingDealt"));
    ProcHealer->GetCombat()->OnHealingDealt.Add(HealingHandler);
    auto* ProcCharge = ProcHealer->FindComponentByClass<UBreakerChargeComponent>();
    auto HealAtProc = [&](float Proc)
    {
        ProcRecipient->GetAttributes()->ApplyHealth(ProcRecipient->GetAttributes()->GetMaxHealth() - 20);
        ProcHealer->GetAttributes()->ApplyClassResource(0);
        ProcCharge->BindAttributes(ProcHealer->GetAttributes()); ProcCharge->SetInCombat(true);
        FBreakerHealRequest Request; Request.Amount = 20; Request.ProcCoefficient = Proc; Request.SetHealer(ProcHealer);
        const auto Result = ProcRecipient->GetCombat()->ApplyHealing(Request);
        TestEqual(TEXT("proc weighting preserves actual restored health"), Result.HealthHealed, 20.0f);
        ProcCharge->AdvanceLoop(1);
        return ProcHealer->GetAttributes()->GetClassResource();
    };
    const float FullProcCharge = HealAtProc(1);
    TestTrue(TEXT("actual allied heal routes through native Charge handler"), FullProcCharge > 0);
    TestEqual(TEXT("quarter-proc allied heal generates quarter Charge"), HealAtProc(.25f), FullProcCharge * .25f, .001f);
    TestEqual(TEXT("zero-proc allied heal restores health but grants no Charge"), HealAtProc(0), 0.0f);
    return true;
}
#endif
