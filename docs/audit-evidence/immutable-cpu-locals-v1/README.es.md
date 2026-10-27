# Locales enteros CPU privados en llamadas coherentes

27 de octubre de 2026. El compilador admite locales automáticos enteros, inicializados con
expresiones CPU puras, que no vuelven a escribirse ni exponen su dirección. El análisis se realiza
sobre el cuerpo expandido e inspecciona también los inicializadores. La
[nota técnica](../../paper/private_cpu_locals_note.es.md) explica la selección clásica de puertas y
los límites de propiedad y coherencia.

## Comprobación dirigida

| Configuración             | Estados complejos | Inversas completas | Fronteras nativas | Rechazos |
| ------------------------- | ----------------- | ------------------ | ----------------- | -------- |
| Normal, cinco niveles GCC | 800               | 400                | 35                | 35       |
| ASan/UBSan O1             | 160               | 80                 | 7                 | 7        |

Ocho familias usan cuatro etiquetas coherentes, CPU 0/1/2/3/4 y ambas políticas de síntesis. Las
tres familias añadidas comprueban enteros con signo, booleanos, ocultación de nombres, predicados y
fases dependientes de locales y retornos. Las cinco anteriores conservan sus casos de llamadas,
fases globales controladas y continuaciones del llamador. No se suman con las campañas previas como
cobertura independiente.

Se comparan todas las amplitudes con fórmulas CPU y evolución independiente de puertas, se exige
espacio privado cero y se invierte la secuencia completa antes de observar. Las fronteras comprueban
división/carga/índice inactivos y cortocircuito. Se rechazan argumentos con efectos, escrituras CPU
observables, locales mutables, direcciones tomadas, `volatile` y lecturas cuánticas sin medir. Las
cinco regresiones por configuración son bindings (519), préstamos de lectura (305), entradas de
funciones (90), operandos controlados (1270) y núcleo (395). Sus recuentos tienen alcances
diferentes.

El [SDK](../../../output/sdk/immutable-locals/README.es.md) contiene el plugin reconstruido y 37
programas; la suite de ejemplos registra 1509 comprobaciones normales. Los tres flujos Windows
comprueban ejecución del paquete, compilación del ejemplo coherente y compilación del ejemplo de
heap, mediante WSL sin montar unidades Windows.

[summary.json](summary.json) identifica las 244 entradas y los productores normales e
instrumentados. Solo cambian dos archivos de producción y el validador dirigido respecto al
compilador anterior; el runtime, driver, backend y motor conservan sus fuentes y binarios.
`evidence.tar.gz` conserva fuentes, comandos, trazas, resultados, hashes y preparación previa.

El CI completo aprobado para 242 entradas no recalifica estas fuentes. Quedan integración completa
de esta ampliación, locales mutables, bucles coherentes, salidas cuánticas, vidas/alias/ABI
generales, flotantes cuánticos y revisión científica general. No se afirma C11 completo, una 1.0
estable, QPU física ni aceleración. La ciberseguridad permanece clásica.
