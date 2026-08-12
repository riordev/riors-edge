# Setup

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
