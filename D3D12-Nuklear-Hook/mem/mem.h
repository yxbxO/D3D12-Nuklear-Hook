#pragma once
#include <cstdint>
#include <string>
#include <format>
#include <windows.h>
#include <vector>
#include <optional>

#include "address.h"
#include "pattern.h"
#include "range.h"
#include "module.h"

namespace mem {
    /**
     * @brief Check if a pointer is within valid user-mode address space
     * @param ptr The pointer to check
     * @return true if pointer is invalid, false if valid
     * 
     * Validates that a pointer is within the standard user-mode address space:
     * - Above 0x1000 (to avoid null page)
     * - Below 0x7FFFFFFEFFFF (user-mode limit on x64)
     */
    [[nodiscard]] inline bool is_bad_ptr(uintptr_t ptr) noexcept {
        constexpr uintptr_t MIN_VALID_ADDR = 0x1000;
        constexpr uintptr_t MAX_VALID_ADDR = 0x7FFFFFFEFFFF;
        return ptr < MIN_VALID_ADDR || ptr > MAX_VALID_ADDR;
    }

    /**
     * @brief Swap a virtual method table entry
     * @param vtable_ptr Pointer to the virtual method table
     * @param hook_fn New function pointer to install
     * @param index Index in the vtable to hook (default 0)
     * @return Original function pointer, or nullptr if operation failed
     * 
     * This function safely swaps a virtual method table entry by:
     * 1. Validating all input parameters
     * 2. Changing page protection to allow writing
     * 3. Swapping the function pointer
     * 4. Restoring original page protection
     * 
     * @note This modifies the vtable in-place. The object using this vtable
     *       must remain valid for the duration of the hook.
     */
    [[nodiscard]] inline void* swap_vmt(void* instance_ptr, void* hook_fn, size_t index = 0) noexcept {
        // validation
        if (!instance_ptr || !hook_fn /*|| is_bad_ptr(reinterpret_cast<uintptr_t>(vtable_ptr))*/) {
            return nullptr;
        }

        auto vtable = *(void***)(instance_ptr);
    
        const SIZE_T protect_size = sizeof(void*) * (index + 1);
    
        DWORD old_protect;
        if (!VirtualProtect(vtable, protect_size, PAGE_READWRITE, &old_protect)) {
            return nullptr;
        }

        void* original = vtable[index];
        vtable[index] = hook_fn;

        VirtualProtect(vtable, protect_size, old_protect, &old_protect);
    
        return original;
    }

    /**
     * @brief Debug logging utility that writes to DebugView
     * @param format Format string using std::format syntax
     * @param args Format arguments
     * 
     * Provides type-safe formatting using std::format and outputs to DebugView.
     * Automatically adds newline if not present.
     * 
     * @example
     * debug_log("Value: {}, String: {}", 42, "test");
     */
    template<typename... Args>
    inline void d_log(std::string_view format, Args&&... args) noexcept {
        std::string msg = std::vformat(format, std::make_format_args(args...));

        if (!msg.empty() && msg.back() != '\n') {
            msg += '\n';
        }

        OutputDebugStringA(msg.c_str());
    }

} // namespace mem