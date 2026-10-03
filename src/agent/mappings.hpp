// Semantic JNI member schema and historical mapping examples.
// Runtime selection uses generated_profiles.hpp exclusively: explicit release
// IDs, audited descriptors and namespace-specific member names. kMojang is the
// generator seed; the legacy Yarn/intermediary examples below are not probed.
// See docs/compatibility.md for supported versions and regeneration.
#pragma once

namespace mcov::mappings {

struct Member {
    const char* name;
    const char* signature;
};

struct ClassNames {
    const char* client;       // Minecraft / MinecraftClient / class_310
    const char* localPlayer;  // LocalPlayer / ClientPlayerEntity
    const char* clientLevel;  // ClientLevel / ClientWorld
    const char* entity;       // Entity
    const char* livingEntity; // LivingEntity
    const char* player;       // Player / PlayerEntity
    const char* enemyMarker;  // Enemy / Monster marker interface
    const char* vec3;         // Vec3 / Vec3d
    const char* camera;       // Camera
    const char* gameRenderer; // GameRenderer
    const char* component;    // Component / Text
    const char* options;      // Options / GameOptions
    const char* optionValue;  // OptionInstance / SimpleOption
    const char* serverData;   // ServerData / ServerInfo
    const char* itemStack;    // ItemStack
    const char* equipSlot;    // EquipmentSlot (enum)
    const char* effectInst;   // MobEffectInstance / StatusEffectInstance
    const char* blockState;   // BlockState
    const char* block;        // Block (AbstractBlock for the name)
    const char* blockPosMut;  // BlockPos.MutableBlockPos / BlockPos$Mutable
    const char* interactionMgr; // MultiPlayerGameMode / ClientPlayerInteractionManager
    const char* slotAction;     // ClickType / SlotActionType
    const char* playerInv;      // Inventory / PlayerInventory
    const char* item;           // Item
    const char* connection;    // ClientPacketListener
    const char* playerInfo;    // PlayerInfo
};

struct MappingSet {
    const char* id;
    ClassNames  classes;

    // Client singleton
    Member methodGetInstance;  // static, returns the client class
    Member fieldPlayer;
    Member fieldLevel;
    Member fieldGameRenderer;
    Member fieldFps;           // static int, optional

    // GameRenderer / Camera. The camera's position is a *field* in every mapping
    // set checked so far; the method form is tried first and is allowed to miss.
    Member methodGetCamera;
    Member methodCameraPosition;
    Member fieldCameraPosition;
    Member methodGetYaw, methodGetPitch;   // read off the Camera, not the Entity

    // Entity
    Member methodGetX, methodGetY, methodGetZ;
    Member methodGetId;
    Member methodGetName;      // -> Component/Text
    Member methodGetBbWidth, methodGetBbHeight;

    // LivingEntity
    Member methodGetHealth, methodGetMaxHealth, methodGetAbsorption, methodGetArmor;

    // ClientLevel
    Member methodEntitiesForRendering;  // -> Iterable
    Member methodDimension;             // -> ResourceKey, read via Object.toString()

    // Component -> String
    Member methodComponentToString;

    // Vec3/Vec3d public doubles
    Member fieldVecX, fieldVecY, fieldVecZ;

    // FOV chain: client.options -> options.fov() -> option.get()
    Member fieldOptions;
    Member methodFov;
    Member methodOptionGet;

    // Current server (null in singleplayer)
    Member methodCurrentServer;
    Member fieldServerAddress;

    // --- Equipment -------------------------------------------------------
    // LivingEntity.getEquippedStack(EquipmentSlot) / getItemBySlot.
    Member methodGetEquipped;
    // The EquipmentSlot enum constants, read as static fields.
    //
    // Enum.name() is NOT usable here: intermediary renames the constant fields
    // (HEAD -> field_6169), so name() does not return "HEAD" at runtime.
    // Reading the mapped static field is the deterministic route.
    Member slotHead, slotChest, slotLegs, slotFeet, slotMainHand, slotOffHand;
    // ItemStack
    Member methodItemIsEmpty, methodItemGetDamage, methodItemGetMaxDamage;
    Member methodItemIsDamageable, methodItemGetCount, methodItemGetName;

    // --- Status effects ---------------------------------------------------
    Member methodGetStatusEffects;   // -> Collection<EffectInstance>
    Member methodEffectAmplifier;
    Member methodEffectDuration;
    Member methodEffectTranslationKey; // "effect.minecraft.speed"
    Member methodEffectIsAmbient;

    // --- Crosshair target (reach) ----------------------------------------
    Member fieldTargetedEntity;

    // Entity's own yaw. Distinct from methodGetYaw above, which is bound on the
    // Camera - the two have different names in every mapping set.
    Member methodEntityYaw;

    // GameRenderer.getFov(Camera, tickDelta, changingFov) - the REAL render FOV,
    // including the sprint/speed/bow modifiers that Options.fov() omits.
    Member methodRealFov;
    // Options.bobView controls the sampled camera bob transform.
    Member fieldBobView;

    // --- Block scanning ---------------------------------------------------
    Member methodGetBlockState;   // BlockView.getBlockState(BlockPos) -> BlockState
    Member methodStateIsAir;
    Member methodStateGetBlock;
    Member methodBlockTransKey;   // AbstractBlock.getTranslationKey()
    Member methodMutableSet;      // BlockPos.Mutable.set(int,int,int)

    // --- Auto totem (the only path that WRITES to the client) -------------
    Member fieldInteractionMgr;
    Member fieldCurrentScreen;    // null => no GUI open
    Member fieldScreenHandler;    // PlayerEntity.containerMenu / currentScreenHandler
    Member fieldSyncId;
    Member fieldInventory;
    Member methodInvGetStack;
    Member methodInvSize;
    Member methodStackGetItem;
    Member methodItemTransKey;
    Member methodClickSlot;
    Member fieldSwapAction;       // SlotActionType.SWAP static
    Member methodConnection, methodEntityUuid, methodPlayerInfo, methodLatency, methodEntityById;
};

// --- Mojang official mappings (NeoForge / Forge runtime) -------------------
inline constexpr MappingSet kMojang{
    "mojmap",
    {
        "net/minecraft/client/Minecraft",
        "net/minecraft/client/player/LocalPlayer",
        "net/minecraft/client/multiplayer/ClientLevel",
        "net/minecraft/world/entity/Entity",
        "net/minecraft/world/entity/LivingEntity",
        "net/minecraft/world/entity/player/Player",
        "net/minecraft/world/entity/monster/Enemy",
        "net/minecraft/world/phys/Vec3",
        "net/minecraft/client/Camera",
        "net/minecraft/client/renderer/GameRenderer",
        "net/minecraft/network/chat/Component",
        "net/minecraft/client/Options",
        "net/minecraft/client/OptionInstance",
        "net/minecraft/client/multiplayer/ServerData",
        "net/minecraft/world/item/ItemStack",
        "net/minecraft/world/entity/EquipmentSlot",
        "net/minecraft/world/effect/MobEffectInstance",
        "net/minecraft/world/level/block/state/BlockState",
        "net/minecraft/world/level/block/Block",
        "net/minecraft/core/BlockPos$MutableBlockPos",
        "net/minecraft/client/multiplayer/MultiPlayerGameMode",
        "net/minecraft/world/inventory/ClickType",
        "net/minecraft/world/entity/player/Inventory",
        "net/minecraft/world/item/Item",
        "net/minecraft/client/multiplayer/ClientPacketListener",
        "net/minecraft/client/multiplayer/PlayerInfo",
    },

    {"getInstance",  "()Lnet/minecraft/client/Minecraft;"},
    {"player",       "Lnet/minecraft/client/player/LocalPlayer;"},
    {"level",        "Lnet/minecraft/client/multiplayer/ClientLevel;"},
    {"gameRenderer", "Lnet/minecraft/client/renderer/GameRenderer;"},
    {"fps",          "I"},

    {"getMainCamera", "()Lnet/minecraft/client/Camera;"},
    {"getPosition",   "()Lnet/minecraft/world/phys/Vec3;"},
    {"position",      "Lnet/minecraft/world/phys/Vec3;"},
    {"getYRot", "()F"}, {"getXRot", "()F"},

    {"getX", "()D"}, {"getY", "()D"}, {"getZ", "()D"},
    {"getId", "()I"},
    {"getName", "()Lnet/minecraft/network/chat/Component;"},
    {"getBbWidth", "()F"}, {"getBbHeight", "()F"},

    {"getHealth", "()F"}, {"getMaxHealth", "()F"},
    {"getAbsorptionAmount", "()F"}, {"getArmorValue", "()I"},

    {"entitiesForRendering", "()Ljava/lang/Iterable;"},
    {"dimension", "()Lnet/minecraft/resources/ResourceKey;"},

    {"getString", "()Ljava/lang/String;"},

    {"x", "D"}, {"y", "D"}, {"z", "D"},

    {"options", "Lnet/minecraft/client/Options;"},
    {"fov",     "()Lnet/minecraft/client/OptionInstance;"},
    {"get",     "()Ljava/lang/Object;"},

    {"getCurrentServer", "()Lnet/minecraft/client/multiplayer/ServerData;"},
    {"ip",               "Ljava/lang/String;"},

    // Equipment. NOTE: the MojMap names below are written from knowledge of the
    // MojMap naming scheme, not generated from a mapping file the way the
    // intermediary set is. They soft-fail individually if wrong.
    {"getItemBySlot", "(Lnet/minecraft/world/entity/EquipmentSlot;)Lnet/minecraft/world/item/ItemStack;"},
    {"HEAD",     "Lnet/minecraft/world/entity/EquipmentSlot;"},
    {"CHEST",    "Lnet/minecraft/world/entity/EquipmentSlot;"},
    {"LEGS",     "Lnet/minecraft/world/entity/EquipmentSlot;"},
    {"FEET",     "Lnet/minecraft/world/entity/EquipmentSlot;"},
    {"MAINHAND", "Lnet/minecraft/world/entity/EquipmentSlot;"},
    {"OFFHAND",  "Lnet/minecraft/world/entity/EquipmentSlot;"},
    {"isEmpty", "()Z"}, {"getDamageValue", "()I"}, {"getMaxDamage", "()I"},
    {"isDamageableItem", "()Z"}, {"getCount", "()I"},
    {"getHoverName", "()Lnet/minecraft/network/chat/Component;"},

    {"getActiveEffects", "()Ljava/util/Collection;"},
    {"getAmplifier", "()I"},
    {"getDuration", "()I"},
    {"getDescriptionId", "()Ljava/lang/String;"},
    {"isAmbient", "()Z"},

    {"crosshairPickEntity", "Lnet/minecraft/world/entity/Entity;"},
    {"getYRot", "()F"},                               // Entity.getYaw
    {"getFov",  "(Lnet/minecraft/client/Camera;FZ)F"},
    {"bobView", "Lnet/minecraft/client/OptionInstance;"},
    {"getBlockState",    "(Lnet/minecraft/core/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;"},
    {"isAir",            "()Z"},
    {"getBlock",         "()Lnet/minecraft/world/level/block/Block;"},
    {"getDescriptionId", "()Ljava/lang/String;"},
    {"set",              "(III)Lnet/minecraft/core/BlockPos$MutableBlockPos;"},

    {"gameMode",         "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;"},
    {"screen",           "Lnet/minecraft/client/gui/screens/Screen;"},
    {"containerMenu",    "Lnet/minecraft/world/inventory/AbstractContainerMenu;"},
    {"containerId",      "I"},
    {"inventory",        "Lnet/minecraft/world/entity/player/Inventory;"},
    {"getItem",          "(I)Lnet/minecraft/world/item/ItemStack;"},
    {"getContainerSize", "()I"},
    {"getItem",          "()Lnet/minecraft/world/item/Item;"},
    {"getDescriptionId", "()Ljava/lang/String;"},
    {"handleInventoryMouseClick", "(IIILnet/minecraft/world/inventory/ClickType;Lnet/minecraft/world/entity/player/Player;)V"},
    {"SWAP",             "Lnet/minecraft/world/inventory/ClickType;"},
    {"getConnection", "()Lnet/minecraft/client/multiplayer/ClientPacketListener;"},
    {"getUUID", "()Ljava/util/UUID;"},
    {"getPlayerInfo", "(Ljava/util/UUID;)Lnet/minecraft/client/multiplayer/PlayerInfo;"},
    {"getLatency", "()I"},
    {"getEntity", "(I)Lnet/minecraft/world/entity/Entity;"},
};

// --- Yarn (Fabric *development* runtime only) ------------------------------
inline constexpr MappingSet kYarn{
    "yarn",
    {
        "net/minecraft/client/MinecraftClient",
        "net/minecraft/client/network/ClientPlayerEntity",
        "net/minecraft/client/world/ClientWorld",
        "net/minecraft/entity/Entity",
        "net/minecraft/entity/LivingEntity",
        "net/minecraft/entity/player/PlayerEntity",
        "net/minecraft/entity/mob/Monster",
        "net/minecraft/util/math/Vec3d",
        "net/minecraft/client/render/Camera",
        "net/minecraft/client/render/GameRenderer",
        "net/minecraft/text/Text",
        "net/minecraft/client/option/GameOptions",
        "net/minecraft/client/option/SimpleOption",
        "net/minecraft/client/network/ServerInfo",
        "net/minecraft/item/ItemStack",
        "net/minecraft/entity/EquipmentSlot",
        "net/minecraft/entity/effect/StatusEffectInstance",
        "net/minecraft/block/BlockState",
        "net/minecraft/block/AbstractBlock",
        "net/minecraft/util/math/BlockPos$Mutable",
        "net/minecraft/client/network/ClientPlayerInteractionManager",
        "net/minecraft/screen/slot/SlotActionType",
        "net/minecraft/entity/player/PlayerInventory",
        "net/minecraft/item/Item",
    },

    {"getInstance",  "()Lnet/minecraft/client/MinecraftClient;"},
    {"player",       "Lnet/minecraft/client/network/ClientPlayerEntity;"},
    {"world",        "Lnet/minecraft/client/world/ClientWorld;"},
    {"gameRenderer", "Lnet/minecraft/client/render/GameRenderer;"},
    {"currentFps",   "I"},

    {"getCamera", "()Lnet/minecraft/client/render/Camera;"},
    {"getPos",    "()Lnet/minecraft/util/math/Vec3d;"},
    {"pos",       "Lnet/minecraft/util/math/Vec3d;"},
    {"getYaw", "()F"}, {"getPitch", "()F"},

    {"getX", "()D"}, {"getY", "()D"}, {"getZ", "()D"},
    {"getId", "()I"},
    {"getName", "()Lnet/minecraft/text/Text;"},
    {"getWidth", "()F"}, {"getHeight", "()F"},

    {"getHealth", "()F"}, {"getMaxHealth", "()F"},
    {"getAbsorptionAmount", "()F"}, {"getArmor", "()I"},

    {"getEntities", "()Ljava/lang/Iterable;"},
    {"getRegistryKey", "()Lnet/minecraft/registry/RegistryKey;"},

    {"getString", "()Ljava/lang/String;"},

    {"x", "D"}, {"y", "D"}, {"z", "D"},

    {"options",  "Lnet/minecraft/client/option/GameOptions;"},
    {"getFov",   "()Lnet/minecraft/client/option/SimpleOption;"},
    {"getValue", "()Ljava/lang/Object;"},

    {"getCurrentServerEntry", "()Lnet/minecraft/client/network/ServerInfo;"},
    {"address",               "Ljava/lang/String;"},

    {"getEquippedStack", "(Lnet/minecraft/entity/EquipmentSlot;)Lnet/minecraft/item/ItemStack;"},
    {"HEAD",     "Lnet/minecraft/entity/EquipmentSlot;"},
    {"CHEST",    "Lnet/minecraft/entity/EquipmentSlot;"},
    {"LEGS",     "Lnet/minecraft/entity/EquipmentSlot;"},
    {"FEET",     "Lnet/minecraft/entity/EquipmentSlot;"},
    {"MAINHAND", "Lnet/minecraft/entity/EquipmentSlot;"},
    {"OFFHAND",  "Lnet/minecraft/entity/EquipmentSlot;"},
    {"isEmpty", "()Z"}, {"getDamage", "()I"}, {"getMaxDamage", "()I"},
    {"isDamageable", "()Z"}, {"getCount", "()I"},
    {"getName", "()Lnet/minecraft/text/Text;"},

    {"getStatusEffects", "()Ljava/util/Collection;"},
    {"getAmplifier", "()I"},
    {"getDuration", "()I"},
    {"getTranslationKey", "()Ljava/lang/String;"},
    {"isAmbient", "()Z"},

    {"targetedEntity", "Lnet/minecraft/entity/Entity;"},
    {"getYaw", "()F"},
    {"getFov",  "(Lnet/minecraft/client/render/Camera;FZ)F"},
    {"bobView", "Lnet/minecraft/client/option/SimpleOption;"},
    {"getBlockState",     "(Lnet/minecraft/util/math/BlockPos;)Lnet/minecraft/block/BlockState;"},
    {"isAir",             "()Z"},
    {"getBlock",          "()Lnet/minecraft/block/Block;"},
    {"getTranslationKey", "()Ljava/lang/String;"},
    {"set",               "(III)Lnet/minecraft/util/math/BlockPos$Mutable;"},

    {"interactionManager",   "Lnet/minecraft/client/network/ClientPlayerInteractionManager;"},
    {"currentScreen",        "Lnet/minecraft/client/gui/screen/Screen;"},
    {"currentScreenHandler", "Lnet/minecraft/screen/ScreenHandler;"},
    {"syncId",               "I"},
    {"inventory",            "Lnet/minecraft/entity/player/PlayerInventory;"},
    {"getStack",             "(I)Lnet/minecraft/item/ItemStack;"},
    {"size",                 "()I"},
    {"getItem",              "()Lnet/minecraft/item/Item;"},
    {"getTranslationKey",    "()Ljava/lang/String;"},
    {"clickSlot",            "(IIILnet/minecraft/screen/slot/SlotActionType;Lnet/minecraft/entity/player/PlayerEntity;)V"},
    {"SWAP",                 "Lnet/minecraft/screen/slot/SlotActionType;"},
};

// --- Fabric intermediary (what a SHIPPED Fabric client runs on) ------------
// Generated from Yarn 1.21.11+build.6. See "Provenance" at the top of the file.
inline constexpr MappingSet kIntermediary{
    "intermediary",
    {
        "net/minecraft/class_310",  // MinecraftClient
        "net/minecraft/class_746",  // ClientPlayerEntity
        "net/minecraft/class_638",  // ClientWorld
        "net/minecraft/class_1297", // Entity
        "net/minecraft/class_1309", // LivingEntity
        "net/minecraft/class_1657", // PlayerEntity
        "net/minecraft/class_1569", // Monster
        "net/minecraft/class_243",  // Vec3d
        "net/minecraft/class_4184", // Camera
        "net/minecraft/class_757",  // GameRenderer
        "net/minecraft/class_2561", // Text
        "net/minecraft/class_315",  // GameOptions
        "net/minecraft/class_7172", // SimpleOption
        "net/minecraft/class_642",  // ServerInfo
        "net/minecraft/class_1799", // ItemStack
        "net/minecraft/class_1304", // EquipmentSlot
        "net/minecraft/class_1293", // StatusEffectInstance
        "net/minecraft/class_2680",  // BlockState
        "net/minecraft/class_4970",  // AbstractBlock (getTranslationKey lives here)
        "net/minecraft/class_2338$class_2339", // BlockPos.Mutable
        "net/minecraft/class_636",   // ClientPlayerInteractionManager
        "net/minecraft/class_1713",  // SlotActionType
        "net/minecraft/class_1661",  // PlayerInventory
        "net/minecraft/class_1792",  // Item
    },

    {"method_1551", "()Lnet/minecraft/class_310;"},  // getInstance
    {"field_1724",  "Lnet/minecraft/class_746;"},    // player
    {"field_1687",  "Lnet/minecraft/class_638;"},    // world
    {"field_1773",  "Lnet/minecraft/class_757;"},    // gameRenderer
    {"field_1738",  "I"},                            // currentFps

    {"method_19418", "()Lnet/minecraft/class_4184;"}, // getCamera
    {"getPos",       "()Lnet/minecraft/class_243;"},  // no method form here; allowed to miss
    {"field_18712",  "Lnet/minecraft/class_243;"},    // Camera.pos
    {"method_19330", "()F"}, {"method_19329", "()F"}, // Camera getYaw / getPitch

    {"method_23317", "()D"}, {"method_23318", "()D"}, {"method_23321", "()D"},
    {"getId", "()I"},                                 // unmapped; keeps its name
    {"method_5477", "()Lnet/minecraft/class_2561;"},  // Nameable.getName
    {"method_17681", "()F"}, {"method_17682", "()F"}, // getWidth / getHeight

    {"method_6032", "()F"}, {"method_6063", "()F"},   // getHealth / getMaxHealth
    {"method_6067", "()F"}, {"method_6096", "()I"},   // getAbsorptionAmount / getArmor

    {"method_18112", "()Ljava/lang/Iterable;"},       // ClientWorld.getEntities
    {"method_27983", "()Lnet/minecraft/class_5321;"}, // World.getRegistryKey

    {"getString", "()Ljava/lang/String;"},            // unmapped; keeps its name

    {"field_1352", "D"}, {"field_1351", "D"}, {"field_1350", "D"}, // Vec3d x/y/z

    {"field_1690",  "Lnet/minecraft/class_315;"},     // options
    {"method_41808", "()Lnet/minecraft/class_7172;"}, // getFov
    {"method_41753", "()Ljava/lang/Object;"},         // SimpleOption.getValue

    {"method_1558", "()Lnet/minecraft/class_642;"},   // getCurrentServerEntry
    {"field_3761",  "Ljava/lang/String;"},            // ServerInfo.address

    // getEquippedStack(EquipmentSlot)
    {"method_6118", "(Lnet/minecraft/class_1304;)Lnet/minecraft/class_1799;"},
    // EquipmentSlot constants, as static fields - Enum.name() does not work here.
    {"field_6169", "Lnet/minecraft/class_1304;"},     // HEAD
    {"field_6174", "Lnet/minecraft/class_1304;"},     // CHEST
    {"field_6172", "Lnet/minecraft/class_1304;"},     // LEGS
    {"field_6166", "Lnet/minecraft/class_1304;"},     // FEET
    {"field_6173", "Lnet/minecraft/class_1304;"},     // MAINHAND
    {"field_6171", "Lnet/minecraft/class_1304;"},     // OFFHAND
    {"method_7960", "()Z"},                           // ItemStack.isEmpty
    {"method_7919", "()I"},                           // ItemStack.getDamage
    {"method_7936", "()I"},                           // ItemStack.getMaxDamage
    {"method_7963", "()Z"},                           // ItemStack.isDamageable
    {"method_7947", "()I"},                           // ItemStack.getCount
    {"method_7964", "()Lnet/minecraft/class_2561;"},  // ItemStack.getName

    {"method_6026", "()Ljava/util/Collection;"},      // getStatusEffects
    {"method_5578", "()I"},                           // getAmplifier
    {"method_5584", "()I"},                           // getDuration
    {"method_5586", "()Ljava/lang/String;"},          // getTranslationKey
    {"method_5591", "()Z"},                           // isAmbient

    {"field_1692", "Lnet/minecraft/class_1297;"},     // MinecraftClient.targetedEntity
    {"method_36454", "()F"},                          // Entity.getYaw
    {"method_3196", "(Lnet/minecraft/class_4184;FZ)F"},  // GameRenderer.getFov
    {"field_1891",  "Lnet/minecraft/class_7172;"},       // Options.bobView
    {"method_8320",  "(Lnet/minecraft/class_2338;)Lnet/minecraft/class_2680;"}, // getBlockState
    {"method_26215", "()Z"},                                                    // isAir
    {"method_26204", "()Lnet/minecraft/class_2248;"},                            // getBlock
    {"method_63499", "()Ljava/lang/String;"},                                    // getTranslationKey
    {"method_10103", "(III)Lnet/minecraft/class_2338$class_2339;"},             // Mutable.set

    {"field_1761",  "Lnet/minecraft/class_636;"},    // interactionManager
    {"field_1755",  "Lnet/minecraft/class_437;"},    // currentScreen (null = no GUI)
    {"field_7512",  "Lnet/minecraft/class_1703;"},   // currentScreenHandler
    {"field_7763",  "I"},                             // syncId
    {"field_7514",  "Lnet/minecraft/class_1661;"},   // inventory
    {"method_5438", "(I)Lnet/minecraft/class_1799;"}, // Inventory.getStack
    {"method_5439", "()I"},                           // Inventory.size
    {"method_7909", "()Lnet/minecraft/class_1792;"},  // ItemStack.getItem
    {"method_7876", "()Ljava/lang/String;"},          // Item.getTranslationKey
    {"method_2906", "(IIILnet/minecraft/class_1713;Lnet/minecraft/class_1657;)V"}, // clickSlot
    {"field_7791",  "Lnet/minecraft/class_1713;"},   // SlotActionType.SWAP
};


} // namespace mcov::mappings
