@echo off
rem builds data for this sample

pushd %~dp0
set DATASRC=./assets
set DATADEST=./data
set TEXTUREC=../../external/bgfx/.build/win64_vs2015/bin/texturecRelease

del /Q "%DATADEST%\*.ktx"

REM source material - leaf
echo Building sponza\textures_pbr\Sponza_Thorn_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Thorn_diffuse.tga" -o "%DATADEST%\Sponza_Thorn_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Thorn_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Thorn_normal.tga" -o "%DATADEST%\Sponza_Thorn_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Thorn_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Thorn_roughness.tga" -o "%DATADEST%\Sponza_Thorn_roughness.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\sponza_thorn_mask.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\sponza_thorn_mask.tga" -o "%DATADEST%\sponza_thorn_mask.ktx" -t BC4 --as ktx
REM source material - vase_round
echo Building sponza\textures_pbr\VaseRound_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VaseRound_diffuse.tga" -o "%DATADEST%\VaseRound_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\VaseRound_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VaseRound_normal.tga" -o "%DATADEST%\VaseRound_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\VaseRound_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VaseRound_roughness.tga" -o "%DATADEST%\VaseRound_roughness.ktx" -t BC3 -m --as ktx
REM source material - Material__57
echo Building sponza\textures_pbr\VasePlant_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VasePlant_diffuse.tga" -o "%DATADEST%\VasePlant_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\VasePlant_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VasePlant_normal.tga" -o "%DATADEST%\VasePlant_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\VasePlant_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VasePlant_roughness.tga" -o "%DATADEST%\VasePlant_roughness.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\vase_plant_mask.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\vase_plant_mask.tga" -o "%DATADEST%\vase_plant_mask.ktx" -t BC4 --as ktx
REM source material - Material__298
echo Building sponza\textures_pbr\Background_Albedo.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Background_Albedo.tga" -o "%DATADEST%\Background_Albedo.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Background_Normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Background_Normal.tga" -o "%DATADEST%\Background_Normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Background_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Background_roughness.tga" -o "%DATADEST%\Background_roughness.ktx" -t BC3 -m --as ktx
REM source material - bricks
echo Building sponza\textures_pbr\Sponza_Bricks_a_Albedo.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Bricks_a_Albedo.tga" -o "%DATADEST%\Sponza_Bricks_a_Albedo.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Bricks_a_Normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Bricks_a_Normal.tga" -o "%DATADEST%\Sponza_Bricks_a_Normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Bricks_a_Roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Bricks_a_Roughness.tga" -o "%DATADEST%\Sponza_Bricks_a_Roughness.ktx" -t BC3 -m --as ktx
REM source material - arch
echo Building sponza\textures_pbr\Sponza_Arch_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Arch_diffuse.tga" -o "%DATADEST%\Sponza_Arch_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Arch_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Arch_normal.tga" -o "%DATADEST%\Sponza_Arch_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Arch_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Arch_roughness.tga" -o "%DATADEST%\Sponza_Arch_roughness.ktx" -t BC3 -m --as ktx
REM source material - ceiling
echo Building sponza\textures_pbr\Sponza_Ceiling_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Ceiling_diffuse.tga" -o "%DATADEST%\Sponza_Ceiling_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Ceiling_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Ceiling_normal.tga" -o "%DATADEST%\Sponza_Ceiling_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Ceiling_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Ceiling_roughness.tga" -o "%DATADEST%\Sponza_Ceiling_roughness.ktx" -t BC3 -m --as ktx
REM source material - column_a
echo Building sponza\textures_pbr\Sponza_Column_a_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_a_diffuse.tga" -o "%DATADEST%\Sponza_Column_a_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Column_a_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_a_normal.tga" -o "%DATADEST%\Sponza_Column_a_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Column_a_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_a_roughness.tga" -o "%DATADEST%\Sponza_Column_a_roughness.ktx" -t BC3 -m --as ktx
REM source material - floor
echo Building sponza\textures_pbr\Sponza_Floor_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Floor_diffuse.tga" -o "%DATADEST%\Sponza_Floor_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Floor_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Floor_normal.tga" -o "%DATADEST%\Sponza_Floor_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Floor_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Floor_roughness.tga" -o "%DATADEST%\Sponza_Floor_roughness.ktx" -t BC3 -m --as ktx
REM source material - column_c
echo Building sponza\textures_pbr\Sponza_Column_c_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_c_diffuse.tga" -o "%DATADEST%\Sponza_Column_c_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Column_c_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_c_normal.tga" -o "%DATADEST%\Sponza_Column_c_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Column_c_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_c_roughness.tga" -o "%DATADEST%\Sponza_Column_c_roughness.ktx" -t BC3 -m --as ktx
REM source material - details
echo Building sponza\textures_pbr\Sponza_Details_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Details_diffuse.tga" -o "%DATADEST%\Sponza_Details_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Details_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Details_normal.tga" -o "%DATADEST%\Sponza_Details_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Details_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Details_roughness.tga" -o "%DATADEST%\Sponza_Details_roughness.ktx" -t BC3 -m --as ktx
REM source material - column_b
echo Building sponza\textures_pbr\Sponza_Column_b_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_b_diffuse.tga" -o "%DATADEST%\Sponza_Column_b_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Column_b_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_b_normal.tga" -o "%DATADEST%\Sponza_Column_b_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Column_b_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Column_b_roughness.tga" -o "%DATADEST%\Sponza_Column_b_roughness.ktx" -t BC3 -m --as ktx
REM source material - flagpole
echo Building sponza\textures_pbr\Sponza_FlagPole_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_FlagPole_diffuse.tga" -o "%DATADEST%\Sponza_FlagPole_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_FlagPole_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_FlagPole_normal.tga" -o "%DATADEST%\Sponza_FlagPole_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Metallic_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Metallic_metallic.tga" -o "%DATADEST%\Metallic_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_FlagPole_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_FlagPole_roughness.tga" -o "%DATADEST%\Sponza_FlagPole_roughness.ktx" -t BC3 -m --as ktx
REM source material - fabric_e
echo Building sponza\textures_pbr\Sponza_Fabric_Green_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_Green_diffuse.tga" -o "%DATADEST%\Sponza_Fabric_Green_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_Green_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_Green_normal.tga" -o "%DATADEST%\Sponza_Fabric_Green_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_metallic.tga" -o "%DATADEST%\Sponza_Fabric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_roughness.tga" -o "%DATADEST%\Sponza_Fabric_roughness.ktx" -t BC3 -m --as ktx
REM source material - fabric_d
echo Building sponza\textures_pbr\Sponza_Fabric_Blue_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_Blue_diffuse.tga" -o "%DATADEST%\Sponza_Fabric_Blue_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_Blue_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_Blue_normal.tga" -o "%DATADEST%\Sponza_Fabric_Blue_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_metallic.tga" -o "%DATADEST%\Sponza_Fabric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_roughness.tga" -o "%DATADEST%\Sponza_Fabric_roughness.ktx" -t BC3 -m --as ktx
REM source material - fabric_a
echo Building sponza\textures_pbr\Sponza_Fabric_Red_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_Red_diffuse.tga" -o "%DATADEST%\Sponza_Fabric_Red_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_Red_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_Red_normal.tga" -o "%DATADEST%\Sponza_Fabric_Red_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_metallic.tga" -o "%DATADEST%\Sponza_Fabric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Fabric_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Fabric_roughness.tga" -o "%DATADEST%\Sponza_Fabric_roughness.ktx" -t BC3 -m --as ktx
REM source material - fabric_g
echo Building sponza\textures_pbr\Sponza_Curtain_Blue_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_Blue_diffuse.tga" -o "%DATADEST%\Sponza_Curtain_Blue_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_Blue_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_Blue_normal.tga" -o "%DATADEST%\Sponza_Curtain_Blue_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_metallic.tga" -o "%DATADEST%\Sponza_Curtain_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_roughness.tga" -o "%DATADEST%\Sponza_Curtain_roughness.ktx" -t BC3 -m --as ktx
REM source material - fabric_c
echo Building sponza\textures_pbr\Sponza_Curtain_Red_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_Red_diffuse.tga" -o "%DATADEST%\Sponza_Curtain_Red_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_Red_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_Red_normal.tga" -o "%DATADEST%\Sponza_Curtain_Red_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_metallic.tga" -o "%DATADEST%\Sponza_Curtain_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_roughness.tga" -o "%DATADEST%\Sponza_Curtain_roughness.ktx" -t BC3 -m --as ktx
REM source material - fabric_f
echo Building sponza\textures_pbr\Sponza_Curtain_Green_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_Green_diffuse.tga" -o "%DATADEST%\Sponza_Curtain_Green_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_Green_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_Green_normal.tga" -o "%DATADEST%\Sponza_Curtain_Green_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_metallic.tga" -o "%DATADEST%\Sponza_Curtain_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Sponza_Curtain_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Sponza_Curtain_roughness.tga" -o "%DATADEST%\Sponza_Curtain_roughness.ktx" -t BC3 -m --as ktx
REM source material - chain
echo Building sponza\textures_pbr\ChainTexture_Albedo.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\ChainTexture_Albedo.tga" -o "%DATADEST%\ChainTexture_Albedo.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\ChainTexture_Normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\ChainTexture_Normal.tga" -o "%DATADEST%\ChainTexture_Normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\ChainTexture_Metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\ChainTexture_Metallic.tga" -o "%DATADEST%\ChainTexture_Metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\ChainTexture_Roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\ChainTexture_Roughness.tga" -o "%DATADEST%\ChainTexture_Roughness.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\chain_texture_mask.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\chain_texture_mask.tga" -o "%DATADEST%\chain_texture_mask.ktx" -t BC4 --as ktx
REM source material - vase_hanging
echo Building sponza\textures_pbr\VaseHanging_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VaseHanging_diffuse.tga" -o "%DATADEST%\VaseHanging_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\VaseHanging_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VaseHanging_normal.tga" -o "%DATADEST%\VaseHanging_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Metallic_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Metallic_metallic.tga" -o "%DATADEST%\Metallic_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\VaseHanging_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\VaseHanging_roughness.tga" -o "%DATADEST%\VaseHanging_roughness.ktx" -t BC3 -m --as ktx
REM source material - vase
echo Building sponza\textures_pbr\Vase_diffuse.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Vase_diffuse.tga" -o "%DATADEST%\Vase_diffuse.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Vase_normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Vase_normal.tga" -o "%DATADEST%\Vase_normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Vase_roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Vase_roughness.tga" -o "%DATADEST%\Vase_roughness.ktx" -t BC3 -m --as ktx
REM source material - Material__25
echo Building sponza\textures_pbr\Lion_Albedo.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Lion_Albedo.tga" -o "%DATADEST%\Lion_Albedo.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Lion_Normal.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Lion_Normal.tga" -o "%DATADEST%\Lion_Normal.ktx" -t BC5 --normalmap -m --as ktx
echo Building sponza\textures_pbr\Dielectric_metallic.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Dielectric_metallic.tga" -o "%DATADEST%\Dielectric_metallic.ktx" -t BC3 -m --as ktx
echo Building sponza\textures_pbr\Lion_Roughness.tga
call "%TEXTUREC%" -f "%DATASRC%\sponza\textures_pbr\Lion_Roughness.tga" -o "%DATADEST%\Lion_Roughness.ktx" -t BC3 -m --as ktx

popd
