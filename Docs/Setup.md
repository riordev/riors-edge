# Setup

**Last reconciled against: O32**

Use Unreal Engine 5.8 on both computers. Install Git and Git LFS, then run `git lfs install` once per machine. Do not sync the project with iCloud, OneDrive, Dropbox, or a network drive.

## macOS

Install Xcode and its command-line tools. Open `riors_edge.uproject`; Unreal will compile the C++ module. If needed, generate Xcode project files from the `.uproject` context menu.

## Windows

Install Visual Studio 2022 with **Game development with C++**, Unreal tooling, and a current Windows SDK. Generate Visual Studio project files from the `.uproject` context menu.

## Switching machines

Close Unreal, commit and push, then pull before opening the project elsewhere. Never commit `Binaries`, `Intermediate`, `Saved`, or `DerivedDataCache`. Avoid concurrent edits to the same Blueprint or map because binary Unreal assets cannot be merged reliably.

## Player assembly

`Content/ProjectBreaker/Characters/BP_BreakerCharacter` is the active assembly and remains a child of the C++ `BreakerCharacter`. It uses `Content/ProjectBreaker/Input/DA_PlayerInputConfig` and `IMC_Player`, which include keyboard/mouse and controller mappings. `Scripts/create_movement_gym_assets.py` can recreate or validate these assets through Unreal Editor automation.

If those assets are absent, the C++ character remains a functional fallback. Its legacy mappings support WASD or left stick movement, mouse or right stick look, Space / gamepad south jump, Shift / left-stick click sprint, Q / gamepad east dash, C or Left Ctrl / gamepad west slide, mouse buttons or triggers fire/aim, and R / gamepad north reload.

## Build, test, capture

The three commands an agent actually needs. Paths assume the repository root;
`<repo>` is that directory.

**Build.** Fails while the editor is open — the Live Coding lock. Close the
editor, or press Ctrl+Alt+F11 in it, first.

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ^
  RiorsEdgeEditor Win64 Development -Project="<repo>\riors_edge.uproject" -WaitMutex
```

When the owner has the MAIN tree's editor open and you are building in a
separate worktree, the lock is a false positive — the guard keys off the shared
`UnrealEditor.exe`, not the project DLL. `-NoHotReloadFromIDE` is the correct
override **in that case only**.

**Tests.** Headless, no RHI:

```
UnrealEditor-Cmd.exe "<repo>\riors_edge.uproject" ^
  -ExecCmds="Automation RunTests RiorsEdge; Quit" -unattended -nop4 -nosplash -nullrhi
```

Then grep the log for `Result={Fail}`. See `CONTEXT.md` for the current expected
result — **two tests fail on `main` by design** and must not be "fixed".

**Capture.** The project can photograph itself. Switches are listed in
`CONTEXT.md`; frames land in `Saved/Screenshots/breaker_NN.png` and the process
exits on its own. `Saved/` is never committed.
