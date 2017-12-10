
qr: qr-utf8.c
	gcc -o $@ -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address $<

test: qr-test.c qr-utf8.c
	gcc -o $@ -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined qr-test.c

