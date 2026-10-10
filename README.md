# Runtime-File-Resolver

[SKSE plugin](https://www.nexusmods.com/skyrimspecialedition/mods/194279) A lightweight, data-driven framework that lets mods integrate with other frameworks by conditionally generating or modifying their INI files according to Skyrim’s active load order

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2026](https://visualstudio.microsoft.com/)
	* Desktop development with C++
	
## User Requirements
* [Skyrim Script Extender (SKSE64)](https://www.nexusmods.com/skyrimspecialedition/mods/30379)
	* Needed for SSE
* [Skyrim Script Extender for VR (SKSEVR)](https://www.nexusmods.com/skyrimspecialedition/mods/30457)
	* Needed for VR	
	
## Register Visual Studio as a Generator
* Open `x64 Native Tools Command Prompt`
* Run `cmake`
* Close the cmd window

## Building
```
git clone https://github.com/nicola89b/Runtime-File-Resolver.git
cd Runtime-File-Resolver
git submodule init
// to update submodules to checked in build
git submodule update
cmake --preset visual-studio
cmake --build --preset visual-studio-release
```
