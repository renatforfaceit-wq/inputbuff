#include "GJBaseGameLayerHook.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

using namespace geode::prelude;

namespace pib {

namespace {

char const* buttonName(PlayerButton button) {
    switch (button) {
        case PlayerButton::Jump:
            return "jump";
        case PlayerButton::Left:
            return "left";
        case PlayerButton::Right:
            return "right";
        default:
            return "unknown";
    }
}

/// Ground modes share a gravity-driven jump. A press that is safe one or two frames
/// later can still be a jump the level requires *now*, so buffering there is opt-in.
/// m_isDashing is deliberately excluded: it marks a spider dash, not a flight mode.
bool bufferAllowedInMode(PlayerObject* player) {
    if (config::bufferInCubeMode()) {
        return true;
    }
    return player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing;
}

} // namespace

GJBaseGameLayerHook::Fields::~Fields() {
    if (!config::debugLogs()) {
        return;
    }

    auto const& stats = buffer.stats();
    if (stats.bufferedPresses == 0 && stats.delayedReleases == 0) {
        return;
    }

    log::info("[PIB] session: {} buffered click(s), average delay {:.2f} ms, "
              "{} early release(s), {} delayed release(s)",
              stats.bufferedPresses, stats.averagePressDelayMs(), stats.earlyReleases,
              stats.delayedReleases);
}

void GJBaseGameLayerHook::handleButton(bool down, int button, bool isPlayer1) {
    // The bindings expose this as an int; only 1..3 map to PlayerButton.
    int const buttonId = button;
    if (buttonId < static_cast<int>(PlayerButton::Jump) ||
        buttonId > static_cast<int>(PlayerButton::Right)) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    auto const playerButton = static_cast<PlayerButton>(buttonId);
    auto* fields = m_fields.self();

    // update() replays queued actions through GJBaseGameLayer::handleButton. That call
    // starts a fresh hook chain, so this hook runs again; consume the flag once and let
    // the call advance to the next hook / the original implementation.
    if (fields->replaying) {
        fields->replaying = false;
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    // ---------------------------------------------------------------- releases
    if (!down) {
        // A quick tap can be released before its buffered press was delivered. Swallow
        // that release; it is re-issued one update after the press.
        if (fields->buffer.releasePendingPress(playerButton, isPlayer1)) {
            if (config::debugLogs()) {
                log::debug("[PIB] early release of {} (player {}) swallowed until the buffered press",
                           buttonName(playerButton), isPlayer1 ? 1 : 2);
            }
            return;
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    // ------------------------------------------------------------------- presses
    if (!config::predictiveBuffer()) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    auto* player = isPlayer1 ? this->m_player1 : this->m_player2;
    if (!player) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    if (!bufferAllowedInMode(player)) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    // Two presses of the same button without a release in between make no sense for
    // GD's input model; forward them instead of stacking queue entries.
    if (fields->buffer.hasPendingPress(playerButton, isPlayer1)) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    auto const mode = config::trajectoryPrediction() ? PredictionMode::Trajectory
                                                     : PredictionMode::SimpleAabb;

    PredictionResult const result = fields->engine.analyze(
        player, this, config::windowSize(), config::extendedWindowSize(),
        config::densityThreshold(), mode, config::scanRadius());

    // Not lethal next frame, or no safe frame in the window: never invent an input.
    if (!result.dangerous || result.safeFrames.empty()) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    int chosenFrame = result.safeFrames.front();
    bool randomized = false;

    // Humanity masking: mostly the earliest safe frame, sometimes any safe frame.
    if (result.safeFrames.size() > 1) {
        std::uniform_int_distribution<int> roll(1, 100);
        if (roll(fields->rng) > config::humanizationChance()) {
            std::uniform_int_distribution<std::size_t> pick(0, result.safeFrames.size() - 1);
            chosenFrame = result.safeFrames[pick(fields->rng)];
            randomized = true;
        }
    }

    if (!fields->buffer.queuePress(playerButton, isPlayer1, chosenFrame, fields->frame,
                                   Clock::now())) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        return;
    }

    if (config::debugLogs()) {
        log::debug("[PIB] buffered {} for {} frame(s) | window {}, nearby hazards {}, "
                   "mode {}, randomized {}",
                   buttonName(playerButton), chosenFrame, result.windowUsed,
                   result.nearbyObjects,
                   mode == PredictionMode::Trajectory ? "trajectory" : "aabb", randomized);
    }
}

void GJBaseGameLayerHook::update(float dt) {
    auto* fields = m_fields.self();
    ++fields->frame;

    // A retry reuses the same layer object, so a pending click must not leak into the
    // next attempt. Dropping it is also correct on death: the frame it was chosen for
    // no longer exists.
    bool const anyPlayerDead = (this->m_player1 && this->m_player1->m_isDead) ||
                               (this->m_player2 && this->m_player2->m_isDead);
    if (anyPlayerDead && !fields->buffer.empty()) {
        fields->buffer.reset();
    }

    if (!fields->buffer.empty()) {
        std::vector<InputAction> actions;
        actions.reserve(4);
        fields->buffer.tick(fields->frame, Clock::now(), actions);

        // Replay through the same entry point the game uses. The replaying flag makes
        // this hook's chain entry pass the action on instead of predicting it again.
        for (auto const& action : actions) {
            fields->replaying = true;
            GJBaseGameLayer::handleButton(action.push, static_cast<int>(action.button),
                                          action.player1);
            fields->replaying = false;
        }
    }

    GJBaseGameLayer::update(dt);
}

} // namespace pib
