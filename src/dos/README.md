# Moon Child DE
![Moon Child DE](readme_imgs/moonchld.png?raw=true)

Moon Child DE (DOS Edition) is a source port of Moon Child for
32-bit DOS computers with 640x480 32-bit color graphics and no sound.

## Controls:
|Action            |Keys      |
|------------------|----------|
|Move              |Arrow keys|
|Fire              |Space     |
|Break out of level|Esc       |
|Quit to DOS       |F10       |

## Building:
1) Install [DJGPP](https://github.com/andrewwutw/build-djgpp)

2) Run `setenvdj.bat` once to set the DJGPP environment variables.

3) Run `bzlib.bat` to build [zlib v1.1.4](https://github.com/madler/zlib/tree/v1.1.4) into `libzlib.a`

4) Run `bdj32.bat` to build `MOONCHLD.exe`
