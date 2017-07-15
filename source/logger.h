#pragma once

void logInit();
void logStr(const char *str);
void logBuf(char *prefix, u8* data, size_t len);
void logExit();
void logCrash(const char *str);

