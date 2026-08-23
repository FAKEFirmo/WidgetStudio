#include "windows/MediaSessionService.h"

#include <algorithm>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

namespace ws {
namespace media = winrt::Windows::Media::Control;
namespace streams = winrt::Windows::Storage::Streams;

struct MediaSessionService::Impl {
    explicit Impl(MediaSessionService& owner) : owner(owner) {}

    bool Initialize() {
        manager = media::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        managerChanged = manager.CurrentSessionChanged([this](const auto&, const auto&) { RefreshSession(); });
        RefreshSession();
        return true;
    }

    void Shutdown() noexcept {
        try {
            UnsubscribeSession();
            if (manager && managerChanged.value) manager.CurrentSessionChanged(managerChanged);
        } catch (...) {}
        manager = nullptr;
    }

    void UnsubscribeSession() noexcept {
        try {
            if (session) {
                if (mediaChanged.value) session.MediaPropertiesChanged(mediaChanged);
                if (playbackChanged.value) session.PlaybackInfoChanged(playbackChanged);
                if (timelineChanged.value) session.TimelinePropertiesChanged(timelineChanged);
            }
        } catch (...) {}
        mediaChanged = {};
        playbackChanged = {};
        timelineChanged = {};
        session = nullptr;
    }

    void RefreshSession() noexcept {
        try {
            UnsubscribeSession();
            session = manager.GetCurrentSession();
            if (session) {
                mediaChanged = session.MediaPropertiesChanged([this](const auto&, const auto&) { RefreshAll(true); });
                playbackChanged = session.PlaybackInfoChanged([this](const auto&, const auto&) { RefreshAll(false); });
                timelineChanged = session.TimelinePropertiesChanged([this](const auto&, const auto&) { RefreshAll(false); });
            }
            RefreshAll(true);
        } catch (...) {
            owner.Publish(MediaSessionSnapshot{});
        }
    }

    void RefreshAll(bool refreshMedia) noexcept {
        try {
            MediaSessionSnapshot snapshot = owner.Snapshot();
            snapshot.hasSession = static_cast<bool>(session);
            snapshot.capturedAt = std::chrono::steady_clock::now();
            if (!session) { owner.Publish(MediaSessionSnapshot{}); return; }

            if (refreshMedia) {
                snapshot.source = session.SourceAppUserModelId().c_str();
                const auto properties = session.TryGetMediaPropertiesAsync().get();
                snapshot.title = properties.Title().c_str();
                snapshot.artist = properties.Artist().c_str();
                snapshot.artwork.reset();
                if (const auto thumbnail = properties.Thumbnail()) {
                    const auto randomAccess = thumbnail.OpenReadAsync().get();
                    const std::uint64_t size = std::min<std::uint64_t>(randomAccess.Size(), 16ULL * 1024ULL * 1024ULL);
                    if (size > 0) {
                        const auto buffer = randomAccess.ReadAsync(
                            streams::Buffer(static_cast<std::uint32_t>(size)), static_cast<std::uint32_t>(size),
                            streams::InputStreamOptions::None).get();
                        auto artwork = std::make_shared<std::vector<std::uint8_t>>(buffer.Length());
                        auto artworkView = winrt::array_view<std::uint8_t>(*artwork);
                        streams::DataReader::FromBuffer(buffer).ReadBytes(artworkView);
                        snapshot.artwork = std::move(artwork);
                    }
                }
            }

            const auto playback = session.GetPlaybackInfo();
            snapshot.playing = playback.PlaybackStatus() ==
                media::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            const auto controls = playback.Controls();
            snapshot.canPrevious = controls.IsPreviousEnabled();
            snapshot.canTogglePlayPause = controls.IsPlayPauseToggleEnabled();
            snapshot.canNext = controls.IsNextEnabled();
            const auto timeline = session.GetTimelineProperties();
            snapshot.position = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.Position());
            snapshot.duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.EndTime());
            owner.Publish(std::move(snapshot));
        } catch (...) {
            MediaSessionSnapshot snapshot{};
            snapshot.hasSession = static_cast<bool>(session);
            snapshot.capturedAt = std::chrono::steady_clock::now();
            owner.Publish(std::move(snapshot));
        }
    }

    template <typename Operation>
    bool Invoke(Operation&& operation) noexcept {
        try {
            return session && operation(session).get();
        } catch (...) {
            return false;
        }
    }

    MediaSessionService& owner;
    media::GlobalSystemMediaTransportControlsSessionManager manager{nullptr};
    media::GlobalSystemMediaTransportControlsSession session{nullptr};
    winrt::event_token managerChanged{};
    winrt::event_token mediaChanged{};
    winrt::event_token playbackChanged{};
    winrt::event_token timelineChanged{};
};

MediaSessionService::MediaSessionService() : impl_(std::make_unique<Impl>(*this)) {}
MediaSessionService::~MediaSessionService() { Shutdown(); }

bool MediaSessionService::Initialize() noexcept {
    try { return impl_->Initialize(); }
    catch (...) { return false; }
}

void MediaSessionService::Shutdown() noexcept {
    if (impl_) impl_->Shutdown();
    SetChangedCallback({});
}

void MediaSessionService::SetChangedCallback(std::function<void()> callback) {
    std::scoped_lock lock(mutex_);
    changedCallback_ = std::move(callback);
}

MediaSessionSnapshot MediaSessionService::Snapshot() const {
    std::scoped_lock lock(mutex_);
    return snapshot_;
}

void MediaSessionService::Publish(MediaSessionSnapshot snapshot) {
    std::function<void()> callback;
    {
        std::scoped_lock lock(mutex_);
        snapshot.revision = snapshot_.revision + 1;
        snapshot.artworkRevision = snapshot.artwork == snapshot_.artwork
            ? snapshot_.artworkRevision : snapshot_.artworkRevision + 1;
        snapshot_ = std::move(snapshot);
        callback = changedCallback_;
    }
    if (callback) callback();
}

bool MediaSessionService::Previous() noexcept {
    return impl_->Invoke([](const auto& session) { return session.TrySkipPreviousAsync(); });
}

bool MediaSessionService::TogglePlayPause() noexcept {
    return impl_->Invoke([](const auto& session) { return session.TryTogglePlayPauseAsync(); });
}

bool MediaSessionService::Next() noexcept {
    return impl_->Invoke([](const auto& session) { return session.TrySkipNextAsync(); });
}

} // namespace ws
