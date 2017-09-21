
@echo off

pushd %~dp0
set DATASRC=./assets
set DATADEST=./data
set TEXTUREC=../../external/bgfx/.build/win64_vs2015/bin/texturecRelease
set GEOMETRYC=../../external/bgfx/.build/win64_vs2015/bin/geometrycRelease
set SHADERC=../../external/bgfx/.build/win64_vs2015/bin/shadercRelease
set SHADER_INC=-i ../../external/bgfx/src
set DEBUG_SHADERS=--debug

if not exist "%DATADEST%" mkdir "%DATADEST%"

del /Q "%DATADEST%\*.mesh"

call "%GEOMETRYC%" --tangent -f "%DATASRC%/mesh/bunny.obj" -o "%DATADEST%/bunny.mesh"
call "%GEOMETRYC%" --tangent -f "%DATASRC%/mesh/orb.obj" -o "%DATADEST%/orb.mesh"
call "%GEOMETRYC%" --tangent -f "%DATASRC%/sponza/sponza_pbr.obj" -o "%DATADEST%/sponza_pbr.mesh"
copy "%DATASRC%/sponza/sponza.mat" "%DATADEST%/sponza.mat"

popd

