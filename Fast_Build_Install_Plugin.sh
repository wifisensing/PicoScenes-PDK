#!/bin/bash

if [ $(uname) = Linux ]; then
    scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}" )" && pwd )"
    ncpu=$(nproc)
    sudo apt purge picoscenes-plugins-demo-echoprobe-forwarder -y
elif [ $(uname) = Darwin ]; then
    scriptDir=$(cd "$(dirname "$0")"; pwd)
    ncpu=$(sudo sysctl -n hw.ncpu) # use sudo to acquire privilege earlier
fi

if [ ! -d $scriptDir/build ]; then
    mkdir $scriptDir/build
else
    rm -rf $scriptDir/build/*.deb 2>/dev/null
    rm -rf $scriptDir/build/*.pkg 2>/dev/null
fi

cd $scriptDir/build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target all -j$ncpu
cpack

if [ $(uname) = Linux ]; then
    cd $scriptDir/build && sudo dpkg -i ./picoscenes*.deb
elif [ $(uname) = Darwin ]; then
    cd $scriptDir/build && sudo installer -pkg ./picoscenes*.pkg -target /
fi