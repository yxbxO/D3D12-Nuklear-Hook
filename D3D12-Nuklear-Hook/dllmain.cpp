#include "pch.h"

static void on_attach(LPVOID lpParameter) {
    mem::d_log("[DllMain] Starting D3D12 Nuklear Hook...");

    // Initialize QueryPerformanceCounter hook to find SwapChain
    mem::module hdxgi(L"dxgi.dll");
    if (!hdxgi.loaded()) {
        mem::d_log("[DllMain] Failed to find dxgi.dll");
        return;
    }

    //static because needed for removing hook
    static auto import = hdxgi.get_import("QueryPerformanceCounter");
    if (!import) {
        mem::d_log("[DllMain] Failed to find QueryPerformanceCounter import");
        return;
    }

    // Install QPC hook to discover SwapChain and hook Present & ResizeBuffers
    g_qpc_hook.install_import(&import, reinterpret_cast<void*>(hooks::QueryPerformanceCounter_hk));

    mem::d_log("[DllMain] Hook initialization complete");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    DisableThreadLibraryCalls(hModule);

    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        on_attach(nullptr);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

