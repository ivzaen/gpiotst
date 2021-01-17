#!/bin/sh

#Аргумент1 ком. строки - BRD=KERNEL_FLAVOUR=wb2|wb6
BRD=${1:-wb6}
echo BRD=$BRD

. ./vars.sh
#Для скриптов отправки надо TARGET=wb5 если wb2.
export TARGET=$TARGET

try ./sftp2.sh "put gpiotst.ko /root/gpiotst.ko"
