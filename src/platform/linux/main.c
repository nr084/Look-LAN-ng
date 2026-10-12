#include <gtk/gtk.h>
#include "desktop_app.h"
#include "build_info.h"
static desktop_app_t app;
static GtkWidget *window,*nic,*range,*filter,*status,*table,*detail,*scan,*stop,*refresh,*export_button,*language;
static GtkListStore *rows;static bool finishing,updating;static const char *system_language;
static GtkWidget *menu_buttons[3];
static const char *tr(const char *en,const char *ru){return desktop_tr(&app,en,ru);}
static void message(const char *text){GtkWidget *dialog=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",text);gtk_dialog_run(GTK_DIALOG(dialog));gtk_widget_destroy(dialog);}
static void populate(void){
    updating=true;gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(nic));
    for(size_t i=0;i<app.interface_count;++i){char ip[32],label[256];ipv4_format(app.interfaces[i].ip,ip,sizeof(ip));snprintf(label,sizeof(label),"%s | %s/%u",app.interfaces[i].name,ip,app.interfaces[i].prefix);gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nic),label);}
    if(app.selected<app.interface_count){gtk_combo_box_set_active(GTK_COMBO_BOX(nic),(gint)app.selected);char text[4096];profile_ranges_text(&app.profile,text,sizeof(text));gtk_entry_set_text(GTK_ENTRY(range),app.profile.range_count==1 ? text : "");gtk_entry_set_placeholder_text(GTK_ENTRY(range),tr("Use all profile ranges","Все диапазоны профиля"));}updating=false;
}
static void update(void){
    GtkTreeSelection *selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(table));GtkTreeModel *selected_model=NULL;GtkTreeIter selected_iter;char *selected_ip=NULL;
    if(gtk_tree_selection_get_selected(selection,&selected_model,&selected_iter))gtk_tree_model_get(selected_model,&selected_iter,1,&selected_ip,-1);
    const char *search=gtk_entry_get_text(GTK_ENTRY(filter));char *fold=g_utf8_casefold(search,-1);gtk_list_store_clear(rows);
    for(size_t i=0;i<app.host_count;++i){char values[10][512];bool match=!fold[0];for(unsigned c=0;c<10;++c){desktop_cell(&app,i,c,values[c],sizeof(values[c]));if(!match){char *value=g_utf8_casefold(values[c],-1);match=strstr(value,fold)!=NULL;g_free(value);}}
        if(!match)continue;GtkTreeIter iter;gtk_list_store_append(rows,&iter);for(unsigned c=0;c<10;++c)gtk_list_store_set(rows,&iter,(gint)c,values[c],-1);if(selected_ip && !strcmp(selected_ip,values[1]))gtk_tree_selection_select_iter(selection,&iter);
    }g_free(fold);g_free(selected_ip);char text[256];snprintf(text,sizeof(text),"%s | %u / %u | %s: %zu",tr(app.scanning ? "Scanning" : "Ready",app.scanning ? "Сканирование" : "Готово"),app.completed,app.total,tr("Hosts","Узлы"),app.host_count);gtk_label_set_text(GTK_LABEL(status),text);
    gtk_widget_set_sensitive(scan,!app.scanning && app.selected<app.interface_count);gtk_widget_set_sensitive(stop,app.scanning);gtk_widget_set_sensitive(refresh,!app.scanning);gtk_widget_set_sensitive(nic,!app.scanning);gtk_widget_set_sensitive(range,!app.scanning);gtk_widget_set_sensitive(export_button,!app.scanning);
}
static void localize(void){
    gtk_button_set_label(GTK_BUTTON(menu_buttons[0]),tr("Open profile","Открыть профиль"));gtk_button_set_label(GTK_BUTTON(menu_buttons[1]),tr("Save profile","Сохранить профиль"));gtk_button_set_label(GTK_BUTTON(menu_buttons[2]),tr("About","О программе"));
    gtk_entry_set_placeholder_text(GTK_ENTRY(range),tr("Use all profile ranges","Все диапазоны профиля"));
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(scan),tr("Scan","Сканировать"));gtk_tool_button_set_label(GTK_TOOL_BUTTON(stop),tr("Stop","Стоп"));gtk_tool_button_set_label(GTK_TOOL_BUTTON(refresh),tr("Refresh","Обновить"));gtk_tool_button_set_label(GTK_TOOL_BUTTON(export_button),tr("Export","Экспорт"));
    const char *en[]={"Status","IP Address","Host Name","MAC Address","Vendor","Device / OS","Latency","Services","First Seen","Last Seen"};
    const char *ru[]={"Статус","IP-адрес","Имя узла","MAC-адрес","Производитель","Устройство / ОС","Задержка","Сервисы","Первый ответ","Последний ответ"};
    for(unsigned c=0;c<10;++c)gtk_tree_view_column_set_title(gtk_tree_view_get_column(GTK_TREE_VIEW(table),(gint)c),app.russian ? ru[c] : en[c]);
    gtk_entry_set_placeholder_text(GTK_ENTRY(filter),tr("Filter hosts…","Фильтр устройств…"));update();
}
static void select_nic(GtkComboBox *sender,gpointer data){(void)data;if(!updating && desktop_select(&app,(size_t)gtk_combo_box_get_active(sender))){populate();update();}}
static void select_language(GtkComboBox *sender,gpointer data){(void)data;desktop_language(&app,(unsigned)gtk_combo_box_get_active(sender),system_language);localize();}
static void filter_changed(GtkEditable *sender,gpointer data){(void)sender;(void)data;update();}
static void scan_clicked(GtkWidget *sender,gpointer data){
    (void)sender;(void)data;const char *text=gtk_entry_get_text(GTK_ENTRY(range));ipv4_range_t parsed;
    if(range_parse(text,strlen(text),&parsed) && (uint64_t)parsed.last-parsed.first+1>4096){GtkWidget *dialog=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_OK_CANCEL,"%s",tr("Scan more than 4096 addresses?","Сканировать более 4096 адресов?"));int response=gtk_dialog_run(GTK_DIALOG(dialog));gtk_widget_destroy(dialog);if(response!=GTK_RESPONSE_OK)return;}
    if(!desktop_start(&app,text))message(app.error);update();
}
static void stop_clicked(GtkWidget *sender,gpointer data){(void)sender;(void)data;posix_stop(&app.backend);}
static void refresh_clicked(GtkWidget *sender,gpointer data){(void)sender;(void)data;desktop_refresh(&app);populate();update();}
static char *choose_file(bool save,const char *name){GtkWidget *dialog=gtk_file_chooser_dialog_new(tr(save ? "Save" : "Open profile",save ? "Сохранить" : "Открыть профиль"),GTK_WINDOW(window),save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,tr("Cancel","Отмена"),GTK_RESPONSE_CANCEL,tr(save ? "Save" : "Open",save ? "Сохранить" : "Открыть"),GTK_RESPONSE_ACCEPT,NULL);if(save){gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),TRUE);gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),name);}char *path=NULL;if(gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT)path=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));gtk_widget_destroy(dialog);return path;}
static void export_clicked(GtkWidget *sender,gpointer data){(void)sender;(void)data;char *path=choose_file(true,"LookLAN.csv");if(path){const char *ext=strrchr(path,'.');report_format_t format=ext && !strcmp(ext,".html") ? REPORT_HTML : ext && !strcmp(ext,".txt") ? REPORT_TXT : REPORT_CSV;if(!desktop_export(&app,path,format))message(tr("Export failed","Ошибка экспорта"));g_free(path);}}
static void open_profile(GtkWidget *sender,gpointer data){(void)sender;(void)data;if(app.scanning)return;char *path=choose_file(false,NULL);if(path){if(!desktop_profile_open(&app,path))message(app.error);else{populate();update();}g_free(path);}}
static void save_profile(GtkWidget *sender,gpointer data){(void)sender;(void)data;char *path=choose_file(true,"LookLAN.lanprofile");if(path){if(!desktop_profile_save(&app,path))message(tr("Profile save failed","Не удалось сохранить профиль"));g_free(path);}}
static void about(GtkWidget *sender,gpointer data){(void)sender;(void)data;gtk_show_about_dialog(GTK_WINDOW(window),"program-name","Look@LAN-ng","version",APP_VERSION,"copyright","© nr084, " APP_BUILD_YEAR,"website","https://github.com/nr084/Look-LAN-ng",NULL);}
static void selection_changed(GtkTreeSelection *selection,gpointer data){(void)data;GtkTreeModel *model;GtkTreeIter iter;GString *description=g_string_new(NULL);if(gtk_tree_selection_get_selected(selection,&model,&iter)){for(gint c=0;c<10;++c){char *value=NULL;gtk_tree_model_get(model,&iter,c,&value,-1);g_string_append_printf(description,"%s: %s\n",gtk_tree_view_column_get_title(gtk_tree_view_get_column(GTK_TREE_VIEW(table),c)),value);g_free(value);}}gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(detail)),description->str,-1);g_string_free(description,TRUE);}
static gboolean close_window(GtkWidget *sender,GdkEvent *event,gpointer data){(void)sender;(void)event;(void)data;finishing=true;posix_stop(&app.backend);gtk_widget_set_sensitive(window,FALSE);return TRUE;}
static gboolean tick(gpointer data){(void)data;if(finishing){if(desktop_close(&app)){gtk_main_quit();return G_SOURCE_REMOVE;}return G_SOURCE_CONTINUE;}if(desktop_poll(&app)){update();if(app.error[0] && !app.scanning){message(app.error);app.error[0]=0;}}return G_SOURCE_CONTINUE;}
static gboolean keypress(GtkWidget *sender,GdkEventKey *key,gpointer data){(void)sender;(void)data;if(key->keyval==GDK_KEY_F5){scan_clicked(NULL,NULL);return TRUE;}if(key->keyval==GDK_KEY_Escape){stop_clicked(NULL,NULL);return TRUE;}return FALSE;}
static GtkWidget *button(GtkToolbar *bar,const char *icon,GCallback callback){GtkToolItem *item=gtk_tool_button_new(gtk_image_new_from_icon_name(icon,GTK_ICON_SIZE_LARGE_TOOLBAR),"");gtk_toolbar_insert(bar,item,-1);g_signal_connect(item,"clicked",callback,NULL);return GTK_WIDGET(item);}
int main(int argc,char **argv){
    system_language=getenv("LC_ALL");if(!system_language || !*system_language)system_language=getenv("LC_MESSAGES");if(!system_language || !*system_language)system_language=getenv("LANG");
    if(argc>1 && !strcmp(argv[1],"--smoke-test"))return desktop_smoke(system_language);
    gtk_init(&argc,&argv);if(!desktop_init(&app,system_language))return 1;
    window=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(window),"Look@LAN-ng " APP_VERSION);gtk_window_set_default_size(GTK_WINDOW(window),1120,720);
    char *program_directory=g_path_get_dirname(argv[0]);char *icon_path=g_build_filename(program_directory,"LookLAN.png",NULL);gtk_window_set_icon_from_file(GTK_WINDOW(window),icon_path,NULL);g_free(icon_path);g_free(program_directory);
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);gtk_container_set_border_width(GTK_CONTAINER(box),12);gtk_container_add(GTK_CONTAINER(window),box);
    GtkWidget *menu=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(box),menu,FALSE,FALSE,0);
    const char *names[]={tr("Open profile","Открыть профиль"),tr("Save profile","Сохранить профиль"),tr("About","О программе")};GCallback callbacks[]={G_CALLBACK(open_profile),G_CALLBACK(save_profile),G_CALLBACK(about)};
    for(unsigned i=0;i<3;++i){GtkWidget *item=gtk_button_new_with_label(names[i]);menu_buttons[i]=item;gtk_box_pack_start(GTK_BOX(menu),item,FALSE,FALSE,0);g_signal_connect(item,"clicked",callbacks[i],NULL);}
    language=gtk_combo_box_text_new();gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(language),"System / Система");gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(language),"English");gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(language),"Русский");gtk_combo_box_set_active(GTK_COMBO_BOX(language),(gint)app.language);gtk_box_pack_end(GTK_BOX(menu),language,FALSE,FALSE,0);
    GtkWidget *toolbar=gtk_toolbar_new();gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),GTK_TOOLBAR_BOTH_HORIZ);gtk_box_pack_start(GTK_BOX(box),toolbar,FALSE,FALSE,0);scan=button(GTK_TOOLBAR(toolbar),"network-workgroup-symbolic",G_CALLBACK(scan_clicked));stop=button(GTK_TOOLBAR(toolbar),"media-playback-stop-symbolic",G_CALLBACK(stop_clicked));refresh=button(GTK_TOOLBAR(toolbar),"view-refresh-symbolic",G_CALLBACK(refresh_clicked));export_button=button(GTK_TOOLBAR(toolbar),"document-save-symbolic",G_CALLBACK(export_clicked));
    GtkWidget *controls=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_pack_start(GTK_BOX(box),controls,FALSE,FALSE,0);nic=gtk_combo_box_text_new();range=gtk_entry_new();filter=gtk_entry_new();gtk_box_pack_start(GTK_BOX(controls),nic,FALSE,FALSE,0);gtk_box_pack_start(GTK_BOX(controls),range,TRUE,TRUE,0);gtk_box_pack_start(GTK_BOX(controls),filter,TRUE,TRUE,0);
    rows=gtk_list_store_new(10,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING);table=gtk_tree_view_new_with_model(GTK_TREE_MODEL(rows));g_object_unref(rows);
    for(gint c=0;c<10;++c){GtkCellRenderer *cell=gtk_cell_renderer_text_new();GtkTreeViewColumn *column=gtk_tree_view_column_new_with_attributes("",cell,"text",c,NULL);gtk_tree_view_column_set_resizable(column,TRUE);gtk_tree_view_column_set_sort_column_id(column,c);gtk_tree_view_append_column(GTK_TREE_VIEW(table),column);}
    GtkWidget *paned=gtk_paned_new(GTK_ORIENTATION_VERTICAL);gtk_box_pack_start(GTK_BOX(box),paned,TRUE,TRUE,0);GtkWidget *scroll=gtk_scrolled_window_new(NULL,NULL);gtk_container_add(GTK_CONTAINER(scroll),table);gtk_paned_pack1(GTK_PANED(paned),scroll,TRUE,FALSE);
    GtkWidget *details_scroll=gtk_scrolled_window_new(NULL,NULL);detail=gtk_text_view_new();gtk_text_view_set_editable(GTK_TEXT_VIEW(detail),FALSE);gtk_container_add(GTK_CONTAINER(details_scroll),detail);gtk_paned_pack2(GTK_PANED(paned),details_scroll,FALSE,FALSE);gtk_widget_set_size_request(details_scroll,-1,140);
    status=gtk_label_new("");gtk_label_set_xalign(GTK_LABEL(status),0);gtk_box_pack_start(GTK_BOX(box),status,FALSE,FALSE,0);
    populate();localize();g_signal_connect(nic,"changed",G_CALLBACK(select_nic),NULL);g_signal_connect(language,"changed",G_CALLBACK(select_language),NULL);g_signal_connect(filter,"changed",G_CALLBACK(filter_changed),NULL);g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(table)),"changed",G_CALLBACK(selection_changed),NULL);g_signal_connect(window,"delete-event",G_CALLBACK(close_window),NULL);g_signal_connect(window,"key-press-event",G_CALLBACK(keypress),NULL);g_timeout_add(50,tick,NULL);gtk_widget_show_all(window);gtk_main();return 0;
}
