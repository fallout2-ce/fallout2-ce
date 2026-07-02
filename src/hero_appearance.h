#ifndef HERO_APPEARANCE_H
#define HERO_APPEARANCE_H

namespace fallout {

void heroAppearanceInit();
void heroAppearanceRefresh();
void heroAppearanceExit();
void heroAppearanceRefreshDudeArt();
bool heroAppearanceIsEnabled();
bool heroAppearanceIsActive();
int heroAppearanceGetRace();
int heroAppearanceGetStyle();
bool heroAppearanceSetRace(int race);
bool heroAppearanceSetStyle(int style);
bool heroAppearanceSetDudeModel(int gender, const char* model);
bool heroAppearanceSelectWindowForCharacterCreation(int raceStyleFlag, bool* acceptedPtr);
// CE-native Hero Appearance remap for player/dude art. Callers should gate this
// to gDude so selected appearances do not affect arbitrary NPCs.
int heroAppearanceResolvePlayerFid(int fid);
int heroAppearanceApplyCritterArtOffset(int fid);
int heroAppearanceRemoveCritterArtOffset(int fid);

} // namespace fallout

#endif // HERO_APPEARANCE_H
