#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MIN_BUF_SIZE 1
#define MAX_BUF_SIZE 134217728

void imprimir_uso()
{
    fprintf(stderr, "Uso: ./merge_tee_exec -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]\nNo admite lectura de la entrada estandar.\n-t BUFSIZE\tTamaño de buffer donde 1 <= BUFSIZE <= 128MB\n-l LOGFILE\tNombre del archivo de log.\n-p NUMPROC\tNúmero de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n");
}

int main(int argc, char **argv)
{
    int opt;
    char *bufsize = "1024";
    char *numproc = "1";
    optind = 1;
    char *logfile = NULL;

    // Lectura de parámetros
    while ((opt = getopt(argc, argv, "l:t:p:h")) != -1)
    {
        switch (opt)
        {
        case 'l':
            logfile = optarg;
            break;
        case 't':
            bufsize = optarg;
            break;
        case 'p':
            numproc = optarg;
            break;
        case 'h':
            imprimir_uso();
            exit(EXIT_SUCCESS);
            break;
        default:
            imprimir_uso();
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (logfile == NULL)
    {
        fprintf(stderr, "Error: No hay fichero de log.\n");
        imprimir_uso();
        exit(EXIT_FAILURE);
    }

    if (atoi(bufsize) < MIN_BUF_SIZE || atoi(bufsize) > MAX_BUF_SIZE)
    {
        fprintf(stderr, "Error: Tamaño de buffer incorrecto.\n");
        imprimir_uso();
        exit(EXIT_FAILURE);
    }

    if (atoi(numproc) < 1 || atoi(numproc) > 8)
    {
        fprintf(stderr, "Error: El número de procesos en ejecución tiene que estar entre 1 y 8.\n");
        imprimir_uso();
        exit(EXIT_FAILURE);
    }

    if (optind == argc)
    {
        fprintf(stderr, "Error: No hay ficheros de entrada.\n");
        imprimir_uso();
        exit(EXIT_FAILURE);
    }

    if (argc - optind > 16)
    {
        fprintf(stderr, "Error: Demasiados ficheros de entrada. Máximo 16 ficheros.\nUso: ./merge_files [-t BUFSIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\nNo admite lectura de la entrada estandar.\n-t BUFSIZE\tTamaño de buffer donde 1 <= BUFSIZE <= 128MB\n-o FILEOUT\tUsa FILEOUT en lugar de la salida estandar\n");
        exit(EXIT_FAILURE);
    }

    int pipefds1[2];
    int pipefds2[2];

    if (pipe(pipefds1) == -1)
    {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    if (pipe(pipefds2) == -1)
    {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    // Creación del hijo que ejecutará "merge_files"
    switch (fork())
    {
    case -1:
        perror("fork(1)");
        exit(EXIT_FAILURE);
        break;
    case 0:
        // Cerrar el extremo de lectura primera tubería
        if (close(pipefds1[0]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }

        // Cerrar ambos extremos segunda tubería
        if (close(pipefds2[0]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }

        if (close(pipefds2[1]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }

        // Redirigir salida estándar a extremo de escritura de la primera tubería
        if (dup2(pipefds1[1], STDOUT_FILENO) == -1)
        {
            perror("dup2(1)");
            exit(EXIT_FAILURE);
        }
        // Cerrar descriptor duplicado
        if (close(pipefds1[1]) == -1)
        {
            perror("close(2)");
            exit(EXIT_FAILURE);
        }
        // Reemplazar binario actual por el de "merge_files"
        int index = 3;
        char *args[21];
        args[0] = "./merge_files";
        args[1] = "-t";
        args[2] = bufsize;
        for (int i = optind; i < argc; i++)
        {
            args[index] = argv[i];
            index++;
        }
        args[index] = NULL;
        execvp(args[0], args);
        perror("execlp(1)");
        exit(EXIT_FAILURE);
        break;
    default:
        break;
    }

    // Creación del hijo que ejecutará "tee"
    switch (fork())
    {
    case -1:
        perror("fork(2)");
        exit(EXIT_FAILURE);
        break;
    case 0:
        // Cerrar extremos de tuberías no utilizados
        // Primera tubería: extremo de escritura
        if (close(pipefds1[1]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }
        // Segunda tubería: extremo de lectura
        if (close(pipefds2[0]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }
        // Redirigir entrada estándar a extremo de lectura de la primera tubería
        if (dup2(pipefds1[0], STDIN_FILENO) == -1)
        {
            perror("dup2(2)");
            exit(EXIT_FAILURE);
        }
        // Cerrar descriptor duplicado
        if (close(pipefds1[0]) == -1)
        {
            perror("close(3)");
            exit(EXIT_FAILURE);
        }
        // Redirigir salida estándar a extremo de escritura de la segunda tubería
        if (dup2(pipefds2[1], STDOUT_FILENO) == -1)
        {
            perror("dup2(3)");
            exit(EXIT_FAILURE);
        }
        // Cerrar descriptor duplicado
        if (close(pipefds2[1]) == -1)
        {
            perror("close(4)");
            exit(EXIT_FAILURE);
        }
        // Reemplazar binario actual por el de "tee"
        execlp("tee", "tee", logfile, NULL);
        perror("execlp(2)");
        exit(EXIT_FAILURE);
        break;
    default:
        break;
    }

    // Creación del hijo que ejecutará "exec_lines"
    switch (fork())
    {
    case -1:
        perror("fork(3)");
        exit(EXIT_FAILURE);
        break;
    case 0:
        // Cerrar ambos extremos primera tubería
        if (close(pipefds1[0]) == -1)
        {
            perror("close(5)");
            exit(EXIT_FAILURE);
        }
        if (close(pipefds1[1]) == -1)
        {
            perror("close(6)");
            exit(EXIT_FAILURE);
        }
        // Cerrar extremo de escritura de la segunda tubería
        if (close(pipefds2[1]) == -1)
        {
            perror("close(7)");
            exit(EXIT_FAILURE);
        }
        // Redirigir entrada estándar al extremo de lectura de la segunda tubería
        if (dup2(pipefds2[0], STDIN_FILENO) == -1)
        {
            perror("dup2(4)");
            exit(EXIT_FAILURE);
        }
        // Cerrar descriptor duplicado
        if (close(pipefds2[0]) == -1)
        {
            perror("close(8)");
            exit(EXIT_FAILURE);
        }
        // Remplazar binario por el de "exec_lines"
        execlp("./exec_lines", "./exec_lines", "-p", numproc);
        perror("execlp(3)");
        exit(EXIT_FAILURE);
        break;
    default:
        break;
    }

    // Cerrar descriptores no usados por el padre
    if (close(pipefds1[0]) == -1)
    {
        perror("close(9)");
        exit(EXIT_FAILURE);
    }

    if (close(pipefds1[1]) == -1)
    {
        perror("close(10)");
        exit(EXIT_FAILURE);
    }

    if (close(pipefds2[0]) == -1)
    {
        perror("close(11)");
        exit(EXIT_FAILURE);
    }

    if (close(pipefds2[1]) == -1)
    {
        perror("close(12)");
        exit(EXIT_FAILURE);
    }
    // Esperar a que terminen los procesos hijo
    if (wait(NULL) == -1)
    {
        perror("wait(1)");
        exit(EXIT_FAILURE);
    }
    if (wait(NULL) == -1)
    {
        perror("wait(2)");
        exit(EXIT_FAILURE);
    }
    if (wait(NULL) == -1)
    {
        perror("wait(3)");
        exit(EXIT_FAILURE);
    }
}