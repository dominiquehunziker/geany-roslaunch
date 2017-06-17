/*
 * plugin.c
 * 
 * Copyright 2016 Dominique Hunziker <dominique.hunziker@gmail.com>
 * 
 * This file is part of geany-roslaunch.
 *
 * geany-roslaunch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * geany-roslaunch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with geany-roslaunch. If not, see <http://www.gnu.org/licenses/>.
 */

#include <regex.h>
#include <stdlib.h>

#include <geanyplugin.h>

#include "roswrapper.h"

enum {
    KB_ROSLAUNCH_SEARCH_SYMBOL,
    KB_COUNT
};

/* module level private variables */
static GeanyPlugin* geany_plugin = NULL;
static GeanyData* geany_data = NULL;

/* init/cleanup level private variables */
static GtkWidget* popup_sep_item = NULL;
static GtkWidget* popup_ff_item = NULL;
static GeanyDocument* current_doc = NULL;

static regex_t regex;
static gulong handler_id;
static gulong popup_handler_id;
static gchar popup_path[4096];

/* 
 * Check the given text position for a loadable roslaunch file.
 *
 * @param[in] sci
 *          Scintilla object
 * @param[in] pos
 *          Click position in the Scintilla object
 * @param[out] path
 *          Output buffer where the extracted path will be stored on success
 *          The calling function is responsible to provide a buffer with minimal
 *          length of 4096 bytes. (maximum path length on linux; including nul)
 *
 * @return TRUE if the path contains a valid argument; FALSE otherwise
 */
static gboolean extract_path(ScintillaObject* const sci, const gint pos, char* path) {
    // get the line corresponding to the given position
    const gint line_no = sci_get_line_from_position(sci, pos);
    const gint line_pos = pos - sci_get_position_from_line(sci, line_no);
    const gchar* const line = sci_get_line(sci, line_no);

    // search for the opening '"'
    gint start = -1;

    for (gint idx = line_pos - 1; idx >= 0; --idx) {
        if (line[idx] == '"') {
            start = idx;
            break;
        }
    }

    if (start == -1)
        return FALSE;

    // search for the closing '"'
    gint end = -1;

    for (gint idx = line_pos; line[idx] != '\0'; ++idx) {
        if (line[idx] == '"') {
            end = idx;
            break;
        }
    }

    if (end == -1)
        return FALSE;

    // extract the match and free the line
    const gint len = end - start - 1;
    gchar match[len + 1];

    memcpy(match, &line[start + 1], len);
    match[len] = '\0';
    
    g_free((gchar*) line);

    // resolve '$([find|env] [val])' directives
    gint i_idx = 0;
    gint o_idx = 0;

    for (;;) {
        regmatch_t m[3];

        int ret = regexec(&regex, match + i_idx, 3, m, 0);
        if (ret == REG_NOMATCH) {
            // no more matches -- copy the rest of the data to the output
            if (strlen(match + i_idx) + o_idx + 1 > 4096)
                return FALSE;  // buffer "overflow"
            
            strcpy(path + o_idx, match + i_idx);
            break;
        } else if (ret) {
            return FALSE;  // regex error
        } else {
            // found a match -- first copy the data before the match to the output 
            memcpy(path + o_idx, match + i_idx, m[0].rm_so);
            o_idx += m[0].rm_so;
            
            // fetch the value of '$([find|env] [val])'
            char val[m[2].rm_eo - m[2].rm_so + 1];
            memcpy(val, match + i_idx + m[2].rm_so, m[2].rm_eo - m[2].rm_so);
            val[m[2].rm_eo - m[2].rm_so] = '\0';
            
            // determine the operation
            if (m[1].rm_eo - m[1].rm_so == 4) {
                // OP: find -- locate the package and substitute the absolute path
                char* pkg_path = get_package_path(val);
                if (!pkg_path) {
                    ui_set_statusbar(FALSE, "Unable to locate package '%s'", val);
                    return FALSE;  // could not resolve package
                }
                
                if (strlen(pkg_path) + o_idx + 1 > 4096) {
                    free(pkg_path);
                    return FALSE;  // buffer "overflow"
                }
                
                memcpy(path + o_idx, pkg_path, strlen(pkg_path));
                o_idx += strlen(pkg_path);
                
                free(pkg_path);
            } else if (m[1].rm_eo - m[1].rm_so == 3) {
                // OP: env -- fetch the environment variable and substitute it
                const char* envvar = getenv(val);
                
                if (strlen(envvar) + o_idx + 1 > 4096)
                    return FALSE;  // buffer "overflow"
                
                memcpy(path + o_idx, envvar, strlen(envvar));
                o_idx += strlen(envvar);
            } else {
                return FALSE;  // invalid operation
            }

            i_idx += m[0].rm_eo;
        }
    }

    // run a basic sanity check on the path
    for (gint idx = 0; idx < strlen(path); ++idx) {
        if (path[idx] == '$' || path[idx] == '(' || path[idx] == ')' || path[idx] == ';') {
            ui_set_statusbar(FALSE, "Invalid path after substitution: %s", path);
            return FALSE;  // invalid path
        }
    }

    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* copied from sciwrappers.c */

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

static gint sci_get_position_from_xy(ScintillaObject *sci, gint x, gint y, gboolean nearby) {
    /* for nearby return -1 if there is no character near to the x,y point. */
    return (gint) SSM(sci, (nearby) ? SCI_POSITIONFROMPOINTCLOSE : SCI_POSITIONFROMPOINT, (uptr_t) x, y);
}

/*----------------------------------------------------------------------------*/
/* mostly copied from editor.c */

static gboolean on_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    GeanyEditor* editor = data;

    static gint click_pos;  /* text position where the mouse was clicked */

    if (event->x > 0.0 && event->y > 0.0)
        click_pos = sci_get_position_from_xy(editor->sci,(gint) event->x, (gint) event->y, FALSE);
    else
        click_pos = sci_get_current_position(editor->sci);
    
    if (event->button == 1) {
        const guint state = keybindings_get_modifiers(event->state);

        if (event->type == GDK_BUTTON_PRESS && state == GEANY_PRIMARY_MOD_MASK) {
            sci_set_current_position(editor->sci, click_pos, FALSE);

            char path[4096];
            if (extract_path(editor->sci, click_pos, path))
                return document_open_file(path, FALSE, NULL, NULL) == NULL ? FALSE : TRUE;
        }
    }

    return FALSE;
}

/*----------------------------------------------------------------------------*/

static void on_document_activate(GObject* obj, GeanyDocument* doc, gpointer user_data) {
    if (current_doc && current_doc->editor && current_doc->editor->sci)
        g_signal_handler_disconnect(current_doc->editor->sci, handler_id);

    current_doc = doc;

    if (current_doc && current_doc->editor && current_doc->editor->sci)
        handler_id = g_signal_connect(current_doc->editor->sci, "button-press-event",
                G_CALLBACK(on_button_press_event), current_doc->editor);
}

static void kb_search_symbol(guint key_id) {
    // sanity checks before we begin
    if (!current_doc || !current_doc->editor || !current_doc->editor->sci)
        return;

    char path[4096];
    if (extract_path(
            current_doc->editor->sci, sci_get_current_position(current_doc->editor->sci), path))
        document_open_file(path, FALSE, NULL, NULL);
}

static void on_find_file(GtkMenuItem* menuitem, gpointer pdata) {
    document_open_file(popup_path, FALSE, NULL, NULL);
}

static void on_update_editor_menu(GObject* obj, const gchar* word, gint pos,
        GeanyDocument* doc, gpointer user_data) {
    gtk_widget_set_sensitive(popup_ff_item, extract_path(doc->editor->sci, pos, popup_path));
}

static gboolean roslaunch_init(GeanyPlugin* plugin, gpointer user_data) {
    if (regcomp(&regex, "\\$(\\(find\\|env\\) \\+\\([a-zA-Z][a-zA-Z0-9_]*\\))", 0))
        return FALSE;

    plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE,
            G_CALLBACK(on_document_activate), NULL);

    // keybinding
    GeanyKeyGroup* key_group = plugin_set_key_group(geany_plugin, "roslaunch", KB_COUNT, NULL);;

    keybindings_set_item(key_group, KB_ROSLAUNCH_SEARCH_SYMBOL, kb_search_symbol,
            0, 0, "roslaunch_search_symbol", "Search for symbol in workspace", NULL);

    // popup menu
    popup_sep_item = gtk_separator_menu_item_new();
    gtk_widget_show(popup_sep_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu), popup_sep_item);

    popup_ff_item = gtk_menu_item_new_with_mnemonic(_("Open File (roslaunch)"));
    gtk_widget_show(popup_ff_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu), popup_ff_item);

    popup_handler_id = g_signal_connect(
            (gpointer) popup_ff_item, "activate", G_CALLBACK(on_find_file), NULL);

    // Geany signals
    plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE,
            G_CALLBACK(on_document_activate), NULL);
    plugin_signal_connect(geany_plugin, NULL, "update-editor-menu", TRUE,
            G_CALLBACK(on_update_editor_menu), NULL);

    return TRUE;
}

static void roslaunch_cleanup(GeanyPlugin* plugin, gpointer user_data) {
    if (current_doc && current_doc->editor && current_doc->editor->sci)
        g_signal_handler_disconnect(current_doc->editor->sci, handler_id);

    g_signal_handler_disconnect(popup_ff_item, popup_handler_id);
    gtk_widget_destroy(popup_ff_item);
    gtk_widget_destroy(popup_sep_item);

    current_doc = NULL;
}

G_MODULE_EXPORT void geany_load_module(GeanyPlugin* plugin) {
    geany_plugin = plugin;
    geany_data = plugin->geany_data;

    /* Step 1: Set metadata */
    plugin->info->name = "roslaunch";
    plugin->info->description = "Add support for roslaunch files";
    plugin->info->version = "0.3.1";
    plugin->info->author = "Dominique Hunziker <dominique.hunziker@gmail.com>";
    
    /* Step 2: Set functions */
    plugin->funcs->init = roslaunch_init;
    plugin->funcs->cleanup = roslaunch_cleanup;

    /* Step 3: Register! */
    GEANY_PLUGIN_REGISTER(plugin, 225);
}
