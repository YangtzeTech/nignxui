#include "nginx_ui.h"
#include <string.h>

gchar** extract_domains_from_config(const gchar *config_content) {
    GPtrArray *domains = g_ptr_array_new();
    gchar **lines = g_strsplit(config_content, "\n", -1);
    
    for (gint i = 0; lines[i] != NULL; i++) {
        gchar *line = g_strdup(lines[i]);
        gchar *stripped = g_strstrip(line);
        
        // Skip comments and empty lines
        if (!stripped || *stripped == '#' || *stripped == '\0') {
            g_free(line);
            continue;
        }
        
        // Check for server_name directive
        if (g_str_has_prefix(stripped, "server_name")) {
            // Extract domains from server_name line
            gchar *start = stripped + strlen("server_name");
            while (*start == ' ' || *start == '\t') start++;
            
            // Find comment or semicolon
            gchar *comment = strchr(start, '#');
            gchar *semicolon = strchr(start, ';');
            gchar *end = start + strlen(start);
            
            if (comment && (!semicolon || comment < semicolon)) {
                end = comment;
            } else if (semicolon) {
                end = semicolon;
            }
            
            // Extract the domain list part
            gchar *domain_list = g_strndup(start, end - start);
            gchar *trimmed = g_strstrip(domain_list);
            
            if (*trimmed) {
                // Split by spaces
                gchar **tokens = g_strsplit(trimmed, " ", -1);
                for (gint j = 0; tokens[j] != NULL; j++) {
                    gchar *token = g_strstrip(tokens[j]);
                    if (*token && *token != '_' && strcmp(token, "default_server") != 0) {
                        // Skip wildcards and regex patterns for now (can be enhanced)
                        if (*token != '~' && *token != '*') {
                            g_ptr_array_add(domains, g_strdup(token));
                        }
                    }
                }
                g_strfreev(tokens);
            }
            
            g_free(domain_list);
        }
        
        g_free(line);
    }
    
    g_strfreev(lines);
    g_ptr_array_add(domains, NULL);
    return (gchar**)g_ptr_array_free(domains, FALSE);
}

gboolean domain_exists_in_hosts(const gchar *domain) {
    FILE *fp = fopen(HOSTS_FILE, "r");
    if (!fp) return FALSE;
    
    gchar line[MAX_LINE_LENGTH];
    gboolean found = FALSE;
    
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments
        if (line[0] == '#') continue;
        
        // Check if domain is in this line
        if (strstr(line, domain)) {
            found = TRUE;
            break;
        }
    }
    
    fclose(fp);
    return found;
}

void add_domain_to_hosts(const gchar *domain) {
    if (domain_exists_in_hosts(domain)) {
        return; // Already exists
    }
    
    gchar *command = g_strdup_printf("sudo sh -c 'echo \"127.0.0.1 %s\" >> %s'", domain, HOSTS_FILE);
    gint result = system(command);
    g_free(command);
    
    if (result == 0) {
        // Success
    }
}
