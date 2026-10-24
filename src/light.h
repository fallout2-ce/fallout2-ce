#ifndef LIGHT_H
#define LIGHT_H

#include "map_defs.h"

namespace fallout {

#define LIGHT_INTENSITY_MIN (65536 / 4)
#define LIGHT_INTENSITY_MAX 65536

typedef void AdjustLightIntensityProc(MapElevation elevation, int tile, int intensity);

int lightInit();
void lightReset();
void lightExit();
int lightGetAmbientIntensity();
void lightSetAmbientIntensity(int intensity, bool shouldUpdateScreen);
int lightGetTileIntensity(MapElevation elevation, int tile);
int lightGetTrueTileIntensity(MapElevation elevation, int tile);
void lightSetTileIntensity(MapElevation elevation, int tile, int intensity);
void lightIncreaseTileIntensity(MapElevation elevation, int tile, int intensity);
void lightDecreaseTileIntensity(MapElevation elevation, int tile, int intensity);
void lightResetTileIntensity();
void lightDecreaseAmbient(int val);
void lightIncreaseAmbient(int val);

} // namespace fallout

#endif /* LIGHT_H */
