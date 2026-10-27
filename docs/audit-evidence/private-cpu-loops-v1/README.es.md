# Locales mutables privados y bucles CPU en llamadas coherentes

27 de octubre de 2026. La [nota técnica](../../paper/private_cpu_loops_note.es.md) explica el
análisis de ramas y los límites de las escrituras privadas. La condición y el número de iteraciones
son clásicos; las puertas mantienen el control cuántico del llamador.

| Configuración             | Estados complejos | Inversas completas | Fronteras nativas | Rechazos |
| ------------------------- | ----------------- | ------------------ | ----------------- | -------- |
| Normal, cinco niveles GCC | 1400              | 700                | 50                | 45       |
| ASan/UBSan O1             | 280               | 140                | 10                | 9        |

Catorce familias usan cuatro etiquetas coherentes, CPU 0/1/2/3/4 y ambas políticas de síntesis. Las
seis nuevas comprueban escrituras privadas bajo condiciones CPU, `for`, `while`, `do while`, bucles
anidados y un local declarado/actualizado dentro de una rama cuántica anidada. Las ocho anteriores
mantienen su alcance de fases, locales y retornos; estos recuentos se solapan con campañas
anteriores y no son cobertura porcentual de C.

Todas las amplitudes se comparan con fórmulas explícitas y evolución independiente de puertas. Se
exige espacio privado cero y se invierte la secuencia completa antes de observar. Las fronteras
incluyen inicializadores y actualizaciones con división/carga inactivos, cortocircuito y rechazo de
efectos CPU observables, direcciones tomadas, `volatile`, lectura cuántica implícita, escritura
privada bajo otra condición cuántica y condición de bucle con efectos todavía sin contrato.

Las regresiones por configuración pasan bindings (519), préstamos de lectura (305), entradas de
funciones (90), operandos controlados (1270) y núcleo (395). El
[SDK](../../../output/sdk/cpu-loops/README.es.md) ejecuta 37 programas y registra 1509
comprobaciones normales de ejemplos. Sus tres lanzadores Windows se ejecutan mediante WSL sin montar
unidades Windows. El ejemplo nuevo se compila también normal e instrumentado con cinco entradas CPU
y comprueba que modificar un parámetro por valor conserva el argumento del llamador.

[summary.json](summary.json) identifica 244 entradas y los productores. Cambian tres archivos de
producción y el validador dirigido respecto a `750bf28`; solo se reconstruye el plugin. El runtime,
driver, backend y motor conservan sus fuentes y binarios. La cápsula conserva fuentes, comandos,
trazas, hashes, el rechazo anterior y la preparación separada. La corrección de reutilización de
consultas internas se prueba sin autorizar llamadas CPU por nombre.

La integración completa aprobada de 242 entradas no recalifica estas fuentes. Quedan su CI completo,
condiciones/salidas cuánticas, saltos coherentes, efectos CPU anidados, vidas/alias/ABI generales,
flotantes cuánticos y revisión científica general. No se afirma una 1.0 estable, C11 completo, QPU
física ni aceleración. La ciberseguridad permanece clásica.
