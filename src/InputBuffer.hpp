#pragma once

#include <Geode/Enums.hpp>

#include <chrono>
#include <cstdint>
#include <vector>

namespace pib {

using Clock = std::chrono::steady_clock;

/// One input that has to reach GJBaseGameLayer::handleButton at some point.
struct InputAction {
    bool push = true;
    PlayerButton button = PlayerButton::Jump;
    bool player1 = true;
};

struct BufferStats {
    std::uint64_t bufferedPresses = 0;
    std::uint64_t delayedReleases = 0;
    std::uint64_t earlyReleases = 0;
    double totalPressDelayMs = 0.0;
    double totalReleaseDelayMs = 0.0;

    double averagePressDelayMs() const {
        return bufferedPresses ? totalPressDelayMs / static_cast<double>(bufferedPresses) : 0.0;
    }

    double averageReleaseDelayMs() const {
        return delayedReleases ? totalReleaseDelayMs / static_cast<double>(delayedReleases) : 0.0;
    }
};

/**
 * Frame- and time-based queue for inputs that must not reach the game immediately.
 *
 * Two independent uses live here:
 *  - presses deferred by the collision predictor (frame based, so it lines up with the
 *    prediction window), and
 *  - Left/Right releases deferred by the release protection (time based, because the
 *    setting is expressed in milliseconds).
 *
 * Each hook owns its own instance inside `struct Fields`, so there are no globals and
 * two player objects never share a queue.
 */
class InputBuffer {
public:
    /// Schedules `button` for `currentFrame + delayFrames`. Returns false if the same
    /// press is already queued (the caller then forwards the press as-is).
    bool queuePress(PlayerButton button, bool player1, int delayFrames,
                    std::uint64_t currentFrame, Clock::time_point now);

    bool hasPendingPress(PlayerButton button, bool player1) const;

    /// Marks an undelivered press as "released early". The press is still delivered on
    /// its due frame, followed by a release one frame later. Returns true when a press
    /// was found and must be swallowed.
    bool releasePendingPress(PlayerButton button, bool player1);

    /// Schedules a release. Returns false when an identical release is already pending.
    bool queueRelease(PlayerButton button, bool player1, Clock::time_point dueAt,
                      Clock::time_point now);

    /// Drops a pending release, used when the button is pressed again.
    bool cancelRelease(PlayerButton button, bool player1);

    /// Appends every action whose deadline has passed. Call once per game update.
    void tick(std::uint64_t frame, Clock::time_point now, std::vector<InputAction>& out);

    void reset();

    bool empty() const {
        return m_presses.empty() && m_releases.empty();
    }

    BufferStats const& stats() const {
        return m_stats;
    }

private:
    struct PendingPress {
        PlayerButton button;
        bool player1;
        std::uint64_t dueFrame;
        Clock::time_point queuedAt;
        bool releasedEarly = false;
    };

    struct PendingRelease {
        PlayerButton button;
        bool player1;
        Clock::time_point dueAt;
        Clock::time_point queuedAt;
    };

    std::vector<PendingPress> m_presses;
    std::vector<PendingRelease> m_releases;
    BufferStats m_stats;
};

} // namespace pib
