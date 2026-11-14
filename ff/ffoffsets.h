/* Engine field offsets used by ffenemy.h. 0 = not measured yet (the accessor logs "not done"). Each line says how it was measured. */
#define OFF_NPC_STATE 0xb70   /* int. 260 accesses in the NPC AI range 0x690000-0x740000; 0x6847d4 sets it to 0x11; 0x68c1c4-0x68c3a0: previous/next state shuffles with +0xb74 */
#define OFF_NPC_STATEFRAME 0xb7c   /* int. NPC::Update 0x68e20c-0x68e214: ldr/add 1/str every update */
#define OFF_NPC_I1 0xbd0   /* int. pair stored with one 64-bit store (d:str x4) with 0xbd4 = I1/I2; heavy int use in the AI range - ASSUMED by layout order (Lua docs order State, StateFrame, ProjectileCooldown, ProjectileDelay, I1, I2, V1, V2) */
#define OFF_NPC_I2 0xbd4   /* int. see I1 */
#define OFF_NPC_V1 0xbe0   /* float x, y at +0xbe4. s:ldr/str pairs; ASSUMED from layout */
#define OFF_NPC_V2 0xbe8   /* float x, y at +0xbec. ASSUMED from layout */
#define OFF_NPC_PROJCD 0xba0   /* int. NPC::Update 0x68c598-0x68c5b8: decrements to 0 each update (increments if type 0x43) */
#define OFF_NPC_PROJDELAY 0
#define OFF_NPC_SCALE 0
#define OFF_ENT_COLLCLASS 0x1e4   /* int. Entity ctor 0x82bd6c-0x82bd70 stores 4 (ENTCOLL_ALL, the Lua default of a bare entity); 0x82dcb8-0x82dcc0 (Entity::IsActiveEnemy 0x82dca8) requires it >= 2 (ENTCOLL_PLAYEROBJECTS) */
#define OFF_ENT_GRIDCOLL 0x1e0   /* int. Entity init 0x82c014 stores 3, 0x82c198-0x82c1a4 store 0 / 5 / 6 (NONE, GRIDCOLL_GROUND 5, BULLET 6); Entity::Update 0x82d5f4 / 0x82d2cc compare it with 5 and 3 */
#define OFF_ENT_FRICTION 0
#define OFF_ENT_MASS 0x350       /* float. Measured from the ff-codigo42 dumps: Slammer 3 (XML collisionMass 3), two Fatty-like 100; no AI uses it yet */
#define OFF_ENT_TARGET 0
#define OFF_ENT_CHILD 0
#define OFF_ENT_SPRITEOFFSET 0
#define OFF_ENT_DEPTHOFFSET 0
#define OFF_PROJ_HEIGHT 0x400   /* float. Projectile init 0x678ed0..0x678eec stores the double at 0x9526d0 = (-23.0f, 0.0f) at +0x400/+0x404: Height default -23 (the Lua default) then FallingSpeed 0 */
#define OFF_PROJ_FALLINGSPEED 0x404   /* float. second half of that init; Projectile::Update 0x678f8c/0x6790c0 adds 0.5*[+0x404] to the velocity-height terms */
#define OFF_PROJ_FALLINGACCEL 0x408   /* float. 0x6792c8 (projectile split copies it as str w25,[x24,#0x408]); 0x6789e0 tests it > 0 and adds 0.5*FallingSpeed */
#define OFF_PROJ_FLAGS 0x428   /* u64. Projectile::Update 0x678dbc / 0x6780bc: ldr [+0x428], tst with flag bit masks (0xc00000000000 = bits 46/47) */
#define OFF_PROJ_SCALE 0x410   /* float. Projectile::Update 0x678088: frame = clamp(3*[+0x410]+3, .., 13) picks the sprite size */
#define OFF_PROJ_CURVING 0
#define OFF_PROJ_HOMING 0
#define OFF_EFF_TIMEOUT 0
#define OFF_EFF_LIFESPAN 0
#define OFF_EFF_SCALE 0
#define OFF_NPC_CANSHUTDOORS 0x384   /* byte, INVERTED (1 = does not shut doors / is not an active enemy): Entity::IsActiveEnemy 0x82dca8 returns false when [+0x384] != 0 (after the type range 10..0x3e7 and EntityCollisionClass >= 2 tests) */
