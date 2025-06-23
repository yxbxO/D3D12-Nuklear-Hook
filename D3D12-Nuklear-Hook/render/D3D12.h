#pragma once

// forward declare.
struct nk_context;

class D3D12Renderer {
  private:
    // Private constructor for singleton
    D3D12Renderer() = default;

    // Delete copy/move constructors and assignment operators
    D3D12Renderer(const D3D12Renderer&) = delete;
    D3D12Renderer(D3D12Renderer&&) = delete;
    D3D12Renderer& operator=(const D3D12Renderer&) = delete;
    D3D12Renderer& operator=(D3D12Renderer&&) = delete;

    // Internal state
    mutable std::recursive_mutex m_mutex;

    // Core swap chain and presentation resources (managed by us)
    IDXGISwapChain3* m_swap_chain = nullptr;
    ID3D12CommandQueue* m_command_queue = nullptr;
    ID3D12DescriptorHeap* m_rtv_descriptor_heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE* m_rtv_handles = nullptr;
    ID3D12Resource** m_rtv_buffers = nullptr;
    UINT m_rtv_desc_increment = 0;
    UINT m_rtv_buffer_count = 0;

    // Nuklear context and UI state
    nk_context* m_nk_ctx = nullptr;

    // State flags
    bool m_initialized = false;
    bool m_shutdown = false;

    // Private helper methods
    bool setup_command_resources();
    bool setup_render_target_heap();
    void setup_window_hook(HWND window);
    void setup_nuklear_fonts();
    void cleanup_presentation_resources();
    void cleanup_command_resources();
    void cleanup_device_resources();

  public:
    // Singleton access
    static D3D12Renderer& get() {
        static D3D12Renderer instance;
        return instance;
    }

    // destructor
    ~D3D12Renderer() {
        shutdown();
    }

    // Thread-safe access
    std::recursive_mutex& mutex() const {
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
    nk_context* nuklear_context() const {
        return m_nk_ctx;
    }
    bool is_initialized() const {
        return m_initialized;
    }
    bool is_shutdown() const {
        return m_shutdown;
    }

    // Presentation management
    void release_swap_chain_buffers();
    void get_swap_chain_buffers(UINT width = 0, UINT height = 0);

    // Rendering pipeline
    void render();
    void draw();
    bool initialize();
    void start_input();
    void shutdown();
};

// Type alias for cleaner usage
using D3D12 = D3D12Renderer;