# Relleno completo que lee el destino: evidencia finita

26 de octubre de 2026. Seguimiento de `0cfbcd5`; la versión permanece en `1.0.0-rc.3`.

La [nota técnica](../../paper/single_fill_note.es.md) describe el caso que falló durante la
integración: `memset(&x, strncmp((char *)&x, "\x01", 1), 1)`, con `x = b + 1`. El límite anterior se
reprodujo sobre RC3, el bloque de cadenas y el compilador posterior de historial. Todos requerían 26
cables y fallaban en modo denso; el mismo caso pasaba en modo disperso.

La corrección elimina una captura de byte en el caso de una sola palabra completa de 8/16 bits,
incondicional y fuera de iteraciones de runtime. Conserva el patch, el dueño público y el transporte
de recetas de inversa. El programa usa ahora 18 cables y ejecuta en modo denso. Las palabras de
32/64 bits, los arrays, escrituras parciales y contextos controlados conservan la captura
compartida.

Se congelaron 237 entradas; solo cambia `src/quantum_semantics.inc` respecto de la copia usada en la
integración anterior. El plugin se reconstruye en ambas configuraciones. Motor, runtime y
controladores conservan los binarios anteriores porque sus fuentes no cambian; los hashes dejan
explícita esa reutilización.

| Comprobación                                   | Normal | ASan/UBSan |
| ---------------------------------------------- | -----: | ---------: |
| Composición de memoria, suite mantenida        |    360 |        360 |
| `memset`, suite mantenida                      |    940 |        940 |
| Historial, modo dirigido                       |    355 |        355 |
| Núcleo nativo                                  |    395 |        395 |
| Densidades públicas adicionales                |    136 |        136 |
| Estados conjuntos por intérprete independiente |    136 |        136 |
| Inversas completas adicionales                 |     68 |         68 |

La matriz adicional compila 34 programas por configuración: 8/16/32 bits, con/sin signo, raíces y
productores, comparación, transformación e identidad; ambas políticas de síntesis y puntos antes y
después de la escritura. Contrasta las fases con las fórmulas C y exige workspace en cero cuando el
productor puede limpiarse. Dos perfiles de transformación de raíces de 32 bits siguen excluidos por
el límite de síntesis de 12 bits relevantes: se comprueba el mismo diagnóstico en el compilador
anterior. No son casos positivos. Los 64 bits no tienen una nueva matriz de estados.

`evidence.tar.gz` conserva fuentes, comandos, estados, construcción, hashes, los límites anteriores
y el fallo de integración. `earlier-attempts.tar.gz` conserva las variantes descartadas: una bajaba
a 17 cables pero dejaba historia redundante y perdía coherencia; otra aumentaba los recursos de un
perfil de 32 bits que antes compilaba. La primera usó fuentes de desarrollo sin congelar; su binario
y traza se preservan sin presentarlos como una identidad completa de fuentes. Los resultados
dirigidos sobre variantes anteriores no califican la versión final.

La integración restante tiene su propio registro y puede encontrar otros fallos. Estos resultados no
aprueban el CI general, todos los programas del perfil cotidiano, vidas/alias/ABI generales,
flotantes cuánticos ni los papers generales. La ciberseguridad permanece clásica.

La siguiente parada es `validate_memory_returns`: `unexpected memory return word history`. Se
reproduce con el compilador anterior y con esta corrección. El caso y su referencia aún deben
auditarse; el fallo no se convierte en una prueba aprobada.
