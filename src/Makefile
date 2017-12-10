
qr: qr.c libqr.c
	gcc -o $@ -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address $<

test: libqr.c test.c
	gcc -o $@ -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined test.c

