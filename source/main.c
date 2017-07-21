#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include "srvsys.h"
#include "draw.h"
#include "service.h"
#include "logger.h"
#include "input.h"
#include "ifile.h"
#include "nfc3d/amitool.h"

#define MAX_SESSIONS 1
#define SERVICE_ENDPOINTS 3

#define AMIIBO_YEAR_FROM_DATE(dt) ((dt >> 1) + 2000)
#define AMIIBO_MONTH_FROM_DATE(dt) ((*((&dt)+1) >> 5) & 0xF)
#define AMIIBO_DAY_FROM_DATE(dt) (*((&dt)+1) & 0x1F)

#define BSWAP_U32(num) ((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000)


static bool showString(const char *str, u32 value);

void showBuf(char *prefix, u8* data, size_t len) {
	char bufstr[len*3 + 3];
	memset(bufstr, 0, sizeof(bufstr));
	for(int pos=0; pos<len; pos++) {
		snprintf(&bufstr[pos*3], 4, "%02x ", data[pos]);
		if (pos > 0 && pos % 12 == 0) {
			bufstr[pos*3+2] = '\n';
		}
	}
	showString(bufstr, 0);
}


typedef enum {
	AS_NOT_INITIALISED, AS_INITIALISED, AS_COMMS_STARTED, AS_SCAN_STARTED, AS_IN_RANGE, AS_DATA_LOADED, AS_OUT_OF_RANGE,AS_STOP_SCAN
} SCANNER_STATE;


static u8 AmiiboFileRaw[AMIIBO_MAX_SIZE];
static u8 AmiiboFilePlain[AMIIBO_MAX_SIZE];

static SCANNER_STATE state = AS_NOT_INITIALISED;


static void logState(int cmd, SCANNER_STATE state) {
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

	//we need to preserve the command buffer for reading any parameter values as any
	//service calls (such as IFILE_Write) will clobber it.

	logState(cmdid, state);
	//showString("cmd", cmdbuf[0]);
	static int stateCheckCounter = 10;
	switch (cmdid) {
		case 0x1: { //initialise
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_INITIALISED;
			stateCheckCounter = 10;
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
			stateCheckCounter = 10;
			break;
		}
		case 0x4: { //StopCommunication
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_INITIALISED;
			stateCheckCounter = 10;
			break;
		}
		case 0x5: { //StartTagScanning
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_SCAN_STARTED;
			stateCheckCounter = 10;
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
		case 0x08: { //ResetTagScanState
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			state = AS_SCAN_STARTED;
			stateCheckCounter = 10;
			break;
		}
		case 0x9: { //UpdateStoredAmiiboData gets called to register owner and nickname
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			break;
		}
		case 0xd: { //GetTagState
			cmdbuf[0] = IPC_MakeHeader(cmdid, 2, 0);
			cmdbuf[1] = 0;

			// 3- TAG_IN_RANGE, 4-TAG_OUT_OF_RANEG,5-tag data loaded (after LoadAmiiboData)
			switch (state) {
				case AS_SCAN_STARTED:
					cmdbuf[2] = NFC_TagState_Scanning;
					stateCheckCounter --;
					if (stateCheckCounter <0 && showString("place amiibo?", 0))
						state = AS_IN_RANGE;
					else
						state = AS_SCAN_STARTED;
					break;
				case AS_IN_RANGE:
					cmdbuf[2] = NFC_TagState_InRange;
					stateCheckCounter --;
					if (stateCheckCounter <0 && showString("remove amiibo?", 0))
					{
						showString("Removing amiibo..", 1);
						state = AS_OUT_OF_RANGE;
						stateCheckCounter = 10;
					}
					break;
				case AS_DATA_LOADED:
					cmdbuf[2] = NFC_TagState_DataReady;
					stateCheckCounter --;
					if (stateCheckCounter < 0 && showString("remove amiibo?", 0))
					{
						showString("Removing amiibo..", 2);
						state = AS_OUT_OF_RANGE;
						stateCheckCounter = 10;
					}
					break;
				case AS_OUT_OF_RANGE:
					cmdbuf[2] = NFC_TagState_OutOfRange;
					break;
				default:
					cmdbuf[2] = NFC_TagState_ScanningStopped;
					break;
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
			NFC_TagInfo taginfo;
			memset(&taginfo, 0, sizeof(taginfo));
			taginfo.id_offset_size = 0x7;
			taginfo.unk_x3 = 0x02;
			taginfo.id[0] = AmiiboFileRaw[0];
			taginfo.id[1] = AmiiboFileRaw[1];
			taginfo.id[2] = AmiiboFileRaw[2];
			taginfo.id[3] = AmiiboFileRaw[4];
			taginfo.id[4] = AmiiboFileRaw[5];
			taginfo.id[5] = AmiiboFileRaw[6];
			taginfo.id[6] = AmiiboFileRaw[7];

			cmdbuf[0] = IPC_MakeHeader(cmdid, (sizeof(taginfo)/4)+1, 0);
			cmdbuf[1] = 0;
            memcpy(&cmdbuf[2], &taginfo, sizeof(taginfo));
			stateCheckCounter = 10;
            break;
		}
		case 0x12: { //CommunicationGetResult
			cmdbuf[0] = IPC_MakeHeader(cmdid, 2, 0);
			cmdbuf[1] = 0;
			cmdbuf[2] = 0;
            break;
		}
		case 0x13: { //OpenAppData

			u32 appid = cmdbuf[1];
			appid = BSWAP_U32(appid);

			if (memcmp(&appid, &AmiiboFilePlain[0xB6], sizeof(appid))) {
				showString("> 0x13 ", 1);
				cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
				cmdbuf[1] = 0xC8A17638; //app id mismatch
				break;
			}

			if (!(AmiiboFilePlain[0x2C] & 0x20)) {
				showString("> 0x13 ", 2);
				cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
				cmdbuf[1] = 0xC8A17620; //uninitialised
				break;
			}

			showString("> 0x13 ", 0);
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			stateCheckCounter = 10;
			break;
		}
		case 0x15: { //ReadAppData
			//logPrintf("readappdata bufsize %x\n", cmdcache[1]);
			//if (cmdcache[1] < 0xD8) {
			//	showString("size too small\n", cmdcache[1]);
			//} else {
			//	showString("size\n", cmdcache[1]);
			//}

			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 2);
			cmdbuf[1] = 0;
			cmdbuf[2]=IPC_Desc_StaticBuffer(0xD8,0);
			//the buffer must exist outside of the function scope as it must survive till the svcReceiveReply is called
			cmdbuf[3]=(u32)&AmiiboFilePlain[0xDC];

			stateCheckCounter = 10;
            break;
		}
		case 0x17: { // 	GetAmiiboSettings
			NFC_AmiiboSettings amsettings;

			memset(&amsettings, 0, sizeof(amsettings));

			cmdbuf[0] = IPC_MakeHeader(cmdid, (sizeof(amsettings)/4)+1, 0);
			if (!(AmiiboFilePlain[0x2C] & 0x10)) {
				cmdbuf[1] = 0xC8A17628; //uninitialised
			} else {
				cmdbuf[1] = 0;
				memcpy(amsettings.mii, &AmiiboFilePlain[0x4C], sizeof(amsettings.mii));
				memcpy(amsettings.nickname, &AmiiboFilePlain[0x38], 4*5);  //amiibo doesnt have the null terminator
				amsettings.flags = AmiiboFilePlain[0x2C] & 0xF; //todo: we should only load some of these values if the unused flag bits are set correctly https://3dbrew.org/wiki/Amiibo
				amsettings.countrycodeid = AmiiboFilePlain[0x2D];

				amsettings.setupdate_year = AMIIBO_YEAR_FROM_DATE(AmiiboFilePlain[0x30]);
				amsettings.setupdate_month = AMIIBO_MONTH_FROM_DATE(AmiiboFilePlain[0x30]);
				amsettings.setupdate_day = AMIIBO_DAY_FROM_DATE(AmiiboFilePlain[0x30]);
			}

            memcpy(&cmdbuf[2], &amsettings, sizeof(amsettings));
			stateCheckCounter = 10;
			break;
		}
		case 0x18: { // 	GetAmiiboConfig

			NFC_AmiiboConfig amconfig;
			memset(&amconfig, 0, sizeof(amconfig));

			amconfig.lastwritedate_year = AMIIBO_YEAR_FROM_DATE(AmiiboFilePlain[0x32]);
			amconfig.lastwritedate_month = AMIIBO_MONTH_FROM_DATE(AmiiboFilePlain[0x32]);
			amconfig.lastwritedate_day = AMIIBO_DAY_FROM_DATE(AmiiboFilePlain[0x32]);

			amconfig.write_counter = (AmiiboFilePlain[0xB4] << 8) | AmiiboFilePlain[0xB5];
			amconfig.characterID[0] = AmiiboFilePlain[0x1DC];
			amconfig.characterID[1] = AmiiboFilePlain[0x1DD];
			amconfig.characterID[2] = AmiiboFilePlain[0x1DE];
			amconfig.series =  AmiiboFilePlain[0x1e2];
			amconfig.amiiboID = (AmiiboFilePlain[0x1e0] << 8) | AmiiboFilePlain[0x1e1];
			amconfig.type = AmiiboFilePlain[0x1DF];
			amconfig.pagex4_byte3 = AmiiboFilePlain[0x2B]; //raw page 0x4 byte 0x3, dec byte
			amconfig.appdata_size = 0xD8;

			cmdbuf[0] = IPC_MakeHeader(cmdid, (sizeof(amconfig)/4)+1, 0);
			cmdbuf[1] = 0;
            memcpy(&cmdbuf[2], &amconfig, sizeof(amconfig));
			stateCheckCounter = 10;
			break;
		}
		case 0x401: { //gets called to write data when resetting an amiibo
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;

			//we ignore the paremeters to this command and just clear the data
			//todo: figure out the parameters of the native service?

			//todo: just clearing the data flag for now. should clear the settings and appdata sections.
			AmiiboFilePlain[0x2C] = 0x0;



			//logBuf("0x401 command", &cmdbuf[1], 3 * 4);
			break;
		}
		case 0x402: { // 	unknown nfc:m method
			cmdbuf[0] = IPC_MakeHeader(cmdid, 17, 0);
			cmdbuf[1] = 0;
            memset(&cmdbuf[2], 0, 16*4); //wrong size

			break;
		}
		case 0x404: { // 	SetAmiiboSettings
			NFC_AmiiboSettings amsettings;
			memcpy(&amsettings, &cmdbuf[1], 41 * 4);

			memcpy(&AmiiboFilePlain[0x4C], amsettings.mii, sizeof(amsettings.mii));
			memcpy(&AmiiboFilePlain[0x38], amsettings.nickname, 4*5);


			AmiiboFilePlain[0x2C] = ((AmiiboFilePlain[0x2C] & 0xF0) | (amsettings.flags & 0xF) | 0x10); //merge the flags with settings set flag
			/*
			todo: set the setup date (inverse of this:)
				amsettings.setupdate_year = AMIIBO_YEAR_FROM_DATE(AmiiboFilePlain[0x30]);
				amsettings.setupdate_month = AMIIBO_MONTH_FROM_DATE(AmiiboFilePlain[0x30]);
				amsettings.setupdate_day = AMIIBO_DAY_FROM_DATE(AmiiboFilePlain[0x30]);
				*/
			//AmiiboFilePlain[0x2D] = how to find the country code?

			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
			stateCheckCounter = 10;
			break;
		}
		case 0x407: { // 	unknown nfc:m method
			cmdbuf[0] = IPC_MakeHeader(cmdid, 2, 0);
			cmdbuf[1] = 0;
			cmdbuf[2] = 0;
			/*
			if (showString("remove?", 0))
				state = AS_IN_RANGE;
			else
				state = AS_OUT_OF_RANGE;
				*/

			//state = AS_OUT_OF_RANGE;
			break;
		}

		default: {// error
			//logStr("NOT IMPLEMENTED\n");
			showString("NOT IMPLEMENTED\n", cmdbuf[0]);
			//cmdbuf[0] = 0x40;
			//cmdbuf[1] = 0xD900183F;
			cmdbuf[0] = IPC_MakeHeader(cmdid, 1, 0);
			cmdbuf[1] = 0;
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

static bool showString(const char *str, u32 value) {
	svcKernelSetState(0x10000, 1);

	Draw_SetupFramebuffer();
	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();

	char buf[2048];

	sprintf(buf, "%s  0x%08x", str, (unsigned int)value);

	Draw_DrawString(10, 10, COLOR_TITLE, buf);
	u32 key = waitInputWithTimeout(10 * 1000);
	if (key & BUTTON_DOWN) {
		svcBreak(USERBREAK_ASSERT);
	}

	Draw_RestoreFramebuffer();
	Draw_Unlock();
	svcKernelSetState(0x10000, 1);

	return key & BUTTON_UP;
}

static int loadFile(const char *filename, u8 *data, int len) {
	IFile file;

    Result res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_READ);
    if (R_FAILED(res)) {
		return 0;
    }

    u64 size;

    res = IFile_GetSize(&file, &size);
    if (R_FAILED(res)) goto loadFile_err;

    if (size < len)
		len = size;

	res = IFile_Read(&file, &size, data, len);
	if (R_FAILED(res)) goto loadFile_err;

	IFile_Close(&file);
	return size;
loadFile_err:
	IFile_Close(&file);
	return 0;
}


int main() {
	//char *file = "/amiibo/Zelda (SSB)_201706081138.bin";
	char *file = "/Fox.bin";

	if (loadFile(file , AmiiboFileRaw, AMIIBO_MAX_SIZE)<=540) {
		showString("Failed to read amiibo dump", 0);
	}

	u8 key[160];
	if (loadFile("/amiibo_keys.bin", key, sizeof(key))!=160) {
		showString("Failed to read amiibo key", 0);
	} else {
		amitool_setKeys(key, 160);
		if (!amitool_unpack(AmiiboFileRaw, AMIIBO_MAX_SIZE, AmiiboFilePlain, AMIIBO_MAX_SIZE))
			showString("Failed to decrypt amiibo", 0);
	}

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
		switch (request_index) {
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
