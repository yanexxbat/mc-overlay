#pragma once
#include "mappings.hpp"

namespace mcov::mappings {
enum class PartialTickSource { Camera, Client, CameraDelta };
struct GeometryProfile {
    const char* boundsClass;
    const char* poseClass;
    const char* walkClass;
    const char* dimensionsClass;
    const char* deltaClass;
    const char* avatarOwnerClass;
    const char* avatarStateClass;
    const char* humanoids[5]; // zombie, skeleton, piglin, enderman, armour stand
    PartialTickSource partialSource;
    Member getBounds, minX, minY, minZ, maxX, maxY, maxZ;
    Member oldX, oldY, oldZ;
    Member bodyYaw, bodyYawOld, headYaw, headYawOld, headPitch, headPitchOld;
    Member getPose, poseStanding, poseCrouch, poseSwim, poseGlide, poseSleep;
    Member isPassenger, walkState, walkSpeed, walkPosition;
    Member getDimensions, dimensionHeight;
    Member partialTick, getDelta, cameraEntity, avatarState;
    Member walkDistance, walkDistanceOld, bob, bobOld;
    Member runFrame, updateCamera;
    // Auch die Client-Tick-Brücke nutzt ausschließlich auditierte Signaturen.
    const char* handClass;
    const char* hitClass;
    Member clientTick, clientPaused, setYaw, setPitch, eyePosition, lineOfSight;
    Member attackCooldown, attackReach, farReach, attackEntity, swingHand;
    Member mainHand, pickBlocks, hitLocation, entityAlive;
    Member turn, viewYaw, viewPitch, yawOld;
    const char* crystalClass;
    const char* blockHitClass;
    const char* pillarClass;
    const char* anchorClass;
    const char* dimensionTypeClass;
    const char* environmentClass;
    const char* environmentAttrsClass;
    Member useItem, selectedSlot, inventoryMenu, carriedStack, hitResult, hitType, blockHitPos;
    Member stateValue, axisProperty, anchorCharge, dimensionType, anchorWorks;
    Member environment, environmentValue, environmentAnchor;
};
} // namespace mcov::mappings
