# Implementación de índices cuánticos

Se añaden lectura, asignación, incremento/decremento y valores de expresión sobre arrays escalares
fijos con índices cuánticos. El selector se acota antes de estrechar su representación, y las
escrituras capturan su valor e índice y retienen los propietarios anteriores. La infraestructura de
programas de valores se extrae de los adaptadores de cadenas para compartirla con el compilador.

Esta tentativa registra un **fallo de construcción, con código 1**: faltaba una declaración
adelantada del guardia de dominio tras extraer los helpers comunes. El proceso se detuvo antes de
compilar los ejemplos. No se ejecutó ningún programa ni suite.

El archivo conserva las fuentes y el log de ese fallo. Las tentativas posteriores tienen sus
propias fuentes y resultados; esta no aporta evidencia de compilación exitosa ni de funcionamiento.
La validación de ejecución sigue aplazada por indicación del usuario y la ciberseguridad permanece
clásica.
