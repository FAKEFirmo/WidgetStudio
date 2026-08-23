#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ws {

struct MediaSessionSnapshot {
    std::wstring title;
    std::wstring artist;
    std::wstring source;
    std::shared_ptr<const std::vector<std::uint8_t>> artwork;
    std::chrono::milliseconds duration{};
    std::chrono::milliseconds position{};
    std::chrono::steady_clock::time_point capturedAt{};
    bool hasSession{false};
    bool playing{false};
    bool canPrevious{false};
    bool canTogglePlayPause{false};
    bool canNext{false};
    std::uint64_t revision{};
    std::uint64_t artworkRevision{};
};

class MediaSessionService {
public:
    MediaSessionService();
    ~MediaSessionService();

    bool Initialize() noexcept;
    void Shutdown() noexcept;
    void SetChangedCallback(std::function<void()> callback);
    [[nodiscard]] MediaSessionSnapshot Snapshot() const;

    bool Previous() noexcept;
    bool TogglePlayPause() noexcept;
    bool Next() noexcept;

private:
    struct Impl;
    friend struct Impl;
    void Publish(MediaSessionSnapshot snapshot);

    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    MediaSessionSnapshot snapshot_{};
    std::function<void()> changedCallback_;
};

} // namespace ws
