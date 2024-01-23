# Minishell-Linux
Para compilar y construir el ejecutable, ejecutamos el siguiente comando:

```bash
gcc -Wall -Wextra minishell.c libparser.a -static -o ejecutable
```
La shell tiene las siguientes funcionalidades:
- Ejecutar en foreground líneas con más de dos mandatos con sus respectivos argumentos, enlazados con ‘|’.
- Redirección de entrada estándar desde archivo y redirección de salida a archivo.
- Ejecutar el mandato *cd*
- Ejecutar tanto en foreground como en background líneas con más de dos mandatos con sus respectivos argumentos, enlazados con ‘|’, redirección de entrada estándar desde archivo y redirección de salida a archivo.
- Ejecutar el mandato *jobs* y *fg*
- Ejecutar el mandato *exit* y *umask*
