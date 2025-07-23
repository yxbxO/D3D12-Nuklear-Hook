#include "../pch.h"

// Internal state for QPC hook
int qpc_tries = 0;
constexpr int MAX_QPC_TRIES = 10;

/**
 * @brief Attempts to find the SwapChain pointer from QPC counter address
 */
__forceinline static void* try_get_swapchain_from_qpc(LARGE_INTEGER* counter) noexcept {
    if (!counter) {
        return nullptr;
    }

    // Calculate potential SwapChain pointer using known offset pattern
    auto addr = reinterpret_cast<__int64>(_ReturnAddress());
    auto offset = *reinterpret_cast<int*>(addr - 11);
    mem::d_log("[QueryPerformanceCounter] Relative SwapChain offset at {:#x}", offset);

    // Sanity check. Not the best but catches most false positives.
    if (offset >= 0x10000)
        return nullptr;

    auto potential_chain =
        reinterpret_cast<IDXGISwapChain3*>(reinterpret_cast<__int64>(counter) - offset);

    // Validate the potential SwapChain pointer
    if (!mem::is_bad_ptr(reinterpret_cast<uintptr_t>(potential_chain))) {
        void** vtable = *reinterpret_cast<void***>(potential_chain);
        mem::module hdxgi(L"dxgi.dll");
        if (vtable && hdxgi.contains(vtable)) {
            return potential_chain;
        }
    }

    return nullptr;
}

BOOL __fastcall hooks::QueryPerformanceCounter_hk(LARGE_INTEGER* lpPerformanceCount) {
    // Call original QPC function first
    auto original = g_qpc_hook.get_original<BOOL(__stdcall*)(LARGE_INTEGER*)>();


    if (!g_renderer && qpc_tries++ < MAX_QPC_TRIES) {
        if(IDXGISwapChain* swap_chain = reinterpret_cast<IDXGISwapChain*>(try_get_swapchain_from_qpc(lpPerformanceCount)); swap_chain != nullptr)
        {
            D3DVersion version = D3DRendererFactory::detect_version();
            g_renderer = D3DRendererFactory::create_renderer(version, swap_chain);

           // auto* renderer = static_cast<D3D12Renderer*>(g_renderer.get());

            mem::d_log("[QueryPerformanceCounter] Found SwapChain at {:#x}",
                reinterpret_cast<uintptr_t>(swap_chain));

            // Remove QPC hook as it's no longer needed
            static_cast<void>(g_qpc_hook.uninstall());

            auto present_hook_status = g_present_hook.install(
                swap_chain, reinterpret_cast<void*>(hooks::Present_hk), 8);
            mem::d_log("[QueryPerformanceCounter] present hook status {}",
                (int)present_hook_status);

            auto resize_hook_status = g_resize_buffers_hook.install(
                swap_chain, reinterpret_cast<void*>(hooks::ResizeBuffers_hk), 13);
            mem::d_log("[QueryPerformanceCounter] resizebuffers hook status {}",
                (int)resize_hook_status);

            mem::d_log("[QueryPerformanceCounter] Successfully unhooked QPC");
        }
    }
    else if (qpc_tries >= MAX_QPC_TRIES) {
        mem::d_log("[QueryPerformanceCounter] Failed to find SwapChain after {} tries",
            MAX_QPC_TRIES);
        static_cast<void>(g_qpc_hook.uninstall());
        mem::d_log("[QueryPerformanceCounter] Unhooked QPC after max tries");
    }

    BOOL ret = original(lpPerformanceCount);
    return ret;
}