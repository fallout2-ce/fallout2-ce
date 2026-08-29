#ifndef ART_H
#define ART_H

#include <assert.h>
#include <memory>

#include "animation.h"
#include "art_defs.h"
#include "cache.h"
#include "draw.h"
#include "memory.h"
#include "obj_types.h"
#include "proto_types.h"

namespace fallout {

typedef struct Art {
    int version;
    short framesPerSecond;
    short actionFrame;
    short frameCount;
    short xOffsets[ROTATION_COUNT];
    short yOffsets[ROTATION_COUNT];
    int dataOffsets[ROTATION_COUNT];
    int padding[ROTATION_COUNT];
    int dataSize;
} Art;

typedef struct ArtFrame {
    short width;
    short height;
    int size;
    short x;
    short y;
} ArtFrame;

extern CritterFrameId _art_vault_guy_num;
extern CritterFrameId _art_vault_person_nums[DUDE_NATIVE_LOOK_COUNT][GENDER_COUNT];

extern Cache gArtCache;

int artInit();
void artReset();
void artExit();
char* artGetObjectTypeName(ObjectType objectType);
int artIsObjectTypeHidden(ObjectType objectType);
void artToggleObjectTypeHidden(ObjectType objectType);
int artGetFidgetCount(int headFid);
void artRender(int fid, unsigned char* dest, int width, int height, int pitch);
int art_list_str(int fid, char* name);
Art* artLock(int fid, CacheEntry** cache_entry);
unsigned char* artLockFrameData(int fid, int frame, Rotation rotation, CacheEntry** out_cache_entry);
int artUnlock(CacheEntry* cache_entry);
int artCacheFlush();
int artCopyFileName(ObjectType objectType, int id, char* dest);
int _art_get_code(AnimationType animation, WeaponAnimation weaponType, char* weaponCodePtr, char* animationCodePtr);
char* artBuildFilePath(int fid);
int artGetFramesPerSecond(Art* art);
int artGetActionFrame(Art* art);
int artGetFrameCount(Art* art);
int artGetWidth(Art* art, int frame = 0, Rotation rotation = ROTATION_NE);
int artGetHeight(Art* art, int frame = 0, Rotation rotation = ROTATION_NE);
int artGetSize(Art* art, int frame, Rotation rotation, int* out_width, int* out_height);
int artGetFrameOffsets(const Art* art, int frame, Rotation rotation, int* xPtr, int* yPtr);
int artGetRotationOffsets(Art* art, Rotation rotation, int* out_offset_x, int* out_offset_y);
unsigned char* artGetFrameData(Art* art, int frame = 0, Rotation rotation = ROTATION_NE);
unsigned char* artGetFrameData(const Art* art, int frame, Rotation rotation, int* widthPtr, int* heightPtr, int* xOffsetPtr, int* yOffsetPtr);
ArtFrame* artGetFrame(const Art* art, int frame, Rotation rotation);
ConstBuffer2D artGetFrameBuffer(const Art* art, int frame, Rotation rotation);
bool artExists(int fid);
bool _art_fid_valid(int fid);
CritterFrameId _art_alias_num(CritterFrameId index);
int artCritterFidShouldRun(int fid);
int artAliasFid(int fid);
int buildFid(ObjectType objectType, int frmId, int animType = 0, int weaponCode = 0, Rotation rotation = ROTATION_NE);
int artListIndex(ObjectType objectType, const char* name);
Art* artLoad(const char* path);
int artRead(const char* path, unsigned char* data);
int artWrite(const char* path, unsigned char* data);

using ArtPtr = InternalPtr<Art>;

class NamedCacheEntry;
std::shared_ptr<NamedCacheEntry> artLockNamedFrameData(const char* path);

class FrmId {
public:
    FrmId() = default;

    explicit FrmId(MiscFrameId misc, AnimationType animType = ANIM_STAND)
        : _objectType(OBJ_TYPE_MISC)
        , _fid(buildFid(OBJ_TYPE_MISC, misc, animType))
    {
    }

    explicit FrmId(SceneryFrameId scenery)
        : _objectType(OBJ_TYPE_SCENERY)
        , _fid(buildFid(OBJ_TYPE_SCENERY, scenery))
    {
    }

    explicit FrmId(WallFrameId wall)
        : _objectType(OBJ_TYPE_WALL)
        , _fid(buildFid(OBJ_TYPE_WALL, wall))
    {
    }

    explicit FrmId(ItemFrameId item)
        : _objectType(OBJ_TYPE_ITEM)
        , _fid(buildFid(OBJ_TYPE_ITEM, item))
    {
    }

    explicit FrmId(TileFrameId tile)
        : _objectType(OBJ_TYPE_TILE)
        , _fid(buildFid(OBJ_TYPE_TILE, tile))
    {
    }

    explicit FrmId(SkillDexFrameId skilldex)
        : _objectType(OBJ_TYPE_SKILLDEX)
        , _fid(buildFid(OBJ_TYPE_SKILLDEX, skilldex))
    {
    }

    explicit FrmId(InterfaceFrameId interface)
        : _objectType(OBJ_TYPE_INTERFACE)
        , _fid(buildFid(OBJ_TYPE_INTERFACE, interface))
    {
    }

    explicit FrmId(CritterFrameId critter, AnimationType animType, WeaponAnimation weaponAnimation, Rotation rotation)
        : _objectType(OBJ_TYPE_CRITTER)
        , _fid(buildFid(OBJ_TYPE_CRITTER, critter, animType, weaponAnimation, rotation))
    {
    }

    explicit FrmId(Object* object, AnimationType animType, WeaponAnimation weaponAnimation, Rotation rotation)
        : _objectType(objectTypeFromFid(object->fid))
        , _fid(object == nullptr ? -1 : buildFid(objectTypeFromFid(object->fid), objectFrameIdFromFid(object->fid), animType, weaponAnimation, rotation))
    {
    }

    explicit FrmId(Head head, HeadAnimation headAnimation = HEAD_ANIMATION_VERY_GOOD_REACTION, int fidget = 0)
        : _objectType(OBJ_TYPE_HEAD)
        , _fid(buildFid(OBJ_TYPE_HEAD, head, headAnimation, fidget))
    {
    }

    explicit FrmId(Background background)
        : _objectType(OBJ_TYPE_BACKGROUND)
        , _fid(buildFid(OBJ_TYPE_BACKGROUND, background))
    {
    }

    explicit FrmId(ObjectType objType, const char* path);
    explicit FrmId(const char* path);

    int fid() const { return _fid; }
    bool hasObjectType() const { return objectTypeIsValid(_objectType); }
    ObjectType objectType() const;
    const char* filePath() const { return _path; }

    bool empty() const { return _fid == -1 && _path == nullptr; }

    operator int() const {
        return _fid; 
    }

private:
    int _fid = -1;
    ObjectType _objectType = OBJ_TYPE_INVALID;
    const char* _path = nullptr;
};

// RAII helper for locking one selected frame from FID-backed or path-backed art.
// lock/unlock use caches instead of just loading/unloading directly.
class FrmImage {
public:
    FrmImage();
    ~FrmImage();

    FrmImage(const FrmImage&) = delete;
    FrmImage& operator=(const FrmImage&) = delete;

    FrmImage(FrmImage&& other) noexcept;

    FrmImage& operator=(FrmImage&& other) noexcept;

    bool isLocked() const { return _key != nullptr || _namedKey; }
    bool lock(const FrmId& frmId);
    bool lock(const FrmId& frmId, int frame, Rotation rotation);
    bool lock(unsigned int fid);
    bool lock(unsigned int fid, int frame, Rotation rotation);
    bool lock(const char* frmPath);
    bool lock(const char* frmPath, int frame, Rotation rotation);
    bool lock(ObjectType objType, const char* frmRelativePath);
    bool lock(ObjectType objType, const char* frmRelativePath, int frame, Rotation rotation);
    void unlock();

    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getXOffset() const { return _xOffset; }
    int getYOffset() const { return _yOffset; }
    // Returns FRM frame data if locked, nullptr otherwise.
    unsigned char* getData() const { return _data; }

    ConstBuffer2D getBuffer() const { return { _data, _width, _height }; };

private:
    void resetInternal();
    bool setFrame(const Art* art, int frame, Rotation rotation);

    std::shared_ptr<NamedCacheEntry> _namedKey;
    CacheEntry* _key = nullptr;
    unsigned char* _data = nullptr;
    int _width = 0;
    int _height = 0;
    int _xOffset = 0;
    int _yOffset = 0;
};

} // namespace fallout

#endif
