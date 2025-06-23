#pragma once
#include <atomic>

#include <Windows.h>
#include <dxgi1_4.h>

/**
 * @brief Hook installation result codes
 */
enum class HookResult : int {
    Success = 0,
    AlreadyInstalled = 1,
    InvalidParameters = 2,
    SwapFailed = 3,
    NotInstalled = 4
};

/**
 * @brief Minimal hook class for DLL injection
 */
class Hook {
  private:
    void* p_table_{nullptr};
    void* original_{nullptr};
    void* target_{nullptr};
    std::atomic<bool> installed_{false};
    int index_{-1};

  public:
    Hook() = default;

    /**
     * @brief Move constructor
     */
    Hook(Hook&& other) noexcept
        : p_table_(std::exchange(other.p_table_, nullptr)),
          original_(std::exchange(other.original_, nullptr)),
          target_(std::exchange(other.target_, nullptr)),
          installed_(other.installed_.exchange(false)),
          index_(std::exchange(other.index_, -1)) {}

    /**
     * @brief Move assignment
     */
    Hook& operator=(Hook&& other) noexcept {
        if (this != &other) {
            static_cast<void>(uninstall());
            p_table_ = std::exchange(other.p_table_, nullptr);
            original_ = std::exchange(other.original_, nullptr);
            target_ = std::exchange(other.target_, nullptr);
            installed_ = other.installed_.exchange(false);
            index_ = std::exchange(other.index_, -1);
        }
        return *this;
    }

    /**
     * @brief Destructor
     */
    ~Hook() {
        static_cast<void>(uninstall());
    }

    // Delete copy operations
    Hook(const Hook&) = delete;
    Hook& operator=(const Hook&) = delete;

    // Essential getters
    [[nodiscard]] bool is_installed() const noexcept {
        return installed_.load();
    }
    [[nodiscard]] void* get_original_ptr() const noexcept {
        return original_;
    }

    /**
     * @brief Get original function as specific type
     */
    template <typename T>
    [[nodiscard]] T get_original() const noexcept {
        return reinterpret_cast<T>(original_);
    }

    /**
     * @brief Install VMT hook
     */
    template <typename T>
    [[nodiscard]] HookResult install(T* object, void* hook_func, int vmt_index) noexcept {
        if (installed_.load())
            return HookResult::AlreadyInstalled;
        if (!object || !hook_func || vmt_index < 0)
            return HookResult::InvalidParameters;

        p_table_ = object;
        index_ = vmt_index;
        target_ = hook_func;

        original_ = mem::swap_vmt(p_table_, target_, index_);
        if (!original_) {
            reset_state();
            return HookResult::SwapFailed;
        }

        installed_.store(true);
        return HookResult::Success;
    }

    /**
     * @brief Install import hook
     */
    [[nodiscard]] HookResult install_import(void* p_import_address, void* hook_func) noexcept {
        if (installed_.load())
            return HookResult::AlreadyInstalled;
        if (!p_import_address || !hook_func)
            return HookResult::InvalidParameters;

        p_table_ = p_import_address;
        index_ = 0;
        target_ = hook_func;

        original_ = mem::swap_vmt(p_table_, target_, index_);
        if (!original_) {
            reset_state();
            return HookResult::SwapFailed;
        }

        installed_.store(true);
        return HookResult::Success;
    }

    /**
     * @brief Remove hook
     */
    [[nodiscard]] HookResult uninstall() noexcept {
        if (!installed_.load())
            return HookResult::NotInstalled;

        void* restored = mem::swap_vmt(p_table_, original_, index_);
        if (!restored)
            return HookResult::SwapFailed;

        reset_state();
        return HookResult::Success;
    }

    /**
     * @brief Force uninstall (ignores return value)
     */
    void force_uninstall() noexcept {
        if (installed_.load()) {
            static_cast<void>(mem::swap_vmt(p_table_, original_, index_));
            reset_state();
        }
    }

  private:
    void reset_state() noexcept {
        p_table_ = nullptr;
        original_ = nullptr;
        target_ = nullptr;
        installed_.store(false);
        index_ = -1;
    }
};

// Global hook instances
inline Hook g_present_hook{};
inline Hook g_resize_buffers_hook{};
inline Hook g_qpc_hook{};

namespace hooks {
// Hook function declarations
extern HRESULT __fastcall Present_hk(uintptr_t ecx, UINT SyncInterval, UINT Flags);
extern HRESULT __fastcall ResizeBuffers_hk(uintptr_t ecx, UINT BufferCount, UINT Width, UINT Height,
                                           DXGI_FORMAT NewFormat, UINT SwapChainFlags);
extern BOOL __fastcall QueryPerformanceCounter_hk(LARGE_INTEGER* lpPerformanceCount);
}  // namespace hooks