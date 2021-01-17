#!/bin/sh

#Аргумент1 ком. строки - BRD=KERNEL_FLAVOUR=wb2|wb6
BRD=${1:-wb6}
echo BRD=$BRD

. ./vars.sh
make $KMAKESTR M=$PWD KBUILD_EXTMOD=$PWD modules
