# Setup

Use Unreal Engine 5.8 on both computers. Install Git and Git LFS, then run `git lfs install` once per machine. Do not sync the project with iCloud, OneDrive, Dropbox, or a network drive.

## macOS

Install Xcode and its command-line tools. Open `riors_edge.uproject`; Unreal will compile the C++ module. If needed, generate Xcode project files from the `.uproject` context menu.

## Windows

Install Visual Studio 2022 with **Game development with C++**, Unreal tooling, and a current Windows SDK. Generate Visual Studio project files from the `.uproject` context menu.

## Switching machines

Close Unreal, commit and push, then pull before opening the project elsewhere. Never commit `Binaries`, `Intermediate`, `Saved`, or `DerivedDataCache`. Avoid concurrent edits to the same Blueprint or map because binary Unreal assets cannot be merged reliably.

## First integration pass

Create `Content/ProjectBreaker/Input/DA_PlayerInputConfig` as a `BreakerInputConfig` Data Asset. Reuse the template's `IA_Move`, `IA_Look`, `IA_Jump`, and `IMC_Default`; add Sprint, Dash, Fire, Aim, and Reload actions as needed. Create a Blueprint child of `BreakerCharacter`, assign the data asset, then migrate visual components or logic from `BP_FirstPersonCharacter` incrementally.

Until that Data Asset exists, the C++ character uses keyboard/mouse fallback mappings: WASD, mouse look, Space jump/wall jump, Shift sprint, Q dash, C or Left Ctrl slide, mouse buttons fire/aim, and R reload. The C++ game mode is the current global default so a clean clone can immediately test movement.
