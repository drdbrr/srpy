#!/bin/bash
clear
if [ ! -d "./build" ]
then
    mkdir ./build
fi
cd ./build
cmake -DPython_EXECUTABLE=$(python3 -c "import sys; print(sys.executable)") ..
cmake  --build . -j 6
