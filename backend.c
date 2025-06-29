#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "indexer.h" // Assuming indexer.h contains IndexNode and HASH_TABLE_SIZE definitions

#define INPUT_PIPE "/tmp/frontend_input"
#define OUTPUT_PIPE "/tmp/frontend_output"
#define MAX_LINE_LEN 4096
#define BATCH_READ_SIZE 100 // Leer múltiples nodos en una sola operación (aún aplicable para eficiencia en lectura directa)

const char *csv_filepath = "dataset.csv";
const char *header_filepath = "header.dat";
const char *index_filepath = "index.dat";

// --- Cache-related code REMOVED ---
// typedef struct {
//     long offset;
//     IndexNode node;
//     int valid;
//     long last_used;
// } NodeCache;
// #define CACHE_SIZE 5000
// static NodeCache node_cache[CACHE_SIZE];
// static int cache_initialized = 0;
// static long cache_counter = 0;
// void init_cache() { /* ... */ }
// IndexNode* get_cached_node(long offset) { /* ... */ }
// void cache_node(long offset, IndexNode* node) { /* ... */ }
// --- End of REMOVED Cache code ---


// Función hash mejorada para BibNumber + Fecha
unsigned long improved_hash_function(const char *bibNumber, const char *fecha) {
    unsigned long hash = 5381;
    unsigned long hash2 = 1;
    
    // Procesar BibNumber con hash principal
    const char *str = bibNumber;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    // Procesar fecha con segundo hash y combinar de manera más efectiva
    str = fecha;
    while ((c = *str++)) {
        hash2 = hash2 * 31 + c;
    }
    
    // Combinar los dos hashes de manera más distribuida
    return hash ^ (hash2 << 16) ^ (hash2 >> 16);
}

// Función para extraer fecha de una línea de manera más eficiente
int extract_date_fast(const char *line, int *month, int *day, int *year) {
    // Buscar la sexta coma directamente
    int comma_count = 0;
    const char *ptr = line;
    
    while (*ptr && comma_count < 5) {
        if (*ptr == ',') comma_count++;
        ptr++;
    }
    
    if (comma_count == 5 && *ptr) {
        return sscanf(ptr, "%d/%d/%d", month, day, year) == 3;
    }
    return 0;
}

// Función optimizada para lectura por lotes de nodos (ahora sin cache)
// La estructura BatchNode se mantiene, pero la función ya no usa la caché
typedef struct {
    long offset;
    IndexNode node;
} BatchNode;

int read_chain_batch(FILE *index_file, long start_offset, BatchNode *batch, int max_nodes) {
    int count = 0;
    long current_offset = start_offset;
    
    while (current_offset != -1 && count < max_nodes) {
        // --- Directly read from file, no cache check ---
        fseek(index_file, current_offset, SEEK_SET);
        if (fread(&batch[count].node, sizeof(IndexNode), 1, index_file) != 1) {
            break; // Error reading or end of file
        }
        batch[count].offset = current_offset;
        current_offset = batch[count].node.next_node_offset; // Move to the next node in the chain
        // --- End of direct read ---
        
        count++;
    }
    
    return count;
}

void perform_search(const char *id_to_find, int filter_year, int filter_month) {
    
    // --- init_cache() call REMOVED ---
    // init_cache(); 

    FILE *header_file = fopen(header_filepath, "rb");
    if (!header_file) {
        perror("Error al abrir el archivo de cabecera"); // Mejorar manejo de errores
        return;
    }
    
    long *header_table = malloc(sizeof(long) * HASH_TABLE_SIZE);
    if (!header_table) {
        fprintf(stderr, "Error: No se pudo asignar memoria para la tabla hash\n");
        fclose(header_file);
        return;
    }
    size_t read_result = fread(header_table, sizeof(long), HASH_TABLE_SIZE, header_file);
    fclose(header_file);
    
    if (read_result != HASH_TABLE_SIZE) {
        fprintf(stderr, "Error: No se pudo leer la tabla hash completa\n"); // Mejorar manejo de errores
        free(header_table);
        return;
    }

    int found_count = 0;
    char line_buffer[MAX_LINE_LEN];
    char result_buffer[MAX_LINE_LEN * 20]; // Buffer más grande
    result_buffer[0] = '\0';

    const char *csv_headers = "BibNumber,ItemBarcode,ItemType,Collection,CallNumber,CheckoutDateTime\n";

    // Crear hash con BibNumber + fecha
    char fecha[8]; // MM/YYYY + null terminator
    snprintf(fecha, sizeof(fecha), "%02d/%04d", filter_month, filter_year);

    unsigned int hash_index = improved_hash_function(id_to_find, fecha) % HASH_TABLE_SIZE;
    long current_node_offset = header_table[hash_index];

    FILE *index_file = NULL;
    FILE *csv_file = NULL;

    if (current_node_offset == -1) {
        goto end_search; // No hay nodos para este hash
    }

    index_file = fopen(index_filepath, "rb");
    csv_file = fopen(csv_filepath, "r");
    
    if (!index_file || !csv_file) {
        snprintf(result_buffer, sizeof(result_buffer), 
                "Error: No se pudieron abrir los archivos de base de datos");
        goto end_search;
    }

    // Procesar en lotes para reducir I/O
    BatchNode batch[BATCH_READ_SIZE];
    
    while (current_node_offset != -1) {
        // Llama a la versión de read_chain_batch que lee directamente de disco
        int batch_size = read_chain_batch(index_file, current_node_offset, batch, BATCH_READ_SIZE);
        
        if (batch_size == 0) { // No se leyeron nodos en el batch
            break;
        }

        for (int i = 0; i < batch_size; i++) {
            IndexNode current_node = batch[i].node;
            
            // Leer línea del CSV
            if (fseek(csv_file, current_node.data_offset, SEEK_SET) != 0) {
                // Error seeking, perhaps the file changed or offset is bad
                continue;
            }
            
            if (!fgets(line_buffer, MAX_LINE_LEN, csv_file)) {
                // Error reading line or end of file
                continue;
            }

            // Extraer ID de manera eficiente
            char *comma_pos = strchr(line_buffer, ',');
            if (!comma_pos) {
                continue;
            }

            // Comparar ID directamente
            int id_len = comma_pos - line_buffer;
            int search_len = strlen(id_to_find);
            
            if (id_len == search_len && strncmp(line_buffer, id_to_find, id_len) == 0) {
                // ID coincide, verificar fecha
                int record_month, record_day, record_year;
                
                if (extract_date_fast(line_buffer, &record_month, &record_day, &record_year)) {
                    // Verificar si coincide con los filtros
                    int year_matches = (filter_year == 0 || record_year == filter_year);
                    int month_matches = (filter_month == 0 || record_month == filter_month);
                    
                    if (year_matches && month_matches) {
                        // Registro válido encontrado
                        if (found_count == 0) {
                            strcat(result_buffer, "Registros encontrados para el ID '");
                            strcat(result_buffer, id_to_find);
                            strcat(result_buffer, "'");
                            
                            if (filter_year > 0) {
                                char year_str_buf[16]; // Use a new buffer to avoid conflicts
                                snprintf(year_str_buf, sizeof(year_str_buf), " (Año: %d)", filter_year);
                                strcat(result_buffer, year_str_buf);
                            }
                            
                            if (filter_month > 0) {
                                char month_str_buf[16]; // Use a new buffer
                                snprintf(month_str_buf, sizeof(month_str_buf), " (Mes: %d)", filter_month);
                                strcat(result_buffer, month_str_buf);
                            }
                            
                            strcat(result_buffer, ":\n");
                            strcat(result_buffer, csv_headers);
                        }
                        
                        // Verificar que no excedamos el buffer
                        if (strlen(result_buffer) + strlen(line_buffer) < sizeof(result_buffer) - 100) {
                            strcat(result_buffer, line_buffer);
                            found_count++;
                        }
                        
                        // Limitar resultados para performance
                        if (found_count >= 200) {
                            strcat(result_buffer, "\n... (más de 200 resultados, mostrando solo los primeros 200)\n");
                            goto end_search;
                        }
                    }
                }
            }
        }
        
        // Continuar con el siguiente lote si hay más nodos en la cadena
        current_node_offset = batch[batch_size-1].node.next_node_offset;
    }

end_search:
    if (index_file) {
        fclose(index_file);
    }
    if (csv_file) {
        fclose(csv_file);
    }

    if (found_count == 0 && result_buffer[0] == '\0') {
        snprintf(result_buffer, sizeof(result_buffer), 
                "ID '%s' no encontrado", id_to_find);
        if (filter_year > 0) {
            char year_str_buf[16];
            snprintf(year_str_buf, sizeof(year_str_buf), " (Año: %d)", filter_year);
            strcat(result_buffer, year_str_buf);
        }
        if (filter_month > 0) {
            char month_str_buf[16];
            snprintf(month_str_buf, sizeof(month_str_buf), " (Mes: %d)", filter_month);
            strcat(result_buffer, month_str_buf);
        }
        strcat(result_buffer, " o no hay registros que coincidan con los filtros de fecha.");
    } else if (found_count > 0 && found_count < 200 && strstr(result_buffer, "\n...") == NULL) {
         // Add a newline if results were found but no truncation message was added
         strcat(result_buffer, "\n");
    }

    // Enviar respuesta al frontend
    int fd = open(OUTPUT_PIPE, O_WRONLY);
    if (fd != -1) {
        write(fd, result_buffer, strlen(result_buffer) + 1);
        close(fd);
    }

    // cerrar malloc de header:
    free(header_table);

}

int main() {
    char request[MAX_LINE_LEN];
    char id_to_find[256];
    char year_str[16];
    char month_str[16];
    int filter_year, filter_month;
    
    mkfifo(INPUT_PIPE, 0666);
    mkfifo(OUTPUT_PIPE, 0666);

    printf("Backend de búsqueda iniciado...\n");

    while (1) {
        int fd = open(INPUT_PIPE, O_RDONLY);
        if (fd == -1) {
            usleep(100000); // Esperar 100ms antes de reintentar
            continue;
        }
        
        ssize_t bytes_read = read(fd, request, sizeof(request) - 1);
        close(fd);
        
        if (bytes_read <= 0) {
            continue;
        }
        
        request[bytes_read] = '\0';
        
        // Parse the request
        char *token = strtok(request, "|");
        if (token) {
            strncpy(id_to_find, token, sizeof(id_to_find) - 1);
            id_to_find[sizeof(id_to_find) - 1] = '\0';
        } else { // Handle case where id_to_find is missing
            id_to_find[0] = '\0';
        }
        
        token = strtok(NULL, "|");
        if (token) {
            strncpy(year_str, token, sizeof(year_str) - 1);
            year_str[sizeof(year_str) - 1] = '\0';
        } else {
            year_str[0] = '\0';
        }
        
        token = strtok(NULL, "|");
        if (token) {
            strncpy(month_str, token, sizeof(month_str) - 1);
            month_str[sizeof(month_str) - 1] = '\0';
        } else {
            month_str[0] = '\0';
        }
        
        filter_year = (strlen(year_str) > 0) ? atoi(year_str) : 0;
        filter_month = (strlen(month_str) > 0) ? atoi(month_str) : 0;
        
        printf("Buscando ID: %s, Año: %d, Mes: %d\n", id_to_find, filter_year, filter_month);
        
        perform_search(id_to_find, filter_year, filter_month);
    }
    
    return 0;
}