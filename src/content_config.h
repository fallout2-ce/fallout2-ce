#ifndef CONTENT_CONFIG_H
#define CONTENT_CONFIG_H

#include "config.h"

namespace fallout {

extern Config gContentConfig;

bool contentConfigInit();
void contentConfigExit();

}

#endif // CONTENT_CONFIG_H
