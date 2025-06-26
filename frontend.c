#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define INPUT_PIPE "/tmp/frontend_input"
#define OUTPUT_PIPE "/tmp/frontend_output"
#define MAX_LINE_LEN 4096

GtkWidget *entry_id;
GtkWidget *entry_year;
GtkWidget *entry_month;
GtkWidget *label_result;

void send_request_to_backend(const char *id, const char *year, const char *month) {
    int fd;
    char request[MAX_LINE_LEN];
    
    // Create the request string
    snprintf(request, sizeof(request), "%s|%s|%s", id, year, month);
    
    // Open the input pipe (frontend writes to this)
    mkfifo(INPUT_PIPE, 0666);
    fd = open(INPUT_PIPE, O_WRONLY);
    write(fd, request, strlen(request)+1);
    close(fd);
}

void read_response_from_backend() {
    int fd;
    char response[MAX_LINE_LEN];
    
    // Open the output pipe (frontend reads from this)
    fd = open(OUTPUT_PIPE, O_RDONLY);
    read(fd, response, sizeof(response));
    close(fd);
    
    // Update the GUI with the response
    gtk_label_set_text(GTK_LABEL(label_result), response);
}

void search_id(GtkWidget *widget, gpointer data) {
    const char *id_to_find = gtk_entry_get_text(GTK_ENTRY(entry_id));
    const char *year_str = gtk_entry_get_text(GTK_ENTRY(entry_year));
    const char *month_str = gtk_entry_get_text(GTK_ENTRY(entry_month));
    
    // Validate month input
    if (strlen(month_str) > 0) {
        int month = atoi(month_str);
        if (month < 1 || month > 12) {
            gtk_label_set_text(GTK_LABEL(label_result), "Error: El mes debe ser un número entre 1 y 12.");
            return;
        }
    }
    
    send_request_to_backend(id_to_find, year_str, month_str);
    read_response_from_backend();
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *label_prompt_id, *label_prompt_year, *label_prompt_month;
    GtkWidget *button_search;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Buscador de IDs CSV con Filtro de Fecha");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    label_prompt_id = gtk_label_new("Ingrese el ID a buscar:");
    gtk_grid_attach(GTK_GRID(grid), label_prompt_id, 0, 0, 1, 1);
    gtk_widget_set_halign(label_prompt_id, GTK_ALIGN_END);

    entry_id = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_id, 1, 0, 2, 1);
    gtk_widget_set_hexpand(entry_id, TRUE);

    label_prompt_year = gtk_label_new("Año (opcional):");
    gtk_grid_attach(GTK_GRID(grid), label_prompt_year, 0, 1, 1, 1);
    gtk_widget_set_halign(label_prompt_year, GTK_ALIGN_END);

    entry_year = gtk_entry_new();
    gtk_entry_set_input_purpose(GTK_ENTRY(entry_year), GTK_INPUT_PURPOSE_DIGITS);
    gtk_entry_set_max_length(GTK_ENTRY(entry_year), 4);
    gtk_grid_attach(GTK_GRID(grid), entry_year, 1, 1, 2, 1);
    gtk_widget_set_hexpand(entry_year, TRUE);

    label_prompt_month = gtk_label_new("Mes (1-12, opcional):");
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

    // Create pipes (just in case they don't exist)
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