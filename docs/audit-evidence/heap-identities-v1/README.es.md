# Identidades del heap: corrección y evidencia finita

**Progreso parcial; quedan dos contraejemplos cuánticos abiertos.** La autoasignación `p[0]=p[0]` y
el autorrelleno `memset(p,p[0],1)` perdían pureza conjunta por historia redundante. En los casos
reproducidos pasan de pureza 1 → 0.5 a conservar 1 → 1, sin mediciones ni resets.

El motor demuestra simbólicamente que el resultado coincide con la palabra existente antes de crear
otro dueño. Usa las expresiones tipadas y certificados de dependencias/épocas; no consulta
amplitudes. La prueba es acotada y, si no alcanza, continúa con la validación y síntesis ordinarias.
Los controles de una definición deben cubrir el contexto de uso y los operandos descartados deben
ser válidos. Los argumentos CPU de `memset` se capturan una vez, incluso con longitud cero.

La regresión registra **2232 puntos de estado complejo completo y 1116 inversas completas**:
1488/744 normales O0/O2 y 744/372 ASan/UBSan O1. Comprueba ocho tipos enteros, con y sin signo, de 8
a 64 bits; ocho perfiles, ambas políticas de síntesis y 0, 1 u 8 iteraciones. Incluye fases, dueños
y cables públicos estables y ausencia de basura privada. Los resultados devueltos se comprueban
hasta 32 bits: las dos capturas de 64 bits con entradas vivas exceden 128 cables y figuran como
exclusiones de recursos. El relleno idéntico nativo usa un byte con bytes altos cero; el validador C
comprueba además máscaras altas y ambos órdenes de bytes.

En ambas construcciones pasan 1682 aserciones nuevas del motor, 16878 del heap existente, 940 de
relleno, 395 del núcleo y los contratos de memoria/historia 23/15. Estos recuentos mezclan
aserciones y contextos y no son porcentajes de C. Las regresiones de memoria del heap y buffers
fijos tienen sus propios estados e inversas en `summary.json`. Los siete programas del bloque 05
pasan con la construcción normal.

Siguen fallando la limpieza de un préstamo no observado al salir del bloque y el sector inactivo de
una copia coherente no idéntica al heap: pureza 1 → 0.5. La cápsula conserva sus fuentes y logs. Los
efectos CPU bajo control cuántico y las expresiones de relleno que mezclan efectos CPU y carga
cuántica siguen rechazados; los rechazos no se cuentan como compatibilidad implementada.

`evidence.tar.gz` incluye las 225 entradas congeladas, comandos, salidas, herramientas auxiliares,
hashes de binarios e inspección ASan/UBSan. `earlier-attempts.tar.gz` conserva diagnósticos de
prototipos y del arnés, sin calificarlos como fuentes congeladas ni sumarlos a los resultados
finales. La nota técnica y los papers históricos conservan su revisión original. No se afirma
ejecución en una QPU física ni cierre del compilador. La ciberseguridad sigue clásica.

Para reproducir la regresión mantenida:

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
build/validate_heap_identities
python3 scripts/validate-heap-identities.py --output build/new-heap-identity-proof --levels O0 O2
```
