# Isolated cooperative combat

The opt-in Fernhall listen-server sandbox uses distinct transient profiles,
the Swift starter kit and no persistent character/account writes. It blocks
player-to-player damage in this mode; normal enemy/self damage remains.
Clients construct the same static Fernhall environment locally once per world;
enemies, loot and interactions remain authoritative server actors.

Run `Scripts/coop-combat-smoke.ps1 -Visible -KeepRunning` for two manual
windows, then close both to return to ordinary play. The default invocation
is hidden, bounded and closes only its own processes. `Breaker.CoopHost`
and `Breaker.CoopJoin host:port` are developer-console alternatives; those
commands latch nonsaving mode until the process restarts.

Run `Scripts/coop-combat-verify.ps1` for the bounded real-network check.
Both scripts use separate fresh disposable user directories, retain real-save
hash checks, and reject any save created inside either disposable directory.
Live logs are read with shared file access to coexist with Unreal's writer.

## Observed on this seat

Verification passed in two Unreal processes on 2026-09-09 at07:07 UTC.
Logs: `Saved/CoopSmoke/3e4e52a5b3c84f88bb4e24f06c448d3c`.

- Server initialized two different authoritative profiles; guest observed both.
- Guest built local Fernhall geometry and lighting.
- Server observed126.48cm guest displacement during the input phase. This is
  displacement evidence, not isolated proof that input was its only cause.
- Guest called normal Weapon.StartFire, which sends ServerStartFire. Server
  recorded nine guest-attributed weapon hits against one ordinary level1 enemy.
- Guest first observed220 health, then148, then0 on that same replicated enemy.
- Server recorded death attributed to the same guest profile. Death notification
  precedes the final hit notification; its logged hit counter was therefore8.
- Script exited0, closed both owned processes and found no changed/created saves.

This is actual fire-RPC and health/death transport evidence. It does not test
latency compensation, matchmaking, NAT, persistent campaign progression,
cross-map travel, full ability replication, drop contention, guest reconnect,
or human death/respawn presentation quality. Those remain explicit multiplayer acceptance work.
The verification target is no-loot/no-respawn and the harness aims automatically;
it does not stand in for human combat feel or visual review.

## Owner slots and guest respawn follow-up

The owner now receives slot ability IDs, resolves their registry definitions,
and reports a granted slot only when a real replicated GAS spec matches its
class and input ID. Empty/invalid slots remain empty; no client grants are
manufactured. Dead characters refuse both slot and direct GAS activation.

The owning guest observes replicated physical health for presentation only:
zero clears held input and starts the existing death beat; positive health
restores input/fade. The server alone owns death events, respawn timing,
teleport, ammunition and vitals restoration. Native tests cover actual paid
activation before/after death and replicated-health presentation boundaries.

Two-process verification passed at09:08 UTC, logs
`Saved/CoopSmoke/475ca9d4220e4e4dbf6caf3f3b9ed012`:

- Guest received Slipcut, empty second slot and Overdrive matched to real specs.
- Nine actual guest weapon hits killed the ordinary220-health target.
- Verification then issued one server environmental lethal hit. Server observed
  one death and one normal timer restoration to100 health about2 seconds later.
- Guest observed health0/input off and health100/input on, with zero local
  gameplay death/restoration delegates. Identity matched the same firing guest.
- Both disposable profiles and real save hashes remained protected.

The input verifier now waits for possession and the guest view target, and
starts its movement clock after issuing the first input. Counting the loading
frame beforehand had consumed the entire short input window.

Optional screenshot extension is parked outside the checkout: an early capture
still disrupts that short movement phase. Three initial frames were inspected;
two preceded camera readiness and one showed the actual rough first-person
world without a complete combat HUD. No successful rendered death/revival
sequence is claimed. Do not treat script receipts as visual acceptance.

Remaining multiplayer work includes local predicted ability activation/HUD
feedback, actual cost/effect transport, loot contention, reconnect, campaign
support, and human input/presentation checks. This remains a transient Swift
combat sandbox, not a multiplayer campaign release.

Final build/full suite:854 passing,3 expected reds,0 unexpected; status regenerated.

## Real predicted guest activation

Owner input now resolves the actual matching replicated GAS spec and starts normal LocalPredicted activation locally. Hold/deployables retain their explicit server-only press handling. Metadata never creates a grant or handle.

Actual two-process verification passed at10:19 UTC in `Saved/CoopSmoke/8938cb98e5ee4ccd9cd579a83ca12ca4`:

- Ordinary movement earned starter Slipcut's20 Momentum. Guest and server each observed one activation, one end and an exact20 debit, under the same native prediction key1.
- The guest immediately saw cooldown and refused a second press without further cost. The server cadence multiplier moved1→2 with the actual window and4-second cooldown.
- Native key catch-up retired the guest's temporary predicted cost effect. Guest current/base resource both read5.078 at that observation. This establishes predicted modifier cleanup; it does not prove exact ongoing server/client bank equality while native decay and replication continue.
- The existing weapon target/death and guest server-respawn/client-input lifecycle also passed. The requested cycle had exactly one server death and one restore, with zero guest gameplay death/restore delegates. Save hashes remained unchanged.

The longer movement-funding phase exposed ordinary ambient deaths before the requested lifecycle. The verifier now preserves and logs those totals (six deaths/restores in this run), then measures the explicitly requested check from its guarded living start. Duplicate events during that measured interval still fail. No immunity, enemy suppression or resource grants were added. The bare Swift smoke profile is not evidence of comfortable or balanced cooperative encounters.

An artificial standalone role-flip prediction fixture was parked after the five-minute investigation cap: it advertised activation but did not reproduce payment, even after refreshing Unreal's cached network role. Its checks were not weakened into a pass. This change's prediction acceptance is the actual two-process run above. Full-suite regression/status is recorded with the landing commit.

Independent reviews covered the production grant lookup, actual GAS callback/prediction-key observations and lifecycle counter baselines. Remaining work: exact bank convergence under latency/rejection, other classes/abilities, loot contention (Refractor can select a protected ally and waste a fork seat), reconnect, campaign support and human presentation/feel.

Landing regression:856 passing,3 expected failures,0 unexpected; status regenerated.
