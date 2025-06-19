#pragma once
#include <Windows.h>
#include <dxgi1_4.h>
#include <atomic>

/**
 * @brief Self-contained hook with automatic VMT management
 */
struct Hook {
	void* p_table{nullptr};         // table pointer
	void* original{nullptr};        // Original function pointer
	void* target{nullptr};          // Our hook function pointer  
	bool installed{false};          // Installation status
	int index;

	Hook() = default;
	
	/**
	 * @brief Install VMT hook
	 */
	template<typename T>
	bool install(T* object, void* hook_func, int vmt_index) {
		if (installed) return false;
		
		p_table = object;
		index = vmt_index;
		target = hook_func;
		original = reinterpret_cast<void*>(mem::swap_vmt(p_table, target, index));
		installed = true;
		return true;
	}
	
	/**
	 * @brief Install import hook  
	 */
	bool install_import(void* p_import_address, void* hook_func) {
		if (installed) return false;
		
		p_table = p_import_address;
		index = 0;
		target = hook_func;
		original = reinterpret_cast<void*>(mem::swap_vmt(p_table, target, index));
		installed = true;
		return true;
	}
	
	/**
	 * @brief Remove hook and re-install original pointer
	 */
	void uninstall() {
		target = mem::swap_vmt(p_table, original, index);

		p_table = target = original = nullptr;
		installed = false;
	}

	/**
	 * @brief Get original function as specific type
	 */
	template<typename T>
	T get_original() const {
		return reinterpret_cast<T>(original);
	}
};

// Global variable definitions
inline Hook g_present_hook{};
inline Hook g_resize_buffers_hook{};
inline Hook g_qpc_hook{};

namespace hooks {
	// Hook function declarations
	extern HRESULT __fastcall Present_hk(uintptr_t ecx, UINT SyncInterval, UINT Flags);
	extern HRESULT __fastcall ResizeBuffers_hk(uintptr_t ecx, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
	extern BOOL __fastcall QueryPerformanceCounter_hk(LARGE_INTEGER* lpPerformanceCount);
}