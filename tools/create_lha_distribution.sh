#!/bin/sh

# 1. Parameter LHA file

DISTDIR=build/LhaDistribution
ARCH=020

mkdir -p ${DISTDIR}
cp -r DistributionTemplate/* ${DISTDIR}
cp KSZ8851/devicedriver/build/build-${ARCH}/ksz8851.device.${ARCH} ${DISTDIR}/network/

#rm -fr ${DISTDIR}

cd ${DISTDIR}
lha c --ignore-mac-files $1 *
cd ../..
cp A1200PlusNetworkDriver.readme build
lha l $1
	