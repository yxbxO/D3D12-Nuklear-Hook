// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#include <winternl.h>
#include <memoryapi.h>
#include <string>
#include <string_view>
#include <mutex>
#include <intrin.h>
#include <format>
#include <vector>
#include <optional>
#include <queue>

#include <d3d12.h>
#include <d3d11.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <memory>

//#define NK_INCLUDE_COMMAND_USERDATA
//#define NK_INCLUDE_STANDARD_BOOL
//#define NK_INCLUDE_FIXED_TYPES
//#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
//#define NK_INCLUDE_FONT_BAKING
//#define NK_INCLUDE_DEFAULT_FONT
//#define NK_INCLUDE_DEFAULT_ALLOCATOR
//
//#define NK_IMPLEMENTATION
//#include "render/Nuklear/nuklear.h"

#include "mem/mem.h"

#include "hooks/hooks.h"

//#include "render/input/WndProc.h"
#include "render/core/ID3DRenderer.h"
#include "render/d3d11/D3D11Renderer.h"
#include "render/d3d12/D3D12Renderer.h"
#include "render/core/D3DRendererFactory.h"
#endif //PCH_H
