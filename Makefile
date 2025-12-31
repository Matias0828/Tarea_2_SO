# Compilador y opciones
CC = gcc
CFLAGS = -Wall -pthread
TARGET = Tarea2_SO
SOURCE = Tarea2_SO.c

# Archivos por defecto
LIBRO = ejemplo_lab1_2025-2.txt
PALABRAS = Palabras.txt
SALIDA = indice.txt
THREADS = 12
MIN_LEN = 3

# Compilar el programa
all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

# Ejecutar con parámetros por defecto
run: $(TARGET)
	./$(TARGET) -f $(LIBRO) -o $(SALIDA) -p $(PALABRAS) -t $(THREADS) -w $(MIN_LEN)

# Limpiar archivos generados
clean:
	rm -f $(TARGET) $(SALIDA)

# Mostrar ayuda
help:
	@echo "Comandos disponibles:"
	@echo "  make        - Compila el programa"
	@echo "  make run    - Compila y ejecuta"
	@echo "  make clean  - Elimina archivos generados"
	@echo "  make help   - Muestra esta ayuda"

.PHONY: all run clean help
