# Menos almacenamiento para un relleno que lee su destino

26 de octubre de 2026. Seguimiento posterior a `0cfbcd5`; la candidata sigue siendo RC3.

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant bool b = superposition(0, 1);
    quant uint8_t x = b + 1;
    memset((void *)&x, strncmp((const char *)&x, "\x01", 1), sizeof x);
    unsigned result = measure(x), input = measure(b);
    return result != input;
}
```

En el target comprobado, `x` vale inicialmente 1 o 2 y la comparación produce 0 o 1. Antes, el
relleno necesitaba 26 cables físicos: superaba el simulador denso de 24, aunque ejecutaba en modo
disperso. El límite también se reprodujo en RC3 y en el bloque anterior de cadenas.

La corrección usa el workspace del propio patch para calcular el byte antes del intercambio. Una
escritura incondicional que cubre una sola palabra de 8 o 16 bits no necesita otra captura de ocho
bits. Se mantienen el dueño público y el transporte comprobado de las recetas anteriores. El caso
mínimo baja a 18 cables y ya ejecuta en modo denso.

Arrays, escrituras parciales, controles coherentes e iteraciones conservan su captura. Las palabras
de 32/64 bits mantienen una captura compartida: expandir cuatro u ocho copias de una expresión puede
aumentar el trabajo y superar el presupuesto. Es una elección de síntesis; los anchos mayores
conservan su contrato existente.

Una primera variante calculaba en un dueño nuevo. Reducía cables, pero retenía un productor anterior
correlacionado como historia privada: la prueba con fases dio un error de densidad pública de 0,5.
Se descartó. Otra variante expandía también palabras grandes y causaba una regresión de recursos en
32 bits. La elección final conserva su captura compartida.

La optimización respeta la no clonación y no introduce mediciones ni borra historia para que el
programa quepa. Las raíces desconocidas conservan la información anterior necesaria; los productores
reconstruibles limpian historia cuando termina su préstamo. Se contrastan densidades públicas, fases
y estados conjuntos, además de invertir puertas mediante otro intérprete.

La [cápsula](../audit-evidence/single-fill-v1/README.es.md) conserva las fuentes, las alternativas
descartadas y la calificación normal y ASan/UBSan. Las exclusiones de síntesis se comparan con el
compilador anterior y quedan fuera de los casos positivos. La matriz usa el target little-endian de
GCC 11 en Linux x86-64. La integración completa, vidas y alias generales, recursos grandes,
flotantes cuánticos y revisión científica general siguen pendientes. La ciberseguridad permanece
clásica.
