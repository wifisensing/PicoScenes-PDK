#!/bin/bash

if [[ $# == 0 ]]; then
  targetGitDir="$(cd "$(dirname "${BASH_SOURCE[0]}" )" && pwd )"
else
  targetGitDir=$1
fi
currentDir="$(pwd)"

cd $targetGitDir
git_timestamp=$(git log -1 --pretty=format:"%ad" --date=format:%Y.%m%d.%H%M)
cd $currentDir # cd back to current dir to prevent pwd change
echo $git_timestamp
