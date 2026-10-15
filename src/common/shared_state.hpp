// shared_state.hpp - the wire format between the in-JVM agent and the external overlay.
//
// This header is compiled into BOTH binaries, so it must stay a strict POD
// contract: no pointers, no std::string, no virtuals, no compiler-dependent
// padding surprises. Bump kSharedVersion whenever anything below changes, or a
// stale agent DLL will happily scribble a mismatched layout into the mapping.
#pragma once
#include "combat_bridge_state.hpp"
#include "item_assist.hpp"
#include "base_render_state.hpp"
#include "knife_state.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <string_view>

namespace mcov {

inline constexpr std::uint32_t kSharedMagic   = 0x564F434Du; // 'MCOV' little-endian
inline constexpr std::uint32_t kSharedVersion = 26;

inline constexpr std::uint32_t kMaxEntities = 256;
inline constexpr std::uint32_t kMaxBlocks   = 1024;

// Block types the scanner can look for. Matched as a substring of the block's
// translation key, so "diamond_ore" catches deepslate_diamond_ore too.
// Capped at 32 because the overlay passes the selection as a uint32 bitmask.
enum BlockKind : std::uint8_t {
    kBlockDiamond = 0,
    kBlockAncientDebris,
    kBlockGold,
    kBlockIron,
    kBlockEmerald,
    kBlockRedstone,
    kBlockLapis,
    kBlockCopper,
    kBlockCoal,
    kBlockQuartz,
    kBlockChest,
    kBlockSpawner,
    kBlockRotatedDeepslate,
    kBlockBeacon,
    kBlockShulker,
    kBlockBarrel,
    kBlockHopper,
    kBlockFurnace,
    kBlockEnderChest,
    kBlockTrappedChest,
    kBlockDispenser,
    kBlockDropper,
    kBlockBlastFurnace,
    kBlockSmoker,
    kBlockCrafter,
    kBlockBrewingStand,
    kBlockKelp,
    kBlockKelpPlant,
    kBlockLargeAmethystBud,
    kBlockAmethystCluster,
    kBlockKindCount,
};
static_assert(kBlockKindCount < 32);

inline const char* BlockKindKey(std::uint8_t kind) {
    switch (kind) {
        case kBlockDiamond:       return "diamond_ore";
        case kBlockAncientDebris: return "ancient_debris";
        case kBlockGold:          return "gold_ore";
        case kBlockIron:          return "iron_ore";
        case kBlockEmerald:       return "emerald_ore";
        case kBlockRedstone:      return "redstone_ore";
        case kBlockLapis:         return "lapis_ore";
        case kBlockCopper:        return "copper_ore";
        case kBlockCoal:          return "coal_ore";
        case kBlockQuartz:        return "quartz_ore";
        case kBlockChest:         return "chest";
        case kBlockSpawner:       return "spawner";
        case kBlockRotatedDeepslate:return "deepslate";
        case kBlockBeacon:        return "beacon";
        case kBlockShulker:       return "shulker_box";
        case kBlockBarrel:        return "barrel";
        case kBlockHopper:        return "hopper";
        case kBlockFurnace:       return "furnace";
        case kBlockEnderChest:    return "ender_chest";
        case kBlockTrappedChest:  return "trapped_chest";
        case kBlockDispenser:     return "dispenser";
        case kBlockDropper:       return "dropper";
        case kBlockBlastFurnace:  return "blast_furnace";
        case kBlockSmoker:        return "smoker";
        case kBlockCrafter:       return "crafter";
        case kBlockBrewingStand:  return "brewing_stand";
        case kBlockKelp:          return "kelp";
        case kBlockKelpPlant:     return "kelp_plant";
        case kBlockLargeAmethystBud:return "large_amethyst_bud";
        case kBlockAmethystCluster:return "amethyst_cluster";
        default:                  return "";
    }
}

inline const char* BlockKindLabel(std::uint8_t kind) {
    switch (kind) {
        case kBlockDiamond:       return "Diamond";
        case kBlockAncientDebris: return "Ancient debris";
        case kBlockGold:          return "Gold";
        case kBlockIron:          return "Iron";
        case kBlockEmerald:       return "Emerald";
        case kBlockRedstone:      return "Redstone";
        case kBlockLapis:         return "Lapis";
        case kBlockCopper:        return "Copper";
        case kBlockCoal:          return "Coal";
        case kBlockQuartz:        return "Nether quartz";
        case kBlockChest:         return "Chests";
        case kBlockSpawner:       return "Spawners";
        case kBlockRotatedDeepslate:return "Rotated deepslate";
        case kBlockBeacon:        return "Beacons";
        case kBlockShulker:       return "Shulker boxes";
        case kBlockBarrel:        return "Barrels";
        case kBlockHopper:        return "Hoppers";
        case kBlockFurnace:       return "Furnaces";
        case kBlockEnderChest:    return "Ender chests";
        case kBlockTrappedChest:  return "Trapped chests";
        case kBlockDispenser:     return "Dispensers";
        case kBlockDropper:       return "Droppers";
        case kBlockBlastFurnace:  return "Blast furnaces";
        case kBlockSmoker:        return "Smokers";
        case kBlockCrafter:       return "Crafters";
        case kBlockBrewingStand:  return "Brewing stands";
        case kBlockKelp:          return "Kelp tops";
        case kBlockKelpPlant:     return "Kelp stems";
        case kBlockLargeAmethystBud:return "Large amethyst buds";
        case kBlockAmethystCluster:return "Amethyst clusters";
        default:                  return "?";
    }
}
// Specific container names must not collapse into chest/furnace. Keep the
// existing ore matching (including deepslate variants) and coloured shulkers.
inline std::int8_t ClassifyBlock(std::string_view key) {
    constexpr std::string_view prefix="block.minecraft.";
    if(!key.starts_with(prefix)) return -1;
    const auto name=key.substr(prefix.size());
    for(std::uint8_t kind=0;kind<kBlockKindCount;++kind) {
        const std::string_view match=BlockKindKey(kind);
        if(kind<kBlockChest) {
            if(name==match||(name.size()>match.size()&&name.ends_with(match)&&name[name.size()-match.size()-1]=='_')) return static_cast<std::int8_t>(kind);
        } else if(kind==kBlockShulker) {
            if(name==match||name.ends_with("_shulker_box")) return static_cast<std::int8_t>(kind);
        } else if(name==match) return static_cast<std::int8_t>(kind);
    }
    return -1;
}
inline constexpr std::uint32_t kMaxEffects  = 16;
inline constexpr std::uint32_t kNameLen     = 40;
inline constexpr std::uint32_t kAddrLen     = 64;

// Equipment slots, in the order the HUD draws them. Kept independent of
// Minecraft's own EquipmentSlot ordinals, which are not stable across versions.
enum EquipSlot : std::uint8_t {
    kSlotHead = 0,
    kSlotChest,
    kSlotLegs,
    kSlotFeet,
    kSlotMainHand,
    kSlotOffHand,
    kSlotCount,
};

// ---------------------------------------------------------------------------
// Agent -> overlay status, so the UI can say something useful instead of just
// going blank when a mapping lookup fails.
// ---------------------------------------------------------------------------
enum class AgentStatus : std::uint32_t {
    Booting            = 0, // Agent_OnAttach ran, sampler not up yet
    ResolvingMappings  = 1, // looking for Minecraft.class under each mapping set
    Running            = 2, // publishing real snapshots
    NoClientInstance   = 3, // class found, Minecraft.getInstance() still null
    MappingsFailed     = 4, // no candidate mapping set matched
    Detaching          = 5, // shutdown in progress, do not trust the snapshot
    WaitingForFrame    = 6, // worker alive, no coherent camera/entity frame ready
};

// Overlay -> agent control bits (written by the overlay, polled by the sampler).
enum ControlFlags : std::uint32_t {
    kCtrlNone          = 0u,
    kCtrlRequestUnload = 1u << 0, // ask the sampler to stop and detach cleanly
    kCtrlPauseSampling = 1u << 1, // stop JNI work but stay resident
    kCtrlAutoTotem     = 1u << 2, // keep a totem in the offhand
    kCtrlEntityDetails = 1u << 3, // langsame Nametag-Daten nur bei Bedarf erfassen
};

enum EntityFlags : std::uint8_t {
    kEntityIsPlayer  = 1u << 0,
    kEntityIsHostile = 1u << 1,
    kEntityIsSelf    = 1u << 2,
    kEntityIsLiving  = 1u << 3,
    kEntityHasBounds = 1u << 4,
};

enum RigKind : std::uint8_t { kRigNone, kRigHumanoid, kRigEnderman };
enum EntityPose : std::uint8_t { kPoseStanding, kPoseCrouching, kPoseSwimming, kPoseGliding, kPoseSleeping };
inline constexpr std::uint8_t kRigRiding = 1u << 0;

// ---------------------------------------------------------------------------
// One equipment slot. maxDamage == 0 means the item has no durability bar
// (a block, food, a totem), which is different from "full durability".
// ---------------------------------------------------------------------------
struct ItemSnapshot {
    char         name[kNameLen];
    std::int32_t damage;      // damage taken, 0 == pristine
    std::int32_t maxDamage;   // 0 == not damageable
    std::int32_t count;
    std::uint8_t present;     // 0 == slot empty
    std::uint8_t _pad;
    std::uint16_t iconIndex;  // eingebetteter Vanilla-Atlas; 0 = unbekanntes Item
};

static_assert(std::is_trivially_copyable_v<ItemSnapshot>);
static_assert(sizeof(ItemSnapshot) == 56, "ItemSnapshot layout changed - bump kSharedVersion");

struct EntityDetailsSnapshot {
    std::int32_t ping = -1;   // echter Tablisten-Ping dieses Spielers; -1 = unbekannt
    std::int32_t armorValue = 0;
    ItemSnapshot equipment[kSlotCount]{};
};
static_assert(std::is_trivially_copyable_v<EntityDetailsSnapshot>);
static_assert(sizeof(EntityDetailsSnapshot) == 344);

// Interpolierter Weltanker und Geometrie bleiben schnell; Details stammen aus
// dem langsamen Worker-Cache. Rendering ruft keine JNI-Funktionen auf.
struct EntitySnapshot {
    double        x, y, z;
    combat::Identity uuid;      // Sampler-Cache; Freund-Marker benötigen kein Render-JNI.
    float         width;
    float         height;
    float         health;
    float         maxHealth;
    float         distance;
    float         yaw;
    float         boundsMinX, boundsMinY, boundsMinZ;
    float         boundsMaxX, boundsMaxY, boundsMaxZ;
    float         modelHeight, headPitch, walkPhase, walkSpeed;
    std::uint32_t entityId;
    std::uint8_t  flags;
    std::uint8_t  rigKind, pose, rigFlags;
    char          name[kNameLen];
    EntityDetailsSnapshot details;
};
static_assert(std::is_trivially_copyable_v<EntitySnapshot>);
static_assert(sizeof(EntitySnapshot) == 496, "EntitySnapshot layout changed - bump kSharedVersion");

// ---------------------------------------------------------------------------
// One active status effect. durationTicks == -1 means infinite (beacon/command).
// amplifier is zero-based: 0 renders as "I", 1 as "II".
// ---------------------------------------------------------------------------
struct EffectSnapshot {
    char         name[kNameLen];
    std::int32_t amplifier;
    std::int32_t durationTicks;
    std::uint8_t ambient;
    std::uint8_t _pad[3];
};

static_assert(std::is_trivially_copyable_v<EffectSnapshot>);
static_assert(sizeof(EffectSnapshot) == 52, "EffectSnapshot layout changed - bump kSharedVersion");

// One located block. Position is the block's minimum corner, so the overlay can
// draw a unit cube from it without needing any Minecraft geometry.
struct BlockSnapshot {
    float        x, y, z;
    std::uint8_t kind;      // BlockKind
    std::uint8_t kelpHeight; // Observed column height below a kelp top, capped at 64.
    std::uint8_t _pad[2];
};

static_assert(std::is_trivially_copyable_v<BlockSnapshot>);
static_assert(sizeof(BlockSnapshot) == 16, "BlockSnapshot layout changed - bump kSharedVersion");

// ---------------------------------------------------------------------------
// One sampled frame of the client's world state.
//
// Note the camera is published as raw parameters (position + yaw/pitch/fov)
// rather than a baked view-projection matrix. Pulling Minecraft's actual matrix
// stack out over JNI is both mapping-fragile and timing-sensitive; rebuilding
// the matrix on the overlay side from four scalars is stable across versions.
// See render_math.hpp for the reconstruction.
// ---------------------------------------------------------------------------
struct WorldSnapshot {
    std::uint64_t frameIndex;
    std::uint64_t renderEpoch; // Kamera-/Depth-Zuordnung, keine Rate-/Tick-Schaetzung.
    std::int64_t  captureQpc;   // QueryPerformanceCounter at capture time

    // Camera (from Minecraft's active Camera, not the player - they differ in
    // third person and while the player is riding something).
    double camX, camY, camZ;
    float yawDeg;               // Minecraft yaw: 0 = +Z (south), increases clockwise
    float pitchDeg;             // Minecraft pitch: positive = looking down
    float fovDeg;               // vertical FOV, degrees
    float nearPlane, farPlane;
    float partialTick;          // same interpolation fraction as the active camera
    float bobPhase, bobAmplitude;

    // Local player
    float posX, posY, posZ;
    float health, maxHealth, absorption;
    float saturation;
    std::int32_t foodLevel;
    std::int32_t armorValue;
    std::int32_t fps;
    std::int32_t ping = -1;
    // Camera bob is reconstructed from sampled walk distance and amplitude.
    std::uint8_t viewBobbing;
    std::uint8_t _padBob[3];
    char  username[kNameLen];
    char  dimension[kNameLen];
    char  serverAddress[kAddrLen];
    char  gameVersion[16];
    char  mappingProfile[40]; // detected once by the JNI worker

    // Equipment, indexed by EquipSlot.
    ItemSnapshot equipment[kSlotCount];

    // Active status effects.
    EffectSnapshot effects[kMaxEffects];
    std::uint32_t  effectCount;

    // Telemetry. Heap figures come from java.lang.Runtime, which is a bootstrap
    // class - no mapping involved, so these are reliable on any client.
    std::int64_t heapUsedBytes;
    std::int64_t heapMaxBytes;

    // Crosshair target. Minecraft computes this itself against the player's
    // interaction range, so a non-zero targetEntityId means "in reach" exactly
    // as the game defines it, rather than as we guess it.
    std::uint32_t targetEntityId;
    std::uint8_t  hasTarget;
    std::uint8_t  _pad1[3];
    float         targetDistance;
    float         targetHealth;
    float         targetMaxHealth;
    char          targetName[kNameLen];

    std::uint32_t  entityCount;
    std::uint32_t  blockCount;
    // Coherent scan metadata travels with the blocks, not separate atomics.
    // A finder evaluates each finished sweep once, never a partial result.
    std::uint64_t blockScanId;
    std::uint32_t blockScanMask;
    std::int32_t blockScanX,blockScanY,blockScanZ;
    std::uint32_t blockScanRadius;
    std::uint8_t blockScanComplete,blockScanTruncated;
    std::uint8_t _padScan[2];
    EntitySnapshot entities[kMaxEntities];
    BlockSnapshot  blocks[kMaxBlocks];
};

static_assert(std::is_trivially_copyable_v<WorldSnapshot>);

// ---------------------------------------------------------------------------
// The mapped block itself.
//
// Cross-process std::atomic is only sound because both binaries are built by
// the same compiler for the same ABI and these types are always lock-free
// (a lock-based atomic would keep its mutex in per-process memory).
// ---------------------------------------------------------------------------
enum class ScanState : std::uint32_t { Idle, Scanning, Ready, MissingBindings };
struct SharedBlock {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t agentPid;
    std::uint32_t _pad0;

    std::atomic<std::uint32_t> sequence;       // seqlock, even = stable
    std::atomic<std::uint32_t> heartbeat;      // publish + worker liveness, also while awaiting a camera frame
    std::atomic<std::uint32_t> controlFlags;   // overlay -> agent
    std::atomic<std::uint32_t> status;         // AgentStatus, agent -> overlay

    // Block scan request, overlay -> agent. Scanning is expensive (a cube of
    // getBlockState calls), so it stays off until the overlay asks for it.
    std::atomic<std::uint32_t> scanMask;       // bit per BlockKind; 0 = disabled
    std::atomic<std::uint32_t> scanRadius;     // blocks
    std::atomic<std::uint32_t> scanIntervalMs;
    std::atomic<std::uint32_t> scanState;
    std::atomic<std::uint32_t> scanProgress; // 0..100, Agent -> Overlay.

    SharedMailbox<CombatRequest> combatRequest; // Overlay -> Client-Tick, eigener Writer.
    SharedMailbox<CombatReport> combatReport;   // Client-Tick -> Overlay, eigener Writer.
    SharedMailbox<items::Request> itemRequest;
    SharedMailbox<items::Report> itemReport;
    SharedMailbox<base::Request> baseRequest;
    SharedMailbox<base::Report> baseReport;
    SharedMailbox<knife::Request> knifeRequest;
    SharedMailbox<knife::Report> knifeReport;
    WorldSnapshot snapshot;
};

static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "SharedBlock relies on lock-free atomics for cross-process safety");

// ---------------------------------------------------------------------------
// Seqlock. The agent is the only writer; the overlay only ever reads. This
// keeps the snapshot tear-free without putting a kernel object (and its
// contention) on the 60 Hz path.
// ---------------------------------------------------------------------------
inline void PublishSnapshot(SharedBlock* block, const WorldSnapshot& src) noexcept {
    const std::uint32_t seq = block->sequence.load(std::memory_order_relaxed);

    block->sequence.store(seq + 1, std::memory_order_relaxed); // -> odd: "writing"
    std::atomic_thread_fence(std::memory_order_release);

    std::memcpy(&block->snapshot, &src, sizeof(WorldSnapshot));

    std::atomic_thread_fence(std::memory_order_release);
    block->sequence.store(seq + 2, std::memory_order_relaxed); // -> even: "stable"

    block->heartbeat.fetch_add(1, std::memory_order_release);
}

inline bool TryReadSnapshot(const SharedBlock* block, WorldSnapshot& out, int attempts = 8) noexcept {
    for (int i = 0; i < attempts; ++i) {
        const std::uint32_t before = block->sequence.load(std::memory_order_relaxed);
        if (before & 1u) continue; // writer mid-update
        std::atomic_thread_fence(std::memory_order_acquire);

        std::memcpy(&out, &block->snapshot, sizeof(WorldSnapshot));

        std::atomic_thread_fence(std::memory_order_acquire);
        if (block->sequence.load(std::memory_order_relaxed) == before) return true;
    }
    return false;
}

// Safe fixed-buffer string copy for the POD fields above.
template <std::size_t N>
inline void CopyField(char (&dst)[N], const char* src) noexcept {
    if (!src) { dst[0] = '\0'; return; }
    std::size_t i = 0;
    for (; i + 1 < N && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

} // namespace mcov
