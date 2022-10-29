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

// Inicializar la estructura Line
void initLine(Line *line, size_t size)
{
    line->dir = calloc(sizeof(char), size * sizeof(char));
    line->used = 0;
    line->size = size;
}

// Insertar los "num_read" primeros bytes de buf en la cadena guardada en la estructura Line
void insertLine(Line *line, char *buf, int num_read)
{
    // Si el tamaño tras la adición supera al tamaño reservado, usamos realloc para aumentar el tamaño
    if ((line->used + num_read) > line->size)
    {
        line->size *= 2;
        line->dir = realloc(line->dir, line->size * sizeof(char));
    }
    // Copiamos lo leído en el buffer en la cadena
    memcpy(line->dir + line->used, buf, num_read);
    // Actualizar el tamaño de la cadena
    line->used += num_read;
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
void imprimir_uso();
Line leer_linea(int fd, int bufsize);
void shift_array(int *files, int file_count, int index);
ssize_t writeall(int fd, void *buf, size_t size);

int main(int argc, char **argv)
{
    int opt, fd, bufsize = 1024;
    optind = 1;
    char *fileout = NULL;

    // Lectura de parámetros
    while ((opt = getopt(argc, argv, "t:o:h")) != -1)
    {
        switch (opt)
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
        exit(EXIT_FAILURE);
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

    // Tamaño de la línea leída
    int line_size;
    // Índice del fichero a leer
    int index = 0;
    // Número de bytes escritos por writeall
    ssize_t num_written;

    // Bucle para leer líneas de ficheros mientras queden ficheros
    while (file_count > 0)
    {
        // Leer una línea del fichero indicado
        Line line = leer_linea(files[index], bufsize);
        // Guardar el tamaño de la línea leída
        line_size = getSize(&line);
        // Si el tamaño es 0, es que hemos acabado ese fichero
        if (line_size == 0)
        {
            // Liberar memoria
            freeLine(&line);
            // Eliminamos el descriptor del fichero que hemos terminado de leer
            shift_array(files, file_count, index);
            // Decrementamos el número de ficheros
            file_count--;
            // Decrementamos el índice, ya que todos los elementos del array de descriptores a partir del eliminado han pasado una posición hacia atrás
            index--;
        }
        else
        {
            // Obtenemos la línea leída
            char *l = getLine(&line);
            // La escribimos en la salida estándar, que podrá estar o no redirigida TODO: (tenemos que tener en cuenta las escrituras parciales)
            num_written = writeall(STDOUT_FILENO, l, line_size);
            if (num_written == -1)
            {
                fprintf(stderr, "write()");
                exit(EXIT_FAILURE);
            }
            // Liberar memoria
            freeLine(&line);
        }
        if (file_count != 0)
        {
            // Actualizar el índice para recorrer todos los ficheros de forma cíclica
            index = (index + 1) % file_count;
        }
    }
    // Liberar memoria
    free(files);
}

void imprimir_uso()
{
    fprintf(stderr, "Uso: ./merge_files [-t BUFSIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\nNo admite lectura de la entrada estandar.\n-t BUFSIZE Tamaño de buffer donde 1 <= BUFSIZE <= 128MB\n-o FILEOUT Usa FILEOUT en lugar de la salida estandar\n");
}

// Esta función lee una línea del fichero representado por fd, en bloques de tamaño bufsize
Line leer_linea(int fd, int bufsize)
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
    initLine(&line, bufsize * 2);

    // Reservar memoria para el buffer de lectura
    if ((buf = (char *)malloc(bufsize * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    // Leer del fichero hasta encontrar un salto de línea
    while ((num_read = read(fd, buf, bufsize)))
    {
        // Al encontrar un salto de línea
        if ((end = (strchr(buf, '\n'))) != NULL)
        {
            // Actualizar la cantidad de bytes que pertenecen a la línea de lo que hemos leído en el buffer
            read_in_line = end - buf + 1;
            // Hacer que el offset del descriptor de fichero apunte al siguiente carácter tras el salto de línea, para la próxima vez que leamos
            lseek(fd, read_in_line - num_read, SEEK_CUR);
            // Insertamos lo leído en el buffer hasta el salto de línea
            insertLine(&line, buf, read_in_line);
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

// Función para mover los elementos del array a partir de un índice una posición hacia abajo
void shift_array(int *files, int file_count, int index)
{
    for (int i = index; i < file_count - 1; i++)
    {
        files[i] = files[i + 1];
    }
}

// Función para el tratamiento de las escrituras parciales
ssize_t writeall(int fd, void *buf, size_t size)
{
    ssize_t num_written;
    int num_written_total = 0;

    while (num_written_total < size && (num_written = write(fd, buf + num_written_total, size - num_written_total)) > 0)
    {
        num_written_total += num_written;
    }

    return num_written == -1 ? -1 : num_written_total;
}