# Dos regresiones del candidato de operandos controlados

El [registro terminal](failure-summary.json) conserva la reconstrucción normal exitosa y la suite
nativa posterior: 391 casos pasan y dos fallan. El XOR booleano controlado se rechaza por una
dependencia computada viva al medir su control; el bucle estático supera el límite nativo de 24
qubits. El registro íntegro conserva los programas exactos y los diagnósticos. Los dos archivos C
aquí publicados transcriben los cuerpos de esos casos para su inspección.

Las 138 entradas del candidato están identificadas por hash y disponibles en la
[cápsula de fuentes](../controlled-operands-maintained/sources.tar.gz). La campaña no ejecutó las
suites posteriores ni una reconstrucción instrumentada. Las pruebas específicas anteriores siguen
siendo evidencia de sus casos concretos y no demuestran una regresión completa exitosa. El candidato
no fue aplicado al compilador integrado. La ciberseguridad permanece clásica.
