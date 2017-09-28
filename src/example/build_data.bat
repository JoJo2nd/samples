@echo off
rem builds data for this sample

pushd %~dp0
set DATASRC=./assets
set DATADEST=./data
set TEXTUREC=../../external/bgfx/.build/win64_vs2015/bin/texturecRelease
set GEOMETRYC=../../external/bgfx/.build/win64_vs2015/bin/geometrycRelease
set SHADERC=../../external/bgfx/.build/win64_vs2015/bin/shadercRelease

if not exist "%DATADEST%" mkdir "%DATADEST%"

if not exist "%DATADEST%/env" mkdir "%DATADEST%/env"

echo "Make sure this folder exists" > "%DATADEST%/env/.keepme"

call build_meshes.bat

call build_shaders.bat

call build_textures.bat

popd
