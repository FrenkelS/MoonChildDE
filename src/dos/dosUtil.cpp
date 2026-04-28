#include <cstdio>
#include "Util.h"
#include "framewrk/frm_int.hpp"

namespace {

char prefOrg[] = "moonchild";
char prefApp[] = "moonchild_shell";

}  // namespace


static const char *SDL_GetBasePath(void) {
  return NULL;
}


static void SDL_free(void *mem) {
}


// Called by the game to get the full path to a file
char *FullPath(char *filename) {
  static char buffer[4096];
  if (!filename) {
    return nullptr;
  }
  const char *base = SDL_GetBasePath();
  if (base) {
    snprintf(buffer, sizeof(buffer), "%sassets/moonchild/%s", base, filename);
    SDL_free((void*)base);
  } else {
    snprintf(buffer, sizeof(buffer), "assets/moonchld/%s", filename);
  }
  return buffer;
}

// Called by the game to get the full path to an audio file
char *FullAudioPath(char *filename) {
  static char buffer[4096];
  if (!filename) {
    return nullptr;
  }
  const char *base = SDL_GetBasePath();
  if (base) {
    snprintf(buffer, sizeof(buffer), "%sassets/audio/%s", base, filename);
    SDL_free((void*)base);
  } else {
    snprintf(buffer, sizeof(buffer), "assets/audio/%s", filename);
  }
  return buffer;
}

char *FullMoviePath(char *filename) {
  static char buffer[4096];
  if (!filename) {
    return nullptr;
  }
  const char *base = SDL_GetBasePath();
  if (base) {
    snprintf(buffer, sizeof(buffer), "%sassets/movies/%s", base, filename);
    SDL_free((void*)base);
  } else {
    snprintf(buffer, sizeof(buffer), "assets/movies/%s", filename);
  }
  return buffer;
}


static char *SDL_GetPrefPath(const char *org, const char *app) {
  return NULL;
}


// Called by the game to get the full path to a writable file (Only hiscore file)
char *FullWritablePath(char *filename) {
  static char buffer[4096];
  if (!filename) {
    return nullptr;
  }
  char *pref = SDL_GetPrefPath(prefOrg, prefApp);
  if (pref) {
    snprintf(buffer, sizeof(buffer), "%smoonchild/%s", pref, filename);
    SDL_free(pref);
  } else {
    snprintf(buffer, sizeof(buffer), "./moonchld/%s", filename);
  }
  return buffer;
}

// Internal method (only used here) to load a TGA file
typedef unsigned char BYTE;
unsigned short*LoadTGA(char *FileName)
{
	char logbuf[100];
	snprintf(logbuf, sizeof(logbuf), "loading: %s\n", FileName);
//	LOG(logbuf);
    
	// load targa file
	BYTE* tgabuff = new BYTE[20];
	bool OK = true;
  FILE *tga = fopen( FullPath(FileName), "rb" );
  if (!tga) return 0;
  fread(tgabuff, 20, 1, tga);
  fclose(tga);
	// gzFile tga = gzopen( FullPath(FileName), "rb" );
	// if (!tga) return 0; 
	// gzread(tga, tgabuff, 20);
	// gzclose( tga );
    
//	LOG("open succeeded\n");
    
	int TgaIDLen;
	int TgaCMapType;
	int TgaImgType;
	int TgaCMapOrig;
	int TgaCMapLen;
	int TgaCMapSize;
	int TgaXPos;
	int TgaYPos;
	int TgaWidth;
	int TgaHeight;
	int TgaPixSize;
	TgaIDLen		= *tgabuff;
	TgaCMapType	= *(tgabuff + 1);
	TgaImgType	= *(tgabuff + 2);
	TgaCMapOrig	= *(tgabuff + 3) + 256 * *(tgabuff + 4);
	TgaCMapLen	= *(tgabuff + 5) + 256 * *(tgabuff + 6);
	TgaCMapSize	= *(tgabuff + 7);
	TgaXPos		= *(tgabuff + 8) + 256 * *(tgabuff + 9);
	TgaYPos		= *(tgabuff + 10) + 256 * *(tgabuff + 11);
	TgaWidth	= *(tgabuff + 12) + 256 * *(tgabuff + 13);
	TgaHeight	= *(tgabuff + 14) + 256 * *(tgabuff + 15);
	TgaPixSize	= *(tgabuff + 16);
	delete [] tgabuff;
    
	int w,h;
	w = TgaWidth;
	h = TgaHeight;
	unsigned short *dest;
	int size = w * 4 * h + 20;
	tgabuff = new BYTE[size];
	dest = new unsigned short[w*h];  // hier komt uitgepakte plaatje
  
  // replace the gzip loading by normal loading
	// tga = gzopen( FullPath(FileName), "rb" );
	// if (!tga)
	// {
	// 	delete [] tgabuff;
	// 	delete [] dest;
	// 	return 0;
	// }
	// int read = gzread( tga, tgabuff, size );
	// gzclose( tga );
  tga = fopen( FullPath(FileName), "rb" );
	if (!tga)
	{
	 	delete [] tgabuff;
	 	delete [] dest;
	 	return 0;
	}
  fread(tgabuff, size, 1, tga);
  fclose(tga);
	
	if (TgaImgType == 1)
	{
		// Palettized image
		unsigned short* pal = new unsigned short[256];
		for ( int i = 0; i < 256; i++ )
		{
			int b = *(tgabuff + 18 + i * 3);
			int g = *(tgabuff + 18 + i * 3 + 1);
			int r = *(tgabuff + 18 + i * 3 + 2);
			pal[i] = (unsigned short)(((r >> 3) << 11) + ((g >> 2) << 5) + (b >> 3));
		}
		unsigned char* src = tgabuff + 18 + 768 + (h - 1) * w;
		unsigned short* dst = (unsigned short*)dest;
		for ( int y = 0; y < h; y++ )
		{
			for ( int x = 0; x < w; x++ )
			{
				int idx = *(src + x);
				*(dst + x) = pal[idx];
			}
			dst += w;
			src -= w;
		}
	}
	else
	{
		// Store the data at the specified target address
		unsigned char* src = (tgabuff + 18) + (((h - 1) * w)*4);
		unsigned short* dst = (unsigned short*)dest;
		for ( int i = 0; i < h; i++ )
		{
			for ( int x = 0; x < w; x++ )
			{
				int r,g,b,a;
				unsigned short rgba;
				b= *src++;
				g= *src++;
				r= *src++;
				a= *src++;
				rgba = ((r>>3)<<11)+((g>>2)<<5)+(b>>3);
				*(dst + x) = rgba; //*(src + x);
			}
			dst += w;
			src -= (w*8);
		}
	}
    
	delete [] tgabuff;
    
//	LOG("tga success\n");
	return dest;
}

// Called by the game to show a picture (tga)
void ShowPicture(char *FileName)
{
  unsigned short *TempPic;
	TempPic   = LoadTGA(FileName);
    
	video->DrawTempPic();
    
	delete [] TempPic;
}

