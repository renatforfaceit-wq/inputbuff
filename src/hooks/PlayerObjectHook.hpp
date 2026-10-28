#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <cstdint>

#include "../Config.hpp"
#include "../InputBuffer.hpp"

namespace pib {

/**
 * Directional release protection.
 *
 * Releasing Left/Right while airborne in a flying mode (or during a dash) is delayed by
 * `release-delay-ms`. The delayed release is replayed through the original
 * PlayerObject::releaseButton from this object's own update hook, so each PlayerObject
 * carries its own queue in `Fields`.
 *
 * A new press on the same button cancels the pending release; otherwise a quick
 * release-press-release pattern would end with the second press being cancelled.
 */
struct PlayerObjectHook : geode::Modify<PlayerObjectHook, PlayerObject> {
    struct Fields {
        InputBuffer buffer;
        std::uint64_t frame = 0;
        /// Set while update() replays a delayed release, see the header comment.
        bool replaying = false;

        ~Fields();
    };

    bool pushButton(PlayerButton button);
    bool releaseButton(PlayerButton button);
    void update(float dt);
};

} // namespace pib
