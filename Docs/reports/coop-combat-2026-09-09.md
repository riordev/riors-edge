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
or death/respawn handoff. Those remain explicit multiplayer acceptance work.
The verification target is no-loot/no-respawn and the harness aims automatically;
it does not stand in for human combat feel or visual review.
