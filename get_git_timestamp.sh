#!/bin/bash
scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}" )" && pwd )"
currentDir="$(pwd)"

cd $scriptDir
git_timestamp=$(git log -1 --pretty=format:"%ad" --date=format:%y.%m%d.%H%M)
cd $currentDir # cd back to current dir to prevent pwd change
echo $git_timestamp
