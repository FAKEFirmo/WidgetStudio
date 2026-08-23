#include "persistence/SceneJsonCodec.h"
#include "persistence/SceneStore.h"
#include "scene/WidgetScene.h"
#include "widgets/WidgetRegistry.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <windows.h>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TestWidget final : public ws::IWidget {
public:
    void Render(const ws::WidgetRenderContext&) const override {}
    [[nodiscard]] std::span<const ws::WidgetSettingDefinition> Settings() const noexcept override { return {}; }
    [[nodiscard]] ws::WidgetState SaveState() const override { return state_; }
    void RestoreState(const ws::WidgetState& state) override { state_ = state; }

private:
    ws::WidgetState state_;
};

ws::WidgetRegistry CreateRegistry() {
    ws::WidgetRegistry registry;
    Require(registry.Register(ws::WidgetDescriptor{
        .typeId = "test",
        .displayName = L"Test",
        .description = L"Test widget",
        .defaultGridSize = ws::GridSize{3, 2},
        .minimumGridSize = ws::GridSize{1, 1},
        .maximumGridSize = ws::GridSize{6, 4},
        .capabilities = ws::WidgetCapability::Configurable,
        .factory = [] { return std::make_unique<TestWidget>(); },
    }), "registry should accept valid descriptor");
    return registry;
}

void TestRegistryAndPlacement() {
    ws::WidgetRegistry registry = CreateRegistry();
    Require(!registry.Register(ws::WidgetDescriptor{
        .typeId = "test", .displayName = L"Duplicate", .description = L"Duplicate",
        .defaultGridSize = {1, 1}, .minimumGridSize = {1, 1}, .maximumGridSize = {1, 1},
        .factory = [] { return std::make_unique<TestWidget>(); },
    }), "registry should reject duplicate type IDs");

    ws::WidgetScene scene(registry);
    std::set<std::string> identifiers;
    for (int index = 0; index < 5; ++index) {
        ws::WidgetInstance* widget = scene.CreateWidget("test", L"DISPLAY-A");
        Require(widget != nullptr, "widget creation should succeed");
        Require(identifiers.insert(widget->instanceId).second, "instance IDs must be unique");
        if (index < 4) {
            Require(widget->grid.column == index * 3 && widget->grid.row == 0,
                "placement should scan left-to-right");
        } else {
            Require(widget->grid.column == 0 && widget->grid.row == 2,
                "placement should continue top-to-bottom");
        }
        Require(scene.PrimarySelection() == widget->instanceId, "new widget should be primary selection");
    }

    const std::string sourceId = scene.Widgets().front().instanceId;
    ws::WidgetInstance* duplicate = scene.DuplicateWidget(sourceId);
    Require(duplicate != nullptr && duplicate->instanceId != sourceId, "duplicate should receive a new ID");
    Require(scene.SetWidgetLocked(duplicate->instanceId, true) && duplicate->locked,
        "generic lock operation should work");
    Require(scene.RemoveWidget(duplicate->instanceId), "generic removal should work");
}

ws::WidgetPersistenceRecord ExampleRecord() {
    ws::WidgetPersistenceRecord record{};
    record.instanceId = "widget-example";
    record.typeId = "test";
    record.monitorId = L"\\\\.\\DISPLAY1 – primary";
    record.layoutMode = ws::LayoutMode::Free;
    record.grid = {2, 3, 4, 2};
    record.free = {12.5f, -4.0f, 420.0f, 180.0f};
    record.locked = true;
    record.contentScale = 1.25f;
    record.appearance.mode = ws::AppearanceMode::Light;
    record.appearance.glassEnabled = false;
    record.appearance.opacity = 0.75f;
    record.appearance.blurRadius = 12.0f;
    record.appearance.cornerRadius = 24.0f;
    record.widgetState.emplace(L"caption", L"Unicode ✓ and \"quotes\"");
    return record;
}

void TestSerialization() {
    const ws::WidgetPersistenceRecord expected = ExampleRecord();
    const std::string json = ws::SceneJsonCodec::Encode({expected});
    std::wstring error;
    const auto decoded = ws::SceneJsonCodec::Decode(json, error);
    Require(decoded.has_value() && decoded->schemaVersion == ws::SceneJsonCodec::kCurrentSchemaVersion,
        "encoded scene should decode");
    Require(decoded->widgets.size() == 1, "decoded scene should contain one widget");
    const auto& actual = decoded->widgets.front();
    Require(actual.instanceId == expected.instanceId && actual.typeId == expected.typeId,
        "stable IDs should round-trip");
    Require(actual.monitorId == expected.monitorId && actual.layoutMode == expected.layoutMode,
        "monitor and layout mode should round-trip");
    Require(actual.widgetState == expected.widgetState, "widget state should round-trip");
    Require(std::abs(actual.contentScale - expected.contentScale) < 0.0001f,
        "content scale should round-trip");

    Require(!ws::SceneJsonCodec::Decode("{\"schemaVersion\":1,\"widgets\":[", error),
        "malformed JSON should be rejected");
    Require(!ws::SceneJsonCodec::Decode("{\"schemaVersion\":99,\"widgets\":[]}", error),
        "unknown schema versions should be rejected safely");
}

void TestAtomicStoreAndRestore() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory = std::filesystem::temp_directory_path() /
        (L"WidgetStudioTests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(suffix));
    std::filesystem::create_directories(directory);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};

    const std::filesystem::path config = directory / L"scene.json";
    ws::SceneStore store(config);
    std::wstring error;
    Require(store.Save({ExampleRecord()}, error), "first atomic save should succeed");
    Require(store.Load().status == ws::SceneLoadStatus::Loaded, "saved scene should load");

    ws::WidgetPersistenceRecord changed = ExampleRecord();
    changed.locked = false;
    Require(store.Save({changed}, error), "replacement save should succeed");
    Require(std::filesystem::exists(config.wstring() + L".bak"), "replacement should retain a backup");
    changed.contentScale = 1.5f;
    Require(store.Save({changed}, error), "repeated replacement save should succeed");

    ws::WidgetRegistry registry = CreateRegistry();
    ws::WidgetScene scene(registry);
    const ws::SceneLoadResult loaded = store.Load();
    Require(loaded.snapshot.size() == 1 && scene.RestoreWidget(loaded.snapshot.front()) != nullptr,
        "loaded widget should restore through the registry");
    Require(scene.Widgets().front().instanceId == changed.instanceId,
        "restore should preserve instance ID");
}

} // namespace

int main() {
    try {
        TestRegistryAndPlacement();
        TestSerialization();
        TestAtomicStoreAndRestore();
        std::cout << "WidgetStudio logic tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WidgetStudio logic test failed: " << error.what() << '\n';
        return 1;
    }
}
