#!/bin/sh

#Скрипт получает команду в кавычках в ком. строке - 1 параметр.
#Если команд несколько, разделять их \n  Новая строка с \ не дает \n в команде. Можно так:
# ./sftp2.sh "
# put a.a \n
# "

. ./psvars.sh

set -x

TMPF=`mktemp`
echo $1 > $TMPF
psftp $TARGET $TPORT -l $TUSER -i $TKEY -be -b $TMPF
rm $TMPF
