#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "indexer.h"

#define INPUT_PIPE "/tmp/frontend_input"
#define OUTPUT_PIPE "/tmp/frontend_output"
#define MAX_LINE_LEN 4096

void perform_search(const char *id_to_find, int filter_year, int filter_month) {
    const char *csv_filepath = "Data2005.csv";
    const char *header_filepath = "header.dat";
    const char *index_filepath = "index.dat";

    FILE *header_file = fopen(header_filepath, "rb");
    if (!header_file) {
        return;
    }
    
    long header_table[HASH_TABLE_SIZE];
    fread(header_table, sizeof(long), HASH_TABLE_SIZE, header_file);
    fclose(header_file);

    unsigned int hash_index = hash_function(id_to_find) % HASH_TABLE_SIZE;
    long current_node_offset = header_table[hash_index];

    if (current_node_offset == -1) {
        return;
    }

    FILE *index_file = fopen(index_filepath, "rb");
    FILE *csv_file = fopen(csv_filepath, "r");
    if (!index_file || !csv_file) {
        if (index_file) fclose(index_file);
        if (csv_file) fclose(csv_file);
        return;
    }

    int found_count = 0;
    char line_buffer[MAX_LINE_LEN];
    char result_buffer[MAX_LINE_LEN * 10]; // Buffer for results
    result_buffer[0] = '\0';

    const char *csv_headers = "BibNumber,ItemBarcode,ItemType,Collection,CallNumber,CheckoutDateTime\n";

    while (current_node_offset != -1) {
        fseek(index_file, current_node_offset, SEEK_SET);

        IndexNode current_node;
        fread(&current_node, sizeof(IndexNode), 1, index_file);

        fseek(csv_file, current_node.data_offset, SEEK_SET);
        fgets(line_buffer, MAX_LINE_LEN, csv_file);

        char line_copy[MAX_LINE_LEN];
        strncpy(line_copy, line_buffer, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *record_id = strtok(line_copy, ",");

        if (record_id != NULL && strcmp(record_id, id_to_find) == 0) {
            char temp_line[MAX_LINE_LEN];
            strncpy(temp_line, line_buffer, sizeof(temp_line) - 1);
            temp_line[sizeof(temp_line) - 1] = '\0';

            char *date_time_str = NULL;
            int col_idx = 0;
            char *token = strtok(temp_line, ",");
            while (token != NULL && col_idx < 5) {
                token = strtok(NULL, ",");
                col_idx++;
            }
            
            if (token != NULL) {
                date_time_str = token;
            }

            int record_month, record_day, record_year;
            if (date_time_str && sscanf(date_time_str, "%d/%d/%d", &record_month, &record_day, &record_year) == 3) {
                int year_matches = (filter_year == 0 || record_year == filter_year);
                int month_matches = (filter_month == 0 || record_month == filter_month);

                if (year_matches && month_matches) {
                    if (found_count == 0) {
                        strcat(result_buffer, "Registros encontrados para el ID '");
                        strcat(result_buffer, id_to_find);
                        strcat(result_buffer, "'");
                        
                        if (filter_year > 0) {
                            char year_str[16];
                            snprintf(year_str, sizeof(year_str), " (Año: %d)", filter_year);
                            strcat(result_buffer, year_str);
                        }
                        
                        if (filter_month > 0) {
                            char month_str[16];
                            snprintf(month_str, sizeof(month_str), " (Mes: %d)", filter_month);
                            strcat(result_buffer, month_str);
                        }
                        
                        strcat(result_buffer, ":\n");
                        strcat(result_buffer, csv_headers);
                    }
                    strcat(result_buffer, line_buffer);
                    found_count++;
                }
            }
        }

        current_node_offset = current_node.next_node_offset;
    }

    if (found_count == 0) {
        snprintf(result_buffer, sizeof(result_buffer), 
                "ID '%s' no encontrado", id_to_find);
        if (filter_year > 0) {
            char year_str[16];
            snprintf(year_str, sizeof(year_str), " (Año: %d)", filter_year);
            strcat(result_buffer, year_str);
        }
        if (filter_month > 0) {
            char month_str[16];
            snprintf(month_str, sizeof(month_str), " (Mes: %d)", filter_month);
            strcat(result_buffer, month_str);
        }
        strcat(result_buffer, " o no hay registros que coincidan con los filtros de fecha.");
    }

    // Send response back to frontend
    int fd = open(OUTPUT_PIPE, O_WRONLY);
    write(fd, result_buffer, strlen(result_buffer)+1);
    close(fd);

    fclose(index_file);
    fclose(csv_file);
}

int main() {
    char request[MAX_LINE_LEN];
    char id_to_find[256];
    char year_str[16];
    char month_str[16];
    int filter_year, filter_month;
    
    // Create pipes (just in case they don't exist)
    mkfifo(INPUT_PIPE, 0666);
    mkfifo(OUTPUT_PIPE, 0666);

    while (1) {
        // Read from input pipe
        int fd = open(INPUT_PIPE, O_RDONLY);
        read(fd, request, sizeof(request));
        close(fd);
        
        // Parse the request
        char *token = strtok(request, "|");
        if (token) strncpy(id_to_find, token, sizeof(id_to_find)-1);
        
        token = strtok(NULL, "|");
        if (token) strncpy(year_str, token, sizeof(year_str)-1);
        else year_str[0] = '\0';
        
        token = strtok(NULL, "|");
        if (token) strncpy(month_str, token, sizeof(month_str)-1);
        else month_str[0] = '\0';
        
        // Convert year and month to integers
        filter_year = (strlen(year_str) > 0) ? atoi(year_str) : 0;
        filter_month = (strlen(month_str) > 0) ? atoi(month_str) : 0;
        
        // Perform the search
        perform_search(id_to_find, filter_year, filter_month);
    }
    
    return 0;
}