#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BLOCK_SIZE 16
#define LINE_SIZE 128

// Función para ejecutar una línea de comandos
void ejecutar_linea(char *line)
{
    // Iniciar proceso hijo para procesar la línea leída
    char *arr[LINE_SIZE];
    int i;
    char *p;
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
        // Actualizar buf con el resto del bloque leído
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
                fprintf(stderr, "Error: el número de procesos en ejecución tiene que estar entre 1 y 8\nUso: %s [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes para ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p   NUMPROC Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n", argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        // Opción -h devuelve un mensaje de ayuda con las instrucciones de uso del programa
        case 'h':
            fprintf(stderr, "Uso: %s [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes para ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p   NUMPROC Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n", argv[0]);
            exit(EXIT_SUCCESS);
        // Cualquier otro argumento provoca un error e imprime las instrucciones de uso
        default:
            fprintf(stderr, "Uso: %s [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes para ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p   NUMPROC Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Uso: %s [-p NUMPROC]\nLee de la entrada estándar una secuencia de líneas conteniendo órdenes para ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n-p   NUMPROC Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    

    // TODO: Implementar lectura de líneas entrada estándar usando solamente read()
    char *buf = malloc(BLOCK_SIZE);
    char *aux = malloc(BLOCK_SIZE);
    char *line = malloc(LINE_SIZE);
    int num_read, length = 0;
    int line_executed;
    char *end;
    int repeated = 0;
    int restantes = numproc;

    int status;
    // Leer de la entrada estándar
    while ((num_read = read(STDIN_FILENO, buf, BLOCK_SIZE)) > 0)
    {
        line_executed = 0;
        repeated = 0;
        // Si la línea supera la longitud de 128 bytes, error.
        if ((length + num_read) > LINE_SIZE)
        {
            fprintf(stderr, "Error. Tamaño de línea mayor que 128\n");
            exit(EXIT_FAILURE);
        }

        // Si en el bloque leído hay uno o más saltos de línea, reservamos lo leído de la siguiente y procesamos la línea terminada
        while ((end = strchr(buf, '\n')))
        {
            if (!repeated)
            {
                memcpy(line + length, buf, end - buf);
                repeated = 0;

            }
            // Comprobar que no hay más procesos activos de los que deben
            if (restantes < 1)
            {
                if (wait(&status) == -1)
                {
                    perror("wait()");
                    exit(EXIT_FAILURE);
                }

                restantes++;
            }

            restantes--;
            ejecutar_linea(line);
            length = 0;
            num_read = num_read - (end - buf);
            memcpy(aux, end + 1, num_read);
            memset(buf, 0, BLOCK_SIZE);
            memset(line, 0, LINE_SIZE);
            memcpy(buf, aux, num_read);
            if ((end = strchr(buf, '\n')))
            {
                repeated = 1;
                num_read = end - buf;
            }
            memcpy(line, buf, num_read);
            length += num_read;
            line_executed = 1;
        }
        if (!line_executed)
        {
            // Añadir lo leído en el buffer a la línea que se está leyendo
            memcpy(line + length, buf, num_read);
            // Aumentar la longitud de la línea
            length += num_read;
        }
    }

    // Tratamiento de la última línea si la entrada no acaba con un salto de línea
    if (restantes < 1)
    {
        if (wait(&status) == -1)
        {
            perror("wait()");
            exit(EXIT_FAILURE);
        }
        restantes++;
    }

    if (line[0] != 0)
    {
        ejecutar_linea(line);
    }


    // Espera de procesos que pueden seguir en ejecución
    while (restantes < numproc)
    {
        if (wait(&status) == -1)
        {
            perror("wait()");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status))
        {
            restantes++;
        }
    }

    // Liberación de memoria
    free(buf);
    free(aux);
    free(line);
}
