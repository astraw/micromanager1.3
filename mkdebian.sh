#portable files + binary (almost none portable)
ROOTPORT=../rootport
rm -Rf $ROOTPORT
mkdir $ROOTPORT

ROOTBIN=$ROOTPORT

#micromanager-imagej
ROOTIJ=../rootij
rm -Rf $ROOTIJ
mkdir $ROOTIJ

##### Meta information
mkdir $ROOTPORT/DEBIAN
mkdir $ROOTIJ/DEBIAN
cp doc/copyright.txt $ROOTPORT/DEBIAN/copyright
cp doc/copyright.txt $ROOTIJ/DEBIAN/copyright
cp debiancontrol.port $ROOTPORT/DEBIAN/control
cp debiancontrol.ij $ROOTIJ/DEBIAN/control
echo "#!/bin/sh" > $ROOTBIN/DEBIAN/postinst
chmod 0555 $ROOTBIN/DEBIAN/postinst

##### Programs
mkdir -p $ROOTBIN/usr/bin/
cp Test_Serial/mm_testserial ModuleTest/mm_moduletest Test_MMCore/mm_testCore $ROOTBIN/usr/bin/
strip $ROOTBIN/usr/bin/*

##### JAR-files
mkdir -p $ROOTIJ/usr/share/imagej/plugins/Micro-Manager/
cp Bleach/MMBleach_.jar autofocus/MMAutofocus_.jar autofocus/MMAutofocusTB_.jar MMCoreJ_wrap/MMCoreJ.jar Tracking/Tracker_.jar \
mmstudio/MMJ_.jar $ROOTIJ/usr/share/imagej/plugins/Micro-Manager/

mkdir -p $ROOTPORT/usr/share/java/
cp MMCoreJ_wrap/MMCoreJ.jar $ROOTPORT/usr/share/java/
	#####classext/bsh-2.0b4.jar classext/syntax.jar classext/ij.jar #assume these are available

##### Core
mkdir -p $ROOTBIN/usr/lib/micro-manager/

cp MMCoreJ_wrap/.libs/libMMCoreJ_wrap.so $ROOTBIN/usr/lib/micro-manager/

##### All plugins
for from in `ls DeviceAdapters/*/.libs/*.so`
	do
		to=${from##*/}
		to=$ROOTBIN/usr/lib/micro-manager/libmmgr_dal_`echo $to|sed 's/\..\{2\}$//'`
		to=$to.so
		cp $from $to
	done
strip $ROOTBIN/usr/lib/micro-manager/*

##### Make shared objects visible without hardcoding the path
mkdir -p $ROOTPORT/etc/ld.so.conf.d/
echo "/usr/lib/micro-manager" > $ROOTPORT/etc/ld.so.conf.d/micro-manager.conf
echo "/sbin/ldconfig" >> $ROOTBIN/DEBIAN/postinst

##### Put together
cd ..
dpkg-deb -b rootport
dpkg-deb -b rootij
mv rootport.deb micromanager.deb
mv rootij.deb micromanager-ij.deb
