#!/bin/sh

#Скрипт получает команду в кавычках в ком. строке - 1 параметр.
#Если команд несколько, разделять их ;

#Значения переменных по умолч. для psftp и plink
. ./psvars.sh
set -x

plink $TARGET $TPORT -l $TUSER -i $TKEY -batch $1

