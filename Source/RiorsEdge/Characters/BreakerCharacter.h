#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Characters/BreakerViewmodelRig.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "BreakerCharacter.generated.h"

class UAbilitySystemComponent;
class UBreakerAttributeSet;
class UBreakerInputConfig;
class UInputMappingContext;
class UCameraComponent;
class UBreakerCharacterMovementComponent;
class UBreakerProgressionComponent;
class UBreakerCombatComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UBreakerPlaytestComponent;
class UBreakerEquipmentComponent;
class UBreakerMomentumComponent;
class UBreakerManaComponent;
class UBreakerScrapComponent;
class UBreakerGritComponent;
class UBreakerChargeComponent;
class UBreakerAbilityComponent;
class UBreakerQuestJournal;
class ABreakerNPC;
class ABreakerLootPickup;
class SBreakerMenu;
struct FInputActionValue;
enum class EBreakerLedgeVerb : uint8;

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    ABreakerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void Landed(const FHitResult& Hit) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const;
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const;
    // Forwards to the movement component's IsTraversingLedge — the execution
    // moved into the custom movement mode, so the pawn holds no traversal
    // state of its own any more. Kept under its old name for Blueprint/HUD
    // callers.
    UFUNCTION(BlueprintPure, Category="Movement") bool IsMantling() const;
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const;
    UFUNCTION(BlueprintPure, Category="Movement") UBreakerCharacterMovementComponent* GetBreakerMovement() const;
    UFUNCTION(BlueprintPure, Category="Combat") UBreakerAttributeSet* GetAttributes() const { return Attributes; }
    UFUNCTION(BlueprintPure, Category="Combat") UBreakerCombatComponent* GetCombat() const { return Combat; }
    UFUNCTION(BlueprintPure, Category="Weapon") UBreakerWeaponComponent* GetWeapon() const { return Weapon; }
    UFUNCTION(BlueprintPure, Category="Equipment") UBreakerEquipmentComponent* GetEquipment() const { return Equipment; }
    UFUNCTION(BlueprintPure, Category="Progression") UBreakerProgressionComponent* GetProgression() const { return Progression; }
    UFUNCTION(BlueprintPure, Category="Momentum") UBreakerMomentumComponent* GetMomentum() const { return Momentum; }
    UFUNCTION(BlueprintPure, Category="Mana") UBreakerManaComponent* GetMana() const { return Mana; }
    UFUNCTION(BlueprintPure, Category="Scrap") UBreakerScrapComponent* GetScrap() const { return Scrap; }
    UFUNCTION(BlueprintPure, Category="Grit") UBreakerGritComponent* GetGrit() const { return Grit; }
    UFUNCTION(BlueprintPure, Category="Charge") UBreakerChargeComponent* GetCharge() const { return Charge; }
    UFUNCTION(BlueprintPure, Category="Abilities") UBreakerAbilityComponent* GetAbilities() const { return Abilities; }
    UFUNCTION(BlueprintCallable, Category="Save") void SaveGameState();
    UFUNCTION(BlueprintCallable, Category="Save") void LoadGameState();
    // Which character this pawn is. Invalid means the pre-roster global
    // slot, which is what a capture run or a PIE drop-in still uses.
    UFUNCTION(BlueprintCallable, Category="Save") void EnterWorldAsCharacter(const FGuid& CharacterId);
    FString ActiveSaveSlotName() const;
    // Reads the active character back off the GameInstance after a level
    // load, because the pawn that knew it was destroyed by the load.
    void AdoptSessionCharacter();
    FGuid ActiveCharacterId;
    // Armed by EnterWorldAsCharacter the moment this pawn's identity is
    // re-pointed at a character whose state it does NOT hold. From then until
    // the level load destroys it, every SaveGameState on this pawn is refused:
    // EndPlay's travel save used to run AFTER the id changed, so the FRONT-END
    // pawn's fresh state (auto-locked Swift, slice points, empty journal) was
    // written over the roster's just-created save — "i made a caster and had
    // swifts skill tree and abilities" was this one ordering.
    bool bRefuseSavesForPendingCharacter = false;
    // Interaction + quest-state groundwork: F talks to the nearest NPC in
    // range; dialogue choices set persistent quest flags.
    UFUNCTION(BlueprintPure, Category="Interaction") ABreakerNPC* FindNearbyNPC() const;
    // The hub gate. Shares the F key with NPCs and loot; see
    // InteractWithNearbyNPC for the precedence and the reason for it.
    UFUNCTION(BlueprintPure, Category="Interaction") class ABreakerTravelPoint* FindNearbyTravelPoint() const;
    // Ground loot: the nearest pickup within its interaction range, or null.
    // F prefers a pickup over NPC dialogue when both are in range — picking
    // items up is by far the more frequent action.
    UFUNCTION(BlueprintPure, Category="Interaction") ABreakerLootPickup* FindNearbyPickup() const;
    UFUNCTION(BlueprintCallable, Category="Interaction") void AddQuestFlag(FName Flag);
    UFUNCTION(BlueprintPure, Category="Interaction") bool HasQuestFlag(FName Flag) const;
    UFUNCTION(BlueprintPure, Category="Interaction") const TArray<FName>& GetQuestFlags() const;
    void SetQuestFlags(const TArray<FName>& NewFlags);
    // The journal owns quest state; the character owns the save slot. Dialogue
    // conditions, quest state derivation and the kill tracker all read through
    // this rather than through a bare array on the pawn.
    UFUNCTION(BlueprintPure, Category="Interaction") UBreakerQuestJournal* GetQuestJournal() const { return Quests; }
    UFUNCTION(BlueprintPure, Category="Playtest") UBreakerPlaytestComponent* GetPlaytest() const { return Playtest; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetLookSensitivity() const { return LookSensitivity; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetCurrentFOV() const;
    // 0 at rest, 1 at the peak of the dash camera punch, decaying back to 0.
    // Exposed so presentation that is NOT owned here — a HUD speed-line burst,
    // a Blueprint effect, a rumble curve — can ride the same envelope instead
    // of inventing its own timer and drifting out of sync with the camera.
    UFUNCTION(BlueprintPure, Category="Movement") float GetDashFeedbackAlpha() const;
    // Direction the last dash committed to, in world space, horizontal.
    UFUNCTION(BlueprintPure, Category="Movement") FVector GetLastDashDirection() const { return LastDashDirection; }
    UFUNCTION(BlueprintPure, Category="UI") bool IsLookInverted() const { return bInvertLookY; }
    UFUNCTION(BlueprintPure, Category="UI") bool IsMenuOpen() const { return MenuWidget.IsValid(); }
    void ApplyMenuSettings(float NewSensitivity, float NewFOV, bool bNewInvertLookY);
    void ResumeFromMenu();
    void ReturnToTitleMenu();
    void QuitFromMenu();
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryDash();
    UFUNCTION(BlueprintCallable, Category="Movement") void StartSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") void StopSlide();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities") TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Progression") TObjectPtr<UBreakerProgressionComponent> Progression;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UBreakerWeaponComponent> Weapon;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment") TObjectPtr<UBreakerEquipmentComponent> Equipment;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Momentum") TObjectPtr<UBreakerMomentumComponent> Momentum;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mana") TObjectPtr<UBreakerManaComponent> Mana;
    // The three remaining class-resource loops (owner authorization
    // 2026-08-16). Attached unconditionally like Momentum and Mana: each loop
    // gates itself internally on the owner's permanent class, so a Swift
    // carries an inert Scrap component exactly as it carries an inert Mana one.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scrap") TObjectPtr<UBreakerScrapComponent> Scrap;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grit") TObjectPtr<UBreakerGritComponent> Grit;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Charge") TObjectPtr<UBreakerChargeComponent> Charge;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities") TObjectPtr<UBreakerAbilityComponent> Abilities;
    // THE RIG ROOT. Historically this was the weapon body itself, a single grey
    // cube; it is now an EMPTY transform that every proxy part and both arms
    // hang from. Keeping the name and the type is deliberate: it is the
    // component the viewmodel spring writes to (UpdateViewmodelKick) and the
    // one BP_BreakerCharacter inherits, and renaming it would silently drop the
    // Blueprint's override without a compile error. It carries no mesh, so the
    // recoil, ADS and dash work all move the whole assembly at once.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> PrototypeWeaponVisual;
    // Pool slots 0 and 1, kept under their original names for the same
    // Blueprint-inheritance reason. They no longer mean "barrel" and "sight" —
    // BreakerViewmodelRig's layout table decides what every slot is.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> PrototypeWeaponBarrel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> PrototypeWeaponSight;
    // The recycled primitive pool. Allocated once in the constructor and never
    // grown: switching weapons re-poses existing components and hides the
    // remainder, so a swap costs no allocation. Same discipline as the pooled
    // tracer renderer.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TArray<TObjectPtr<UStaticMeshComponent>> ViewmodelParts;
    // The intake gun, when the layout names one: replaces the proxy PARTS
    // whole and rides the same recoil-driven rig transform. Hidden whenever
    // the layout's NamedMeshPath is unset or fails to load — the primitives
    // are the fallback, ABreakerNPC's floor-still-works shape.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> NamedWeaponVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UPointLightComponent> PrototypeMuzzleFlash;
    // Forearms and gloves. These hang off the RIG, not off the camera: the
    // hands hold the gun, so they must ride the recoil spring with it. Before
    // this pass both arms were camera-parented at transforms that projected to
    // y=1283 and y=1384 on a 1080-tall frame — entirely below the viewport.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> LeftArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> RightArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> LeftGloveVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> RightGloveVisual;

    // --- Viewmodel blockout tuning (O2) ---------------------------------
    // The default layout table lives in BreakerViewmodelRig.cpp; this map
    // replaces any archetype's row on the instance with no recompile, which is
    // the same escape hatch UBreakerWeaponComponent::RecoilOverrides gives the
    // feel layer. O2 PLACEHOLDER: every value in the default table is frozen
    // and unplaytested.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Viewmodel")
    TMap<EBreakerWeaponArchetype, FBreakerViewmodelLayout> ViewmodelLayoutOverrides;
    // Uniform scale on the whole rig. The one knob that answers "the gun is too
    // big / too small in frame" without touching a single proportion.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Viewmodel", meta=(ClampMin="0.2", ClampMax="3.0"))
    float ViewmodelScale = 0.9f;  // O2 PLACEHOLDER
    // Where the arms enter frame, in CAMERA space. Behind and below the camera
    // on purpose: the near plane clips the shoulder end, so the limbs read as
    // coming from the player's body rather than as floating sticks.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Viewmodel")
    FVector SupportShoulderAnchorCm = FVector(30.0f, -22.0f, -29.0f);  // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Viewmodel")
    FVector FiringShoulderAnchorCm = FVector(10.0f, 19.0f, -33.0f);    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Viewmodel", meta=(ClampMin="1"))
    float ForearmWidthCm = 6.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation|Viewmodel", meta=(ClampMin="1"))
    float GloveSizeCm = 9.5f;      // O2 PLACEHOLDER
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Playtest") TObjectPtr<UBreakerPlaytestComponent> Playtest;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UBreakerInputConfig> InputConfig;
    // The mapping context this pawn actually registered: InputConfig's
    // authored default, or a transient clone of it with the player's keybind
    // overrides applied (R10 — the settings screen's rebinds are live, not
    // cosmetic). UPROPERTY so GC keeps the clone alive while registered;
    // outered to the pawn so it dies with it.
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> ActiveMappingContext;
    // Registers the right mapping context for Overrides: removes whatever
    // this pawn registered before, then adds either a rebuilt clone or the
    // untouched default. Called from BeginPlay with the overrides loaded off
    // the profile ini, and again — via UBreakerGameSettings::
    // OnKeybindOverridesChanged — every time the settings screen rebinds,
    // clears or resets a key mid-session.
    void ApplyKeybindOverrides(const TMap<FName, FKey>& Overrides);
    // The mantle parameters that lived here — reach, height window, duration
    // — moved to UBreakerCharacterMovementComponent (Part One-R), where the
    // ceiling is the PUBLISHED MantleStepHeightCm rather than a hand-copied
    // 150 disagreeing with the grammar's 145 by five centimetres.

    // --- Dash camera feedback -------------------------------------------
    // Owner report: the dash's speed change is correct but unreadable. In a
    // first-person view over open ground a pure velocity change has almost no
    // optical flow to read, so the fix is a camera cue, not a movement change:
    // no dash value is touched.
    // Two cues, because they answer different questions. A short FOV punch that
    // recovers is the genre-standard "you just got fast" signal (it widens the
    // frustum, which multiplies the peripheral motion the eye actually uses to
    // judge speed) and it answers HOW MUCH. A brief camera roll answers WHICH
    // WAY: a pure forward dash rolls not at all and a side dash rolls fully,
    // which is exactly the information FOV alone cannot carry.
    // Both obey the Movement-Design guardrail that camera roll and FOV changes
    // stay subtle and configurable — set either amplitude to 0, or clear
    // bDashCameraFeedback, to remove it without a rebuild.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback") bool bDashCameraFeedback = true;
    // Peak FOV added on top of the player's own setting, in degrees. The base
    // FOV is never overwritten, so this can never leak into saved settings.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback", meta=(ClampMin="0", ClampMax="40")) float DashFOVPunch = 12.0f;
    // Attack is deliberately near-instant: the punch has to arrive with the
    // velocity change or it reads as a separate event.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback", meta=(ClampMin="0.01")) float DashFOVPunchAttack = 0.05f;
    // Recovery is long enough to be felt as a settle rather than a snap, and
    // short enough to be finished well inside the 4 s dash cooldown.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback", meta=(ClampMin="0.01")) float DashFOVPunchRecovery = 0.30f;
    // Peak roll, degrees, applied about the view axis and signed by how lateral
    // the dash was. Kept small: roll is a readability cue, not a stunt.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback", meta=(ClampMin="0", ClampMax="20")) float DashCameraRoll = 5.0f;
    // Dash speed that maps to the full punch. Faster dashes (gear, momentum
    // carried in) punch proportionally harder, so the cue tracks the thing the
    // owner could not see rather than being a fixed stamp.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback", meta=(ClampMin="1")) float DashFeedbackReferenceSpeed = 1700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback", meta=(ClampMin="0.1", ClampMax="2")) float DashFeedbackMinimumScale = 0.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Dash Feedback", meta=(ClampMin="1", ClampMax="3")) float DashFeedbackMaximumScale = 1.35f;

    // --- ADS camera (D5, owner-ruled) -------------------------------------
    // A small FOV push toward the sights, riding the SAME eased aim blend as
    // every other ADS quantity — no second timer, no second curve. Small on
    // purpose: it is a commitment cue, not a scope; per-archetype zoom would
    // be a different feature and is deliberately not authored here. Composed
    // with the dash punch over the player's own base FOV; 0 removes it
    // without a rebuild. Holstered pawns never push. O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Aim", meta=(ClampMin="0", ClampMax="30")) float AimFOVNarrowDegrees = 6.0f;

    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnFireInput(bool bPressed);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnAimInput(bool bPressed);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnReloadInput();
    UFUNCTION(BlueprintImplementableEvent, Category="Movement") void OnDashPerformed();
    UFUNCTION(BlueprintImplementableEvent, Category="Movement") void OnSlideChanged(bool bNowSliding);

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void MoveForwardLegacy(float Value);
    void MoveRightLegacy(float Value);
    void TurnLegacy(float Value);
    void LookUpLegacy(float Value);
    void StartSprint();
    void HandleDashInput();
    void HandleJumpInput();
    bool TryMantle();
    void StartFire();
    void StopFire();
    void StartAim();
    void StopAim();
    void HandleReloadInput();
    void EquipPrimaryWeapon();
    void EquipSecondaryWeapon();
    void ApplyWeaponPresentation();
    // The layout for an archetype: the instance override if one exists, the
    // shared default table otherwise.
    FBreakerViewmodelLayout ResolveViewmodelLayout(EBreakerWeaponArchetype Archetype) const;
    // Poses every pooled primitive and both arms from the active layout. Called
    // only when the archetype actually changes, never per frame.
    void RebuildViewmodelParts();
    // Dev capture only (-BreakerCycleWeapons=<seconds>): walks the equipped
    // archetype through the enum so a screenshot run can photograph all eight.
    void StartViewmodelCaptureCycle();
    void PoseArm(UStaticMeshComponent* Forearm, UStaticMeshComponent* Glove,
        const FVector& AnchorCm, const FVector& HandRigCm);
    // Rest pose of the viewmodel rig, before the weapon component's per-shot
    // kick offset is added on top of it in Tick. The ADS pose is DERIVED from
    // the layout's sight height rather than authored, so every archetype puts
    // its own sight on the crosshair.
    FVector GetWeaponRestLocation() const;
    void UpdateViewmodelKick();

    // ---- Viewmodel motion channel (sway / bob / landing dip) --------------
    // Tuning lives in the world-free params struct; the character only owns
    // the two pieces of state a pure function cannot: the distance-driven bob
    // phase, and the last airborne fall speed (Landed fires after the
    // movement component has already zeroed the vertical velocity, so the dip
    // reads the cached value from the final falling frame).
    FBreakerViewmodelMotionParams ViewmodelMotion;
    float ViewmodelBobPhase = 0.0f;
    float LastFallingSpeed = 0.0f;
    // The traversal exit dip (D3): bound to the movement component's
    // completed-only broadcast, pays the verb's authored kick into the
    // existing spring — the 3 cm exit drop is invisible to the landing path
    // (zero vertical speed), so the completion event is the honest trigger.
    void HandleLedgeTraversalCompleted(EBreakerLedgeVerb Verb);
    // Dev capture only (-BreakerTraversalDemo[=scale]): spawns a mantle-height
    // block ahead, slows the traversal clock and runs the verb, so a
    // screenshot cadence of 2 s can photograph the mid-traversal viewmodel —
    // a real vault crosses in 0.12 s and no frame would ever catch it.
    bool bTraversalDemoArmed = false;
    float TraversalDemoClockScale = 25.0f;
    double TraversalDemoStartTime = -1.0;
    int32 TraversalDemoStep = 0;
    void TickTraversalDemo();
    UFUNCTION() void HandleDashStarted(FVector DashDirection, float DashSpeed);
    void UpdateDashCameraFeedback(float DeltaSeconds);
    // The ONE writer of the camera's live FOV during play (D5): composes the
    // player's base FOV, the dash punch and the ADS narrow every frame, so
    // two effects can never fight over SetFieldOfView. Writes only while an
    // offset is live (or on the frame it dies), so an idle camera costs one
    // comparison.
    void UpdateCameraFieldOfView();
    bool bCameraFOVOffsetApplied = false;
    void ApplyBaseFieldOfView();
    void ResetPlaytest();
    void CopyPlaytestReport();
    void TogglePlaytestDiagnostics();
    void IncreaseFOV();
    void DecreaseFOV();
    void IncreaseSensitivity();
    void DecreaseSensitivity();
    void SavePlaytestSettings() const;
    void TogglePauseMenu();
    // Enter/Space while a menu is open. Bound with bExecuteWhenPaused,
    // because the menu is only ever open while the game is paused.
    void ConfirmMenuKey();
    // Forwards any key to the settings screen while a keybind row is
    // listening. Inert otherwise - see SBreakerMenu::HandleRebindKey.
    void MenuRebindKey(FKey Key);
    void ToggleInventoryMenu();
    void ToggleCharacterSheet();
    void InteractWithNearbyNPC();
    UFUNCTION(Server, Reliable) void ServerPickupLoot(ABreakerLootPickup* Pickup);
    void StartWave();
    void ActivateAbilityOne();
    void ActivateAbilityTwo();
    void ActivateUltimate();
    void ShowInitialMenu();
public:
    // THE D1 GUARD, extracted pure so the shipped decision is testable
    // without a world (RiorsEdge.Game.BootFlow). The title menu belongs to
    // sessions that have not entered the world yet: the front end always
    // shows it; any other map shows it only when the session carries no
    // active character (a PIE drop-in on the template map). A mid-session
    // map arrival — the pawn OpenLevel just rebuilt — shows nothing.
    static bool ShouldShowInitialMenu(bool bIsFrontEndMap, bool bSessionHasActiveCharacter)
    {
        return bIsFrontEndMap || !bSessionHasActiveCharacter;
    }
    // The EnterWorldAsCharacter dispatch, pinned the same way. Entering a
    // character TRAVELS (a level load into the Anchor, landing at the hub
    // gate) from the front end and from any mid-session switch — the old
    // in-place branch loaded the new character onto the old character's pawn,
    // at the old character's location. The only in-place entry left is a PIE
    // drop-in on a template map: no front end, no prior character.
    static bool ShouldTravelOnEnterWorld(bool bIsFrontEndMap, bool bHadMidSessionCharacter)
    {
        return bIsFrontEndMap || bHadMidSessionCharacter;
    }
    // True in the Anchor (a social space): fire and abilities are refused and
    // the viewmodel presents lowered. Set once in BeginPlay from the map role
    // — a pawn never outlives a map, so it cannot go stale.
    bool IsWeaponsHolstered() const { return bWeaponsHolstered; }
private:
    bool bWeaponsHolstered = false;
    void OpenMenu(bool bInitialMenu);
    // Dev capture only (-BreakerCaptureMenu=<name>): opens the front end on a
    // named screen so a screenshot run can photograph it.
    void OpenMenuScreenForCapture(const FString& ScreenName);
    UFUNCTION() void HandleShotCosmetics(const FBreakerShotResult& Shot);
    UFUNCTION() void HandlePlayerDeath();
    // O82 (amended): the campaign respawn. After the death beat, back to the
    // tileset start with vitals and ammunition restored — the WORLD is left
    // standing (F1 remains the dev reset that rebuilds it), except a live
    // boss encounter, which resets whole per the ruling.
    void RespawnAtTilesetStart();
    // --- Class-resource event wiring (the T7 step-2 call sites) ------------
    // Each handler fans one real combat/weapon event out to whichever resource
    // loop is live for this character's class; the loops themselves stay
    // inert for non-owners, so the fan-out needs no class branches here.
    UFUNCTION() void HandleClassResourceKill(const FBreakerHitContext& Hit);
    UFUNCTION() void HandleClassResourceHitDealt(const FBreakerHitContext& Hit);
    UFUNCTION() void HandleClassResourceDamageTaken(const FBreakerHitContext& Hit);
    UFUNCTION() void HandleClassResourceHealingDealt(const FBreakerHealContext& Heal);
    UFUNCTION() void HandleClassResourceReloadCompleted(bool bAnyRoundFired);
    UFUNCTION() void HandleClassResourceMagazineEmptied(bool bStartedFull);
    // Derives the discrete combat/proximity state the Grit and Charge loops
    // read (in-combat, enemy-within-5m), on a coarse poll rather than a
    // per-frame scan. Runs only while one of those loops is live.
    void UpdateClassResourceStates();
    // The campaign's only non-dialogue flag source today: a kill advances the
    // counted objectives of whichever quest is active. Bound to the combat
    // component's attacker-side OnKillDealt.
    UFUNCTION() void HandleQuestKill(const FBreakerHitContext& Hit);
    // Pays a quest out exactly once. Flags are monotonic and the journal
    // broadcasts a flag only on the transition, so "exactly once" is a
    // property of the flag rather than a bookkeeping field that can drift.
    void GrantQuestRewardForFlag(FName Flag);
    void EndShotCosmetics();

    FTransform PlaytestSpawnTransform;
    float FallKillZ = -100000.0f;
    // THE CHARACTER BODY (ruled): the first skeletal mesh the game has ever
    // touched. ACharacter's inherited Mesh gets the template mannequin, the
    // unarmed locomotion ABP, and the three ruled montages — fire, reload,
    // hit react — playing over it. Loaded at runtime with the audio/NPC
    // fallback shape: a clone without the template folders keeps the
    // meshless pawn it always had. The proxy viewmodel is deliberately
    // untouched until this body proves out.
    void ApplyCharacterBody();
    // Montage-or-sequence tolerant: the template ships both kinds under the
    // MM_ prefix, and PlayBodyAction plays either.
    void PlayBodyAction(class UAnimSequenceBase* Action);
    UPROPERTY(Transient) TObjectPtr<class UAnimSequenceBase> FireMontage;
    UPROPERTY(Transient) TObjectPtr<class UAnimSequenceBase> ReloadMontage;
    UPROPERTY(Transient) TObjectPtr<class UAnimSequenceBase> HitReactMontage;

    // Camera shake (ruled: fire and take-damage only, subtle, tuned live
    // through Breaker.Shake.*). Trauma model in Characters/BreakerShakeMath.h.
    void UpdateCameraShake(float DeltaSeconds);
    float ShakeTrauma = 0.0f;
    FRotator LastShakeOffset = FRotator::ZeroRotator;

    // The death beat: input dead, HUD reads REDEPLOYING, then the respawn.
    // Long enough to register as a consequence, short enough that unlimited
    // campaign respawn stays a rhythm rather than a punishment. O2 PLACEHOLDER.
    float RespawnDelaySeconds = 2.0f;
    bool bRespawnPending = false;
    FTimerHandle RespawnTimer;
public:
    // The HUD's read for the death beat's own line.
    bool IsAwaitingRespawn() const { return bRespawnPending; }
private:
    UPROPERTY() TObjectPtr<UBreakerQuestJournal> Quests;
    float LookSensitivity = 1.0f;
    bool bInvertLookY = false;
    bool bShowingInitialMenu = false;
    TSharedPtr<SBreakerMenu> MenuWidget;
    FTimerHandle ShotCosmeticTimer;
    // The traversal execution state that lived here (bMantling, the elapsed
    // clock, start/target/exit velocity) moved into the movement component's
    // custom mode with the smoothstep itself — the pawn asks, never executes.

    // The player's chosen FOV, kept separately from the camera's live FOV so
    // the dash punch is a pure offset. GetCurrentFOV() reports THIS, which is
    // what the settings screen edits and SavePlaytestSettings persists — a
    // punch mid-frame must never be able to become the saved preference.
    float BaseFieldOfView = 90.0f;
    // Negative = no dash feedback in flight.
    float DashFeedbackElapsed = -1.0f;
    float DashFeedbackScale = 1.0f;
    float DashFeedbackRollSign = 0.0f;
    FVector LastDashDirection = FVector::ZeroVector;
    bool bDashRollApplied = false;

    // The layout currently posed onto the pool, and the archetype it came from.
    // Tick compares the live archetype against this: the weapon can change from
    // the loadout screen, an item equip or a dev swap, and none of those route
    // through EquipPrimary/EquipSecondary — which is exactly why the proxy used
    // to keep the previous gun's proportions after a loadout change.
    FBreakerViewmodelLayout ActiveLayout;
    FVector PosedArmRestLocation = FVector(FLT_MAX);
    FTimerHandle ViewmodelCycleTimer;
    FTimerHandle ViewmodelFireTimer;
    int32 ViewmodelCycleIndex = 0;
    bool bViewmodelBuilt = false;
    EBreakerWeaponArchetype PresentedArchetype = EBreakerWeaponArchetype::Count;

    // --- Class-resource state derivation ---------------------------------
    // "In combat" for the Grit/Charge loops: damage taken OR dealt within the
    // window. O2 PLACEHOLDER — the project has no shared combat-state concept
    // yet, and this derivation is deliberately the dumbest thing that is true.
    float CombatStateWindowSeconds = 6.0f;   // O2 PLACEHOLDER
    // Class-Kits-Tank §1.1: an enemy within 5 m is the proximity source.
    float GritProximityRadiusCm = 500.0f;    // O2 PLACEHOLDER (§1.1: 5 m)
    float ClassResourcePollInterval = 0.25f; // O2 PLACEHOLDER
    float ClassResourcePollElapsed = 0.0f;
    double LastHitDealtTime = -1000.0;
};
