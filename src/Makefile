
qr: qr.c encode.c decode.c print.c noise.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address qr.c encode.c decode.c print.c load.c noise.c version_db.c ssim.c gssim.c xalloc.c -lm

test: encode.c test.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined test.c xalloc.c version_db.c

theft: fuzz.c theft.c encode.c decode.c print.c
	gcc -o $@ -I. -I ${HOME}/include -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address fuzz.c theft.c encode.c decode.c util.c print.c noise.c xalloc.c version_db.c -L ${HOME}/lib -ltheft

