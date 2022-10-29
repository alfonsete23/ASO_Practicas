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

// Prototipos de funciones
void imprimir_uso();
int leer_linea(int fd, int bufsize, char *buf);
void shift_array(int *files, int file_count, int index);
ssize_t writeall(int fd, void *buf, size_t size);
int find_line(int num_read, char *buf);

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
        fprintf(stderr, "Error: Tamaño de buffer incorrecto.\n");
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
        fprintf(stderr, "Error: No hay ficheros de entrada.\n");
        imprimir_uso();
        exit(EXIT_FAILURE);
    }

    // Comprobar que no hay más de 16 ficheros de entrada
    if (argc - optind > 16)
    {
        fprintf(stderr, "Error: Demasiados ficheros de entrada. Máximo 16 ficheros.\n");
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
            fprintf(stderr, "Aviso: No se puede abrir \'%s\': No such file or directory\n", argv[i]);
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
        fprintf(stderr, "Error: No hay ficheros de entrada.\n");
        imprimir_uso();
    }

    // Reservar memoria para buffer de lectura
    char *buf = malloc(sizeof(char) * bufsize);
    // Tamaño de la línea leída
    int line_size;
    // Índice del fichero a leer
    int index = 0;
    // Número de bytes escritos por writeall
    ssize_t num_written;

    // Bucle para leer líneas de ficheros mientras queden ficheros
    while (file_count > 0)
    {
        // Leer una línea del fichero indicado y escribirla en la salida estándar
        int line_size = leer_linea(files[index], bufsize, buf);
        // Si el tamaño es 0, es que hemos acabado ese fichero
        if (line_size == 0)
        {
            // Eliminamos el descriptor del fichero que hemos terminado de leer
            shift_array(files, file_count, index);
            // Decrementamos el número de ficheros
            file_count--;
            // Decrementamos el índice, ya que todos los elementos del array de descriptores a partir del eliminado han pasado una posición hacia atrás
            index--;
        }
        if (file_count != 0)
        {
            // Actualizar el índice para recorrer todos los ficheros de forma cíclica
            index = (index + 1) % file_count;
        }
    }
    // Liberar memoria
    free(buf);
    free(files);
}

void imprimir_uso()
{
    fprintf(stderr, "Uso: ./merge_files [-t BUFSIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\nNo admite lectura de la entrada estandar.\n-t BUFSIZE\tTamaño de buffer donde 1 <= BUFSIZE <= 128MB\n-o FILEOUT\tUsa FILEOUT en lugar de la salida estandar\n");
}

// Esta función lee una línea del fichero representado por fd, en bloques de tamaño bufsize
int leer_linea(int fd, int bufsize, char *buf)
{
    int length = 0;
    // Número de bytes leídos y número de bytes leídos que pertenecen a la línea que estamos leyendo
    int num_read, read_in_line;
    // Puntero que apunta a la ocurrencia del carácter '\n'
    char *end;

    // Leer del fichero hasta encontrar un salto de línea
    while ((num_read = read(fd, buf, bufsize)))
    {
        // Al encontrar un salto de línea
        if ((read_in_line = (find_line(num_read, buf))) != -1)
        {
            // Actualizar la cantidad de bytes que pertenecen a la línea de lo que hemos leído en el buffer

            // Hacer que el offset del descriptor de fichero apunte al siguiente carácter tras el salto de línea, para la próxima vez que leamos
            lseek(fd, read_in_line - num_read, SEEK_CUR);
            // Insertamos lo leído en el buffer hasta el salto de línea
            length += writeall(STDOUT_FILENO, buf, read_in_line);
            // Devolver line, el proceso principal se encargará de extraer la cadena de caracteres
            return length;
        }
        // Si no se encuentra salto de línea, simplemente insertamos lo leído en el buffer dentro de la línea
        length += writeall(STDOUT_FILENO, buf, num_read);
    }
    return length;
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

int find_line(int num_read, char *buf)
{
    for (int i = 0; i < num_read; i++)
    {
        if (buf[i] == 0x0a)
        {
            return i + 1;
        }
    }
    return -1;
}