# Spinbot rotation

Spinbot publishes independent yaw/pitch values for the local player's movement
packets. The physical entity retains the camera's yaw and pitch, including direct
field reads and movement code outside the travel/jump scopes. Mouse input updates
the physical entity and detached camera together while packet rotation is active.
The spin continues while moving.

The Java transformer identifies local-player packet builders by their position
and rotation packet constructor signatures and mapped yaw/pitch getter calls.
It replaces the getter results at those call sites, including the angle-change
comparison and last-sent angle cache. Both riding rotation packets and the older
one-boolean and newer two-boolean packet layouts are covered. It leaves position,
velocity, input, sprint state and packet timing unchanged.

Missing packet, movement or camera hooks disable Spinbot with a diagnostic.
Menu/focus pauses, expired client requests, world changes, disabling, Freecam,
Kill Aura priority and shutdown release the packet override. Restart Minecraft
and Keim after updating an already attached agent.

Regression checks cover actual instrumented fixture bytecode for all 11 runtime
profiles, including attachment to already loaded classes, packet contents,
unchanged physical rotation, mouse input, other entities and override cleanup.
These checks do not establish compatibility with a particular server's movement
validation. A server may still correct movement when the transmitted spin angle
differs from the movement direction; packet-only rotation cannot guarantee that
every server will accept it.
