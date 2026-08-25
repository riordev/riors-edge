// ---------------------------------------------------------------------------
// The enemy health bar, and nothing else.
//
// WHY THIS FILE EXISTS, AND WHY IT IS IN Combat/ RATHER THAN UI/. Two lanes
// shipped a trash-bar rule on the same day — b44c0fc and d967342 — and both
// merged clean, because they touched different lines of one 3,596-line HUD
// file. That is not a merge accident to be handled better next time; it is two
// owners on one question, and it recurs until the question has one owner. The
// bar answers "which enemy am I fighting, and how close is it to dead", which
// is a combat read, so the directory that names its owner is this one.
//
// The function below is still a member of ABreakerPlaytestHUD and still draws
// through the HUD's canvas. It has to be: a bar is projected world-space
// geometry over a Canvas the HUD owns. A member function's DEFINITION may live
// in any translation unit and keeps full private access, so this split cost
// the HUD class exactly nothing — no widened access, no exported helpers, no
// friend declaration. The five visibility constants moved here whole because
// nothing outside this pass ever read them.
//
// What did NOT move: DrawMinimap consumes EnemyBlips, which the enemy loop
// below fills. That ordering contract is restated at the loop; it is the one
// coupling that survives the cut.
// ---------------------------------------------------------------------------

#include "UI/BreakerPlaytestHUD.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerTargetDummy.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UI/BreakerUIStyle.h"

// Named for this pass, not BreakerHUD: unity builds merge translation units,
// and a second `namespace BreakerHUD` carrying its own constants under the old
// names is a redefinition the moment the two files land in one blob. The
// project has shipped that collision twice under other names.
namespace BreakerEnemyBar
{
    // Enemy bar visibility rules.
    static constexpr float MaxDistance = 5000.0f;
    // Aim cone for "the enemy I am asking about". Presentation, not balance:
    // it decides which enemy gets a bar and a verbose label, never anything
    // about damage or aim. Roughly matches the loot focus cone so the two
    // agree about what the player is pointing at.
    static constexpr float FocusMinimumDot = 0.985f;
    // How long a trash bar lingers after the aim leaves it (selective bars;
    // above-Trash ranks are always barred). O2 PLACEHOLDER.
    static constexpr float FocusFadeSeconds = 0.6f;

    // --- The dummy block's own two constants -------------------------------
    // These feed the TARGET DUMMY loop and nothing else, and that is a ruling
    // rather than an accident of where the code sits. A dummy is a gym
    // INSTRUMENT, not a crowd member: there are four of them, they never move,
    // and the single question they exist to answer is "did that hurt". A
    // recency window answers exactly that question, which is why the enemy
    // path dropping it does not take the dummy path with it. The enemies went
    // aimed-at-only because at fifty to a hundred concurrent with cleave in
    // the kit, recency is not a filter — one AoE lights the whole pack. Four
    // stationary targets are not a pack.
    //
    // So this is not a rebase artifact that happens to still work. Do not
    // "finish" the selective-bar change by deleting it.
    //
    // The 6.0s -> 1.5s retune stands on its own: at six seconds a bar outlives
    // the shot that earned it. O2 PLACEHOLDER — the owner tunes this in hand.
    static constexpr float RecentDamageSeconds = 1.5f;
    static constexpr float AlwaysDistance = 1500.0f;
}

namespace
{
    // Where the bar sits and how big it is. Both loops below place it
    // identically — centred on the projected point, shrinking with distance —
    // and both then hang a label off the same rectangle, so this returns the
    // rectangle rather than drawing it.
    //
    // ScaleUnit is the HUD's S(1.0f) passed in, because S() is private and
    // these are free functions. S(x) is x * UIScale and ScaleUnit IS UIScale,
    // so x * ScaleUnit is the same product of the same two floats.
    struct FBreakerEnemyBarRect
    {
        float X = 0.0f;
        float Y = 0.0f;
        float W = 0.0f;
        float H = 0.0f;
        // The distance scale itself, exposed because the label under the bar
        // and the elite edge on it both shrink on the same curve.
        float Scale = 1.0f;
    };

    FBreakerEnemyBarRect BreakerEnemyBarPlace(const FVector& Projected, float Distance, float ScaleUnit)
    {
        // Gentle distance scaling: readable up close, unobtrusive far away.
        const float DistanceAlpha = FMath::Clamp(
            (Distance - 500.0f) / (BreakerEnemyBar::MaxDistance - 500.0f), 0.0f, 1.0f);

        FBreakerEnemyBarRect Rect;
        Rect.Scale = FMath::Lerp(1.0f, 0.55f, DistanceAlpha);
        Rect.W = BreakerUI::HudEnemyBarWidth * ScaleUnit * Rect.Scale;
        Rect.H = BreakerUI::HudEnemyBarHeight * ScaleUnit * Rect.Scale;
        Rect.X = Projected.X - Rect.W * 0.5f;
        Rect.Y = Projected.Y;
        return Rect;
    }

    // ONE bar body, drawn by both loops below. It was two: the enemy loop and
    // the dummy loop each carried the fill line and the seven-line shield
    // block near-verbatim. A segmented bar is coming, and authored into only
    // one of these, the gym — the copy the owner actually plays — would have
    // been the one surface where the new bar was invisible.
    //
    // Alpha is the only axis the two ever differed on: the enemy loop fades a
    // trash bar out, the dummy loop never fades. BreakerUI::Alpha SETS the
    // alpha channel rather than scaling it, and every colour here arrives from
    // Hex() at A=1, so Alpha(C, 1.0f) is exactly C — the dummy's appearance is
    // unchanged by routing it through the fading path.
    void BreakerEnemyBarDrawBody(AHUD& HUD, const FBreakerEnemyBarRect& Bar,
        float Health, float MaxHealth, float Shield, float MaxShield, float ScaleUnit, float BarAlpha)
    {
        if (Shield > 0.0f && MaxShield > UE_SMALL_NUMBER)
        {
            const float ShieldH = FMath::Max(Bar.H * 0.45f, 2.0f * ScaleUnit);
            const float ShieldY = Bar.Y - ShieldH - ScaleUnit;
            HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Panel10, BarAlpha), Bar.X, ShieldY, Bar.W, ShieldH);
            HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Cyan, BarAlpha), Bar.X, ShieldY,
                Bar.W * FMath::Clamp(Shield / MaxShield, 0.0f, 1.0f), ShieldH);
        }

        HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Panel10, BarAlpha), Bar.X, Bar.Y, Bar.W, Bar.H);
        HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Harm, BarAlpha), Bar.X, Bar.Y,
            Bar.W * FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f), Bar.H);
    }
}

// --------------------------------------------------------------------------
// §Anchors — enemy bars 180x8 with the rank word at 11px beneath. Above-Trash
// ranks are barred whenever they are inside MaxDistance; trash is barred only
// while it is under the crosshair, and fades for FocusFadeSeconds after the
// aim leaves. See the selective-bar block below for why that is the rule and
// not a recency window.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawEnemyHealthBars(const ABreakerCharacter* Character)
{
    // ORDERING CONTRACT: this runs before DrawMinimap, and it is the ONE enemy
    // iteration the HUD makes. Reset (not Empty) keeps the capacity, so the
    // array stops allocating after the first busy frame — DrawHUD runs every
    // frame and a container built inside it is a per-frame allocation.
    EnemyBlips.Reset();

    UWorld* World = GetWorld();
    if (!World || !Character) return;
    const FVector ViewerLocation = Character->GetActorLocation();
    // S() is private and the geometry helpers above are free functions, so the
    // scale crosses that boundary as a value. Resolved once per frame.
    const float ScaleUnit = S(1.0f);

    // Which enemy the player is actually asking about. This is what decides
    // whether a trash mob is barred at all, and it is the only enemy that gets
    // the verbose state line; see the label block below for that split. Same
    // aim-cone shape DrawLootPickups already uses to pick its focused pickup,
    // so "what am I pointing at" means one thing across the whole HUD.
    const ABreakerEnemy* FocusedEnemy = nullptr;
    if (PlayerOwner && PlayerOwner->PlayerCameraManager)
    {
        const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
        const FVector CameraForward = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
        float BestDot = BreakerEnemyBar::FocusMinimumDot;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            const ABreakerEnemy* Candidate = *It;
            if (!Candidate || Candidate->IsDeadEnemy()) continue;
            const FVector ToEnemy = (Candidate->GetActorLocation() - CameraLocation);
            if (ToEnemy.IsNearlyZero()) continue;
            const float Dot = FVector::DotProduct(CameraForward, ToEnemy.GetSafeNormal());
            if (Dot > BestDot)
            {
                BestDot = Dot;
                FocusedEnemy = Candidate;
            }
        }
    }

    // Reset per frame: these are screen-space rectangles, and last frame's are
    // meaningless the moment the camera moves.
    DrawnLabelBounds.Reset();

    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        const ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;

        // Collected BEFORE the health-bar culls, because the two readouts want
        // different ranges: a bar is pointless past 50 m, and a minimap is
        // mostly useful for the hostiles that are further away than that.
        if (!Enemy->IsDeadEnemy())
        {
            FBreakerHUDMapBlip& Blip = EnemyBlips.AddDefaulted_GetRef();
            Blip.World = Enemy->GetActorLocation();
            // The same rank-predicate shape as the bar fix beside it: == Elite
            // exactly blipped a ModifierBearing champion as TRASH. It blips as
            // an elite; Boss keeps its own mark.
            Blip.bElite = Enemy->GetMonsterRank() == EBreakerMonsterRank::Elite
                || Enemy->GetMonsterRank() == EBreakerMonsterRank::ModifierBearing;
            Blip.bBoss = Enemy->GetMonsterRank() == EBreakerMonsterRank::Boss;
        }

        const float Distance = FVector::Distance(ViewerLocation, Enemy->GetActorLocation());
        if (Distance > BreakerEnemyBar::MaxDistance) continue;

        const UAbilitySystemComponent* EnemyAbilitySystem = Enemy->GetAbilitySystemComponent();
        const UBreakerAttributeSet* EnemyAttributes = EnemyAbilitySystem ? EnemyAbilitySystem->GetSet<UBreakerAttributeSet>() : nullptr;
        if (!EnemyAttributes) continue;

        const float Health = EnemyAttributes->GetHealth();
        const float MaxHealth = EnemyAttributes->GetMaxHealth();
        if (Health <= 0.0f || MaxHealth <= UE_SMALL_NUMBER) continue;

        // SELECTIVE BARS (ruled for the crowd): in a fight of eighty, eighty
        // bars is a rendering cost AND the noise that hides the read the bars
        // exist for. The rules, built now with the visual to swap in later:
        //  * ABOVE TRASH — always barred inside MaxDistance. Elites and
        //    champions are the fight's anchors; their health is standing
        //    information. IsEliteOrBetter, NEVER IsElite: rank == Elite
        //    exactly would exclude ModifierBearing and Boss — the two ranks
        //    ABOVE the one meant — and the project has shipped that predicate
        //    bug twice (the enemy header records both).
        //  * TRASH — barred only while AIMED AT, fading for FocusFadeSeconds
        //    after the aim leaves so glancing across a pack reads as a sweep,
        //    not a strobe. An unfocused trash hit shows no bar: the damage
        //    numbers already carry "it hurt", and a recency rule at ANY window
        //    would light the whole pack the moment a cleave landed.
        //
        // What this rule deliberately does not carry: the trash mob at 8%
        // health in a pack of eighty, which is the highest-value information
        // on screen and is invisible here unless the player happens to be
        // crosshaired on it. That read lives on the BODY — the tint ramp and
        // the fracture mask — not on the bar. Focus-only is correct here
        // BECAUSE the body carries near-death; the two are halves of one rule.
        const bool bAboveTrash = Enemy->IsEliteOrBetter();
        float BarAlpha = 1.0f;
        if (!bAboveTrash)
        {
            const double Now = World->GetTimeSeconds();
            if (Enemy == FocusedEnemy)
            {
                LastFocusBarEnemy = Enemy;
                LastFocusBarTime = Now;
            }
            else if (Enemy == LastFocusBarEnemy.Get()
                && Now - LastFocusBarTime < BreakerEnemyBar::FocusFadeSeconds)
            {
                BarAlpha = 1.0f - static_cast<float>((Now - LastFocusBarTime) / BreakerEnemyBar::FocusFadeSeconds);
            }
            else
            {
                continue;
            }
        }

        const FVector Projected = Project(Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), false);
        if (Projected.Z <= 0.0f) continue;

        // The gold edge and the rank word follow the SAME predicate as the
        // permanent bar. ModifierBearing prints ELITE — it is, by the enemy
        // header's own definition, a modifier-bearing elite, and its modifier
        // banner (always printed below) is what distinguishes it further.
        const bool bElite = Enemy->IsEliteOrBetter();
        const bool bBossRank = Enemy->GetMonsterRank() == EBreakerMonsterRank::Boss;

        const FBreakerEnemyBarRect Bar = BreakerEnemyBarPlace(Projected, Distance, ScaleUnit);
        BreakerEnemyBarDrawBody(*this, Bar, Health, MaxHealth,
            EnemyAttributes->GetShield(), EnemyAttributes->GetMaxShield(), ScaleUnit, BarAlpha);
        if (bElite)
        {
            // Gold edge, not a gold fill: the health colour must stay readable.
            DrawBorder(Bar.X, Bar.Y, Bar.W, Bar.H, BreakerUI::Alpha(BreakerUI::Gold, BarAlpha), ScaleUnit * Bar.Scale);
        }

        // ---- The label, and how much of it -----------------------------
        // Owner: "theres a lot of text bloat on enemies". Every enemy printed
        // ELITE/HOSTILE plus the whole of GetEnemyStateLabel(), which is up to
        // THREE stacked lines (family banner, modifier banner, state) — and
        // nothing checked whether two enemies' labels landed on the same
        // pixels. Six enemies in a pocket produced the overlapping mush in the
        // report: "WARDED | VOLATILE" printed through "HOSTILE · WIND-UP"
        // printed through "CHASE".
        //
        // What survives, and why that split:
        //  - The MODIFIER banner is load-bearing and always prints.
        //    Encounter-Design §1.2's first acceptance test is that a modifier
        //    is identifiable within 1.5s of the enemy entering view, and an
        //    unannounced modifier is an unfair death rather than a challenge.
        //    Culling it to declutter would trade legibility for legibility.
        //  - The STATE line (CHASE / CLOSING / WIND-UP) prints only for the
        //    enemy under the crosshair. It was the loudest line and the least
        //    informative: the enemy's own telegraph already shows a wind-up as
        //    a scaling, brightening emitter, so the text was restating in
        //    words, six times over, something the world was already saying.
        //  - ELITE prints; HOSTILE does not. "HOSTILE" on every hostile is not
        //    information — the health bar already says it is an enemy.
        const bool bFocused = (Enemy == FocusedEnemy);
        const FString ModifierBanner = Enemy->GetEnemyModifierBanner();
        TArray<FString> Lines;
        if (bElite) Lines.Add(bBossRank ? TEXT("BOSS") : TEXT("ELITE"));
        if (!ModifierBanner.IsEmpty()) Lines.Add(ModifierBanner);
        if (bFocused) Lines.Add(Enemy->GetEnemyStateLabel());

        if (Lines.Num() > 0)
        {
            const FString Label = FString::Join(Lines, TEXT("\n"));
            const float LabelY = Bar.Y + Bar.H + S(3.0f);
            // Screen-space overlap suppression. Two enemies standing in line
            // with the camera project to nearly the same point, and the second
            // label lands on top of the first — unreadable, and worse than
            // showing one. The focused enemy is drawn regardless, because it is
            // the one the player is deliberately asking about.
            const float LineCount = static_cast<float>(Lines.Num());
            const float LabelH = S(13.0f) * Bar.Scale * LineCount;
            const float LabelW = Bar.W;
            bool bOccluded = false;
            if (!bFocused)
            {
                for (const FVector4& Taken : DrawnLabelBounds)
                {
                    if (FMath::Abs(Projected.X - Taken.X) < (LabelW + Taken.Z) * 0.5f
                        && FMath::Abs(LabelY - Taken.Y) < (LabelH + Taken.W) * 0.5f)
                    {
                        bOccluded = true;
                        break;
                    }
                }
            }
            if (!bOccluded)
            {
                DrawnLabelBounds.Emplace(Projected.X, LabelY, LabelW, LabelH);
                DrawSpecTextCentered(Label, Projected.X, LabelY,
                    bElite ? BreakerUI::Gold : BreakerUI::TextMuted, 11.0f * Bar.Scale);
            }
        }
    }

    // --- Target dummies (ruled with the reaction extraction) ----------------
    // A looter shooter where you cannot tell whether you are doing damage has
    // no feedback loop, and the gym's own targets had no readout at all. The
    // CORE of the enemy bar — shield pip, health fill, distance scaling — over
    // every live dummy, plus its profile label; no blips, no elite border, no
    // focus line: those are enemy facts.
    //
    // The visibility rule here is the RECENCY window, deliberately, and the
    // enemy path above going aimed-at-only does not take it with it. See
    // BreakerEnemyBar::RecentDamageSeconds for the ruling.
    for (TActorIterator<ABreakerTargetDummy> It(World); It; ++It)
    {
        const ABreakerTargetDummy* Dummy = *It;
        if (!Dummy) continue;
        const float Distance = FVector::Distance(ViewerLocation, Dummy->GetActorLocation());
        if (Distance > BreakerEnemyBar::MaxDistance) continue;
        const UAbilitySystemComponent* DummyAbilitySystem = Dummy->GetAbilitySystemComponent();
        const UBreakerAttributeSet* DummyAttributes = DummyAbilitySystem ? DummyAbilitySystem->GetSet<UBreakerAttributeSet>() : nullptr;
        if (!DummyAttributes) continue;
        const float Health = DummyAttributes->GetHealth();
        const float MaxHealth = DummyAttributes->GetMaxHealth();
        if (Health <= 0.0f || MaxHealth <= UE_SMALL_NUMBER) continue;
        const UBreakerCombatComponent* DummyCombat = Dummy->FindComponentByClass<UBreakerCombatComponent>();
        const bool bRecentlyDamaged = DummyCombat && DummyCombat->GetSecondsSinceDamage() < BreakerEnemyBar::RecentDamageSeconds;
        if (!bRecentlyDamaged && Distance > BreakerEnemyBar::AlwaysDistance) continue;
        const FVector Projected = Project(Dummy->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), false);
        if (Projected.Z <= 0.0f) continue;

        const FBreakerEnemyBarRect Bar = BreakerEnemyBarPlace(Projected, Distance, ScaleUnit);
        // Full opacity: a dummy never fades, and Alpha(C, 1.0f) is exactly C.
        BreakerEnemyBarDrawBody(*this, Bar, Health, MaxHealth,
            DummyAttributes->GetShield(), DummyAttributes->GetMaxShield(), ScaleUnit, 1.0f);
        DrawSpecTextCentered(Dummy->GetProfileLabel(), Projected.X, Bar.Y + Bar.H + S(3.0f),
            BreakerUI::TextMuted, 10.0f * Bar.Scale);
    }
}
