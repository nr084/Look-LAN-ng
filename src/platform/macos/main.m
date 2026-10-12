#import <Cocoa/Cocoa.h>
#include "desktop_app.h"
#include "build_info.h"
#include <string.h>
#include <stdlib.h>
static NSString *text(const char *s){return [NSString stringWithUTF8String:s] ?: @"";}
@interface LANDelegate : NSObject <NSApplicationDelegate,NSTableViewDataSource,NSTableViewDelegate,NSToolbarDelegate,NSWindowDelegate>
@property NSWindow *window;
@property NSTableView *table;
@property NSPopUpButton *interfaces;
@property NSTextField *range,*filter,*status;
@property NSTextView *detail;
@property NSMutableArray<NSNumber *> *visible;
@property NSTimer *timer;
@property NSString *systemLanguage;
@property BOOL terminating;
@end
static desktop_app_t app;
@implementation LANDelegate
- (NSString *)tr:(const char *)en ru:(const char *)ru{return text(desktop_tr(&app,en,ru));}
- (void)menu {
    NSMenu *bar=[NSMenu new];NSMenuItem *application=[NSMenuItem new];[bar addItem:application];
    NSMenu *applicationMenu=[[NSMenu alloc]initWithTitle:@"Look@LAN-ng"];application.submenu=applicationMenu;
    [applicationMenu addItemWithTitle:[self tr:"About Look@LAN-ng" ru:"О Look@LAN-ng"] action:@selector(about:) keyEquivalent:@""];
    [applicationMenu addItem:NSMenuItem.separatorItem];[applicationMenu addItemWithTitle:[self tr:"Quit" ru:"Выход"] action:@selector(terminate:) keyEquivalent:@"q"];
    NSMenuItem *file=[NSMenuItem new];[bar addItem:file];NSMenu *files=[[NSMenu alloc]initWithTitle:[self tr:"File" ru:"Файл"]];file.submenu=files;
    NSMenuItem *open=[files addItemWithTitle:[self tr:"Open profile…" ru:"Открыть профиль…"] action:@selector(openProfile:) keyEquivalent:@"o"];open.target=self;
    NSMenuItem *save=[files addItemWithTitle:[self tr:"Save profile…" ru:"Сохранить профиль…"] action:@selector(saveProfile:) keyEquivalent:@"s"];save.target=self;
    NSMenuItem *language=[NSMenuItem new];[bar addItem:language];NSMenu *languages=[[NSMenu alloc]initWithTitle:[self tr:"Language" ru:"Язык"]];language.submenu=languages;
    NSArray *names=@[[self tr:"System language" ru:"Язык системы"],@"English",@"Русский"];
    for(NSUInteger i=0;i<3;++i){NSMenuItem *item=[languages addItemWithTitle:names[i] action:@selector(language:) keyEquivalent:@""];item.tag=(NSInteger)i;item.target=self;item.state=app.language==i ? NSControlStateValueOn : NSControlStateValueOff;}
    NSApp.mainMenu=bar;
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;self.systemLanguage=NSLocale.preferredLanguages.firstObject ?: @"en";
    if(!desktop_init(&app,self.systemLanguage.UTF8String))exit(EXIT_FAILURE);
    self.window=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,1120,720) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"Look@LAN-ng " @APP_VERSION;self.window.minSize=NSMakeSize(760,500);self.window.delegate=self;[self.window center];
    NSView *content=self.window.contentView;
    self.interfaces=[[NSPopUpButton alloc]initWithFrame:NSMakeRect(14,674,310,28) pullsDown:NO];self.interfaces.target=self;self.interfaces.action=@selector(selectInterface:);self.interfaces.autoresizingMask=NSViewMinYMargin;
    self.range=[[NSTextField alloc]initWithFrame:NSMakeRect(334,674,310,28)];self.range.autoresizingMask=NSViewMinYMargin;
    self.filter=[[NSTextField alloc]initWithFrame:NSMakeRect(654,674,452,28)];self.filter.target=self;self.filter.action=@selector(filter:);self.filter.autoresizingMask=NSViewMinYMargin|NSViewWidthSizable;
    [content addSubview:self.interfaces];[content addSubview:self.range];[content addSubview:self.filter];
    NSScrollView *scroll=[[NSScrollView alloc]initWithFrame:NSMakeRect(14,202,1092,458)];scroll.hasVerticalScroller=YES;scroll.hasHorizontalScroller=YES;scroll.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
    self.table=[[NSTableView alloc]initWithFrame:scroll.bounds];self.table.dataSource=self;self.table.delegate=self;self.table.usesAlternatingRowBackgroundColors=YES;self.table.rowSizeStyle=NSTableViewRowSizeStyleMedium;
    const CGFloat widths[]={105,130,175,165,180,130,90,150,170,170};
    for(unsigned c=0;c<10;++c){NSTableColumn *column=[[NSTableColumn alloc]initWithIdentifier:[NSString stringWithFormat:@"%u",c]];column.width=widths[c];[self.table addTableColumn:column];}
    scroll.documentView=self.table;[content addSubview:scroll];
    NSScrollView *details=[[NSScrollView alloc]initWithFrame:NSMakeRect(14,40,1092,150)];details.hasVerticalScroller=YES;details.autoresizingMask=NSViewWidthSizable;
    self.detail=[[NSTextView alloc]initWithFrame:details.bounds];self.detail.editable=NO;self.detail.font=[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];details.documentView=self.detail;[content addSubview:details];
    self.status=[NSTextField labelWithString:@""];self.status.frame=NSMakeRect(14,10,1092,22);self.status.autoresizingMask=NSViewWidthSizable;[content addSubview:self.status];
    self.visible=[NSMutableArray new];[self populateInterfaces];[self localize];
    self.timer=[NSTimer scheduledTimerWithTimeInterval:.05 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
    [self.window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
}
- (void)localize {
    [self menu];NSArray *titles=app.russian ? @[@"Статус",@"IP-адрес",@"Имя узла",@"MAC-адрес",@"Производитель",@"Устройство / ОС",@"Задержка",@"Сервисы",@"Первый ответ",@"Последний ответ"] : @[@"Status",@"IP Address",@"Host Name",@"MAC Address",@"Vendor",@"Device / OS",@"Latency",@"Services",@"First Seen",@"Last Seen"];
    for(NSUInteger c=0;c<10;++c)self.table.tableColumns[c].title=titles[c];self.filter.placeholderString=[self tr:"Filter hosts…" ru:"Фильтр устройств…"];
    NSToolbar *toolbar=[[NSToolbar alloc]initWithIdentifier:@"LookLAN-toolbar"];toolbar.delegate=self;toolbar.displayMode=NSToolbarDisplayModeIconAndLabel;toolbar.allowsUserCustomization=NO;self.window.toolbar=toolbar;self.window.toolbarStyle=NSWindowToolbarStyleUnifiedCompact;[self refreshRows];
}
- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar{(void)toolbar;return @[@"scan",@"stop",@"refresh",NSToolbarFlexibleSpaceItemIdentifier,@"export"];}
- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar{return [self toolbarAllowedItemIdentifiers:toolbar];}
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)identifier willBeInsertedIntoToolbar:(BOOL)flag {
    (void)toolbar;(void)flag;NSArray *ids=@[@"scan",@"stop",@"refresh",@"export"],*symbols=@[@"network",@"stop.fill",@"arrow.clockwise",@"square.and.arrow.down"];
    NSUInteger i=[ids indexOfObject:identifier];if(i==NSNotFound)return nil;NSToolbarItem *item=[[NSToolbarItem alloc]initWithItemIdentifier:identifier];
    NSArray *labels=app.russian ? @[@"Сканировать",@"Стоп",@"Обновить",@"Экспорт"] : @[@"Scan",@"Stop",@"Refresh",@"Export"];
    item.label=labels[i];item.toolTip=labels[i];item.image=[NSImage imageWithSystemSymbolName:symbols[i] accessibilityDescription:labels[i]];item.target=self;item.action=NSSelectorFromString(@[@"scan:",@"stop:",@"refresh:",@"export:"][i]);return item;
}
- (BOOL)validateToolbarItem:(NSToolbarItem *)item{if([item.itemIdentifier isEqual:@"stop"])return app.scanning;if([item.itemIdentifier isEqual:@"scan"] && app.selected>=app.interface_count)return NO;return !app.scanning && !app.closing;}
- (void)populateInterfaces {
    [self.interfaces removeAllItems];for(size_t i=0;i<app.interface_count;++i){char ip[32];ipv4_format(app.interfaces[i].ip,ip,sizeof(ip));[self.interfaces addItemWithTitle:[NSString stringWithFormat:@"%s  %s/%u",app.interfaces[i].name,ip,app.interfaces[i].prefix]];}
    if(app.selected<app.interface_count){[self.interfaces selectItemAtIndex:(NSInteger)app.selected];char ranges[4096];profile_ranges_text(&app.profile,ranges,sizeof(ranges));self.range.stringValue=app.profile.range_count==1 ? text(ranges) : @"";self.range.placeholderString=[self tr:"Use all profile ranges" ru:"Все диапазоны профиля"];}
}
- (void)selectInterface:(id)sender{(void)sender;if(desktop_select(&app,(size_t)self.interfaces.indexOfSelectedItem)){[self populateInterfaces];[self refreshRows];}}
- (void)language:(NSMenuItem *)sender{desktop_language(&app,(unsigned)sender.tag,self.systemLanguage.UTF8String);[self localize];}
- (void)filter:(id)sender{(void)sender;[self refreshRows];}
- (void)message:(NSString *)message{NSAlert *alert=[NSAlert new];alert.messageText=@"Look@LAN-ng";alert.informativeText=message;[alert runModal];}
- (void)scan:(id)sender{(void)sender;ipv4_range_t range;if(range_parse(self.range.stringValue.UTF8String,strlen(self.range.stringValue.UTF8String),&range) && (uint64_t)range.last-range.first+1>4096){NSAlert *alert=[NSAlert new];alert.messageText=[self tr:"Scan more than 4096 addresses?" ru:"Сканировать более 4096 адресов?"];[alert addButtonWithTitle:[self tr:"Scan" ru:"Сканировать"]];[alert addButtonWithTitle:[self tr:"Cancel" ru:"Отмена"]];if([alert runModal]!=NSAlertFirstButtonReturn)return;}if(!desktop_start(&app,self.range.stringValue.UTF8String))[self message:text(app.error)];}
- (void)stop:(id)sender{(void)sender;posix_stop(&app.backend);}
- (void)refresh:(id)sender{(void)sender;desktop_refresh(&app);[self populateInterfaces];[self refreshRows];}
- (void)refreshRows {
    [self.visible removeAllObjects];for(size_t i=0;i<app.host_count;++i){BOOL match=!self.filter.stringValue.length;for(unsigned c=0;!match && c<10;++c){char value[512];desktop_cell(&app,i,c,value,sizeof(value));match=[text(value) localizedCaseInsensitiveContainsString:self.filter.stringValue];}if(match)[self.visible addObject:@(i)];}[self.table reloadData];
    self.status.stringValue=[NSString stringWithFormat:@"%@  |  %u / %u  |  %@: %zu",[self tr:(app.scanning ? "Scanning" : "Ready") ru:(app.scanning ? "Сканирование" : "Готово")],app.completed,app.total,[self tr:"Hosts" ru:"Узлы"],app.host_count];
    self.interfaces.enabled=!app.scanning;self.range.enabled=!app.scanning;[self.window.toolbar validateVisibleItems];
}
- (NSInteger)numberOfRowsInTableView:(NSTableView *)table{(void)table;return (NSInteger)self.visible.count;}
- (NSView *)tableView:(NSTableView *)table viewForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
    (void)table;NSTextField *label=[NSTextField labelWithString:@""];char value[512];desktop_cell(&app,self.visible[(NSUInteger)row].unsignedLongLongValue,(unsigned)column.identifier.intValue,value,sizeof(value));label.stringValue=text(value);label.lineBreakMode=NSLineBreakByTruncatingTail;return label;
}
- (void)tableViewSelectionDidChange:(NSNotification *)notification{(void)notification;NSInteger row=self.table.selectedRow;if(row<0 || row>=(NSInteger)self.visible.count){self.detail.string=@"";return;}NSMutableString *description=[NSMutableString new];for(unsigned c=0;c<10;++c){char value[512];desktop_cell(&app,self.visible[(NSUInteger)row].unsignedLongLongValue,c,value,sizeof(value));[description appendFormat:@"%@: %@\n",self.table.tableColumns[c].title,text(value)];}self.detail.string=description;}
- (void)tick:(id)sender{(void)sender;if(self.terminating){if(desktop_close(&app)){[self.timer invalidate];[NSApp replyToApplicationShouldTerminate:YES];}return;}if(desktop_poll(&app)){[self refreshRows];if(app.error[0] && !app.scanning){[self message:text(app.error)];app.error[0]=0;}}}
- (void)export:(id)sender{(void)sender;NSSavePanel *panel=NSSavePanel.savePanel;panel.nameFieldStringValue=@"LookLAN.csv";if([panel runModal]==NSModalResponseOK){NSString *ext=panel.URL.pathExtension.lowercaseString;report_format_t format=[ext isEqual:@"html"] ? REPORT_HTML : [ext isEqual:@"txt"] ? REPORT_TXT : REPORT_CSV;if(!desktop_export(&app,panel.URL.path.fileSystemRepresentation,format))[self message:[self tr:"Export failed" ru:"Ошибка экспорта"]];}}
- (void)openProfile:(id)sender{(void)sender;if(app.scanning)return;NSOpenPanel *panel=NSOpenPanel.openPanel;panel.allowedFileTypes=@[@"lanprofile"];if([panel runModal]==NSModalResponseOK){if(!desktop_profile_open(&app,panel.URL.path.fileSystemRepresentation))[self message:text(app.error)];else{[self populateInterfaces];[self refreshRows];}}}
- (void)saveProfile:(id)sender{(void)sender;NSSavePanel *panel=NSSavePanel.savePanel;panel.nameFieldStringValue=@"LookLAN.lanprofile";if([panel runModal]==NSModalResponseOK && !desktop_profile_save(&app,panel.URL.path.fileSystemRepresentation))[self message:[self tr:"Profile save failed" ru:"Не удалось сохранить профиль"]];}
- (void)about:(id)sender{(void)sender;[self message:@"Look@LAN-ng " @APP_VERSION @"\n© nr084, " @APP_BUILD_YEAR];}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender{(void)sender;if(!self.terminating && desktop_close(&app)){[self.timer invalidate];return NSTerminateNow;}self.terminating=YES;return NSTerminateLater;}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender{(void)sender;return YES;}
- (BOOL)windowShouldClose:(NSWindow *)sender{(void)sender;[NSApp terminate:nil];return NO;}
@end
int main(int argc,char **argv){@autoreleasepool {
    if(argc>1 && !strcmp(argv[1],"--smoke-test"))return desktop_smoke(NSLocale.preferredLanguages.firstObject.UTF8String);
    [NSApplication sharedApplication];NSApp.activationPolicy=NSApplicationActivationPolicyRegular;__attribute__((objc_precise_lifetime)) LANDelegate *delegate=[LANDelegate new];NSApp.delegate=delegate;[NSApp run];return 0;
}}
