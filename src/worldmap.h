#ifndef WORLD_MAP_H
#define WORLD_MAP_H

#include "color.h"
#include "db.h"
#include "obj_types.h"
#include "worldmap_defs.h"

namespace fallout {

#define CAR_FUEL_MAX (80000)

extern Color* circleBlendTable;
extern bool gDidMeetFrankHorrigan;

int wmWorldMap_init();
void wmWorldMap_exit();
int wmWorldMap_reset();
int wmWorldMap_save(File* stream);
int wmWorldMap_load(File* stream);
int wmMapMaxCount();

inline bool mapIsValid(int map)
{
    return map >= MAP_FIRST && map < wmMapMaxCount();
}

bool cityIsValid(int city);
int wmMapIdxToName(Map mapIdx, char* dest, size_t size);
Map wmMapMatchNameToIdx(char* name);
bool wmMapIdxIsSaveable(Map mapIdx);
bool wmMapIsSaveable();
bool wmMapDeadBodiesAge();
bool wmMapCanRestHere(int elevation);
void wmSetRestMode(RestModeFlag mode);
void wmSetEncounterDetection(bool enabled);
bool wmRestModeIsDisabled();
bool wmRestModeIsStrict();
bool wmRestModeNoHealing();
bool wmMapPipboyActive();
int wmMapMarkVisited(Map mapIdx);
int wmMapMarkMapEntranceState(Map mapIdx, int elevation, int state);
void wmWorldMap();
int wmCheckGameAreaEvents();
int wmSetupRandomEncounter();
bool wmEvalTileNumForPlacement(int tile);
int wmSubTileMarkRadiusVisited(int x, int y, int radius);
int wmSubTileGetVisitedState(int x, int y, int* statePtr);
int wmGetAreaIdxName(City areaIdx, char* name);
bool wmAreaIsKnown(City areaIdx);
VisitedState wmAreaVisitedState(City areaIdx);
bool wmMapIsKnown(Map mapIdx);
int wmAreaMarkVisited(City areaIdx);
bool wmAreaMarkVisitedState(City areaIdx, VisitedState state);
bool wmAreaSetVisibleState(City areaIdx, CityState state, bool force);
int wmAreaSetWorldPos(City areaIdx, int x, int y);
int wmGetPartyWorldPos(int* xPtr, int* yPtr);
int wmGetPartyCurArea(City* areaIdxPtr);
void wmTownMap();
int wmCarUseGas(int amount);
int wmCarFillGas(int amount);
int wmCarGasAmount();
void wmSetCarInterfaceArt(ObjectFrameId artIndex);
bool wmCarIsOutOfGas();
int wmCarCurrentArea();
int wmCarGiveToParty();
int wmSfxMaxCount();
int wmSfxRollNextIdx();
int wmSfxIdxName(int sfxIdx, char** namePtr);
int wmMapMusicStart();
int wmSetMapMusic(Map mapIdx, const char* name);
int wmMatchAreaContainingMapIdx(Map mapIdx, City* areaIdxPtr);
int wmTeleportToArea(City areaIdx);

// CE
bool wmStartWorldPosIsConfigured();
void wmSetPartyCurArea(City areaIdx);
void wmClearPartyWalking();
void wmSetPartyWorldPos(int x, int y);
void wmCarSetCurrentArea(City area);
void wmForceEncounter(Map map, EncounterFlag flags);
void wmSetScriptWorldMapMulti(float value);
bool wmTerrainNameIsValidSubtile(int x, int y);
void wmSetTerrainName(int x, int y, const char* name);
const char* wmGetTerrainName(int x, int y);
const char* wmGetCurrentTerrainName();
void wmSetTownTitle(City areaIdx, const char* title);
void wmRemoveTownNames(bool state);
int worldmapGetWindow();

} // namespace fallout

#endif /* WORLD_MAP_H */
