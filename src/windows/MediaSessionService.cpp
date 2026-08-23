#include "windows/MediaSessionService.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

namespace ws {
namespace media = winrt::Windows::Media::Control;
namespace streams = winrt::Windows::Storage::Streams;

struct MediaSessionService::Impl {
    enum class Command { Previous, TogglePlayPause, Next };

    explicit Impl(MediaSessionService& owner) : owner(owner) {}

    bool Initialize() {
        if (worker.joinable()) return true;
        try {
            worker = std::thread([this] { WorkerMain(); });
            return true;
        } catch (...) {
            return false;
        }
    }

    void Shutdown() noexcept {
        {
            std::scoped_lock lock(queueMutex);
            stopping = true;
        }
        queueChanged.notify_one();
        if (worker.joinable()) worker.join();
    }

    bool Enqueue(Command command) noexcept {
        {
            std::scoped_lock lock(queueMutex);
            if (stopping) return false;
            commands.push_back(command);
        }
        queueChanged.notify_one();
        return true;
    }

    void RequestRefresh(bool mediaProperties) noexcept {
        {
            std::scoped_lock lock(queueMutex);
            refreshRequested = true;
            refreshMedia = refreshMedia || mediaProperties;
        }
        queueChanged.notify_one();
    }

    void RequestSessionRefresh() noexcept {
        {
            std::scoped_lock lock(queueMutex);
            sessionRefreshRequested = true;
        }
        queueChanged.notify_one();
    }

    void WorkerMain() noexcept {
        bool apartmentInitialized = false;
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            apartmentInitialized = true;
            manager = media::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            managerChanged = manager.CurrentSessionChanged(
                [this](const auto&, const auto&) { RequestSessionRefresh(); });
            RefreshSessionOnWorker();

            while (true) {
                bool refreshSession = false;
                bool refresh = false;
                bool includeMedia = false;
                std::deque<Command> pendingCommands;
                {
                    std::unique_lock lock(queueMutex);
                    queueChanged.wait(lock, [this] {
                        return stopping || sessionRefreshRequested || refreshRequested || !commands.empty();
                    });
                    if (stopping) break;
                    refreshSession = std::exchange(sessionRefreshRequested, false);
                    refresh = std::exchange(refreshRequested, false);
                    includeMedia = std::exchange(refreshMedia, false);
                    pendingCommands.swap(commands);
                }

                if (refreshSession) RefreshSessionOnWorker();
                else if (refresh) RefreshAllOnWorker(includeMedia);
                for (Command command : pendingCommands) InvokeOnWorker(command);
            }
        } catch (...) {
            owner.Publish(MediaSessionSnapshot{});
        }

        UnsubscribeSessionOnWorker();
        try {
            if (manager && managerChanged.value) manager.CurrentSessionChanged(managerChanged);
        } catch (...) {}
        managerChanged = {};
        manager = nullptr;
        if (apartmentInitialized) winrt::uninit_apartment();
    }

    void UnsubscribeSessionOnWorker() noexcept {
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

    void RefreshSessionOnWorker() noexcept {
        try {
            UnsubscribeSessionOnWorker();
            session = manager.GetCurrentSession();
            if (session) {
                mediaChanged = session.MediaPropertiesChanged(
                    [this](const auto&, const auto&) { RequestRefresh(true); });
                playbackChanged = session.PlaybackInfoChanged(
                    [this](const auto&, const auto&) { RequestRefresh(false); });
                timelineChanged = session.TimelinePropertiesChanged(
                    [this](const auto&, const auto&) { RequestRefresh(false); });
            }
            RefreshAllOnWorker(true);
        } catch (...) {
            owner.Publish(MediaSessionSnapshot{});
        }
    }

    void RefreshAllOnWorker(bool includeMedia) noexcept {
        try {
            MediaSessionSnapshot snapshot = owner.Snapshot();
            snapshot.hasSession = static_cast<bool>(session);
            snapshot.capturedAt = std::chrono::steady_clock::now();
            if (!session) { owner.Publish(MediaSessionSnapshot{}); return; }

            if (includeMedia) {
                snapshot.source = session.SourceAppUserModelId().c_str();
                const auto properties = session.TryGetMediaPropertiesAsync().get();
                snapshot.title = properties.Title().c_str();
                snapshot.artist = properties.Artist().c_str();
                snapshot.artwork.reset();
                if (const auto thumbnail = properties.Thumbnail()) {
                    const auto randomAccess = thumbnail.OpenReadAsync().get();
                    const std::uint64_t size = std::min<std::uint64_t>(
                        randomAccess.Size(), 16ULL * 1024ULL * 1024ULL);
                    if (size > 0) {
                        const auto buffer = randomAccess.ReadAsync(
                            streams::Buffer(static_cast<std::uint32_t>(size)),
                            static_cast<std::uint32_t>(size),
                            streams::InputStreamOptions::None).get();
                        if (buffer.Length() > 0) {
                            auto artwork = std::make_shared<std::vector<std::uint8_t>>(buffer.Length());
                            streams::DataReader::FromBuffer(buffer).ReadBytes(
                                winrt::array_view<std::uint8_t>(
                                    artwork->data(), artwork->data() + artwork->size()));
                            snapshot.artwork = std::move(artwork);
                        }
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

    void InvokeOnWorker(Command command) noexcept {
        if (!session) return;
        try {
            switch (command) {
            case Command::Previous: static_cast<void>(session.TrySkipPreviousAsync().get()); break;
            case Command::TogglePlayPause: static_cast<void>(session.TryTogglePlayPauseAsync().get()); break;
            case Command::Next: static_cast<void>(session.TrySkipNextAsync().get()); break;
            }
            RefreshAllOnWorker(false);
        } catch (...) {}
    }

    MediaSessionService& owner;
    std::thread worker;
    std::mutex queueMutex;
    std::condition_variable queueChanged;
    std::deque<Command> commands;
    bool stopping{false};
    bool sessionRefreshRequested{false};
    bool refreshRequested{false};
    bool refreshMedia{false};
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
    return impl_->Enqueue(Impl::Command::Previous);
}

bool MediaSessionService::TogglePlayPause() noexcept {
    return impl_->Enqueue(Impl::Command::TogglePlayPause);
}

bool MediaSessionService::Next() noexcept {
    return impl_->Enqueue(Impl::Command::Next);
}

} // namespace ws
