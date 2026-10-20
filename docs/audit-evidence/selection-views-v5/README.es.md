# Selecciones enteras mediante vistas internas

Esta revisión reduce el trabajo privado de `condición ? sí : no` sobre enteros. Las columnas
idénticas de ambas ramas usan la misma expresión interna; dos constantes distintas usan el control o
su complemento. Las demás columnas se calculan una vez por par de expresiones y se reutilizan dentro
del plan. Cada bit del resultado público sigue teniendo su propio qubit y recibe una X controlada;
no se copian estados cuánticos arbitrarios ni se comparten los qubits públicos. El plan se invierte
después de copiar el resultado, sin medir ni reiniciar.

El caso que antes fallaba al seleccionar directamente entre INT64_MIN e INT64_MAX ahora pasa la
prueba completa con su resultado de resto y las inversas reales del compilador. El fallo original de
148 entradas sigue en el archivo anterior; no se cambia su estado histórico.

La revisión final tiene 151 entradas congeladas. Pasan, por configuración normal y ASan/UBSan, 1195
comprobaciones de expresiones, 55 de desplazamientos, 990 de división, 393 nativas y 271 de
actualizaciones. Se añadieron 288 dominios finitos de selección: todos los bytes, ambas etiquetas y
polaridades del control, resultados de 8/32/64 bits con y sin signo, columnas compartidas o
complementadas, selección anidada y ramas aritméticas. El recorrido de puertas compara todos los
qubits, conserva las fases y comprueba auxiliares cero tras invertir. Son pruebas de acción sobre
bases; no simulaciones densas de registros de 64 bits. Sus perfiles usan como límite ancho del
resultado más 40 qubits: nueve entradas públicas y hasta 31 columnas/guardas privados.

Los núcleos reutilizados proceden de reconstrucciones normales y ASan/UBSan de la revisión previa,
con todas las fuentes de producción idénticas. Se verifican sus hashes, se recompila el validador
cambiado y se ejecutan las cinco suites sobre las fuentes finales. El registro de esa revisión
previa conserva su fallo del test; un build correcto no se presenta como una suite correcta. No se
afirma una reconstrucción nueva del núcleo en esta ejecución dirigida.

Dos matrices independientes de programas realmente compilados pasan 768 estados complejos completos:
por configuración, cada matriz tiene 64 capturas, 32 referencias C y 192 estados, en tres puntos
antes/después de la limpieza. La primera comprueba selecciones directas de límites
int32/int64/uint64, resultados bool/byte y divisores CPU modificados después de capturarlos. La
segunda prepara los 256 bytes coherentes y comprueba resultados completos de 64 bits, columnas
repetidas/complementadas, condiciones anidadas, ramas con restos y ambas polaridades. Incluyen fases
distintas, dos niveles GCC, ambos modos de builtins y ambas políticas de síntesis. El máximo
observado en estas matrices compiladas es 102 qubits. Se comprueban las inversas emitidas por el
compilador y todos los auxiliares cero; no se inserta una inversa manual de apoyo.

Estas pruebas de estados conservan su revisión de 151 entradas anterior al ajuste del presupuesto
del test. El único archivo diferente de la revisión final es `tests/selection_state.inc`; todas las
fuentes de producción son idénticas. `proof-sources.tar.gz` conserva esa identidad por separado. No
se atribuyen ejecuciones nuevas a las pruebas ya cerradas.

Los intentos previos del test quedan conservados en `earlier-test-attempts.json` y
`evidence.tar.gz`: máximos sin signo codificados como constantes negativas, máscaras sin conversión
explícita al tipo de la operación y un presupuesto que omitía parte del trabajo de selecciones
anidadas. El verificador rechazó los dos primeros IR incorrectos; el tercer caso usaba 98 qubits
frente al presupuesto inicial de 96. La copia diagnóstica se identifica como tal y no sustituye las
suites.

`sources.tar.gz` y `verified-sources.json` describen la revisión final. `qualification.json`
conserva los doce comandos cerrados; `inherited-fresh-builds.json`, la procedencia de las
herramientas; `evidence-files.json`, los hashes de cada miembro de la evidencia comprimida.
`broad-campaign-at-archive.json` es una fotografía de la nueva campaña amplia, no un resultado de
éxito. La regresión amplia y el CI de esta revisión siguen separados. C11 completo, selecciones
generales que exceden los recursos, la división común y otros contextos siguen pendientes. La
ciberseguridad permanece clásica y no se afirma ejecución en hardware cuántico ni aceleración.
