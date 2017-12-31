
qr: qr.c encode.c decode.c seg.c mask.c print.c noise.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined qr.c encode.c decode.c seg.c mask.c util.c print.c load.c noise.c version_db.c ssim.c gssim.c xalloc.c -lm

test: encode.c test.c mask.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined test.c mask.c xalloc.c version_db.c util.c

theft: fuzz.c theft.c encode.c decode.c seg.c mask.c print.c
	gcc -o $@ -I. -I ${HOME}/include -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined fuzz.c theft.c encode.c decode.c seg.c mask.c util.c print.c noise.c xalloc.c version_db.c -L ${HOME}/lib -ltheft

