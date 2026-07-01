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
bool heroAppearanceSelectWindow(int raceStyleFlag);
int heroAppearanceApplyCritterArtOffset(int fid);
int heroAppearanceRemoveCritterArtOffset(int fid);

} // namespace fallout

#endif // HERO_APPEARANCE_H
