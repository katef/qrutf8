
qr: fuzz.c qr.c encode.c decode.c module.c datastream.c seg.c mask.c print.c noise.c pcg.c version_db.c ssim.c gssim.c yv12.c xalloc.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined fuzz.c qr.c encode.c decode.c module.c datastream.c seg.c mask.c util.c print.c load.c noise.c pcg.c version_db.c ssim.c gssim.c yv12.c xalloc.c -lm

test: encode.c decode.c module.c datastream.c test.c mask.c
	gcc -o $@ -I. -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=undefined test.c module.c datastream.c mask.c xalloc.c version_db.c util.c

theft: fuzz.c theft.c encode.c decode.c module.c datastream.c seg.c mask.c print.c noise.c pcg.c
	gcc -o $@ -I. -I ${HOME}/include -std=c99 -Wshadow -Wall -pedantic -Werror -g -Og -W -fsanitize=address fuzz.c theft.c encode.c module.c decode.c datastream.c seg.c mask.c util.c print.c noise.c pcg.c xalloc.c version_db.c -L ${HOME}/lib -ltheft

