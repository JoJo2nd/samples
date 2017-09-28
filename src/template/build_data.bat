@echo off
rem builds data for this sample

pushd %~dp0
set DATASRC=../../data/assets
set DATADEST=data
set TEXTUREC=../../external/bgfx/.build/win64_vs2015/bin/texturecRelease
set GEOMETRYC=../../external/bgfx/.build/win64_vs2015/bin/geometrycRelease
set SHADERC=../../external/bgfx/.build/win64_vs2015/bin/shadercRelease
set SHADER_INC=-i ../../external/bgfx/src
rem set DEBUG_SHADERS=--debug

if not exist "%DATADEST%" mkdir "%DATADEST%"

call "%GEOMETRYC%" -f "%DATASRC%/meshes/lostempire/lost_empire.obj" -o "%DATADEST%/lostempire.mesh"
call "%SHADERC%" -f "%DATASRC%/shaders/vs_simple.sc" %SHADER_INC% --type vertex -o "%DATADEST%/simple.vs" -p vs_5_0 --platform windows %DEBUG_SHADERS%
call "%SHADERC%" -f "%DATASRC%/shaders/ps_simple.sc" %SHADER_INC% --type fragment -o "%DATADEST%/simple.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS%

popd
