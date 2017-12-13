
qr: qr.c libqr.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address $<

test: libqr.c test.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined test.c

fuzz: fuzz.c libqr.c
	gcc -o $@ -I. -I ${HOME}/include -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address fuzz.c -L ${HOME}/lib -ltheft

