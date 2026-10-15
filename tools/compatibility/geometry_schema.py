"""Semantic names for off-thread geometry, pose and camera interpolation."""


def geometry_schema(version):
    modern = version in ('26.2', '1.21.11')
    classes = {
        'boundsClass': 'net/minecraft/world/phys/AABB',
        'poseClass': 'net/minecraft/world/entity/Pose',
        'walkClass': 'net/minecraft/world/entity/WalkAnimationState',
        'dimensionsClass': 'net/minecraft/world/entity/EntityDimensions',
        'deltaClass': 'net/minecraft/client/DeltaTracker' if version == '26.2' else '',
        'avatarOwnerClass': 'net/minecraft/client/player/AbstractClientPlayer' if modern else '',
        'avatarStateClass': 'net/minecraft/client/entity/ClientAvatarState' if modern else '',
        'handClass': 'net/minecraft/world/InteractionHand',
        'hitClass': 'net/minecraft/world/phys/HitResult',
        'crystalClass': 'net/minecraft/world/entity/boss/enderdragon/EndCrystal',
        'blockHitClass': 'net/minecraft/world/phys/BlockHitResult',
        'pillarClass': 'net/minecraft/world/level/block/RotatedPillarBlock',
        'anchorClass': 'net/minecraft/world/level/block/RespawnAnchorBlock',
        'dimensionTypeClass': 'net/minecraft/world/level/dimension/DimensionType',
        'environmentClass': 'net/minecraft/world/attribute/EnvironmentAttributeSystem' if modern else '',
        'environmentAttrsClass': 'net/minecraft/world/attribute/EnvironmentAttributes' if modern else '',
        'keyMappingClass': 'net/minecraft/client/KeyMapping',
        'blockPosClass': 'net/minecraft/core/BlockPos',
        'directionClass': 'net/minecraft/core/Direction',
        'resultClass': 'net/minecraft/world/InteractionResult',
    }
    entity = 'net/minecraft/world/entity/Entity'
    living = 'net/minecraft/world/entity/LivingEntity'
    player = 'net/minecraft/world/entity/player/Player'
    camera = 'net/minecraft/client/Camera'
    client = 'net/minecraft/client/Minecraft'
    definitions = {}
    def add(key, owner, name, desc, kind='fields', static=False):
        definitions[key] = (owner, name, desc, kind, static)
    add('getBounds', entity, 'getBoundingBox', '()L' + classes['boundsClass'] + ';', 'methods')
    for name in ['minX', 'minY', 'minZ', 'maxX', 'maxY', 'maxZ']:
        add(name, classes['boundsClass'], name, 'D')
    for key, name in [('oldX', 'xOld'), ('oldY', 'yOld'), ('oldZ', 'zOld')]: add(key, entity, name, 'D')
    add('bodyYaw', living, 'yBodyRot', 'F')
    add('bodyYawOld', living, 'yBodyRotO', 'F')
    add('headYaw', living, 'yHeadRot', 'F')
    add('headYawOld', living, 'yHeadRotO', 'F')
    add('headPitch', entity, 'getXRot', '()F', 'methods')
    add('headPitchOld', entity, 'xRotO', 'F')
    add('getPose', entity, 'getPose', '()L' + classes['poseClass'] + ';', 'methods')
    for key, name in [('poseStanding', 'STANDING'), ('poseCrouch', 'CROUCHING'), ('poseSwim', 'SWIMMING'),
                      ('poseGlide', 'FALL_FLYING'), ('poseSleep', 'SLEEPING')]:
        add(key, classes['poseClass'], name, 'L' + classes['poseClass'] + ';', static=True)
    add('isPassenger', entity, 'isPassenger', '()Z', 'methods')
    add('walkState', living, 'walkAnimation', 'L' + classes['walkClass'] + ';')
    add('walkSpeed', classes['walkClass'], 'speed', '(F)F', 'methods')
    add('walkPosition', classes['walkClass'], 'position', '(F)F', 'methods')
    add('getDimensions', entity, 'getDimensions', '(L' + classes['poseClass'] + ';)L' + classes['dimensionsClass'] + ';', 'methods')
    add('dimensionHeight', classes['dimensionsClass'], 'height', 'F')
    partial_source = 'Camera'
    if version == '26.2':
        partial_source = 'CameraDelta'
        add('partialTick', camera, 'getCameraEntityPartialTicks', '(L' + classes['deltaClass'] + ';)F', 'methods')
        add('getDelta', client, 'getDeltaTracker', '()L' + classes['deltaClass'] + ';', 'methods')
    elif version == '1.20.1':
        partial_source = 'Client'
        add('partialTick', client, 'getFrameTime', '()F', 'methods')
    else:
        add('partialTick', camera, 'getPartialTickTime', '()F', 'methods')
    add('cameraEntity', camera, 'entity', 'L' + entity + ';')
    add('freecamSetup', camera, 'alignWithEntity' if version == '26.2' else 'setup', '(F)V' if version == '26.2' else '(L' + ('net/minecraft/world/level/Level' if version == '1.21.11' else 'net/minecraft/world/level/BlockGetter') + ';L' + entity + ';ZZF)V', 'methods')
    add('freecamPosition', camera, 'setPosition', '(DDD)V', 'methods')
    add('freecamRotation', camera, 'setRotation', '(FF)V', 'methods')
    add('freecamDetached', camera, 'detached', 'Z')
    hand_desc = {'26.2':'(Lnet/minecraft/client/renderer/state/level/CameraRenderState;FLorg/joml/Matrix4fc;)V','1.21.11':'(FZLorg/joml/Matrix4f;)V','1.21.1':'(Lnet/minecraft/client/Camera;FLorg/joml/Matrix4f;)V','1.20.1':'(Lcom/mojang/blaze3d/vertex/PoseStack;Lnet/minecraft/client/Camera;F)V'}
    add('freecamHand', 'net/minecraft/client/renderer/GameRenderer', 'renderItemInHand', hand_desc[version], 'methods')
    add('freecamAttack', client, 'startAttack', '()Z', 'methods')
    add('freecamContinueAttack', client, 'continueAttack', '(Z)V', 'methods')
    add('runFrame', client, 'runTick', '(Z)V', 'methods')
    add('clientTick', client, 'tick', '()V', 'methods')
    add('clientPaused', client, 'isPaused', '()Z', 'methods')
    add('useItem',client,'startUseItem','()V','methods')
    add('useKey','net/minecraft/client/Options','keyUse','L'+classes['keyMappingClass']+';')
    add('keyDown',classes['keyMappingClass'],'isDown','()Z','methods')
    for axis in 'XYZ':
        add('pos'+axis,classes['blockPosClass'],'get'+axis,'()I','methods')
        add('step'+axis,classes['directionClass'],'getStep'+axis,'()I','methods')
    add('blockPosCtor',classes['blockPosClass'],'<init>','(III)V','methods')
    add('vecCtor','net/minecraft/world/phys/Vec3','<init>','(DDD)V','methods')
    add('blockHitCtor',classes['blockHitClass'],'<init>','(Lnet/minecraft/world/phys/Vec3;Lnet/minecraft/core/Direction;Lnet/minecraft/core/BlockPos;Z)V','methods')
    add('hitDirection',classes['blockHitClass'],'getDirection','()L'+classes['directionClass']+';','methods')
    add('upDirection',classes['directionClass'],'UP','L'+classes['directionClass']+';',static=True)
    add('useOn','net/minecraft/client/multiplayer/MultiPlayerGameMode','useItemOn','(Lnet/minecraft/client/player/LocalPlayer;Lnet/minecraft/world/InteractionHand;Lnet/minecraft/world/phys/BlockHitResult;)L'+classes['resultClass']+';','methods')
    add('consumesAction',classes['resultClass'],'consumesAction','()Z','methods')
    add('replaceable','net/minecraft/world/level/block/state/BlockState','canBeReplaced','()Z','methods')
    if version == '1.20.1':
        add('oldBlockReach','net/minecraft/client/multiplayer/MultiPlayerGameMode','getPickRange','()F','methods')
    else:
        add('blockReach',player,'blockInteractionRange','()D','methods')
    add('selectedSlot','net/minecraft/world/entity/player/Inventory','selected','I')
    add('inventoryMenu',player,'inventoryMenu','Lnet/minecraft/world/inventory/InventoryMenu;')
    add('carriedStack','net/minecraft/world/inventory/AbstractContainerMenu','getCarried','()Lnet/minecraft/world/item/ItemStack;','methods')
    add('hitResult',client,'hitResult','L'+classes['hitClass']+';')
    add('hitType',classes['hitClass'],'getType','()Lnet/minecraft/world/phys/HitResult$Type;','methods')
    add('blockHitPos',classes['blockHitClass'],'getBlockPos','()Lnet/minecraft/core/BlockPos;','methods')
    add('stateValue','net/minecraft/world/level/block/state/BlockState','getValue','(Lnet/minecraft/world/level/block/state/properties/Property;)Ljava/lang/Comparable;','methods')
    add('axisProperty',classes['pillarClass'],'AXIS','Lnet/minecraft/world/level/block/state/properties/EnumProperty;',static=True)
    add('anchorCharge',classes['anchorClass'],'CHARGE','Lnet/minecraft/world/level/block/state/properties/IntegerProperty;',static=True)
    if modern:
        add('environment','net/minecraft/world/level/Level','environmentAttributes','()L'+classes['environmentClass']+';','methods')
        add('environmentValue',classes['environmentClass'],'getValue','(Lnet/minecraft/world/attribute/EnvironmentAttribute;Lnet/minecraft/core/BlockPos;)Ljava/lang/Object;','methods')
        add('environmentAnchor',classes['environmentAttrsClass'],'RESPAWN_ANCHOR_WORKS','Lnet/minecraft/world/attribute/EnvironmentAttribute;',static=True)
    else:
        add('dimensionType','net/minecraft/world/level/Level','dimensionType','()L'+classes['dimensionTypeClass']+';','methods')
        add('anchorWorks',classes['dimensionTypeClass'],'respawnAnchorWorks','()Z','methods')
    add('setYaw', entity, 'setYRot', '(F)V', 'methods')
    add('setPitch', entity, 'setXRot', '(F)V', 'methods')
    add('eyePosition', entity, 'getEyePosition', '()Lnet/minecraft/world/phys/Vec3;', 'methods')
    add('lineOfSight', living, 'hasLineOfSight', '(L' + entity + ';)Z', 'methods')
    add('attackCooldown', player, 'getAttackStrengthScale', '(F)F', 'methods')
    if version == '1.20.1':
        add('farReach', 'net/minecraft/client/multiplayer/MultiPlayerGameMode', 'hasFarPickRange', '()Z', 'methods')
    else:
        add('attackReach', player, 'entityInteractionRange', '()D', 'methods')
    add('attackEntity', 'net/minecraft/client/multiplayer/MultiPlayerGameMode', 'attack', '(L' + player + ';L' + entity + ';)V', 'methods')
    add('swingHand', living, 'swing', '(L' + classes['handClass'] + ';)V', 'methods')
    add('mainHand', classes['handClass'], 'MAIN_HAND', 'L' + classes['handClass'] + ';', static=True)
    add('pickBlocks', entity, 'pick', '(DFZ)L' + classes['hitClass'] + ';', 'methods')
    add('hitLocation', classes['hitClass'], 'getLocation', '()Lnet/minecraft/world/phys/Vec3;', 'methods')
    add('entityAlive', entity, 'isAlive', '()Z', 'methods')
    add('turn', entity, 'turn', '(DD)V', 'methods')
    add('viewYaw', entity, 'getViewYRot', '(F)F', 'methods')
    add('viewPitch', entity, 'getViewXRot', '(F)F', 'methods')
    # Physik-Scopes: Richtung korrigieren, ohne Modell-/Netzwerk-Rotation zu ersetzen.
    add('movementRelative', entity, 'moveRelative', '(FLnet/minecraft/world/phys/Vec3;)V', 'methods')
    add('movementTravel', living, 'travel', '(Lnet/minecraft/world/phys/Vec3;)V', 'methods')
    add('movementJump', living, 'jumpFromGround', '()V', 'methods')
    add('movementPlayerTravel', player, 'travel', '(Lnet/minecraft/world/phys/Vec3;)V', 'methods')
    if not modern:
        add('movementPlayerJump', player, 'jumpFromGround', '()V', 'methods')
    add('movementLocalStep', 'net/minecraft/client/player/LocalPlayer', 'aiStep', '()V', 'methods')
    add('yawOld', entity, 'yRotO', 'F')
    if version == '26.2':
        add('updateCamera', camera, 'update', '(Lnet/minecraft/client/DeltaTracker;)V', 'methods')
    else:
        level = 'net/minecraft/world/level/Level' if version == '1.21.11' else 'net/minecraft/world/level/BlockGetter'
        add('updateCamera', camera, 'setup', '(L' + level + ';L' + entity + ';ZZF)V', 'methods')
    if modern:
        add('avatarState', classes['avatarOwnerClass'], 'avatarState', '()L' + classes['avatarStateClass'] + ';', 'methods')
    add('walkDistance', classes['avatarStateClass'] if modern else entity, 'walkDist', 'F')
    add('walkDistanceOld', classes['avatarStateClass'] if modern else entity, 'walkDistO', 'F')
    add('bob', classes['avatarStateClass'] if modern else player, 'bob', 'F')
    add('bobOld', classes['avatarStateClass'] if modern else player, 'bobO' if modern else 'oBob', 'F')
    monster = 'net/minecraft/world/entity/monster/'
    humanoids = [monster + ('zombie/Zombie' if modern else 'Zombie'),
                 monster + ('skeleton/AbstractSkeleton' if modern else 'AbstractSkeleton'),
                 monster + 'piglin/AbstractPiglin', monster + 'EnderMan',
                 'net/minecraft/world/entity/decoration/ArmorStand']
    return classes, humanoids, definitions, partial_source
