# Entradas de funciones cuánticas comprobadas

27 de octubre de 2026. Corrección posterior a RC3; la 1.0 estable sigue pendiente.

Se reproducen dos falsas ejecuciones exitosas del compilador anterior: una llamada a través de un
prototipo clásico y un puntero guardado antes de una definición `quant`. Podían alcanzar un stub que
retornaba sin ejecutar la operación cuántica. Un inicializador global de punteros también podía
eludir la comprobación de direcciones que existía para expresiones locales.

El frontend registra referencias clásicas por símbolo de ensamblador y las contrasta al capturar la
definición cuántica. También comprueba inicializadores globales y callbacks automáticos. La entrada
nativa restante informa un error explícito si se alcanza mediante búsqueda dinámica o código
externo. Las llamadas válidas siguen expandiendo la plantilla con sus contratos de propiedad; los
callbacks CPU conservan su implementación clásica.

## Fuentes y resultados

La instantánea contiene 240 entradas. Cambian el plugin, la composición del frontend y el runtime;
el motor, driver y backend conservan sus fuentes y binarios. Se reconstruyen plugin y runtime. Pasan
en cada configuración normal y ASan/UBSan:

- 90 contratos de entrada: 30 composiciones nativas válidas, 55 rechazos en compilación y cinco
  rechazos explícitos en ejecución mediante búsqueda dinámica POSIX, en cinco niveles GCC.
- La regresión de préstamos de lectura: 305 comprobaciones, incluidos estados completos, fases e
  inversas independientes con el alcance de sus programas booleanos.
- Bindings: 519 comprobaciones. Núcleo nativo: 395 pruebas.

El [SDK](../../../output/sdk/checked-entry/README.es.md) reconstruye 37 programas de seis bloques y
pasa 1509 comprobaciones normales de ejemplos. Sus tres flujos Windows ejecutan el paquete y
compilan los ejemplos de interferencia y heap. Los paquetes anteriores conservan sus identidades.

La primera preparación del caso dinámico se detuvo porque faltaba `_GNU_SOURCE` antes de los
headers; se conserva separada de la calificación final. El informe también distingue objetos
inicialmente copiados de los objetos de runtime reconstruidos antes de ejecutar las pruebas.
`evidence.tar.gz` guarda fuentes, comandos, registros, manifiestos y los contraejemplos.

## Integración pendiente

El CI de `0740cfa` se detuvo en `scripts/validate-runtime-memory.py` con
`unmodelled retained version`. Este fallo pertenece a sus fuentes de 239 entradas y sigue abierto;
no se ha determinado aquí si requiere corregir el circuito o la referencia. Su estado y registros se
conservan por separado. Los cuatro README que fallaron formato se corrigieron en `c1bc6aa`: 420
archivos y los contratos del formateador pasaron sin cambiar las entradas del compilador.

Esta campaña dirigida no completa `scripts/verify.sh`, la ABI cuántica general, recursión, punteros
de función cuánticos, vidas/alias arbitrarios, flotantes ni la revisión general de papers. No se
afirma ejecución en QPU física ni aceleración. La ciberseguridad permanece clásica.
