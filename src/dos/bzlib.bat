if "%DJDIR%" == "" goto error

set CFLAGS=-march=i386
set CFLAGS=%CFLAGS% -O3 -flto -fwhole-program -fomit-frame-pointer

@set GLOBOBJS=
@set GLOBOBJS=%GLOBOBJS% ../zlib/adler32.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/infblock.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/infcodes.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/inffast.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/inflate.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/inftrees.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/infutil.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/uncompr.c
@set GLOBOBJS=%GLOBOBJS% ../zlib/zutil.c

gcc %GLOBOBJS% %CFLAGS% -c
ar rcs libzlib.a adler32.o infblock.o infcodes.o inffast.o inflate.o inftrees.o infutil.o uncompr.o zutil.o

del *.o

goto end

:error
@echo Set the environment variables before running this script!

:end
