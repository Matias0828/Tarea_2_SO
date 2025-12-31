/* Tarea de Programación 2. Construcción de 
indice invertido con Threads

Autor: Matias Muñoz Catalán
Curso: Sistemas operativos
Profesora: Victoria Flores 

Descripción del problema: Implementar en C que lea un archivo de texto plano que represente un libro.
El archivo estará dividido en páginas, donde tres saltos de línea consecutivos indica  el inicio de 
una nueva página.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_LONGITUD_PALABRA 100
#define MAX_PAGINAS 1000
#define MAX_PALABRAS_BUSCAR 10000

// Estructura para una entrada de palabra en el índice
typedef struct EntradaPalabra {
    char palabra[MAX_LONGITUD_PALABRA];
    int *paginas;
    int *conteos;
    int num_paginas;
    int capacidad;
    struct EntradaPalabra *siguiente;
} EntradaPalabra;

// Estructura para el índice invertido global
typedef struct {
    EntradaPalabra *cabeza;
    pthread_mutex_t mutex;
} IndiceInvertido;

// Estructura para almacenar las palabras a buscar
typedef struct {
    char palabras[MAX_PALABRAS_BUSCAR][MAX_LONGITUD_PALABRA];
    int cantidad;
    pthread_mutex_t mutex;
} ListaPalabras;

// Estructura para pasar argumentos a cada thread
typedef struct {
    char *texto_pagina;
    int num_pagina;
    IndiceInvertido *indice_global;
    ListaPalabras *lista_palabras;
    int longitud_minima_palabra;
} ArgumentosThread;

// Variables globales
IndiceInvertido indice_global;
ListaPalabras palabras_buscar;

// Función para inicializar el índice invertido
void inicializar_indice(IndiceInvertido *indice) {
    indice->cabeza = NULL;
    pthread_mutex_init(&indice->mutex, NULL);
}

// Función para inicializar la lista de palabras
void inicializar_lista_palabras(ListaPalabras *lista) {
    lista->cantidad = 0;
    pthread_mutex_init(&lista->mutex, NULL);
}

// Función para normalizar una palabra (convertir a minúsculas)
void normalizar_palabra(char *palabra) {
    for (int i = 0; palabra[i]; i++) {
        palabra[i] = tolower(palabra[i]);
    }
}

// Función para verificar si una palabra está en la lista de búsqueda
int palabra_en_lista(ListaPalabras *lista, const char *palabra) {
    for (int i = 0; i < lista->cantidad; i++) {
        if (strcmp(lista->palabras[i], palabra) == 0) {
            return 1;
        }
    }
    return 0;
}

// Función para cargar palabras desde archivo
int cargar_palabras_buscar(const char *archivo, ListaPalabras *lista) {
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        perror("Error al abrir archivo de palabras");
        return 0;
    }
    
    char linea[MAX_LONGITUD_PALABRA];
    while (fgets(linea, sizeof(linea), fp) && lista->cantidad < MAX_PALABRAS_BUSCAR) {
        // Eliminar salto de línea
        linea[strcspn(linea, "\n")] = 0;
        
        // Ignorar líneas vacías
        if (strlen(linea) == 0) continue;
        
        // Normalizar y agregar
        normalizar_palabra(linea);
        strcpy(lista->palabras[lista->cantidad], linea);
        lista->cantidad++;
    }
    
    fclose(fp);
    printf("Se cargaron %d palabras para buscar\n", lista->cantidad);
    return 1;
}

// Función para agregar o actualizar una palabra en el índice
void agregar_palabra_al_indice(IndiceInvertido *indice, const char *palabra, int num_pagina) {
    pthread_mutex_lock(&indice->mutex);
    
    EntradaPalabra *actual = indice->cabeza;
    EntradaPalabra *previo = NULL;
    
    // Buscar si la palabra ya existe
    while (actual != NULL) {
        int comparacion = strcmp(palabra, actual->palabra);
        if (comparacion == 0) {
            // Palabra encontrada, buscar si la página ya existe
            int pagina_encontrada = 0;
            for (int i = 0; i < actual->num_paginas; i++) {
                if (actual->paginas[i] == num_pagina) {
                    actual->conteos[i]++;
                    pagina_encontrada = 1;
                    break;
                }
            }
            
            // Si la página no existe, agregarla
            if (!pagina_encontrada) {
                if (actual->num_paginas >= actual->capacidad) {
                    actual->capacidad *= 2;
                    actual->paginas = realloc(actual->paginas, actual->capacidad * sizeof(int));
                    actual->conteos = realloc(actual->conteos, actual->capacidad * sizeof(int));
                }
                actual->paginas[actual->num_paginas] = num_pagina;
                actual->conteos[actual->num_paginas] = 1;
                actual->num_paginas++;
            }
            
            pthread_mutex_unlock(&indice->mutex);
            return;
        } else if (comparacion < 0) {
            break; // Insertar antes de actual
        }
        previo = actual;
        actual = actual->siguiente;
    }
    
    // Crear nueva entrada
    EntradaPalabra *nueva_entrada = malloc(sizeof(EntradaPalabra));
    strcpy(nueva_entrada->palabra, palabra);
    nueva_entrada->capacidad = 10;
    nueva_entrada->paginas = malloc(nueva_entrada->capacidad * sizeof(int));
    nueva_entrada->conteos = malloc(nueva_entrada->capacidad * sizeof(int));
    nueva_entrada->paginas[0] = num_pagina;
    nueva_entrada->conteos[0] = 1;
    nueva_entrada->num_paginas = 1;
    
    // Insertar en orden alfabético
    if (previo == NULL) {
        nueva_entrada->siguiente = indice->cabeza;
        indice->cabeza = nueva_entrada;
    } else {
        nueva_entrada->siguiente = actual;
        previo->siguiente = nueva_entrada;
    }
    
    pthread_mutex_unlock(&indice->mutex);
}

// Función que ejecuta cada thread
void *procesar_pagina(void *arg) {
    ArgumentosThread *args = (ArgumentosThread *)arg;
    char *texto = args->texto_pagina;
    int num_pagina = args->num_pagina;
    int longitud_min = args->longitud_minima_palabra;
    
    char palabra[MAX_LONGITUD_PALABRA];
    int indice_palabra = 0;
    
    // Tokenizar y procesar cada palabra
    for (int i = 0; texto[i] != '\0'; i++) {
        if (isalnum(texto[i])) {
            if (indice_palabra < MAX_LONGITUD_PALABRA - 1) {
                palabra[indice_palabra++] = texto[i];
            }
        } else {
            if (indice_palabra > 0) {
                palabra[indice_palabra] = '\0';
                
                // Filtrar por longitud mínima
                if (indice_palabra >= longitud_min) {
                    normalizar_palabra(palabra);
                    
                    // Solo agregar si está en la lista de búsqueda
                    if (palabra_en_lista(args->lista_palabras, palabra)) {
                        agregar_palabra_al_indice(args->indice_global, palabra, num_pagina);
                    }
                }
                
                indice_palabra = 0;
            }
        }
    }
    
    // Procesar última palabra si existe
    if (indice_palabra > 0) {
        palabra[indice_palabra] = '\0';
        if (indice_palabra >= longitud_min) {
            normalizar_palabra(palabra);
            if (palabra_en_lista(args->lista_palabras, palabra)) {
                agregar_palabra_al_indice(args->indice_global, palabra, num_pagina);
            }
        }
    }
    
    free(args->texto_pagina);
    free(args);
    
    return NULL;
}

// Función para ordenar las páginas de cada palabra
void ordenar_paginas(IndiceInvertido *indice) {
    EntradaPalabra *actual = indice->cabeza;
    while (actual != NULL) {
        // Crear array temporal para ordenar
        int *paginas_temp = malloc(actual->num_paginas * sizeof(int));
        int *conteos_temp = malloc(actual->num_paginas * sizeof(int));
        
        for (int i = 0; i < actual->num_paginas; i++) {
            paginas_temp[i] = actual->paginas[i];
            conteos_temp[i] = actual->conteos[i];
        }
        
        // Ordenamiento burbuja simple por páginas
        for (int i = 0; i < actual->num_paginas - 1; i++) {
            for (int j = 0; j < actual->num_paginas - i - 1; j++) {
                if (paginas_temp[j] > paginas_temp[j + 1]) {
                    int temp = paginas_temp[j];
                    paginas_temp[j] = paginas_temp[j + 1];
                    paginas_temp[j + 1] = temp;
                    
                    temp = conteos_temp[j];
                    conteos_temp[j] = conteos_temp[j + 1];
                    conteos_temp[j + 1] = temp;
                }
            }
        }
        
        memcpy(actual->paginas, paginas_temp, actual->num_paginas * sizeof(int));
        memcpy(actual->conteos, conteos_temp, actual->num_paginas * sizeof(int));
        
        free(paginas_temp);
        free(conteos_temp);
        
        actual = actual->siguiente;
    }
}

// Función para escribir el índice en archivo y pantalla
void escribir_indice(IndiceInvertido *indice, const char *archivo_salida) {
    FILE *archivo = fopen(archivo_salida, "w");
    if (!archivo) {
        perror("Error al abrir archivo de salida");
        return;
    }
    
    ordenar_paginas(indice);
    
    EntradaPalabra *actual = indice->cabeza;
    while (actual != NULL) {
        fprintf(archivo, "%s: ", actual->palabra);
        printf("%s: ", actual->palabra);
        
        for (int i = 0; i < actual->num_paginas; i++) {
            fprintf(archivo, "[%d,%d]", actual->paginas[i], actual->conteos[i]);
            printf("[%d,%d]", actual->paginas[i], actual->conteos[i]);
            
            if (i < actual->num_paginas - 1) {
                fprintf(archivo, ",");
                printf(",");
            }
        }
        
        fprintf(archivo, "\n");
        printf("\n");
        
        actual = actual->siguiente;
    }
    
    fclose(archivo);
}

// Función para liberar memoria del índice
void liberar_indice(IndiceInvertido *indice) {
    EntradaPalabra *actual = indice->cabeza;
    while (actual != NULL) {
        EntradaPalabra *siguiente = actual->siguiente;
        free(actual->paginas);
        free(actual->conteos);
        free(actual);
        actual = siguiente;
    }
    pthread_mutex_destroy(&indice->mutex);
}

int main(int argc, char *argv[]) {
    char *archivo_entrada = NULL;
    char *archivo_salida = NULL;
    char *archivo_palabras = NULL;
    int max_threads = 1;
    int longitud_minima_palabra = 3;
    
    // Parsear argumentos
    int opcion;
    while ((opcion = getopt(argc, argv, "f:o:t:w:p:")) != -1) {
        switch (opcion) {
            case 'f':
                archivo_entrada = optarg;
                break;
            case 'o':
                archivo_salida = optarg;
                break;
            case 't':
                max_threads = atoi(optarg);
                break;
            case 'w':
                longitud_minima_palabra = atoi(optarg);
                break;
            case 'p':
                archivo_palabras = optarg;
                break;
            default:
                fprintf(stderr, "Uso: %s -f <libro> -o <salida> -p <palabras> -t <threads> [-w <longitud_min>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    if (!archivo_entrada || !archivo_salida || !archivo_palabras) {
        fprintf(stderr, "Faltan argumentos requeridos: -f, -o y -p son obligatorios\n");
        exit(EXIT_FAILURE);
    }
    
    // Inicializar lista de palabras y cargarlas
    inicializar_lista_palabras(&palabras_buscar);
    if (!cargar_palabras_buscar(archivo_palabras, &palabras_buscar)) {
        fprintf(stderr, "Error al cargar palabras\n");
        exit(EXIT_FAILURE);
    }
    
    // Leer archivo
    FILE *archivo = fopen(archivo_entrada, "r");
    if (!archivo) {
        perror("Error al abrir archivo");
        exit(EXIT_FAILURE);
    }
    
    fseek(archivo, 0, SEEK_END);
    long tamanio_archivo = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);
    
    char *contenido = malloc(tamanio_archivo + 1);
    fread(contenido, 1, tamanio_archivo, archivo);
    contenido[tamanio_archivo] = '\0';
    fclose(archivo);
    
    // Dividir en páginas
    char *paginas[MAX_PAGINAS];
    int num_paginas = 0;
    
    char *inicio = contenido;
    char *puntero = contenido;
    int contador_saltos_linea = 0;
    
    while (*puntero && num_paginas < MAX_PAGINAS) {
        if (*puntero == '\n') {
            contador_saltos_linea++;
            if (contador_saltos_linea == 3) {
                int longitud = puntero - inicio - 2;
                paginas[num_paginas] = malloc(longitud + 1);
                strncpy(paginas[num_paginas], inicio, longitud);
                paginas[num_paginas][longitud] = '\0';
                num_paginas++;
                
                inicio = puntero + 1;
                contador_saltos_linea = 0;
            }
        } else {
            contador_saltos_linea = 0;
        }
        puntero++;
    }
    
    // Última página
    if (inicio < puntero && num_paginas < MAX_PAGINAS) {
        int longitud = puntero - inicio;
        paginas[num_paginas] = malloc(longitud + 1);
        strncpy(paginas[num_paginas], inicio, longitud);
        paginas[num_paginas][longitud] = '\0';
        num_paginas++;
    }
    
    free(contenido);
    
    printf("Se encontraron %d páginas en el libro\n", num_paginas);
    
    // Inicializar índice global
    inicializar_indice(&indice_global);
    
    // Crear threads
    int threads_reales = (num_paginas < max_threads) ? num_paginas : max_threads;
    pthread_t *threads = malloc(threads_reales * sizeof(pthread_t));
    int indice_thread = 0;
    
    for (int i = 0; i < num_paginas; i++) {
        ArgumentosThread *args = malloc(sizeof(ArgumentosThread));
        args->texto_pagina = paginas[i];
        args->num_pagina = i + 1;
        args->indice_global = &indice_global;
        args->lista_palabras = &palabras_buscar;
        args->longitud_minima_palabra = longitud_minima_palabra;
        
        pthread_create(&threads[indice_thread], NULL, procesar_pagina, args);
        indice_thread++;
        
        // Esperar si alcanzamos el máximo de threads
        if (indice_thread >= threads_reales) {
            for (int j = 0; j < threads_reales; j++) {
                pthread_join(threads[j], NULL);
            }
            indice_thread = 0;
        }
    }
    
    // Esperar threads restantes
    for (int i = 0; i < indice_thread; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    
    // Escribir resultado
    printf("\n=== ÍNDICE INVERTIDO ===\n");
    escribir_indice(&indice_global, archivo_salida);
    
    // Liberar memoria
    liberar_indice(&indice_global);
    pthread_mutex_destroy(&palabras_buscar.mutex);
    
    return 0;
}
