#ifndef TRAIT_DEFS
#define TRAIT_DEFS

namespace fallout {

// The maximum number of traits a player is allowed to select.
#define TRAITS_MAX_SELECTED_COUNT 2

// Available traits.
enum Trait : int {
    TRAIT_INVALID = -1,
    TRAIT_FAST_METABOLISM,
    TRAIT_BRUISER,
    TRAIT_SMALL_FRAME,
    TRAIT_ONE_HANDER,
    TRAIT_FINESSE,
    TRAIT_KAMIKAZE,
    TRAIT_HEAVY_HANDED,
    TRAIT_FAST_SHOT,
    TRAIT_BLOODY_MESS,
    TRAIT_JINXED,
    TRAIT_GOOD_NATURED,
    TRAIT_CHEM_RELIANT,
    TRAIT_CHEM_RESISTANT,
    TRAIT_SEX_APPEAL,
    TRAIT_SKILLED,
    TRAIT_GIFTED,
    TRAIT_COUNT,
    TRAIT_FIRST = TRAIT_FAST_METABOLISM
};


inline bool traitIsValid(int trait)
{
    return trait >= TRAIT_FIRST && trait < TRAIT_COUNT;
}

inline Trait operator++(Trait& e, int)
{
    Trait result = e;
    e = static_cast<Trait>(static_cast<int>(e) + 1);
    return result;
}

} // namespace fallout

#endif /* TRAIT_DEFS */
