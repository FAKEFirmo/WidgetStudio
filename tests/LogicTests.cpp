#include "persistence/SceneJsonCodec.h"
#include "persistence/SceneStore.h"
#include "persistence/AssetLibrary.h"
#include "layout/AuthoredContentLayout.h"
#include "layout/Alignment.h"
#include "layout/OuterLayout.h"
#include "scene/WidgetScene.h"
#include "widgets/ClockWidget.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/calendar/CalendarModel.h"
#include "widgets/photo/PhotoLayout.h"
#include "windows/MonitorTopology.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
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

    ws::WidgetInstance* otherMonitor = scene.CreateWidget("test", L"DISPLAY-B");
    Require(otherMonitor != nullptr, "widget creation on a second monitor should succeed");
    ws::GridLayout grid;
    const ws::GridMetrics metrics = grid.Calculate({1200.0f, 700.0f});
    const ws::PointF sharedPoint{
        grid.RectFor(scene.Widgets().front().grid, metrics).x + 1.0f,
        grid.RectFor(scene.Widgets().front().grid, metrics).y + 1.0f,
    };
    const auto firstMonitorHit = scene.HitTest(sharedPoint, grid, metrics, L"DISPLAY-A");
    const auto secondMonitorHit = scene.HitTest(sharedPoint, grid, metrics, L"DISPLAY-B");
    Require(firstMonitorHit == scene.Widgets().front().instanceId,
        "hit testing should isolate the first monitor scene");
    Require(secondMonitorHit == otherMonitor->instanceId,
        "hit testing should isolate the second monitor scene");
}

void TestMonitorMigration() {
    ws::WidgetRegistry registry = CreateRegistry();
    ws::WidgetScene scene(registry);
    ws::WidgetInstance* missing = scene.CreateWidget("test", L"REMOVED-DISPLAY");
    ws::WidgetInstance* present = scene.CreateWidget("test", L"DISPLAY-PRIMARY");
    Require(missing && present, "monitor migration fixtures should be created");
    missing->grid = {20, 20, 20, 20};
    missing->free = {-20.0f, 900.0f, 2000.0f, 900.0f};
    const ws::MonitorTopology topology(std::vector<ws::MonitorDescriptor>{ws::MonitorDescriptor{
        .id = L"DISPLAY-PRIMARY",
        .workAreaDips = {0.0f, 0.0f, 800.0f, 600.0f},
        .pixelWidth = 1200,
        .pixelHeight = 900,
        .dpi = 144,
        .primary = true,
    }});
    Require(topology.MigrateMissingWidgets(scene, 12, 7) == 1,
        "only widgets assigned to missing monitors should migrate");
    Require(missing->monitorId == L"DISPLAY-PRIMARY" && present->monitorId == L"DISPLAY-PRIMARY",
        "missing widgets should migrate to the primary monitor");
    Require(missing->grid.column == 0 && missing->grid.row == 0 &&
        missing->grid.columnSpan == 12 && missing->grid.rowSpan == 7,
        "migrated grid geometry should clamp to the destination grid");
    Require(missing->free.x == 0.0f && missing->free.y == 0.0f &&
        missing->free.width == 800.0f && missing->free.height == 600.0f,
        "migrated free geometry should clamp to the destination work area");
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

void TestAuthoredLayout() {
    constexpr float infinity = std::numeric_limits<float>::infinity();
    const std::array profiles{
        ws::AuthoredLayoutProfile{"portrait", 0.0f, 0.90f, {240.0f, 340.0f}},
        ws::AuthoredLayoutProfile{"square", 0.90f, 1.45f, {300.0f, 300.0f}},
        ws::AuthoredLayoutProfile{"landscape", 1.45f, 2.35f, {390.0f, 215.0f}},
        ws::AuthoredLayoutProfile{"ultra-wide", 2.35f, infinity, {520.0f, 150.0f}},
    };
    Require(ws::AuthoredContentLayout::SelectProfile(profiles, 0.75f) == 0,
        "portrait profile should be selected");
    Require(ws::AuthoredContentLayout::SelectProfile(profiles, 1.0f) == 1,
        "square profile should be selected");
    Require(ws::AuthoredContentLayout::SelectProfile(profiles, 2.0f) == 2,
        "landscape profile should be selected");
    Require(ws::AuthoredContentLayout::SelectProfile(profiles, 2.5f) == 3,
        "ultra-wide profile should be selected");
    Require(ws::AuthoredContentLayout::SelectProfile(profiles, 1.43f, 2, 0.03f) == 2,
        "hysteresis should retain the current profile near a breakpoint");

    const ws::AuthoredLayoutResult fit = ws::AuthoredContentLayout::FitReference(
        {270.0f, 120.0f}, {10.0f, 20.0f, 540.0f, 300.0f});
    Require(std::abs(fit.scale - 2.0f) < 0.0001f, "fit should use one uniform scale");
    Require(std::abs(fit.renderedSize.width - 540.0f) < 0.0001f &&
        std::abs(fit.renderedSize.height - 240.0f) < 0.0001f,
        "fit should preserve reference aspect ratio");
    Require(std::abs(fit.origin.x - 10.0f) < 0.0001f && std::abs(fit.origin.y - 50.0f) < 0.0001f,
        "fit should explicitly center the rendered reference rectangle");
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

void TestClockStateAndScheduling() {
    ws::ClockWidget clock;
    clock.RestoreState({
        {L"use24Hour", L"false"},
        {L"showSeconds", L"true"},
        {L"showDivider", L"false"},
        {L"dateFormat", L"compact"},
    });
    const ws::WidgetState state = clock.SaveState();
    Require(state.at(L"use24Hour") == L"false" && state.at(L"showSeconds") == L"true" &&
        state.at(L"showDivider") == L"false" && state.at(L"dateFormat") == L"compact",
        "clock settings should round-trip through widget state");
    const auto now = std::chrono::system_clock::now();
    const auto next = clock.NextUpdateTime();
    Require(next.has_value() && *next > now && *next - now <= std::chrono::seconds(1),
        "seconds mode should schedule the next displayed-second boundary");
    Require(clock.Settings().size() == 4, "clock should expose generic setting definitions");
}

void TestCalendarModel() {
    Require(ws::CalendarModel::IsLeapYear(2000) && !ws::CalendarModel::IsLeapYear(2100),
        "Gregorian leap-year rules should be applied");
    Require(ws::CalendarModel::DaysInMonth(2024, 2) == 29 &&
        ws::CalendarModel::DayOfWeek({2026, 8, 1}) == 6,
        "month length and weekday calculations should be deterministic");

    const auto mondayGrid = ws::CalendarModel::Build(2026, 8, true, {2026, 8, 23});
    Require(mondayGrid.front().date.year == 2026 && mondayGrid.front().date.month == 7 &&
        mondayGrid.front().date.day == 27,
        "Monday-first calendar should include the correct leading adjacent-month date");
    Require(mondayGrid[27].today && mondayGrid[27].weekend,
        "today and weekend flags should reflect the represented civil date");

    const auto sundayGrid = ws::CalendarModel::Build(2026, 8, false, {2026, 8, 23});
    Require(sundayGrid.front().date.day == 26 && sundayGrid.front().date.month == 7,
        "Sunday-first calendar should shift the six-week grid correctly");
}

void TestPhotoLayoutAndAssetImport() {
    const ws::PhotoLayoutResult fill = ws::PhotoLayout::Calculate(
        {400.0f, 200.0f}, {10.0f, 20.0f, 100.0f, 100.0f}, ws::PhotoFitMode::Fill, 1.0f, 0.5f);
    Require(std::abs(fill.source.width - 200.0f) < 0.0001f &&
        std::abs(fill.source.height - 200.0f) < 0.0001f &&
        std::abs(fill.source.x - 200.0f) < 0.0001f,
        "fill mode should crop proportionally around the continuous focal point");
    const ws::PhotoLayoutResult fit = ws::PhotoLayout::Calculate(
        {400.0f, 200.0f}, {10.0f, 20.0f, 100.0f, 100.0f}, ws::PhotoFitMode::Fit, 0.5f, 0.5f);
    Require(std::abs(fit.destination.width - 100.0f) < 0.0001f &&
        std::abs(fit.destination.height - 50.0f) < 0.0001f &&
        std::abs(fit.destination.y - 45.0f) < 0.0001f,
        "fit mode should preserve aspect ratio and center the whole image");

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory = std::filesystem::temp_directory_path() /
        (L"WidgetStudioAssetTests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(suffix));
    std::filesystem::create_directories(directory);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};
    const std::filesystem::path source = directory / L"source.png";
    { std::ofstream output(source, std::ios::binary); output << "local-image-bytes"; }
    ws::AssetLibrary library(directory / L"assets");
    std::wstring error;
    const auto imported = library.Import(source, error);
    Require(imported.has_value() && imported->parent_path() == library.Directory() &&
        std::filesystem::exists(*imported),
        "asset import should copy into application-owned storage");
    std::ifstream input(*imported, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    Require(bytes == "local-image-bytes", "asset import should preserve file contents");
}

void TestFreeLayoutAndAlignment() {
    ws::FreePlacement reference{10.0f, 20.0f, 100.0f, 50.0f};
    ws::FreePlacement target{0.0f, 80.0f, 40.0f, 20.0f};
    std::array items{
        ws::AlignmentItem{&reference, true, false},
        ws::AlignmentItem{&target, false, false},
    };
    Require(ws::Alignment::Apply(items, ws::AlignmentOperation::Right, {0.0f, 0.0f, 500.0f, 500.0f}) &&
        std::abs(target.x - 70.0f) < 0.0001f,
        "right alignment should use the primary selection as reference");
    Require(ws::Alignment::Apply(items, ws::AlignmentOperation::MatchBoth, {0.0f, 0.0f, 500.0f, 500.0f}) &&
        target.width == reference.width && target.height == reference.height,
        "match-both should copy primary dimensions");

    ws::FreePlacement first{0.0f, 0.0f, 10.0f, 10.0f};
    ws::FreePlacement middle{40.0f, 0.0f, 10.0f, 10.0f};
    ws::FreePlacement last{100.0f, 0.0f, 10.0f, 10.0f};
    std::array distributed{
        ws::AlignmentItem{&first, true, false}, ws::AlignmentItem{&middle, false, false},
        ws::AlignmentItem{&last, false, false},
    };
    Require(ws::Alignment::Apply(distributed, ws::AlignmentOperation::DistributeHorizontally,
            {0.0f, 0.0f, 200.0f, 100.0f}) && std::abs(middle.x - 50.0f) < 0.0001f,
        "horizontal distribution should create equal edge-to-edge gaps");

    const ws::FreePlacement moved = ws::OuterLayout::MoveFreeToPoint(
        {20.0f, 20.0f, 80.0f, 60.0f}, {500.0f, 500.0f}, {10.0f, 10.0f},
        {0.0f, 0.0f, 300.0f, 200.0f});
    Require(moved.x == 220.0f && moved.y == 140.0f,
        "free dragging should clamp the full widget within available bounds");
}

} // namespace

int main() {
    try {
        TestRegistryAndPlacement();
        TestMonitorMigration();
        TestSerialization();
        TestAuthoredLayout();
        TestAtomicStoreAndRestore();
        TestClockStateAndScheduling();
        TestCalendarModel();
        TestPhotoLayoutAndAssetImport();
        TestFreeLayoutAndAlignment();
        std::cout << "WidgetStudio logic tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WidgetStudio logic test failed: " << error.what() << '\n';
        return 1;
    }
}
