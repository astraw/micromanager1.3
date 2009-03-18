
cd "\projects\micromanager1.3\"

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
