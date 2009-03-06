ECHO building native libraries...
cd "C:\Program Files\Microsoft Visual Studio 9.0\VC\"
call vcvarsall.bat
 
cd "\projects\micromanager1.3\"
devenv /REBUILD Release .\MMCoreJ_wrap\MMCoreJ_wrap.sln

ECHO Building Java components...

cd autofocus
call \projects\3rdparty\apache-ant-1.6.5\bin\ant -buildfile build.xml cleanAutofocus compileAutofocus buildAutofocus 
cd ..


cd plugins\Tracker 
call \projects\3rdparty\apache-ant-1.6.5\bin\ant -buildfile build.xml cleanMMTracking compileMMTracking buildMMTracking 
cd ..\..

cd mmStudio\src
call \projects\3rdparty\apache-ant-1.6.5\bin\ant -buildfile ..\build.xml cleanMMStudio compileMMStudio buildMMStudio buildMMReader install packInstaller

ECHO "Done"
cd "..\.."