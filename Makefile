
qr: qr-utf8.c
	cc -o $@ -std=c99 -Wall -pedantic -Werror -g -Og -W -fsanitize=address $<

