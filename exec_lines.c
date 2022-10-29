#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_SIZE 16
#define LINE_SIZE 128

// Función para ejecutar una línea de comandos
void ejecutar_linea(char *line, int length)
{
    // Iniciar proceso hijo para procesar la línea leída
    char *arr[LINE_SIZE];
    int i;
    char *p, *end;
    pid_t pid;

    switch (pid = fork())
    {
    // Fork falla
    case -1:
        perror("fork()");
        exit(EXIT_FAILURE);
        break;
    // Fork exitoso, procesamos la línea leída
    case 0:
        // Separar el contenido de la línea en un array para la llamada a execvp
        i = 0;
        p = strtok(line, " ");
        while (p != NULL)
        {
            arr[i++] = p;
            p = strtok(NULL, " ");
        }
        arr[i] = NULL;
        execvp(arr[0], arr);

    default:
        return;
    }
}

int main(int argc, char **argv)
{
    int opt, numproc = 1;
    optind = 1;

    // Leer la opción "-p" si la hubiera
    while ((opt = getopt(argc, argv, "p:h")) != -1)
    {
        switch (opt)
        {
        // Opción -p nos da el número de procesos en ejecución
        case 'p':
            numproc = atoi(optarg);
            // Comprobar que numproc está entre 1 y 8
            if (numproc < 1 || numproc > 8)
            {
                fprintf(stderr, "Error: El número de procesos en ejecución tiene que estar entre 1 y 8.\nUso: exec_lines [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes\npara ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p NUMPROC\tNúmero de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n");
                exit(EXIT_FAILURE);
            }
            break;
        // Opción -h devuelve un mensaje de ayuda con las instrucciones de uso del programa
        case 'h':
            fprintf(stderr, "Uso: exec_lines [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes\npara ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p NUMPROC\tNúmero de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n");
            exit(EXIT_SUCCESS);
        // Cualquier otro argumento provoca un error e imprime las instrucciones de uso
        default:
            fprintf(stderr, "Uso: exec_lines [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes\npara ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p NUMPROC\tNúmero de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Uso: %s [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes para ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p   NUMPROC Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_read, resto, status, total = 0, ejecutando = 0, length = 0, ejecutado = 0;
    char *buf = malloc(sizeof(char) * BUF_SIZE);
    char *line = malloc(sizeof(char) * (LINE_SIZE + BUF_SIZE));
    char *end;

    while ((num_read = read(STDIN_FILENO, buf, BUF_SIZE)))
    {
        ejecutado = 0;
        memcpy(line + length, buf, num_read);
        length += num_read;
        total += num_read;
        while (length > 0)
        {
            if ((end = strchr(line, '\n')))
            {
                ejecutado = 1;
                if (end - line > total - 1)
                {
                    break;
                }
                resto = total - (end - line) - 1;
                length = end - line;
                if (ejecutando == numproc)
                {
                    wait(&status);
                    ejecutando--;
                }
                ejecutando++;
                char *l = malloc(sizeof(char) * length);
                memcpy(l, line, length);
                ejecutar_linea(l, length);
                free(l);
                if (resto > 0)
                {
                    memcpy(line, end + 1, resto);
                }
                total = total - length - 1;
                length = resto;
            }
            else
            {
                break;
            }
        }
        if (ejecutado == 0)
        {
            memcpy(line, line + length, resto);
            length = resto;
        }
    }

    if (length > 0)
    {
        if (ejecutando == numproc)
        {
            wait(&status);
            ejecutando--;
        }
        ejecutando++;
        char *l = malloc(sizeof(char) * length);
        memcpy(l, line, length);
        ejecutar_linea(l, length);
        free(l);
    }

    while (ejecutando > 0)
    {
        wait(&status);
        ejecutando--;
    }

    free(line);
    free(buf);
}