# Starter Project (Mobile)

A Blueprint-only Unreal Engine 5.7+ project based on [daftsoftware/StarterProject](https://github.com/daftsoftware/StarterProject).

## Changes from the original

- **Third Person template content** added to `Content/` (Mannequin, ThirdPersonBP, etc.)
- **`StarterHotpatch` module** (C++) replaced with the pluginized version from [Vaei/ForwardRender](https://github.com/Vaei/ForwardRender), included as a git submodule at `Plugins/ForwardRender`
- **ProjectCleaner** plugin removed
- **All project C++ modules removed** — this is now a Blueprint-only project

## Getting Started

1. Clone with submodules:
   ```
   git clone --recurse-submodules <repo-url>
   ```
2. Open `VA_Base.uproject` in Unreal Engine 5.7+
3. The ForwardRender plugin will auto-compile on first editor launch
