#pragma once

// forward declare.
//struct nk_context;

class D3D12Renderer : public ID3DRenderer {
  private:
    // Core swap chain and presentation resources
    IDXGISwapChain3* m_swap_chain = nullptr;
    ID3D12CommandQueue* m_command_queue = nullptr;
    ID3D12DescriptorHeap* m_rtv_descriptor_heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE* m_rtv_handles = nullptr;
    ID3D12Resource** m_rtv_buffers = nullptr;
    UINT m_rtv_desc_increment = 0;
    UINT m_rtv_buffer_count = 0;

    // Nuklear context and UI state
    nk_context* m_nk_ctx = nullptr;

    // Window procedure hook state
    WNDPROC m_originalWndProc = nullptr;

    // Private helper methods
    bool setup_command_resources();
    bool setup_render_target_heap();
    void setup_window_hook(HWND window) override;
    void remove_window_hook() override;
    HWND get_window() const override { return m_window; }
    WNDPROC get_original_wndproc() const override { return m_originalWndProc; }
    void setup_nuklear_fonts();
    void cleanup_presentation_resources();
    void cleanup_command_resources();
    void cleanup_device_resources();
    void initialize_command_queue_from_swapchain(IDXGISwapChain* swap_chain) {
        if (!m_command_queue && swap_chain) {
            mem::module hdxgi(L"dxgi.dll");
            auto result = hdxgi.scan("8B 87 ? ? ? ? 89 06 8B 87 ? ? ? ? 89 46 04 8B 87");
            if (result.cast()) {
                auto commandQueueOffset = result.self_offset(2).get<unsigned int>() - 0x8;
                auto command_queue = *reinterpret_cast<ID3D12CommandQueue**>((uintptr_t)swap_chain + commandQueueOffset);
                set_command_queue(command_queue);
                mem::d_log("[Present] CommandQueue initialized at {:#x} (offset: {:#x})",
                           reinterpret_cast<uintptr_t>(command_queue), commandQueueOffset);
            } else {
                mem::d_log("[Present] Failed to find CommandQueue pattern");
            }
        }
    }

  public:
    // Constructor
    D3D12Renderer(IDXGISwapChain* swap_chain) {
        m_swap_chain = static_cast<IDXGISwapChain3*>(swap_chain);
        mem::d_log("[Present] SwapChain initialized at {:#x}", reinterpret_cast<uintptr_t>(m_swap_chain));
        // Check if this is actually D3D12
        mem::module d3d12(L"d3d12.dll");
        if (d3d12.loaded() && d3d12.get_export("D3D12CreateDevice")) {
            mem::d_log("[Present] Detected: Application is likely using D3D12");
        } else {
            mem::d_log("[Present] WARNING: Application appears to be using D3D11, not D3D12!");
            mem::d_log("[Present] This D3D12 hook may not work properly.");
        }
        initialize_command_queue_from_swapchain(swap_chain);
    }
    ~D3D12Renderer() {
        shutdown();
    }

    // Thread-safe access
    std::recursive_mutex& mutex() {
        return m_mutex;
    }

    // SwapChain management
    void set_swap_chain(IDXGISwapChain3* swap_chain) {
        m_swap_chain = swap_chain;
    }
    void set_command_queue(ID3D12CommandQueue* command_queue) {
        m_command_queue = command_queue;
    }

    // Resource access
    IDXGISwapChain3* swap_chain() const {
        return m_swap_chain;
    }
    ID3D12CommandQueue* command_queue() const {
        return m_command_queue;
    }
    nk_context* nuklear_context() const override {
        return m_nk_ctx;
    }
    bool is_initialized() const override {
        return m_swap_chain && m_command_queue;
    }

    bool is_shutdown() const override {
        return m_shutdown;
    }

    // Presentation management
    void release_swap_chain_buffers() override;
    void get_swap_chain_buffers(UINT width = 0, UINT height = 0) override;

    // Rendering pipeline
    void render() override;
    void draw() override;
    bool initialize() override;
    void start_input();
    void shutdown() override;

    // Screenshot functionality
    bool take_screenshot(const std::string& filename = "") override;

    // Version info
    D3DVersion get_version() const override { return D3DVersion::D3D12; }

    // Input handling
    LRESULT wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
};
