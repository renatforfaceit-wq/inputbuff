#include "PlayerObjectHook.hpp"

#include <chrono>
#include <vector>

using namespace geode::prelude;

namespace pib {

namespace {

bool isPlayerOne(GJBaseGameLayer* layer, PlayerObject* player) {
    return layer && layer->m_player1 == player;
}

} // namespace

PlayerObjectHook::Fields::~Fields() {
    if (!config::debugLogs()) {
        return;
    }

    auto const& stats = buffer.stats();
    if (stats.delayedReleases == 0) {
        return;
    }

    log::info("[PIB] release protection: {} delayed release(s), average extra {:.2f} ms",
              stats.delayedReleases, stats.averageReleaseDelayMs());
}

bool PlayerObjectHook::pushButton(PlayerButton button) {
    auto* fields = m_fields.self();
    bool const player1 = isPlayerOne(this->m_gameLayer, this);

    // A fresh press invalidates a still-pending release of the same button.
    if (fields->buffer.cancelRelease(button, player1) && config::debugLogs()) {
        log::debug("[PIB] pending release of button {} cancelled by a new press",
                   static_cast<int>(button));
    }

    return PlayerObject::pushButton(button);
}

bool PlayerObjectHook::releaseButton(PlayerButton button) {
    auto* fields = m_fields.self();

    // update() replays delayed releases through PlayerObject::releaseButton. That call
    // starts a fresh hook chain, so this hook runs again; consume the flag once and let
    // the call advance to the next hook / the original implementation.
    if (fields->replaying) {
        fields->replaying = false;
        return PlayerObject::releaseButton(button);
    }

    if (!config::releaseProtection() || config::releaseDelayMs() <= 0) {
        return PlayerObject::releaseButton(button);
    }

    // Only directional releases matter for wave / dash lines. Jump releases can stay
    // exact: delaying them would eat orb and ring activations.
    if (button != PlayerButton::Left && button != PlayerButton::Right) {
        return PlayerObject::releaseButton(button);
    }

    if (this->m_isOnGround) {
        return PlayerObject::releaseButton(button);
    }

    // isFlying() covers ship, UFO, wave (m_isDart) and swing; m_isDashing covers the
    // dash. Fields are read directly to avoid depending on inline helper definitions.
    bool const inFlight = this->m_isShip || this->m_isBird || this->m_isDart ||
                          this->m_isSwing || this->m_isDashing;
    if (!inFlight) {
        return PlayerObject::releaseButton(button);
    }

    bool const player1 = isPlayerOne(this->m_gameLayer, this);
    auto const now = Clock::now();
    auto const delay = std::chrono::milliseconds(config::releaseDelayMs());

    if (!fields->buffer.queueRelease(button, player1, now + delay, now)) {
        // Already scheduled: swallow the duplicate, the first one will fire.
        return true;
    }

    if (config::debugLogs()) {
        log::debug("[PIB] delayed release of {} by {} ms", static_cast<int>(button),
                   config::releaseDelayMs());
    }

    // The release is handled later by update(); returning true tells the caller it was
    // consumed now. See about.md for the caveat around the layer's own input bookkeeping.
    return true;
}

void PlayerObjectHook::update(float dt) {
    auto* fields = m_fields.self();
    ++fields->frame;

    if (this->m_isDead && !fields->buffer.empty()) {
        fields->buffer.reset();
    }

    if (!fields->buffer.empty()) {
        std::vector<InputAction> actions;
        actions.reserve(2);
        fields->buffer.tick(fields->frame, Clock::now(), actions);

        for (auto const& action : actions) {
            // This PlayerObject never queues presses itself, only releases.
            if (!action.push) {
                fields->replaying = true;
                PlayerObject::releaseButton(action.button);
                fields->replaying = false;
            }
        }
    }

    PlayerObject::update(dt);
}

} // namespace pib
