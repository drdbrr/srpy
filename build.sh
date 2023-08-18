#!/bin/bash
#clear && cd ./build && rm -rf * &&
clear && cd ./build && cmake -DPython_EXECUTABLE=$(python3 -c "import sys; print(sys.executable)") .. && cmake  --build . -j 6
