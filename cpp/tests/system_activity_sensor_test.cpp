#include "core/system_activity_sensor.hpp"

#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <vector>

using eu_digital::CanonicalEvent;
using eu_digital::CapabilityRegistry;
using eu_digital::CapabilityState;
using eu_digital::ModuleLifecycleManager;
using eu_digital::ProcessInfo;
using eu_digital::SystemActivityAdapter;
using eu_digital::SystemActivityConfig;
using eu_digital::SystemActivityPlugin;
using eu_digital::SystemActivitySensor;
using eu_digital::SystemActivitySnapshot;
using eu_digital::WindowsSystemActivityAdapter;
using eu_digital::WindowInfo;

class FakeSystemActivityAdapter final : public SystemActivityAdapter {
public:
    std::vector<SystemActivitySnapshot> snapshots;
    std::size_t next_snapshot{0};
    bool fail_capture{false};
    bool reconnect_allowed{true};
    int reconnect_calls{0};
    std::string error{"permission denied"};

    bool capture(SystemActivitySnapshot& output) override {
        if (fail_capture || next_snapshot >= snapshots.size()) return false;
        output = snapshots[next_snapshot++];
        return true;
    }

    bool reconnect() override {
        ++reconnect_calls;
        if (!reconnect_allowed) return false;
        fail_capture = false;
        return true;
    }

    std::string last_error() const override { return error; }
};

SystemActivitySnapshot snapshot(WindowInfo active, std::map<std::uint32_t, ProcessInfo> processes) {
    return SystemActivitySnapshot{std::move(active), std::move(processes)};
}

int main() {
    FakeSystemActivityAdapter adapter;
    adapter.snapshots.push_back(snapshot(
        {10, "editor.exe", "Editor"},
        {{10, {10, "editor.exe"}}, {20, {20, "worker.exe"}}}));
    adapter.snapshots.push_back(snapshot(
        {30, "browser.exe", "Browser"},
        {{10, {10, "editor.exe"}}, {30, {30, "browser.exe"}}}));

    std::vector<CanonicalEvent> events;
    SystemActivitySensor sensor(adapter, [&](const CanonicalEvent& event) { events.push_back(event); });
    assert(sensor.descriptor().capability_id == "system.activity");
    assert(sensor.descriptor().provides_operation("observe.system_activity"));

    assert(sensor.poll());
    assert(events.empty());
    assert(sensor.poll());
    assert(events.size() == 3);
    assert(events[0].event_type == "system.window_focus_changed");
    assert(events[1].event_type == "system.process_started");
    assert(events[2].event_type == "system.process_ended");
    assert(events[0].payload.find("browser.exe") != std::string::npos);
    assert(events[1].payload.find("browser.exe") != std::string::npos);
    assert(events[2].payload.find("worker.exe") != std::string::npos);
    assert(events[0].payload.find("window_title\":\"\"") != std::string::npos);
    assert(events[0].payload.find("text_content_observed\":false") != std::string::npos);
    assert(events[0].payload.find("application_category\":\"browser\"") != std::string::npos);
    assert(sensor.health().available);
    assert(sensor.health().average_cpu_percent <= sensor.config().cpu_budget_percent);
    assert(sensor.health_check());

    FakeSystemActivityAdapter plugin_adapter;
    plugin_adapter.snapshots.push_back(
        snapshot({50, "plugin.exe", "Plugin"}, {{50, {50, "plugin.exe"}}}));
    SystemActivityPlugin plugin(plugin_adapter, [&](const CanonicalEvent&) {});
    CapabilityRegistry registry;
    ModuleLifecycleManager lifecycle(registry);
    assert(lifecycle.install(plugin));
    assert(registry.record("windows.system_activity").state.state == CapabilityState::available);

    FakeSystemActivityAdapter reconnecting;
    reconnecting.fail_capture = true;
    reconnecting.snapshots.push_back(snapshot({40, "terminal.exe", "Terminal"}, {{40, {40, "terminal.exe"}}}));
    SystemActivitySensor reconnecting_sensor(reconnecting, [&](const CanonicalEvent&) {});
    assert(reconnecting_sensor.poll());
    assert(reconnecting.reconnect_calls == 1);
    assert(reconnecting_sensor.health().available);

    FakeSystemActivityAdapter denied;
    denied.fail_capture = true;
    denied.reconnect_allowed = false;
    SystemActivitySensor denied_sensor(denied, [&](const CanonicalEvent&) {});
    assert(!denied_sensor.poll());
    assert(!denied_sensor.health().available);
    assert(denied_sensor.health().consecutive_failures == 1);
    assert(denied_sensor.health().permission_denied);

    eu_digital::ObservationPrivacyPolicy restricted_policy;
    restricted_policy.allowlist = {"editor.exe"};
    SystemActivityConfig restricted_config;
    restricted_config.privacy_policy = restricted_policy;
    FakeSystemActivityAdapter restricted;
    restricted.snapshots.push_back(snapshot(
        {10, "password-manager.exe", "Vault"},
        {{10, {10, "password-manager.exe"}}, {20, {20, "editor.exe"}}}));
    std::vector<CanonicalEvent> restricted_events;
    SystemActivitySensor restricted_sensor(
        restricted, [&](const CanonicalEvent& event) { restricted_events.push_back(event); }, restricted_config);
    assert(restricted_sensor.poll());
    assert(restricted_events.empty());
    assert(restricted_sensor.health().suppressed_observations > 0);
    assert(restricted_sensor.health().last_suppression_reason == "application_denylist");

    SystemActivityConfig paused_config;
    paused_config.privacy_policy.global_pause = true;
    FakeSystemActivityAdapter paused_adapter;
    std::vector<CanonicalEvent> paused_events;
    SystemActivitySensor paused_sensor(
        paused_adapter, [&](const CanonicalEvent& event) { paused_events.push_back(event); }, paused_config);
    assert(paused_sensor.poll());
    assert(paused_events.empty());
    assert(paused_sensor.health().paused);

    WindowsSystemActivityAdapter default_windows_adapter;
    assert(!default_windows_adapter.captures_window_title());
    WindowsSystemActivityAdapter explicit_title_adapter(true);
    assert(explicit_title_adapter.captures_window_title());
}
