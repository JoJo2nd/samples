@echo off

call "%VS140COMNTOOLS%\VsMsBuildCmd.bat"

pushd "%1"
MSBuild /t:Build /p:Configuration=%3 /v:m /nologo /m %2
popd