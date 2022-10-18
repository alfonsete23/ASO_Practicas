#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MIN_BUFSIZE 1
#define MAX_BUFSIZE 134217728
#define MAX_FILES 16


// Estructura para leer líneas de forma dinámica, con el tamaño de la línea duplicándose cada vez que se lee 
typedef struct
{
    char *dir;
    size_t used;
    size_t size;
} Line;

void initLine(Line *line, size_t size)
{
    line->dir = calloc(sizeof(char), size * sizeof(char));
    line->used = 0;
    line->size = size;
}

void insertLine(Line *line, char *buf, int num_read)
{
    if ((line->used + num_read) > line->size)
    {
        line->size *= 2;
        line->dir = realloc(line->dir, line->size * sizeof(char));
    }
    memcpy(line->dir + line->used, buf, num_read);
    line->used += num_read;
}

char *getLine(Line *line)
{
    return line->dir;
}

size_t getSize(Line *line)
{
    return line->used;
}

void freeLine(Line *line)
{
    free(line->dir);
    line->used = 0;
    line->size = 0;
}


// Función para imprimir uso del programa por la salida estándar de error
void imprimir_uso();
Line leer_linea(int fd, int bufsize);

int main(int argc, char **argv)
{
    int opt, fd, bufsize = 1024;
    optind = 1;
    char *fileout = NULL;

    // Lectura de parámetros
    while ((opt = getopt(argc, argv, "t:o:h")) != -1)
    {
        switch(opt)
        {
            case 't':
                bufsize = atoi(optarg);
                break;
            case 'o':
                fileout = optarg;
                break;
            case 'h':
                imprimir_uso();
                exit(EXIT_SUCCESS);
            default:
                imprimir_uso();
                exit(EXIT_FAILURE);
        }
    }

    // Procesamiento y comprobación de parámetros
    // Comprobar que bufsize sea correcto
    if (bufsize < MIN_BUFSIZE || bufsize > MAX_BUFSIZE)
    {
        fprintf(stderr, "Error. Tamaño de buffer incorrecto.\n");
        imprimir_uso();
    }

    // Redirigir salida estándar a fileout si es necesario
    if (fileout)
    {
        if (close(STDOUT_FILENO) == -1)
        {
            perror("close()");
            exit(EXIT_FAILURE);
        }

        if ((fd = open(fileout, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU)) == -1)
        {
            perror("open()");
            exit(EXIT_FAILURE);
        }
    }

    // Comprobar que hay al menos un fichero de entrada
    if (optind == argc)
    {
        fprintf(stderr, "Error. No hay ficheros de entrada\n");
        imprimir_uso();
        exit(EXIT_FAILURE);
    }

    // Comprobar que no hay más de 16 ficheros de entrada
    if (argc - optind > 16)
    {
        fprintf(stderr, "Error. Demasiados ficheros de entrada. Máximo 16 ficheros.\n");
        imprimir_uso();
        exit(EXIT_FAILURE);
    }

    // Array para guardar los descriptores de cada uno de los ficheros de entrada
    int *files = malloc(sizeof(int) * 16);

    // Contador para el número de ficheros
    int file_count = 0;

    // Abrir dichos ficheros para su posterior lectura
    for (int i = optind; i < argc; i++)
    {
        if ((fd = open(argv[i], O_RDONLY)) == -1)
        {
            fprintf(stderr, "El fichero %s no se ha podido abrir\n", argv[i]);
        }
        else
        {
            files[file_count] = fd;
            file_count++;
        }
    }

    // Si ningún fichero se ha podido abrir, acabar la ejecución
    if (file_count == 0)
    {
        fprintf(stderr, "Error. No hay ficheros válidos de entrada\n");
        imprimir_uso();
    }
    
    // TODO: Función para leer una línea de un fichero en bloques de BUFSIZE
    int line_size;
    int index = 0;

    while (1)
    {
        Line line = leer_linea(fd, bufsize);
        line_size = getSize(&line);
        if (line_size == 0)
        {
            freeLine(&line);
            break;
        }
        else
        {
            char *l = getLine(&line);
            printf("%s", l);
            freeLine(&line);
        }
    }
    free(files);
    // TODO: Función para eliminar un fichero del array y mover las posiciones de todos los demás
    // TODO: Bucle para ir llamando a la función lectora de líneas, contando la línea por la que vamos, llamando a la función que elimina el fichero si encontramos en este EOF
}

void imprimir_uso()
{
    fprintf(stderr, "Uso: ./merge_files [-t BUFSIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\nNo admite lectura de la entrada estandar.\n-t BUFSIZE Tamaño de buffer donde 1 <= BUFSIZE <= 128MB\n-o FILEOUT Usa FILEOUT en lugar de la salida estandar\n");
}

Line leer_linea(int fd, int bufsize)
{
    char *buf;
    int num_read, read_in_line;
    Line line;
    char *end;
    int length;

    initLine(&line, bufsize * 2);
    
    if ((buf = (char *) malloc(bufsize * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    while ((num_read = read(fd, buf, bufsize)))
    {
        if ((end = (strchr(buf, '\n'))) != NULL)
        {
            read_in_line = end - buf + 1;
            lseek(fd, read_in_line - num_read, SEEK_CUR);
            insertLine(&line, buf, read_in_line);
            free(buf);
            return line;
        }
        insertLine(&line, buf, num_read);
    }
    free(buf);
    return line;
}