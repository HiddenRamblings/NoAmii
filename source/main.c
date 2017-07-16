#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include "srvsys.h"
#include "draw.h"
#include "service.h"
#include "logger.h"
#include "input.h"

#define MAX_SESSIONS 1
#define SERVICE_ENDPOINTS 3
static void handle_commands(void)
{
	u32* cmdbuf;
	u16 cmdid;
	//int res;

	cmdbuf = getThreadCommandBuffer();
	cmdid = cmdbuf[0] >> 16;
	//res = 0;
	switch (cmdid) {
	case 1: {//initialise
		cmdbuf[0] = 0x30040;
		cmdbuf[1] = 0;
		break;
	}
	default: // error
	{
		cmdbuf[0] = 0x40;
		cmdbuf[1] = 0xD900183F;
		break;
	}
	}
	/*
	FS_ProgramInfo title;
	FS_ProgramInfo update;
	u32* cmdbuf;
	u16 cmdid;
	int res;
	Handle handle;
	u64 prog_handle;

	cmdbuf = getThreadCommandBuffer();
	cmdid = cmdbuf[0] >> 16;
	res = 0;
	switch (cmdid)
	{
	case 1: // LoadProcess
	{
		res = loader_LoadProcess(&handle, *(u64 *)&cmdbuf[1]);
		cmdbuf[0] = 0x10042;
		cmdbuf[1] = res;
		cmdbuf[2] = 16;
		cmdbuf[3] = handle;
		break;
	}
	case 2: // RegisterProgram
	{
		memcpy(&title, &cmdbuf[1], sizeof(FS_ProgramInfo));
		memcpy(&update, &cmdbuf[5], sizeof(FS_ProgramInfo));
		res = loader_RegisterProgram(&prog_handle, &title, &update);
		cmdbuf[0] = 0x200C0;
		cmdbuf[1] = res;
		*(u64 *)&cmdbuf[2] = prog_handle;
		break;
	}
	case 3: // UnregisterProgram
	{
		if (g_cached_prog_handle == prog_handle)
		{
		g_cached_prog_handle = 0;
		}
		cmdbuf[0] = 0x30040;
		cmdbuf[1] = loader_UnregisterProgram(*(u64 *)&cmdbuf[1]);
		break;
	}
	case 4: // GetProgramInfo
	{
		prog_handle = *(u64 *)&cmdbuf[1];
		if (prog_handle != g_cached_prog_handle)
		{
		res = loader_GetProgramInfo(&g_exheader, prog_handle);
		if (res >= 0)
		{
			g_cached_prog_handle = prog_handle;
		}
		else
		{
			g_cached_prog_handle = 0;
		}
		}
		memcpy(&g_ret_buf, &g_exheader, 1024);
		cmdbuf[0] = 0x40042;
		cmdbuf[1] = res;
		cmdbuf[2] = 0x1000002;
		cmdbuf[3] = (u32) &g_ret_buf;
		break;
	}
	default: // error
	{
		cmdbuf[0] = 0x40;
		cmdbuf[1] = 0xD900182F;
		break;
	}
	} */
}

static Result should_terminate(int *term_request)
{
	u32 notid;
	Result ret;

	ret = srvSysReceiveNotification(&notid);
	if (R_FAILED(ret))
	{
	return ret;
	}
	if (notid == 0x100) // term request
	{
	*term_request = 1;
	}
	return 0;
}

Result fsinitValue;

// this is called before main
void __appInit() {
	srvSysInit();
	fsSysInit();

}

// this is called after main exits
void __appExit() {

	fsSysExit();
	srvSysExit();
}

// stubs for non-needed pre-main functions
void __sync_init();
void __sync_fini();
void __system_initSyscalls();
void __libc_init_array(void);
void __libc_fini_array(void);

void initSystem(void (*retAddr)(void)) {
	__libc_init_array();
	__sync_init();
	__system_initSyscalls();
	__appInit();
}

void __ctru_exit(int rc) {
	__appExit();
	__sync_fini();
	__libc_fini_array();
	svcExitProcess();
}


void testLog() {
	logInit();
	logStr("test message v2");
	logExit();
}

void doDraw() {
	svcKernelSetState(0x10000, 1);
	//svcSleepThread(2000000000);
	Draw_SetupFramebuffer();

	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();

	testLog();

	char data[2014];

	sprintf(data, "0x%08x", (unsigned int)fsinitValue);
	Draw_DrawString(10, 10, COLOR_TITLE, data);//"Hello world1\n");
	//svcSleepThread(2000000000);
	waitInputWithTimeout(10 * 1000);

	Draw_RestoreFramebuffer();
	Draw_Unlock();
	svcKernelSetState(0x10000, 1);
}

int main() {
	doDraw();

	int g_active_handles;
	//char g_ret_buf[1024];


	Result ret;

	Handle *hndNfuU;
	Handle *hndNfuM;
	Handle *hndNotification;

	ret = 0;

	Handle g_handles[MAX_SESSIONS+SERVICE_ENDPOINTS];
	hndNotification = &g_handles[0];
	hndNfuU = &g_handles[1];
	hndNfuM = &g_handles[2];
	g_active_handles = SERVICE_ENDPOINTS;

	if (R_FAILED(srvSysRegisterService(hndNfuU, "nfc:u", MAX_SESSIONS))) {
		svcBreak(USERBREAK_ASSERT);
	}
	if (R_FAILED(srvSysRegisterService(hndNfuM, "nfc:m", MAX_SESSIONS))) {
		svcBreak(USERBREAK_ASSERT);
	}
	if (R_FAILED(srvSysEnableNotification(hndNotification))) {
		svcBreak(USERBREAK_ASSERT);
	}

	Handle reply_target = 0;
	int term_request = 0;
	do {
		if (reply_target == 0) {
			u32 *cmdbuf = getThreadCommandBuffer();
			cmdbuf[0] = 0xFFFF0000;
		}
		s32 request_index;
		ret = svcReplyAndReceive(&request_index, g_handles, g_active_handles, reply_target);

		if (R_FAILED(ret)) {
			// check if any handle has been closed
			if (ret == 0xC920181A) {
				if (request_index == -1) {
					for (int i = 2; i < MAX_SESSIONS+SERVICE_ENDPOINTS; i++) {
						if (g_handles[i] == reply_target) {
							request_index = i;
							break;
						}
					}
				}
				svcCloseHandle(g_handles[request_index]);
				g_handles[request_index] = g_handles[g_active_handles-1];
				g_active_handles--;
				reply_target = 0;
			} else {
				svcBreak(USERBREAK_ASSERT);
			}
			continue;
		}

		// process responses
		reply_target = 0;
		switch (request_index)
		{
			case 0: { // notification
				if (R_FAILED(should_terminate(&term_request))) {
					svcBreak(USERBREAK_ASSERT);
				}
				break;
			}
			case 1: // new session
			case 2: {// new session
				Handle handle;
				if (R_FAILED(svcAcceptSession(&handle, g_handles[request_index]))) {
					svcBreak(USERBREAK_ASSERT);
				}
				if (g_active_handles < MAX_SESSIONS+SERVICE_ENDPOINTS) {
					g_handles[g_active_handles] = handle;
					g_active_handles++;
				} else {
					svcCloseHandle(handle);
				}
				break;
			}
			default: { // session
				handle_commands();
				reply_target = g_handles[request_index];
				break;
			}
		}
	} while (!term_request);

	srvSysUnregisterService("nfc:m");
	srvSysUnregisterService("nfc:u");

	svcCloseHandle(*hndNfuM);
	svcCloseHandle(*hndNfuU);
	svcCloseHandle(*hndNotification);

	return 0;
}
