# Ability menus and cache interaction

Ability rows now calculate wrapping from the usable nested panel width, including scrollbar space. Vacant ability slots retain their slot names without redundant NONE labels. Existing fonts and ability selection behavior are unchanged.

Cache focus and opening share the same alive, unopened, distance and static-cover checks. Distant cache labels no longer crowd the world HUD. The focused cache prompt anchors at the console itself: the former person-height offset projected above the screen when standing close to it.

Rendered verification inspected both frames in each disposable capture under the workspace seat/captures directory:

- abilities-swift-1080-0805: 1920x1080, Swift selection rows and descriptions fit; quiet vacant second slot.
- abilities-caster-720-0806: 1280x720, Caster choices and assignment controls fit the panel.
- cache-prompt-fixed-0801: 1920x1080, nearby console displays F / CLEAR NEARBY HOSTILES with guards present.

The ability capture helper requires a fresh disposable UserDir and refuses saves. It clears the disposable pawn's automatic Swift selection before choosing the requested class. It does not edit player saves or unlock the roster. These captures prove static layout, not mouse focus recovery, hover states or interactive menu comfort.

Native cache coverage checks distant and covered focus rejection, restored focus after cover removal, and ordinary guarded interaction. Full regression results are recorded in STATE and the commit.
