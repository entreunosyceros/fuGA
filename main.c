/*
 * main.c — Punto de entrada y bucle principal de fuGA.
 *
 * Flujo:
 *   1) Inicializar semilla aleatoria, struct Juego y ncurses.
 *   2) Bucle infinito (~30 FPS):
 *        a) Vaciar cola de teclas (getch no bloqueante).
 *        b) juego_actualizar() — un paso de simulación.
 *        c) renderizado_fotograma() — dibujar.
 *        d) nanosleep() — limitar velocidad del bucle.
 *
 * La lógica de estados (título / jugar / fin) vive repartida entre
 * manejar_entrada() y juego.c según quién deba mutar el struct Juego.
 */

#define _POSIX_C_SOURCE 200809L

#include "fuga.h"

#include <curses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Prototipos del módulo renderizado (no están en fuga.h para mantener
 * la cabecera libre de dependencias de ncurses). */
void renderizado_iniciar(void);
void renderizado_cerrar(void);
void renderizado_fotograma(const Juego *j);

/* 55 ms ≈ 18 fotogramas/s: más tiempo para ver y pulsar. */
static const struct timespec RETRASO_FOTOGRAMA = {0, 55000000L};

/*
 * tecla_aletear — Unifica las teclas que cuentan como “saltar”.
 * KEY_UP requiere keypad(stdscr, TRUE) en renderizado_iniciar().
 */
static bool tecla_aletear(int tecla) {
    return tecla == ' ' || tecla == 'w' || tecla == 'W' ||
           tecla == '\n' || tecla == '\r' || tecla == KEY_UP;
}

/*
 * manejar_entrada — Traduce una tecla en acciones de juego.
 *   Q / Esc → salir del proceso (exit).
 *   Título  → espacio inicia partida (reinicia + ESTADO_JUGANDO).
 *   Jugando → espacio llama juego_aletear().
 *   Fin     → espacio reinicia otra partida.
 */
static void manejar_entrada(Juego *j, int tecla) {
    if (tecla == 'q' || tecla == 'Q' || tecla == 27)
        exit(0);

    if (j->estado == ESTADO_TITULO) {
        if (tecla_aletear(tecla)) {
            juego_reiniciar(j);
            j->estado = ESTADO_JUGANDO;
            juego_aletear(j); /* impulso inicial: no caer nada al empezar */
        }
        return;
    }

    if (j->estado == ESTADO_JUGANDO && tecla_aletear(tecla))
        juego_aletear(j);

    if (j->estado == ESTADO_FIN && tecla_aletear(tecla)) {
        juego_reiniciar(j);
        j->estado = ESTADO_JUGANDO;
        juego_aletear(j);
    }
}

int main(void) {
    /* Semilla para hueco_aleatorio_y() en juego.c. */
    srand((unsigned)time(NULL));

    Juego juego;
    juego_iniciar(&juego);

    renderizado_iniciar();
    /* Asegura restaurar la terminal aunque salgamos con exit() desde Q. */
    atexit(renderizado_cerrar);

    for (;;) {
        /*
         * Drenar todas las teclas pulsadas este fotograma.
         * ERR significa “no hay más teclas”; el while no bloquea gracias a
         * timeout(0) configurado en renderizado_iniciar().
         */
        int tecla;
        while ((tecla = getch()) != ERR)
            manejar_entrada(&juego, tecla);

        juego_actualizar(&juego);
        renderizado_fotograma(&juego);
        nanosleep(&RETRASO_FOTOGRAMA, NULL);
    }

    return 0;
}
