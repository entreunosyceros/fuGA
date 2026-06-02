# fuGA — proyecto de evasión 1.0

<img width="768" height="419" alt="fuGA" src="https://github.com/user-attachments/assets/0848fa5a-f62d-400e-a4a7-e0e66c2c1a96" />

![Versión](https://img.shields.io/badge/Versi%C3%B3n-1.0-yellow)
![Licencia](https://img.shields.io/badge/Licencia-Libre-green)
![Estado](https://img.shields.io/badge/Estado-En_Evasi%C3%B3n-red)
![Motor](https://img.shields.io/badge/Engine-ncurses-blue)

Juego de evasión en terminal (C + ncurses): pájaro con gravedad, tuberías que avanzan, colisiones por rectángulos y puntuación. Pensado para aprender física sencilla y detección de choques en C.

## Requisitos

- Compilador **GCC** con soporte C11
- Biblioteca **ncurses** (desarrollo)

En Debian/Ubuntu:

```bash
sudo apt install build-essential libncurses-dev
```

## Compilar y ejecutar

```bash
make
./fuga
```

Para limpiar artefactos de compilación:

```bash
make clean
```

## Controles

| Tecla | Acción |
|-------|--------|
| Espacio, W, Enter o ↑ | Aletear / empezar / reintentar |
| Q o Esc | Salir del juego |

## Estructura del proyecto

| Archivo | Descripción |
|---------|-------------|
| `fuga.h` | Constantes, tipos (`Pajaro`, `Tuberia`, `Juego`), `NOMBRE_JUEGO` y prototipos |
| `juego.c` | Física, generación de tuberías, puntuación y colisiones AABB |
| `renderizado.c` | Dibujo con ncurses (marco, suelo, tuberías, pájaro, menús) |
| `main.c` | Bucle principal (~30 FPS) y lectura de teclado |
| `Makefile` | Reglas de compilación y enlace |

## Cómo funciona (resumen)

1. **Física**: cada fotograma se suma `GRAVEDAD` a `vel_y` y se actualiza `y`. Al aletear, `vel_y` pasa a `VELOCIDAD_ALETEO`. `CAIDA_MAXIMA` limita la velocidad de caída.
2. **Tuberías**: aparecen a la derecha con hueco aleatorio y se desplazan a la izquierda. Al pasar una tubería, el puntaje sube en 1.
3. **Colisiones**: el pájaro es un rectángulo fijo en X; cada tubería son dos bloques sólidos. Choque → `ESTADO_FIN`.
4. **Renderizado**: ncurses en modo no bloqueante; el suelo se anima con `desplazamiento_suelo`.

---

## Programar la física

La simulación del pájaro está en `juego_actualizar()` (`juego.c`). El modelo actual es **integración de Euler con paso fijo** (un tick ≈ 33 ms en `main.c`):

```c
j->pajaro.vel_y += GRAVEDAD;      /* aceleración → velocidad */
if (j->pajaro.vel_y > CAIDA_MAXIMA)
    j->pajaro.vel_y = CAIDA_MAXIMA;
j->pajaro.y += j->pajaro.vel_y;   /* velocidad → posición */
```

El aleteo **no suma** velocidad: la **sustituye** en `juego_aletear()`:

```c
j->pajaro.vel_y = VELOCIDAD_ALETEO;
```

### Ajustes rápidos (macros en `fuga.h`)

| Macro | Qué controla |
|-------|----------------|
| `GRAVEDAD` | Cuánto acelera hacia abajo cada fotograma |
| `VELOCIDAD_ALETEO` | Impulso al aletear (más negativo = sube más) |
| `CAIDA_MAXIMA` | Velocidad terminal de caída |
| `VELOCIDAD_TUBERIAS` | Velocidad del mundo (tuberías + suelo) |

### Ideas para ampliar la física

**1. Gravedad variable (más realista al subir y bajar)**

```c
/* En juego_actualizar(), sustituir la línea de gravedad por: */
if (j->pajaro.vel_y < 0.0f)
    j->pajaro.vel_y += GRAVEDAD * 0.6f;  /* subiendo: menos gravedad */
else
    j->pajaro.vel_y += GRAVEDAD * 1.2f;  /* cayendo: más gravedad */
```

**2. Aleteo que acumula en lugar de reemplazar**

```c
void juego_aletear(Juego *j) {
    if (j->estado != ESTADO_JUGANDO)
        return;
    j->pajaro.vel_y += VELOCIDAD_ALETEO;  /* impulso relativo */
    if (j->pajaro.vel_y < VELOCIDAD_ALETEO)
        j->pajaro.vel_y = VELOCIDAD_ALETEO; /* tope de subida */
}
```

**3. Paso de tiempo explícito (`delta_t`)**

Útil si cambias los FPS en `main.c` sin retocar la sensación del juego:

```c
/* En fuga.h */
#define DELTA_T 0.033f

/* En juego_actualizar() */
j->pajaro.vel_y += GRAVEDAD * DELTA_T * 60.0f;
j->pajaro.y += j->pajaro.vel_y * DELTA_T * 60.0f;
```

Ajusta el factor `60.0f` hasta que se sienta igual que antes.

**4. Rozamiento / resistencia del aire**

```c
j->pajaro.vel_y *= 0.98f;  /* al final de juego_actualizar(), antes de y += vel_y */
```

**5. Doble salto o planeo**

Añade un campo `int saltos_restantes` en `Pajaro`, reinícialo en `juego_reiniciar()` y en `juego_aletear()` decrementa solo si `saltos_restantes > 0`.

### Dónde tocar cada comportamiento

| Comportamiento | Archivo | Función |
|----------------|---------|---------|
| Caída y movimiento vertical | `juego.c` | `juego_actualizar()` |
| Impulso al pulsar tecla | `juego.c` | `juego_aletear()` |
| Constantes globales | `fuga.h` | macros `GRAVEDAD`, etc. |
| FPS del bucle | `main.c` | `RETRASO_FOTOGRAMA` |
| Hitbox del pájaro | `fuga.h` + `juego.c` | `ANCHO_PAJARO`, `juego_rectangulo_pajaro()` |

---

## Añadir nuevos niveles

El juego base **no tiene niveles**: la dificultad es fija vía macros. Para niveles progresivos conviene parametrizar la generación de tuberías y la física.

### Paso 1: Definir un struct de nivel

En `fuga.h` (o un nuevo `niveles.h`):

```c
typedef struct {
    const char *nombre;
    int puntos_para_siguiente;  /* 0 = último nivel */
    float gravedad;
    float velocidad_aleteo;
    float velocidad_tuberias;
    int alto_hueco;
    int separacion_tuberias;
} Nivel;

extern const Nivel NIVELES[];
extern const int CANTIDAD_NIVELES;
```

### Paso 2: Tabla de niveles

En `niveles.c`:

```c
#include "fuga.h"

const Nivel NIVELES[] = {
    { "Fácil",   5,  0.30f, -2.5f, 0.45f, 8, 26 },
    { "Normal", 10,  0.35f, -2.8f, 0.55f, 7, 22 },
    { "Difícil", 0,  0.42f, -3.0f, 0.65f, 6, 18 },
};
const int CANTIDAD_NIVELES = 3;
```

### Paso 3: Guardar el nivel activo en `Juego`

```c
typedef struct {
    /* ... campos existentes ... */
    int indice_nivel;
} Juego;
```

Inicializa `indice_nivel = 0` en `juego_iniciar()` / `juego_reiniciar()`.

### Paso 4: Usar el nivel en la simulación

En `juego_actualizar()`, en lugar de las macros fijas:

```c
const Nivel *nv = &NIVELES[j->indice_nivel];

j->pajaro.vel_y += nv->gravedad;
/* ... */
j->tuberias[i].x -= nv->velocidad_tuberias;
```

En `hueco_aleatorio_y()` y `crear_tuberia()`, usa `nv->alto_hueco` y `nv->separacion_tuberias` (puedes pasar `const Nivel *` como parámetro o leer `j->indice_nivel`).

En `juego_aletear()`:

```c
j->pajaro.vel_y = NIVELES[j->indice_nivel].velocidad_aleteo;
```

### Paso 5: Subir de nivel al alcanzar puntos

Al final del bucle de puntuación en `juego_actualizar()`:

```c
const Nivel *nv = &NIVELES[j->indice_nivel];
if (nv->puntos_para_siguiente > 0 &&
    j->puntaje >= nv->puntos_para_siguiente &&
    j->indice_nivel + 1 < CANTIDAD_NIVELES) {
    j->indice_nivel++;
    /* Opcional: mensaje en pantalla, sonido, pausa breve */
}
```

### Paso 6: Mostrar el nivel en pantalla

En `dibujar_interfaz()` (`renderizado.c`):

```c
mvprintw(origen_y - 1, origen_x + 20, " %s ",
         NIVELES[j->indice_nivel].nombre);
```

(No olvides declarar `NIVELES` en un header incluido por `renderizado.c`.)

### Paso 7: Compilar el nuevo módulo

En el `Makefile`:

```makefile
OBJS = main.o juego.o renderizado.o niveles.o
```

### Otras ideas de “niveles” sin tabla

| Enfoque | Cómo |
|---------|------|
| Dificultad continua | Cada N puntos, `VELOCIDAD_TUBERIAS += 0.02f` en `juego_actualizar()` |
| Nivel con obstáculos fijos | Array de `Tuberia` precalculado por nivel, sin `hueco_aleatorio_y()` |
| Modo “práctica” | Hueco enorme (`ALTO_HUECO = 12`) vía flag en `Juego` |
| Nivel desde archivo | Leer JSON/lineas con `fscanf` al iniciar (más trabajo, muy flexible) |

### Checklist al añadir un nivel

- [ ] ¿Cambia física, tuberías o ambos?
- [ ] ¿`juego_reiniciar()` vuelve al nivel 0 o mantiene progreso?
- [ ] ¿Las colisiones usan `ALTO_HUECO` del nivel o una macro global?
- [ ] ¿El HUD muestra nombre del nivel?
- [ ] ¿Recompilaste con `make clean && make`?

---

## Ajustar la dificultad (sin sistema de niveles)

Edita solo las macros en `fuga.h`:

| Macro | Efecto |
|-------|--------|
| `GRAVEDAD` | Más valor = cae más rápido |
| `VELOCIDAD_ALETEO` | Más negativo = salto más fuerte |
| `ALTO_HUECO` | Hueco más pequeño = más difícil |
| `SEPARACION_TUBERIAS` | Menos valor = tuberías más juntas |
| `VELOCIDAD_TUBERIAS` | Más valor = juego más rápido |

## Licencia

Código de ejemplo libre para uso educativo y personal... el que quiera que lo coja y lo use
