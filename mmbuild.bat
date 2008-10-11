ECHO building native libraries...
cd "C:\Program Files\Microsoft Visual Studio 9.0\VC\"
call vcvarsall.bat
 
cd "\projects\micromanager1.3\"
devenv /REBUILD Release .\MMCoreJ_wrap\MMCoreJ_wrap.sln

ECHO Building Java components...
cd mmStudio\src
call \projects\3rdparty\apache-ant-1.6.5\bin\ant -buildfile ..\build.xml cleanMMStudio compileMMStudio buildMMStudio buildMMReader install packInstaller

ECHO "Done"
cd "..\.."