#include "nginx_ui.h"
#include <glib/gstdio.h>

void append_log(AppData *app_data, const gchar *message) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app_data->logs_text));
    GtkTextIter iter;
    GDateTime *now = g_date_time_new_now_local();
    gchar *timestamp = g_date_time_format(now, "[%H:%M:%S]");
    
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, timestamp, -1);
    gtk_text_buffer_insert(buffer, &iter, " ", -1);
    gtk_text_buffer_insert(buffer, &iter, message, -1);
    gtk_text_buffer_insert(buffer, &iter, "\n", -1);
    
    g_free(timestamp);
    g_date_time_unref(now);
    
    // Auto-scroll to bottom
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(app_data->logs_text),
                                 gtk_text_buffer_get_insert(buffer),
                                 0.0, FALSE, 0.0, 0.0);
}

static void setup_list_item(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data) {
    (void)factory; (void)user_data; // Unused parameters
    GtkWidget *label = gtk_label_new(NULL);
    gtk_list_item_set_child(item, label);
}

static void bind_list_item(GtkListItemFactory *factory, GtkListItem *item, gpointer user_data) {
    (void)factory; (void)user_data; // Unused parameters
    GtkWidget *label = gtk_list_item_get_child(item);
    GtkStringObject *str_obj = GTK_STRING_OBJECT(gtk_list_item_get_item(item));
    if (str_obj) {
        const gchar *text = gtk_string_object_get_string(str_obj);
        gtk_label_set_text(GTK_LABEL(label), text);
    }
}

void on_file_selected(GObject *object, GParamSpec *pspec, AppData *app_data) {
    (void)pspec; // Unused parameter
    GtkSingleSelection *selection = GTK_SINGLE_SELECTION(object);
    guint position = gtk_single_selection_get_selected(selection);
    
    if (position == GTK_INVALID_LIST_POSITION) {
        return;
    }
    
    GListModel *model = gtk_single_selection_get_model(selection);
    GObject *item = g_list_model_get_item(model, position);
    if (!item) return;
    
    const gchar *filename = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
    if (!filename) {
        g_object_unref(item);
        return;
    }
    
    g_free(app_data->current_file);
    app_data->current_file = g_strdup(filename);
    
    // Load file content
    gchar *filepath = g_strdup_printf("%s/%s", NGINX_CONF_DIR, filename);
    gchar *content = NULL;
    GError *error = NULL;
    
    if (g_file_get_contents(filepath, &content, NULL, &error)) {
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(app_data->source_buffer), content, -1);
        
        // Apply syntax highlighting after loading (only if not using GtkSourceView)
        // GtkSourceView handles highlighting automatically
#ifndef HAVE_GTKSOURCEVIEW
        apply_syntax_highlighting(GTK_TEXT_BUFFER(app_data->source_buffer));
#endif
        
        g_free(content);
        
        gchar *msg = g_strdup_printf("Loaded: %s", filename);
        append_log(app_data, msg);
        g_free(msg);
    } else {
        append_log(app_data, error->message);
        g_error_free(error);
    }
    
    g_free(filepath);
    g_object_unref(item);
    
    // Enable save and delete buttons
    gtk_widget_set_sensitive(app_data->save_btn, TRUE);
    gtk_widget_set_sensitive(app_data->delete_btn, TRUE);
}

void refresh_file_list(AppData *app_data) {
    // Read files from /etc/nginx/conf.d/
    GDir *dir = g_dir_open(NGINX_CONF_DIR, 0, NULL);
    if (dir) {
        const gchar *filename;
        GtkStringList *string_list = gtk_string_list_new(NULL);
        
        while ((filename = g_dir_read_name(dir)) != NULL) {
            if (g_str_has_suffix(filename, ".conf")) {
                gtk_string_list_append(string_list, filename);
            }
        }
        
        GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(string_list));
        g_signal_connect(selection, "notify::selected", G_CALLBACK(on_file_selected), app_data);
        gtk_list_view_set_model(GTK_LIST_VIEW(app_data->file_list), GTK_SELECTION_MODEL(selection));
        g_dir_close(dir);
        
        guint count = g_list_model_get_n_items(G_LIST_MODEL(string_list));
        gchar *msg = g_strdup_printf("Loaded %u config file(s) from %s", count, NGINX_CONF_DIR);
        append_log(app_data, msg);
        g_free(msg);
    } else {
        append_log(app_data, "Error: Cannot access /etc/nginx/conf.d/");
    }
}

void apply_syntax_highlighting(GtkTextBuffer *buffer) {
#ifdef HAVE_GTKSOURCEVIEW
    (void)buffer; // Unused when GtkSourceView handles highlighting
#else
    // Nginx keywords and directives
    const gchar *keywords[] = {
        "server", "listen", "server_name", "location", "root", "index", 
        "proxy_pass", "proxy_set_header", "access_log", "error_log",
        "return", "rewrite", "try_files", "include", "if", "set",
        "upstream", "worker_processes", "events", "http", "gzip",
        "ssl_certificate", "ssl_certificate_key", "ssl_protocols",
        "client_max_body_size", "keepalive_timeout", "types",
        NULL
    };
    
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    if (!text || !*text) {
        g_free(text);
        return;
    }
    
    // Remove all existing tags first
    gtk_text_buffer_remove_all_tags(buffer, &start, &end);
    
    // Apply comment highlighting
    gchar *line_start = text;
    gchar *line_end;
    gint line_num = 0;
    while ((line_end = strchr(line_start, '\n')) != NULL || *line_start != '\0') {
        gint line_len = line_end ? (gint)(line_end - line_start) : (gint)strlen(line_start);
        gchar *line = g_strndup(line_start, line_len);
        gchar *trimmed = g_strstrip(line);
        
        // Check for comments
        if (*trimmed == '#') {
            GtkTextIter comment_start, comment_end;
            gtk_text_buffer_get_iter_at_line(buffer, &comment_start, line_num);
            gtk_text_buffer_get_iter_at_line(buffer, &comment_end, line_num);
            gtk_text_iter_forward_to_line_end(&comment_end);
            gtk_text_buffer_apply_tag_by_name(buffer, "comment", &comment_start, &comment_end);
        } else {
            // Check for strings (simple: text between quotes)
            gchar *quote_start = strchr(line_start, '"');
            if (quote_start && quote_start < (line_end ? line_end : line_start + strlen(line_start))) {
                gchar *quote_end = strchr(quote_start + 1, '"');
                if (quote_end && quote_end < (line_end ? line_end : line_start + strlen(line_start))) {
                    GtkTextIter str_start, str_end;
                    gint str_start_offset = quote_start - text;
                    gint str_end_offset = quote_end - text + 1;
                    gtk_text_buffer_get_iter_at_offset(buffer, &str_start, str_start_offset);
                    gtk_text_buffer_get_iter_at_offset(buffer, &str_end, str_end_offset);
                    gtk_text_buffer_apply_tag_by_name(buffer, "string", &str_start, &str_end);
                }
            }
            
            // Check for keywords
            for (gint i = 0; keywords[i] != NULL; i++) {
                gchar *keyword_pos = line_start;
                gsize keyword_len = strlen(keywords[i]);
                while ((keyword_pos = strstr(keyword_pos, keywords[i])) != NULL) {
                    // Check if it's a whole word
                    gboolean is_word = TRUE;
                    if (keyword_pos > text && 
                        (g_ascii_isalnum(keyword_pos[-1]) || keyword_pos[-1] == '_')) {
                        is_word = FALSE;
                    }
                    if (keyword_pos + keyword_len < text + strlen(text) &&
                        (g_ascii_isalnum(keyword_pos[keyword_len]) || keyword_pos[keyword_len] == '_')) {
                        is_word = FALSE;
                    }
                    
                    if (is_word) {
                        GtkTextIter kw_start, kw_end;
                        gint kw_offset = keyword_pos - text;
                        gtk_text_buffer_get_iter_at_offset(buffer, &kw_start, kw_offset);
                        gtk_text_buffer_get_iter_at_offset(buffer, &kw_end, kw_offset + keyword_len);
                        
                        // Use directive tag for common nginx directives
                        if (strcmp(keywords[i], "server") == 0 || 
                            strcmp(keywords[i], "location") == 0 ||
                            strcmp(keywords[i], "upstream") == 0 ||
                            strcmp(keywords[i], "http") == 0 ||
                            strcmp(keywords[i], "events") == 0) {
                            gtk_text_buffer_apply_tag_by_name(buffer, "directive", &kw_start, &kw_end);
                        } else {
                            gtk_text_buffer_apply_tag_by_name(buffer, "keyword", &kw_start, &kw_end);
                        }
                    }
                    keyword_pos += keyword_len;
                }
            }
        }
        
        g_free(line);
        if (line_end) {
            line_start = line_end + 1;
            line_num++;
        } else {
            break;
        }
    }
    
    g_free(text);
#endif
}

static void on_text_changed(GtkTextBuffer *buffer, AppData *app_data) {
    (void)app_data; // Unused parameter
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    (void)strlen(text); // Character count available if needed
    g_free(text);
    
    // Apply syntax highlighting (only if not using GtkSourceView)
    // GtkSourceView handles highlighting automatically
#ifndef HAVE_GTKSOURCEVIEW
    apply_syntax_highlighting(buffer);
#endif
}

void setup_ui(GtkApplication *app, AppData *app_data) {
    // Create main window
    app_data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(app_data->window), "Nginx Config Editor");
    gtk_window_set_default_size(GTK_WINDOW(app_data->window), 1400, 900);
    // Set minimum window size to prevent layout issues when window is small
    gtk_widget_set_size_request(GTK_WIDGET(app_data->window), 1000, 700);
    
    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(app_data->window), main_box);
    
    // Header bar
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(header_box, 12);
    gtk_widget_set_margin_end(header_box, 12);
    gtk_widget_set_margin_top(header_box, 12);
    gtk_widget_set_margin_bottom(header_box, 12);
    
    GtkWidget *title_label = gtk_label_new("Nginx Config Editor");
    gtk_widget_add_css_class(title_label, "title");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_size_new(18 * PANGO_SCALE));
    gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
    pango_attr_list_unref(attrs);
    
    gtk_box_append(GTK_BOX(header_box), title_label);
    gtk_box_append(GTK_BOX(header_box), gtk_label_new("")); // Spacer
    
    app_data->test_btn = gtk_button_new_with_label("Test Config");
    gtk_widget_add_css_class(app_data->test_btn, "suggested-action");
    g_signal_connect(app_data->test_btn, "clicked", G_CALLBACK(on_test_config_clicked), app_data);
    gtk_box_append(GTK_BOX(header_box), app_data->test_btn);
    
    app_data->reload_btn = gtk_button_new_with_label("Reload Nginx");
    gtk_widget_add_css_class(app_data->reload_btn, "suggested-action");
    g_signal_connect(app_data->reload_btn, "clicked", G_CALLBACK(on_reload_nginx_clicked), app_data);
    gtk_box_append(GTK_BOX(header_box), app_data->reload_btn);
    
    app_data->refresh_btn = gtk_button_new_with_label("Refresh");
    gtk_widget_add_css_class(app_data->refresh_btn, "suggested-action");
    g_signal_connect(app_data->refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), app_data);
    gtk_box_append(GTK_BOX(header_box), app_data->refresh_btn);
    
    gtk_box_append(GTK_BOX(main_box), header_box);
    
    // Horizontal paned for three panels
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(main_box), hpaned);
    
    // Left panel: Config Files (increased height)
    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(left_panel, 12);
    gtk_widget_set_margin_end(left_panel, 12);
    gtk_widget_set_margin_top(left_panel, 12);
    gtk_widget_set_margin_bottom(left_panel, 12);
    // Set minimum width but allow it to shrink if needed
    gtk_widget_set_size_request(left_panel, 250, -1);
    
    GtkWidget *files_label = gtk_label_new("Config Files");
    gtk_widget_add_css_class(files_label, "title");
    gtk_box_append(GTK_BOX(left_panel), files_label);
    
    // New file input box - use a box that handles shrinking better
    GtkWidget *new_file_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(new_file_box, 0);
    gtk_widget_set_margin_end(new_file_box, 0);
    
    app_data->file_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app_data->file_entry), "newfile.conf");
    // Make entry expand and fill available space
    gtk_widget_set_hexpand(app_data->file_entry, TRUE);
    gtk_widget_set_halign(app_data->file_entry, GTK_ALIGN_FILL);
    // Set minimum width for entry to remain usable
    gtk_widget_set_size_request(app_data->file_entry, 120, -1);
    gtk_box_append(GTK_BOX(new_file_box), app_data->file_entry);
    
    GtkWidget *new_btn = gtk_button_new_with_label("New");
    gtk_widget_add_css_class(new_btn, "suggested-action");
    // Button should not expand, keep its natural size
    gtk_widget_set_hexpand(new_btn, FALSE);
    gtk_widget_set_halign(new_btn, GTK_ALIGN_CENTER);
    // Set minimum width for button
    gtk_widget_set_size_request(new_btn, 60, -1);
    g_signal_connect(new_btn, "clicked", G_CALLBACK(on_new_file_clicked), app_data);
    gtk_box_append(GTK_BOX(new_file_box), new_btn);
    
    // Make the box itself handle shrinking gracefully
    gtk_widget_set_hexpand(new_file_box, TRUE);
    gtk_box_append(GTK_BOX(left_panel), new_file_box);
    
    // File list with increased minimum height
    GtkStringList *string_list = gtk_string_list_new(NULL);
    GtkSingleSelection *selection_model = gtk_single_selection_new(G_LIST_MODEL(string_list));
    
    GtkListItemFactory *factory = GTK_LIST_ITEM_FACTORY(gtk_signal_list_item_factory_new());
    g_signal_connect(factory, "setup", G_CALLBACK(setup_list_item), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_list_item), NULL);
    
    app_data->file_list = gtk_list_view_new(GTK_SELECTION_MODEL(selection_model), factory);
    
    GtkWidget *scrolled_files = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_files), app_data->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_files), 
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    // Make file list expand to fill available vertical space
    gtk_widget_set_vexpand(scrolled_files, TRUE);
    gtk_widget_set_hexpand(scrolled_files, TRUE);
    gtk_widget_set_valign(scrolled_files, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(left_panel), scrolled_files);
    
    gtk_paned_set_start_child(GTK_PANED(hpaned), left_panel);
    
    // Right side: Editor and Logs
    GtkWidget *right_vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    
    // Center panel: Editor (increased height)
    GtkWidget *editor_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(editor_panel, 12);
    gtk_widget_set_margin_end(editor_panel, 12);
    gtk_widget_set_margin_top(editor_panel, 12);
    gtk_widget_set_margin_bottom(editor_panel, 12);
    
    GtkWidget *editor_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *editing_label = gtk_label_new("Editing:");
    gtk_widget_add_css_class(editing_label, "title");
    gtk_box_append(GTK_BOX(editor_header), editing_label);
    gtk_box_append(GTK_BOX(editor_header), gtk_label_new("")); // Spacer
    
    app_data->save_btn = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(app_data->save_btn, "suggested-action");
    g_signal_connect(app_data->save_btn, "clicked", G_CALLBACK(on_save_clicked), app_data);
    gtk_widget_set_sensitive(app_data->save_btn, FALSE);
    gtk_box_append(GTK_BOX(editor_header), app_data->save_btn);
    
    app_data->delete_btn = gtk_button_new_with_label("Delete");
    gtk_widget_add_css_class(app_data->delete_btn, "destructive-action");
    g_signal_connect(app_data->delete_btn, "clicked", G_CALLBACK(on_delete_clicked), app_data);
    gtk_widget_set_sensitive(app_data->delete_btn, FALSE);
    gtk_box_append(GTK_BOX(editor_header), app_data->delete_btn);
    
    gtk_box_append(GTK_BOX(editor_panel), editor_header);
    
    // Source view for syntax highlighting (or plain text view if GtkSourceView not available)
#ifdef HAVE_GTKSOURCEVIEW
    app_data->source_buffer = GTK_TEXT_BUFFER(gtk_source_buffer_new(NULL));
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    
    // Try to find nginx language definition
    GtkSourceLanguage *lang = gtk_source_language_manager_get_language(lm, "nginx");
    if (!lang) {
        // Try alternative names
        lang = gtk_source_language_manager_get_language(lm, "conf");
        if (!lang) {
            lang = gtk_source_language_manager_get_language(lm, "apache");
        }
    }
    
    if (lang) {
        gtk_source_buffer_set_language(GTK_SOURCE_BUFFER(app_data->source_buffer), lang);
    } else {
        // Create a simple style scheme for basic highlighting
        GtkSourceStyleSchemeManager *scheme_mgr = gtk_source_style_scheme_manager_get_default();
        GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(scheme_mgr, "classic");
        if (scheme) {
            gtk_source_buffer_set_style_scheme(GTK_SOURCE_BUFFER(app_data->source_buffer), scheme);
        }
    }
    
    app_data->editor = gtk_source_view_new_with_buffer(GTK_SOURCE_BUFFER(app_data->source_buffer));
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(app_data->editor), TRUE);
    gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(app_data->editor), TRUE);
    gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(app_data->editor), 4);
    gtk_source_view_set_insert_spaces_instead_of_tabs(GTK_SOURCE_VIEW(app_data->editor), TRUE);
    gtk_source_buffer_set_highlight_syntax(GTK_SOURCE_BUFFER(app_data->source_buffer), TRUE);
    
    // Enable word wrap
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app_data->editor), GTK_WRAP_WORD);
#else
    app_data->source_buffer = gtk_text_buffer_new(NULL);
    app_data->editor = gtk_text_view_new_with_buffer(app_data->source_buffer);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(app_data->editor), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app_data->editor), GTK_WRAP_WORD);
    
    // Add basic syntax highlighting using text tags
    GtkTextTagTable *tag_table = gtk_text_buffer_get_tag_table(app_data->source_buffer);
    
    // Keywords tag (blue)
    GtkTextTag *keyword_tag = gtk_text_tag_new("keyword");
    g_object_set(keyword_tag, "foreground", "#0000FF", NULL);
    gtk_text_tag_table_add(tag_table, keyword_tag);
    
    // Directives tag (dark blue/bold)
    GtkTextTag *directive_tag = gtk_text_tag_new("directive");
    g_object_set(directive_tag, "foreground", "#0066CC", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_tag_table_add(tag_table, directive_tag);
    
    // Strings tag (green)
    GtkTextTag *string_tag = gtk_text_tag_new("string");
    g_object_set(string_tag, "foreground", "#008000", NULL);
    gtk_text_tag_table_add(tag_table, string_tag);
    
    // Comments tag (gray/italic)
    GtkTextTag *comment_tag = gtk_text_tag_new("comment");
    g_object_set(comment_tag, "foreground", "#808080", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_text_tag_table_add(tag_table, comment_tag);
#endif
    
    GtkWidget *scrolled_editor = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_editor), app_data->editor);
    // Make editor expand to fill available space
    gtk_widget_set_vexpand(scrolled_editor, TRUE);
    gtk_widget_set_hexpand(scrolled_editor, TRUE);
    gtk_widget_set_valign(scrolled_editor, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(editor_panel), scrolled_editor);
    
    g_signal_connect(app_data->source_buffer, "changed", G_CALLBACK(on_text_changed), app_data);
    
    gtk_paned_set_start_child(GTK_PANED(right_vpaned), editor_panel);
    
    // Bottom panel: Logs
    GtkWidget *logs_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(logs_panel, 12);
    gtk_widget_set_margin_end(logs_panel, 12);
    gtk_widget_set_margin_top(logs_panel, 12);
    gtk_widget_set_margin_bottom(logs_panel, 12);
    
    GtkWidget *logs_label = gtk_label_new("Logs");
    gtk_widget_add_css_class(logs_label, "title");
    gtk_box_append(GTK_BOX(logs_panel), logs_label);
    
    app_data->logs_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app_data->logs_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(app_data->logs_text), TRUE);
    
    // Add text tags for log formatting
    GtkTextBuffer *log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app_data->logs_text));
    GtkTextTagTable *log_tag_table = gtk_text_buffer_get_tag_table(log_buffer);
    
    // Error tag (red)
    GtkTextTag *error_tag = gtk_text_tag_new("error");
    g_object_set(error_tag, "foreground", "#CC0000", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_tag_table_add(log_tag_table, error_tag);
    
    // Success tag (green)
    GtkTextTag *success_tag = gtk_text_tag_new("success");
    g_object_set(success_tag, "foreground", "#008000", NULL);
    gtk_text_tag_table_add(log_tag_table, success_tag);
    
    // Info tag (blue)
    GtkTextTag *info_tag = gtk_text_tag_new("info");
    g_object_set(info_tag, "foreground", "#0066CC", NULL);
    gtk_text_tag_table_add(log_tag_table, info_tag);
    
    // Set better font size for readability using CSS
    gtk_widget_add_css_class(app_data->logs_text, "log-text");
    // Create a CSS provider for log text styling
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider,
        "textview.log-text { font-family: monospace; font-size: 10pt; }");
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(app_data->logs_text),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    GtkWidget *scrolled_logs = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_logs), app_data->logs_text);
    // Increase logs height - set minimum but allow expansion
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_logs), 300);
    gtk_widget_set_vexpand(scrolled_logs, TRUE);
    gtk_widget_set_hexpand(scrolled_logs, TRUE);
    gtk_widget_set_valign(scrolled_logs, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(logs_panel), scrolled_logs);
    
    gtk_paned_set_end_child(GTK_PANED(right_vpaned), logs_panel);
    // Adjust paned position - give more space to both editor and logs
    // For 900px window: editor ~550px, logs ~300px (with margins)
    gtk_paned_set_position(GTK_PANED(right_vpaned), 600);
    
    gtk_paned_set_end_child(GTK_PANED(hpaned), right_vpaned);
    // Adjust horizontal paned - give more space to editor area
    // Set initial position to match left panel minimum width
    gtk_paned_set_position(GTK_PANED(hpaned), 250);
    // Prevent left panel from shrinking too much
    gtk_paned_set_shrink_start_child(GTK_PANED(hpaned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(hpaned), TRUE);
    
    // Initial file list refresh
    refresh_file_list(app_data);
    
    gtk_window_present(GTK_WINDOW(app_data->window));
}
