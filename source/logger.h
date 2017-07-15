 
#pragma once

static u64 NFC_ID = 0x4013000004002;

void openLogger();
void logstr(const char *str);
void logu64(u64 progId);
void closeLogger();
void panicstr(const char *str);

