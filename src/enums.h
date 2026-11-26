#ifndef ENUMS_H
#define ENUMS_H

// Enum forward declarations must match the scopedness and underlying types
// of their definitions in the corresponding headers.

namespace fallout {

// animation_defs.h
enum AnimationRequestOptions : int;
enum AnimationType : int;

// art_defs.h
enum class InterfaceFrameId : int;
enum class WeaponAnimation : int;

// combat_ai_defs.h
enum AreaAttackMode : int;
enum AttackWho : int;
enum BestWeapon : int;
enum ChemUse : int;
enum DistanceMode : int;
enum Disposition : int;
enum RunAwayMode : int;

// combat_defs.h
enum HitMode : int;

// critter.h
enum RadiationLevel : int;

// game_movie.h
enum GameMovie : int;

// game_vars.h
enum GameGlobalVar : int;

// inventory.h
enum Hand : int;

// interpreter_extra.h
enum class InvenSlot : int;

// sfall_script_hooks.h
enum class RemoveInventoryObjectHookReason;

// map_defs.h
enum MapElevation : int;

// mouse.h
enum class MouseButtonEvent : unsigned int;

// obj_types.h
enum Dam : int;
enum ObjectFlags : unsigned int;
enum ObjectType : int;
enum Rotation : int;

// perk_defs.h
enum Perk : int;

// proto_types.h
enum DamageType : int;
enum Gender : int;
enum ItemType : int;
enum KillType : int;

// queue.h
enum EventType : int;

// random.h
enum class Roll : int;

// scripts.h
enum ScriptProc : int;
enum ScriptType : int;

// skill_defs.h
enum Skill : int;

// skilldex.h
enum SkilldexRC : int;

// stat_defs.h
enum Stat : int;

// trait_defs.h
enum Trait : int;

// window_defs.h
enum TextAlignment : int;

// window_manager.h
enum WindowFlags : unsigned int;

// worldmap.h
enum Map : int;

} // namespace fallout

#endif /* ENUMS_H */
