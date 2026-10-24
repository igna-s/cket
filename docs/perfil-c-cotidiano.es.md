# Alcance actual: C cotidiano funcional

**Acordado el 23 de octubre de 2026.** El objetivo actual es poder escribir programas completos con
las herramientas habituales de C: funciones, tipos comunes, control, arrays, punteros y vectores. La
compatibilidad debe funcionar en conjunto dentro de un perfil explícito. La idea de «el 50% que usás
siempre» expresa esta prioridad; no es una cifra de cobertura medida.

El inventario de todas las interfaces C11 se conserva como referencia técnica y posible ampliación.
Completar cada una de sus entradas deja de ser condición para cerrar este alcance. Una declaración
aceptada, un diagnóstico correcto o un ejemplo que solo compila tampoco completan una capacidad.

## Qué debe poder usarse

| Parte del núcleo         | Resultado que se busca                                                                                            | Estado actual y trabajo pendiente                                                                                                                                                                                                                                                                           |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tipos y operadores       | `bool`, caracteres, enteros con y sin signo, aliases, `size_t`, conversiones y operadores habituales.             | Hay representación y síntesis entera hasta 64 bits. Faltan combinaciones de contextos, división general y cierre de recursos; no están completos todos los usos de cada tipo.                                                                                                                               |
| Funciones                | Parámetros, llamadas entre funciones, resultados, locales y funciones que procesan buffers.                       | Composición directa en la misma unidad y préstamos parciales. Faltan usos generales de punteros, retornos y vidas; la ABI entre unidades conserva restricciones.                                                                                                                                            |
| Control                  | Condiciones, `for`, `while`, `do while` y salidas habituales con efectos y limpieza correctos.                    | Hay condiciones coherentes, bucles acotados y control CPU. Faltan combinaciones de flujo e historia; un bucle con condición cuántica general sigue pendiente.                                                                                                                                               |
| Arrays y structs básicos | Vectores, matrices, campos enteros, recorridos, lecturas y actualizaciones.                                       | Arrays y productos fijos implementados con límites. La nueva lectura/escritura con índice cuántico admite arrays escalares de 1 a 64 elementos con ejecuciones y evidencia finita de estados e inversas.                                                                                                    |
| Punteros y buffers       | Pasar un buffer y su longitud, desreferenciar, indexar, crear alias y escribir conservando procedencia y límites. | Hay préstamos comprobados con funciones encadenadas e índices cuánticos sobre escalado canónico; hay lecturas con índices CPU y recorridos con acumulación y retorno. Se añaden heaps nativos con tamaño CPU, alias y préstamos de funciones. Faltan más formas de direcciones/vidas e integración general. |
| Vectores dinámicos       | Reservar almacenamiento, mantener longitud/capacidad, recorrer, actualizar y liberar con una vida definida.       | Implementación parcial de `malloc`, `calloc`, `realloc` y `free`, capacidad CPU, préstamos y recorridos. `dynamic_heap.c` usa almacenamiento que crece realmente. Faltan escrituras con índice cuántico, consumidores con resultado y memoria/ABI generales. [Contrato](c11-heap.md).                       |
| Biblioteca frecuente     | Copiar, mover y llenar memoria; manejar cadenas y conversiones habituales dentro de contratos comprobados.        | Existen adaptadores parciales. Las nuevas conversiones decimales, escrituras por búsqueda e indexación coherente tienen ejecuciones y pruebas finitas; sus dominios generales siguen pendientes.                                                                                                            |
| Cálculo real habitual    | `float` y `double` cuánticos, operaciones y conversiones con semántica definida.                                  | Pendiente. Los flotantes clásicos de GCC y los ángulos de las puertas no implementan esos tipos cuánticos. Se abordan después de integrar el núcleo entero, funciones y memoria.                                                                                                                            |
| Entrada/salida           | Leer datos clásicos, preparar entradas, medir explícitamente y mostrar o guardar resultados.                      | Frontera CPU existente; debe componerse con los programas del perfil sin mediciones ocultas.                                                                                                                                                                                                                |

La ciberseguridad, criptografía y autenticación permanecen clásicas. El cierre de este perfil no
requiere completar caracteres anchos, complejos, atomics, threads ni todas las interfaces poco
habituales. Sus entradas pendientes siguen visibles en el inventario.

## Orden de trabajo

1. Integrar los enteros y sus consumidores con funciones, arrays y structs básicos.
1. Completar el recorrido habitual de punteros y buffers, incluidos alias y resultados de funciones.
1. Incorporar almacenamiento y vida de vectores dinámicos, con límites de recursos declarados.
1. Completar las operaciones frecuentes de memoria/cadenas que necesiten esos programas y después el
   cálculo habitual con `float` y `double`.
1. Cerrar la validación integrada y ajustar documentación y afirmaciones de los papers al resultado.

Se entrega capacidad utilizable por bloques. El usuario abrió ahora la validación de propiedad,
copias, fases e inversas. La [campaña dirigida](audit-evidence/ownership-contracts-v1/README.es.md)
ejecuta siete ejemplos recientes y comprueba estados de funciones, buffers, índices, historia y
conversiones. La evidencia finita nueva no completa la integración general de las filas anteriores.

## Cómo se determina que funciona

El cierre debe incluir programas completos que combinen las capacidades, con una referencia C
ordinaria para los resultados definidos. Los casos de aceptación deben incluir:

- Funciones que reciben un vector y su longitud, recorren sus elementos, acumulan y devuelven un
  resultado; llamadas encadenadas con locales y resultados intermedios.
- Arrays y matrices con lecturas y escrituras, índices CPU y cuánticos dentro del contrato, structs
  básicos y valores producidos por asignaciones e incrementos.
- Buffers compartidos entre funciones, alias admitidos, copias y movimientos de memoria, cadenas y
  conversión de datos, conservando los valores que siguen vivos.
- Un vector con almacenamiento dinámico que se reserva, se usa y se libera según su contrato; un
  nombre de ejemplo o un array fijo no sustituye esta capacidad.
- Combinaciones de condiciones y bucles que ejerciten limpieza de temporales, historia y retornos;
  medición explícita en la frontera de salida.

Para datos cuánticos, comprobar solo números medidos no alcanza: también hacen falta casos de fase,
correlación e inversa, contrastes independientes apropiados y agotamiento de recursos declarado. La
validación se concentra en este perfil y sus combinaciones. Cada resultado debe identificar las
fuentes que realmente se ejecutaron.

«100% funcional» significa cumplir los contratos de este perfil y sus programas integrados. No
promete recursos ilimitados, toda la biblioteca C11 ni ejecución demostrada en una QPU física. El
estado actual todavía no cumple ese cierre.

Los contratos precisos se conservan en la [matriz de capacidades](c-coverage.md), los
[bloques entregados](entregas-incrementales.es.md) y el
[estado general](estado-general-del-proyecto.es.md).
