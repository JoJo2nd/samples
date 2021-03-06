@echo off
rem builds data for this sample

pushd %~dp0
set DATASRC=./assets
set DATADEST=./data
set TEXTUREC=../../external/bgfx/.build/win64_vs2015/bin/texturecRelease
set GEOMETRYC=../../external/bgfx/.build/win64_vs2015/bin/geometrycRelease
set SHADERC=../../external/bgfx/.build/win64_vs2015/bin/shadercRelease
set SHADER_INC=-i ../../external/bgfx/src
set DEBUG_SHADERS=--debug

if not exist "%DATADEST%" mkdir "%DATADEST%"

del /Q "%DATADEST%\*.*"

call "%GEOMETRYC%" -f "%DATASRC%/mesh/bunny.obj" -o "%DATADEST%/bunny.mesh"
call "%GEOMETRYC%" -f "%DATASRC%/mesh/orb.obj" -o "%DATADEST%/orb.mesh"
call "%GEOMETRYC%" -f "../../data/assets/meshes/sponza_pbr.obj" -o "%DATADEST%/sponza_pbr.mesh"

call "%SHADERC%" -f "%DATASRC%/shaders/vs.sc" %SHADER_INC% --type vertex -o "%DATADEST%/simple.vs" -p vs_5_0 --platform windows %DEBUG_SHADERS%
call "%SHADERC%" -f "%DATASRC%/shaders/vs_postex.sc" %SHADER_INC% --type vertex -o "%DATADEST%/postex.vs" -p vs_5_0 --platform windows %DEBUG_SHADERS%

call "%SHADERC%" -f "%DATASRC%/shaders/ps.sc" %SHADER_INC% --type fragment -o "%DATADEST%/simple.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS%
call "%SHADERC%" -f "%DATASRC%/shaders/ps.sc" %SHADER_INC% --type fragment -o "%DATADEST%/simple_wcs.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS% --define CS_LIGHT_CULL=1
call "%SHADERC%" -f "%DATASRC%/shaders/ps.sc" %SHADER_INC% --type fragment -o "%DATADEST%/debug01.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS% --define DEBUG_LIGHT_EVAL=1
call "%SHADERC%" -f "%DATASRC%/shaders/ps.sc" %SHADER_INC% --type fragment -o "%DATADEST%/debug02.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS% --define DEBUG_LIGHT_EVAL=1;DEBUG_NO_BINS=1

call "%SHADERC%" -f "%DATASRC%/shaders/ps_debug.sc" %SHADER_INC% --type fragment -o "%DATADEST%/fs_debug_wocs.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS% --define CS_LIGHT_CULL=0
call "%SHADERC%" -f "%DATASRC%/shaders/ps_debug.sc" %SHADER_INC% --type fragment -o "%DATADEST%/fs_debug_wcs.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS% --define CS_LIGHT_CULL=1

call "%SHADERC%" -f "%DATASRC%/shaders/light_grid.cc" %SHADER_INC% --type compute -o "%DATADEST%/light_grid.cs" -p cs_5_0 --platform windows %DEBUG_SHADERS%
call "%SHADERC%" -f "%DATASRC%/shaders/light_grid.cc" %SHADER_INC% --type compute -o "%DATADEST%/light_grid_o.cs" -p cs_5_0 --platform windows %DEBUG_SHADERS% --define CS_MULTI=1

call "%SHADERC%" -f "%DATASRC%/shaders/vs_zfill.sc" %SHADER_INC% --type vertex -o "%DATADEST%/zfill.vs" -p vs_5_0 --platform windows %DEBUG_SHADERS%
call "%SHADERC%" -f "%DATASRC%/shaders/ps_zfill.sc" %SHADER_INC% --type fragment -o "%DATADEST%/zfill.ps" -p ps_5_0 --platform windows %DEBUG_SHADERS%

popd
