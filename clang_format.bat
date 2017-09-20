@echo off

for %%f in (src\zbin\*.cpp) do "external\LLVM\clang-format" -i -style=file %%f
for %%f in (src\zbin\*.h) do "external\LLVM\clang-format" -i -style=file %%f
del /Q src\zbin\*.TMP