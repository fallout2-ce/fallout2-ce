#ifndef SKILLDEX_H
#define SKILLDEX_H

namespace fallout {

enum SkilldexRC : int {
    SKILLDEX_RC_ERROR = -1,
    SKILLDEX_RC_CANCELED,
    SKILLDEX_RC_SNEAK,
    SKILLDEX_RC_LOCKPICK,
    SKILLDEX_RC_STEAL,
    SKILLDEX_RC_TRAPS,
    SKILLDEX_RC_FIRST_AID,
    SKILLDEX_RC_DOCTOR,
    SKILLDEX_RC_SCIENCE,
    SKILLDEX_RC_REPAIR,
    SKILLDEX_RC_COUNT,
};

SkilldexRC skilldexOpen();
int skilldexGetWindow();

} // namespace fallout

#endif /* SKILLDEX_H */
