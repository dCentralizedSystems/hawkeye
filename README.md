# Hawkeye

## Build dependencies
-cmake (3.11+)

## Building
``` git clone https://github.com/dCentralizedSystems/hawkeye.git ```
``` cd hawkeye ```
``` git submodule init ```
``` git submodule update ```
``` mkdir build && cd build ```
``` cmake ../ ```
``` make clean && make && sudo make install ```

## New command-line parameters
``` --apriltag-detect ``` 
Detects apriltags in the image (tag41h12 family only) and stores them in the COM segment of the output jpg file. This only works for yuv color images.

