@echo off

for %%f in (src\zbin\*.cpp) do "external\LLVM\clang-format" -i -style=file %%f
for %%f in (src\zbin\*.h) do "external\LLVM\clang-format" -i -style=file %%f
for %%f in (src\example\*.cpp) do "external\LLVM\clang-format" -i -style=file %%f
for %%f in (src\example\*.h) do "external\LLVM\clang-format" -i -style=file %%f
del /Q src\zbin\*.TMP
del /Q src\example\*.TMP