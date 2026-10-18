# CI completo del compilador integrado de 137 entradas

El [CI remoto](https://github.com/igna-s/cket/actions/runs/39135202402) terminó con éxito sobre
`a2527e7d038817fafda6c10d60f79c6d7ce76331`. Pasaron `scripts/verify.sh`, el formato y la
preservación de contratos del formateador. El [resumen](summary.json) conserva los registros y las
identidades.

La suite ejecutó, en normal y ASan/UBSan, 393 casos nativos, 290 de retornos de memoria, 170 de
distancias entre punteros y las demás suites del flujo. Su fuzzing tuvo 145 compilaciones por
configuración con semilla 20261011. No se presenta como la campaña local ampliada de 665
compilaciones y semilla 20261012. También se validaron 70 circuitos por configuración con QuEST: 64
estados y 6 canales explícitos. Los 137 archivos de compilación coinciden con los integrados; los
cambios posteriores enumerados pertenecen a documentación y evidencia.

Este resultado pertenece al compilador integrado y no al candidato v5. Los bugs conocidos de
operandos y cargas CPU y la cobertura incompleta de C11 siguen pendientes. Un CI exitoso no
demuestra ausencia de todos los bugs. La ciberseguridad permanece clásica.
