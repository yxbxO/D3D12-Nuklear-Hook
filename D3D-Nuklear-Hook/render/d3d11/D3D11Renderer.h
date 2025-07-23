#pragma once

struct nk_context;

class D3D11Renderer : public ID3DRenderer {
private:
    IDXGISwapChain3* m_swap_chain = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    ID3D11RenderTargetView* m_rt_view = nullptr;
    nk_context* m_nk_ctx = nullptr;
    WNDPROC m_originalWndProc = nullptr;

public:
    D3D11Renderer(IDXGISwapChain* swap_chain);
    ~D3D11Renderer();

    // ID3DRenderer interface
    bool initialize() override;
    void render() override;
    void draw() override;
    void shutdown() override;
    void release_swap_chain_buffers() override;
    void get_swap_chain_buffers(unsigned int width = 0, unsigned int height = 0) override;
    bool take_screenshot(const std::string& filename = "") override;
    bool is_initialized() const override;
    bool is_shutdown() const override;
    nk_context* nuklear_context() const override;
    D3DVersion get_version() const override { return D3DVersion::D3D11; }
    void setup_nuklear_fonts();
    LRESULT wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    void setup_window_hook(HWND window) override;
    void remove_window_hook() override;
    HWND get_window() const override { return m_window; }
    WNDPROC get_original_wndproc() const override { return m_originalWndProc; }

    void start_input();
};