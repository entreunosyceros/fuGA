# Makefile — Compila fuGA (proyecto de evasión 1.0).
#
#   main.o        → bucle y entrada de teclado
#   juego.o       → física, tuberías y colisiones
#   renderizado.o → dibujo en terminal
#
# Uso: make | make clean  →  ejecutable: ./fuga

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS = -lncurses -lm

OBJS = main.o juego.o renderizado.o

fuga: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.c fuga.h
juego.o: juego.c fuga.h
renderizado.o: renderizado.c fuga.h

clean:
	rm -f fuga $(OBJS)

.PHONY: clean
