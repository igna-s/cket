# Resto entero por constantes pequeñas

La revisión de 148 entradas añade un núcleo reversible para `%` con divisores de magnitud 2 a 16. El
camino existente de potencias de dos sigue teniendo prioridad. Se conserva el dividendo y se
acumulan sus bits ponderados en un residuo de hasta cuatro bits. Cada suma modular es una
permutación del residuo, implementada mediante X controladas y transposiciones por caminos Gray. Se
enumeran las codificaciones del pequeño acumulador, no los valores del dividendo. La corrección de
signo resta `2^ancho mod divisor` en la rama negativa y respeta el resto de C, incluido `INT_MIN`.
La inversa del plan limpia los auxiliares sin medir ni reiniciar.

La auditoría anterior sobre bool, caracteres, short e int, con variantes sin signo, tenía 632
capturas correctas y cuatro límites de recursos: `int % 3`. Con este núcleo pasan las 640 capturas
de sus 320 referencias C. Son perfiles finitos con dos etiquetas y diez expresiones; no cierran
todos los tipos ni sus contextos de C11.

En normal y ASan/UBSan pasan reconstrucciones nuevas y las suites de expresiones (907), división
(990), nativos (393) y actualizaciones (271), por configuración. Las 810 pruebas nuevas de
expresiones recorren todos los bytes y 47 etiquetas elegidas de cada dominio ancho, incluidos los
extremos y patrones deterministas. Comprueban promociones, resto firmado/sin signo, conversión a
bool, byte y entero completo de 32 bits, ambas ramas del control, fases y auxiliares cero tras la
inversa. Son recorridos de acción sobre bases de las puertas posteriores a la preparación; no una
simulación densa completa de 32 o 64 bits. El resultado completo de 64 bits sigue sujeto al límite
de 128 qubits junto con sus entradas y trabajo.

La prueba independiente de programas compilados sobre los 256 bytes pasa 720 estados complejos
completos por configuración: 240 capturas, 120 referencias C, todos los divisores de ±2 a ±16,
resultados int de 32 bits y tres puntos antes/después de las inversas de resultado y propietario. Se
conservan sus 147 entradas originales. Todas sus fuentes de producción son idénticas a las de la
revisión actual; las diferencias son el nuevo archivo de regresiones, su llamada y el contador de
archivo. No se presenta como otra ejecución sobre las 148 entradas.

Otra prueba actual pasa 264 estados completos por configuración: 88 capturas y 44 referencias C. Usa
dos etiquetas en límites de int32/int64/uint64, resultados bool/byte y divisores literales o CPU
modificados después de capturarlos. Comprueba las inversas reales del compilador y las fases, con
todos los auxiliares cero. Para 64 bits prepara las etiquetas mediante vistas de desplazamiento y
complemento: 0/INT64_MIN, -1/INT64_MAX o UINT64_MAX/INT64_MAX. Las referencias conservan las
conversiones del objetivo GCC Linux LP64. Entre ambas pruebas completas se verifican 1968 estados;
las 640 capturas de perfiles de tipos son una campaña distinta.

El intento anterior de seleccionar directamente entre INT64_MIN e INT64_MAX falló en ambas
configuraciones al preparar esa selección, antes de calcular el resto: superaba 128 qubits. Se
conservan el fallo, los eventos y su prefijo de 72 estados por configuración; ese prefijo no cierra
el experimento. La preparación por vistas posterior no elimina ese pendiente.

`sources.tar.gz` identifica las 148 entradas actuales. `proof147-sources.tar.gz` conserva las
fuentes de la prueba previa. `evidence.tar.gz` contiene fuentes de pruebas, oráculos independientes,
referencias, puertas exportadas, eventos, registros y cierres; `evidence-files.json` permite
verificar cada miembro por bytes. `qualification.json` mantiene los diez comandos cerrados. La
regresión completa y el CI de estas entradas siguen pendientes. No se afirma ejecución en hardware
cuántico, ventaja de velocidad ni C11 completo. La ciberseguridad permanece clásica.

## Cierre posterior de la campaña histórica de 144 entradas

`historical144-broad.tar.gz` conserva también el cierre posterior de la campaña anterior: 80
comandos correctos, 59 reutilizados de su prefijo cerrado y 21 ejecutados en la continuación, con
665 compilaciones de fuzzing por configuración y semilla 20261012. Los hashes de las 144 entradas
coinciden con su archivo histórico. Las reconstrucciones originales y la reutilización de sus
herramientas están diferenciadas; el proceso interrumpido no recibe un código de éxito inventado.
Este cierre de 144 entradas no verifica las revisiones de 146, 147 o 148.
