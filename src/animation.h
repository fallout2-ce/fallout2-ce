#ifndef ANIMATION_H
#define ANIMATION_H

#include "animation_defs.h"
#include "art_defs.h"
#include "combat_defs.h"
#include "obj_types.h"

namespace fallout {

// Signature of animation callback accepting 2 parameters.
typedef int(AnimationCallback)(void* a1, void* a2);

// Signature of animation callback accepting 3 parameters.
typedef int(AnimationCallback3)(void* a1, void* a2, void* a3);

typedef struct StraightPathNode {
    int tile;
    int elevation;
    int x;
    int y;
} StraightPathNode;

typedef Object* PathBuilderCallback(Object* object, int tile, int elevation);

void animationInit();
void animationReset();
void animationExit();
bool animationCheckCombatMode();
void animationSetCombatCheck(bool enable);
void animationResetCombatCheck();
int reg_anim_begin(AnimationRequestOptions requestOptions);
int _register_priority(int a1);
int reg_anim_clear(Object* a1);
int reg_anim_end();
int animationIsBusy(Object* a1);
int animationRegisterMoveToObject(Object* owner, Object* destination, int actionPoints, int delay);
int animationRegisterRunToObject(Object* owner, Object* destination, int actionPoints, int delay);
int animationRegisterMoveToTile(Object* owner, int tile, int elevation, int actionPoints, int delay);
int animationRegisterRunToTile(Object* owner, int tile, int elevation, int actionPoints, int delay);
int animationRegisterMoveToTileStraight(Object* object, int tile, int elevation, AnimationType anim, int delay);
int animationRegisterMoveToTileStraightAndWaitForComplete(Object* owner, int tile, int elev, AnimationType anim, int delay);
int animationRegisterAnimate(Object* owner, AnimationType anim, int delay);
int animationRegisterAnimateReversed(Object* owner, AnimationType anim, int delay);
int animationRegisterAnimateAndHide(Object* owner, AnimationType anim, int delay);
int animationRegisterRotateToTile(Object* owner, int tile);
int animationRegisterRotateClockwise(Object* owner);
int animationRegisterRotateCounterClockwise(Object* owner);
int animationRegisterHideObject(Object* object);
int animationRegisterHideObjectForced(Object* object);
int animationRegisterCallback(void* a1, void* a2, AnimationCallback* proc, int delay);
int animationRegisterCallback3(void* a1, void* a2, void* a3, AnimationCallback3* proc, int delay);
int animationRegisterCallbackForced(void* a1, void* a2, AnimationCallback* proc, int delay);
int animationRegisterSetFlag(Object* object, ObjectFlags flag, int delay);
int animationRegisterUnsetFlag(Object* object, ObjectFlags flag, int delay);
int animationRegisterSetFid(Object* owner, int fid, int delay);
int animationRegisterTakeOutWeapon(Object* owner, WeaponAnimation weaponAnimationCode, int delay);
int animationRegisterSetLightDistance(Object* owner, int lightDistance, int delay);
int animationRegisterToggleOutline(Object* object, bool outline, int delay);
int animationRegisterPlaySoundEffect(Object* owner, const char* soundEffectName, int delay);
int animationRegisterAnimateForever(Object* owner, AnimationType anim, int delay);
int animationRegisterPing(AnimationRequestOptions requestOptions, int delay);
int _make_path(Object* object, int from, int to, unsigned char* rotations, int requireEmptyDest);
int pathfinderFindPath(Object* object, int from, int to, unsigned char* rotations, int requireEmptyDest, PathBuilderCallback* callback);
int _make_straight_path(Object* object, int from, int to, StraightPathNode* straightPathNodeList, Object** obstaclePtr, int a6);
int _make_straight_path_func(Object* object, int from, int to, StraightPathNode* straightPathNodeList, Object** obstaclePtr, int a6, PathBuilderCallback* callback);
void _object_animate();
int _check_move(int* actionPointsPtr);
int _dude_move(int actionPoints);
int _dude_run(int actionPoints);
void _dude_fidget();
void _dude_stand(Object* obj, Rotation rotation, int fid);
void _dude_standup(Object* a1);
void animationStop();

int animationRegisterSetLightIntensity(Object* owner, int lightDistance, int lightIntensity, int delay);

} // namespace fallout

#endif /* ANIMATION_H */
