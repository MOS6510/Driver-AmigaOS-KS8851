#!/bin/sh
# Creates LHA distribution file.

# 1. Parameter: LHA archive file to create

DISTDIRNAME=A1200PlusNetworkDriver
DISTDIR=build/${DISTDIRNAME}
ARCH=020

echo "Build LHA distribution:"
mkdir -p ${DISTDIR}
cp -r DistributionTemplate/* ${DISTDIR}
cp KSZ8851/devicedriver/build/build-${ARCH}/ksz8851.device.${ARCH} ${DISTDIR}/network/
mv ${DISTDIR}/Folder.info ${DISTDIR}.info 

cd ${DISTDIR}/..
lha c --ignore-mac-files -x *.uaem $1 ${DISTDIRNAME}*
cd ../..
lha l $1
	