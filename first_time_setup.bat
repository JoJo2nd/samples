@echo off

pushd %~dp0

call "%VS140COMNTOOLS%\VsMsBuildCmd.bat"

REM setup and build BGFX

if exist "external/bgfx/.build" rmdir /Q /S "external/bgfx/.build"

pushd "external/bx"
git checkout .
git clean -f
git apply ../bx.01.patch
popd

pushd "external/bgfx"
git checkout .
git clean -f
git apply ../bgfx.01.patch

..\bx\tools\bin\windows\genie --with-tools vs2015
REM ..\bx\tools\bin\windows\genie --with-examples --with-tools vs2015
popd

pushd "external/bgfx/.build/projects/vs2015"
MSBuild /t:Build /p:Configuration=Debug /p:Platform=x64 /v:m /nologo /m bgfx.sln
MSBuild /t:Build /p:Configuration=Release /p:Platform=x64 /v:m /nologo /m bgfx.sln
popd

REM build the real project
if exist build rmdir /Q /S build
mkdir build
pushd build
cmake -G "Visual Studio 14 2015 Win64" ..
cmake --build .
popd

popd