# OptiCraft Heritage Cocoazú Mod

>This is a heavy focused Opticraft Heritage for PC
>
>If you wanna compile it for PS2 and Wii, Good Luck! :)

OptiCraft Heritage Cocoazú! its a Modified Opticraft Heritage Edition for QoL improvements and balancing the game and improving the performance for PC, and esthetic options... It is heavily designed for Linux; you can try it on Windows.

In the Future, the Project will add blocks and items and more!

This project is also a version of Minecraft that I would like...

## Opticraft Heritage README
OptiCraft Heritage is a heavily modified, clean-room C++ implementation of classic Minecraft-era gameplay designed around portability, low-end hardware, and console-specific optimization.

This repository is not intended to be a line-for-line source translation. The runtime, platform layers, rendering paths, input backends, storage systems, user interface, asset loading, memory policies, and console support have been extensively reworked for the needs of this project.



<table>
  <tr>
    <td width="50%" align="center">
      <img width="859" height="484" alt="image" src="https://github.com/user-attachments/assets/abfebcde-d9fe-4ff5-a13c-660b0b8587c7" />
      <br />
      <sub><em>Main Menu</em></sub>
    </td>
    <td width="50%" align="center">
      <img width="860" height="486" alt="image" src="https://github.com/user-attachments/assets/afeee26e-0013-42d3-8229-193a5ed6d3df" />
      <br />
      <sub><em>Video Options in Legacy UI (Optifine Options here!)</em></sub>
    </td>
  </tr>
</table>

### THIS FORK DONT INCLUDE PS2 AND WII COMPILATION AND 32-BIT SUPPORT SOON
*or i think the 32 bit support works*

## Clean-room implementation

OptiCraft Heritage is developed as a clean-room implementation. The project code is independently implemented in C/C++ and is heavily modified around its own runtime and platform architecture.

The project does not rely on original proprietary game source code as part of its implementation. Compatibility-oriented behavior may be reproduced from observable behavior, documented formats, protocol behavior, and independently developed interfaces.

This project is not affiliated with, endorsed by, or sponsored by Mojang Studios or Microsoft.

## Supported targets

### PC

The desktop build uses SDL2, OpenGL, and the shared platform abstraction layer. A dedicated 32-bit legacy profile is available for older SSE2-class CPUs and legacy OpenGL hardware.

## Source layout

```text
src/
  client/       Client-side shared code
  java/         Java compatibility/runtime helpers
  net/          Game implementation
  platform/     Shared platform interfaces and backend selection
  pc/           Desktop-specific implementation
  ps2/          PlayStation 2 implementation
  wii/          Nintendo Wii implementation
  util/         Shared utility code

cmake/          Toolchains, source selection, and platform build logic
external/       Third-party dependencies
```

Platform targets deliberately select one implementation for each public backend. This keeps PC, PS2, and Wii implementations from accidentally entering the same link target.

### Desktop
Make Sure to have `sdl2_net` to avoid errors!

Debug Build :
```text
  cmake -B build -G "Ninja" \
          -DSDL_PIPEWIRE=OFF \
          -DPC_LEGACY_BUILD=ON \
          -DCMAKE_BUILD_TYPE=Debug \
          --preset linux-debug \

  cmake --build build -j$(nproc)
```

Release Build :
```text
  cmake -B build -G "Ninja" \
          -DSDL_PIPEWIRE=OFF \
          -DPC_LEGACY_BUILD=ON \
          -DCMAKE_BUILD_TYPE=Debug \
          --preset linux-release \

  cmake --build build -j$(nproc)
```

[Then, The bin folder and Debug, Download and Put all in the / of the Debug folder...](https://drive.google.com/uc?export=download&id=1AI4qjkCv9aW7dJD2_nwsiEslmdwhUVaK)
Or else, Will not run!

## Roadmap
- [x] Open the Survival inventory with `R` in Creative Mode..
- [ ] Unlock the Chat for commands and more in singleplayer...
- [ ] Add chainmail item to craft its type of armor.
- [ ] Improve Optifine, and smarter integration of optimization of chunks
      
## Development notes

OptiCraft Heritage contains substantial platform-specific changes compared with the behavior it reproduces. Examples include custom render backends, legacy UI work, low-memory chunk policies, console input layers, asset streaming, platform storage, audio backends, profiling, and console-specific performance tuning.

When changing shared systems, keep the platform abstraction boundary intact and avoid introducing PC-only assumptions into common code. Likewise, console-specific optimizations should remain behind platform policies or dedicated backends whenever possible.

## Third-party software

Third-party libraries are kept under `external/` and retain their respective licenses and notices. Review those licenses independently before redistributing binaries.
