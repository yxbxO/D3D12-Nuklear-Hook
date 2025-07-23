#pragma once

struct nk_context;

struct InputEvent {
    HWND hwnd;
    UINT msg;
    WPARAM wparam;
    LPARAM lparam;
};

enum class D3DVersion {
    D3D11,
    D3D12,
    Unknown
};

class ID3DRenderer {
protected:
    std::queue<InputEvent> m_inputQueue;
    std::recursive_mutex m_mutex;
    HWND m_window = nullptr;
    bool m_initialized = false;
    bool m_shutdown = false;
public:
    virtual ~ID3DRenderer() = default;
    virtual bool initialize() = 0;
    virtual void render() = 0;
    virtual void draw() = 0;
    virtual void shutdown() = 0;
    virtual void release_swap_chain_buffers() = 0;
    virtual void get_swap_chain_buffers(unsigned int width = 0, unsigned int height = 0) = 0;
    virtual bool take_screenshot(const std::string& filename = "") = 0;
    virtual bool is_initialized() const = 0;
    virtual bool is_shutdown() const = 0;
    virtual nk_context* nuklear_context() const = 0;
    virtual D3DVersion get_version() const = 0;

    // Add input handling interface
    virtual LRESULT wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;

    // Window proc management
    virtual void setup_window_hook(HWND window) = 0;
    virtual void remove_window_hook() = 0;
    virtual HWND get_window() const = 0;
    virtual WNDPROC get_original_wndproc() const = 0;
}; 

inline std::unique_ptr<ID3DRenderer> g_renderer;

inline LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_renderer) {
        return g_renderer->wndproc(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}