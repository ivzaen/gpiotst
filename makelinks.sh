#Для сборки драйвера нужны линки:  на исходники ядра, на каталог build с результатами под wb2 и wb6, на скрипты создающий переменные для сборки и для sftp/ssh.

#Создаем симлинки. Если они уже созданы - не переписываем, т.к. лучше проверить вручную перед удалением.
ln -sn ~/rep/linux-wiren-pull2 KERNEL
ln -sn ~/rep/wiren/build_kernel/build build
ln -sn ~/rep/wiren/build_kernel/vars.sh vars.sh
ln -sn ~/rep/wiren/build_kernel/psvars.sh psvars.sh
