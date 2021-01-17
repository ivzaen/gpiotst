#=== Переменные с именами и исходниками lkm. Нужны в обоих случаях запуска (из ком. строки и из kbuild)

#Имя модуля *.ko не должно совпадать ни с одним из линкуемых c-шников из списка *-y
M_NAME=gpiotst.ko
#В модулях драйвера не должны повторяться имена модулей приложения. Флаги компиляции для ядра другие. Нет гарантии, что будет работать.
#M_SRC=gpiotest.c supply_lkm.c
M_SRC=gpiot2.c supply_lkm.c
#Из drivinclude.h
M_INCL=drivinclude.h mydefs.h adr_map.h dmadefsmy.h supply.h supply_cmn.c

#====== Вызов из командной строки, не из kbuild?
ifndef KERNELRELEASE

else   # ====== Обратный вызов данного makefile из kbuild

#Имя модуля .ko не должно совпадать ни с одним из линкуемых c-шников из списка modname-y
obj-m += $(M_NAME:.ko=.o)
gpiotst-y:= $(M_SRC:.c=.o)  #replace .с with .o
# ${warning BUILD_HOSTNAME=$(BUILD_HOSTNAME)}

#Чтобы сделать ассемблер (.s) :
#CFLAGS_dmadriv.o:= -save-temps=obj
#-Wa,-alh=dmadriv.s doesn't work

endif
