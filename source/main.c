#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include "srvsys.h"
#include "draw.h"
#include "service.h"

#define MAX_SESSIONS 1

static Handle g_handles[MAX_SESSIONS+2];
static int g_active_handles;
static u64 g_cached_prog_handle;
static char g_ret_buf[1024];

static void handle_commands(void)
{
  u32* cmdbuf;
  u16 cmdid;
  int res;

  cmdbuf = getThreadCommandBuffer();
  cmdid = cmdbuf[0] >> 16;
  res = 0;
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
    openLogger();
    logstr("test message");
    closeLogger();
}

void *memset32(void *dest, u32 value, u32 size)
{
    u32 *dest32 = (u32 *)dest;

    for(u32 i = 0; i < size/4; i++) dest32[i] = value;

    return dest;
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

    sprintf(data, "%08x", (u64)fsinitValue);
	Draw_DrawString(10, 10, COLOR_TITLE, data);//"Hello world1\n");
	svcSleepThread(2000000000);
	Draw_RestoreFramebuffer();
	Draw_Unlock();
	svcKernelSetState(0x10000, 1);
}

int main()
{
	doDraw();
  Result ret;
  Handle handle;
  Handle reply_target;
  Handle *srv_handle;
  Handle *notification_handle;
  s32 index;
  int i;
  int term_request;
  u32* cmdbuf;

  ret = 0;
  srv_handle = &g_handles[1];
  notification_handle = &g_handles[0];

  if (R_FAILED(srvSysRegisterService(srv_handle, "nfc:u", MAX_SESSIONS)))
  {
    svcBreak(USERBREAK_ASSERT);
  }

  if (R_FAILED(srvSysEnableNotification(notification_handle)))
  {
    svcBreak(USERBREAK_ASSERT);
  }

  g_active_handles = 2;
  g_cached_prog_handle = 0;
  index = 1;

  reply_target = 0;
  term_request = 0;
  do
  {
    if (reply_target == 0)
    {
      cmdbuf = getThreadCommandBuffer();
      cmdbuf[0] = 0xFFFF0000;
    }
    ret = svcReplyAndReceive(&index, g_handles, g_active_handles, reply_target);

    if (R_FAILED(ret))
    {
      // check if any handle has been closed
      if (ret == 0xC920181A)
      {
        if (index == -1)
        {
          for (i = 2; i < MAX_SESSIONS+2; i++)
          {
            if (g_handles[i] == reply_target)
            {
              index = i;
              break;
            }
          }
        }
        svcCloseHandle(g_handles[index]);
        g_handles[index] = g_handles[g_active_handles-1];
        g_active_handles--;
        reply_target = 0;
      }
      else
      {
        svcBreak(USERBREAK_ASSERT);
      }
    }
    else
    {
      // process responses
      reply_target = 0;
      switch (index)
      {
        case 0: // notification
        {
          if (R_FAILED(should_terminate(&term_request)))
          {
            svcBreak(USERBREAK_ASSERT);
          }
          break;
        }
        case 1: // new session
        {
          if (R_FAILED(svcAcceptSession(&handle, *srv_handle)))
          {
            svcBreak(USERBREAK_ASSERT);
          }
          if (g_active_handles < MAX_SESSIONS+2)
          {
            g_handles[g_active_handles] = handle;
            g_active_handles++;
          }
          else
          {
            svcCloseHandle(handle);
          }
          break;
        }
        default: // session
        {
          handle_commands();
          reply_target = g_handles[index];
          break;
        }
      }
    }
  } while (!term_request || g_active_handles != 2);

  srvSysUnregisterService("nfc:u");
  svcCloseHandle(*srv_handle);
  svcCloseHandle(*notification_handle);

  return 0;
}
