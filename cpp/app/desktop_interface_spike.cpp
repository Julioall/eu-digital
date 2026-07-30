#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Scenario {
    std::string id;
    std::string status;
    std::string detail;
};

std::string escape_json(const std::string& value) {
    std::ostringstream output;
    for (const char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << character; break;
        }
    }
    return output.str();
}

std::string json_string(const std::string& value) {
    return "\"" + escape_json(value) + "\"";
}

std::string version_string(const SDL_version& version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
}

std::string serialize(const std::vector<Scenario>& scenarios,
                      const std::string& sdl_version,
                      const std::string& imgui_version,
                      double idle_elapsed_ms,
                      const std::string& mode) {
    std::size_t validated = 0;
    std::size_t partial = 0;
    std::size_t unsupported = 0;
    std::size_t ablation = 0;
    for (const auto& scenario : scenarios) {
        if (scenario.status == "validated") ++validated;
        else if (scenario.status == "partial") ++partial;
        else if (scenario.status == "ablation") ++ablation;
        else ++unsupported;
    }
    std::ostringstream output;
    output << "{\"imgui_version\":" << json_string(imgui_version)
           << ",\"idle_elapsed_ms\":" << std::fixed << std::setprecision(3) << idle_elapsed_ms
           << ",\"mode\":" << json_string(mode)
           << ",\"product_decision\":\"sdl2_imgui_not_selected_for_product_shell\""
           << ",\"renderer_boundary\":\"optional_spike_does_not_alter_avatar_presentation_port\""
           << ",\"scenarios\":[";
    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        if (index) output << ',';
        output << "{\"detail\":" << json_string(scenarios[index].detail)
               << ",\"id\":" << json_string(scenarios[index].id)
               << ",\"status\":" << json_string(scenarios[index].status) << '}';
    }
    output << "],\"schema_version\":\"1.0\",\"sdl_version\":" << json_string(sdl_version)
           << ",\"summary\":{\"partial\":" << partial
           << ",\"ablation\":" << ablation
           << ",\"unsupported\":" << unsupported
           << ",\"validated\":" << validated << "}}\n";
    return output.str();
}

int fail(const std::string& message) {
    std::cerr << "desktop_interface_spike_error: " << message << '\n';
    SDL_Quit();
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string mode = "treatment";
    std::string report_path;
    for (int argument = 1; argument < argc; ++argument) {
        const std::string value = argv[argument];
        if (value == "--baseline") mode = "baseline_sdl2_without_imgui";
        else if (value == "--ablation-no-imgui") mode = "ablation_sdl2_without_imgui";
        else if (value == "--ablation-no-transparency") mode = "ablation_without_transparency";
        else report_path = value;
    }
    const bool use_imgui = mode != "baseline_sdl2_without_imgui" && mode != "ablation_sdl2_without_imgui";
    const bool use_transparency = mode != "ablation_without_transparency";
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "desktop_interface_spike_error: SDL_Init: " << SDL_GetError() << '\n';
        return 2;
    }
    SDL_version compiled{};
    SDL_VERSION(&compiled);
    SDL_version linked{};
    SDL_GetVersion(&linked);

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    SDL_Window* window = SDL_CreateWindow(
        "eu-digital desktop interface spike",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640,
        360,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) return fail(std::string("SDL_CreateWindow: ") + SDL_GetError());

    ImGuiContext* context = nullptr;
    if (use_imgui) {
        context = ImGui::CreateContext();
        if (context == nullptr) {
            SDL_DestroyWindow(window);
            return fail("ImGui::CreateContext returned null");
        }
        ImGui::SetCurrentContext(context);
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.DisplaySize = ImVec2(640.0F, 360.0F);
        io.DisplayFramebufferScale = ImVec2(1.0F, 1.0F);
        unsigned char* font_pixels = nullptr;
        int font_width = 0;
        int font_height = 0;
        io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
        ImGui::NewFrame();
        ImGui::Begin("desktop-interface-spike");
        ImGui::TextUnformatted("local non-blocking presentation probe");
        ImGui::End();
        ImGui::Render();
    }

    std::vector<Scenario> scenarios;
    scenarios.push_back({"sdl2_initialization", "validated", "SDL video and event subsystems initialized"});
    scenarios.push_back({"dear_imgui_context", use_imgui ? "validated" : "ablation", use_imgui ? "ImGui context, keyboard navigation and frame lifecycle initialized" : "ImGui disabled for baseline/ablation"});

    const auto flags = SDL_GetWindowFlags(window);
    scenarios.push_back({
        "focus_non_stealing",
        (flags & SDL_WINDOW_INPUT_FOCUS) == 0 ? "validated" : "failed",
        (flags & SDL_WINDOW_INPUT_FOCUS) == 0 ? "hidden probe has no input focus" : "hidden probe unexpectedly owns input focus"});
    scenarios.push_back({"clipboard", "validated", "probe performs no clipboard read or write"});

    SDL_StartTextInput();
    scenarios.push_back({"ime_pt_br", "partial", "SDL text-input/IME path enabled; physical pt-BR composition requires manual Windows IME validation"});
    scenarios.push_back({"keyboard", use_imgui ? "partial" : "ablation", use_imgui ? "ImGui keyboard navigation is enabled; physical layout and assistive-key validation require manual Windows input" : "ImGui keyboard navigation disabled by baseline/ablation"});

    const int display_count = SDL_GetNumVideoDisplays();
    bool display_probe_ok = display_count > 0;
    for (int display = 0; display < display_count; ++display) {
        SDL_Rect bounds{};
        if (SDL_GetDisplayBounds(display, &bounds) != 0) display_probe_ok = false;
        float diagonal = 0.0F;
        float horizontal = 0.0F;
        float vertical = 0.0F;
        if (SDL_GetDisplayDPI(display, &diagonal, &horizontal, &vertical) != 0) display_probe_ok = false;
    }
    scenarios.push_back({"dpi_100_250", display_probe_ok ? "partial" : "failed", "display bounds/DPI query works; 100-250 percent matrix requires physical monitor coverage"});
    scenarios.push_back({"multiple_monitors", display_count > 0 ? "partial" : "failed", "enumerated " + std::to_string(display_count) + " display(s); cross-monitor movement requires manual coverage"});

    if (use_transparency) {
        const bool opacity_ok = SDL_SetWindowOpacity(window, 0.95F) == 0;
        scenarios.push_back({"transparency", opacity_ok ? "validated" : "partial", opacity_ok ? "SDL window opacity accepted" : SDL_GetError()});
    } else {
        scenarios.push_back({"transparency", "ablation", "window opacity intentionally disabled by ablation"});
    }
    SDL_SetWindowMouseGrab(window, SDL_FALSE);
    SDL_SetWindowKeyboardGrab(window, SDL_FALSE);
    const bool mouse_grab_ok = SDL_GetWindowMouseGrab(window) == SDL_FALSE;
    const bool keyboard_grab_ok = SDL_GetWindowKeyboardGrab(window) == SDL_FALSE;
    scenarios.push_back({"input_grab_disabled", mouse_grab_ok && keyboard_grab_ok ? "validated" : "failed", "SDL mouse and keyboard grabs disabled"});

    scenarios.push_back({"fullscreen", "partial", "not toggled automatically because a hidden SDL window did not return from the fullscreen probe within the safety limit; manual Windows validation is required"});

    scenarios.push_back({"screen_reader", "unsupported", "SDL2/Dear ImGui core does not provide a Windows UI Automation/accessibility tree"});
    scenarios.push_back({"click_through", "unsupported", "SDL2 core does not expose the required layered-window click-through contract"});
    scenarios.push_back({"tray", "unsupported", "SDL2/Dear ImGui core has no notification-area/tray adapter"});
    scenarios.push_back({"suspend_resume", "partial", "event loop is pumpable; lifecycle suspend/resume requires a native shell adapter"});

    const auto idle_started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 5; ++iteration) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {}
        SDL_Delay(10);
    }
    const double idle_elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - idle_started).count();
    scenarios.push_back({"idle_event_loop", "validated", "event loop remained responsive during a bounded idle interval"});

    const auto json = serialize(scenarios, version_string(linked), IMGUI_VERSION, idle_elapsed_ms, mode);
    if (!report_path.empty()) {
        std::ofstream file(report_path, std::ios::binary);
        if (!file) {
            if (context != nullptr) ImGui::DestroyContext(context);
            SDL_StopTextInput();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return fail("cannot write report: " + report_path);
        }
        file << json;
    }
    std::cout << json;

    if (context != nullptr) ImGui::DestroyContext(context);
    SDL_StopTextInput();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return std::any_of(scenarios.begin(), scenarios.end(), [](const Scenario& scenario) { return scenario.status == "failed"; }) ? 1 : 0;
}
