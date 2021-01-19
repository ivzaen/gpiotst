#!/bin/sh

#Вызов сборки драйвера gpiotst.

#Аргумент1 ком. строки - BRD=KERNEL_FLAVOUR=wb2|wb6
BRD=${1:-wb6}
echo BRD=$BRD

. ./vars.sh
colormake $KMAKESTR M=$PWD KBUILD_EXTMOD=$PWD modules
