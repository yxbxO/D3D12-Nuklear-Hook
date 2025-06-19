# D3D12 Nuklear Hook

Universal DirectX 12 hook with [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) immediate-mode GUI for overlay development and runtime modification.

![D3D12 Hello Triangle Demo](2025-06-19%2004_18_01-D3D12%20Hello%20Triangle.png)

## Features

* DirectX 12 integration with native D3D12 rendering pipeline
* [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) immediate-mode GUI with minimal overhead
* Custom VMT (Virtual Method Table) hooking without external dependencies
* Thread-safe design for multi-threaded applications
* Automatic SwapChain and CommandQueue detection
* Input handling with selective forwarding
* Custom HLSL shaders optimized for UI rendering

## Usage

Call the hook initialization in your DLL injection process. The library will automatically discover D3D12 resources and inject the [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) GUI overlay.

## How it works

### DirectX 12

We use a multi-stage hooking approach:

1. **Discovery Phase**: Hook `QueryPerformanceCounter` import to detect SwapChain creation
2. **Attachment Phase**: Install VMT hooks on discovered D3D12 interfaces  
3. **Rendering Phase**: Intercept `Present` calls to inject [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) UI

**Note**: QPC hooking is a quick method that may slip basic anticheat detection but carries risk of being caught by advanced systems. For improved stealth, consider using codecave/trampoline techniques where hooks point to legit memory regions instead of directly to your module. Tested successfully on protected D3D12 applications.

The implementation obtains pointers to the D3D12 SwapChain and CommandQueue interfaces for hooking. The UI rendering commands are submitted during `Present()` calls to ensure proper integration with the application's rendering pipeline.

## Compilation

1. Download and install Microsoft Visual Studio 2022
2. Open `D3D12-Nuklear-Hook.sln` with Visual Studio
3. Select **Release x64** (recommended)
4. Press `Ctrl+Shift+B` to build (creates DLL)
5. Inject `x64/Release/D3D12-Nuklear-Hook.dll` into a D3D12 application

## Runtime Control

* Press `INSERT` key to toggle the UI overlay
* Mouse and keyboard input is automatically captured when UI is active
* Use DebugView to monitor hook status and debug output

## Dependencies

* **Windows SDK 10.0+** - Required for D3D12 and DXGI headers
* **Visual Studio 2022** - C++23 standard library features
* **[Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)** - Embedded immediate-mode GUI library (header-only)

## Known Issues

**Tip**: Try pressing the "Insert" key to toggle the overlay if something isn't working.

* **[!] May conflict with other overlays such as MSI Afterburner and Steam overlay**
* Some applications may require specific injection timing
* Applications using D3D11on12 are not supported

## Compatibility

* **Operating Systems**: Windows 10/11 (x64 only)
* **D3D12 Feature Levels**: 11_0, 11_1, 12_0, 12_1, 12_2
* **Display Modes**: Windowed, borderless fullscreen, exclusive fullscreen
* **Tested**: Unity games, Unreal Engine titles, custom D3D12 engines

## License

This project is licensed under the MIT License.

## Acknowledgments

* **[Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)** - Immediate mode GUI library by Micha Mettke
* **Microsoft** - DirectX 12 graphics API
* Inspired by various hooking techniques and D3D12 community resources

---

**Note**: This library modifies runtime behavior of applications. Always test in development environments. Users are responsible for compliance with applicable software licenses. 