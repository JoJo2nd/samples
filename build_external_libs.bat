@echo off

pushd %~dp0

call "%VS140COMNTOOLS%\VsMsBuildCmd.bat"

pushd "external/bgfx/.build/projects/vs2015"
MSBuild /t:Build /p:Configuration=Debug /p:Platform=x64 /v:m /nologo /m bgfx.sln
MSBuild /t:Build /p:Configuration=Release /p:Platform=x64 /v:m /nologo /m bgfx.sln
popd

popd