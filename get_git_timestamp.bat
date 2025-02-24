@echo off
setlocal

if "%~1"=="" (
  set "targetGitDir=%~dp0"
) else (
  set "targetGitDir=%~1"
)

set "currentDir=%cd%"

cd /d "%targetGitDir%"
for /f "tokens=*" %%i in ('git log -1 --pretty^=format:"%%ad" --date^=format:%%Y.%%m%%d.%%H%%M') do set "git_timestamp=%%i"
cd /d "%currentDir%"

echo %git_timestamp%

endlocal
