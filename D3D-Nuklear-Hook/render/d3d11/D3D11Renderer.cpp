#include "../../pch.h"

#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_ASSERT

#include "../Nuklear/nuklear.h"

#define NK_D3D11_IMPLEMENTATION
#include "../Nuklear/nuklear_d3d11.h"


#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

static bool menu_is_open = true;

/**
 * @brief Constructs a D3D11Renderer with the given swap chain.
 * @param swap_chain Pointer to IDXGISwapChain
 */
D3D11Renderer::D3D11Renderer(IDXGISwapChain* swap_chain)
{
    m_swap_chain = static_cast<IDXGISwapChain3 * >(swap_chain);
    m_device = nullptr;
    m_context = nullptr;
    m_rt_view = nullptr;
    m_nk_ctx = nullptr;
    m_originalWndProc = nullptr;
    m_initialized = false;
    m_shutdown = false;
}

/**
 * @brief Destructor. Ensures proper shutdown and resource release.
 */
D3D11Renderer::~D3D11Renderer() {
    shutdown();
}

/**
 * @brief Initializes the D3D11 renderer, device, context, Nuklear, and window hooks.
 * @return true if initialization succeeded, false otherwise
 */
bool D3D11Renderer::initialize() {
    if (m_shutdown) return false;
    if (m_initialized) return true;
    if (!m_swap_chain) return false;

    // Get device from swap chain
    HRESULT hr = m_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&m_device);
    if (FAILED(hr) || !m_device) return false;

    m_device->GetImmediateContext(&m_context);
    if (!m_context) return false;

    DXGI_SWAP_CHAIN_DESC sd{};
    hr = m_swap_chain->GetDesc(&sd);
    if (FAILED(hr)) return false;
    m_window = sd.OutputWindow;

    // Create render target view
    ID3D11Texture2D* back_buffer = nullptr;
    hr = m_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) return false;

    hr = m_device->CreateRenderTargetView(back_buffer, nullptr, &m_rt_view);
    back_buffer->Release();
    if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC1 sd1{};
    hr = m_swap_chain->GetDesc1(&sd1);

    mem::d_log("[D3D11Renderer] Window: {}x{} handle: {:#x}", sd1.Width, sd1.Height,
        reinterpret_cast<uintptr_t>(sd.OutputWindow));

    // Initialize Nuklear
    m_nk_ctx = nk_d3d11_init(m_device, sd1.Height, sd1.Width, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);
    if (!m_nk_ctx) return false;

    // Setup window proc hook
    setup_window_hook(m_window);

    // Setup fonts
    setup_nuklear_fonts();

    m_initialized = true;
    m_shutdown = false;
    return true;
}

/**
 * @brief Renders the Nuklear GUI using D3D11.
 */
void D3D11Renderer::render() {
    if (!m_initialized || !m_nk_ctx || !m_rt_view) return;
    m_mutex.lock();
    m_context->OMSetRenderTargets(1, &m_rt_view, nullptr);
    nk_d3d11_render(m_context, NK_ANTI_ALIASING_ON);
    m_mutex.unlock();
}

/**
 * @brief Begins Nuklear input frame and processes queued input events.
 */
void D3D11Renderer::start_input() {
    // Begin Nuklear input frame
    nk_input_begin(m_nk_ctx);

    // Process all queued input events
    while (!m_inputQueue.empty()) {
        const auto& evt = m_inputQueue.front();
        nk_d3d11_handle_event(evt.hwnd, evt.msg, evt.wparam, evt.lparam);
        m_inputQueue.pop();
    }

    // End Nuklear input frame
    nk_input_end(m_nk_ctx);
}

/**
 * @brief Draws the GUI and handles input. Initializes if needed.
 */
void D3D11Renderer::draw() {
    if (!initialize()) return;


    start_input();

    if (!menu_is_open)
        return;

    m_mutex.lock();

    /* GUI */
    if (nk_begin(m_nk_ctx, "Demo", nk_rect(50, 50, 230, 250),
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE |
        NK_WINDOW_TITLE)) {
        enum {
            EASY,
            HARD
        };
        static int op = EASY;
        static int property = 20;

        nk_layout_row_static(m_nk_ctx, 30, 80, 1);
        if (nk_button_label(m_nk_ctx, "button")) {
            // Button pressed - implement functionality here
        }
        nk_layout_row_dynamic(m_nk_ctx, 30, 2);
        if (nk_option_label(m_nk_ctx, "easy", op == EASY))
            op = EASY;
        if (nk_option_label(m_nk_ctx, "hard", op == HARD))
            op = HARD;
        nk_layout_row_dynamic(m_nk_ctx, 22, 1);
        nk_property_int(m_nk_ctx, "Compression:", 0, &property, 100, 10, 1);

        nk_layout_row_dynamic(m_nk_ctx, 20, 1);
        nk_label(m_nk_ctx, "background:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(m_nk_ctx, 25, 1);

        static struct nk_colorf bg;
        if (nk_combo_begin_color(m_nk_ctx, nk_rgb_cf(bg),
            nk_vec2(nk_widget_width(m_nk_ctx), 400))) {
            nk_layout_row_dynamic(m_nk_ctx, 120, 1);
            bg = nk_color_picker(m_nk_ctx, bg, NK_RGBA);
            nk_layout_row_dynamic(m_nk_ctx, 25, 1);
            bg.r = nk_propertyf(m_nk_ctx, "#R:", 0, bg.r, 1.0f, 0.01f, 0.005f);
            bg.g = nk_propertyf(m_nk_ctx, "#G:", 0, bg.g, 1.0f, 0.01f, 0.005f);
            bg.b = nk_propertyf(m_nk_ctx, "#B:", 0, bg.b, 1.0f, 0.01f, 0.005f);
            bg.a = nk_propertyf(m_nk_ctx, "#A:", 0, bg.a, 1.0f, 0.01f, 0.005f);
            nk_combo_end(m_nk_ctx);
        }

        nk_end(m_nk_ctx);
    }

    m_mutex.unlock();
}

/**
 * @brief Sets up Nuklear font atlas for the GUI.
 */
void D3D11Renderer::setup_nuklear_fonts() {
    struct nk_font_atlas* atlas;
    nk_d3d11_font_stash_begin(&atlas);
    nk_d3d11_font_stash_end();
}

/**
 * @brief Releases the swap chain's render target view.
 */
void D3D11Renderer::release_swap_chain_buffers() {
    if (m_rt_view) {
        m_rt_view->Release();
        m_rt_view = nullptr;
    }
}

/**
 * @brief Recreates swap chain buffers and render target view.
 * @param width New width (optional)
 * @param height New height (optional)
 */
void D3D11Renderer::get_swap_chain_buffers(unsigned int width, unsigned int height) {
    // Recreate RTV
    if (!m_swap_chain || !m_device) return;
    ID3D11Texture2D* back_buffer = nullptr;
    HRESULT hr = m_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) return;
    if (m_rt_view) m_rt_view->Release();
    hr = m_device->CreateRenderTargetView(back_buffer, nullptr, &m_rt_view);
    back_buffer->Release();
}

/**
 * @brief Shuts down the renderer and releases all resources.
 */
void D3D11Renderer::shutdown() {
    if (m_shutdown) return;
    m_shutdown = true;
    m_initialized = false;
    remove_window_hook();
    if (m_nk_ctx) {
        nk_d3d11_shutdown();
        m_nk_ctx = nullptr;
    }
    if (m_rt_view) {
        m_rt_view->Release();
        m_rt_view = nullptr;
    }
    if (m_context) {
        m_context->ClearState();
        m_context = nullptr;
    }
}

/**
 * @brief Takes a screenshot (not implemented).
 * @param filename Output filename (optional)
 * @return Always false (not implemented)
 */
bool D3D11Renderer::take_screenshot(const std::string& filename) {
    // Not implemented
    return false;
}

/**
 * @brief Checks if the renderer is initialized.
 * @return true if initialized
 */
bool D3D11Renderer::is_initialized() const {
    return m_initialized;
}

/**
 * @brief Checks if the renderer is shut down.
 * @return true if shut down
 */
bool D3D11Renderer::is_shutdown() const {
    return m_shutdown;
}

/**
 * @brief Returns the Nuklear context pointer.
 * @return Pointer to nk_context
 */
nk_context* D3D11Renderer::nuklear_context() const {
    return m_nk_ctx;
}

/**
 * @brief Window procedure for handling input and toggling menu.
 * @param hWnd Window handle
 * @param msg Message
 * @param wParam WPARAM
 * @param lParam LPARAM
 * @return LRESULT from original or handled
 */
LRESULT D3D11Renderer::wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_nk_ctx)
        return CallWindowProcW(m_originalWndProc, hWnd, msg, wParam, lParam);

    m_inputQueue.push({ hWnd, msg, wParam, lParam });

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        menu_is_open = !menu_is_open;
    }

    bool shouldCapture = menu_is_open && nk_item_is_any_active(m_nk_ctx);
    if (shouldCapture && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN ||
                          msg == WM_RBUTTONUP || msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
                          msg == WM_MOUSEWHEEL || msg == WM_MOUSEMOVE || msg == WM_KEYDOWN ||
                          msg == WM_KEYUP || msg == WM_CHAR)) {
        return 0;
    }
    return CallWindowProcW(m_originalWndProc, hWnd, msg, wParam, lParam);
}

/**
 * @brief Installs a window procedure hook for input handling.
 * @param window Window handle
 */
void D3D11Renderer::setup_window_hook(HWND window) {
    if (!window) return;
    if (m_window && m_window != window) {
        remove_window_hook();
    }
    m_window = window;
    m_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc))
    );
}

/**
 * @brief Removes the window procedure hook if installed.
 */
void D3D11Renderer::remove_window_hook() {
    if (m_window && m_originalWndProc) {
        SetWindowLongPtr(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalWndProc));
        m_window = nullptr;
        m_originalWndProc = nullptr;
    }
}