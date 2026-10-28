#include "InputBuffer.hpp"

#include <algorithm>

namespace pib {

namespace {

/// A tap that arrived before its buffered press must not end with a permanent hold:
/// the press is delivered on its due frame and the release follows one update later.
constexpr auto kHoldAfterBufferedPress = std::chrono::milliseconds(16);

double millisecondsBetween(Clock::time_point from, Clock::time_point to) {
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(to - from).count();
}

bool sameButton(PlayerButton lhs, bool lhsPlayer1, PlayerButton rhs, bool rhsPlayer1) {
    return lhs == rhs && lhsPlayer1 == rhsPlayer1;
}

} // namespace

bool InputBuffer::queuePress(PlayerButton button, bool player1, int delayFrames,
                             std::uint64_t currentFrame, Clock::time_point now) {
    if (hasPendingPress(button, player1)) {
        return false;
    }

    // A delay of zero is pointless: the current frame is already being processed.
    auto const frames = static_cast<std::uint64_t>(std::max(delayFrames, 1));
    m_presses.push_back(PendingPress{button, player1, currentFrame + frames, now, false});
    return true;
}

bool InputBuffer::hasPendingPress(PlayerButton button, bool player1) const {
    return std::any_of(m_presses.begin(), m_presses.end(), [&](PendingPress const& press) {
        return sameButton(press.button, press.player1, button, player1);
    });
}

bool InputBuffer::releasePendingPress(PlayerButton button, bool player1) {
    for (auto& press : m_presses) {
        if (sameButton(press.button, press.player1, button, player1)) {
            press.releasedEarly = true;
            return true;
        }
    }
    return false;
}

bool InputBuffer::queueRelease(PlayerButton button, bool player1, Clock::time_point dueAt,
                               Clock::time_point now) {
    for (auto const& release : m_releases) {
        if (sameButton(release.button, release.player1, button, player1)) {
            return false;
        }
    }

    m_releases.push_back(PendingRelease{button, player1, dueAt, now});
    return true;
}

bool InputBuffer::cancelRelease(PlayerButton button, bool player1) {
    auto const before = m_releases.size();
    std::erase_if(m_releases, [&](PendingRelease const& release) {
        return sameButton(release.button, release.player1, button, player1);
    });
    return m_releases.size() != before;
}

void InputBuffer::tick(std::uint64_t frame, Clock::time_point now, std::vector<InputAction>& out) {
    for (auto it = m_presses.begin(); it != m_presses.end();) {
        if (frame < it->dueFrame) {
            ++it;
            continue;
        }

        out.push_back(InputAction{true, it->button, it->player1});
        ++m_stats.bufferedPresses;
        m_stats.totalPressDelayMs += millisecondsBetween(it->queuedAt, now);

        if (it->releasedEarly) {
            ++m_stats.earlyReleases;
            m_releases.push_back(PendingRelease{it->button, it->player1,
                                                now + kHoldAfterBufferedPress, now});
        }

        it = m_presses.erase(it);
    }

    for (auto it = m_releases.begin(); it != m_releases.end();) {
        if (now < it->dueAt) {
            ++it;
            continue;
        }

        out.push_back(InputAction{false, it->button, it->player1});
        ++m_stats.delayedReleases;
        m_stats.totalReleaseDelayMs += millisecondsBetween(it->queuedAt, now);
        it = m_releases.erase(it);
    }
}

void InputBuffer::reset() {
    // Stats intentionally survive a reset so the end-of-session summary stays useful.
    m_presses.clear();
    m_releases.clear();
}

} // namespace pib
