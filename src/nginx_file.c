#include "nginx_ui.h"
#include <glib/gstdio.h>

gchar* execute_command(const gchar *command) {
    FILE *fp = popen(command, "r");
    if (!fp) {
        return g_strdup("Error executing command");
    }
    
    GString *output = g_string_new(NULL);
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        g_string_append(output, buffer);
    }
    
    pclose(fp);
    return g_string_free(output, FALSE);
}

void on_new_file_clicked(GtkButton *button, AppData *app_data) {
    (void)button; // Unused parameter
    const gchar *filename = gtk_editable_get_text(GTK_EDITABLE(app_data->file_entry));
    if (!filename || !*filename) {
        append_log(app_data, "Error: Please enter a filename");
        return;
    }
    
    gchar *full_filename;
    if (g_str_has_suffix(filename, ".conf")) {
        full_filename = g_strdup(filename);
    } else {
        full_filename = g_strdup_printf("%s.conf", filename);
    }
    
    gchar *filepath = g_strdup_printf("%s/%s", NGINX_CONF_DIR, full_filename);
    
    // Create empty config file with sudo
    gchar *command = g_strdup_printf("sudo touch %s && sudo chmod 644 %s", filepath, filepath);
    gint result = system(command);
    g_free(command);
    
    gchar *domain_name = NULL;
    gchar *default_config = NULL;
    
    if (result == 0) {
        // Add default server block
        domain_name = g_strdup(full_filename);
        if (g_str_has_suffix(domain_name, ".conf")) {
            domain_name[strlen(domain_name) - 5] = '\0';
        }
        
        default_config = g_strdup_printf(
            "server {\n"
            "    listen 80;\n"
            "    server_name %s;\n"
            "    \n"
            "    location / {\n"
            "        root /var/www/html;\n"
            "        index index.html;\n"
            "    }\n"
            "}\n",
            domain_name
        );
        
        // Write config to temp file first, then copy with sudo
        gchar *temp_config = g_strdup_printf("/tmp/nginx_new_%s", full_filename);
        if (g_file_set_contents(temp_config, default_config, -1, NULL)) {
            gchar *write_cmd = g_strdup_printf("sudo cp %s %s && sudo chmod 644 %s", 
                                               temp_config, filepath, filepath);
            result = system(write_cmd);
            g_free(write_cmd);
            g_unlink(temp_config);
        } else {
            result = -1;
        }
        g_free(temp_config);
        
        if (result == 0) {
            // Extract domain and add to /etc/hosts
            gchar **domains = extract_domains_from_config(default_config);
            for (gint i = 0; domains[i] != NULL; i++) {
                if (!domain_exists_in_hosts(domains[i])) {
                    add_domain_to_hosts(domains[i]);
                    gchar *msg = g_strdup_printf("Added domain '%s' to /etc/hosts", domains[i]);
                    append_log(app_data, msg);
                    g_free(msg);
                }
            }
            g_strfreev(domains);
            
            refresh_file_list(app_data);
            gtk_editable_set_text(GTK_EDITABLE(app_data->file_entry), "");
            
            gchar *msg = g_strdup_printf("Created: %s", full_filename);
            append_log(app_data, msg);
            g_free(msg);
        } else {
            append_log(app_data, "Error: Failed to write config file");
        }
    } else {
        append_log(app_data, "Error: Failed to create config file");
    }
    
    g_free(domain_name);
    g_free(default_config);
    g_free(filepath);
    g_free(full_filename);
}

void on_save_clicked(GtkButton *button, AppData *app_data) {
    (void)button; // Unused parameter
    if (!app_data->current_file) {
        append_log(app_data, "Error: No file selected");
        return;
    }
    
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(app_data->source_buffer);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *content = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    gchar *filepath = g_strdup_printf("%s/%s", NGINX_CONF_DIR, app_data->current_file);
    gchar *temp_file = g_strdup_printf("/tmp/nginx_%s", app_data->current_file);
    
    // Write to temp file first
    if (g_file_set_contents(temp_file, content, -1, NULL)) {
        // Copy to destination with sudo
        gchar *command = g_strdup_printf("sudo cp %s %s && sudo chmod 644 %s", temp_file, filepath, filepath);
        gint result = system(command);
        g_free(command);
        
        if (result == 0) {
            // Extract domains and check/add to /etc/hosts
            gchar **domains = extract_domains_from_config(content);
            for (gint i = 0; domains[i] != NULL; i++) {
                if (!domain_exists_in_hosts(domains[i])) {
                    add_domain_to_hosts(domains[i]);
                    gchar *msg = g_strdup_printf("Added domain '%s' to /etc/hosts", domains[i]);
                    append_log(app_data, msg);
                    g_free(msg);
                }
            }
            g_strfreev(domains);
            
            gchar *msg = g_strdup_printf("Saved: %s", app_data->current_file);
            append_log(app_data, msg);
            g_free(msg);
        } else {
            append_log(app_data, "Error: Failed to save file");
        }
        
        g_unlink(temp_file);
    } else {
        append_log(app_data, "Error: Failed to write temporary file");
    }
    
    g_free(temp_file);
    g_free(filepath);
    g_free(content);
}

static void on_delete_response(GtkDialog *dialog, gint response_id, AppData *app_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
    
    if (response_id == GTK_RESPONSE_YES) {
        gchar *filepath = g_strdup_printf("%s/%s", NGINX_CONF_DIR, app_data->current_file);
        gchar *command = g_strdup_printf("sudo rm %s", filepath);
        gint result = system(command);
        g_free(command);
        g_free(filepath);
        
        if (result == 0) {
            gchar *deleted_file = g_strdup(app_data->current_file);
            gtk_text_buffer_set_text(GTK_TEXT_BUFFER(app_data->source_buffer), "", -1);
            g_free(app_data->current_file);
            app_data->current_file = NULL;
            gtk_widget_set_sensitive(app_data->save_btn, FALSE);
            gtk_widget_set_sensitive(app_data->delete_btn, FALSE);
            
            refresh_file_list(app_data);
            
            gchar *msg = g_strdup_printf("Deleted: %s", deleted_file);
            append_log(app_data, msg);
            g_free(msg);
            g_free(deleted_file);
        } else {
            append_log(app_data, "Error: Failed to delete file");
        }
    }
}

void on_delete_clicked(GtkButton *button, AppData *app_data) {
    (void)button; // Unused parameter
    if (!app_data->current_file) {
        append_log(app_data, "Error: No file selected");
        return;
    }
    
    // Create confirmation dialog using GTK4 API
    gchar *message = g_strdup_printf("Are you sure you want to delete %s?", app_data->current_file);
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app_data->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "%s",
        message
    );
    g_free(message);
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_delete_response), app_data);
    gtk_window_present(GTK_WINDOW(dialog));
}

void on_test_config_clicked(GtkButton *button, AppData *app_data) {
    (void)button; // Unused parameter
    append_log(app_data, "Testing Nginx configuration...");
    gchar *output = execute_command("sudo nginx -t");
    append_log(app_data, output);
    g_free(output);
}

void on_reload_nginx_clicked(GtkButton *button, AppData *app_data) {
    (void)button; // Unused parameter
    append_log(app_data, "Reloading Nginx...");
    gchar *output = execute_command("sudo systemctl reload nginx");
    if (output && strlen(output) == 0) {
        append_log(app_data, "Nginx reloaded successfully");
    } else {
        append_log(app_data, output ? output : "Reload command executed");
    }
    g_free(output);
}

void on_refresh_clicked(GtkButton *button, AppData *app_data) {
    (void)button; // Unused parameter
    refresh_file_list(app_data);
}
