# Haystack

A needle in 4 million pieces of hay. A half-dome of individually hoverable, grabbable and throwable hay pieces, replicated between a listen server and its clients, at 60+ FPS on High scalability. Unreal Engine 5.8.2, C++ modules `Haystack`, `HaystackTests` and `HaystackGauntlet`.

Needs a DirectX 12 GPU with Shader Model 6 (Nanite, Lumen and virtual shadow maps). Target: RTX 5060 class at 1080p, High scalability.

Two engines are in play. `Haystack.uproject` associates the launcher 5.8, which builds and runs everything in Development. Shipping with the console, the log and the CSV profiler needs a source-built engine, because `Haystack.Target.cs` gives Shipping a unique build environment there and an installed engine refuses one. On the dev box that engine is registered as `VA-UE582-src`, every script below takes `-Engine <launcher version | registered build name | engine directory>` and defaults to the association.


## GPU performance story

How the pile went from 2 FPS and 21 GB to 100+ FPS at 1080p, run by run, with the measurements: [Docs/GpuPerfStory.md](Docs/GpuPerfStory.md).


## Scripts

Every `.ps1` in `Scripts/` dot-sources `Scripts/Engine.ps1`, resolves `-Engine` and builds the project's editor target with that engine before it runs, so switching engines does not stop at the module rebuild prompt. Arguments shown are the ones worth knowing, each script's header lists the rest.

Scripts/RunTests.ps1 [-Filter Haystack.Layout] -> Runs the automation tests headless with `UnrealEditor-Cmd` and prints one line per test. Seven tests cover the layout, the pick ray and the piece state.

Scripts/RunBots.ps1 [-RunSeconds 150] [-BotArgs on|needle] [-MaxFps 60] -> Starts a listen server and a client side by side in 960x540 windows, both running the dig bot and `Hay.NetStats`, then prints the interesting log lines. The replication measurement setup.

Scripts/RunGauntletDig.ps1 [-Players 2] [-Random] [-NeedleDepth "0,10"] [-Timeout 600] [-Build editor|<staged dir>] [-Configuration Development|Shipping] -> Gauntlet multiplayer dig test: a listen server and bot clients dig until someone lifts the needle. Fails on the timeout or when a role runs standalone. Writes `Saved/Results/HayDig_<Configuration>_<time>.txt` with the outcome, seconds to the find, connected players and each role's takes.

Scripts/RunGauntletPerf.ps1 [-Resolutions 1920x1080,2560x1440] [-Configurations Development,Shipping] [-Warmup 5] [-Capture 60] [-Fullscreen] -> Gauntlet idle performance test: one client stands at the pile per resolution and configuration, CSV profiler capture included. Any configuration other than Development is cooked and staged into `Saved/StagedBuilds/<Configuration>` first. Writes summaries and a PerfReportTool report to `Saved/Results/HayPerf_<time>`. Default resolutions are 1080p, 1440p and the primary display's native resolution.

Scripts/PackageShipping.ps1 [-NoZip] -> Cooks, stages and packages Shipping into `Packaged/Windows` without debug files, copies `Scripts/Packaged/*` next to the executable and zips the folder at 7-Zip ultra Deflate into `Packaged/Haystack-Shipping-<date>.zip`.

Scripts/RunGauntletDig-Launcher.bat, RunGauntletPerf-Launcher.bat -> The same scripts pinned to the launcher engine `5.8`. Arguments pass through.

Scripts/RunGauntletDig-Source.bat, RunGauntletPerf-Source.bat, PackageShipping-Source.bat -> Pinned to the source engine `VA-UE582-src`. Shipping runs need these.

Scripts/Packaged/ -> What ships inside the package: `PlayLocal.bat` (listen server and client side by side on one machine), `Host.bat` (listen server for the LAN), `Join.bat <ip>`, and the `README.txt` that explains them to the player.

Build/Scripts/ -> The Gauntlet test nodes `HayDig` and `HayPerf` in C#, compiled by UAT when a script passes `-ScriptsForProject`.

The `.cmd` files in the repository root and `Tools/Scripts` are the build, cook, package and run wrappers inherited from the daftsoftware StarterProject template.


## How to play

W A S D -> Move
Mouse -> Look
Space -> Jump
Left mouse button -> Pick up the piece under the crosshair, press again to throw it. The crosshair turns green over a piece within reach, the prompt names the key.

The needle is one piece of the pile, at least 40 cm below the surface and never higher than 2 m above the ground. Dig toward it by picking pieces away. Whoever lifts it wins, every player sees who found it.

To host or join from a build, give the executable a URL as its first argument: `Haystack.exe /Game/ThirdPersonBP/Maps/ThirdPersonExampleMap?listen` hosts, `Haystack.exe 192.168.1.20` joins. The launchers in `Scripts/Packaged` do exactly that.


## Console

The console is enabled in Shipping builds made with the source engine.

Hay.ToggleVisibility -> Hides or shows all hay. The needle stays visible, so you can confirm it exists.
Hay.NetStats <seconds> -> Logs connections, bandwidth and game thread time every N seconds. 0 stops.
Hay.DigBot [on|off|needle] -> A bot that digs on your player: random spots, or straight at the needle.
stat unit, stat fps -> The usual engine stats.


## Logs and results

Editor and `-game` runs -> `Saved/Logs`. Gauntlet role logs -> `Saved/Gauntlet`.
Packaged game -> `%LOCALAPPDATA%\Haystack\Saved\Logs\Haystack.log`, a second instance on the same machine writes `Haystack_2.log`.
Measurements kept in git -> `Saved/Results`: perf summaries with the PerfReportTool summary table, and dig test summaries.
