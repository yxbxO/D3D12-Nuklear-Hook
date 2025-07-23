#include "../pch.h"

/**
 * @brief Hooked Present function for DXGI swap chain. Initializes renderer if needed and draws GUI.
 * @param rcx Swap chain pointer (this)
 * @param SyncInterval Sync interval for presentation
 * @param Flags Presentation flags
 * @return HRESULT from original Present
 */
HRESULT __fastcall hooks::Present_hk(uintptr_t rcx, UINT SyncInterval, UINT Flags) {
    if (!g_renderer) {
        IDXGISwapChain* swap_chain = reinterpret_cast<IDXGISwapChain*>(rcx);
        D3DVersion version = D3DRendererFactory::detect_version();
        g_renderer = D3DRendererFactory::create_renderer(version, swap_chain);
    }

    if (g_renderer && !(Flags & DXGI_PRESENT_TEST)) {
        g_renderer->draw();
        g_renderer->render();
    }

    // Call original Present function
    auto original = g_present_hook.get_original<HRESULT(__fastcall*)(uintptr_t, UINT, UINT)>();
    return original(rcx, SyncInterval, Flags);
}