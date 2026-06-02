/*
 * renderizado.c — Capa visual con ncurses (fuGA).
 *
 * Solo lee el struct Juego; no modifica física ni puntuación.
 * Cada fotograma: borrar pantalla → dibujar marco → contenido según
 * estado → refresh() para que el usuario vea el cambio.
 */

#include "fuga.h"

#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ── Primitivas de dibujo (funciones internas static) ─────────────────── */

/*
 * dibujar_marco — Rectángulo con esquinas ACS_* y líneas horizontales/
 * verticales. Separa visualmente el área de juego del resto de la terminal.
 */
static void dibujar_marco(int y, int x, int alto, int ancho) {
    /* Cuatro esquinas del recuadro. */
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + ancho - 1, ACS_URCORNER);
    mvaddch(y + alto - 1, x, ACS_LLCORNER);
    mvaddch(y + alto - 1, x + ancho - 1, ACS_LRCORNER);
    /* Bordes superior e inferior. */
    for (int i = 1; i < ancho - 1; i++) {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + alto - 1, x + i, ACS_HLINE);
    }
    /* Bordes izquierdo y derecho. */
    for (int i = 1; i < alto - 1; i++) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + ancho - 1, ACS_VLINE);
    }
}

/*
 * dibujar_columna_tuberia — Pinta una columna de alto `alto` celdas
 * en (pantalla_x, pantalla_y). par_color elige el par de colores ncurses.
 */
static void dibujar_columna_tuberia(int pantalla_y, int pantalla_x,
                                    int alto, int par_color) {
    attron(COLOR_PAIR(par_color));
    for (int fila = 0; fila < alto; fila++)
        mvaddch(pantalla_y + fila, pantalla_x, ACS_CKBOARD);
    attroff(COLOR_PAIR(par_color));
}

/*
 * dibujar_suelo — Dos filas en FILA_SUELO y FILA_SUELO+1.
 * La fila superior alterna '=' y '-' según (desplazamiento_suelo + x) % 4
 * para simular césped en movimiento sin mover colisiones.
 */
static void dibujar_suelo(const Juego *j, int origen_y, int origen_x) {
    attron(COLOR_PAIR(4));
    for (int x = 0; x < ANCHO_JUEGO; x++) {
        int baldosa = ((int)(j->desplazamiento_suelo) + x) % 4;
        chtype caracter = (baldosa < 2) ? '=' : '-';
        mvaddch(origen_y + FILA_SUELO, origen_x + x, caracter);
        mvaddch(origen_y + FILA_SUELO + 1, origen_x + x, ACS_HLINE);
    }
    attroff(COLOR_PAIR(4));
}

/*
 * dibujar_pajaro — Carácter '>' en amarillo (par 3).
 * roundf en y para alinear el sprite flotante a la rejilla de celdas.
 * No dibuja por debajo de FILA_SUELO (zona de suelo).
 */
static void dibujar_pajaro(const Juego *j, int origen_y, int origen_x) {
    int fila_pajaro = (int)roundf(j->pajaro.y);
    attron(COLOR_PAIR(3) | A_BOLD);
    if (fila_pajaro >= 0 && fila_pajaro < FILA_SUELO)
        mvaddch(origen_y + fila_pajaro, origen_x + COLUMNA_PAJARO, '>');
    attroff(COLOR_PAIR(3) | A_BOLD);
}

/*
 * dibujar_tuberias — Recorre el array y, por cada tubería visible,
 * dibuja ANCHO_TUBERIA columnas: bloque superior hasta hueco_y,
 * bloque inferior desde hueco_y + ALTO_HUECO hasta el suelo.
 */
static void dibujar_tuberias(const Juego *j, int origen_y, int origen_x) {
    for (int i = 0; i < j->cantidad_tuberias; i++) {
        const Tuberia *t = &j->tuberias[i];
        int px = (int)roundf(t->x);

        /* Culling: omitir tuberías totalmente fuera del área. */
        if (px + ANCHO_TUBERIA < 0 || px >= ANCHO_JUEGO)
            continue;

        for (int col = 0; col < ANCHO_TUBERIA; col++) {
            int sx = px + col;
            if (sx < 0 || sx >= ANCHO_JUEGO)
                continue;

            if (t->hueco_y > 0)
                dibujar_columna_tuberia(origen_y, origen_x + sx, t->hueco_y, 1);

            int hueco_abajo = t->hueco_y + ALTO_HUECO;
            int alto_inferior = FILA_SUELO - hueco_abajo;
            if (alto_inferior > 0)
                dibujar_columna_tuberia(origen_y + hueco_abajo, origen_x + sx,
                                        alto_inferior, 2);
        }
    }
}

/* Puntaje y récord encima del marco del área de juego. */
static void dibujar_interfaz(const Juego *j, int origen_y, int origen_x) {
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(origen_y - 1, origen_x, " PUNTOS %d ", j->puntaje);
    mvprintw(origen_y - 1, origen_x + ANCHO_JUEGO - 14, " RECORD %d ", j->record);
    attroff(COLOR_PAIR(5) | A_BOLD);
}

/* Centra una cadena ASCII dentro de un ancho dado (menús y game over). */
static void dibujar_texto_centrado(int y, int x, int ancho, const char *texto) {
    int longitud = (int)strlen(texto);
    int inicio = x + (ancho - longitud) / 2;
    if (inicio < x)
        inicio = x;
    mvprintw(y, inicio, "%s", texto);
}

/* ── Inicialización y cierre de ncurses ───────────────────────────────── */

/*
 * renderizado_iniciar — Configura la terminal para el juego.
 * timeout(0): getch() devuelve ERR si no hay tecla (bucle no bloqueante).
 */
void renderizado_iniciar(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(0);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_CYAN, COLOR_BLACK);
        init_pair(6, COLOR_WHITE, COLOR_BLACK);
        init_pair(7, COLOR_RED, COLOR_BLACK);
    }
}

/* Restaura la terminal al salir (registrado con atexit en main.c). */
void renderizado_cerrar(void) {
    endwin();
}

/* ── Un fotograma completo ────────────────────────────────────────────── */

/*
 * renderizado_fotograma — Orquesta todo el dibujo de un tick.
 * origen_y/origen_x: esquina superior izquierda del área interior al marco.
 */
void renderizado_fotograma(const Juego *j) {
    const int margen_y = 2;
    const int margen_x = 2;
    const int alto_marco = ALTO_JUEGO + 2;
    const int ancho_marco = ANCHO_JUEGO + 2;

    erase();
    dibujar_marco(margen_y, margen_x, alto_marco, ancho_marco);

    const int origen_y = margen_y + 1;
    const int origen_x = margen_x + 1;

    if (j->estado == ESTADO_TITULO) {
        /* Pantalla de título: solo texto, sin simulación visible. */
        attron(COLOR_PAIR(6) | A_BOLD);
        dibujar_texto_centrado(origen_y + 3, origen_x, ANCHO_JUEGO, NOMBRE_JUEGO);
        attroff(COLOR_PAIR(6) | A_BOLD);

        attron(COLOR_PAIR(5));
        dibujar_texto_centrado(origen_y + 5, origen_x, ANCHO_JUEGO, SUBTITULO_JUEGO);
        dibujar_texto_centrado(origen_y + 9, origen_x, ANCHO_JUEGO,
                               "ESPACIO / W para empezar");
        dibujar_texto_centrado(origen_y + 11, origen_x, ANCHO_JUEGO, "Q para salir");
        attroff(COLOR_PAIR(5));

        attron(COLOR_PAIR(3));
        dibujar_texto_centrado(origen_y + 15, origen_x, ANCHO_JUEGO, "> >");
        attroff(COLOR_PAIR(3));

        if (j->record > 0) {
            char buffer[32];
            snprintf(buffer, sizeof buffer, "Record: %d", j->record);
            dibujar_texto_centrado(origen_y + 16, origen_x, ANCHO_JUEGO, buffer);
        }
    } else {
        /* Partida o game over: mundo completo + overlay si ESTADO_FIN. */
        dibujar_suelo(j, origen_y, origen_x);
        dibujar_tuberias(j, origen_y, origen_x);
        dibujar_pajaro(j, origen_y, origen_x);
        dibujar_interfaz(j, origen_y, origen_x);

        if (j->estado == ESTADO_FIN) {
            attron(COLOR_PAIR(7) | A_BOLD);
            dibujar_texto_centrado(origen_y + ALTO_JUEGO / 2 - 3, origen_x,
                                   ANCHO_JUEGO, "FIN DEL JUEGO");
            attroff(COLOR_PAIR(7) | A_BOLD);

            /* Easter egg fuGA: burocracia infinita, aleteo eterno. */
            attron(COLOR_PAIR(5));
            dibujar_texto_centrado(origen_y + ALTO_JUEGO / 2 - 1, origen_x,
                                   ANCHO_JUEGO, EASTER_EGG_XUNTA);
            dibujar_texto_centrado(origen_y + ALTO_JUEGO / 2 + 1, origen_x,
                                   ANCHO_JUEGO, EASTER_EGG_ALETEAR);
            dibujar_texto_centrado(origen_y + ALTO_JUEGO / 2 + 3, origen_x,
                                   ANCHO_JUEGO, "Q para salir");
            attroff(COLOR_PAIR(5));
        }
    }

    refresh(); /* vuelca el buffer de ncurses a la pantalla real */
}
