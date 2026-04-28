if "%DJDIR%" == "" goto error

mkdir release

set CFLAGS=-march=i386
set CFLAGS=%CFLAGS% -Ofast -fomit-frame-pointer -flto -fwhole-program -Wno-attributes -fno-exceptions

@set GLOBOBJS=
@set GLOBOBJS=%GLOBOBJS% dosAudio.cpp
@set GLOBOBJS=%GLOBOBJS% dosGame.cpp
@set GLOBOBJS=%GLOBOBJS% dosMain.cpp
@set GLOBOBJS=%GLOBOBJS% dosMoviePlayer.cpp
@set GLOBOBJS=%GLOBOBJS% dosUtil.cpp

@set GLOBOBJS=%GLOBOBJS% ../framewrk/audio.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/blitbuf.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/fastfile.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/movie.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/pcxff.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/sprite.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/timer.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/video.cpp
@set GLOBOBJS=%GLOBOBJS% ../framewrk/windows.cpp

@set GLOBOBJS=%GLOBOBJS% ../moonchild/anim.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/asset.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/asteroid.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/basic.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bat.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bbot.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bee.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bolt.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bonus.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/boss.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bouncey.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bullet.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/bump.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/cannon.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/chain.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/claw.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/cloud.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/diamond.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/doldoler.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/editor.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/element.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/elevat.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/glim.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/globals.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/goodday.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/gravlift.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/gumbal.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/hoi.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/ironauto.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/ironring.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/levinits.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/lift.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/mc.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/metbal.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/metcan.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/mine.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/mixer.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/mouth.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/mushroom.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/objects.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/paal.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/plant.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/plof.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/prefs.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/ptoei.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/punt.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/robyn.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/rock.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/rocket.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/score.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/scroll.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/seamine.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/smlheart.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/snake.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/sneak.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/sokoban.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/sound.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/spies.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/spike.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/stukhout.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/switsj.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/test.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/trigger.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/vgdll.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/warp.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/weight.cpp
@set GLOBOBJS=%GLOBOBJS% ../moonchild/wheel.cpp

g++ %GLOBOBJS% %CFLAGS% -I.. -I../framewrk -I../moonchild -D__cdecl= -Wno-write-strings -L. -lzlib -o release/MOONCHLD.EXE
strip -s release/MOONCHLD.EXE
stubedit release/MOONCHLD.EXE dpmi=CWSDPR0.EXE

goto end

:error
@echo Set the environment variables before running this script!

:end
