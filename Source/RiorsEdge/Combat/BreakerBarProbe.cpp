// ---------------------------------------------------------------------------
// THE BAR PROBE — the instrument that makes the enemy read photographable.
//
// WHY IT EXISTS. Two visual rules shipped this project unphotographed, and not
// through carelessness: nothing in the harness could produce the frame they
// live in. The enemy bar culls at 50 m measured from the PLAYER, and autoplay
// spawns the player facing a berm; the crowd probe's grid stands at 60 m, past
// the cull; and `-BreakerCaptureTour` moves the CAMERA while the cull still
// measures from the player, so a vantage standing among enemies culls every bar
// anyway. O129's tint ramp is worse — it needs a DAMAGED body, and no console
// command in this project sets health or deals damage. So the bar, the bands,
// the shield hatch, the modifier marks and the whole health ramp had exactly
// one available frame: a crowd at full health, too far away to draw.
//
// The owner's first playtest landed on precisely that gap — "every bar reads as
// one flat red stripe" — and the ruling that followed is "photograph it at 12 m
// and 35 m before touching anything". This is what does that.
//
// WHAT IT MAKES. A frozen tableau in front of the player: four rank rows by
// five health columns, at each of two distances. Rank varies down, health
// varies across, so one frame carries the entire authored matrix and the two
// distances answer the two questions the pack's crowd study asks — does rank
// survive to 35 m, and does the ramp read at all.
//
// FROZEN IS THE POINT. The bodies have their tick disabled after posing, so
// they neither charge the camera nor move between frames. Two captures of the
// same tableau are comparable, which is what makes a before-and-after on a
// readability change mean anything.
//
// WHAT IT IS NOT. `DebugPoseHealthFraction` writes the health attribute and
// repaints. It fires no damage event, no hit reaction, no telemetry and no
// death. Nothing here may ever be used to test anything about damage — it is a
// camera rig, and a camera rig that starts answering gameplay questions is the
// false-negative instrument this project has already been bitten by once.
// ---------------------------------------------------------------------------

#include "Combat/BreakerEnemy.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING

namespace
{
    // Prefixed for the unity build as always.

    // The two distances the ruling names, in centimetres. NEAR is inside the
    // modifier marks' word band and comfortably inside the bar cull; FAR is
    // past the band where the pack's study says the gold edge and the rank
    // word have both fallen below legibility, which is the distance the rank
    // glyph exists for. O2 PLACEHOLDER, but these two came from the ruling
    // rather than from taste.
    constexpr float BreakerBarProbeNearCm = 1200.0f;
    constexpr float BreakerBarProbeFarCm = 3500.0f;

    // Grid pitch. Wide enough that neighbouring bars do not overlap at NEAR,
    // tight enough that the whole matrix fits a 90 degree horizontal FOV.
    constexpr float BreakerBarProbeColumnPitchCm = 240.0f;
    constexpr float BreakerBarProbeRowPitchCm = 300.0f;

    // Retry while the world settles. -BreakerAutoPlay travels to the gym, so a
    // command fired from -ExecCmds at engine init arrives before there is a
    // pawn to stand in front of. Retry rather than fail: an instrument that
    // silently does nothing is the shape this file exists to remove.
    constexpr float BreakerBarProbeRetrySeconds = 0.5f;
    constexpr int32 BreakerBarProbeMaxAttempts = 40;
    // A PAWN IS NOT ENOUGH, AND THAT COST TWO CAPTURES. -BreakerAutoPlay
    // TRAVELS to the gym, and the front end it travels FROM already has a
    // player pawn — so "wait until a pawn exists" is satisfied at frame one, on
    // the world about to be torn down, and forty bodies are placed into a level
    // that ceases to exist a moment later. The probe reported success both
    // times and photographed an empty berm both times, which is the
    // silently-does-nothing shape this file's own header warns about.
    //
    // The floor puts the first attempt after travel has settled and before the
    // capture harness takes its first frame at 6.0 s. Ten attempts at half a
    // second is 5.0 s.
    constexpr int32 BreakerBarProbeMinAttempts = 10;

    ABreakerCharacter* BreakerBarProbeFindPlayer(UWorld* World)
    {
        for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
        {
            if (*It) return *It;
        }
        return nullptr;
    }

    void BreakerBarProbePlaceBank(UWorld* World, const FVector& Origin,
        const FVector& Forward, const FVector& Right, float DistanceCm, int32& OutPlaced)
    {
        const EBreakerMonsterRank Ranks[] = {
            EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
            EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };
        // The authored stops, so the columns ARE the table rather than a
        // sampling of it.
        const float Fractions[] = { 1.0f, 0.75f, 0.5f, 0.25f, 0.10f };
        constexpr int32 Columns = UE_ARRAY_COUNT(Fractions);
        constexpr int32 Rows = UE_ARRAY_COUNT(Ranks);

        for (int32 Row = 0; Row < Rows; ++Row)
        {
            for (int32 Column = 0; Column < Columns; ++Column)
            {
                const float Lateral = (Column - (Columns - 1) * 0.5f) * BreakerBarProbeColumnPitchCm;
                const float Depth = DistanceCm + Row * BreakerBarProbeRowPitchCm;
                const FVector Spot = Origin + Forward * Depth + Right * Lateral + FVector(0, 0, 100.0f);

                FActorSpawnParameters Params;
                Params.ObjectFlags |= RF_Transient;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(
                    ABreakerEnemy::StaticClass(), Spot, (-Forward).Rotation(), Params);
                if (!Enemy) continue;

                Enemy->ConfigureCrowdProbe();
                Enemy->SetAreaLevel(10);
                // Modifiers on the ModifierBearing row only, and three of them,
                // because three is the authored cap and the mark row's whole
                // job is to say "that one has three of something" at range.
                if (Ranks[Row] == EBreakerMonsterRank::ModifierBearing)
                {
                    Enemy->ConfigureWithExactModifiers({
                        EBreakerEnemyModifier::Warded,
                        EBreakerEnemyModifier::Volatile,
                        EBreakerEnemyModifier::Reflective });
                }
                Enemy->SetMonsterRank(Ranks[Row]);
                Enemy->DebugPoseHealthFraction(Fractions[Column]);
                // FROZEN. No chase, no patrol, no drift between frames.
                Enemy->SetActorTickEnabled(false);
                ++OutPlaced;
            }
        }
    }

    // THE CURRENT GAME WORLD, resolved fresh every tick rather than captured.
    // The world this command was invoked on is the FRONT END, and it is about
    // to be destroyed by -BreakerAutoPlay's travel.
    UWorld* BreakerBarProbeCurrentWorld()
    {
        if (!GEngine) return nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
                && Context.World())
            {
                return Context.World();
            }
        }
        return nullptr;
    }

    void BreakerBarProbePlaceNow(UWorld* World, ABreakerCharacter* Player)
    {
        // ORIGIN FROM THE PAWN, DIRECTION FROM THE CAMERA, and the split is the
        // whole reason the first run of this photographed an empty berm. The
        // bar cull measures distance from the PLAYER, so the origin has to be
        // the pawn or the tableau lands outside the range it was built to
        // test. The capture photographs the CAMERA, so the direction has to be
        // the camera's or the tableau lands off-frame. Using the pawn for both
        // put forty bodies behind the player's shoulder — the same
        // camera-versus-player mismatch this lane reported in the capture tour,
        // walked into while building the instrument that exists to expose it.
        const FVector Origin = Player->GetActorLocation();
        FVector Forward = Player->GetActorForwardVector().GetSafeNormal2D();
        if (const APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
        {
            if (Controller->PlayerCameraManager)
            {
                const FVector CameraForward =
                    Controller->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal2D();
                if (!CameraForward.IsNearlyZero()) Forward = CameraForward;
            }
        }
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

        // DIAGNOSTICS OFF, because the probe photographs the SHIPPING read and
        // the F3 overlay is not it. `bDiagnosticsVisible` defaults to TRUE, and
        // one of the things it draws is an orange GetEnemyStateLabel over every
        // enemy within 25 m — no focus gate, no occlusion suppression, no cap.
        // That pass is the label collision in the owner's playtest frame, and
        // leaving it on here would photograph a debug overlay and call it the
        // bar. Isolating the subject is the instrument's job.
        if (UBreakerPlaytestComponent* Playtest = Player->GetPlaytest())
        {
            if (Playtest->AreDiagnosticsVisible())
            {
                Playtest->ToggleDiagnostics();
                UE_LOG(LogTemp, Display,
                    TEXT("[BreakerBarProbe] diagnostics were ON (the default) and are suppressed ")
                    TEXT("for this capture; the F3 overlay is not the shipping read."));
            }
        }

        int32 Placed = 0;
        BreakerBarProbePlaceBank(World, Origin, Forward, Right, BreakerBarProbeNearCm, Placed);
        BreakerBarProbePlaceBank(World, Origin, Forward, Right, BreakerBarProbeFarCm, Placed);

        // The log half of the proof: a headless reader greps this for WHAT was
        // posed and WHERE, then reads the frames for whether the world agreed.
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerBarProbe] placed %d frozen bodies: 4 rank rows x 5 health columns ")
            TEXT("(1.00/0.75/0.50/0.25/0.10) at %.0f cm and %.0f cm ahead of the pawn."),
            Placed, BreakerBarProbeNearCm, BreakerBarProbeFarCm);
    }

    // A CORE TICKER, NOT A WORLD TIMER, for the reason the capture harness
    // already gives for its own: a world timer dies with its world. The first
    // attempt at this scheduled its retry on the FRONT END's timer manager,
    // travel destroyed that manager, and the retry chain simply stopped —
    // silently, with the probe having reported success on a level that no
    // longer existed. The core ticker outlives travel and re-resolves the world
    // every attempt.
    void BreakerBarProbeArm()
    {
        TSharedPtr<int32> Attempt = MakeShared<int32>(0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [Attempt](float) -> bool
            {
                const int32 Now = (*Attempt)++;
                if (Now >= BreakerBarProbeMaxAttempts)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[BreakerBarProbe] no player pawn after %d attempts; nothing placed."), Now);
                    return false;
                }
                if (Now < BreakerBarProbeMinAttempts) return true;   // see the floor above
                UWorld* World = BreakerBarProbeCurrentWorld();
                ABreakerCharacter* Player = World ? BreakerBarProbeFindPlayer(World) : nullptr;
                if (!World || !Player) return true;
                BreakerBarProbePlaceNow(World, Player);
                return false;
            }), BreakerBarProbeRetrySeconds);
    }

    // --- THE HEADLESS DAMAGE ENTRY POINT ----------------------------------
    // Asked for by GROUND, and owed by ORDERS twice over. The rift chain
    // (O168) is unit-tested at every branch and the wiring is proven by build,
    // but the end-to-end mark-then-kill path has never run: the harness can
    // drive a mark, and could not drive a kill. Separately, Part One-C's scope
    // caveat says the crowd sweep is HALF A FIGHT because nothing in it takes
    // damage, and closing that needs a damage entry point in `Combat/` — this
    // file's directory.
    //
    // IT GOES THROUGH ReceiveDamage, NOT AROUND IT. That is the entire value:
    // mitigation, the band-break bit, the hit reaction, the damage number, the
    // death beat, loot, telemetry and O168's terminator raise all hang off the
    // real path, and a command that wrote health directly would prove none of
    // them. `DebugPoseHealthFraction` in this same file deliberately does NOT
    // do this — it poses a body for a photograph and fires nothing. The two
    // must not be confused, which is why they read so differently.
    //
    // TrueDamage so a kill is a kill: armour, shields and resistances are
    // exactly the systems a verification run must not have to defeat first.
    void BreakerFieldDamageNearest(UWorld* World, float Amount)
    {
        if (!World) return;
        ABreakerCharacter* Player = BreakerBarProbeFindPlayer(World);
        if (!Player)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreakerField] no player pawn; nothing damaged."));
            return;
        }

        ABreakerEnemy* Nearest = nullptr;
        float NearestSq = TNumericLimits<float>::Max();
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Candidate = *It;
            if (!Candidate || Candidate->IsDeadEnemy()) continue;
            const float DistanceSq = FVector::DistSquared(Player->GetActorLocation(), Candidate->GetActorLocation());
            if (DistanceSq < NearestSq)
            {
                NearestSq = DistanceSq;
                Nearest = Candidate;
            }
        }
        if (!Nearest)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreakerField] no live enemy in the world; nothing damaged."));
            return;
        }

        UBreakerCombatComponent* Combat = Nearest->FindComponentByClass<UBreakerCombatComponent>();
        if (!Combat)
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreakerField] nearest enemy has no combat component."));
            return;
        }

        FBreakerDamageRequest Request;
        // A non-positive argument means LETHAL: the caller wants the body gone
        // and should not have to know its health. Sized off the target's own
        // max health rather than a large literal, so it stays lethal when the
        // chassis curve moves.
        Request.BaseDamage = Amount > 0.0f ? Amount : FMath::Max(1.0f, Nearest->GetMonsterMaxHealth() * 4.0f);
        Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Request.Instigator = Player;

        const float Before = Nearest->GetMonsterMaxHealth();
        const bool bTerminator = Nearest->IsRiftTerminator();
        Combat->ReceiveDamage(Request);
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerField] dealt %.0f TrueDamage to the nearest enemy at %.0f cm ")
            TEXT("(max health %.0f, rift terminator=%d, dead now=%d) through the real ReceiveDamage path."),
            Request.BaseDamage, FMath::Sqrt(NearestSq), Before, bTerminator ? 1 : 0,
            Nearest->IsDeadEnemy() ? 1 : 0);
    }

    FAutoConsoleCommandWithWorldAndArgs GBreakerFieldDamageCommand(
        TEXT("Breaker.Field.Damage"),
        TEXT("Deals TrueDamage to the nearest live enemy through the real damage path. ")
        TEXT("Optional amount; omitted or non-positive means lethal."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
            [](const TArray<FString>& Args, UWorld* World)
            {
                const float Amount = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f;
                BreakerFieldDamageNearest(World, Amount);
            }));

    // --- THE ONE RUN NEITHER LANE COULD TAKE -------------------------------
    // GROUND: "the end-to-end mark-then-kill path is NOT verified in play —
    // the harness can drive neither a mark nor a kill. I would rather one of us
    // confirm it than both of us assume it." This drives the whole O168 chain
    // through PUBLIC CONSOLE COMMANDS ONLY — GROUND's Breaker.MarkTerminator
    // and this file's Breaker.Field.Damage — so no lane reaches into another's
    // code to test it, which is the same boundary the seam itself keeps.
    //
    // Sequenced on the core ticker because -ExecCmds fires everything at engine
    // init, in one breath, before any body exists to mark. It waits for a live
    // enemy (the bar probe supplies them), marks, then kills on a LATER tick —
    // the mark must be committed before the damage lands or the kill arrives at
    // an unmarked body and the whole run silently proves nothing.
    void BreakerFieldVerifyRiftChain()
    {
        TSharedPtr<int32> Stage = MakeShared<int32>(0);
        TSharedPtr<int32> Attempt = MakeShared<int32>(0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [Stage, Attempt](float) -> bool
            {
                if ((*Attempt)++ > BreakerBarProbeMaxAttempts)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[BreakerField] rift-chain check gave up waiting for a live enemy."));
                    return false;
                }
                UWorld* World = BreakerBarProbeCurrentWorld();
                if (!World || !GEngine) return true;

                if (*Stage == 0)
                {
                    bool bHasLiveEnemy = false;
                    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                    {
                        if (*It && !It->IsDeadEnemy()) { bHasLiveEnemy = true; break; }
                    }
                    if (!bHasLiveEnemy) return true;
                    GEngine->Exec(World, TEXT("Breaker.MarkTerminator"));
                    UE_LOG(LogTemp, Display, TEXT("[BreakerField] rift-chain check: marked, killing next tick."));
                    *Stage = 1;
                    return true;
                }

                GEngine->Exec(World, TEXT("Breaker.Field.Damage"));
                UE_LOG(LogTemp, Display,
                    TEXT("[BreakerField] rift-chain check: kill issued. A [Rift] line below is the seam firing — ")
                    TEXT("'run complete' if a rift is set, 'completion refused' if none is, and EITHER proves ")
                    TEXT("the raise reached the consume site."));
                return false;
            }), BreakerBarProbeRetrySeconds);
    }

    FAutoConsoleCommandWithWorld GBreakerFieldVerifyRiftChainCommand(
        TEXT("Breaker.Field.VerifyRiftChain"),
        TEXT("Marks the nearest enemy and kills it, driving O168's raise-consume-pay chain end to end."),
        FConsoleCommandWithWorldDelegate::CreateStatic(
            [](UWorld*) { BreakerFieldVerifyRiftChain(); }));

    FAutoConsoleCommandWithWorld GBreakerBarProbeCommand(
        TEXT("Breaker.Field.BarProbe"),
        TEXT("Freezes a 4 rank x 5 health matrix at 12 m and 35 m ahead of the pawn, ")
        TEXT("so the enemy bar and the O129 tint ramp can be photographed."),
        FConsoleCommandWithWorldDelegate::CreateStatic(
            [](UWorld*) { BreakerBarProbeArm(); }));
}

#endif
