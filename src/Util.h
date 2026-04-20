#pragma once

char *FullPath(char *filename);
char *FullAudioPath(char *filename);
char *FullMoviePath(char *filename);

char *FullWritablePath(char *filename);

extern int g_MouseFlg;
extern int g_MouseActualFlg;
extern int g_MouseXDown;
extern int g_MouseYDown;
extern int g_MouseXCurrent;
extern int g_MouseYCurrent;

