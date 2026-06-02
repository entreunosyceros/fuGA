/*
 * fuga.h — Cabecera compartida de fuGA (proyecto de evasión 1.0).
 *
 * Centraliza constantes (pantalla, física, tuberías), los tipos de datos
 * del pájaro y del estado de partida, y los prototipos de la lógica en
 * juego.c. Cualquier cambio de dificultad o tamaño del mundo suele
 * empezar aquí.
 */

#ifndef FUGA_H
#define FUGA_H

#include <stdbool.h>

/* Identidad del juego (título, README, pantalla de inicio). */
#define NOMBRE_JUEGO     "fuGA"
#define SUBTITULO_JUEGO  "proyecto de evasión 1.0"

/* ── Dimensiones del mundo ─────────────────────────────────────────────
 * El área jugable ocupa ANCHO_JUEGO × ALTO_JUEGO celdas de terminal.
 * FILA_SUELO reserva las dos últimas filas para el suelo animado.
 */
#define ANCHO_JUEGO      60
#define ALTO_JUEGO       20

/* ── Pájaro ────────────────────────────────────────────────────────────
 * El pájaro no se mueve en horizontal: siempre está en COLUMNA_PAJARO.
 * Su hitbox es un rectángulo de ANCHO_PAJARO × ALTO_PAJARO celdas.
 */
#define COLUMNA_PAJARO   12
#define ANCHO_PAJARO     1   /* hitbox pequeña = más perdón al jugador */
#define ALTO_PAJARO      1

/* ── Física (modo accesible: flotar con toques lentos) ─────────────────
 * Un aleteo ≈ 2 celdas arriba; caída lenta. Ajusta aquí si aún cuesta.
 */
#define GRAVEDAD         0.07f
#define VELOCIDAD_ALETEO (-0.65f)
#define CAIDA_MAXIMA     0.9f

/* ── Tuberías (equilibrio: columnas visibles + hueco jugable) ────────────
 * Valores entre el modo original (hueco 7, rápido) y el modo fácil (hueco 12).
 */
#define ANCHO_TUBERIA       6
#define ALTO_HUECO          9
#define SEPARACION_TUBERIAS 28
#define VELOCIDAD_TUBERIAS  0.42f

/* Encoge la hitbox para colisión sin cambiar el sprite (ver juego.c). */
#define MARGEN_COLISION     0

/* Primera fila del suelo sólido (por encima no se dibuja el pájaro). */
#define FILA_SUELO       (ALTO_JUEGO - 2)

/* Máximo de tuberías vivas a la vez (array fijo en struct Juego). */
#define MAX_TUBERIAS     8

/*
 * EstadoJuego — Máquina de estados de la aplicación.
 *   ESTADO_TITULO:  pantalla de bienvenida.
 *   ESTADO_JUGANDO: física y colisiones activas.
 *   ESTADO_FIN:     game over; se sigue dibujando el mundo con overlay.
 */
typedef enum {
    ESTADO_TITULO,
    ESTADO_JUGANDO,
    ESTADO_FIN
} EstadoJuego;

/*
 * Pajaro — Entidad controlada por el jugador.
 *   y:     posición vertical en flotantes (permite movimiento suave).
 *   vel_y: velocidad vertical; la actualiza la gravedad y el aleteo.
 */
typedef struct {
    float y;
    float vel_y;
} Pajaro;

/*
 * Tuberia — Un par de columnas verdes con hueco en medio.
 *   x:               borde izquierdo del obstáculo (se decrementa cada tick).
 *   hueco_y:         fila donde empieza el hueco libre.
 *   puntaje_contado: evita sumar más de un punto por la misma tubería.
 */
typedef struct {
    float x;
    int hueco_y;
    bool puntaje_contado;
} Tuberia;

/*
 * Juego — Estado global; un solo struct recorre main → juego → renderizado.
 *   tuberias[]:          obstáculos activos (máx. MAX_TUBERIAS).
 *   cantidad_tuberias:   cuántas entradas del array están en uso.
 *   desplazamiento_suelo: solo visual; anima el patrón del suelo.
 *   fotograma:           contador de ticks; útil para efectos o niveles.
 */
typedef struct {
    EstadoJuego estado;
    Pajaro pajaro;
    Tuberia tuberias[MAX_TUBERIAS];
    int cantidad_tuberias;
    int puntaje;
    int record;
    float desplazamiento_suelo;
    int fotograma;
} Juego;

/* Pone el juego en pantalla de título y record a cero. */
void juego_iniciar(Juego *j);

/* Reinicia pájaro, puntaje y tuberías para una partida nueva. */
void juego_reiniciar(Juego *j);

/* Aplica impulso de aleteo si estamos en ESTADO_JUGANDO. */
void juego_aletear(Juego *j);

/* Avanza un fotograma: física, tuberías, colisiones y puntuación. */
void juego_actualizar(Juego *j);

/*
 * juego_rectangulo_pajaro — Convierte y flotante en celdas enteras
 * para colisión AABB. La X es fija (COLUMNA_PAJARO).
 */
bool juego_rectangulo_pajaro(float y_pajaro, int *arriba, int *abajo,
                             int *izquierda, int *derecha);

#endif
