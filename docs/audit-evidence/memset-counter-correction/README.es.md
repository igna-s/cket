# Corrección del recuento de `memset`

Los logs normal y ASan/UBSan de las mismas 144 entradas registran, por configuración, **350
ejecuciones nativas y 70 contratos de diagnóstico**. La documentación decía 340 y 80. El total
permanece en 800 comprobaciones, con las categorías de estados, trayectorias y generación separadas.

[summary.json](summary.json) conserva los hashes de las fuentes y de los dos logs, además del hash
del archivo de fuentes de la integración de cadenas. Ambos comandos terminaron con código cero. Se
extrajeron de una campaña general que seguía en ejecución; esta corrección no declara completada esa
campaña ni añade compatibilidad cuántica a otra función.
