#include "widgets/DebugWidget.h"

#include "rendering/WidgetRenderContext.h"
#include "widgets/WidgetRegistry.h"

#include <iterator>
#include <memory>
#include <d2d1helper.h>
#include <wrl/client.h>

namespace ws {

void DebugWidget::Render(const WidgetRenderContext& context) const {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> titleBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> detailBrush;
    if (FAILED(context.renderTarget.CreateSolidColorBrush(
            D2D1::ColorF(0.97f, 0.97f, 0.95f, 0.96f), titleBrush.GetAddressOf())) ||
        FAILED(context.renderTarget.CreateSolidColorBrush(
            D2D1::ColorF(0.97f, 0.97f, 0.95f, 0.58f), detailBrush.GetAddressOf()))) return;

    const float padding = 16.0f * context.contentScale;
    constexpr wchar_t title[] = L"Debug Widget";
    const std::wstring identifier(context.instanceId.begin(), context.instanceId.end());
    const D2D1_RECT_F titleRect = D2D1::RectF(
        context.bounds.Left() + padding, context.bounds.Top() + padding,
        context.bounds.Right() - padding, context.bounds.Top() + padding + 30.0f * context.contentScale);
    const D2D1_RECT_F detailRect = D2D1::RectF(
        context.bounds.Left() + padding, context.bounds.Top() + padding + 32.0f * context.contentScale,
        context.bounds.Right() - padding, context.bounds.Bottom() - padding);
    context.renderTarget.DrawTextW(title, static_cast<UINT32>(std::size(title) - 1),
        &context.titleFormat, &titleRect, titleBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    context.renderTarget.DrawTextW(identifier.c_str(), static_cast<UINT32>(identifier.size()),
        &context.detailFormat, &detailRect, detailBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

std::span<const WidgetSettingDefinition> DebugWidget::Settings() const noexcept { return {}; }
WidgetState DebugWidget::SaveState() const { return {}; }
void DebugWidget::RestoreState(const WidgetState&) {}

void RegisterBuiltInWidgets(WidgetRegistry& registry) {
    registry.Register(WidgetDescriptor{
        .typeId = "debug",
        .displayName = L"Debug Widget",
        .description = L"Internal widget used to validate the shared widget lifecycle.",
        .defaultGridSize = GridSize{3, 2},
        .minimumGridSize = GridSize{2, 1},
        .maximumGridSize = GridSize{6, 4},
        .capabilities = WidgetCapability::Scalable,
        .factory = [] { return std::make_unique<DebugWidget>(); },
    });
}

} // namespace ws
