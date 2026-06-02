/*
 * juego.c — Lógica de simulación de fuGA (sin dibujar nada en pantalla).
 *
 * Responsabilidades:
 *   - Integrar la física del pájaro (gravedad + aleteo).
 *   - Crear, mover y reciclar tuberías.
 *   - Detectar colisiones con tuberías, techo y suelo.
 *   - Actualizar puntaje y record.
 *
 * No incluye ncurses: solo manipula el struct Juego.
 */

#include "fuga.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Generación de tuberías ─────────────────────────────────────────── */

/*
 * hueco_aleatorio_y — Calcula en qué fila empieza el hueco de una tubería.
 * Deja margen arriba (min_y) y abajo (hueco + suelo) para que siempre
 * quepa un pasillo jugable.
 */
/*
 * hueco_aleatorio_y — Hueco amplio y, la mayoría de veces, cerca del centro
 * vertical para que pasar no exija precisión extrema.
 */
static int hueco_aleatorio_y(void) {
    int min_y = 2;
    int max_y = FILA_SUELO - ALTO_HUECO - 1;
    int centro = (FILA_SUELO - ALTO_HUECO) / 2;

    if (max_y < min_y)
        return centro;

    /* ~50 % de tuberías con hueco casi centrado (±1 fila). */
    if (rand() % 10 < 5) {
        int offset = (rand() % 3) - 1;
        int y = centro + offset;
        if (y < min_y)
            y = min_y;
        if (y > max_y)
            y = max_y;
        return y;
    }
    return min_y + rand() % (max_y - min_y + 1);
}

/*
 * crear_tuberia — Inserta una tubería al borde derecho del mundo.
 * Si el array está lleno, no hace nada (en la práctica no debería
 * ocurrir si eliminar_tuberias_fuera funciona bien).
 */
static void crear_tuberia(Juego *j) {
    if (j->cantidad_tuberias >= MAX_TUBERIAS)
        return;

    Tuberia *t = &j->tuberias[j->cantidad_tuberias++];
    t->x = (float)ANCHO_JUEGO;       /* aparece fuera de la vista derecha */
    t->hueco_y = hueco_aleatorio_y();
    t->puntaje_contado = false;
}

/*
 * eliminar_tuberias_fuera — Compactación “in-place” del array.
 * Conserva solo tuberías cuyo borde derecho sigue visible (x + ancho > -1).
 * Así reutilizamos slots sin malloc/free.
 */
static void eliminar_tuberias_fuera(Juego *j) {
    int escrito = 0;
    for (int leido = 0; leido < j->cantidad_tuberias; leido++) {
        if (j->tuberias[leido].x + ANCHO_TUBERIA > -1.0f)
            j->tuberias[escrito++] = j->tuberias[leido];
    }
    j->cantidad_tuberias = escrito;
}

/* ── API pública del módulo juego ─────────────────────────────────────── */

/*
 * juego_iniciar — Llamar una vez al arrancar el programa.
 * Deja la pantalla en ESTADO_TITULO; el record persiste hasta cerrar.
 */
void juego_iniciar(Juego *j) {
    memset(j, 0, sizeof *j);
    j->estado = ESTADO_TITULO;
    j->record = 0;
}

/*
 * juego_reiniciar — Prepara una partida desde cero (título → jugar o reintentar).
 * Coloca al pájaro en el centro y genera dos tuberías ya visibles para
 * que el jugador no empiece contra un muro instantáneo.
 */
void juego_reiniciar(Juego *j) {
    /* Centro vertical del área jugable (por encima del suelo). */
    j->pajaro.y = (float)(FILA_SUELO / 2 - ALTO_PAJARO / 2);
    j->pajaro.vel_y = 0.0f;
    j->puntaje = 0;
    j->cantidad_tuberias = 0;
    j->desplazamiento_suelo = 0.0f;
    j->fotograma = 0;

    crear_tuberia(j);
    crear_tuberia(j);
    /* Primera tubería a mitad de pantalla; la segunda, más a la derecha. */
    j->tuberias[0].x = (float)(ANCHO_JUEGO / 2);
    j->tuberias[1].x = (float)(ANCHO_JUEGO / 2 + SEPARACION_TUBERIAS);
}

/*
 * juego_aletear — El jugador pulsa espacio/enter/flecha arriba.
 * Sustituye vel_y por VELOCIDAD_ALETEO (no suma): cada aleteo es un
 * impulso fijo en cada pulsación (estilo arcade).
 */
void juego_aletear(Juego *j) {
    if (j->estado != ESTADO_JUGANDO)
        return;
    /*
     * Impulso acumulativo con tope: varios toques seguidos suben un poco más,
     * pero sin disparar el pájaro al techo.
     */
    j->pajaro.vel_y += VELOCIDAD_ALETEO;
    if (j->pajaro.vel_y < VELOCIDAD_ALETEO * 1.5f)
        j->pajaro.vel_y = VELOCIDAD_ALETEO * 1.5f;
}

/*
 * juego_rectangulo_pajaro — Traduce posición flotante a rejilla de celdas.
 * floorf en y porque la colisión trabaja con índices de fila enteros.
 */
bool juego_rectangulo_pajaro(float y_pajaro, int *arriba, int *abajo,
                             int *izquierda, int *derecha) {
    *arriba = (int)floorf(y_pajaro);
    *abajo = *arriba + ALTO_PAJARO - 1;
    *izquierda = COLUMNA_PAJARO;
    *derecha = COLUMNA_PAJARO + ANCHO_PAJARO - 1;
    return true;
}

/* ── Colisiones (AABB: Axis-Aligned Bounding Box) ───────────────────── */

/*
 * rectangulos_solapan — Prueba de intersección 2D entre dos rectángulos
 * alineados a los ejes. Cuatro comparaciones: hay solape en Y y en X.
 */
static bool rectangulos_solapan(int arriba1, int abajo1, int izq1, int der1,
                                int arriba2, int abajo2, int izq2, int der2) {
    return arriba1 <= abajo2 && abajo1 >= arriba2 &&
           izq1 <= der2 && der1 >= izq2;
}

/*
 * choca_tuberia — Para cada tubería construye dos rectángulos sólidos:
 *   [fila 0 .. hueco_y-1]           → tramo superior
 *   [hueco_y+ALTO_HUECO .. suelo]   → tramo inferior
 * Si el rectángulo del pájaro toca cualquiera, hay game over.
 */
static bool choca_tuberia(const Juego *j, int arriba, int abajo,
                          int izquierda, int derecha) {
    for (int i = 0; i < j->cantidad_tuberias; i++) {
        const Tuberia *t = &j->tuberias[i];
        int tubo_izq = (int)floorf(t->x);
        int tubo_der = tubo_izq + ANCHO_TUBERIA - 1;

        if (rectangulos_solapan(arriba, abajo, izquierda, derecha,
                                0, t->hueco_y - 1, tubo_izq, tubo_der))
            return true;

        if (rectangulos_solapan(arriba, abajo, izquierda, derecha,
                                t->hueco_y + ALTO_HUECO, FILA_SUELO - 1,
                                tubo_izq, tubo_der))
            return true;
    }
    return false;
}

/* ── Bucle de simulación (un paso por fotograma) ────────────────────── */

/*
 * juego_actualizar — Corazón de la física y del gameplay.
 * Orden típico en juegos 2D arcade:
 *   1) Salir pronto si no estamos jugando.
 *   2) Mover entidades (pájaro + tuberías + decoración).
 *   3) Generar contenido nuevo (tuberías).
 *   4) Comprobar colisiones y puntuación.
 */
void juego_actualizar(Juego *j) {
    if (j->estado != ESTADO_JUGANDO)
        return;

    j->fotograma++;

    /* Animación del suelo (solo estado visual; comparte velocidad del mundo). */
    j->desplazamiento_suelo += VELOCIDAD_TUBERIAS;
    if (j->desplazamiento_suelo >= 4.0f)
        j->desplazamiento_suelo -= 4.0f;

    /*
     * Física del pájaro (integración explícita de Euler, un paso fijo):
     *   vel_y += GRAVEDAD        → aceleración constante hacia abajo
     *   clamp a CAIDA_MAXIMA     → velocidad terminal
     *   y     += vel_y           → integrar posición
     * Para otro “feel”, prueba semi-implícito o multiplicar por delta_t.
     */
    j->pajaro.vel_y += GRAVEDAD;
    if (j->pajaro.vel_y > CAIDA_MAXIMA)
        j->pajaro.vel_y = CAIDA_MAXIMA;
    j->pajaro.y += j->pajaro.vel_y;

    /* El mundo se desplaza a la izquierda: restamos x a cada tubería. */
    for (int i = 0; i < j->cantidad_tuberias; i++)
        j->tuberias[i].x -= VELOCIDAD_TUBERIAS;

    eliminar_tuberias_fuera(j);

    /* Spawn: cuando la última tubería avanzó lo bastante, creamos otra. */
    if (j->cantidad_tuberias == 0 ||
        j->tuberias[j->cantidad_tuberias - 1].x <
            (float)(ANCHO_JUEGO - SEPARACION_TUBERIAS)) {
        crear_tuberia(j);
    }

    int arriba, abajo, izquierda, derecha;
    juego_rectangulo_pajaro(j->pajaro.y, &arriba, &abajo, &izquierda, &derecha);

    /* Techo/suelo: fila 0 y la última fila jugable tienen margen extra. */
    if (arriba < 0 || abajo >= FILA_SUELO) {
        j->estado = ESTADO_FIN;
        if (j->puntaje > j->record)
            j->record = j->puntaje;
        return;
    }

    if (choca_tuberia(j, arriba, abajo, izquierda, derecha)) {
        j->estado = ESTADO_FIN;
        if (j->puntaje > j->record)
            j->record = j->puntaje;
        return;
    }

    /* Punto: el borde derecho de la tubería pasó la columna del pájaro. */
    for (int i = 0; i < j->cantidad_tuberias; i++) {
        Tuberia *t = &j->tuberias[i];
        if (!t->puntaje_contado && t->x + ANCHO_TUBERIA < (float)COLUMNA_PAJARO) {
            t->puntaje_contado = true;
            j->puntaje++;
        }
    }
}
