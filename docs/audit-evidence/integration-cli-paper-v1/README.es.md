# Referencia del CLI y ejemplos actuales de los papers

26 de octubre de 2026. Seguimiento de `be96caf`; la candidata continúa siendo `1.0.0-rc.3`.

La primera ejecución de `scripts/verify.sh` reconstruyó toda la cadena y pasó el núcleo nativo (395
comprobaciones) y los 37 ejemplos en cinco niveles de GCC. Se detuvo después, en el contrato del
CLI: `tests/test_examples.c` todavía buscaba `0.4.0` en la respuesta de `--version`, mientras que el
programa informaba RC3. El registro original y sus 237 entradas quedan preservados.

Se cambia exclusivamente esa expectativa por la línea completa de versión vigente. La nueva copia
congelada contiene las mismas 237 entradas, con solo ese archivo distinto. Se reconstruye el test de
ejemplos y se reutilizan por hash los binarios restantes de la construcción completa anterior. Pasan
el núcleo y las 1509 comprobaciones de los 37 ejemplos, incluidos contratos CLI, entradas y
exportaciones. La evidencia de esta cápsula se limita a esa fase terminada; no afirma que todas las
fases normales e instrumentadas de la integración hayan pasado.

También se extraen los tres bloques C exactos de los manuscritos español e inglés y se comprueba que
son iguales entre idiomas. Con el compilador actual en `-O2`:

| Ejemplo | Ejecución nativa, semilla 7        | Referencia de estado completo                                                                             |
| ------- | ---------------------------------- | --------------------------------------------------------------------------------------------------------- |
| Suma    | Imprime `12`.                      | Base 12 y workspace en cero.                                                                              |
| Bell    | Imprime una pareja correlacionada. | Amplitudes `1/sqrt(2)` en 00 y 11.                                                                        |
| Grover  | Imprime `3`.                       | Todas las amplitudes según `sin(7 asin(1/4))` y `cos(7 asin(1/4))/sqrt(15)`, salvo una fase global común. |

Son tres ejecuciones nativas y seis comparaciones completas: una por ejemplo y por política de
síntesis. Las mediciones explícitas se desactivan para comparar el estado unitario. La ejecución con
una semilla no constituye una campaña estadística de Grover o Bell. No se vuelven a producir las
tablas de la evaluación histórica ni se revisa visualmente el PDF en esta campaña.

`evidence.tar.gz` conserva fuentes, hashes, comandos, programas extraídos, estados, fórmulas,
registros y el rechazo anterior del CLI. `summary.json` identifica cada ámbito. Las aclaraciones de
documentación distinguen los casos del heap corregidos en RC2, los buffers fijos de RC3 y los
bloques posteriores de cadenas y efectos seleccionados. La revisión general de papers, la
integración completa y el núcleo cotidiano siguen pendientes. La ciberseguridad permanece clásica.
