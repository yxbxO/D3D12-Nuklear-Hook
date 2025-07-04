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
#include <d3d12.h>

#include "mem/mem.h"

#include "hooks/hooks.h"

#include "render/D3D12.h"
#endif //PCH_H
