#include "../pch.h"

HRESULT __fastcall hooks::Present_hk(uintptr_t ecx, UINT SyncInterval, UINT Flags) {
    auto& renderer = D3D12::get();

    // Initialize SwapChain if not already done
    if (!renderer.swap_chain()) {
        renderer.set_swap_chain(reinterpret_cast<IDXGISwapChain3*>(ecx));
        mem::d_log("[Present] SwapChain initialized at {:#x}",
                   reinterpret_cast<uintptr_t>(renderer.swap_chain()));
    }

    // Initialize CommandQueue if not already done
    if (!renderer.command_queue()) {
        mem::module hdxgi(L"dxgi.dll");
        auto result = hdxgi.scan("8B 87 ? ? ? ? 89 06 8B 87 ? ? ? ? 89 46 04 8B 87");
        if (result.cast()) {
            auto commandQueueOffset = result.self_offset(2).get<unsigned int>() - 0x8;
            auto command_queue = *reinterpret_cast<ID3D12CommandQueue**>(ecx + commandQueueOffset);
            renderer.set_command_queue(command_queue);
            mem::d_log("[Present] CommandQueue initialized at {:#x} (offset: {:#x})",
                       reinterpret_cast<uintptr_t>(renderer.command_queue()), commandQueueOffset);
        } else {
            mem::d_log("[Present] Failed to find CommandQueue pattern");
        }
    }

    // Only render UI if we have both SwapChain and CommandQueue, and this isn't a test present
    if (renderer.swap_chain() && renderer.command_queue() && !(Flags & DXGI_PRESENT_TEST)) {
        renderer.draw();
        renderer.render();
    }

    // Call original Present function
    auto original = g_present_hook.get_original<HRESULT(__fastcall*)(uintptr_t, UINT, UINT)>();
    return original ? original(ecx, SyncInterval, Flags) : E_FAIL;
}