#pragma once

#include <d2d1helper.h>

namespace ws {

class ScopedRenderTransform {
public:
    ScopedRenderTransform(ID2D1RenderTarget& target, const D2D1_MATRIX_3X2_F& transform) noexcept
        : target_(target) {
        target_.GetTransform(&previous_);
        target_.SetTransform(transform * previous_);
    }

    ~ScopedRenderTransform() { target_.SetTransform(previous_); }
    ScopedRenderTransform(const ScopedRenderTransform&) = delete;
    ScopedRenderTransform& operator=(const ScopedRenderTransform&) = delete;

private:
    ID2D1RenderTarget& target_;
    D2D1_MATRIX_3X2_F previous_{};
};

} // namespace ws
