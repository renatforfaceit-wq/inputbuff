#pragma once

#include <Geode/Geode.hpp>

#include <vector>

namespace pib {

enum class PredictionMode {
    SimpleAabb,
    Trajectory,
};

struct PredictionResult {
    /// True when the very next frame is predicted to overlap a hazard, i.e. the click
    /// is unsafe as-is and buffering is worth attempting.
    bool dangerous = false;
    int firstDangerFrame = -1;
    int windowUsed = 0;
    int nearbyObjects = 0;
    /// Frame offsets (1-based, inside the window) whose predicted hitbox is clear.
    std::vector<int> safeFrames;
};

/**
 * Deliberately small collision heuristic. It is NOT a physics simulation.
 *
 * The player's current hitbox is extrapolated frame by frame and tested against the
 * axis-aligned rects of nearby lethal objects. Orbs, pads, portals, slopes and
 * moving/triggered objects are not simulated; see PredictionEngine.cpp for the exact
 * assumptions. Two modes are available:
 *
 *  - SimpleAabb:  linear extrapolation from the current velocity (cheap, no gravity).
 *  - Trajectory:  per-frame integration with an assumed gravity magnitude, using the
 *                 player's own gravity direction (better on arcs, still approximate).
 */
class PredictionEngine {
public:
    PredictionResult analyze(PlayerObject* player,
                             GJBaseGameLayer* layer,
                             int baseWindow,
                             int extendedWindow,
                             int densityThreshold,
                             PredictionMode mode,
                             float scanRadius) const;

    /// Lethal object types according to the bindings, not a guess.
    static bool isHazard(GameObject* object);

private:
    struct Kinematics {
        cocos2d::CCPoint center;
        float halfWidth = 15.0f;
        float halfHeight = 15.0f;
        double xVelocity = 0.0;
        double yVelocity = 0.0;
        double gravitySign = 1.0;
    };

    static Kinematics readKinematics(PlayerObject* player);
    static cocos2d::CCPoint predictCenter(Kinematics const& kinematics, int frame,
                                          PredictionMode mode);
};

} // namespace pib
