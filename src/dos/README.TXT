Moon Child DE (DOS Edition) 
https://github.com/FrenkelS/MoonChildDE/tree/main/src/dos

Moon Child DE (DOS Edition) is a source port of Moon Child for
32-bit DOS computers with 640x480/320x240/320x200 256 color graphics and no sound.

Controls:
|Action            |Keys      |
|------------------|----------|
|Move              |Arrow keys|
|Fire              |Space     |
|Break out of level|Esc       |
|Quit to DOS       |F10       |

Command line arguments:
When no command line arguments are used, the game runs in 640x480 256 color mode
at atmost 60 frames per second.
Use -nolfb/-modex/-mode13h to run in a different graphics mode.
Use -notimer to run as fast as possible.

|Command line argument|Effect                                                    |
|---------------------|----------------------------------------------------------|
|-nolfb               |Disable linear frame buffer -> use bank switching         |
|-modex320x480        |Use Mode X   320x480                                      |
|-modex               |Use Mode X   320x240                                      |
|-mode13h             |Use Mode 13h 320x200                                      |
|-notimer             |Disable timer -> run as fast as possible                  |
|-timedemo            |Run benchmark. This disables the timer and keyboard input.|
