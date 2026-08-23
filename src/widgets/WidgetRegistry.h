#pragma once

#include "widgets/WidgetDescriptor.h"

#include <memory>
#include <string_view>
#include <vector>

namespace ws {

class WidgetRegistry {
public:
    bool Register(WidgetDescriptor descriptor);
    [[nodiscard]] const std::vector<WidgetDescriptor>& Descriptors() const noexcept { return descriptors_; }
    [[nodiscard]] const WidgetDescriptor* Find(std::string_view typeId) const noexcept;
    [[nodiscard]] std::unique_ptr<IWidget> Create(std::string_view typeId) const;

private:
    std::vector<WidgetDescriptor> descriptors_;
};

} // namespace ws
