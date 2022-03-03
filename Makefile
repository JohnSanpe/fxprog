# SPDX-License-Identifier: GPL-2.0-or-later
flags = -Wall -l usb-1.0
heads = fxhw.h fxprog.h
objs  = fxprog.o hexprase.o main.o

%.o:%.c $(heads)
	@ echo -e "  \e[32mCC\e[0m	" $@
	@ gcc -o $@ -c $< -g $(flags)

fxprog: $(objs)
	@ echo -e "  \e[34mMKELF\e[0m	" $@
	@ gcc -o $@ $^ -g $(flags)

clean:
	@ rm -f $(objs) fxprog
