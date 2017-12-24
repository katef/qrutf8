
qr: qr.c encode.c print.c noise.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address qr.c print.c load.c noise.c version_db.c

test: encode.c test.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined test.c xalloc.c version_db.c

fuzz: fuzz.c encode.c print.c
	gcc -o $@ -I. -I ${HOME}/include -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address fuzz.c print.c noise.c xalloc.c version_db.c -L ${HOME}/lib -ltheft

