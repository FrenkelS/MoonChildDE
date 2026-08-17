# Moon Child DE
![Moon Child DE](readme_imgs/moonchld.png?raw=true)

Moon Child DE (DOS Edition) is a source port of Moon Child for
32-bit DOS computers with 640x480/320x480/320x240/320x200 256 color graphics and no sound.
Download Moon Child DE [here](https://github.com/FrenkelS/MoonChildDE/releases).

## Controls:
|Action            |Keys      |
|------------------|----------|
|Move              |Arrow keys|
|Fire              |Space     |
|Break out of level|Esc       |
|Quit to DOS       |F10       |

## Command line arguments:
|Command line argument|Effect                                                    |
|---------------------|----------------------------------------------------------|
|-nolfb               |Disable linear frame buffer -> use bank switching         |
|-modex320x480        |Use Mode X   320x480                                      |
|-modex               |Use Mode X   320x240                                      |
|-mode13h             |Use Mode 13h 320x200                                      |
|-notimer             |Disable timer -> run as fast as possible                  |
|-timedemo            |Run benchmark. This disables the timer and keyboard input.|

## Building:
1) Install [DJGPP](https://github.com/andrewwutw/build-djgpp)

2) Run `setenvdj.bat` to set the DJGPP environment variables.

3) Run `bzlib.bat` to build [zlib v1.1.4](https://github.com/madler/zlib/tree/v1.1.4) into `libzlib.a`

4) Run `bdj32.bat` to build `MOONCHLD.exe`
