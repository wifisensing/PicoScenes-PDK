#!/bin/bash

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}" )" && pwd )"

sudo apt-get remove picoscenes-plugins-demo-rxsbroadcaster-chronos -y
sudo dpkg -P picoscenes-plugins-demo-rxsbroadcaster-chronos 2>/dev/null

cd $scriptDir && rm -rf $scriptDir/build && mkdir $scriptDir/build && cd $scriptDir/build
cmake .. && make package -j6

cd $scriptDir/build && sudo dpkg -i ./picoscenes*.deb
