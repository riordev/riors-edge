#pragma once
class UWorld;
// Isolated rendered benchmark: legal purchases, paid Mark, actual Lattice windup.
void BreakerScheduleTellCapture(UWorld* World);
// -BreakerCaptureBlast: spawn a Volatile body, kill it, and hold the fuse open
// long enough to photograph the blast ring it draws on the ground.
void BreakerScheduleBlastCapture(UWorld* World);
// -BreakerCapturePocketRift: stand the player in front of a pocket's arrival
// tear and flare it on a loop, so the frames catch it both at rest and at the
// moment a patrol comes through. A tear is 750 cm behind a formation and
// therefore never in shot on the ordinary Fernhall route.
void BreakerSchedulePocketRiftCapture(UWorld* World);
// -BreakerCaptureChest: stand in front of the nearest supply chest. They are
// placed by a session roll rather than authored, so there is no fixed point to
// aim a camera at.
void BreakerScheduleChestCapture(UWorld* World);
