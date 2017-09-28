@echo off

pushd %~dp0
for %%f in (src\zbin\*.cpp) do "external\LLVM\clang-format" -i -style=file %%f
for %%f in (src\zbin\*.h) do "external\LLVM\clang-format" -i -style=file %%f

for %%f in (src\example\*.cpp) do "external\LLVM\clang-format" -i -style=file %%f
for %%f in (src\example\*.h) do "external\LLVM\clang-format" -i -style=file %%f

for %%f in (src\common\*.cpp) do "external\LLVM\clang-format" -i -style=file %%f
for %%f in (src\common\*.h) do "external\LLVM\clang-format" -i -style=file %%f

del /Q src\zbin\*.TMP
del /Q src\example\*.TMP
del /Q src\common\*.TMP
popd
