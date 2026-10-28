#pragma once

#include <Geode/loader/Mod.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

/**
 * Thin, header-only wrapper around mod.json settings.
 *
 * Every accessor re-reads the setting, so toggling something in the Geode UI takes
 * effect immediately without any listener plumbing. Reading a setting is a map lookup
 * and is only done on input events / level teardown, never per frame inside physics.
 */
namespace pib::config {

inline geode::Mod* mod() {
    return geode::Mod::get();
}

inline bool predictiveBuffer() {
    return mod()->getSettingValue<bool>("predictive-buffer");
}

/**
 * Flight modes are buffered by default because a missed frame there is usually just a
 * bad arc corner. Ground modes have gravity: delaying a jump can make a required jump
 * impossible, so they have to be opted into explicitly.
 */
inline bool bufferInCubeMode() {
    return mod()->getSettingValue<bool>("buffer-in-cube-mode");
}

inline int windowSize() {
    return std::clamp(static_cast<int>(mod()->getSettingValue<int64_t>("window-size")), 1, 32);
}

inline int extendedWindowSize() {
    return std::clamp(static_cast<int>(mod()->getSettingValue<int64_t>("extended-window-size")), 1, 32);
}

inline int densityThreshold() {
    return std::clamp(static_cast<int>(mod()->getSettingValue<int64_t>("density-threshold")), 1, 100);
}

inline float scanRadius() {
    return std::clamp(static_cast<float>(mod()->getSettingValue<double>("scan-radius")), 50.0f, 400.0f);
}

inline bool trajectoryPrediction() {
    return mod()->getSettingValue<std::string>("prediction-mode") == "trajectory";
}

inline int humanizationChance() {
    return std::clamp(static_cast<int>(mod()->getSettingValue<int64_t>("humanization-chance")), 0, 100);
}

inline bool releaseProtection() {
    return mod()->getSettingValue<bool>("release-protection");
}

inline int releaseDelayMs() {
    return std::clamp(static_cast<int>(mod()->getSettingValue<int64_t>("release-delay-ms")), 0, 200);
}

inline bool debugLogs() {
    // Called from Fields destructors, which can run late during shutdown when the mod
    // pointer may already be gone. Never dereference it unguarded.
    auto* current = mod();
    return current && current->getSettingValue<bool>("debug-logs");
}

} // namespace pib::config
