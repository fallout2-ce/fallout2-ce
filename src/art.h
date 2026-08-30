#ifndef ART_H
#define ART_H

#include <cstring>
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

class NamedCacheEntry;
std::shared_ptr<NamedCacheEntry> artLockNamedFrameData(const char* path);

class FrmId {
public:
    static constexpr int EmptyFid = -1;

    constexpr FrmId()
        : _objectType(OBJ_TYPE_INVALID)
        , _fid(EmptyFid)
        , _path(nullptr)
    {
    }

    static const FrmId& Empty()
    {
        static const FrmId emptyInstance {};
        return emptyInstance;
    }

    explicit FrmId(int fid)
        : _objectType(objectTypeFromFid(fid))
        , _fid(fid)
    {
    }

    explicit FrmId(MiscFrameId misc, AnimationType animType = ANIM_STAND);
    explicit FrmId(SceneryFrameId scenery);
    explicit FrmId(WallFrameId wall);
    explicit FrmId(ItemFrameId item);
    explicit FrmId(TileFrameId tile);
    explicit FrmId(SkillDexFrameId skilldex);
    explicit FrmId(InterfaceFrameId interface);
    explicit FrmId(CritterFrameId critter, AnimationType animType, WeaponAnimation weaponAnimation, Rotation rotation);
    explicit FrmId(Object* object, AnimationType animType, WeaponAnimation weaponAnimation, Rotation rotation);
    explicit FrmId(HeadFrameId head, HeadAnimation headAnimation = HEAD_ANIMATION_VERY_GOOD_REACTION, int fidget = 0);
    explicit FrmId(BackgroundFrameId background);
    explicit FrmId(ObjectType objType, const char* path);
    explicit FrmId(ObjectType objectType, int frmId, int animType = 0, int weaponCode = 0, Rotation rotation = ROTATION_NE);

    int fid() const { return _fid; }
    bool hasObjectType() const { return objectTypeIsValid(_objectType); }
    ObjectType objectType() const;
    const char* filePath() const { return _path; }

    bool empty() const { return (*this) == Empty(); }

    bool operator==(const FrmId& other) const
    {
        if (_fid != other._fid) return false;
        if (_objectType != other._objectType) return false;
        if (_path == nullptr && other._path == nullptr) return true;
        if (_path == nullptr || other._path == nullptr) return false;

        return std::strcmp(_path, other._path) == 0;
    }

    bool operator!=(const FrmId& other) const
    {
        return !(*this == other);
    }

private:
    int _fid = EmptyFid;
    ObjectType _objectType = OBJ_TYPE_INVALID;
    const char* _path = nullptr;
};

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

inline Art* artLock(const FrmId& frmId, CacheEntry** cache_entry)
{
    return artLock(frmId.fid(), cache_entry);
}

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

inline bool artExists(const FrmId& frmId)
{
    return artExists(frmId.fid());
}

bool _art_fid_valid(int fid);
CritterFrameId _art_alias_num(CritterFrameId index);
int artCritterFidShouldRun(int fid);
int artAliasFid(int fid);
int artListIndex(ObjectType objectType, const char* name);
Art* artLoad(const char* path);
int artRead(const char* path, unsigned char* data);
int artWrite(const char* path, unsigned char* data);

using ArtPtr = InternalPtr<Art>;

class NamedCacheEntry;
std::shared_ptr<NamedCacheEntry> artLockNamedFrameData(const char* path);

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
    bool lock(const FrmId& frmId, int frame = 0, Rotation rotation = ROTATION_NE);
    bool lock(const char* frmPath, int frame = 0, Rotation rotation = ROTATION_NE);
    bool lock(ObjectType objType, const char* frmRelativePath, int frame = 0, Rotation rotation = ROTATION_NE);
    void unlock();

    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getXOffset() const { return _xOffset; }
    int getYOffset() const { return _yOffset; }
    // Returns FRM frame data if locked, nullptr otherwise.
    unsigned char* getData() const { return _data; }

    ConstBuffer2D getBuffer() const { return { _data, _width, _height }; };

private:
    bool lock(unsigned int fid, int frame = 0, Rotation rotation = ROTATION_NE);
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
