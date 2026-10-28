#include "PredictionEngine.hpp"

#include <algorithm>
#include <cmath>

namespace pib {

namespace {

/// All physics numbers below are approximations, not GD internals.
///
/// The predictor is intentionally cheap: it only needs to answer "would this click be
/// lethal next frame, and is there a later frame in the window that is clear?". A real
/// simulation would have to copy GJGameState, run PlayerObject::update and deal with
/// every special object; that is out of scope for this mod.
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr double kGravityPixelsPerSecondSquared = 2600.0;

/// getObjectRect() returns the sprite rect, which is wider than the actual collision
/// shape. Shrinking it makes the heuristic prefer false negatives (no buffering) over
/// false positives (buffering a click that was fine), which is the safer failure mode.
constexpr float kPlayerHitboxScale = 0.8f;
constexpr float kHazardInset = 1.0f;
constexpr float kDefaultPlayerHalfSize = 15.0f;

bool overlapsHazard(cocos2d::CCPoint const& center, float halfWidth, float halfHeight,
                    cocos2d::CCRect const& hazard) {
    float const left = hazard.origin.x + kHazardInset;
    float const right = hazard.origin.x + hazard.size.width - kHazardInset;
    float const bottom = hazard.origin.y + kHazardInset;
    float const top = hazard.origin.y + hazard.size.height - kHazardInset;

    if (right <= left || top <= bottom) {
        return false;
    }

    return center.x + halfWidth > left && center.x - halfWidth < right &&
           center.y + halfHeight > bottom && center.y - halfHeight < top;
}

std::vector<GameObject*> collectNearbyHazards(GJBaseGameLayer* layer,
                                             cocos2d::CCPoint const& center,
                                             float radius) {
    std::vector<GameObject*> hazards;
    if (!layer || !layer->m_objects) {
        return hazards;
    }

    auto* objects = layer->m_objects;
    unsigned int const count = objects->count();
    float const radiusSquared = radius * radius;

    hazards.reserve(64);

    for (unsigned int i = 0; i < count; ++i) {
        auto* object = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (!object || !PredictionEngine::isHazard(object)) {
            continue;
        }
        if (object->m_isDisabled || object->m_isDisabled2 || object->m_isInvisible) {
            continue;
        }

        auto const& rect = object->getObjectRect();
        float const objectX = rect.origin.x + rect.size.width * 0.5f;
        float const objectY = rect.origin.y + rect.size.height * 0.5f;
        float const dx = objectX - center.x;
        float const dy = objectY - center.y;

        if (dx * dx + dy * dy <= radiusSquared) {
            hazards.push_back(object);
        }
    }

    return hazards;
}

} // namespace

bool PredictionEngine::isHazard(GameObject* object) {
    if (!object) {
        return false;
    }

    switch (object->m_objectType) {
        case GameObjectType::Hazard:
        case GameObjectType::AnimatedHazard:
            return true;
        default:
            break;
    }

    // Slopes flagged as hazard only hurt in specific directions, but treating them as
    // lethal is the conservative choice for a buffer that must never make things worse.
    return object->m_slopeIsHazard;
}

PredictionEngine::Kinematics PredictionEngine::readKinematics(PlayerObject* player) {
    Kinematics kinematics;

    auto const& rect = player->getObjectRect();
    kinematics.center = cocos2d::CCPoint{rect.origin.x + rect.size.width * 0.5f,
                                         rect.origin.y + rect.size.height * 0.5f};

    kinematics.halfWidth = rect.size.width * 0.5f * kPlayerHitboxScale;
    kinematics.halfHeight = rect.size.height * 0.5f * kPlayerHitboxScale;
    if (kinematics.halfWidth <= 0.5f || kinematics.halfHeight <= 0.5f) {
        kinematics.halfWidth = kDefaultPlayerHalfSize;
        kinematics.halfHeight = kDefaultPlayerHalfSize;
    }

    // Same expression the game uses for horizontal movement, read straight from the
    // bindings fields to avoid depending on inline helper linkage.
    kinematics.xVelocity = static_cast<double>(player->m_playerSpeed) * player->m_speedMultiplier;
    kinematics.yVelocity = player->m_yVelocity;
    kinematics.gravitySign = player->m_isUpsideDown ? -1.0 : 1.0;

    return kinematics;
}

cocos2d::CCPoint PredictionEngine::predictCenter(Kinematics const& kinematics, int frame,
                                                PredictionMode mode) {
    double const t = static_cast<double>(frame) * kFrameSeconds;

    double const x = kinematics.center.x + kinematics.xVelocity * t;
    double y = kinematics.center.y + kinematics.yVelocity * t;

    if (mode == PredictionMode::Trajectory) {
        double const acceleration = -kGravityPixelsPerSecondSquared * kinematics.gravitySign;
        y += 0.5 * acceleration * t * t;
    }

    return cocos2d::CCPoint{static_cast<float>(x), static_cast<float>(y)};
}

PredictionResult PredictionEngine::analyze(PlayerObject* player,
                                           GJBaseGameLayer* layer,
                                           int baseWindow,
                                           int extendedWindow,
                                           int densityThreshold,
                                           PredictionMode mode,
                                           float scanRadius) const {
    PredictionResult result;
    if (!player) {
        return result;
    }

    Kinematics const kinematics = readKinematics(player);
    std::vector<GameObject*> const hazards =
        collectNearbyHazards(layer, kinematics.center, scanRadius);

    result.nearbyObjects = static_cast<int>(hazards.size());

    int window = std::clamp(baseWindow, 1, 32);
    if (result.nearbyObjects >= densityThreshold) {
        // "Dense section": allow a longer delay, but never shrink a window the user
        // configured larger than the extended one.
        window = std::clamp(std::max(extendedWindow, baseWindow), 1, 32);
    }
    result.windowUsed = window;

    result.safeFrames.reserve(static_cast<std::size_t>(window));

    for (int frame = 1; frame <= window; ++frame) {
        auto const predicted = predictCenter(kinematics, frame, mode);

        bool collides = false;
        for (auto* hazard : hazards) {
            if (overlapsHazard(predicted, kinematics.halfWidth, kinematics.halfHeight,
                               hazard->getObjectRect())) {
                collides = true;
                break;
            }
        }

        if (collides) {
            if (result.firstDangerFrame < 0) {
                result.firstDangerFrame = frame;
            }
            if (frame == 1) {
                result.dangerous = true;
            }
        } else {
            result.safeFrames.push_back(frame);
        }
    }

    return result;
}

} // namespace pib
