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

void showString(const char *str, int value);


typedef enum {
	AS_NOT_INITIALISED, AS_INITIALISED, AS_COMMS_STARTED, AS_SCAN_STARTED, AS_IN_RANGE, AS_DATA_LOADED, AS_OUT_OF_RANGE,AS_STOP_SCAN
} SCANNER_STATE;


bool amiiboloaded = 0;
bool counter = 0;
bool removeAmiibo = 0;

SCANNER_STATE state = AS_NOT_INITIALISED;


void logState(int cmd, SCANNER_STATE state) {
	char *cmdstr = NULL;
	switch (cmd) {
		case 0x0001: cmdstr = "Initialize"; break;
		case 0x0002: cmdstr = "Shutdown"; break;
		case 0x0003: cmdstr = "StartCommunication"; break;
		case 0x0004: cmdstr = "StopCommunication"; break;
		case 0x0005: cmdstr = "StartTagScanning"; break;
		case 0x0006: cmdstr = "StopTagScanning"; break;
		case 0x0007: cmdstr = "LoadAmiiboData"; break;
		case 0x0008: cmdstr = "ResetTagScanState"; break;
		case 0x0009: cmdstr = "UpdateStoredAmiiboData"; break;
		case 0x000B: cmdstr = "GetTagInRangeEvent(?)"; break;
		case 0x000C: cmdstr = "GetTagOutOfRangeEvent(?)"; break;
		case 0x000D: cmdstr = "GetTagState"; break;
		case 0x000F: cmdstr = "CommunicationGetStatus"; break;
		case 0x0010: cmdstr = "GetTagInfo2"; break;
		case 0x0011: cmdstr = "GetTagInfo"; break;
		case 0x0012: cmdstr = "CommunicationGetResult"; break;
		case 0x0013: cmdstr = "OpenAppData"; break;
		case 0x0014: cmdstr = "InitializeWriteAppData"; break;
		case 0x0015: cmdstr = "ReadAppData"; break;
		case 0x0016: cmdstr = "WriteAppData"; break;
		case 0x0017: cmdstr = "GetAmiiboSettings"; break;
		case 0x0018: cmdstr = "GetAmiiboConfig"; break;
		case 0x0019: cmdstr = "GetAppDataInitStruct"; break;
		case 0x001F: cmdstr = "StartOtherTagScanning"; break;
		case 0x0020: cmdstr = "SendTagCommand"; break;
		case 0x0021: cmdstr = "Cmd21"; break;
		case 0x0022: cmdstr = "Cmd22"; break;
		case 0x0404: cmdstr = "SetAmiiboSettings"; break;
	}

	char *statestr = NULL;
	switch (state) {
		case AS_NOT_INITIALISED: statestr = "AS_NOT_INITIALISED"; break;
		case AS_INITIALISED: statestr = "AS_INITIALISED"; break;
		case AS_COMMS_STARTED: statestr = "AS_COMMS_STARTED"; break;
		case AS_SCAN_STARTED: statestr = "AS_SCAN_STARTED"; break;
		case AS_IN_RANGE: statestr = "AS_IN_RANGE"; break;
		case AS_DATA_LOADED: statestr = "AS_DATA_LOADED"; break;
		case AS_OUT_OF_RANGE: statestr = "AS_OUT_OF_RANGE"; break;
		case AS_STOP_SCAN: statestr = "AS_STOP_SCAN"; break;
		default:
			statestr = "Unknown";
	}

	if (cmdstr != NULL) {
        logPrintf("cmd %s\tstate %s\n", cmdstr, statestr);
	} else {
        logPrintf("cmd %08x\tstate %s\n", cmd, statestr);
	}
}

static void handle_commands(void) {
	u32* cmdbuf = getThreadCommandBuffer();
	u16 cmdid = cmdbuf[0] >> 16;

	showString("cmd", cmdid);
	logState(cmdid, state);

	switch (cmdid) {
		case 0x1: { //initialise
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_INITIALISED;
			break;
		}
		case 0x2: { //shutdown
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_NOT_INITIALISED;
			break;
		}
		case 0x3: { //StartCommunication
			cmdbuf[0] = 0x30040;
			cmdbuf[1] = 0;
			state = AS_COMMS_STARTED;
			break;
		}
		case 0x4: { //StopCommunication
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_INITIALISED;
			break;
		}
		case 0x5: { //StartTagScanning
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_SCAN_STARTED;
			break;
		}
		case 0x6: { //StopTagScanning
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_COMMS_STARTED;
			break;
		}
		case 0x07: { //LoadAmiiboData
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_DATA_LOADED;
			break;
		}
		case 0xd: { //GetTagState
			cmdbuf[0] = IPC_MakeHeader(cmdid, 2, 0);
			cmdbuf[1] = 0;

			// 3- TAG_IN_RANGE, 4-TAG_OUT_OF_RANEG,5-tag data loaded (after LoadAmiiboData)
			if (state == AS_SCAN_STARTED || state == AS_IN_RANGE) {
				cmdbuf[2] = 0x03;
				state = AS_IN_RANGE;
			} else if (state == AS_DATA_LOADED) {
				cmdbuf[2] = 0x05;
			} if (state == AS_OUT_OF_RANGE) {
				cmdbuf[2] = 0x03;
			} else {
				cmdbuf[2] = 0x03;
			}
			break;
		}
		case 0xf: { //CommunicationGetStatus
			cmdbuf[0] = IPC_MakeHeader(cmdid, 2, 0);
			cmdbuf[1] = 0;
			cmdbuf[2] = 0x02;
			break;
		}
		case 0x11: { //GetTagInfo
			NFC_TagInfo taginfo = {
				0x7,
				0x0,
				0x02,
				{0x04,0xA4,0x7F,0x52,0xC2,0x3E,0x80}
			};
			cmdbuf[0] = IPC_MakeHeader(cmdid, 13, 0);
			cmdbuf[1] = 0;
            memcpy(&cmdbuf[2], &taginfo, sizeof(taginfo));
            break;
		}
		case 0x12: { //CommunicationGetResult
			cmdbuf[0] = IPC_MakeHeader(cmdid, 2, 0);
			cmdbuf[1] = 0;
			cmdbuf[2] = 0;
            break;
		}
		case 0x13: { //OpenAppData
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
            break;
		}
		case 0x15: { //ReadAppData
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
            break;
		}
		case 0x17: { // 	GetAmiiboSettings
			NFC_AmiiboSettings settings;
			cmdbuf[0] = IPC_MakeHeader(cmdid, 44, 0);
			cmdbuf[1] = 0;
            memcpy(&cmdbuf[2], &settings, sizeof(settings));
			break;
		}
		case 0x18: { // 	GetAmiiboConfig
			NFC_AmiiboConfig  config;
			cmdbuf[0] = IPC_MakeHeader(cmdid, 18, 0);
			cmdbuf[1] = 0;
            memcpy(&cmdbuf[2], &config, sizeof(config));
			break;
		}
		case 0x402: { // 	unknown nfc:m method
			cmdbuf[0] = IPC_MakeHeader(cmdid, 17, 0);
			cmdbuf[1] = 0;
            memset(&cmdbuf[2], 0, 16);
			break;
		}
		case 0x404: { // 	SetAmiiboSettings
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
		}
		case 0x407: { // 	unknown nfc:m method
			cmdbuf[0] = IPC_MakeHeader(cmdid, 2, 0);
			cmdbuf[1] = 0;
			cmdbuf[2] = 0;
			state = AS_OUT_OF_RANGE;
			break;
		}

		default: {// error
			cmdbuf[0] = 0x40;
			cmdbuf[1] = 0xD900183F;
			break;
		}
	}
}

static Result should_terminate(int *term_request) {
	u32 notid;

	Result ret = srvSysReceiveNotification(&notid);
	if (R_FAILED(ret)) {
		return ret;
	}
	if (notid == 0x100) {// term request
		*term_request = 1;
	}
	return 0;
}

Result fsinitValue;

// this is called before main
void __appInit() {
	srvSysInit();
	fsSysInit();
	logInit();
}

// this is called after main exits
void __appExit() {
	logExit();
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
	logStr("test message v2");
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

void showString(const char *str, int value) {
	svcKernelSetState(0x10000, 1);

	Draw_SetupFramebuffer();
	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();

	char buf[2048];

	sprintf(buf, "%s  %x", str, value);

	Draw_DrawString(10, 10, COLOR_TITLE, buf);
	u32 key = waitInputWithTimeout(10 * 1000);
	if (key & BUTTON_DOWN) {
		svcBreak(USERBREAK_ASSERT);
	}

	Draw_RestoreFramebuffer();
	Draw_Unlock();
	svcKernelSetState(0x10000, 1);
}

int main() {
	int g_active_handles;

	Result ret = 0;

	Handle *hndNfuU;
	Handle *hndNfuM;
	Handle *hndNotification;

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
