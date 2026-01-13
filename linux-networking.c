/*
 * Dave's Network Inquisition
 * Development Version: v1.0-dev-2025-01-13
 * Website: https://prowse.tech
 */

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>

// Structure to hold application state
typedef struct {
    GtkWidget *window;
    GtkWidget *ip_info_text;
    GtkWidget *route_info_text;
    GtkWidget *ping_entry;
    GtkWidget *ping_output;
    GtkWidget *ping_button;
    GtkWidget *dig_entry;
    GtkWidget *dig_output;
    GtkWidget *dig_button;
    GtkWidget *network_graph;
    GtkWidget *graph_frame;
    GtkWidget *interface_dropdown;
    GtkWidget *terminal;
    GtkWidget *terminal_frame;
    GtkWidget *main_box;
    GtkWidget *paned;
    GtkWidget *terminal_button_bar;
    
    // Network statistics
    unsigned long long prev_rx_bytes;
    unsigned long long prev_tx_bytes;
    char selected_interface[64];
    
    // Graph data (60 data points for 60 seconds)
    double rx_data[60];
    double tx_data[60];
    int graph_position;
    
    // Terminal font size
    double terminal_font_scale;
    
    // Graph maximization state
    gboolean graph_maximized;
    int original_graph_height;
    
    guint refresh_timer;
    guint graph_timer;
    guint terminal_visibility_check;
} AppData;

// Function prototypes
static void activate(GtkApplication *app, gpointer user_data);
static void update_ip_info(AppData *data);
static void update_route_info(AppData *data);
static void on_ping_clicked(GtkButton *button, gpointer user_data);
static void on_ping_activate(GtkEntry *entry, gpointer user_data);
static void on_dig_clicked(GtkButton *button, gpointer user_data);
static void on_dig_activate(GtkEntry *entry, gpointer user_data);
static gboolean refresh_network_info(gpointer user_data);
static gboolean update_network_graph(gpointer user_data);
static void network_graph_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
static void get_interface_stats(const char *interface, unsigned long long *rx_bytes, unsigned long long *tx_bytes);
static void populate_interface_dropdown(AppData *data);
static void on_interface_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);
static void flash_button_green(GtkWidget *button);
static gboolean reset_button_style(gpointer button);
static gboolean on_terminal_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data);
static gboolean on_terminal_key(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
static void on_graph_double_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
static gboolean check_terminal_visibility(gpointer user_data);
static void on_terminal_button_clicked(GtkButton *button, gpointer user_data);

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    
    app = gtk_application_new("org.linux.networking", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = g_new0(AppData, 1);
    
    // Load CSS for button styling
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css_data = ".success { background: #4CAF50; color: white; }";
    gtk_css_provider_load_from_string(css_provider, css_data);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // Create main window
    data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(data->window), "🌐 Dave's Network Inquisition - https://prowse.tech");
    gtk_window_set_default_size(GTK_WINDOW(data->window), 1400, 900);
    gtk_window_maximize(GTK_WINDOW(data->window));
    
    // Create scrolled window for entire content
    GtkWidget *main_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_window_set_child(GTK_WINDOW(data->window), main_scroll);
    
    // Create main vertical box
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 10);
    gtk_widget_set_margin_bottom(main_box, 10);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(main_scroll), main_box);
    data->main_box = main_box;
    
    // ROW 1: IP Address Info and IP Route Info (increased height by 50%)
    GtkWidget *row1_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_vexpand(row1_box, TRUE);
    gtk_widget_set_size_request(row1_box, -1, 135);
    gtk_box_append(GTK_BOX(main_box), row1_box);
    
    // IP Address Info
    GtkWidget *ip_frame = gtk_frame_new("📡 IP ADDRESS INFO");
    gtk_widget_set_hexpand(ip_frame, TRUE);
    gtk_box_append(GTK_BOX(row1_box), ip_frame);
    
    GtkWidget *ip_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ip_scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_frame_set_child(GTK_FRAME(ip_frame), ip_scroll);
    
    data->ip_info_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->ip_info_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(data->ip_info_text), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ip_scroll), data->ip_info_text);
    
    // IP Route Info
    GtkWidget *route_frame = gtk_frame_new("🗺️ IP ROUTE INFO");
    gtk_widget_set_hexpand(route_frame, TRUE);
    gtk_box_append(GTK_BOX(row1_box), route_frame);
    
    GtkWidget *route_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(route_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_frame_set_child(GTK_FRAME(route_frame), route_scroll);
    
    data->route_info_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->route_info_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(data->route_info_text), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(route_scroll), data->route_info_text);
    
    // ROW 2 & 3: PING/DIG and Graph in a paned widget (resizable)
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(main_box), paned);
    data->paned = paned;
    
    // Top pane: PING and DIG
    GtkWidget *row2_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_vexpand(row2_box, TRUE);
    gtk_widget_set_size_request(row2_box, -1, 300);
    gtk_paned_set_start_child(GTK_PANED(paned), row2_box);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);
    
    // PING Tool (on left)
    GtkWidget *ping_frame = gtk_frame_new("📶 PING");
    gtk_widget_set_hexpand(ping_frame, TRUE);
    gtk_box_append(GTK_BOX(row2_box), ping_frame);
    
    GtkWidget *ping_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(ping_vbox, 5);
    gtk_widget_set_margin_end(ping_vbox, 5);
    gtk_widget_set_margin_top(ping_vbox, 5);
    gtk_widget_set_margin_bottom(ping_vbox, 5);
    gtk_frame_set_child(GTK_FRAME(ping_frame), ping_vbox);
    
    GtkWidget *ping_input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(ping_vbox), ping_input_box);
    
    data->ping_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->ping_entry), "Enter hostname or IP (e.g., 8.8.8.8)");
    gtk_widget_set_hexpand(data->ping_entry, TRUE);
    g_signal_connect(data->ping_entry, "activate", G_CALLBACK(on_ping_activate), data);
    gtk_box_append(GTK_BOX(ping_input_box), data->ping_entry);
    
    data->ping_button = gtk_button_new_with_label("GO");
    g_signal_connect(data->ping_button, "clicked", G_CALLBACK(on_ping_clicked), data);
    gtk_box_append(GTK_BOX(ping_input_box), data->ping_button);
    
    GtkWidget *ping_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(ping_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ping_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_append(GTK_BOX(ping_vbox), ping_scroll);
    
    data->ping_output = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->ping_output), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(data->ping_output), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(data->ping_output), GTK_WRAP_WORD);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ping_scroll), data->ping_output);
    
    // DIG Tool (on right)
    GtkWidget *dig_frame = gtk_frame_new("🔍 DIG");
    gtk_widget_set_hexpand(dig_frame, TRUE);
    gtk_box_append(GTK_BOX(row2_box), dig_frame);
    
    GtkWidget *dig_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(dig_vbox, 5);
    gtk_widget_set_margin_end(dig_vbox, 5);
    gtk_widget_set_margin_top(dig_vbox, 5);
    gtk_widget_set_margin_bottom(dig_vbox, 5);
    gtk_frame_set_child(GTK_FRAME(dig_frame), dig_vbox);
    
    GtkWidget *dig_input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(dig_vbox), dig_input_box);
    
    data->dig_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->dig_entry), "Enter domain name (e.g., google.com)");
    gtk_widget_set_hexpand(data->dig_entry, TRUE);
    g_signal_connect(data->dig_entry, "activate", G_CALLBACK(on_dig_activate), data);
    gtk_box_append(GTK_BOX(dig_input_box), data->dig_entry);
    
    data->dig_button = gtk_button_new_with_label("GO");
    g_signal_connect(data->dig_button, "clicked", G_CALLBACK(on_dig_clicked), data);
    gtk_box_append(GTK_BOX(dig_input_box), data->dig_button);
    
    GtkWidget *dig_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(dig_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dig_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_append(GTK_BOX(dig_vbox), dig_scroll);
    
    data->dig_output = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->dig_output), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(data->dig_output), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(data->dig_output), GTK_WRAP_WORD);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(dig_scroll), data->dig_output);
    
    // Bottom pane of paned: Network Send/Receive Graph
    GtkWidget *graph_frame = gtk_frame_new("📊 Network SEND/RECEIVE");
    gtk_widget_set_vexpand(graph_frame, TRUE);
    gtk_widget_set_size_request(graph_frame, -1, 200);
    gtk_paned_set_end_child(GTK_PANED(paned), graph_frame);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);
    data->graph_frame = graph_frame;
    data->graph_maximized = FALSE;
    data->original_graph_height = 200;
    
    // Set initial position (300px for PING/DIG)
    gtk_paned_set_position(GTK_PANED(paned), 300);
    
    // Add double-click gesture to graph frame
    GtkGesture *click_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), GDK_BUTTON_PRIMARY);
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(on_graph_double_click), data);
    gtk_widget_add_controller(graph_frame, GTK_EVENT_CONTROLLER(click_gesture));
    
    GtkWidget *graph_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(graph_vbox, 5);
    gtk_widget_set_margin_end(graph_vbox, 5);
    gtk_widget_set_margin_top(graph_vbox, 5);
    gtk_widget_set_margin_bottom(graph_vbox, 5);
    gtk_frame_set_child(GTK_FRAME(graph_frame), graph_vbox);
    
    // Interface selector with GtkDropDown
    GtkWidget *interface_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(graph_vbox), interface_box);
    
    GtkWidget *interface_label = gtk_label_new("Interface:");
    gtk_box_append(GTK_BOX(interface_box), interface_label);
    
    // Create string list for interfaces
    GtkStringList *string_list = gtk_string_list_new(NULL);
    data->interface_dropdown = gtk_drop_down_new(G_LIST_MODEL(string_list), NULL);
    gtk_widget_set_hexpand(data->interface_dropdown, FALSE);
    g_signal_connect(data->interface_dropdown, "notify::selected", G_CALLBACK(on_interface_changed), data);
    gtk_box_append(GTK_BOX(interface_box), data->interface_dropdown);
    
    populate_interface_dropdown(data);
    
    // Drawing area for graph
    data->network_graph = gtk_drawing_area_new();
    gtk_widget_set_vexpand(data->network_graph, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(data->network_graph),
                                   network_graph_draw, data, NULL);
    gtk_box_append(GTK_BOX(graph_vbox), data->network_graph);
    
    // Add color key for graph
    GtkWidget *key_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(key_box, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(graph_vbox), key_box);
    
    // RX (green) key
    GtkWidget *rx_label = gtk_label_new("🟩 RX (Download)");
    gtk_box_append(GTK_BOX(key_box), rx_label);
    
    // TX (red) key
    GtkWidget *tx_label = gtk_label_new("🟥 TX (Upload)");
    gtk_box_append(GTK_BOX(key_box), tx_label);
    
    // Initialize graph data
    for (int i = 0; i < 60; i++) {
        data->rx_data[i] = 0.0;
        data->tx_data[i] = 0.0;
    }
    data->graph_position = 0;
    
    // ROW 4: Terminal
    GtkWidget *terminal_frame = gtk_frame_new("💻 TERMINAL");
    gtk_widget_set_vexpand(terminal_frame, TRUE);
    gtk_widget_set_size_request(terminal_frame, -1, 250);
    gtk_box_append(GTK_BOX(main_box), terminal_frame);
    data->terminal_frame = terminal_frame;
    
    GtkWidget *terminal_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(terminal_box, 5);
    gtk_widget_set_margin_end(terminal_box, 5);
    gtk_widget_set_margin_top(terminal_box, 5);
    gtk_widget_set_margin_bottom(terminal_box, 5);
    gtk_frame_set_child(GTK_FRAME(terminal_frame), terminal_box);
    
    // Create VTE terminal
    data->terminal = vte_terminal_new();
    gtk_widget_set_vexpand(data->terminal, TRUE);
    data->terminal_font_scale = 1.0;
    
    // Add scroll event controller for Ctrl+Scroll zoom
    GtkEventController *scroll_controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(on_terminal_scroll), data);
    gtk_widget_add_controller(data->terminal, scroll_controller);
    
    // Add key event controller for Ctrl+Shift+Plus and Ctrl+Minus zoom
    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_terminal_key), data);
    gtk_widget_add_controller(data->terminal, key_controller);
    
    gtk_box_append(GTK_BOX(terminal_box), data->terminal);
    
    // Spawn shell in terminal
    char **envp = g_get_environ();
    char *shell = vte_get_user_shell();
    char *argv[] = {shell, NULL};
    
    vte_terminal_spawn_async(
        VTE_TERMINAL(data->terminal),
        VTE_PTY_DEFAULT,
        NULL,           // working directory
        argv,           // argv
        envp,           // environment
        G_SPAWN_DEFAULT,
        NULL, NULL,     // child setup
        NULL,           // child setup data destroy
        -1,             // timeout
        NULL,           // cancellable
        NULL,           // callback
        NULL            // user data
    );
    
    g_strfreev(envp);
    g_free(shell);
    
    // Terminal visibility button bar (initially hidden)
    data->terminal_button_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(data->terminal_button_bar, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(data->terminal_button_bar, TRUE);
    gtk_box_append(GTK_BOX(main_box), data->terminal_button_bar);
    
    GtkWidget *terminal_button = gtk_button_new_with_label("TERMINAL ↓");
    gtk_widget_set_hexpand(terminal_button, TRUE);
    g_signal_connect(terminal_button, "clicked", G_CALLBACK(on_terminal_button_clicked), data);
    gtk_box_append(GTK_BOX(data->terminal_button_bar), terminal_button);
    
    gtk_widget_set_visible(data->terminal_button_bar, FALSE);
    
    // Initial updates
    update_ip_info(data);
    update_route_info(data);
    
    // Set up timers
    data->refresh_timer = g_timeout_add_seconds(30, refresh_network_info, data);
    data->graph_timer = g_timeout_add_seconds(1, update_network_graph, data);
    data->terminal_visibility_check = g_timeout_add(500, check_terminal_visibility, data);
    
    gtk_window_present(GTK_WINDOW(data->window));
}

static void update_ip_info(AppData *data) {
    struct ifaddrs *ifaddr, *ifa;
    char host[256];
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->ip_info_text));
    GString *output = g_string_new("");
    
    if (getifaddrs(&ifaddr) == -1) {
        g_string_append(output, "Error getting interface information\n");
    } else {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL)
                continue;
            
            int family = ifa->ifa_addr->sa_family;
            
            if (family == AF_INET) {
                if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                               host, sizeof(host), NULL, 0, NI_NUMERICHOST) == 0) {
                    g_string_append_printf(output, "Interface: %s\n", ifa->ifa_name);
                    g_string_append_printf(output, "  IPv4: %s\n\n", host);
                }
            }
        }
        freeifaddrs(ifaddr);
    }
    
    gtk_text_buffer_set_text(buffer, output->str, -1);
    g_string_free(output, TRUE);
}

static void update_route_info(AppData *data) {
    FILE *fp;
    char line[512];
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->route_info_text));
    GString *output = g_string_new("");
    
    fp = popen("ip route show", "r");
    if (fp == NULL) {
        g_string_append(output, "Error getting route information\n");
    } else {
        while (fgets(line, sizeof(line), fp) != NULL) {
            g_string_append(output, line);
        }
        pclose(fp);
    }
    
    gtk_text_buffer_set_text(buffer, output->str, -1);
    g_string_free(output, TRUE);
}

static void on_dig_activate(GtkEntry *entry, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    flash_button_green(data->dig_button);
    on_dig_clicked(NULL, user_data);
}

static void on_dig_clicked(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (button != NULL) {
        flash_button_green(GTK_WIDGET(button));
    }
    
    GtkEntryBuffer *buffer = gtk_entry_get_buffer(GTK_ENTRY(data->dig_entry));
    const char *domain = gtk_entry_buffer_get_text(buffer);
    
    if (strlen(domain) == 0) {
        GtkTextBuffer *output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->dig_output));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(output_buffer, &end);
        gtk_text_buffer_insert(output_buffer, &end, "Please enter a domain name\n", -1);
        return;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "dig %s", domain);
    
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        GtkTextBuffer *output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->dig_output));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(output_buffer, &end);
        gtk_text_buffer_insert(output_buffer, &end, "Error executing dig command\n", -1);
        return;
    }
    
    GtkTextBuffer *output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->dig_output));
    GtkTextIter end;
    
    // Add separator and command to history
    gtk_text_buffer_get_end_iter(output_buffer, &end);
    char header[600];
    snprintf(header, sizeof(header), "\n=== dig %s ===\n", domain);
    gtk_text_buffer_insert(output_buffer, &end, header, -1);
    
    // Read output
    GString *result = g_string_new("");
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        g_string_append(result, line);
    }
    pclose(fp);
    
    // Insert result
    gtk_text_buffer_get_end_iter(output_buffer, &end);
    gtk_text_buffer_insert(output_buffer, &end, result->str, -1);
    g_string_free(result, TRUE);
    
    // Add separator at end
    gtk_text_buffer_get_end_iter(output_buffer, &end);
    gtk_text_buffer_insert(output_buffer, &end, "══════════════════════════════════════\n\n", -1);
    
    // Scroll to end
    GtkTextMark *mark = gtk_text_buffer_create_mark(output_buffer, NULL, &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(data->dig_output), mark, 0.0, FALSE, 0.0, 0.0);
}

static void on_ping_activate(GtkEntry *entry, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    flash_button_green(data->ping_button);
    on_ping_clicked(NULL, user_data);
}

static void on_ping_clicked(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (button != NULL) {
        flash_button_green(GTK_WIDGET(button));
    }
    
    GtkEntryBuffer *buffer = gtk_entry_get_buffer(GTK_ENTRY(data->ping_entry));
    const char *host = gtk_entry_buffer_get_text(buffer);
    
    if (strlen(host) == 0) {
        GtkTextBuffer *output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->ping_output));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(output_buffer, &end);
        gtk_text_buffer_insert(output_buffer, &end, "Please enter a hostname or IP address\n", -1);
        return;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "ping -c 4 %s 2>&1", host);
    
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        GtkTextBuffer *output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->ping_output));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(output_buffer, &end);
        gtk_text_buffer_insert(output_buffer, &end, "Error executing ping command\n", -1);
        return;
    }
    
    GtkTextBuffer *output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->ping_output));
    GtkTextIter end;
    
    // Add separator and command to history
    gtk_text_buffer_get_end_iter(output_buffer, &end);
    char header[600];
    snprintf(header, sizeof(header), "\n=== ping %s ===\n", host);
    gtk_text_buffer_insert(output_buffer, &end, header, -1);
    
    // Read and display output line by line
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        gtk_text_buffer_get_end_iter(output_buffer, &end);
        gtk_text_buffer_insert(output_buffer, &end, line, -1);
        
        // Scroll to end and process GTK events to show live output
        GtkTextMark *mark = gtk_text_buffer_get_insert(output_buffer);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(data->ping_output), mark, 0.0, FALSE, 0.0, 0.0);
        
        while (g_main_context_pending(NULL)) {
            g_main_context_iteration(NULL, FALSE);
        }
    }
    
    pclose(fp);
    
    // Add separator at end
    gtk_text_buffer_get_end_iter(output_buffer, &end);
    gtk_text_buffer_insert(output_buffer, &end, "══════════════════════════════════════\n\n", -1);
    
    // Scroll to end
    gtk_text_buffer_get_end_iter(output_buffer, &end);
    GtkTextMark *mark = gtk_text_buffer_create_mark(output_buffer, NULL, &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(data->ping_output), mark, 0.0, FALSE, 0.0, 0.0);
}

static gboolean refresh_network_info(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    update_ip_info(data);
    update_route_info(data);
    return G_SOURCE_CONTINUE;
}

static void get_interface_stats(const char *interface, unsigned long long *rx_bytes, unsigned long long *tx_bytes) {
    char path[256];
    FILE *fp;
    
    *rx_bytes = 0;
    *tx_bytes = 0;
    
    // Read RX bytes
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", interface);
    fp = fopen(path, "r");
    if (fp != NULL) {
        if (fscanf(fp, "%llu", rx_bytes) != 1) {
            *rx_bytes = 0;
        }
        fclose(fp);
    }
    
    // Read TX bytes
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", interface);
    fp = fopen(path, "r");
    if (fp != NULL) {
        if (fscanf(fp, "%llu", tx_bytes) != 1) {
            *tx_bytes = 0;
        }
        fclose(fp);
    }
}

static gboolean update_network_graph(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    if (strlen(data->selected_interface) == 0) {
        return G_SOURCE_CONTINUE;
    }
    
    unsigned long long rx_bytes, tx_bytes;
    get_interface_stats(data->selected_interface, &rx_bytes, &tx_bytes);
    
    // Calculate speeds (bytes per second)
    double rx_speed = 0.0;
    double tx_speed = 0.0;
    
    if (data->prev_rx_bytes > 0) {
        rx_speed = (double)(rx_bytes - data->prev_rx_bytes);
        tx_speed = (double)(tx_bytes - data->prev_tx_bytes);
    }
    
    data->prev_rx_bytes = rx_bytes;
    data->prev_tx_bytes = tx_bytes;
    
    // Update graph data
    data->rx_data[data->graph_position] = rx_speed / 1024.0; // Convert to KB/s
    data->tx_data[data->graph_position] = tx_speed / 1024.0;
    data->graph_position = (data->graph_position + 1) % 60;
    
    // Trigger redraw
    gtk_widget_queue_draw(data->network_graph);
    
    return G_SOURCE_CONTINUE;
}

static void network_graph_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    // Grid lines
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.5);
    cairo_set_line_width(cr, 1.0);
    
    for (int i = 0; i <= 5; i++) {
        double y = (height / 5.0) * i;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
        cairo_stroke(cr);
    }
    
    // Find max value for scaling
    double max_value = 1.0;
    for (int i = 0; i < 60; i++) {
        if (data->rx_data[i] > max_value) max_value = data->rx_data[i];
        if (data->tx_data[i] > max_value) max_value = data->tx_data[i];
    }
    max_value *= 1.2; // Add 20% headroom
    
    // Draw RX line (receive - green)
    cairo_set_source_rgb(cr, 0.2, 0.8, 0.2);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, 0, height);
    
    for (int i = 0; i < 60; i++) {
        int index = (data->graph_position + i) % 60;
        double x = (width / 60.0) * i;
        double y = height - (data->rx_data[index] / max_value * height);
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
    // Draw TX line (transmit - red)
    cairo_set_source_rgb(cr, 0.8, 0.2, 0.2);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, 0, height);
    
    for (int i = 0; i < 60; i++) {
        int index = (data->graph_position + i) % 60;
        double x = (width / 60.0) * i;
        double y = height - (data->tx_data[index] / max_value * height);
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
    // Draw legend and current values
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    
    char legend[256];
    int current_index = (data->graph_position - 1 + 60) % 60;
    snprintf(legend, sizeof(legend), "↓ RX: %.2f KB/s  ↑ TX: %.2f KB/s  Max: %.2f KB/s",
             data->rx_data[current_index], data->tx_data[current_index], max_value);
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, 10, 20);
    cairo_show_text(cr, legend);
}

static void populate_interface_dropdown(AppData *data) {
    struct ifaddrs *ifaddr, *ifa;
    GtkStringList *string_list = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(data->interface_dropdown)));
    
    if (getifaddrs(&ifaddr) == -1) {
        return;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        
        // Skip loopback
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;
        
        int family = ifa->ifa_addr->sa_family;
        
        if (family == AF_INET) {
            // Check if already added
            gboolean found = FALSE;
            guint n_items = g_list_model_get_n_items(G_LIST_MODEL(string_list));
            
            for (guint i = 0; i < n_items; i++) {
                const char *item = gtk_string_list_get_string(string_list, i);
                if (strcmp(item, ifa->ifa_name) == 0) {
                    found = TRUE;
                    break;
                }
            }
            
            if (!found) {
                gtk_string_list_append(string_list, ifa->ifa_name);
            }
        }
    }
    
    freeifaddrs(ifaddr);
    
    // Select first interface by default
    gtk_drop_down_set_selected(GTK_DROP_DOWN(data->interface_dropdown), 0);
}

static void on_interface_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    GtkStringList *string_list = GTK_STRING_LIST(gtk_drop_down_get_model(dropdown));
    
    if (selected != GTK_INVALID_LIST_POSITION) {
        const char *interface = gtk_string_list_get_string(string_list, selected);
        
        if (interface != NULL) {
            strncpy(data->selected_interface, interface, sizeof(data->selected_interface) - 1);
            data->selected_interface[sizeof(data->selected_interface) - 1] = '\0';
            
            // Reset graph data
            for (int i = 0; i < 60; i++) {
                data->rx_data[i] = 0.0;
                data->tx_data[i] = 0.0;
            }
            data->graph_position = 0;
            data->prev_rx_bytes = 0;
            data->prev_tx_bytes = 0;
        }
    }
}

static void flash_button_green(GtkWidget *button) {
    // Add CSS class to make button green temporarily
    gtk_widget_add_css_class(button, "success");
    
    // Schedule reset after 200ms
    g_timeout_add(200, reset_button_style, button);
}

static gboolean reset_button_style(gpointer button) {
    gtk_widget_remove_css_class(GTK_WIDGET(button), "success");
    return G_SOURCE_REMOVE;
}

static gboolean on_terminal_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    GdkModifierType state = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(controller));
    
    // Check if Ctrl is pressed
    if (state & GDK_CONTROL_MASK) {
        // Zoom in/out based on scroll direction
        if (dy < 0) {
            // Scroll up - zoom in
            data->terminal_font_scale *= 1.1;
        } else {
            // Scroll down - zoom out
            data->terminal_font_scale *= 0.9;
        }
        
        // Clamp scale between 0.5 and 3.0
        if (data->terminal_font_scale < 0.5) data->terminal_font_scale = 0.5;
        if (data->terminal_font_scale > 3.0) data->terminal_font_scale = 3.0;
        
        // Apply new font scale
        vte_terminal_set_font_scale(VTE_TERMINAL(data->terminal), data->terminal_font_scale);
        
        return TRUE; // Event handled
    }
    
    return FALSE; // Let normal scrolling happen
}

static gboolean on_terminal_key(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Check for Ctrl+Shift+Plus or Ctrl+Plus (zoom in)
    if ((state & GDK_CONTROL_MASK) && 
        ((keyval == GDK_KEY_plus || keyval == GDK_KEY_KP_Add || keyval == GDK_KEY_equal) ||
         ((state & GDK_SHIFT_MASK) && (keyval == GDK_KEY_plus || keyval == GDK_KEY_equal)))) {
        data->terminal_font_scale *= 1.1;
        if (data->terminal_font_scale > 3.0) data->terminal_font_scale = 3.0;
        vte_terminal_set_font_scale(VTE_TERMINAL(data->terminal), data->terminal_font_scale);
        return TRUE;
    }
    
    // Check for Ctrl+Minus (zoom out)
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_minus || keyval == GDK_KEY_KP_Subtract)) {
        data->terminal_font_scale *= 0.9;
        if (data->terminal_font_scale < 0.5) data->terminal_font_scale = 0.5;
        vte_terminal_set_font_scale(VTE_TERMINAL(data->terminal), data->terminal_font_scale);
        return TRUE;
    }
    
    // Check for Ctrl+0 (reset zoom)
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_0 || keyval == GDK_KEY_KP_0)) {
        data->terminal_font_scale = 1.0;
        vte_terminal_set_font_scale(VTE_TERMINAL(data->terminal), data->terminal_font_scale);
        return TRUE;
    }
    
    return FALSE;
}

static void on_graph_double_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Only respond to double-clicks
    if (n_press != 2) {
        return;
    }
    
    if (!data->graph_maximized) {
        // Maximize the graph (hide everything except paned widget and terminal)
        data->graph_maximized = TRUE;
        
        // Hide IP/Routes row, but keep the paned widget (which contains the graph)
        GtkWidget *child = gtk_widget_get_first_child(data->main_box);
        while (child != NULL) {
            GtkWidget *next = gtk_widget_get_next_sibling(child);
            // Hide everything except paned, terminal_frame, and terminal_button_bar
            if (child != data->paned && child != data->terminal_frame && child != data->terminal_button_bar) {
                gtk_widget_set_visible(child, FALSE);
            }
            child = next;
        }
        
        // Hide the PING/DIG section within the paned widget
        GtkWidget *ping_dig_box = gtk_paned_get_start_child(GTK_PANED(data->paned));
        if (ping_dig_box) {
            gtk_widget_set_visible(ping_dig_box, FALSE);
        }
        
        // Make graph take all paned space
        gtk_paned_set_position(GTK_PANED(data->paned), 0);
        
    } else {
        // Restore normal view
        data->graph_maximized = FALSE;
        
        // Show all rows again
        GtkWidget *child = gtk_widget_get_first_child(data->main_box);
        while (child != NULL) {
            GtkWidget *next = gtk_widget_get_next_sibling(child);
            // Don't show terminal button bar unless needed
            if (child != data->terminal_button_bar) {
                gtk_widget_set_visible(child, TRUE);
            }
            child = next;
        }
        
        // Show PING/DIG section again
        GtkWidget *ping_dig_box = gtk_paned_get_start_child(GTK_PANED(data->paned));
        if (ping_dig_box) {
            gtk_widget_set_visible(ping_dig_box, TRUE);
        }
        
        // Restore paned position
        gtk_paned_set_position(GTK_PANED(data->paned), 300);
    }
}

static gboolean check_terminal_visibility(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Check if terminal frame is realized and mapped
    if (!gtk_widget_get_realized(data->terminal_frame)) {
        return G_SOURCE_CONTINUE;
    }
    
    // Get the scrolled window's visible area
    GtkWidget *scrolled = GTK_WIDGET(gtk_widget_get_ancestor(data->main_box, GTK_TYPE_SCROLLED_WINDOW));
    if (scrolled == NULL) {
        return G_SOURCE_CONTINUE;
    }
    
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
    double scroll_value = gtk_adjustment_get_value(vadj);
    double page_size = gtk_adjustment_get_page_size(vadj);
    double visible_bottom = scroll_value + page_size;
    
    // Calculate terminal's position relative to the scrolled content
    int terminal_y = 0;
    GtkWidget *child = gtk_widget_get_first_child(data->main_box);
    while (child != NULL && child != data->terminal_frame) {
        if (gtk_widget_get_visible(child)) {
            terminal_y += gtk_widget_get_height(child);
        }
        child = gtk_widget_get_next_sibling(child);
    }
    
    // Show button if terminal is completely below visible area
    gboolean terminal_hidden = (terminal_y >= visible_bottom);
    gtk_widget_set_visible(data->terminal_button_bar, terminal_hidden);
    
    return G_SOURCE_CONTINUE;
}

static void on_terminal_button_clicked(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Scroll to terminal
    GtkWidget *scrolled = GTK_WIDGET(gtk_widget_get_ancestor(data->main_box, GTK_TYPE_SCROLLED_WINDOW));
    if (scrolled != NULL) {
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
        double upper = gtk_adjustment_get_upper(vadj);
        double page_size = gtk_adjustment_get_page_size(vadj);
        gtk_adjustment_set_value(vadj, upper - page_size);
    }
}