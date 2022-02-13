#!/bin/bash

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}" )" && pwd )"

sudo apt purge picoscenes-plugins-demo-echoprobe-forwarder -y

cd $scriptDir && rm -rf $scriptDir/build && mkdir $scriptDir/build && cd $scriptDir/build
cmake .. && make package -j`nproc`

cd $scriptDir/build && sudo dpkg -i ./picoscenes*.deb
