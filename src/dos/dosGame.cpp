#include "Game.h"
#include "Util.h"
#include "framewrk/frm_int.hpp"
#include "moonchild/mc.hpp"
#include "moonchild/globals.hpp"
#include "moonchild/prefs.hpp"

int g_MouseFlg = 0;
int g_MouseActualFlg = 0;
int g_MouseXDown = 0;
int g_MouseYDown = 0;
int g_MouseXCurrent = 0;
int g_MouseYCurrent = 0;

namespace {

int bytesPerPixel = 4;

void setPixelRGBA(uint8_t *base, int pitch, int x, int y, uint8_t r, uint8_t g,
                  uint8_t b, uint8_t a) {
  uint8_t *row = base + y * pitch;
  uint8_t *p = row + x * bytesPerPixel;
  p[0] = r;
  p[1] = g;
  p[2] = b;
  p[3] = a;
}

void fillTestPattern(uint8_t *pixels, int width, int height, int pitch) {
  int cell = 40;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int cx = x / cell;
      int cy = y / cell;
      bool redCell = ((cx + cy) & 1) == 0;
      if (redCell) {
        setPixelRGBA(pixels, pitch, x, y, 255, 0, 0, 255);
      } else {
        setPixelRGBA(pixels, pitch, x, y, 0, 0, 255, 255);
      }
    }
  }
  int box = 32;
  for (int y = 0; y < box && y < height; y++) {
    for (int x = 0; x < box && x < width; x++) {
      setPixelRGBA(pixels, pitch, x, y, 255, 255, 255, 255);
    }
  }
}

}  // namespace

void initMoonChild(unsigned char *pixelBuffer, int width, int height, MoviePlayer *moviePlayer) {
	video = new Cvideo();
	audio = new Caudio();  // create audio AFTER window is created!
	timer = new Ctimer();  // Create timer facilities
	movie = new Cmovie(audio, moviePlayer);  // Initiate movie playback features
	
	g_SettingsFlg = 0;	//we starten met het settings window off 
	gbGameLoop = 1;
	
	heartbeat = NULL;
    
    if ( !video->on(pixelBuffer, width, height, 256) )
    {
        //EXIT!
        return;
    }
    
    heartbeat = framework_InitGame(video, audio, timer, movie);
    
    if (heartbeat == NULL)
    {
        //EXIT!
        return;
    }
  }
   
  // if someone wants to reset progress (aka start over). This is how to do it
  void resetProgress() {
    //reset code
    maxlevel = 0;
    
    for(int i=0; i<13; i++)
    {
        blacksperlevel[i] = 0;
        scoreblacksperlevel[i] = 0;
    }
  }
 
  // if someone wants to enable the cheat. This is how to do it
  void enableCheat(){
    //cheat code
    maxlevel = 12;
    
    for(int i=0; i<13; i++)
    {
        blacksperlevel[i] = 0;
        scoreblacksperlevel[i] = 0;
    }
}

void gameTick(uint8_t *pixels, int width, int height, int pitch, uint8_t *keyboardState) {
  (void)keyboardState;
//  fillTestPattern(pixels, width, height, pitch);

    //if(showdeadsequence)
    {
        // gameOverPic.hidden = NO;
        // if(touchView.mousePressing)
        // {
        //     touchView.mousePressing = 0;
        //     showdeadsequence = 0;
        //     gameOverPic.hidden = YES;
        //     return;
        // }
    }
    
    audio->checkVolume();
    
    
    if(g_SettingsFlg)	//settings screen op het beeld?
    {
        //				unsigned char buf[4] = {1,2,3,0};
        //				int getal;
        //				getal = g_KeyConfig.LeftKey;
        //				buf[2] = getal/100;
        //				getal = getal - ((getal/100)*100);
        //				buf[1] = getal/10;
        //				buf[0] = getal%10;
        video->DrawSettings();
        //				lvideo->DisplayChars2(buf, 5, 5);
        
    }
    else
    {
//        lmovie->movieplay();            // if movie is playing this routine will handle frame advancement
        if (heartbeat != NULL)
        {
            heartbeat = (HEARTBEAT_FN) heartbeat();
            if(heartbeat == NULL)  // No heartbeat anymore, Let's close
            {
                //exit the game
            }
            
            
        }
    }
}
