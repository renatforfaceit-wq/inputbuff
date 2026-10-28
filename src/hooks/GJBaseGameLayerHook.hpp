#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include <cstdint>
#include <random>

#include "../Config.hpp"
#include "../InputBuffer.hpp"
#include "../PredictionEngine.hpp"

namespace pib {

/**
 * Owns the press-side queue and the per-frame frame counter.
 *
 * `handleButton` silently drops a press that the predictor marks as lethal and stores
 * it in `Fields::buffer`. `update` drains whatever became due and replays it through the
 * original GJBaseGameLayer::handleButton, so the game sees a completely normal click on
 * a later frame.
 */
struct GJBaseGameLayerHook : geode::Modify<GJBaseGameLayerHook, GJBaseGameLayer> {
    struct Fields {
        InputBuffer buffer;
        PredictionEngine engine;
        std::mt19937 rng{std::random_device{}()};
        std::uint64_t frame = 0;
        /// Set while update() replays a queued action. The replay must reach the game
        /// untouched, otherwise the predictor would immediately re-buffer its own click.
        bool replaying = false;

        ~Fields();
    };

    void handleButton(bool down, int button, bool isPlayer1);
    void update(float dt);
};

} // namespace pib
