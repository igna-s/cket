# Búsquedas guardadas y lectura de caracteres

Se añaden préstamos locales inmutables de los resultados de búsqueda: capturan un selector cuántico
una vez, conservan el objeto de origen y admiten alias, posición, comparaciones y lectura de
caracteres con `*p` o índices constantes. La selección no mide el puntero ni usa una dirección CPU
como si contuviera datos cuánticos. Escrituras, rebinding y ABI general siguen pendientes.

El motor conserva ahora intervalos no negativos demostrados de resultados calculados. Los usa para
analizar consumidores y omitir filas de tabla certificadas como inalcanzables. Los cambios de base
invalidan esos intervalos; una inferencia fallida mantiene el dominio de tipo completo. Esta
ampliación compartida necesita la calificación extensa posterior.

La fotografía contiene **172 entradas de compilador**. Una reconstrucción normal y el runner del
bloque 06 terminaron con código 0: cuatro programas nativos, cuatro circuitos y cuatro informes de
recursos. El ejemplo nuevo observó:

```text
text=aa position=1 character=97 next=0 has_b=0
```

También pasaron las **393 comprobaciones nativas existentes** del motor, sin sanitizadores nuevos.
Las muestras adicionales por semilla se registran en `summary.json`. Estos resultados no son una
comparación exhaustiva de amplitudes/fases, inversas, mutaciones CPU/cuánticas, accesos a páginas
protegidas o contextos GCC. Esas matrices y la integración amplia permanecen aplazadas al cierre.

La cápsula conserva fuentes, logs, exportaciones y el intento anterior. En ese primer intento el
compilador se construyó, pero el ejemplo nuevo fue rechazado por el dominio de la posición: el
backend olvidaba el rango del resultado guardado. La IR y el prefijo diagnóstico se conservan como
evidencia del fallo; la versión corregida añade certificados internos en lugar de quitar el guard.

El [contrato](../../c11-string-searches.md) distingue las capacidades implementadas de las que
todavía requieren calificación. El inventario sigue con 36 nombres de funciones parciales y no
registra ninguna interfaz C11 como completamente implementada.
