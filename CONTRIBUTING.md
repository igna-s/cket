# Contribuir a `|c>`

`|c>` (`cket`) es un compilador experimental. Los cambios deben preservar promociones y conversiones de C, efectos evaluados una vez, propiedad de registros, coherencia y limpieza de auxiliares. El [contrato de 1.0](docs/v1-plan.md) distingue requisitos de publicación y capacidades actuales.

## Preparar el entorno

Usá Linux o WSL. Para la configuración de referencia en Ubuntu 22.04:

```bash
sudo apt-get update
sudo apt-get install -y gcc-11 g++-11 gcc-11-plugin-dev binutils python3 python3-venv
export CC=gcc-11
export CXX=g++-11
bash scripts/build.sh
bash scripts/test.sh
```

El plugin necesita cabeceras que correspondan al GCC utilizado. El README describe `GCC_PLUGIN_INCLUDE` y la alternativa de dependencias locales.

## Validar un cambio

Ejecutá las comprobaciones relacionadas con el comportamiento modificado. Para cambios en semántica, síntesis o ejecución, usá la suite nativa y los sanitizadores:

```bash
bash scripts/test.sh
bash scripts/sanitize-all.sh
```

Para promociones, casts y transformaciones del frontend:

```bash
python3 scripts/fuzz_frontend.py
```

Para recursos aritméticos y exportación independiente:

```bash
python3 scripts/benchmark_synthesis.py
python3 -m venv .venv
.venv/bin/python -m pip install -r scripts/requirements-validation.txt
.venv/bin/python scripts/validate_qasm.py
```

El motor nativo no requiere Python ni Qiskit. Estas dependencias se utilizan para validación y mediciones de desarrollo. Los scripts guardan evidencia en `build/`; CI adjunta sus registros y reproducciones.

## Issues y pull requests

Un reporte de error debe incluir un programa mínimo, el comando exacto, las versiones de GCC y `cket`, el comportamiento esperado y el observado. Para fases y entrelazamiento, adjuntá amplitudes o un circuito que permita distinguir errores de fase de errores de probabilidad.

Una PR debe explicar el problema, el cambio de comportamiento y la validación ejecutada. Si amplía el subconjunto aceptado, actualizá el README y el contrato correspondiente en `docs/`. Documentá las cotas, los límites defensivos y cualquier cambio en propiedad o ABI. No presentes un rechazo conservador ni una prueba sobre un dominio pequeño como soporte general.

Los archivos generados, SDKs descargados, binarios y credenciales deben permanecer fuera de Git. Los módulos QIR de ejemplo se versionan como código fuente. Los commits originales conservan las fechas asignadas al inicializar el repositorio; usá fechas reales para los cambios posteriores.

Para posibles vulnerabilidades, seguí [SECURITY.md](SECURITY.md).
