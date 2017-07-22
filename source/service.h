#pragma once

#include <3ds.h>

#ifdef __cplusplus
extern "C" {
#endif

Handle fsSysGetSession();
Result fsSysInit();
void fsSysExit();

#ifdef __cplusplus
}
#endif
