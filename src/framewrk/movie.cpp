#include "frm_int.hpp"
#include "../MoviePlayer.h"
// videos of Moon Child were in a proprietary format called Smackplayer'. However that doesn't exist anymore so videos not supported.
// hover I leave this class here so you'll see the point where the movie get's called... It just doesn't play anything now

extern char *FullMoviePath( char *a_File );
bool movieFinishedNaturally = false;
bool movieDoneSignal = false;

void onMovieDone(bool naturalEnd, void *userData) {
  (void)userData;
  movieFinishedNaturally = naturalEnd;
  movieDoneSignal = true;
}

Cmovie::Cmovie(Caudio *audio, MoviePlayer *moviePlayer)
{
    this->videoFilename = NULL;
    this->moviePlayer = moviePlayer;
}


Cmovie::Cmovie( void )
{
    this->videoFilename = NULL;
    this->moviePlayer = nullptr;
}

Cmovie::~Cmovie(void)
{
}


Smack *Cmovie::open(char *filename)
{
    printf("trying to play movie: %s", filename);
    if(strcmp(filename, (char *)"intro.smk")==0)
    {
        this->videoFilename = "intro.mp4";
        this->videoReady = false;
        return (Smack *)1;
    }
    if(strcmp(filename, (char *)"bumper12.smk")==0)
    {
        this->videoFilename = "bumper12.mp4";
        this->videoReady = false;
        return (Smack *)1;
    }
    if(strcmp(filename, (char *)"bumper23.smk")==0)
    {
        this->videoFilename = "bumper23.mp4";
        this->videoReady = false;
        return (Smack *)1;
    }
    if(strcmp(filename, (char *)"bumper34.smk")==0)
    {
        this->videoFilename = "bumper34.mp4";
        this->videoReady = false;
        return (Smack *)1;
    }
    if(strcmp(filename, (char *)"extro.smk")==0)
    {
        this->videoFilename = "extro.mp4";
        this->videoReady = false;
        return (Smack *)1;
    }
    if(strcmp(filename, (char *)"gameover.smk")==0)
    {
        this->videoFilename = "gameover.mp4";
        this->videoReady = false;
        return (Smack *)1;
    }
    
    printf("Trying to play unknown moviefile!");
    
	return 0;
}


void   Cmovie::close(Smack *smk)
{
    movieDoneSignal = true;
    moviePlayer->stop(false);
}

void   Cmovie::playtovideo(Smack *smk, Cvideo *video, Cblitbuf *hulpbuf, UINT16 zoomfactor)
{
    movieDoneSignal = false;
    moviePlayer->playFile(FullMoviePath(this->videoFilename), onMovieDone, nullptr);
}

UINT16 Cmovie::stillplaying(void)
{
	return !movieDoneSignal;
}

void   Cmovie::movieplay(void)
{
}

void Cmovie::returnpal(BYTE *destpal)
{
}




void Cmovie::ClearBack(u32 flipafter)
{
}

void Cmovie::dopal(void)
{
}


void Cmovie::blitrect( u32 x, u32 y, u32 w, u32 h)
{
}

void Cmovie::DoPaint()
{
}


void Cmovie::mergeinterrect(whRECT* r1, whRECT* r2, whRECT* m, whRECT* i)
{
}

void Cmovie::mergerects(whRECT* r1, u32 r1num, whRECT* r2, u32 r2num, whRECT** o, u32* onum)
{
}


void Cmovie::DoAdvance( )
{
}


void Cmovie::InitPal( void )
{
}



