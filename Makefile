#В файле должны быть концы строк <LF> и <TAB> в начале строк в рецептах. Настроить редактор.
#Цель dtb компилит дерево устройств devicetree.dtb. Обычно не используется, т.к. дерево лежит в отдельном репозитории и собирается отдельно.
#Цель clean под виндами и линуксом стирает все результаты компиляции, в т.ч. библиотеки.

#=== Переменные с именами и исходниками lkm. Нужны в обоих случаях запуска (из ком. строки и из kbuild)

#Имя модуля gpiotst.ko не должно совпадать ни с одним из линкуемых c-шников из списка gpiotst-y
M_NAME=gpiotst.ko
#В модулях драйвера не должны повторяться имена модулей приложения. Флаги компиляции для ядра другие. Нет гарантии, что будет работать.
#M_SRC=gpiotest.c supply_lkm.c
M_SRC=gpiot2.c supply_lkm.c
#Из drivinclude.h
M_INCL=drivinclude.h mydefs.h adr_map.h dmadefsmy.h supply.h supply_cmn.c


#---Путь к ядру линукса
#Задается в build agent environment  http://teamcity.ircoc.vrn.ru:8080/admin/editProject.html?projectId=T8Firmware&tab=projectParams
#или здесь, если не задан на момент запуска.
KERNPATH ?= ~/rep/imx_v6_v7-mainline-linux/KERNEL

#====== Вызов из командной строки, не из kbuild?
ifndef KERNELRELEASE

# архитектура - используется при сборке драйвера. 
# может быть arm или arm64
#export ARCH ?= arm

#Стандартные переменные, используемые в implicit rules
CC = $(CROSS)gcc
CXX = $(CROSS)g++
LD= $(CROSS)ld
#Нестандартная
CSIZE=$(CROSS)size

$(M_NAME): $(M_SRC) $(M_INCL)
	make -j8 ARCH=arm LOCALVERSION=ivz CROSS_COMPILE=$(CROSS) ccflags-y:="$(ADDRESS_CFLAGS)" -C $(KERNPATH) M=$(CURDIR) KBUILD_EXTMOD=$(CURDIR) modules

else   # ====== Обратный вызов данного makefile из kbuild

#Имя модуля .ko не должно совпадать ни с одним из линкуемых c-шников из списка modname-y
obj-m += $(M_NAME:.ko=.o)
gpiotst-y:= $(M_SRC:.c=.o)  #replace .с with .o
# ${warning BUILD_HOSTNAME=$(BUILD_HOSTNAME)}

#Чтобы сделать ассемблер (.s) :
#CFLAGS_dmadriv.o:= -save-temps=obj
#-Wa,-alh=dmadriv.s doesn't work


endif