#ifndef ART_H
#define ART_H

#include <cassert>
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
    union FrameId {
        int id;
        MiscFrameId misc;
        SceneryFrameId scenery;
        WallFrameId wall;
        ItemFrameId item;
        TileFrameId tile;
        SkillDexFrameId skilldex;
        InterfaceFrameId interface;
        CritterFrameId critter;
        HeadFrameId head;
        BackgroundFrameId background;
    };

    static constexpr int kEmptyFid = -1;
    static constexpr short kInvalidFrameId = -1;
    static constexpr short kMinFrameId = 0;
    static constexpr short kMaxFrameId = 4095;

    constexpr FrmId()
        : _objectType(OBJ_TYPE_INVALID)
        , _fid(kEmptyFid)
        , _frameId { kInvalidFrameId }
        , _path(nullptr)
    {
    }

    static const FrmId& Empty()
    {
        static const FrmId emptyInstance {};
        return emptyInstance;
    }

    constexpr explicit FrmId(int fid)
        : _objectType(objectTypeFromFid(fid))
        , _fid(fid)
        , _frameId { frameIdFromFid(fid) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(MiscFrameId misc, AnimationType animType = ANIM_STAND)
        : _objectType(OBJ_TYPE_MISC)
        , _fid(buildFid(OBJ_TYPE_MISC, static_cast<int>(misc), animType))
        , _frameId { static_cast<int>(misc) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(SceneryFrameId scenery)
        : _objectType(OBJ_TYPE_SCENERY)
        , _fid(buildFid(OBJ_TYPE_SCENERY, static_cast<int>(scenery)))
        , _frameId { static_cast<int>(scenery) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(WallFrameId wall)
        : _objectType(OBJ_TYPE_WALL)
        , _fid(buildFid(OBJ_TYPE_WALL, static_cast<int>(wall)))
        , _frameId { static_cast<int>(wall) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(ItemFrameId item)
        : _objectType(OBJ_TYPE_ITEM)
        , _fid(buildFid(OBJ_TYPE_ITEM, static_cast<int>(item)))
        , _frameId { static_cast<int>(item) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(TileFrameId tile)
        : _objectType(OBJ_TYPE_TILE)
        , _fid(buildFid(OBJ_TYPE_TILE, static_cast<int>(tile)))
        , _frameId { static_cast<int>(tile) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(SkillDexFrameId skilldex)
        : _objectType(OBJ_TYPE_SKILLDEX)
        , _fid(buildFid(OBJ_TYPE_SKILLDEX, static_cast<int>(skilldex)))
        , _frameId { static_cast<int>(skilldex) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(InterfaceFrameId interface)
        : _objectType(OBJ_TYPE_INTERFACE)
        , _fid(buildFid(OBJ_TYPE_INTERFACE, static_cast<int>(interface)))
        , _frameId { static_cast<int>(interface) }
        , _path(nullptr)
    {
    }

    // cannot be made constexpr as internally calls artExists which cannot be constexpr
    explicit FrmId(CritterFrameId critter, AnimationType animType, WeaponAnimation weaponAnimation, Rotation rotation);
    explicit FrmId(Object* object, AnimationType animType, WeaponAnimation weaponAnimation, Rotation rotation);
    explicit FrmId(ObjectType objectType, int frmId, AnimationType animType = ANIM_STAND, WeaponAnimation weaponAnimation = WEAPON_ANIMATION_NONE, Rotation rotation = ROTATION_NE);

    constexpr explicit FrmId(HeadFrameId head, HeadAnimation headAnimation = HEAD_ANIMATION_VERY_GOOD_REACTION, int fidget = 0)
        : _objectType(OBJ_TYPE_HEAD)
        , _fid(buildFid(OBJ_TYPE_HEAD, static_cast<int>(head), headAnimation, fidget))
        , _frameId { static_cast<int>(head) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(BackgroundFrameId background)
        : _objectType(OBJ_TYPE_BACKGROUND)
        , _fid(buildFid(OBJ_TYPE_BACKGROUND, static_cast<int>(background)))
        , _frameId { static_cast<int>(background) }
        , _path(nullptr)
    {
    }

    constexpr explicit FrmId(ObjectType objType, const char* path)
        : _objectType(objType)
        , _fid(kEmptyFid)
        , _frameId { kInvalidFrameId }
        , _path(path)

    {
        assert(objectTypeIsValid(objType));
    }

    constexpr int fid() const { return _fid; }

    constexpr bool hasObjectType() const { return objectTypeIsValid(_objectType); }

    constexpr ObjectType objectType() const
    {
        assert(hasObjectType());
        return _objectType;
    }

    constexpr const char* filePath() const { return _path; }

    bool valid() const { return !empty() && ((_frameId.id >= kMinFrameId && _frameId.id <= kMaxFrameId) || _path != nullptr); }

    bool empty() const { return (*this) == Empty(); }

    constexpr const FrameId& frameId() const { return _frameId; }

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
    ObjectType _objectType;
    int _fid;

    FrameId _frameId;

    const char* _path;

    /* FID Structure:
        3 bits for rotation
        4 bits for object type
        8 bits for animation type
        4 bits for weapon code
        12 bits for frame ID

        animType doesn't have to be of AnimationType enum only but also HeadAnimation
        weaponAnimation doesn't have to be WeaponAnimation enum only but also Fidget or flags
    */
    constexpr int buildFid(ObjectType objectType, int frmId, unsigned char animType = 0, unsigned char weaponAnimation = 0, Rotation rotation = ROTATION_NE)
    {
        return ((rotation << 28) & 0x70000000) | (objectType << 24) | ((animType << 16) & 0xFF0000) | ((weaponAnimation << 12) & 0xF000) | (frmId & kMaxFrameId);
    }

    int buildObjectFid(ObjectType objectType, int frmId, AnimationType animType, WeaponAnimation weaponCode, Rotation rotation);
};

template <typename T>
struct MapFrameIdToObjectType;

template <>
struct MapFrameIdToObjectType<MiscFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_MISC;
};
template <>
struct MapFrameIdToObjectType<SceneryFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_SCENERY;
};
template <>
struct MapFrameIdToObjectType<WallFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_WALL;
};
template <>
struct MapFrameIdToObjectType<ItemFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_ITEM;
};
template <>
struct MapFrameIdToObjectType<TileFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_TILE;
};
template <>
struct MapFrameIdToObjectType<SkillDexFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_SKILLDEX;
};
template <>
struct MapFrameIdToObjectType<InterfaceFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_INTERFACE;
};
template <>
struct MapFrameIdToObjectType<BackgroundFrameId> {
    static constexpr ObjectType value = OBJ_TYPE_BACKGROUND;
};

template <ObjectType ObjType, typename TFrameId>
class TypedFrmId : public FrmId {
public:
    static_assert(
        MapFrameIdToObjectType<TFrameId>::value == ObjType,
        "TypedFrmId can only be instantiated with a supported frame id type");

    constexpr TypedFrmId()
        : FrmId()
    {
    }
    constexpr TypedFrmId(TFrameId frameId)
        : FrmId(frameId)
    {
    }
    constexpr TypedFrmId(const char* path)
        : FrmId(ObjType, path)
    {
    }

    using FrmId::operator==;
    using FrmId::operator!=;
};

using MiscFrmId = TypedFrmId<OBJ_TYPE_MISC, MiscFrameId>;
using SceneryFrmId = TypedFrmId<OBJ_TYPE_SCENERY, SceneryFrameId>;
using WallFrmId = TypedFrmId<OBJ_TYPE_WALL, WallFrameId>;
using ItemFrmId = TypedFrmId<OBJ_TYPE_ITEM, ItemFrameId>;
using TileFrmId = TypedFrmId<OBJ_TYPE_TILE, TileFrameId>;
using SkillDexFrmId = TypedFrmId<OBJ_TYPE_SKILLDEX, SkillDexFrameId>;
using InterfaceFrmId = TypedFrmId<OBJ_TYPE_INTERFACE, InterfaceFrameId>;
using BackgroundFrmId = TypedFrmId<OBJ_TYPE_BACKGROUND, BackgroundFrameId>;

class CritterFrmId : public FrmId {
public:
    constexpr CritterFrmId()
        : FrmId()
    {
    }

    // cannot be made constexpr as internally calls artExists which cannot be constexpr
    CritterFrmId(CritterFrameId critter, AnimationType animType, WeaponAnimation weaponAnimation, Rotation rotation)
        : FrmId(critter, animType, weaponAnimation, rotation)
    {
    }

    constexpr CritterFrmId(const char* path)
        : FrmId(OBJ_TYPE_CRITTER, path)
    {
    }

    using FrmId::operator==;
    using FrmId::operator!=;
};

class HeadFrmId : public FrmId {
public:
    constexpr HeadFrmId()
        : FrmId()
    {
    }

    constexpr explicit HeadFrmId(HeadFrameId head, HeadAnimation headAnimation = HEAD_ANIMATION_VERY_GOOD_REACTION, int fidget = 0)
        : FrmId(head, headAnimation, fidget)
    {
    }

    constexpr HeadFrmId(const char* path)
        : FrmId(OBJ_TYPE_HEAD, path)
    {
    }

    using FrmId::operator==;
    using FrmId::operator!=;
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
