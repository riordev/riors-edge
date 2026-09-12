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
    struct FBreakerMetronomeAfterimageFixture
    {
        UWorld* World = nullptr;
        ABreakerCharacter* Player = nullptr;
        AActor* Target = nullptr;
        UBreakerProgressionTree* CoreTree = nullptr;
        uint64 SavedFrame = GFrameCounter;
        FBreakerMetronomeAfterimageFixture(EBreakerClassId Class, bool bOwnAfterimage)
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
            auto* Tree=NewObject<UBreakerProgressionTree>(Definition); Tree->TreeId=TEXT("Test.Afterimage.Metronome"); Tree->Currency=EBreakerPointCurrency::CorePoints;
            Definition->BranchTrees.Add(Tree); Progression->ClassDefinition=Definition; CoreTree=Tree;
            auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Afterimage.Metronome.Rule"); Node->Currency=Tree->Currency;
            Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))); Tree->Nodes.Add(Node);
            Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(40,Progression->ExperienceCurve));
            FText Reason; if (bOwnAfterimage) Progression->PurchaseNode(Tree,Node->NodeId,Reason);
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
        ~FBreakerMetronomeAfterimageFixture() { if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } GFrameCounter=SavedFrame; }
        void AimTarget() { FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim); Target->SetActorLocation(Eye+Aim.Vector()*800); }
        void Tick(float Seconds)
        {
            // Registered state components have automatic ticking disabled;
            // the save-isolated fixture drives each native clock exactly once.
            for (int32 I = 0; I < FMath::CeilToInt(Seconds / .01f); ++I)
            {
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, .01f);
                for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
                    if (auto* State = It->FindComponentByClass<UBreakerAbilityStateComponent>()) State->AdvanceTime(.01f);
                Player->FindComponentByClass<UBreakerChargeComponent>()->AdvanceLoop(.01f);
            }
        }
        ABreakerCharacter* AddAlly(bool bOwnAfterimage)
        {
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Ally = World->SpawnActor<ABreakerCharacter>(FVector(0, 200, 0), FRotator::ZeroRotator, Spawn);
            Ally->bRefuseSavesForPendingCharacter = true; Ally->SetActorTickEnabled(false);
            Ally->GetBreakerMovement()->SetComponentTickEnabled(false); Ally->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
            auto* ASC = Ally->GetAbilitySystemComponent(); auto* Attr = Ally->GetAttributes();
            ASC->InitAbilityActorInfo(Ally, Ally); ASC->AddAttributeSetSubobject(Attr); Ally->GetCombat()->BindAttributes(Attr);
            auto* P = Ally->GetProgression(); P->BindAttributes(Attr); P->ChoosePermanentClassById(EBreakerClassId::Swift);
            if (bOwnAfterimage)
            {
                auto* Definition = DuplicateObject<UBreakerClassDefinition>(P->ClassDefinition, Ally);
                auto* Tree = DuplicateObject<UBreakerProgressionTree>(CoreTree, Definition);
                Definition->BranchTrees.Add(Tree); P->ClassDefinition = Definition;
                P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, P->ExperienceCurve));
                FText Reason; P->PurchaseNode(Tree, TEXT("Test.Afterimage.Metronome.Rule"), Reason);
            }
            UBreakerAbilityStateComponent::FindOrAdd(Ally)->SetComponentTickEnabled(false);
            auto* Weapon = Ally->GetWeapon(); Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
            Weapon->WeaponDefinition->HipSpreadDegrees = Weapon->WeaponDefinition->AimSpreadDegrees = 0;
            Weapon->WeaponDefinition->BleedChance = 0; Weapon->ResetAmmunition();
            return Ally;
        }
        float Shot(ABreakerCharacter* Shooter = nullptr)
        {
            if (!Shooter) Shooter = Player;
            FVector Eye; FRotator Aim; Shooter->GetActorEyesViewPoint(Eye, Aim); Target->SetActorLocation(Eye + Aim.Vector()*800);
            auto* W=Shooter->GetWeapon();
            const float CriticalMultiplier=Shooter->GetAttributes()->GetCriticalMultiplier();
            W->StartFire(); W->StopFire();
            const auto& Result=W->GetLastShot().DamageResult;
            // These real hits retain their ordinary critical rolls. Compare
            // the flat lane on the same pre-critical basis for each result;
            // progression recomposition must not change what is measured.
            return Result.HealthDamage / (Result.bCritical ? CriticalMultiplier : 1.0f);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerMetronomeAfterimageTest, "RiorsEdge.Abilities.AfterimageMetronomeDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerMetronomeAfterimageTest::RunTest(const FString&)
{
    for (int32 Scenario = 0; Scenario < 10; ++Scenario)
    {
        FBreakerMetronomeAfterimageFixture F(EBreakerClassId::Support, Scenario != 7);
        if (!F.Player) return false;
        auto* Player = F.Player; auto* P = Player->GetProgression(); auto* ASC = Player->GetAbilitySystemComponent();
        auto* Abilities = Player->GetAbilities(); auto* Charge = Player->FindComponentByClass<UBreakerChargeComponent>();
        auto* Ally = F.AddAlly(Scenario == 7);
        if (!TestNotNull(TEXT("Actual foreign recipient"), Ally)) return false;
        FText Reason; FBreakerQuestFlagSet Flags;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions()) for (const auto& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        P->SettleDoctrineEntitlement(Flags);
        auto* Tree = UBreakerProgressionLibrary::GetSupportConductorTree();
        if (!TestTrue(TEXT("Actual Conductor commitment"), P->CommitToBranch(Tree->TreeId, Reason))) return false;
        if (!TestTrue(TEXT("Paid Discipline creates actual distinct self deadline"), P->PurchaseNode(Tree, TEXT("Support.Conductor.DownbeatDiscipline"), Reason))) return false;
        const int32 TokensBefore = P->GetUnspentAbilityTokens();
        if (!TestTrue(TEXT("Earned level token unlocks Metronome"), P->SpendAbilityToken(TEXT("Support.Metronome"), Reason))) return false;
        TestEqual(TEXT("Unlock spends exactly one token"), P->GetUnspentAbilityTokens(), TokensBefore - 1);
        const auto Slot = EBreakerAbilitySlot::ClassAbilityOne;
        if (!TestTrue(TEXT("Equip purchased Metronome"), P->EquipAbility(Slot, TEXT("Support.Metronome"), Reason))) return false;
        Abilities->RefreshGrants();
        if (Scenario == 2)
        {
            const auto Cost = BreakerCoreRespecCost(P->GetCharacterLevel()); auto* Equipment = Player->GetEquipment();
            for (int32 Seed = 1; Seed <= 100 && !Equipment->GetForgeWallet().CanAfford(Cost); ++Seed)
            {
                const auto Item = UBreakerLootLibrary::RollItem(TEXT("Test.Metronome.Salvage"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Standard, 40, Seed);
                if (!Equipment->AddToBackpack(Item) || !Equipment->SalvageFromBackpack(Item.ItemId)) return false;
            }
            if (!TestTrue(TEXT("Ordinary salvage funds real Core respec"), Equipment->GetForgeWallet().CanAfford(Cost))) return false;
        }
        // Native effective-heal credit plus the actual global generation cap;
        // this fixture does not claim to replay healing-ability acquisition.
        auto* TargetCombat = F.Target->FindComponentByClass<UBreakerCombatComponent>();
        for (int32 Pulse = 0; Pulse < 60 && Charge->GetCharge() < 100; ++Pulse)
        {
            FBreakerDamageRequest Damage; Damage.BaseDamage = 20000; Damage.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Damage.bCanCritical = false; TargetCombat->ReceiveDamage(Damage);
            const auto Heal = TargetCombat->ApplyHealingAmount(20000, Player, FGameplayTag());
            Charge->NotifyHealingDone(Heal.HealthHealed, 0, 100000, false, 1); F.Tick(1);
        }
        if (!TestEqual(TEXT("Effective healing earns full native Charge"), Charge->GetCharge(), 100.0f, .001f)) return false;
        const float SourceBase = F.Shot(); const float AllyBase = F.Shot(Ally); F.Tick(.5f);
        const float Before = Charge->GetCharge(); const float Quote = Abilities->GetCost(Slot);
        if (!TestTrue(TEXT("Equipped purchased Metronome activates"), Abilities->TryActivateSlot(Slot))) return false;
        TestEqual(TEXT("Actual quote is paid once"), Before - Charge->GetCharge(), Quote, .001f);
        auto* Spec = ASC->FindAbilitySpecFromClass(UBreakerAbility_Metronome::StaticClass()); if (!Spec) return false;
        const auto Handle = Spec->Handle; const double Start = F.World->GetTimeSeconds();
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Player); auto* AllyState = UBreakerAbilityStateComponent::FindOrAdd(Ally);
        const float AllyDuration = AllyState->GetWindowRemaining(UBreakerAbility_Metronome::WindowKey());
        const float SourceDuration = State->GetWindowRemaining(UBreakerAbility_Metronome::WindowKey());
        TestEqual(TEXT("Purchased self extension is Downbeat Discipline's four seconds"), SourceDuration - AllyDuration, 4.0f, .001f);
        auto AdvanceTo = [&](double Offset) { F.Tick(FMath::Max(0.0f, static_cast<float>(Start + Offset - F.World->GetTimeSeconds()))); };
        const float PerStack = GetDefault<UBreakerAbility_Metronome>()->FlatDamagePerStack;
        const float AllyFlat = PerStack * Ally->GetAttributes()->GetDamageMultiplier();
        AdvanceTo(AllyDuration - .7f);
        TestEqual(TEXT("First real recipient shot starts ramp"), F.Shot(Ally), AllyBase, .01f); F.Tick(.2f);
        TestEqual(TEXT("Second real shot carries one stack"), F.Shot(Ally), AllyBase + AllyFlat, .01f); F.Tick(.2f);
        TestEqual(TEXT("Third real shot carries two stacks"), F.Shot(Ally), AllyBase + AllyFlat * 2, .01f);
        if (Scenario == 8)
        {
            AllyState->CloseWindow(UBreakerAbility_Metronome::WindowKey());
            F.Tick(.3f);
            TestEqual(TEXT("Explicit early window removal gives no tail"), F.Shot(Ally), AllyBase, .01f);
            continue;
        }
        AdvanceTo(AllyDuration + .05f);
        TestFalse(TEXT("Recipient permission ends at its own deadline"), AllyState->IsWindowActive(UBreakerAbility_Metronome::WindowKey()));
        TestTrue(TEXT("Paid self extension remains active"), State->IsWindowActive(UBreakerAbility_Metronome::WindowKey()));
        TestEqual(TEXT("Expired recipient no longer counts as maintained"), State->GetMaintainedBuffRecipientCount(), 1);
        if (Scenario == 7)
        {
            TestTrue(TEXT("Recipient owns Afterimage independently"), Ally->GetProgression()->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))));
            TestEqual(TEXT("Recipient ownership cannot grant source's missing tail"), F.Shot(Ally), AllyBase, .01f);
            continue;
        }
        TestFalse(TEXT("Recipient needs no Afterimage purchase"), Ally->GetProgression()->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Afterimage"))));
        TestEqual(TEXT("Source purchase grants foreign half stack contribution"), F.Shot(Ally), AllyBase + AllyFlat * 1.5f, .01f);
        F.Tick(.3f); TestEqual(TEXT("Tail shots cannot grow expired stacks"), F.Shot(Ally), AllyBase + AllyFlat * 1.5f, .01f);
        if (Scenario == 9)
        {
            F.Tick(.3f); Player->Destroy();
            TestEqual(TEXT("Destroyed source revokes foreign numerical lease"), F.Shot(Ally), AllyBase, .01f);
            continue;
        }
        auto Kill = [&](ABreakerCharacter* Victim)
        {
            FBreakerDamageRequest Damage; Damage.BaseDamage = 100000; Damage.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Damage.bCanCritical = false; Damage.SetInstigator(F.Target); Victim->GetCombat()->ReceiveDamage(Damage);
            TestTrue(TEXT("Actual hostile damage kills"), Victim->GetCombat()->IsDead()); Victim->GetCombat()->RestoreVitals();
        };
        if (Scenario == 1) ASC->CancelAbilityHandle(Handle);
        else if (Scenario == 2)
        {
            if (!P->RespecCore(Reason)) return false;
            F.Tick(.3f); TestEqual(TEXT("Source respec removes foreign tail"), F.Shot(Ally), AllyBase, .01f);
            if (!P->PurchaseNode(F.CoreTree, TEXT("Test.Afterimage.Metronome.Rule"), Reason)) return false;
        }
        else if (Scenario == 3) Kill(Player);
        else if (Scenario == 4) Kill(Ally);
        else if (Scenario == 5) ASC->ClearAbility(Handle);
        else if (Scenario == 6)
        {
            F.Tick(Abilities->GetCooldownRemaining(Slot) + .02f);
            TestTrue(TEXT("Actual cooldown expires while foreign tail remains"), F.World->GetTimeSeconds() < Start + AllyDuration + 2);
            const float RecastBefore = Charge->GetCharge(); const float RecastQuote = Abilities->GetCost(Slot);
            if (!TestTrue(TEXT("Real funded recast replaces old holder lease"), Abilities->TryActivateSlot(Slot))) return false;
            TestEqual(TEXT("Recast pays actual quote"), RecastBefore - Charge->GetCharge(), RecastQuote, .001f);
        }
        if (Scenario != 0)
        {
            F.Tick(.3f); TestEqual(TEXT("Cleanup or fresh cast cannot retain old foreign tail"), F.Shot(Ally), AllyBase, .01f);
            continue;
        }
        const float SourceFlat = PerStack * Player->GetAttributes()->GetDamageMultiplier();
        AdvanceTo(SourceDuration - .7f);
        TestEqual(TEXT("Caster ramp remains independent"), F.Shot(), SourceBase, .01f); F.Tick(.2f);
        TestEqual(TEXT("Caster second shot carries one stack"), F.Shot(), SourceBase + SourceFlat, .01f); F.Tick(.2f);
        TestEqual(TEXT("Caster third shot carries two stacks"), F.Shot(), SourceBase + SourceFlat * 2, .01f);
        AdvanceTo(SourceDuration + .05f);
        TestFalse(TEXT("Final holder ends actual ability"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        TestEqual(TEXT("No maintained recipients during pure numerical tail"), State->GetMaintainedBuffRecipientCount(), 0);
        TestEqual(TEXT("Caster receives own independently timed half tail"), F.Shot(), SourceBase + SourceFlat * 1.5f, .01f);
        TestEqual(TEXT("Earlier foreign tail has expired"), F.Shot(Ally), AllyBase, .01f);
        F.Tick(2.05f); TestEqual(TEXT("Caster tail expires"), F.Shot(), SourceBase, .01f);
    }
    return true;
}
#endif
