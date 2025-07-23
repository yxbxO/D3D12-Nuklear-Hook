#include "../pch.h"

/**
 * @brief Hooked ResizeBuffers function for DXGI swap chain. Releases and recreates swap chain buffers.
 * @param rcx Swap chain pointer (this)
 * @param BufferCount Number of buffers
 * @param Width New width
 * @param Height New height
 * @param NewFormat New DXGI format
 * @param SwapChainFlags Swap chain flags
 * @return HRESULT from original ResizeBuffers
 */
HRESULT __fastcall hooks::ResizeBuffers_hk(uintptr_t rcx, UINT BufferCount, UINT Width, UINT Height,
                                           DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    HRESULT hr = S_OK;
    auto* renderer = static_cast<D3D12Renderer*>(g_renderer.get());

    renderer->mutex().lock();

    renderer->release_swap_chain_buffers();

    auto original =
        g_resize_buffers_hook
            .get_original<HRESULT(__fastcall*)(uintptr_t, UINT, UINT, UINT, DXGI_FORMAT, UINT)>();
    if (original) {
        hr = original(rcx, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    if (SUCCEEDED(hr)) {
        renderer->get_swap_chain_buffers(Width, Height);
    }

    renderer->mutex().unlock();

    return hr;
}