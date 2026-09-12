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
#include "Game/BreakerPocketRift.h"
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
        PocketCenters.Init(FVector::ZeroVector, 11);
        int32 PocketCounts[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        int32 Elites = 0;
        int32 SubstationMelee = 0, SubstationWardens = 0, SubstationSkirmishers = 0, EntryLattices = 0;
        int32 DepotWardens = 0, DepotElites = 0, DepotBodies = 0;
        // THE ENTRY YARD ALONE, for the XP pin below. It is read off the body's
        // own AREA LEVEL rather than off a list of pocket indices, because a
        // list is what was wrong here before: the loop skipped pocket 2 and
        // counted pocket 4 — a SUBSTATION fight — as entry-yard XP. A yard's
        // level is the thing that actually says which yard a body is in.
        int32 EntryXp = 0;
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
            for (int32 Index = 0; Index < 11; ++Index)
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
            if (Pocket >= 5 && Pocket < 8)
            {
                ++DepotBodies;
                if (Enemy->IsA<ABreakerWardenEnemy>()) ++DepotWardens;
                if (Enemy->GetMonsterRank() != EBreakerMonsterRank::Trash) ++DepotElites;
                TestEqual(TEXT("The depot is the deepest yard and says so"), Enemy->GetAreaLevel(), 13);
            }
            if (Pocket >= 8)
            {
                // The siding is beside the entry yard, not above it (O276).
                TestEqual(TEXT("The siding sits one rung over the entry yard"), Enemy->GetAreaLevel(), 6);
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
        AddInfo(FString::Printf(TEXT("FERNHALL POCKETS  total %d, per pocket %d/%d/%d/%d/%d/%d/%d/%d, elites %d"),
            Enemies.Num(), PocketCounts[0], PocketCounts[1], PocketCounts[2], PocketCounts[3],
            PocketCounts[4], PocketCounts[5], PocketCounts[6], PocketCounts[7], Elites));
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
        // 17 -> 26 WITH A THIRD YARD. The depot is two seams deep, so its nine
        // bodies cannot reach the entry yard's XP and cannot change what the
        // first contract is worth; that is why the expansion went there rather
        // than into the yards the player already crosses.
        // 26 -> 35 WITH THE SIDING (O276): nine level-6 Skitters one seam off
        // the plaza. They CAN be reached before the first turn-in; what that
        // does to the contract's worth is the owner's to feel, recorded at the
        // pocket table.
        if (!TestEqual(TEXT("Thirty-five outdoor enemies populate each fresh visit"), Enemies.Num(), 35)) return false;
        TestEqual(TEXT("Four additional courtyard enemies"), CourtyardCount, 4);
        TestEqual(TEXT("Three ordinary courtyard melee"), CourtyardMelee, 3);
        TestEqual(TEXT("One courtyard Lattice"), CourtyardLattices, 1);
        // ONE IN THE ENTRY YARD, ONE IN THE DEPOT. The entry elite is what the
        // first contract's elite objective counts, and it has to stay exactly
        // one; the depot's is the rank the deepest yard carries, and it is two
        // seams away from anything the contract measures.
        // FOUR, AND THE NUMBER IS THE QUEST'S. Quest.Pattern asks for three
        // marked kills and its objective is elite-gated, so the world has to
        // stand at least three where the player will meet them or the counter
        // reaches 2 of 3 and stops — which is what "quest marked enemies are
        // still bugged" actually was. One in the entry yard, TWO in the
        // substation, one in the depot: three before the deepest seam and a
        // fourth past it. Assert the RELATIONSHIP as well as the figure, so a
        // future population change that drops below the quest fails here.
        TestEqual(TEXT("Four ranked outdoor bodies"), Elites, 4);
        TestTrue(TEXT("and enough of them for the marked-kill contract"), Elites >= 3);
        TestEqual(TEXT("The depot carries exactly one of them"), DepotElites, 1);
        TestEqual(TEXT("and a Warden to anchor its set piece"), DepotWardens, 1);
        TestEqual(TEXT("Nine bodies stand in the third yard"), DepotBodies, 9);
        const int32 ExpectedCounts[] = { 4, 3, 4, 3, 3, 3, 3, 3, 3, 3, 3 };
        for (int32 Index = 0; Index < 11; ++Index)
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
        // The depot's three are their own ground too, by the same measure.
        for (int32 Index = 5; Index < 8; ++Index)
            for (int32 Other = 0; Other < 11; ++Other)
                if (Other != Index)
                    TestTrue(*FString::Printf(TEXT("Depot pocket %d is its own ground against %d"), Index, Other),
                        FVector::Dist2D(PocketCenters[Index], PocketCenters[Other]) > 1800);
        // And the siding's three (O276), by the same measure.
        for (int32 Index = 8; Index < 11; ++Index)
            for (int32 Other = 0; Other < 11; ++Other)
                if (Other != Index)
                    TestTrue(*FString::Printf(TEXT("Siding pocket %d is its own ground against %d"), Index, Other),
                        FVector::Dist2D(PocketCenters[Index], PocketCenters[Other]) > 1800);
        // And the third yard is a YARD away, not a corner of the second.
        TestTrue(TEXT("The depot occupies its own yard"),
            FVector::Dist2D(PocketCenters[6], PocketCenters[2]) > 4000);

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
            // ENTRY YARD ONLY. Read off the body's area level, not off a list
            // of pocket numbers — see the note at EntryXp above.
            if (Enemy->GetAreaLevel() != 5) continue;
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
        EntryXp = ClearedYardXp;
        AddInfo(FString::Printf(TEXT("ENTRY YARD CLEARED  %d XP, ceiling 279"), EntryXp));
        // 267 -> 207, AND THE OLD FIGURE WAS NEVER THE ENTRY YARD'S. This loop
        // used to skip pocket 2 by name and count pocket 4 — a SUBSTATION fight
        // — as entry XP, so the pin it fed was measuring two yards and calling
        // them one. Reading the body's own area level fixes that permanently:
        // a yard added anywhere else can no longer move this number.
        TestEqual(TEXT("Entry kills retain their natural XP"), ClearedYardXp, 207);
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

            // ---- O274: THE WORLD AT REST SHOWS NO TEARS, and a tear stands
            // only where the composer authored no opening. Every yard pocket
            // returns through EITHER an authored spawn site within reach of
            // its formation (a bay mouth, a dock face, a seam mouth) OR a
            // tear, never both and never neither — and every tear is closed
            // until a return.
            TArray<ABreakerPocketRift*> Tears;
            for (TActorIterator<ABreakerPocketRift> It(World); It; ++It) Tears.Add(*It);
            for (const ABreakerPocketRift* Tear : Tears)
            {
                TestFalse(TEXT("A tear is not open before anything returns"), Tear->IsOpen());
                TestTrue(TEXT("and draws nothing at rest"), Tear->IsClosed());
            }
            // WHICH POCKETS TEAR, read off the world rather than off a list:
            // a tear stands 750 cm past its pocket's centre, so each one is
            // handed to the pocket whose body mean is nearest. The tightest
            // pair in the zone (siding 8 and 10) puts a neighbour's tear at
            // ~1500 cm against ~900 for the pocket's own, so nearest is
            // unambiguous; a tear nearer no pocket than that is a stray.
            const FName PocketYards[] = { NAME_None, NAME_None, FName(TEXT("substation")),
                                          NAME_None, FName(TEXT("substation")),
                                          FName(TEXT("depot")), FName(TEXT("depot")), FName(TEXT("depot")),
                                          FName(TEXT("siding")), FName(TEXT("siding")), FName(TEXT("siding")) };
            bool bPocketTears[11] = {};
            for (const ABreakerPocketRift* Tear : Tears)
            {
                int32 Nearest = INDEX_NONE;
                float NearestCm = TNumericLimits<float>::Max();
                for (int32 Index = 0; Index < 11; ++Index)
                {
                    const float Cm = FVector::Dist2D(Tear->GetActorLocation(), PocketCenters[Index]);
                    if (Cm < NearestCm) { NearestCm = Cm; Nearest = Index; }
                }
                TestTrue(*FString::Printf(TEXT("A tear stands beside a pocket (%.0f cm from pocket %d)"), NearestCm, Nearest),
                    Nearest != INDEX_NONE && NearestCm < 1500.0f);
                if (Nearest == INDEX_NONE) continue;
                TestFalse(*FString::Printf(TEXT("Pocket %d placed at most one tear"), Nearest), bPocketTears[Nearest]);
                bPocketTears[Nearest] = true;
            }
            int32 TearPockets = 0, SitePockets = 0;
            for (int32 Index = 0; Index < 11; ++Index)
            {
                if (bPocketTears[Index]) ++TearPockets; else ++SitePockets;
                // The rule the mode reads, read again here: a pocket tears
                // exactly when no site of its yard is within reach of its
                // formation centre. The body mean is up to a few hundred cm
                // off that centre (the Warden formations more than the plain
                // ones), so each side of the rule is checked with that slack
                // rather than at the boundary itself.
                constexpr float CentreSlackCm = 500.0f;
                const bool bSiteNear = UBreakerZoneBuilder::NearestSpawnSite(Markers, PocketYards[Index],
                    PocketCenters[Index], UBreakerZoneBuilder::FernhallSpawnSiteReachCm - CentreSlackCm) != nullptr;
                const bool bSiteFar = UBreakerZoneBuilder::NearestSpawnSite(Markers, PocketYards[Index],
                    PocketCenters[Index], UBreakerZoneBuilder::FernhallSpawnSiteReachCm + CentreSlackCm) == nullptr;
                if (bSiteNear)
                    TestFalse(*FString::Printf(TEXT("Pocket %d has an opening well in reach and placed no tear"), Index), bPocketTears[Index]);
                if (bSiteFar)
                    TestTrue(*FString::Printf(TEXT("Pocket %d has no opening near reach and placed a tear"), Index), bPocketTears[Index]);
            }
            AddInfo(FString::Printf(TEXT("FERNHALL RETURNS  %d pockets through authored openings, %d through tears, %d tears standing"),
                SitePockets, TearPockets, Tears.Num()));
            TestEqual(TEXT("Tears stand exactly where no opening was in reach"), Tears.Num(), TearPockets);
            TestEqual(TEXT("and every pocket returns through one or the other"), TearPockets + SitePockets, 11);
            TestTrue(TEXT("At least one pocket returns through an authored opening (O274: authored ground first)"), SitePockets >= 1);
            TestTrue(TEXT("and at least one still tears, where the composer authored nothing"), TearPockets >= 1);

            Standing->DispatchBeginPlay();
            FBreakerDamageRequest Kill;
            Kill.BaseDamage = 1000000; Kill.bCanCritical = false; Kill.bBypassShield = true;
            Kill.SetInstigator(Player);
            if (!TestTrue(TEXT("The courtyard body dies to combat damage"),
                Standing->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill).bKilled)) return false;

            // EVERY RETURN IS READ ON ITS FIRST FRAME, because where a body
            // APPEARS is the whole claim and a body walks off that point at
            // once. A yard body that was not standing before the probe is
            // caught the tick it exists: if its pocket tears, it stands at an
            // open tear; if its pocket has an opening, it stands ON the
            // authored site with no tear anywhere near it. Either way it is
            // emerging — the window is the same clock on both paths.
            TSet<ABreakerEnemy*> Seen = PreExisting;
            Seen.Add(Standing);
            int32 SiteArrivals = 0, TearArrivals = 0;
            auto Tick = [&](int32 Steps)
            {
                for (int32 Step = 0; Step < Steps; ++Step)
                {
                    ++GFrameCounter;
                    World->Tick(LEVELTICK_All, 0.05f);
                    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                    {
                        if (Seen.Contains(*It)) continue;
                        Seen.Add(*It);
                        int32 Pocket = INDEX_NONE;
                        for (int32 Index = 0; Index < 11; ++Index)
                            if (It->Tags.Contains(FName(*FString::Printf(TEXT("Fernhall.Outdoor.%d"), Index)))) Pocket = Index;
                        if (Pocket == INDEX_NONE) continue;   // the courtyard's is read below
                        const FVector At = It->GetActorLocation();
                        TestTrue(*FString::Printf(TEXT("A pocket %d body emerges on arrival"), Pocket), It->IsEmerging());
                        float NearestTearCm = TNumericLimits<float>::Max();
                        const ABreakerPocketRift* NearestTear = nullptr;
                        for (const ABreakerPocketRift* Tear : Tears)
                        {
                            const float Cm = FVector::Dist2D(At, Tear->GetActorLocation());
                            if (Cm < NearestTearCm) { NearestTearCm = Cm; NearestTear = Tear; }
                        }
                        if (bPocketTears[Pocket])
                        {
                            ++TearArrivals;
                            TestTrue(*FString::Printf(TEXT("A pocket %d body comes out of its tear (%.0f cm)"), Pocket, NearestTearCm),
                                NearestTear && NearestTearCm < 400.0f && NearestTear->IsOpen());
                        }
                        else
                        {
                            ++SiteArrivals;
                            const FBreakerZoneMarker* Site = UBreakerZoneBuilder::NearestSpawnSite(
                                Markers, PocketYards[Pocket], At, 50.0f);
                            TestNotNull(*FString::Printf(TEXT("A pocket %d body appears on an authored opening of its yard"), Pocket), Site);
                            TestTrue(*FString::Printf(TEXT("and no tear stands near that opening (%.0f cm)"), NearestTearCm),
                                NearestTearCm >= 300.0f);
                        }
                    }
                }
            };
            // A body that was not standing before the probe and is alive now.
            auto FindNew = [&](const TCHAR* Tag) -> ABreakerEnemy*
            {
                for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                    if (*It != Standing && !It->IsDeadEnemy() && !PreExisting.Contains(*It)
                        && (!Tag || It->Tags.Contains(FName(Tag)))) return *It;
                return nullptr;
            };

            // ---- THE CLOCKS, IN ORDER. The ten entry-yard bodies killed for
            // the XP figure above (pockets 0, 1 and 3) hold LOWER slot indices
            // than the courtyard's and have stood empty exactly as long, so
            // the shared clock claims them first, pocket 0's first slot at
            // 0.10 s, then one more every 0.10 s; the courtyard's is the
            // eleventh, at 1.10 s. That is the order the shipped rule produces
            // and the probe reads it rather than steering it.
            //
            // A TEAR OPENS BEFORE ITS BODY EXISTS (O274). Two ticks is the
            // first claim, pocket 0's. Where pocket 0 tears: the tear is open,
            // and NOTHING has come through. Where pocket 0 has an authored
            // opening: no tear opens, and its body is already standing on the
            // site — the claim IS the arrival, as it is for the courtyard.
            Tick(2);
            ABreakerPocketRift* Torn = nullptr;
            int32 OpenTears = 0;
            for (ABreakerPocketRift* Tear : Tears)
                if (Tear->IsOpen()) { ++OpenTears; Torn = Tear; }
            if (bPocketTears[0])
            {
                TestEqual(TEXT("The first claim opens exactly one tear"), OpenTears, 1);
                TestNull(TEXT("and no body has come through it yet: the tear opens first"), FindNew(nullptr));
            }
            else
            {
                TestEqual(TEXT("The first claim opens no tear: pocket 0 returns through authored ground"), OpenTears, 0);
                TestNotNull(TEXT("and its body already stands on the opening"), FindNew(TEXT("Fernhall.Outdoor.0")));
            }

            // THE BODY COMES THROUGH ONCE THE TEAR HAS OPENED. The claim was
            // at 0.10 s and the arrival is AppearSeconds (0.8) after it, so by
            // 1.10 s — one claim's worth of margin — a new pocket-0 body stands
            // at the tear that opened for it.
            Tick(20);
            ABreakerEnemy* Emerged = FindNew(TEXT("Fernhall.Outdoor.0"));
            if (TestNotNull(TEXT("A pocket body returns once its claim has resolved"), Emerged) && Torn && bPocketTears[0])
            {
                TestTrue(TEXT("and it came out of the tear that opened"),
                    FVector::Dist2D(Emerged->GetActorLocation(), Torn->GetActorLocation()) < 400.0f);
                TestTrue(TEXT("which is still open while it walks out"), Torn->IsOpen());
            }

            // THE COURTYARD, INSIDE THE ORIGINAL 2 s BUDGET. Its slot has no
            // tear (it arrives through the authored bay doorway), so its claim
            // at 1.10 s is its arrival — the O268 behaviour, untouched.
            Tick(18);

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

            // ---- AND THEN IT CLOSES (O274). Pocket 0's four slots are claimed
            // at 0.10-0.40 s and arrive at 0.90-1.20 s; the close is armed
            // CloseAfterSeconds (2.0) after the LAST of them, at 3.20 s, and
            // the tear is shut CloseSeconds (0.6) later, at 3.80 s. The last
            // entry-yard arrival of all is pocket 3's third, claimed at 1.00 s,
            // through at 1.80 s, closing at 3.80 s and shut at 4.40 s. Ticked
            // to 5.00 s: 0.60 s of slack for the timer manager's own accounting
            // and forty ticks of float accumulation, and not a second more.
            Tick(60);
            if (Torn)
            {
                TestFalse(TEXT("The tear closes after its last arrival"), Torn->IsOpen());
                TestTrue(TEXT("and draws nothing once it has"), Torn->IsClosed());
            }
            for (const ABreakerPocketRift* Tear : Tears)
                TestTrue(TEXT("The world at rest shows no tears"), Tear->IsClosed());
            // The ten killed entry-yard bodies (pockets 0, 1, 3) have all had
            // time to come back by now; each was read on its first frame
            // above. Logged rather than pinned per path, because which of the
            // three pockets has an opening in reach is the composer's call.
            AddInfo(FString::Printf(TEXT("FERNHALL RETURNS PROBED  %d through authored openings, %d through tears"),
                SiteArrivals, TearArrivals));
            TestTrue(TEXT("The killed entry-yard pockets came back at all"), SiteArrivals + TearArrivals >= 1);
        }
        TestEqual(TEXT("Repeated startup preserves the outdoor population"), AfterRepeat - CourtyardAfterRepeat, 35);
        TestEqual(TEXT("Repeated startup preserves thirty-five outdoor plus courtyard four"), AfterRepeat, 39);
        TestFalse(TEXT("Clearing outdoor encounters never starts waves"), Mode->IsWaveActive());
    }
    return true;
}
#endif
