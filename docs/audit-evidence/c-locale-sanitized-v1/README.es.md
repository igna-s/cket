# Perfiles y locale numérico: continuación instrumentada

26 de octubre de 2026. Pasan 96 comprobaciones de perfiles inmutables y 70 de locale numérico con
ASan/UBSan. Son dos validadores pendientes de la
[campaña anterior](../c-priority-integration-v1/README.es.md); no se repiten sus diez aprobados.

La evidencia verifica las dependencias locales y la instrumentación de los binarios reutilizados. El
único cambio entre las fuentes del harness anterior y las actuales afecta al test QASM, que no es
dependencia de estos validadores. El compilador y motor permanecen iguales al SDK. Para locale se
conserva la exclusión específica de glibc, comprobada en la campaña anterior.

`evidence.tar.gz` conserva las 237 entradas actuales, comandos, hashes y logs. Quedan cinco
validadores instrumentados de aquella campaña: capturas de bytes, operandos controlados, división,
ctype y frontera de libc. Esta continuación no completa `scripts/verify.sh`, la revisión general de
papers ni el perfil cotidiano. La ciberseguridad permanece clásica.
