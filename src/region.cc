#include "region.h"

#include <limits.h>
#include <string.h>

#include "debug.h"
#include "memory_manager.h"

namespace fallout {

// 0x50D394
static char _aNull[] = "<null>";

// Probably recalculates bounding box of the region.
//
// 0x4A2B50 regionSetBound_
void _regionSetBound(Region* region)
{
    int minX = INT_MAX;
    int maxX = INT_MIN;
    int minY = INT_MAX;
    int maxY = INT_MIN;
    int pointCount = 0;
    int sumX = 0;
    int sumY = 0;

    for (int index = 0; index < region->pointsLength; index++) {
        Point* point = &(region->points[index]);
        if (minX >= point->x) minX = point->x;
        if (minY >= point->y) minY = point->y;
        if (maxX <= point->x) maxX = point->x;
        if (maxY <= point->y) maxY = point->y;
        sumX += point->x;
        sumY += point->y;
        pointCount++;
    }

    region->minY = minY;
    region->maxX = maxX;
    region->maxY = maxY;
    region->minX = minX;

    if (pointCount != 0) {
        region->centroidX = sumX / pointCount;
        region->centroidY = sumY / pointCount;
    }
}

// 0x4A2C14 pointInRegion_
bool regionContainsPoint(Region* region, int x, int y)
{
    if (region == nullptr) {
        return false;
    }

    if (x < region->minX || x > region->maxX || y < region->minY || y > region->maxY) {
        return false;
    }

    int previousQuadrant;

    Point* prev = &(region->points[0]);
    if (x >= prev->x) {
        if (y >= prev->y) {
            previousQuadrant = 2;
        } else {
            previousQuadrant = 1;
        }
    } else {
        if (y >= prev->y) {
            previousQuadrant = 3;
        } else {
            previousQuadrant = 0;
        }
    }

    int winding = 0;
    for (int index = 0; index < region->pointsLength; index++) {
        int quadrant;

        Point* point = &(region->points[index + 1]);
        if (x >= point->x) {
            if (y >= point->y) {
                quadrant = 2;
            } else {
                quadrant = 1;
            }
        } else {
            if (y >= point->y) {
                quadrant = 3;
            } else {
                quadrant = 0;
            }
        }

        int quadrantDelta = quadrant - previousQuadrant;
        switch (quadrantDelta) {
        case -3:
            quadrantDelta = 1;
            break;
        case -2:
        case 2:
            if ((double)x < ((double)point->x - (double)(prev->x - point->x) / (double)(prev->y - point->y) * (double)(point->y - y))) {
                quadrantDelta = -quadrantDelta;
            }
            break;
        case 3:
            quadrantDelta = -1;
            break;
        }

        prev = point;
        previousQuadrant = quadrant;

        winding += quadrantDelta;
    }

    if (winding == 4 || winding == -4) {
        return true;
    }

    return false;
}

// 0x4A2D78 allocateRegion_
Region* regionCreate(int initialCapacity)
{
    Region* region = (Region*)internal_malloc_safe(sizeof(*region), __FILE__, __LINE__); // "..\int\REGION.C", 142
    memset(region, 0, sizeof(*region));

    if (initialCapacity != 0) {
        region->points = (Point*)internal_malloc_safe(sizeof(*region->points) * (initialCapacity + 1), __FILE__, __LINE__); // "..\int\REGION.C", 147
        region->pointsCapacity = initialCapacity + 1;
    } else {
        region->points = nullptr;
        region->pointsCapacity = 0;
    }

    region->name[0] = '\0';
    region->flags = 0;
    region->minY = INT_MIN;
    region->maxY = INT_MAX;
    region->procs[3] = 0;
    region->rightProcs[1] = 0;
    region->rightProcs[3] = 0;
    region->field_68 = 0;
    region->rightProcs[0] = 0;
    region->field_70 = 0;
    region->rightProcs[2] = 0;
    region->mouseEventCallback = nullptr;
    region->rightMouseEventCallback = nullptr;
    region->mouseEventCallbackUserData = nullptr;
    region->rightMouseEventCallbackUserData = nullptr;
    region->pointsLength = 0;
    region->minX = INT_MIN;
    region->maxX = INT_MAX;
    region->procs[2] = 0;
    region->procs[1] = 0;
    region->procs[0] = 0;
    region->rightProcs[0] = 0;

    return region;
}

// regionAddPoint
// 0x4A2E68 regionAddPoint_
void regionAddPoint(Region* region, int x, int y)
{
    if (region == nullptr) {
        debugPrint("regionAddPoint(): null region ptr\n");
        return;
    }

    if (region->points != nullptr) {
        if (region->pointsCapacity - 1 == region->pointsLength) {
            region->points = (Point*)internal_realloc_safe(region->points, sizeof(*region->points) * (region->pointsCapacity + 1), __FILE__, __LINE__); // "..\int\REGION.C", 190
            region->pointsCapacity++;
        }
    } else {
        region->pointsCapacity = 2;
        region->pointsLength = 0;
        region->points = (Point*)internal_malloc_safe(sizeof(*region->points) * 2, __FILE__, __LINE__); // "..\int\REGION.C", 185
    }

    int pointIndex = region->pointsLength;
    region->pointsLength++;

    Point* point = &(region->points[pointIndex]);
    point->x = x;
    point->y = y;

    Point* end = &(region->points[pointIndex + 1]);
    end->x = region->points->x;
    end->y = region->points->y;
}

// regionDelete
// 0x4A2F0C regionDelete_
void regionDelete(Region* region)
{
    if (region == nullptr) {
        debugPrint("regionDelete(): null region ptr\n");
        return;
    }

    if (region->points != nullptr) {
        internal_free_safe(region->points, __FILE__, __LINE__); // "..\int\REGION.C", 206
    }

    internal_free_safe(region, __FILE__, __LINE__); // "..\int\REGION.C", 207
}

// regionAddName
// 0x4A2F54 regionAddName_
void regionSetName(Region* region, const char* name)
{
    if (region == nullptr) {
        debugPrint("regionAddName(): null region ptr\n");
        return;
    }

    if (name == nullptr) {
        region->name[0] = '\0';
        return;
    }

    strncpy(region->name, name, REGION_NAME_LENGTH - 1);
}

// regionGetName
// 0x4A2F80 regionGetName_
char* regionGetName(Region* region)
{
    if (region == nullptr) {
        debugPrint("regionGetName(): null region ptr\n");
        return _aNull;
    }

    return region->name;
}

// regionGetUserData
// 0x4A2F98 regionGetUserData_
void* regionGetUserData(Region* region)
{
    if (region == nullptr) {
        debugPrint("regionGetUserData(): null region ptr\n");
        return nullptr;
    }

    return region->userData;
}

// regionSetUserData
// 0x4A2FB4 regionSetUserData_
void regionSetUserData(Region* region, void* data)
{
    if (region == nullptr) {
        debugPrint("regionSetUserData(): null region ptr\n");
        return;
    }

    region->userData = data;
}

// 0x4A2FD0 regionSetFlag_
void regionAddFlag(Region* region, int value)
{
    region->flags |= value;
}

} // namespace fallout
