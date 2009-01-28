## Process this file with automake to produce Makefile.in
MMPlugins_.jar:  \
	$(srcdir)/Tracker/src/com/imaging100x/tracker/TrackerControl.java
	test -d build || mkdir build
	rm -rf build/*
	$(JAVAC) -source 1.5 -target 1.5 -sourcepath $(srcdir):. -classpath $(IJJARPATH):$(BSHJARPATH):../bin/MMCoreJ.jar:../mmstudio/MMJ_.jar:. $(srcdir)/Tracker/src/com/imaging100x/tracker/*.java -d build
	## Work around bug in 'jar': jar cf MM_.jar -C bin/ bin/* does not work
	cd build && jar cf ../MMPlugins_.jar * 

all: MMPlugins_.jar

install: 
	test -d $(IJPATH)/plugins/Micro-Manager || mkdir $(IJPATH)/plugins/Micro-Manager
	cp MMPlugins_.jar $(IJPATH)/plugins/Micro-Manager

clean:
	rm MMPlugins_.jar 

EXTRA_DIST = src build.xml
