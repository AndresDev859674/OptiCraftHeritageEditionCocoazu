# OptiCraft Heritage Linux

OptiCraft Heritage is a heavily modified, clean-room C++ implementation of classic Minecraft-era gameplay designed around portability, low-end hardware, and console-specific optimization.

This repository is not intended to be a line-for-line source translation. The runtime, platform layers, rendering paths, input backends, storage systems, user interface, asset loading, memory policies, and console support have been extensively reworked for the needs of this project.

Why this Fork?, Simple, The Project Has many Visual Studio 2022 Objectives and many windows compilation, SO, i make this fork for the compilation for linux its much easier

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

```text
  cmake -B build -G "Ninja" \
          -DSDL_PIPEWIRE=OFF \
          -DPC_LEGACY_BUILD=ON \
          -DCMAKE_BUILD_TYPE=Debug

  cmake --build build -j$(nproc)
```

[Then, The bin folder and Debug, Download and Put all in the / of the Debug folder...](https://drive.google.com/uc?export=download&id=1TgTq_0Ypl51yVwQNqhVKcAxRSuqtj9KV)
Or else, Will not run!

## Development notes

OptiCraft Heritage contains substantial platform-specific changes compared with the behavior it reproduces. Examples include custom render backends, legacy UI work, low-memory chunk policies, console input layers, asset streaming, platform storage, audio backends, profiling, and console-specific performance tuning.

When changing shared systems, keep the platform abstraction boundary intact and avoid introducing PC-only assumptions into common code. Likewise, console-specific optimizations should remain behind platform policies or dedicated backends whenever possible.

## Third-party software

Third-party libraries are kept under `external/` and retain their respective licenses and notices. Review those licenses independently before redistributing binaries.

## Compiling whith linux
You needed original b1.7.3 jar file and unzip into assets

```sh 
# clone the proyect
git clone https://github.com/OptiJuegos/OptiCraftHeritageEdition.git
cd OptiCraftHeritageEditionLinux

# move your b1.7.3 jar file to `/assets`
unzip b1.7.3.jar

# make output folder
mkdir -p bin/Release

# create a simblinks 
ln -s ../../assets bin/Release/assets

# just compile(this compilation is from my cpu zen3)
cmake -B build -G "Ninja" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-march=znver3 -mtune=znver3 -O3 -pipe" \
  -DCMAKE_CXX_FLAGS="-march=znver3 -mtune=znver3 -O3 -pipe" \
  -DOPTICRAFT_ENABLE_LTO=ON \
  -DSDL_PIPEWIRE=OFF \
  -DPC_LEGACY_BUILD=OFF
cmake --build build -j$(nproc)

# Just run
cd bin/Release
./OptiCraft

```
