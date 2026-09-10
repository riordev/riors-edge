#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "Save/BreakerQuestContent.h"
#include "Interaction/BreakerNPC.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/BreakerFeedstockPickup.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerQuestJournal.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFernhallEncounterRuntimeTest,
    "RiorsEdge.Zone.Fernhall.OutdoorEncounterRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallEncounterRuntimeTest::RunTest(const FString& Parameters)
{
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    // Two independent map builds prove that finite enemies return on a visit,
    // without timers, new save state, or changes to the existing quest rules.
    for (int32 Visit = 0; Visit < 2; ++Visit)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/FernhallOutdoor_%s/Lvl_Fernhall"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Package->SetFlags(RF_Transient);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Lvl_Fernhall")), Package,
            true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("Isolated outdoor world"), World)) return false;
        FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
        Context.SetCurrentWorld(World);
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
        UBreakerGameInstance* Session = NewObject<UBreakerGameInstance>();
        World->SetGameInstance(Session);
        Context.OwningGameInstance = Session;
        World->GetWorldSettings()->DefaultGameMode = ABreakerGameMode::StaticClass();
        if (!TestTrue(TEXT("Authority mode installed"), World->SetGameMode(FURL()))) return false;
        World->InitializeActorsForPlay(FURL());
        ABreakerGameMode* Mode = World->GetAuthGameMode<ABreakerGameMode>();
        if (!TestNotNull(TEXT("Game mode"), Mode)) return false;
        Mode->DispatchBeginPlay();
        ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Controller"), Controller)) return false;
        Controller->Possess(Player);
        // No Character BeginPlay: that routine owns save loading/persistence.
        // Bind the same reflected kill handler it binds, so actual combat
        // deaths reach real quest logic without duplicating its conditions.
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        if (!TestNotNull(TEXT("Actual character quest kill handler exists"), Player->FindFunction(FName(TEXT("HandleQuestKill"))))) return false;
        FScriptDelegate QuestKill;
        QuestKill.BindUFunction(Player, FName(TEXT("HandleQuestKill")));
        Player->GetCombat()->OnKillDealt.Add(QuestKill);
        UBreakerQuestJournal* Journal = Player->GetQuestJournal();
        if (!TestNotNull(TEXT("Character constructs its quest journal before BeginPlay"), Journal)) return false;
        Journal->SetFlag(Visit == 0 ? FName(TEXT("Quest.FirstContract.Accepted")) : FName(TEXT("Quest.KessSalvage.Accepted")));
        Mode->HandleStartingNewPlayer_Implementation(Controller);
        TestFalse(TEXT("Ordinary Fernhall stays outside a rift"), Mode->IsRiftInstance());
        TestFalse(TEXT("Outdoor patrols do not activate the wave controller"), Mode->IsWaveActive());
        TArray<ABreakerEnemy*> Enemies;
        int32 CourtyardCount = 0, CourtyardMelee = 0, CourtyardLattices = 0;
        TArray<FVector> PocketCenters;
        PocketCenters.Init(FVector::ZeroVector, 5);
        int32 PocketCounts[] = { 0, 0, 0, 0, 0 };
        int32 Elites = 0;
        int32 SubstationMelee = 0, SubstationWardens = 0, SubstationSkirmishers = 0, EntryLattices = 0;
        ABreakerEnemy* SubstationSkirmisher = nullptr;
        ABreakerEnemy* SubstationWarden = nullptr;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Enemy = *It;
            const bool bCourtyard = Enemy->Tags.Contains(TEXT("Fernhall.Outdoor.Courtyard"));
            if (!bCourtyard) Enemies.Add(Enemy);
            TestFalse(TEXT("Outdoor enemy is finite"), Enemy->DoesRespawn());
            TestFalse(TEXT("Outdoor enemy cannot complete a rift"), Enemy->IsRiftTerminator());
            Elites += Enemy->GetMonsterRank() != EBreakerMonsterRank::Trash ? 1 : 0;
            int32 Pocket = INDEX_NONE;
            for (int32 Index = 0; Index < 5; ++Index)
                if (Enemy->Tags.Contains(FName(*FString::Printf(TEXT("Fernhall.Outdoor.%d"), Index)))) Pocket = Index;
            if (!TestTrue(TEXT("Every body belongs to an authored pocket or courtyard"), Pocket != INDEX_NONE || bCourtyard)) return false;
            if (bCourtyard)
            {
                ++CourtyardCount;
                CourtyardMelee += Enemy->GetClass() == ABreakerEnemy::StaticClass();
                CourtyardLattices += Enemy->GetClass() == ABreakerRangedEnemy::StaticClass();
                TestEqual(TEXT("Courtyard remains entry level"), Enemy->GetAreaLevel(), 5);
                TestEqual(TEXT("Courtyard roster remains ordinary"), Enemy->GetMonsterRank(), EBreakerMonsterRank::Trash);
                TestEqual(TEXT("Courtyard roster is Vestige"), Enemy->GetFamily(), EBreakerEnemyFamily::Vestige);
            }
            else
            {
                PocketCenters[Pocket] += Enemy->GetActorLocation();
                ++PocketCounts[Pocket];
            }
            if (Pocket == 1 && Enemy->IsA<ABreakerRangedEnemy>()) ++EntryLattices;
            if (Pocket == 2)
            {
                if (Enemy->GetClass() == ABreakerEnemy::StaticClass()) ++SubstationMelee;
                if (Enemy->IsA<ABreakerWardenEnemy>()) { ++SubstationWardens; SubstationWarden = Enemy; }
                if (Enemy->IsA<ABreakerSkirmisherEnemy>()) { ++SubstationSkirmishers; SubstationSkirmisher = Enemy; }
            }
            const UCapsuleComponent* Capsule = Enemy->FindComponentByClass<UCapsuleComponent>();
            if (!TestNotNull(TEXT("Enemy capsule"), Capsule)) return false;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FernhallOutdoorTest), false, Enemy);
            FHitResult Floor;
            if (TestTrue(TEXT("Each enemy stands on actual imported floor"), World->LineTraceSingleByObjectType(Floor,
                Enemy->GetActorLocation(), Enemy->GetActorLocation() - FVector(0, 0, 1000),
                FCollisionObjectQueryParams(ECC_WorldStatic), Query)))
                TestEqual(TEXT("Feet are grounded after rank scaling"),
                    Enemy->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight() - Floor.ImpactPoint.Z, 2.0, 1.0);
            TestFalse(TEXT("Enemy capsule is clear of world props and other bodies"), World->OverlapBlockingTestByChannel(
                Enemy->GetActorLocation(), FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query));
            TestTrue(TEXT("Arrival is outside every patrol's detection range"),
                FVector::Dist2D(Player->GetActorLocation(), Enemy->GetActorLocation()) > Enemy->GetDetectionRange());
        }
        AddInfo(FString::Printf(TEXT("FERNHALL POCKETS  total %d, per pocket %d/%d/%d/%d/%d, elites %d"),
            Enemies.Num(), PocketCounts[0], PocketCounts[1], PocketCounts[2], PocketCounts[3], PocketCounts[4], Elites));
        // 11 -> 17. Before this the persistent world held fifteen bodies and a
        // player crossed two 106 m yards meeting three fights; the far half of
        // each yard was walked and never contested. The added six are HALF of
        // wave one of the same solved budget — plain Skitters, no Lattice, no
        // Skirmisher, no Warden, no elite — so the two fights that carry a rank
        // are still pockets 1 and 2 and the new ground is the space between.
        //
        // Half rather than all twelve because the XP economy noticed: placing
        // the whole wave took a cleared entry yard from 159 XP to 375, past
        // the 279 that reaches level two, which would have made the first
        // contract a reward for something the player had already outgrown.
        if (!TestEqual(TEXT("Seventeen outdoor enemies populate each fresh visit"), Enemies.Num(), 17)) return false;
        TestEqual(TEXT("Four additional courtyard enemies"), CourtyardCount, 4);
        TestEqual(TEXT("Three ordinary courtyard melee"), CourtyardMelee, 3);
        TestEqual(TEXT("One courtyard Lattice"), CourtyardLattices, 1);
        TestEqual(TEXT("Roster retains one elite for the initial contract"), Elites, 1);
        const int32 ExpectedCounts[] = { 4, 3, 4, 3, 3 };
        for (int32 Index = 0; Index < 5; ++Index)
        {
            if (!TestEqual(TEXT("Distinct pocket roster"), PocketCounts[Index], ExpectedCounts[Index])) return false;
            PocketCenters[Index] /= PocketCounts[Index];
        }
        TestTrue(TEXT("Entry fights occupy separate combat spaces"), FVector::Dist2D(PocketCenters[0], PocketCenters[1]) > 2500);
        TestTrue(TEXT("Substation encounter occupies its own yard"), FVector::Dist2D(PocketCenters[1], PocketCenters[2]) > 4000);
        // The two added pockets are their own ground, not a second body of an
        // existing fight. 1800 cm is a formation's own width rather than a
        // round number: the grid is three wide on the pocket's spacing, so
        // closer than that and two pockets interleave into one fight.
        //
        // NOT a detection-range test, and it was written as one first. 2200 cm
        // detection is player-to-enemy; two pockets inside it do not merge,
        // they mean a fight can SPILL into its neighbour — which in a
        // destination is a feature and is the owner's to judge, not a rule to
        // assert here.
        TestTrue(TEXT("The entry yard's off-lane pocket is its own ground"),
            FMath::Min(FVector::Dist2D(PocketCenters[3], PocketCenters[0]),
                       FVector::Dist2D(PocketCenters[3], PocketCenters[1])) > 1800);
        TestTrue(TEXT("The substation's off-lane pocket is its own ground"),
            FVector::Dist2D(PocketCenters[4], PocketCenters[2]) > 1800);
        TestTrue(TEXT("Rank still sits in exactly one outdoor fight"), Elites == 1);

        // THE CONTRACT GIVER, ON THE MARKER THE COMPOSER SHIPPED. The yard
        // authored marker_npc_contract, the loader validated it, and nothing
        // read it — so Fernhall held no NPC at all and every quest in the game
        // was given in the hub. This asserts the seam AND the content: the
        // shipped Data/dialogue.json must actually carry his row, and the
        // shipped Data/quests.json his contract. A giver whose rows exist only
        // in a fixture is content the player cannot reach.
        {
            TArray<ABreakerNPC*> Npcs;
            for (TActorIterator<ABreakerNPC> It(World); It; ++It)
                if (!It->DialogueId.IsNone()) Npcs.Add(*It);
            if (TestEqual(TEXT("Fernhall stands exactly one contract giver"), Npcs.Num(), 1))
            {
                const ABreakerNPC* Keeper = Npcs[0];
                TestEqual(TEXT("and he is the Watchkeeper"), Keeper->DialogueId, FName(TEXT("Watchkeeper")));
                TestTrue(TEXT("with dialogue loaded from the shipped file"), Keeper->DialogueNodes.Num() > 0);

                TArray<FBreakerZonePiece> KeeperPieces;
                FBreakerZoneMarkers Markers;
                if (UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), KeeperPieces)
                    && UBreakerZoneBuilder::ExtractMarkers(KeeperPieces, Markers))
                {
                    const FBreakerZoneMarker* Contract = Markers.Find(EBreakerZoneMarkerRole::NPCContract);
                    if (TestNotNull(TEXT("the contract marker is authored"), Contract))
                    {
                        TestTrue(TEXT("he stands on it"),
                            FVector::Dist2D(Keeper->GetActorLocation(), Contract->Location) < 1.0f);
                    }
                }

                // SHIPPED CONFIGURATION: both rows resolve in the committed
                // data, not in anything this test built.
                const FBreakerDialogueRow* Row = ABreakerNPC::GetDialogueData().Npcs.FindByPredicate(
                    [](const FBreakerDialogueRow& E) { return E.Id == FName(TEXT("Watchkeeper")); });
                TestNotNull(TEXT("his dialogue row ships in Data/dialogue.json"), Row);
                const bool bQuestShips = UBreakerQuestLibrary::GetFallbackQuests().ContainsByPredicate(
                    [](const FBreakerQuestDefinition& Q) { return Q.QuestId == FName(TEXT("Quest.Watch")); });
                TestTrue(TEXT("his contract ships in Data/quests.json"), bQuestShips);
            }
        }
        TestEqual(TEXT("Entry retains its ranged pressure"), EntryLattices, 1);
        TestEqual(TEXT("Substation gains two melee flankers"), SubstationMelee, 2);
        TestEqual(TEXT("Substation has one Warden anchor"), SubstationWardens, 1);
        TestEqual(TEXT("Substation has one cover Skirmisher"), SubstationSkirmishers, 1);
        if (!SubstationSkirmisher || !SubstationWarden) return false;
        TArray<FBreakerZonePiece> Pieces;
        FBreakerZoneMarkers Markers;
        if (!UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces)
            || !UBreakerZoneBuilder::ExtractMarkers(Pieces, Markers)) return false;
        FVector2D Approach, Forward2D;
        if (!UBreakerZoneBuilder::YardFrame(Markers, TEXT("substation"), Approach, Forward2D)) return false;
        const FVector Forward(Forward2D.X, Forward2D.Y, 0);
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
        const FVector Threat(Approach.X, Approach.Y, SubstationSkirmisher->GetActorLocation().Z + 80.0f);
        FHitResult CoverHit;
        FCollisionQueryParams CoverQuery(SCENE_QUERY_STAT(FernhallFormationCover), false, SubstationSkirmisher);
        const bool bBlocked = World->LineTraceSingleByObjectType(CoverHit, Threat,
            SubstationSkirmisher->GetActorLocation() + FVector(0, 0, 60), FCollisionObjectQueryParams(ECC_WorldStatic), CoverQuery);
        TestTrue(TEXT("Real full-height cover hides the Skirmisher from approach at eye height"), bBlocked);
        const auto* CoverMesh = Cast<UStaticMeshComponent>(CoverHit.GetComponent());
        TestTrue(TEXT("Sightline blocker is an authored full-height piece"), CoverMesh && CoverMesh->GetStaticMesh()
            && CoverMesh->GetStaticMesh()->GetName().StartsWith(TEXT("blk_full_")));
        int32 LeftFlank = 0, RightFlank = 0;
        for (ABreakerEnemy* Enemy : Enemies)
            if (Enemy->Tags.Contains(FName(TEXT("Fernhall.Outdoor.2"))) && Enemy->GetClass() == ABreakerEnemy::StaticClass())
            {
                const FVector Offset = Enemy->GetActorLocation() - SubstationWarden->GetActorLocation();
                LeftFlank += FVector::DotProduct(Offset, Right) < -400.0f ? 1 : 0;
                RightFlank += FVector::DotProduct(Offset, Right) > 400.0f ? 1 : 0;
                TestTrue(TEXT("Melee flankers pressure ahead of the Warden"), FVector::DotProduct(Offset, Forward) < 0);
            }
        TestEqual(TEXT("One melee on each Warden flank"), LeftFlank, 1);
        TestEqual(TEXT("Opposite melee flank is occupied"), RightFlank, 1);
        const int32 XpBefore = Player->GetProgression()->GetProgressionState().TotalExperience;
        for (ABreakerEnemy* Enemy : Enemies)
        {
            if (Enemy->Tags.Contains(FName(TEXT("Fernhall.Outdoor.2")))) continue;
            Enemy->DispatchBeginPlay();
            FBreakerDamageRequest Kill;
            Kill.BaseDamage = 1000000;
            Kill.bCanCritical = false;
            Kill.bBypassShield = true;
            Kill.SetInstigator(Player);
            TestTrue(TEXT("Actual outdoor enemy dies to combat damage"),
                Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill).bKilled);
        }
        // 159 -> 267 as the yard gained six bodies. THE NUMBER IS NOT THE
        // POINT; the threshold under it is. 279 XP reaches level two, so a
        // cleared yard must stay BELOW that or the first contract's turn-in
        // stops being what levels the player and becomes a reward for
        // something they had already outgrown. Placing all twelve of wave
        // one read 375 and broke exactly that, which is why the pockets carry
        // three each. Assert the relationship, not just the figure — a future
        // population change that crosses the line should fail here and say so.
        const int32 ClearedYardXp = Player->GetProgression()->GetProgressionState().TotalExperience - XpBefore;
        TestEqual(TEXT("Entry kills retain their natural XP"), ClearedYardXp, 267);
        TestTrue(TEXT("A cleared yard still leaves level two to the contract's turn-in"),
            ClearedYardXp < 279);
        if (Visit == 0)
        {
            TestTrue(TEXT("Entry encounters complete the accepted spill objective"), Journal->HasFlag(TEXT("Quest.FirstContract.SpillThinned")));
            TestTrue(TEXT("Entry elite completes the accepted elite objective"), Journal->HasFlag(TEXT("Quest.FirstContract.EliteDown")));
        }
        else
        {
            TestFalse(TEXT("Return kills alone do not collect feedstock"), Journal->HasFlag(TEXT("Quest.KessSalvage.FeedstockTaken")));
            TArray<ABreakerFeedstockPickup*> Residue;
            for (TActorIterator<ABreakerFeedstockPickup> It(World); It; ++It) Residue.Add(*It);
            for (auto* Pickup : Residue)
            {
                Player->SetActorLocation(Pickup->GetActorLocation() + FVector(0,0,100));
                Pickup->TryCollect(Player);
            }
            TestTrue(TEXT("A return visit supplies physically collected feedstock"), Journal->HasFlag(TEXT("Quest.KessSalvage.FeedstockTaken")));
        }
        Mode->HandleStartingNewPlayer_Implementation(Controller);
        int32 AfterRepeat = 0, CourtyardAfterRepeat = 0;
        // Everything alive before the repopulation probe below, so the body it
        // finds afterwards is provably a NEW one rather than a survivor.
        TSet<ABreakerEnemy*> PreExisting;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ++AfterRepeat;
            PreExisting.Add(*It);
            CourtyardAfterRepeat += It->Tags.Contains(TEXT("Fernhall.Outdoor.Courtyard")) ? 1 : 0;
        }
        TestEqual(TEXT("Repeated startup preserves courtyard quartet"), CourtyardAfterRepeat, 4);
        if (Visit == 0)
        {
            // ---- ARRIVAL FICTION: the courtyard comes back, THROUGH ITS DOOR
            // The courtyard's roster was discarded at its call site, so the one
            // pocket in the yard with an authored doorway was also the only one
            // that never repopulated. Both halves are asserted here: that it
            // registers slots at all, and that a returning body appears at the
            // BAY MOUTH rather than resolving into existence standing on its
            // post. Last in the visit, after the cleared-yard XP figure above,
            // so a courtyard kill cannot move that number.
            ABreakerEnemy* Standing = nullptr;
            for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                if (It->Tags.Contains(TEXT("Fernhall.Outdoor.Courtyard")) && !It->IsDeadEnemy()) { Standing = *It; break; }
            if (!TestNotNull(TEXT("A courtyard body is standing to be killed"), Standing)) return false;
            const FVector Post = Standing->GetActorLocation();

            // The clearance gate is real and is not being bypassed: the player
            // is moved well beyond it so the refill is ALLOWED, which is the
            // shipped rule rather than a test-only exemption.
            Player->SetActorLocation(Post + FVector(0.0f, 0.0f, 40000.0f));
            Mode->OutdoorRepopulationDelaySeconds = 0.1f;

            Standing->DispatchBeginPlay();
            FBreakerDamageRequest Kill;
            Kill.BaseDamage = 1000000; Kill.bCanCritical = false; Kill.bBypassShield = true;
            Kill.SetInstigator(Player);
            if (!TestTrue(TEXT("The courtyard body dies to combat damage"),
                Standing->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill).bKilled)) return false;

            for (int32 Step = 0; Step < 40; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); }

            ABreakerEnemy* Returned = nullptr;
            for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                if (*It != Standing && It->Tags.Contains(TEXT("Fernhall.Outdoor.Courtyard")) && !It->IsDeadEnemy()
                    && !PreExisting.Contains(*It)) { Returned = *It; break; }
            if (!TestNotNull(TEXT("The courtyard repopulates at all"), Returned)) return false;
            TestTrue(TEXT("A returning courtyard body keeps the pocket's own tag"),
                Returned->Tags.Contains(TEXT("Fernhall.Outdoor.Courtyard")));
            // THE FICTION ITSELF. Appearing at the post is what every other
            // pocket does and is what this pocket would do with no doorway; the
            // courtyard has one, so the body starts a long way from where it is
            // going and walks. The posts sit thousands of centimetres inside
            // the bay, so this margin cannot be met by spawn jitter.
            const float FromPost = FVector::Dist2D(Returned->GetActorLocation(), Post);
            TestTrue(*FString::Printf(TEXT("It arrives through the doorway, not on its post (%.0f cm away)"), FromPost),
                FromPost > 1000.0f);
            // And it is pointed at where it is going, because it is arriving
            // rather than standing.
            const FVector ToPost = (Post - Returned->GetActorLocation()).GetSafeNormal2D();
            TestTrue(TEXT("and faces the post it is walking to"),
                FVector::DotProduct(Returned->GetActorForwardVector().GetSafeNormal2D(), ToPost) > 0.5f);
        }
        TestEqual(TEXT("Repeated startup preserves the outdoor population"), AfterRepeat - CourtyardAfterRepeat, 17);
        TestEqual(TEXT("Repeated startup preserves seventeen outdoor plus courtyard four"), AfterRepeat, 21);
        TestFalse(TEXT("Clearing outdoor encounters never starts waves"), Mode->IsWaveActive());
    }
    return true;
}
#endif
