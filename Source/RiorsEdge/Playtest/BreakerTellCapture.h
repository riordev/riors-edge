#pragma once
class UWorld;
// Isolated rendered benchmark: legal purchases, paid Mark, actual Lattice windup.
void BreakerScheduleTellCapture(UWorld* World);
// -BreakerCaptureBlast: spawn a Volatile body, kill it, and hold the fuse open
// long enough to photograph the blast ring it draws on the ground.
void BreakerScheduleBlastCapture(UWorld* World);
