#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BLOCK_SIZE 16
#define LINE_SIZE 128

int ejecutando = 0;

// Estructura para leer líneas de forma dinámica, con el tamaño de la línea duplicándose cada vez que se lee
typedef struct
{
    char *dir;
    size_t used;
    size_t size;
} Line;

// Inicializar la estructura Line
void initLine(Line *line)
{
    line->dir = calloc(sizeof(char) * LINE_SIZE, LINE_SIZE);
    line->used = 0;
    line->size = LINE_SIZE;
}

// Insertar los "num_read" primeros bytes de buf en la cadena guardada en la estructura Line
int insertLine(Line *line, char *buf, int num_read)
{
    // Si el tamaño tras la adición supera al tamaño reservado, devolvemos el error y paramos la ejecución
    if ((line->used + num_read) > line->size)
    {
        return -1;
    }
    // Copiamos lo leído en el buffer en la cadena
    memcpy(line->dir + line->used, buf, num_read);
    // Actualizar el tamaño de la cadena
    line->used += num_read;
    return 0;
}

// Devuelve la cadena almacenada
char *getLine(Line *line)
{
    return line->dir;
}

// Devuelve el tamaño de la cadena
size_t getSize(Line *line)
{
    return line->used;
}

// Liberar memoria
void freeLine(Line *line)
{
    free(line->dir);
    line->used = 0;
    line->size = 0;
}

// Prototipos de funciones
Line leer_linea(int fd);
void ejecutar_linea(char *l);

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

    char *l;
    Line line;
    int num_read, length = 0;
    char *end;
    int line_size;
    int status;

    // Leer de la entrada estándar
    while (1)
    {
        // Leer línea
        line = leer_linea(STDIN_FILENO);
        if ((line_size = getSize(&line)) == 0)
        {
            freeLine(&line);
            break;
        }
        l = getLine(&line);
        if (ejecutando == numproc)
        {
            if (wait(&status) == -1)
            {
                perror("wait()");
                exit(EXIT_FAILURE);
            }

            if (WIFEXITED(status))
            {
                ejecutando--;
            }
        }
        printf("%s", l);
        // ejecutar_linea(l);
        freeLine(&line);
    }


    // Espera de procesos que pueden seguir en ejecución
    while (ejecutando > 0)
    {
        if (wait(&status) == -1)
        {
            perror("wait()");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status))
        {
            ejecutando--;
        }
    }
}

// Esta función lee una línea del fichero representado por fd, en bloques de tamaño bufsize
Line leer_linea(int fd)
{
    // Buffer de lectura
    char *buf;
    // Número de bytes leídos y número de bytes leídos que pertenecen a la línea que estamos leyendo
    int num_read, read_in_line;
    // Line es una especie de array dinámico que duplica su tamaño cada vez que lo leído supere al tamaño reservado en memoria
    Line line;
    // Puntero que apunta a la ocurrencia del carácter '\n'
    char *end;

    // Inicializar line con un tamaño igual al doble del tamaño del buffer de lectura
    initLine(&line);

    // Reservar memoria para el buffer de lectura
    if ((buf = (char *)malloc(BLOCK_SIZE * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    // Leer del fichero hasta encontrar un salto de línea
    while ((num_read = read(fd, buf, BLOCK_SIZE)))
    {
        // Al encontrar un salto de línea
        if ((end = (strchr(buf, '\n'))) != NULL)
        {
            // Actualizar la cantidad de bytes que pertenecen a la línea de lo que hemos leído en el buffer
            read_in_line = end - buf;
            // Hacer que el offset del descriptor de fichero apunte al siguiente carácter tras el salto de línea, para la próxima vez que leamos
            lseek(fd, read_in_line - num_read, SEEK_CUR);
            // Insertamos lo leído en el buffer hasta el salto de línea
            if (insertLine(&line, buf, read_in_line) == -1)
            {
                free(buf);
                freeLine(&line);
                fprintf(stderr, "Error. Tamaño de línea mayor que 128\n");
                exit(EXIT_FAILURE);
            }
            // Liberar memoria del buffer
            free(buf);
            // Devolver line, el proceso principal se encargará de extraer la cadena de caracteres
            return line;
        }
        // Si no se encuentra salto de línea, simplemente insertamos lo leído en el buffer dentro de la línea
        insertLine(&line, buf, num_read);
    }
    // Liberar memoria del buffer
    free(buf);
    // Devolver line
    return line;
}

// Función para ejecutar una línea de comandos
void ejecutar_linea(char *l)
{
    ejecutando++;
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
        p = strtok(l, " ");
        while (p != NULL)
        {
            arr[i++] = p;
            p = strtok(NULL, " ");
        }
        arr[i] = NULL;
        execvp(arr[0], arr);
        perror("execvp()");
        exit(EXIT_FAILURE);

    default:
        return;
    }
}