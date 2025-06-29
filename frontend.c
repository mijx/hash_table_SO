#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glib.h> // Necesario para g_thread y g_idle_add

#define INPUT_PIPE "/tmp/frontend_input"
#define OUTPUT_PIPE "/tmp/frontend_output"
#define MAX_LINE_LEN 4096

GtkWidget *entry_id;
GtkWidget *entry_year;
GtkWidget *entry_month;
GtkWidget *label_result;
GtkWidget *button_search; // Referencia al botón para deshabilitarlo/habilitarlo

// Estructura para pasar datos al hilo de búsqueda
typedef struct {
    char id[256];
    char year[16];
    char month[16];
} SearchRequest;

// Estructura para pasar resultados del hilo al hilo principal
typedef struct {
    char response_message[MAX_LINE_LEN];
} SearchResult;

// Callback para actualizar la GUI desde el hilo principal
gboolean update_gui_with_result(gpointer data) {
    SearchResult *result = (SearchResult *)data;
    gtk_label_set_text(GTK_LABEL(label_result), result->response_message);
    gtk_widget_set_sensitive(button_search, TRUE); // Habilita el botón de nuevo
    g_free(result); // Libera la memoria de la estructura de resultados
    return G_SOURCE_REMOVE; // Elimina este evento de la cola
}

// Función que se ejecutará en un hilo separado
gpointer backend_communication_thread(gpointer data) {
    SearchRequest *request_data = (SearchRequest *)data;
    
    char request_string[MAX_LINE_LEN];
    SearchResult *result_data = g_malloc(sizeof(SearchResult)); // Se liberará en update_gui_with_result

    // 1. Enviar la solicitud al backend
    snprintf(request_string, sizeof(request_string), "%s|%s|%s", 
             request_data->id, request_data->year, request_data->month);
    
    int fd_input = open(INPUT_PIPE, O_WRONLY);
    if (fd_input != -1) {
        write(fd_input, request_string, strlen(request_string) + 1);
        close(fd_input);
    } else {
        snprintf(result_data->response_message, sizeof(result_data->response_message), 
                 "Error: No se pudo abrir la tubería de entrada al backend.");
        g_idle_add(update_gui_with_result, result_data);
        g_free(request_data);
        return NULL;
    }

    // 2. Leer la respuesta del backend
    int fd_output = open(OUTPUT_PIPE, O_RDONLY);
    if (fd_output != -1) {
        read(fd_output, result_data->response_message, sizeof(result_data->response_message));
        close(fd_output);
    } else {
        snprintf(result_data->response_message, sizeof(result_data->response_message), 
                 "Error: No se pudo abrir la tubería de salida del backend.");
        g_idle_add(update_gui_with_result, result_data);
        g_free(request_data);
        return NULL;
    }

    // 3. Enviar la respuesta al hilo principal para actualizar la GUI
    g_idle_add(update_gui_with_result, result_data);

    g_free(request_data); // Liberar la memoria de la estructura de solicitud
    return NULL;
}

void search_id(GtkWidget *widget, gpointer data) {
    const char *id_to_find_raw = gtk_entry_get_text(GTK_ENTRY(entry_id));
    const char *year_str_raw = gtk_entry_get_text(GTK_ENTRY(entry_year));
    const char *month_str_raw = gtk_entry_get_text(GTK_ENTRY(entry_month));
    
    // Validar que existe un ID
    if(strlen(id_to_find_raw) == 0){
        gtk_label_set_text(GTK_LABEL(label_result), "Error: No se ha ingresado un ID.");
        return;
    }
    // Validar entrada del año
    if (strlen(year_str_raw) > 0) {
        int year = atoi(year_str_raw);
        if (year < 2005 || year > 2017) {
            gtk_label_set_text(GTK_LABEL(label_result), "Error: El año debe estar entre 2005 y 2017.");
            return;
        }
    }else{
        gtk_label_set_text(GTK_LABEL(label_result), "Error: No fue ingresado un año.");
        return;
    }

    // Validate month input
    if (strlen(month_str_raw) > 0) {
        int month = atoi(month_str_raw);
        if (month < 1 || month > 12) {
            gtk_label_set_text(GTK_LABEL(label_result), "Error: El mes debe ser un número entre 1 y 12.");
            return;
        }
    }else{
        gtk_label_set_text(GTK_LABEL(label_result), "Error: No fue ingresado un mes.");
        return;
    }

    // Deshabilitar el botón para evitar múltiples clics mientras se procesa
    gtk_widget_set_sensitive(button_search, FALSE);
    gtk_label_set_text(GTK_LABEL(label_result), "Buscando..."); // Mensaje de "cargando"

    // Crear y llenar la estructura de solicitud
    SearchRequest *request_data = g_malloc(sizeof(SearchRequest));
    strncpy(request_data->id, id_to_find_raw, sizeof(request_data->id) - 1);
    request_data->id[sizeof(request_data->id) - 1] = '\0';
    strncpy(request_data->year, year_str_raw, sizeof(request_data->year) - 1);
    request_data->year[sizeof(request_data->year) - 1] = '\0';
    strncpy(request_data->month, month_str_raw, sizeof(request_data->month) - 1);
    request_data->month[sizeof(request_data->month) - 1] = '\0';
    
    // Iniciar el hilo para la comunicación con el backend
    g_thread_new("BackendCommThread", backend_communication_thread, request_data);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *label_prompt_id, *label_prompt_year, *label_prompt_month;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Buscador de IDs CSV con Filtro de Fecha");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    label_prompt_id = gtk_label_new("Ingrese el ID a buscar (BibNum):");
    gtk_grid_attach(GTK_GRID(grid), label_prompt_id, 0, 0, 1, 1);
    gtk_widget_set_halign(label_prompt_id, GTK_ALIGN_END);

    entry_id = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_id, 1, 0, 2, 1);
    gtk_widget_set_hexpand(entry_id, TRUE);

    label_prompt_year = gtk_label_new("Año:");
    gtk_grid_attach(GTK_GRID(grid), label_prompt_year, 0, 1, 1, 1);
    gtk_widget_set_halign(label_prompt_year, GTK_ALIGN_END);

    entry_year = gtk_entry_new();
    gtk_entry_set_input_purpose(GTK_ENTRY(entry_year), GTK_INPUT_PURPOSE_DIGITS);
    gtk_entry_set_max_length(GTK_ENTRY(entry_year), 4);
    gtk_grid_attach(GTK_GRID(grid), entry_year, 1, 1, 2, 1);
    gtk_widget_set_hexpand(entry_year, TRUE);

    label_prompt_month = gtk_label_new("Mes (01-12, dos dígitos):");
    gtk_grid_attach(GTK_GRID(grid), label_prompt_month, 0, 2, 1, 1);
    gtk_widget_set_halign(label_prompt_month, GTK_ALIGN_END);

    entry_month = gtk_entry_new();
    gtk_entry_set_input_purpose(GTK_ENTRY(entry_month), GTK_INPUT_PURPOSE_DIGITS);
    gtk_entry_set_max_length(GTK_ENTRY(entry_month), 2);
    gtk_grid_attach(GTK_GRID(grid), entry_month, 1, 2, 2, 1);
    gtk_widget_set_hexpand(entry_month, TRUE);

    button_search = gtk_button_new_with_label("Buscar y Filtrar");
    gtk_grid_attach(GTK_GRID(grid), button_search, 1, 3, 1, 1);
    g_signal_connect(button_search, "clicked", G_CALLBACK(search_id), NULL);

    label_result = gtk_label_new("Resultados aparecerán aquí.");
    gtk_label_set_selectable(GTK_LABEL(label_result), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label_result), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(label_result), TRUE);
    gtk_grid_attach(GTK_GRID(grid), label_result, 0, 4, 3, 1);
    gtk_widget_set_hexpand(label_result, TRUE);
    gtk_widget_set_vexpand(label_result, TRUE);

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    // Inicializar el sistema de hilos de GLib
    // Esto es importante para que g_thread_new funcione correctamente
    if (!g_thread_supported()) {
        g_thread_init(NULL); // Obsoleto en GLib 2.32+, pero buena práctica
    }
    
    // Crear pipes (just in case they don't exist)
    mkfifo(INPUT_PIPE, 0666);
    mkfifo(OUTPUT_PIPE, 0666);

    app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    // Clean up pipes when done
    unlink(INPUT_PIPE);
    unlink(OUTPUT_PIPE);

    return status;
}