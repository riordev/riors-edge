#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "EngineUtils.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FBreakerDownbeatAfterimageFixture
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        AActor* Target = nullptr;
        UBreakerProgressionTree* CoreTree = nullptr;
        uint64 SavedFrame = GFrameCounter;
        FBreakerDownbeatAfterimageFixture(EBreakerClassId Class)
        {
            UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            if (!World) return;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
            Player=World->SpawnActor<ABreakerCharacter>(); Player->bRefuseSavesForPendingCharacter=true; Player->SetActorTickEnabled(false);
            Player->GetBreakerMovement()->SetComponentTickEnabled(false); Player->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
            auto* ASC=Player->GetAbilitySystemComponent(); auto* Attr=Player->GetAttributes();
            ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr); Player->GetCombat()->BindAttributes(Attr);
            auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr); Progression->ChoosePermanentClassById(Class);
            auto* Definition=DuplicateObject<UBreakerClassDefinition>(Progression->ClassDefinition,Player);
            auto* Tree=NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId=TEXT("Test.Afterimage.Downbeat"); Tree->Currency=EBreakerPointCurrency::CorePoints;
            Definition->BranchTrees.Add(Tree); Progression->ClassDefinition=Definition; CoreTree=Tree;
            auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Afterimage.Downbeat.Rule"); Node->Currency=Tree->Currency;
            Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))); Tree->Nodes.Add(Node);
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(40,Progression->ExperienceCurve));
            FText Reason; Progression->PurchaseNode(Tree,Node->NodeId,Reason);
            Player->FindComponentByClass<UBreakerChargeComponent>()->BindAttributes(Attr);
            Player->FindComponentByClass<UBreakerChargeComponent>()->SetInCombat(true);
            Attr->ApplyClassResource(0); Player->FindComponentByClass<UBreakerChargeComponent>()->SetComponentTickEnabled(false);
            Player->GetMomentum()->BindAttributes(Attr); Player->GetMomentum()->SetComponentTickEnabled(false);
            UBreakerAbilityStateComponent::FindOrAdd(Player)->SetComponentTickEnabled(false);
            auto* Weapon=Player->GetWeapon(); Weapon->WeaponDefinition=DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(),Weapon);
            Weapon->WeaponDefinition->HipSpreadDegrees=Weapon->WeaponDefinition->AimSpreadDegrees=0; Weapon->WeaponDefinition->BleedChance=0;
            Weapon->ResetAmmunition();
            Target=World->SpawnActor<AActor>(); auto* Body=NewObject<USphereComponent>(Target); Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
            Body->SetSphereRadius(60); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
            Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block); Body->RegisterComponent(); AimTarget();
            auto* Sink=NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Sink); Sink->RegisterComponent();
            auto* Health=NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); Sink->BindAttributes(Health);
        }
        ~FBreakerDownbeatAfterimageFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } GFrameCounter=SavedFrame; }
        void AimTarget() { FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim); Target->SetActorLocation(Eye+Aim.Vector()*800); }
        void Tick(float Seconds)
        {
            auto* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
            // The save-isolated fixture omits actor BeginPlay. Its registered
            // state component has automatic ticking disabled and advances once.
            for (int32 I = 0; I < FMath::CeilToInt(Seconds / .01f); ++I)
            {
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, .01f);
                State->AdvanceTime(.01f);
                Player->FindComponentByClass<UBreakerChargeComponent>()->AdvanceLoop(.01f);
            }
        }
        float Shot()
        {
            auto* W=Player->GetWeapon();
            const float CriticalMultiplier=Player->GetAttributes()->GetCriticalMultiplier();
            W->StartFire(); W->StopFire();
            const auto& Result=W->GetLastShot().DamageResult;
            // These real hits retain their ordinary critical rolls. Compare
            // the flat lane on the same pre-critical basis for each result;
            // progression recomposition must not change what is measured.
            return Result.HealthDamage / (Result.bCritical ? CriticalMultiplier : 1.0f);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDownbeatAfterimageTest, "RiorsEdge.Abilities.AfterimageDownbeatDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDownbeatAfterimageTest::RunTest(const FString&)
{
    for (int32 Scenario = 0; Scenario < 6; ++Scenario)
    {
        FBreakerDownbeatAfterimageFixture F(EBreakerClassId::Support);
        if (!F.Player) return false;
        auto* Player = F.Player; auto* Progression = Player->GetProgression();
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Charge = Player->FindComponentByClass<UBreakerChargeComponent>();
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
        auto* Abilities = Player->GetAbilities();
        FBreakerQuestFlagSet Flags;
        // Restored campaign entitlement supplies eight real Doctrine points.
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions()) for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Progression->SettleDoctrineEntitlement(Flags);
        FText Reason;
        auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
        if (!TestTrue(TEXT("Real Conductor commitment"), Progression->CommitToBranch(Tree->TreeId, Reason))) return false;
        for (const TCHAR* Id : { TEXT("Support.Conductor.DownbeatDiscipline"), TEXT("Support.Conductor.DownbeatDiscipline"),
            TEXT("Support.Conductor.Rehearsal"), TEXT("Support.Conductor.Rehearsal"),
            TEXT("Support.Conductor.Sustain"), TEXT("Support.Conductor.Sustain"), TEXT("Support.Conductor.Downbeat") })
            if (!TestTrue(FString::Printf(TEXT("Paid %s: %s"), Id, *Reason.ToString()), Progression->PurchaseNode(Tree, Id, Reason))) return false;
        if (!TestTrue(TEXT("Afterimage purchased with earned point"), Progression->HasNodeTag(
            FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))))) return false;
        // Class selection registered Conduit as the base ultimate; Cadence is
        // an unlockable and must spend the actual earned level token first.
        TestTrue(TEXT("Actual Support kit unlocks Conduit"), Progression->IsAbilityUnlocked(TEXT("Support.Conduit")));
        const int32 TokensBefore = Progression->GetUnspentAbilityTokens();
        if (!TestTrue(TEXT("Earned token purchases Cadence"), Progression->SpendAbilityToken(TEXT("Support.Cadence"), Reason))) return false;
        TestEqual(TEXT("Cadence spends exactly one token"), Progression->GetUnspentAbilityTokens(), TokensBefore - 1);
        if (!TestTrue(TEXT("Equip actual Conduit ultimate"), Progression->EquipAbility(EBreakerAbilitySlot::Ultimate, TEXT("Support.Conduit"), Reason))) return false;
        if (!TestTrue(TEXT("Equip purchased Cadence"), Progression->EquipAbility(EBreakerAbilitySlot::ClassAbilityOne, TEXT("Support.Cadence"), Reason))) return false;
        Abilities->RefreshGrants();
        if (Scenario == 2)
        {
            const auto Cost = BreakerCoreRespecCost(Progression->GetCharacterLevel());
            auto* Equipment = Player->GetEquipment();
            for (int32 Seed = 1; Seed <= 100 && !Equipment->GetForgeWallet().CanAfford(Cost); ++Seed)
            {
                const auto Item = UBreakerLootLibrary::RollItem(TEXT("Test.Downbeat.Salvage"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Standard, 40, Seed);
                if (!Equipment->AddToBackpack(Item) || !Equipment->SalvageFromBackpack(Item.ItemId)) return false;
            }
            if (!TestTrue(TEXT("Salvage funds actual respec"), Equipment->GetForgeWallet().CanAfford(Cost))) return false;
        }
        // Credit actual effective healing through the same native Charge seam
        // used by Support heals. No wallet grant or resource attribute refill.
        // This isolates ultimate delivery, not healing-ability acquisition.
        auto* TargetCombat = F.Target->FindComponentByClass<UBreakerCombatComponent>();
        for (int32 Pulse = 0; Pulse < 60 && Charge->GetCharge() < 100; ++Pulse)
        {
            FBreakerDamageRequest Damage; Damage.BaseDamage = 20000; Damage.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Damage.bCanCritical = false; TargetCombat->ReceiveDamage(Damage);
            const auto Heal = TargetCombat->ApplyHealingAmount(20000, Player, FGameplayTag());
            Charge->NotifyHealingDone(Heal.HealthHealed, 0, 100000, false, 1);
            F.Tick(1);
        }
        if (!TestEqual(TEXT("Effective healing earns native capped Charge"), Charge->GetCharge(), 100.0f, .001f)) return false;
        F.Tick(.5f); const float Baseline = F.Shot(); F.Tick(.5f);
        const float SourceMultiplier = Player->GetAttributes()->GetDamageMultiplier();
        if (!TestTrue(TEXT("Equipped paid Downbeat ultimate activates"), Abilities->TryActivateSlot(EBreakerAbilitySlot::Ultimate))) return false;
        auto* ConduitSpec = ASC->FindAbilitySpecFromClass(UBreakerAbility_Conduit::StaticClass());
        if (!TestNotNull(TEXT("Registered Conduit slot grants real spec"), ConduitSpec)) return false;
        const auto Handle = ConduitSpec->Handle;
        TestEqual(TEXT("Ultimate spends full earned bar"), Charge->GetCharge(), 0.0f, .001f);
        const float Duration = State->GetWindowRemaining(UBreakerSupportAbility::ConduitWindowKey());
        TestTrue(TEXT("Actual finite Conduit window"), Duration > 2);
        F.Tick(Duration - 1);
        // A real self-first Cadence supplies one unique holder through expiry.
        if (!TestTrue(TEXT("Conduit permits equipped purchased Cadence"), Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne))) return false;
        auto* CadenceSpec = ASC->FindAbilitySpecFromClass(UBreakerAbility_Cadence::StaticClass());
        if (!TestNotNull(TEXT("Registered Cadence slot grants real spec"), CadenceSpec)) return false;
        const auto Cadence = CadenceSpec->Handle;
        F.Tick(.05f);
        TestEqual(TEXT("Self is one unique maintained recipient"), State->GetMaintainedBuffRecipientCount(), 1);
        const float Flat = GetDefault<UBreakerAbility_Conduit>()->DownbeatFlatDamagePerBuffedTarget * SourceMultiplier;
        TestEqual(TEXT("Full weapon-only flat on actual rifle"), F.Shot(), Baseline + Flat, .01f);
        TestEqual(TEXT("Nested Cadence retains existing doubled contribution"), Player->GetWeapon()->GetReloadSpeedMultiplier(), 1.5f, .001f);
        if (Scenario == 1)
        {
            ASC->CancelAbilityHandle(Handle); F.Tick(.5f);
            TestEqual(TEXT("Explicit cancellation leaves no flat tail"), F.Shot(), Baseline, .01f);
            continue;
        }
        if (Scenario == 4)
        {
            FBreakerDamageRequest Kill; Kill.BaseDamage = 100000; Kill.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Kill.bCanCritical = false; Kill.SetInstigator(F.Target); Player->GetCombat()->ReceiveDamage(Kill);
            TestTrue(TEXT("Full window actually dies"), Player->GetCombat()->IsDead());
            Player->GetCombat()->RestoreVitals(); F.Tick(.5f);
            TestEqual(TEXT("Death during full window cannot leave tail"), F.Shot(), Baseline, .01f);
            continue;
        }
        F.Tick(1.02f);
        TestFalse(TEXT("Conduit permission ends normally"), State->IsWindowActive(UBreakerSupportAbility::ConduitWindowKey()));
        TestFalse(TEXT("Ultimate GAS ends before numerical tail"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        TestEqual(TEXT("Ordinary Cadence contribution resumes independently"), Player->GetWeapon()->GetReloadSpeedMultiplier(), 1.25f, .001f);
        TestEqual(TEXT("Actual rifle receives half Downbeat flat"), F.Shot(), Baseline + Flat * .5f, .01f);
        FBreakerDamageRequest Ability; Ability.BaseDamage = 10; Ability.Delivery = EBreakerDamageDelivery::Ability;
        Player->GetCombat()->ApplyOutgoingModifiers(Ability);
        TestEqual(TEXT("Weapon-only tail never enters ability damage"), Ability.BaseDamage, 10.0f);
        ASC->CancelAbilityHandle(Cadence);
        TestEqual(TEXT("Membership ends without erasing frozen source contribution"), State->GetMaintainedBuffRecipientCount(), 0);
        if (Scenario == 5)
        {
            ASC->ClearAbility(Handle); F.Tick(.5f);
            TestEqual(TEXT("Removing inactive ability revokes its tail"), F.Shot(), Baseline, .01f);
            continue;
        }
        if (Scenario == 0)
        {
            F.Tick(.5f);
            TestEqual(TEXT("Lost membership cannot mutate expired lease"), F.Shot(), Baseline + Flat * .5f, .01f);
        }
        if (Scenario == 2)
        {
            if (!Progression->RespecCore(Reason)) return false;
            F.Tick(.5f);
            TestEqual(TEXT("Respec revokes tail and composed point floor"), F.Shot(),
                Baseline * Player->GetAttributes()->GetDamageMultiplier() / SourceMultiplier, .01f);
            if (!Progression->PurchaseNode(F.CoreTree, TEXT("Test.Afterimage.Downbeat.Rule"), Reason)) return false;
            F.Tick(.5f); TestEqual(TEXT("Rebuy cannot revive old tail"), F.Shot(), Baseline, .01f);
        }
        else if (Scenario == 3)
        {
            FBreakerDamageRequest Kill; Kill.BaseDamage = 100000; Kill.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Kill.bCanCritical = false; Kill.SetInstigator(F.Target); Player->GetCombat()->ReceiveDamage(Kill);
            TestTrue(TEXT("Tail owner really dies"), Player->GetCombat()->IsDead());
            Player->GetCombat()->RestoreVitals(); F.Tick(.5f);
            TestEqual(TEXT("Revival cannot retain old flat tail"), F.Shot(), Baseline, .01f);
        }
        F.Tick(2.05f); TestEqual(TEXT("Natural tail expires"), F.Shot(), Baseline, .01f);
    }
    return true;
}
#endif
