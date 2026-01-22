#ifndef NGINX_UI_H
#define NGINX_UI_H

#include <gtk/gtk.h>
#ifdef HAVE_GTKSOURCEVIEW
#include <gtksourceview/gtksource.h>
#endif

#define NGINX_CONF_DIR "/etc/nginx/conf.d"
#define HOSTS_FILE "/etc/hosts"
#define MAX_LINE_LENGTH 4096

typedef struct {
    GtkWidget *window;
    GtkWidget *file_list;
    GtkWidget *file_entry;
    GtkWidget *editor;
    GtkWidget *logs_text;
    GtkWidget *save_btn;
    GtkWidget *delete_btn;
    GtkWidget *test_btn;
    GtkWidget *reload_btn;
    GtkWidget *refresh_btn;
    GtkTextBuffer *source_buffer;
    gchar *current_file;
} AppData;

// UI functions
void append_log(AppData *app_data, const gchar *message);
void refresh_file_list(AppData *app_data);
void setup_ui(GtkApplication *app, AppData *app_data);

// File operations
gchar* execute_command(const gchar *command);
void on_new_file_clicked(GtkButton *button, AppData *app_data);
void on_save_clicked(GtkButton *button, AppData *app_data);
void on_delete_clicked(GtkButton *button, AppData *app_data);
void on_file_selected(GObject *object, GParamSpec *pspec, AppData *app_data);

// Nginx operations
void on_test_config_clicked(GtkButton *button, AppData *app_data);
void on_reload_nginx_clicked(GtkButton *button, AppData *app_data);
void on_refresh_clicked(GtkButton *button, AppData *app_data);

// Hosts file operations
gchar** extract_domains_from_config(const gchar *config_content);
gboolean domain_exists_in_hosts(const gchar *domain);
void add_domain_to_hosts(const gchar *domain);

// Syntax highlighting (when GtkSourceView not available)
void apply_syntax_highlighting(GtkTextBuffer *buffer);

#endif // NGINX_UI_H
