#include "service.h"
#include "srvsys.h"

static Handle fsuHandle;

Result fsSysInit() {
    //since the "srv" service is initalised by our srvsys.c methods, normal ctrulib init function cannot work directly
    //as they don't have access to the "srv" service handle
    Result ret;

	ret = srvSysGetServiceHandle(&fsuHandle, "fs:USER");
	if (R_SUCCEEDED(ret) && envGetHandle("fs:USER") == 0) {
		ret = FSUSER_Initialize(fsuHandle);
		if (R_FAILED(ret))
            svcCloseHandle(fsuHandle);
        else
            fsUseSession(fsuHandle);
	}

	FSUSER_SetPriority(0);

    return ret;
}

void fsSysExit() {
    svcCloseHandle(fsuHandle);
}
