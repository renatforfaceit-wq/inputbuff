#include <Geode/Geode.hpp>

#include "Config.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("Predictive Input Buffer v1.0.0 loaded (Geode 5.11.2 / GD 2.2081)");

    if (pib::config::debugLogs()) {
        log::debug("[PIB] window {} / extended {} / density {} / radius {:.0f} / mode {} / "
                   "release {} ({} ms) / humanization {}% / ground modes {}",
                   pib::config::windowSize(), pib::config::extendedWindowSize(),
                   pib::config::densityThreshold(), pib::config::scanRadius(),
                   pib::config::trajectoryPrediction() ? "trajectory" : "aabb",
                   pib::config::releaseProtection(), pib::config::releaseDelayMs(),
                   pib::config::humanizationChance(),
                   pib::config::bufferInCubeMode() ? "buffered" : "passed through");
    }
}
