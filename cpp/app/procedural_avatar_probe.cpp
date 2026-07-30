#include "core/digest.hpp"
#include "core/procedural_avatar.hpp"

#include <fstream>
#include <iostream>
#include <string>

namespace {

std::string frame_json(const eu_digital::AvatarFrame& frame) {
    std::size_t nonzero = 0;
    for (const auto pixel : frame.rgba) {
        if (pixel != 0) ++nonzero;
    }
    const std::string bytes(reinterpret_cast<const char*>(frame.rgba.data()), frame.rgba.size() * sizeof(std::uint32_t));
    return "{\"blocks_work\":false,\"captures_input\":false,\"has_focus\":false,\"height\":" +
        std::to_string(frame.height) + ",\"model_required\":false,\"nonzero_pixel_count\":" +
        std::to_string(nonzero) + ",\"pixel_count\":" + std::to_string(frame.rgba.size()) +
        ",\"pixels_sha256\":\"" + eu_digital::digest::hex(eu_digital::digest::sha256(bytes)) +
        "\",\"reason\":\"" + frame.reason + "\",\"rendered\":" + (frame.rendered ? "true" : "false") +
        ",\"schema_version\":\"1.0\",\"source\":\"procedural\",\"width\":" +
        std::to_string(frame.width) + "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "usage: procedural_avatar_probe [report-path]\n";
        return 2;
    }
    try {
        eu_digital::AvatarPresentationProfile profile;
        eu_digital::AvatarRenderControls controls;
        controls.consent_granted = true;
        eu_digital::ProceduralAvatarRenderer renderer(
            profile,
            {"probe", eu_digital::AvatarViewKind::question, std::string("notice-probe")},
            controls,
            {1, 0});
        const auto frame = renderer.render(64, 64, 0);
        const auto json = frame_json(frame);
        if (argc == 2) {
            std::ofstream report(argv[1], std::ios::binary);
            if (!report) {
                std::cerr << "cannot write report: " << argv[1] << '\n';
                return 1;
            }
            report << json;
        }
        std::cout << json;
        return frame.rendered && !frame.rgba.empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "procedural_avatar_probe_error: " << error.what() << '\n';
        return 1;
    }
}
