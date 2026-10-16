You need:

 - devkitPro with devkitARM r63 or higher (older versions are untested)
 - ```3ds-zlib``` and ```3ds-mbedtls``` (version 2.16.6-1 is required for now) installed in portlibs of devkitPro  
   You can install it by opening devkitPro msys2 and typing ```pacman -S [package name]```.

Simply run ```make``` in a terminal in the root directory to build it.  

 - Building of dependency libraries(optional)  
   For ffmpeg, libbrotli, and libcurl, follow built.txt in each directory  
   For libctru, just type ```make``` in library\libctru\source\libctru  
