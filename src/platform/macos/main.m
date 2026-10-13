#import <Cocoa/Cocoa.h>
#include "desktop_app.h"
#include "build_info.h"
#include <string.h>
#include <stdlib.h>
#include "statistics.h"
static NSString *text(const char *s){return [NSString stringWithUTF8String:s] ?: @"";}
@interface LANDelegate : NSObject <NSApplicationDelegate,NSTableViewDataSource,NSTableViewDelegate,NSToolbarDelegate,NSWindowDelegate,NSTextFieldDelegate,NSOutlineViewDataSource,NSOutlineViewDelegate,NSSplitViewDelegate,NSTabViewDelegate>
@property NSWindow *window;
@property NSTableView *table,*logTable;
@property uint64_t logSequence;
@property NSPopUpButton *interfaces;
@property NSTextField *range,*filter,*status;
@property NSTextField *networkInfo,*emptyState;
@property NSScrollView *hostScroll,*sidebarScroll;
@property NSView *detailPane;
@property NSSegmentedControl *detailSelector;
@property NSTabView *detailTabs;
@property NSSplitView *networkSplit,*hostSplit;
@property NSMutableArray<NSTextView *> *detailViews;
@property NSOutlineView *sidebar;
@property NSArray *sidebarNodes;
@property BOOL updatingSidebar,remapping;
@property NSMutableArray<NSNumber *> *visible;
@property NSTimer *timer;
@property NSString *systemLanguage;
@property BOOL terminating;
@property BOOL rowsDirty;
@property NSTimeInterval nextRowsRefresh;
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
    NSMenuItem *edit=[files addItemWithTitle:[self tr:"Profile…" ru:"Профиль…"] action:@selector(editProfile:) keyEquivalent:@","];edit.target=self;
    NSMenuItem *export=[files addItemWithTitle:[self tr:"Export…" ru:"Экспорт…"] action:@selector(export:) keyEquivalent:@"e"];export.target=self;
    NSMenuItem *scanMenu=[NSMenuItem new];[bar addItem:scanMenu];NSMenu *scans=[[NSMenu alloc]initWithTitle:[self tr:"Scan" ru:"Сканирование"]];scanMenu.submenu=scans;
    NSArray *scanNames=@[[self tr:"Scan" ru:"Сканировать"],[self tr:"Stop" ru:"Стоп"],[self tr:"Refresh interfaces" ru:"Обновить интерфейсы"],[self tr:"Single host…" ru:"Один узел…"]];
    NSArray *scanActions=@[@"scan:",@"stop:",@"refresh:",@"single:"],*scanKeys=@[@"r",@".",@"",@""];
    for(NSUInteger i=0;i<scanNames.count;++i){NSMenuItem *item=[scans addItemWithTitle:scanNames[i] action:NSSelectorFromString(scanActions[i]) keyEquivalent:scanKeys[i]];item.target=self;}
    NSMenuItem *tools=[NSMenuItem new];[bar addItem:tools];NSMenu *actions=[[NSMenu alloc]initWithTitle:[self tr:"Tools" ru:"Инструменты"]];tools.submenu=actions;
    NSArray *actionNames=@[[self tr:"Single host…" ru:"Один узел…"],[self tr:"Monitoring" ru:"Мониторинг"],[self tr:"Statistics" ru:"Статистика"],[self tr:"History" ru:"История"],[self tr:"Notes…" ru:"Заметки…"]];NSArray *selectors=@[@"single:",@"monitoring:",@"statistics:",@"history:",@"notes:"];
    for(NSUInteger i=0;i<actionNames.count;++i){NSMenuItem *item=[actions addItemWithTitle:actionNames[i] action:NSSelectorFromString(selectors[i]) keyEquivalent:@""];item.target=self;}
    NSMenuItem *language=[NSMenuItem new];[bar addItem:language];NSMenu *languages=[[NSMenu alloc]initWithTitle:[self tr:"Language" ru:"Язык"]];language.submenu=languages;
    NSArray *names=@[[self tr:"System language" ru:"Язык системы"],@"English",@"Русский"];
    for(NSUInteger i=0;i<3;++i){NSMenuItem *item=[languages addItemWithTitle:names[i] action:@selector(language:) keyEquivalent:@""];item.tag=(NSInteger)i;item.target=self;item.state=app.language==i ? NSControlStateValueOn : NSControlStateValueOff;}
    NSApp.mainMenu=bar;
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;self.systemLanguage=NSLocale.preferredLanguages.firstObject ?: @"en";
    if(!desktop_init(&app,self.systemLanguage.UTF8String))exit(EXIT_FAILURE);
    self.window=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,1120,720) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"Look@LAN-ng " @APP_VERSION;self.window.minSize=NSMakeSize(900,560);self.window.delegate=self;[self.window center];[self.window setFrameAutosaveName:@"LookLAN-main"];
    NSView *content=self.window.contentView;
    self.interfaces=[[NSPopUpButton alloc]initWithFrame:NSMakeRect(14,674,310,28) pullsDown:NO];self.interfaces.target=self;self.interfaces.action=@selector(selectInterface:);self.interfaces.autoresizingMask=NSViewMinYMargin;
    self.range=[[NSTextField alloc]initWithFrame:NSMakeRect(334,674,310,28)];self.range.autoresizingMask=NSViewMinYMargin;
    self.filter=[[NSTextField alloc]initWithFrame:NSMakeRect(654,674,452,28)];self.filter.target=self;self.filter.action=@selector(filter:);self.filter.autoresizingMask=NSViewMinYMargin|NSViewWidthSizable;
    self.filter.delegate=self;[self layoutTopControls];
    [content addSubview:self.interfaces];[content addSubview:self.range];[content addSubview:self.filter];
    self.networkInfo=[NSTextField labelWithString:@""];self.networkInfo.frame=NSMakeRect(14,644,1092,22);self.networkInfo.autoresizingMask=NSViewMinYMargin|NSViewWidthSizable;self.networkInfo.lineBreakMode=NSLineBreakByTruncatingTail;[content addSubview:self.networkInfo];
    NSScrollView *scroll=[[NSScrollView alloc]initWithFrame:NSMakeRect(240,202,866,432)];scroll.hasVerticalScroller=YES;scroll.hasHorizontalScroller=YES;scroll.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
    self.table=[[NSTableView alloc]initWithFrame:scroll.bounds];self.table.dataSource=self;self.table.delegate=self;self.table.usesAlternatingRowBackgroundColors=NO;self.table.style=NSTableViewStyleFullWidth;self.table.rowHeight=26;self.table.autosaveName=@"LookLAN-columns-v2";self.table.autosaveTableColumns=YES;self.table.target=self;self.table.doubleAction=@selector(showGeneral:);
    const CGFloat widths[]={116,122,154,148,156,124,82,128,150,150};
    for(unsigned c=0;c<10;++c){NSTableColumn *column=[[NSTableColumn alloc]initWithIdentifier:[NSString stringWithFormat:@"%u",c]];column.width=widths[c];column.sortDescriptorPrototype=[NSSortDescriptor sortDescriptorWithKey:column.identifier ascending:YES];[self.table addTableColumn:column];}
    scroll.documentView=self.table;scroll.borderType=NSNoBorder;self.hostScroll=scroll;
    self.emptyState=[NSTextField wrappingLabelWithString:@""];self.emptyState.alignment=NSTextAlignmentCenter;self.emptyState.textColor=NSColor.secondaryLabelColor;self.emptyState.font=[NSFont systemFontOfSize:14];[scroll addSubview:self.emptyState];
    NSScrollView *sideScroll=[[NSScrollView alloc]initWithFrame:NSMakeRect(14,40,216,594)];sideScroll.hasVerticalScroller=YES;sideScroll.hasHorizontalScroller=YES;sideScroll.autoresizingMask=NSViewHeightSizable;
    self.sidebar=[[NSOutlineView alloc]initWithFrame:sideScroll.bounds];NSTableColumn *sideColumn=[[NSTableColumn alloc]initWithIdentifier:@"network"];sideColumn.width=210;[self.sidebar addTableColumn:sideColumn];self.sidebar.outlineTableColumn=sideColumn;self.sidebar.headerView=nil;self.sidebar.dataSource=self;self.sidebar.delegate=self;sideScroll.documentView=self.sidebar;sideScroll.borderType=NSNoBorder;self.sidebar.rowSizeStyle=NSTableViewRowSizeStyleSmall;self.sidebar.style=NSTableViewStyleSourceList;self.sidebarScroll=sideScroll;
    self.detailTabs=[[NSTabView alloc]initWithFrame:NSMakeRect(240,40,866,150)];self.detailTabs.autoresizingMask=NSViewWidthSizable;self.detailViews=[NSMutableArray new];self.detailTabs.tabViewType=NSNoTabsNoBorder;self.detailTabs.delegate=self;
    self.detailPane=[[NSView alloc]initWithFrame:NSMakeRect(0,0,866,190)];self.detailSelector=[[NSSegmentedControl alloc]initWithFrame:NSMakeRect(12,158,600,26)];self.detailSelector.segmentCount=6;self.detailSelector.trackingMode=NSSegmentSwitchTrackingSelectOne;self.detailSelector.segmentStyle=NSSegmentStyleTexturedRounded;self.detailSelector.target=self;self.detailSelector.action=@selector(selectDetail:);self.detailSelector.selectedSegment=0;[self.detailPane addSubview:self.detailSelector];[self.detailPane addSubview:self.detailTabs];
    for(unsigned i=0;i<6;++i){NSTabViewItem *item=[[NSTabViewItem alloc]initWithIdentifier:@(i)];NSScrollView *details=[[NSScrollView alloc]initWithFrame:self.detailTabs.contentRect];details.hasVerticalScroller=YES;details.hasHorizontalScroller=i==5;details.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;if(i==5){self.logTable=[[NSTableView alloc]initWithFrame:details.bounds];self.logTable.dataSource=self;self.logTable.delegate=self;const CGFloat widths[]={160,200,140,380};for(unsigned col=0;col<4;++col){NSTableColumn *column=[[NSTableColumn alloc]initWithIdentifier:[NSString stringWithFormat:@"%u",col]];column.width=widths[col];[self.logTable addTableColumn:column];}details.documentView=self.logTable;item.view=details;[self.detailTabs addTabViewItem:item];continue;}NSTextView *view=[[NSTextView alloc]initWithFrame:details.bounds];view.editable=NO;view.textContainerInset=NSMakeSize(12,10);view.font=[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];view.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;view.verticallyResizable=YES;view.horizontallyResizable=NO;view.textContainer.widthTracksTextView=YES;details.documentView=view;item.view=details;[self.detailViews addObject:view];[self.detailTabs addTabViewItem:item];}
    self.networkSplit=[[NSSplitView alloc]initWithFrame:NSMakeRect(14,40,1092,594)];self.networkSplit.vertical=YES;self.networkSplit.dividerStyle=NSSplitViewDividerStyleThin;self.networkSplit.delegate=self;self.networkSplit.autoresizingMask=NSViewNotSizable;
    self.hostSplit=[[NSSplitView alloc]initWithFrame:NSMakeRect(0,0,866,594)];self.hostSplit.vertical=NO;self.hostSplit.dividerStyle=NSSplitViewDividerStyleThin;self.hostSplit.delegate=self;
    sideScroll.frame=NSMakeRect(0,0,216,594);scroll.frame=NSMakeRect(0,0,866,424);self.detailTabs.frame=NSMakeRect(0,0,866,170);self.detailTabs.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
    [self.hostSplit addSubview:scroll];[self.hostSplit addSubview:self.detailPane];[self.hostSplit adjustSubviews];[self.hostSplit setPosition:424 ofDividerAtIndex:0];
    [self.networkSplit addSubview:sideScroll];[self.networkSplit addSubview:self.hostSplit];[self.networkSplit adjustSubviews];[self.networkSplit setPosition:216 ofDividerAtIndex:0];
    self.networkSplit.autosaveName=@"LookLAN-network-split-v2";self.hostSplit.autosaveName=@"LookLAN-details-split-v2";[content addSubview:self.networkSplit];
    self.status=[NSTextField labelWithString:@""];self.status.frame=NSMakeRect(14,10,1092,22);self.status.autoresizingMask=NSViewWidthSizable;self.status.font=[NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];self.status.textColor=NSColor.secondaryLabelColor;self.networkInfo.font=[NSFont systemFontOfSize:11];self.networkInfo.textColor=NSColor.secondaryLabelColor;[content addSubview:self.status];
    self.visible=[NSMutableArray new];[self layoutTopControls];[self populateInterfaces];[self localize];
    NSMenu *context=[NSMenu new];NSArray *contextNames=@[[self tr:"Copy IP" ru:"Копировать IP"],[self tr:"Copy MAC" ru:"Копировать MAC"],[self tr:"Properties" ru:"Свойства"],[self tr:"Acknowledge" ru:"Подтвердить"],[self tr:"Rescan host" ru:"Повторно проверить узел"],@"Web",@"HTTPS",@"SSH",@"SMB",@"RDP",[self tr:"Copy hostname" ru:"Копировать имя узла"],@"Ping",@"Traceroute",[self tr:"Scan ports" ru:"Проверить порты"]];for(NSUInteger i=0;i<contextNames.count;++i){NSMenuItem *item=[context addItemWithTitle:contextNames[i] action:@selector(contextAction:) keyEquivalent:@""];item.target=self;item.tag=(NSInteger)i;}self.table.menu=context;
    self.timer=[NSTimer timerWithTimeInterval:.05 target:self selector:@selector(tick:) userInfo:nil repeats:YES];[[NSRunLoop mainRunLoop]addTimer:self.timer forMode:NSRunLoopCommonModes];[[NSRunLoop mainRunLoop]addTimer:self.timer forMode:NSModalPanelRunLoopMode];desktop_reload_profiles(&app);
    [self.window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
}
- (CGFloat)splitView:(NSSplitView *)split constrainMinCoordinate:(CGFloat)proposed ofSubviewAt:(NSInteger)index{(void)proposed;(void)index;return split==self.networkSplit ? 190:180;}
- (CGFloat)splitView:(NSSplitView *)split constrainMaxCoordinate:(CGFloat)proposed ofSubviewAt:(NSInteger)index{(void)proposed;(void)index;return split==self.networkSplit ? MIN(300,MAX(190,split.bounds.size.width-500)):MAX(180,split.bounds.size.height-170);}
- (BOOL)splitView:(NSSplitView *)split canCollapseSubview:(NSView *)view{(void)split;(void)view;return NO;}
- (void)layoutTopControls {
    NSSize size=self.window.contentView.bounds.size;CGFloat width=size.width,height=size.height;
    CGFloat margin=12,gap=10,available=MAX(0,width-2*margin-2*gap);
    CGFloat nic=MIN(290,available*.29),ranges=MIN(420,available*.37);
    self.interfaces.frame=NSMakeRect(margin,height-42,nic,28);
    self.range.frame=NSMakeRect(margin+nic+gap,height-42,ranges,28);
    self.filter.frame=NSMakeRect(margin+nic+ranges+2*gap,height-42,available-nic-ranges,28);
    self.networkInfo.frame=NSMakeRect(margin,height-66,width-2*margin,18);
    self.networkSplit.frame=NSMakeRect(0,30,width,MAX(0,height-104));
    self.status.frame=NSMakeRect(margin,7,width-2*margin,16);
    [self layoutSplit:self.networkSplit];[self layoutSplit:self.hostSplit];[self layoutDetailPane];
    NSSize viewport=self.hostScroll.contentView.bounds.size;
    self.emptyState.frame=NSMakeRect(20,MAX(20,(viewport.height-64)/2),MAX(0,viewport.width-40),64);
}
- (void)layoutDetailPane {
    NSSize size=self.detailPane.bounds.size;
    self.detailSelector.frame=NSMakeRect(12,MAX(0,size.height-34),MIN(610,MAX(0,size.width-24)),26);
    self.detailTabs.frame=NSMakeRect(0,0,size.width,MAX(0,size.height-42));
}
- (void)layoutSplit:(NSSplitView *)split {
    if(split.subviews.count!=2)return;
    CGFloat divider=split.dividerThickness;NSSize size=split.bounds.size;
    if(split==self.networkSplit){
        CGFloat sidebar=MIN(300,MAX(190,NSWidth(split.subviews[0].frame)));
        sidebar=MIN(sidebar,MAX(0,size.width-500));
        split.subviews[0].frame=NSMakeRect(0,0,sidebar,size.height);
        split.subviews[1].frame=NSMakeRect(sidebar+divider,0,MAX(0,size.width-sidebar-divider),size.height);
        [self layoutSplit:self.hostSplit];
    }else{
        CGFloat detail=MIN(300,MAX(170,NSHeight(split.subviews[1].frame)));
        detail=MIN(detail,MAX(0,size.height-180));CGFloat tableHeight=MAX(0,size.height-detail-divider);
        split.subviews[0].frame=NSMakeRect(0,split.isFlipped ? 0:detail+divider,size.width,tableHeight);
        split.subviews[1].frame=NSMakeRect(0,split.isFlipped ? tableHeight+divider:0,size.width,detail);
        [self layoutDetailPane];
        NSSize viewport=self.hostScroll.contentView.bounds.size;
        self.emptyState.frame=NSMakeRect(20,MAX(20,(viewport.height-64)/2),MAX(0,viewport.width-40),64);
    }
}
- (void)splitViewDidResizeSubviews:(NSNotification *)notification{(void)notification;[self layoutDetailPane];}
- (void)windowDidResize:(NSNotification *)notification{(void)notification;[self layoutTopControls];}
- (void)selectDetail:(NSSegmentedControl *)sender{if(sender.selectedSegment>=0)[self.detailTabs selectTabViewItemAtIndex:sender.selectedSegment];}
- (void)tabView:(NSTabView *)view didSelectTabViewItem:(NSTabViewItem *)item{(void)view;self.detailSelector.selectedSegment=[self.detailTabs indexOfTabViewItem:item];}
- (void)showGeneral:(id)sender{(void)sender;if(app.closing || self.table.selectedRow<0)return;[self.detailTabs selectTabViewItemAtIndex:0];}
- (void)localize {
    [self menu];NSArray *tabs=app.russian ? @[@"Общее",@"Порты",@"Ping",@"Trace",@"История",@"Журнал"] : @[@"General",@"Ports",@"Ping",@"Trace",@"History",@"Log"];for(NSUInteger i=0;i<6;++i){self.detailTabs.tabViewItems[i].label=tabs[i];[self.detailSelector setLabel:tabs[i] forSegment:(NSInteger)i];}[self populateSidebar];NSArray *logTitles=app.russian ? @[@"Время",@"Событие",@"IP-адрес",@"Подробности"]:@[@"Time",@"Event",@"IP Address",@"Details"];for(NSUInteger i=0;i<4;++i)self.logTable.tableColumns[i].title=logTitles[i];[self.logTable reloadData];NSArray *titles=app.russian ? @[@"Статус",@"IP-адрес",@"Имя узла",@"MAC-адрес",@"Производитель",@"Устройство / ОС",@"Задержка",@"Сервисы",@"Первый ответ",@"Последний ответ"] : @[@"Status",@"IP Address",@"Host Name",@"MAC Address",@"Vendor",@"Device / OS",@"Latency",@"Services",@"First Seen",@"Last Seen"];
    for(NSUInteger c=0;c<10;++c)self.table.tableColumns[c].title=titles[c];self.filter.placeholderString=[self tr:"Filter hosts…" ru:"Фильтр устройств…"];self.range.placeholderString=[self tr:"Use all profile ranges" ru:"Все диапазоны профиля"];
    NSToolbar *toolbar=[[NSToolbar alloc]initWithIdentifier:@"LookLAN-toolbar"];toolbar.delegate=self;toolbar.displayMode=NSToolbarDisplayModeIconOnly;toolbar.allowsUserCustomization=NO;self.window.toolbar=toolbar;self.window.toolbarStyle=NSWindowToolbarStyleUnifiedCompact;[self refreshRows];
    NSArray *contextNames=@[[self tr:"Copy IP" ru:"Копировать IP"],[self tr:"Copy MAC" ru:"Копировать MAC"],[self tr:"Properties" ru:"Свойства"],[self tr:"Acknowledge" ru:"Подтвердить"],[self tr:"Rescan host" ru:"Повторно проверить узел"]];for(NSUInteger i=0;i<MIN(contextNames.count,self.table.menu.itemArray.count);++i)self.table.menu.itemArray[i].title=contextNames[i];if(self.table.menu.numberOfItems>13){self.table.menu.itemArray[10].title=[self tr:"Copy hostname" ru:"Копировать имя узла"];self.table.menu.itemArray[13].title=[self tr:"Scan ports" ru:"Проверить порты"];}
}
- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar{(void)toolbar;return @[@"scan",@"stop",@"refresh",@"monitor",@"ping",@"trace",NSToolbarFlexibleSpaceItemIdentifier,@"export"];}
- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar{return [self toolbarAllowedItemIdentifiers:toolbar];}
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)identifier willBeInsertedIntoToolbar:(BOOL)flag {
    (void)toolbar;(void)flag;NSArray *ids=@[@"scan",@"stop",@"refresh",@"monitor",@"ping",@"trace",@"export"],*symbols=@[@"network",@"stop.fill",@"arrow.clockwise",@"waveform.path",@"antenna.radiowaves.left.and.right",@"point.3.connected.trianglepath.dotted",@"square.and.arrow.down"];
    NSUInteger i=[ids indexOfObject:identifier];if(i==NSNotFound)return nil;NSToolbarItem *item=[[NSToolbarItem alloc]initWithItemIdentifier:identifier];
    NSArray *labels=app.russian ? @[@"Сканировать",@"Стоп",@"Обновить",@"Мониторинг",@"Ping",@"Trace",@"Экспорт"] : @[@"Scan",@"Stop",@"Refresh",@"Monitoring",@"Ping",@"Trace",@"Export"];
    item.label=labels[i];item.toolTip=labels[i];item.target=self;item.action=NSSelectorFromString(@[@"scan:",@"stop:",@"refresh:",@"monitoring:",@"ping:",@"trace:",@"export:"][i]);
    NSButton *button=[NSButton buttonWithTitle:labels[i] target:self action:item.action];button.image=[NSImage imageWithSystemSymbolName:symbols[i] accessibilityDescription:labels[i]];button.imagePosition=NSImageLeading;button.bezelStyle=NSBezelStyleTexturedRounded;button.controlSize=NSControlSizeSmall;button.font=[NSFont systemFontOfSize:12 weight:NSFontWeightMedium];[button sizeToFit];button.frame=NSMakeRect(0,0,MAX(62,button.frame.size.width+8),28);item.view=button;return item;
}
- (BOOL)validateToolbarItem:(NSToolbarItem *)item{if(app.closing)return NO;if([item.itemIdentifier isEqual:@"stop"])return app.scanning || app.monitoring || app.tools || app.io || app.names_pending;if([item.itemIdentifier isEqual:@"monitor"])return !app.closing && (app.monitoring || (!app.scanning && !app.tools && !app.io));if([item.itemIdentifier isEqual:@"ping"] || [item.itemIdentifier isEqual:@"trace"])return !app.scanning && !app.monitoring && !app.tools && !app.io && self.table.selectedRow>=0;if([item.itemIdentifier isEqual:@"scan"] && app.selected>=app.interface_count)return NO;return !app.scanning && !app.monitoring && !app.tools && !app.io && !app.closing;}
- (void)populateInterfaces {
    [self.interfaces removeAllItems];for(size_t i=0;i<app.interface_count;++i){char ip[32];ipv4_format(app.interfaces[i].ip,ip,sizeof(ip));[self.interfaces addItemWithTitle:[NSString stringWithFormat:@"%s  %s/%u",app.interfaces[i].name,ip,app.interfaces[i].prefix]];}
    [self populateSidebar];if(app.selected<app.interface_count){[self.interfaces selectItemAtIndex:(NSInteger)app.selected];char ranges[4096];profile_ranges_text(&app.profile,ranges,sizeof(ranges));self.range.stringValue=app.profile.range_count==1 ? text(ranges) : @"";self.range.placeholderString=[self tr:"Use all profile ranges" ru:"Все диапазоны профиля"];}
}
- (void)populateSidebar{
    if(!self.sidebar)return;self.updatingSidebar=YES;NSMutableArray *networks=[NSMutableArray new],*profiles=[NSMutableArray new];for(size_t i=0;i<app.interface_count;++i){char ip[32];ipv4_format(app.interfaces[i].ip,ip,sizeof(ip));NSString *caption=[NSString stringWithFormat:@"%s%@",app.interfaces[i].name,app.interfaces[i].automatic ? @"":[self tr:" (manual)" ru:" (вручную)"]];[networks addObject:@{@"title":caption,@"nic":@(i),@"children":@[@{@"title":[NSString stringWithFormat:@"%s/%u",ip,app.interfaces[i].prefix],@"nic":@(i)}]}];}for(size_t i=0;i<app.saved_count;++i)[profiles addObject:@{@"title":text(app.saved_profiles[i].name),@"path":text(app.saved_profiles[i].path)}];self.sidebarNodes=@[@{@"title":[self tr:"Networks" ru:"Сети"],@"children":networks},@{@"title":[self tr:"Saved profiles" ru:"Сохранённые профили"],@"children":profiles}];[self.sidebar reloadData];[self.sidebar expandItem:nil expandChildren:YES];self.updatingSidebar=NO;
}
- (NSInteger)outlineView:(NSOutlineView *)view numberOfChildrenOfItem:(id)item{(void)view;return item ? (NSInteger)[item[@"children"] count]:(NSInteger)self.sidebarNodes.count;}
- (id)outlineView:(NSOutlineView *)view child:(NSInteger)index ofItem:(id)item{(void)view;return item ? item[@"children"][(NSUInteger)index]:self.sidebarNodes[(NSUInteger)index];}
- (BOOL)outlineView:(NSOutlineView *)view isItemExpandable:(id)item{(void)view;return [item[@"children"] count]>0;}
- (NSView *)outlineView:(NSOutlineView *)view viewForTableColumn:(NSTableColumn *)column item:(id)item{(void)view;(void)column;return [NSTextField labelWithString:item[@"title"]];}
- (void)outlineViewSelectionDidChange:(NSNotification *)notification{(void)notification;if(self.updatingSidebar || self.sidebar.selectedRow<0 || app.scanning || app.monitoring || app.tools || app.io)return;id item=[self.sidebar itemAtRow:self.sidebar.selectedRow];if(item[@"nic"]){if([item[@"nic"] unsignedLongLongValue]==app.selected)return;if(desktop_select(&app,[item[@"nic"] unsignedLongLongValue])){[self populateInterfaces];[self refreshRows];}}else if(item[@"path"]){if(!desktop_profile_open(&app,[item[@"path"] fileSystemRepresentation]))[self message:text(app.error)];}}
- (BOOL)validateMenuItem:(NSMenuItem *)item{
    if(app.closing)return NO;
    BOOL idle=!app.scanning && !app.monitoring && !app.tools && !app.io;
    if(item.action==@selector(scan:) || item.action==@selector(refresh:))return idle;
    if(item.action==@selector(monitoring:))return app.monitoring || idle;
    if(item.action==@selector(stop:))return app.scanning || app.monitoring || app.tools || app.io;
    if(item.action==@selector(contextAction:)){
        NSInteger row=self.table.clickedRow>=0 ? self.table.clickedRow:self.table.selectedRow;
        if(row<0 || row>=(NSInteger)self.visible.count || self.visible[(NSUInteger)row].unsignedLongLongValue>=app.host_count)return NO;
        return (item.tag==4 || item.tag==11 || item.tag==12 || item.tag==13) ? idle:YES;
    }
    if(item.action==@selector(ping:) || item.action==@selector(trace:))return idle && self.table.selectedRow>=0;
if(item.action==@selector(openProfile:) || item.action==@selector(editProfile:) || item.action==@selector(single:))return !app.scanning && !app.monitoring && !app.tools && !app.io && !app.closing;if(item.action==@selector(saveProfile:) || item.action==@selector(export:))return !app.io && !app.closing;if(item.action==@selector(notes:)){NSInteger row=self.table.selectedRow;return row>=0 && row<(NSInteger)self.visible.count && self.visible[(NSUInteger)row].unsignedLongLongValue<app.host_count;}return !app.closing;}
- (void)selectInterface:(id)sender{(void)sender;if(desktop_select(&app,(size_t)self.interfaces.indexOfSelectedItem)){[self populateInterfaces];[self refreshRows];}}
- (void)language:(NSMenuItem *)sender{desktop_language(&app,(unsigned)sender.tag,self.systemLanguage.UTF8String);[self localize];}
- (void)filter:(id)sender{(void)sender;[self refreshRows];}
- (void)controlTextDidChange:(NSNotification *)notification{if(notification.object==self.filter)[self refreshRows];}
- (void)message:(NSString *)message{if(app.closing)return;app.error[0]=0;NSAlert *alert=[NSAlert new];alert.messageText=@"Look@LAN-ng";alert.informativeText=message;[alert addButtonWithTitle:[self tr:"Close" ru:"Закрыть"]];[alert runModal];}
- (void)scan:(id)sender{(void)sender;if(app.closing || app.scanning || app.monitoring || app.tools || app.io)return;if(desktop_scope_size(&app,self.range.stringValue.UTF8String)>4096){NSAlert *alert=[NSAlert new];alert.messageText=[self tr:"Scan more than 4096 addresses?" ru:"Сканировать более 4096 адресов?"];[alert addButtonWithTitle:[self tr:"Scan" ru:"Сканировать"]];[alert addButtonWithTitle:[self tr:"Cancel" ru:"Отмена"]];if([alert runModal]!=NSAlertFirstButtonReturn || app.closing)return;}if(!desktop_start(&app,self.range.stringValue.UTF8String))[self message:text(app.error)];}
- (void)stop:(id)sender{(void)sender;desktop_stop(&app);[self refreshRows];}
- (void)monitoring:(id)sender{(void)sender;if(app.monitoring){desktop_stop(&app);[self refreshRows];return;}if(app.scanning)return;[self scan:nil];if(app.scanning)app.monitoring=true;[self refreshRows];}
- (void)refresh:(id)sender{(void)sender;if(app.closing || app.scanning || app.monitoring || app.tools || app.io)return;if(!desktop_refresh(&app)){[self message:text(app.error)];return;}[self populateInterfaces];[self refreshRows];}
- (void)refreshRows {
    if(app.closing)return;
    self.rowsDirty=NO;self.nextRowsRefresh=NSDate.timeIntervalSinceReferenceDate+.15;
    if(self.logSequence!=app.monitor.sequence){self.logSequence=app.monitor.sequence;[self.logTable reloadData];}
    char network[1024];desktop_network_text(&app,network,sizeof(network));self.networkInfo.stringValue=text(network);self.networkInfo.toolTip=text(network);
    uint32_t selectedIP=0;BOOL hadSelection=self.table.selectedRow>=0 && self.table.selectedRow<(NSInteger)self.visible.count;if(hadSelection && self.visible[(NSUInteger)self.table.selectedRow].unsignedLongLongValue>=app.host_count)hadSelection=NO;if(hadSelection)selectedIP=ipv4_value(&app.hosts[self.visible[(NSUInteger)self.table.selectedRow].unsignedLongLongValue].address);
    [self.visible removeAllObjects];for(size_t i=0;i<app.host_count;++i){BOOL match=!self.filter.stringValue.length;for(unsigned c=0;!match && c<10;++c){char value[512];desktop_cell(&app,i,c,value,sizeof(value));match=[text(value) localizedCaseInsensitiveContainsString:self.filter.stringValue];}if(match)[self.visible addObject:@(i)];}
    NSSortDescriptor *sort=self.table.sortDescriptors.firstObject;if(sort){[self.visible sortUsingComparator:^NSComparisonResult(NSNumber *first,NSNumber *second){int order=desktop_compare(&app,first.unsignedLongLongValue,second.unsignedLongLongValue,(unsigned)sort.key.intValue);if(!sort.ascending)order=-order;return order<0 ? NSOrderedAscending:order>0 ? NSOrderedDescending:NSOrderedSame;}];}[self.table reloadData];
    char status[512];desktop_status_text(&app,status,sizeof(status));self.status.stringValue=text(status);self.status.toolTip=text(status);
    if(hadSelection){NSUInteger selected=NSNotFound;for(NSUInteger i=0;i<self.visible.count;++i)if(ipv4_value(&app.hosts[self.visible[i].unsignedLongLongValue].address)==selectedIP){selected=i;break;}if(selected!=NSNotFound)[self.table selectRowIndexes:[NSIndexSet indexSetWithIndex:selected] byExtendingSelection:NO];else [self.table deselectAll:nil];}
    [self tableViewSelectionDidChange:[NSNotification notificationWithName:NSTableViewSelectionDidChangeNotification object:self.table]];self.sidebar.enabled=!app.scanning && !app.monitoring && !app.tools && !app.io;self.interfaces.enabled=!app.scanning && !app.monitoring && !app.tools && !app.io;self.range.enabled=!app.scanning && !app.monitoring && !app.tools && !app.io;[self.window.toolbar validateVisibleItems];for(NSToolbarItem *item in self.window.toolbar.items)if([item.view isKindOfClass:NSButton.class])((NSButton *)item.view).enabled=[self validateToolbarItem:item];self.emptyState.hidden=self.visible.count!=0;self.emptyState.stringValue=self.filter.stringValue.length ? [self tr:"No matching devices\nTry another search" ru:"Ничего не найдено\nИзмените фильтр"] : [self tr:"Discover your network\nSelect an interface and click Scan" ru:"Устройства вашей сети\nВыберите интерфейс и нажмите «Сканировать»"];
}
- (NSInteger)numberOfRowsInTableView:(NSTableView *)table{if(table==self.logTable)return (NSInteger)app.monitor.log_count;return (NSInteger)self.visible.count;}
- (NSView *)tableView:(NSTableView *)table viewForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
    if(table==self.logTable){char value[600];desktop_log_cell(&app,(size_t)row,(unsigned)column.identifier.intValue,value,sizeof(value));NSTextField *label=[NSTextField labelWithString:text(value)];label.lineBreakMode=NSLineBreakByTruncatingTail;label.toolTip=text(value);return label;}
    NSTextField *label=[NSTextField labelWithString:@""];char value[512];if(row<0 || row>=(NSInteger)self.visible.count)return label;size_t index=self.visible[(NSUInteger)row].unsignedLongLongValue;if(index>=app.host_count)return label;desktop_cell(&app,index,(unsigned)column.identifier.intValue,value,sizeof(value));label.stringValue=text(value);label.lineBreakMode=NSLineBreakByTruncatingTail;if(column.identifier.intValue==0){const lan_host_t *host=&app.hosts[index];NSView *cell=[[NSView alloc]initWithFrame:NSMakeRect(0,0,column.width,24)];NSView *dot=[[NSView alloc]initWithFrame:NSMakeRect(4,9,7,7)];dot.wantsLayer=YES;dot.layer.cornerRadius=3.5;dot.layer.backgroundColor=(!host->first_seen || host->cache_only || !host->online ? NSColor.systemGrayColor:host->new_device ? NSColor.systemTealColor:host->changed ? NSColor.systemOrangeColor:NSColor.systemGreenColor).CGColor;label.frame=NSMakeRect(18,3,column.width-18,20);label.autoresizingMask=NSViewWidthSizable;[cell addSubview:dot];[cell addSubview:label];return cell;}return label;
}
- (void)tableViewSelectionDidChange:(NSNotification *)notification{if(notification.object==self.logTable)return;NSInteger row=self.table.selectedRow;size_t index=row>=0 && row<(NSInteger)self.visible.count ? self.visible[(NSUInteger)row].unsignedLongLongValue:SIZE_MAX;for(unsigned part=0;part<5;++part){char value[16384];desktop_detail_part(&app,index,part,value,sizeof(value));self.detailViews[part].string=index==SIZE_MAX ? [self tr:"Select a device in the table to see its details." ru:"Выберите устройство в таблице, чтобы увидеть подробности."]:text(value);}[self.window.toolbar validateVisibleItems];for(NSToolbarItem *item in self.window.toolbar.items)if([item.view isKindOfClass:NSButton.class])((NSButton *)item.view).enabled=[self validateToolbarItem:item];}
- (void)diagnostic:(BOOL)trace{if(app.closing || app.scanning || app.monitoring || app.tools || app.io)return;NSInteger row=self.table.selectedRow;if(row<0 || row>=(NSInteger)self.visible.count || self.visible[(NSUInteger)row].unsignedLongLongValue>=app.host_count)return;const lan_host_t *host=&app.hosts[self.visible[(NSUInteger)row].unsignedLongLongValue];if(desktop_diagnostic(&app,ipv4_value(&host->address),trace))[self.detailTabs selectTabViewItemAtIndex:trace ? 3:2];else[self message:[self tr:"Cannot start diagnostic" ru:"Не удалось начать диагностику"]];[self refreshRows];}
- (void)ping:(id)sender{(void)sender;[self diagnostic:NO];}
- (void)trace:(id)sender{(void)sender;[self diagnostic:YES];}
- (void)tableView:(NSTableView *)table sortDescriptorsDidChange:(NSArray<NSSortDescriptor *> *)old{(void)table;(void)old;[self refreshRows];}
- (void)contextAction:(NSMenuItem *)sender{if(![self validateMenuItem:sender])return;NSInteger row=self.table.clickedRow>=0 ? self.table.clickedRow:self.table.selectedRow;if(row<0 || row>=(NSInteger)self.visible.count || self.visible[(NSUInteger)row].unsignedLongLongValue>=app.host_count)return;size_t index=self.visible[(NSUInteger)row].unsignedLongLongValue;const lan_host_t *host=&app.hosts[index];char ip[32];ipv4_format(ipv4_value(&host->address),ip,sizeof(ip));
    if(sender.tag==10){[NSPasteboard.generalPasteboard clearContents];[NSPasteboard.generalPasteboard setString:text(host->hostname) forType:NSPasteboardTypeString];return;}if(sender.tag==11 || sender.tag==12){if(desktop_diagnostic(&app,ipv4_value(&host->address),sender.tag==12))[self.detailTabs selectTabViewItemAtIndex:sender.tag==12 ? 3:2];else[self message:[self tr:"Cannot start diagnostic" ru:"Не удалось начать диагностику"]];[self refreshRows];return;}if(sender.tag==13){if(!app.scanning && !app.monitoring && !app.tools && !app.io){if(!desktop_single(&app,ipv4_value(&host->address)))[self message:text(app.error)];[self refreshRows];}return;}if(sender.tag<2){char value[512];desktop_cell(&app,index,sender.tag==0 ? 1:3,value,sizeof(value));[NSPasteboard.generalPasteboard clearContents];[NSPasteboard.generalPasteboard setString:text(value) forType:NSPasteboardTypeString];return;}if(sender.tag==2){char value[16384];desktop_detail(&app,index,value,sizeof(value));[self showText:text(value) title:[self tr:"Properties" ru:"Свойства"]];return;}if(sender.tag==3){monitor_acknowledge(&app.monitor,index);[self refreshRows];return;}if(sender.tag==4){if(!app.scanning && !app.monitoring && !app.tools && !app.io){if(!desktop_single(&app,ipv4_value(&host->address)))[self message:text(app.error)];[self refreshRows];}return;}if(sender.tag>=5 && sender.tag<10){NSArray *schemes=@[@"http",@"https",@"ssh",@"smb",@"rdp"];NSURL *url=[NSURL URLWithString:[NSString stringWithFormat:@"%@://%s",schemes[(NSUInteger)sender.tag-5],ip]];if(![NSWorkspace.sharedWorkspace openURL:url])[self message:[self tr:"No system application is configured for this action" ru:"Для этого действия не настроено системное приложение"]];}}
- (void)tick:(id)sender {
    (void)sender;
    if(self.terminating){if(desktop_close(&app)){[self.timer invalidate];[NSApp replyToApplicationShouldTerminate:YES];}return;}
    if(desktop_poll(&app)){
        if(app.pending_profile && !self.remapping)[self remapProfile];
        if(app.closing)return;
        if(app.profile_changed){[self populateInterfaces];app.profile_changed=false;}
        if(app.saved_changed){[self populateSidebar];app.saved_changed=false;}
        self.rowsDirty=YES;
        if(app.error[0] && !app.scanning && !NSApp.modalWindow)[self message:text(app.error)];
    }
    if(self.rowsDirty && (!app.scanning || NSDate.timeIntervalSinceReferenceDate>=self.nextRowsRefresh))[self refreshRows];
}
- (void)remapProfile{self.remapping=YES;NSAlert *alert=[NSAlert new];alert.messageText=[self tr:"Profile interface unavailable" ru:"Интерфейс профиля недоступен"];[alert addButtonWithTitle:[self tr:"Use selected interface" ru:"Использовать выбранный интерфейс"]];[alert addButtonWithTitle:[self tr:"Cancel" ru:"Отмена"]];NSPopUpButton *combo=[[NSPopUpButton alloc]initWithFrame:NSMakeRect(0,0,320,28)];for(size_t i=0;i<app.interface_count;++i)[combo addItemWithTitle:text(app.interfaces[i].name)];if(app.selected<app.interface_count)[combo selectItemAtIndex:(NSInteger)app.selected];alert.accessoryView=combo;NSModalResponse response=[alert runModal];if(!app.closing)desktop_profile_rebind(&app,response==NSAlertFirstButtonReturn ? (size_t)combo.indexOfSelectedItem:SIZE_MAX);self.remapping=NO;}
- (void)export:(id)sender{(void)sender;if(app.closing || app.io)return;NSSavePanel *panel=NSSavePanel.savePanel;panel.nameFieldStringValue=@"LookLAN.csv";if([panel runModal]==NSModalResponseOK && !app.closing){NSString *ext=panel.URL.pathExtension.lowercaseString;report_format_t format=[ext isEqual:@"html"] ? REPORT_HTML : [ext isEqual:@"txt"] ? REPORT_TXT : REPORT_CSV;if(!desktop_export(&app,panel.URL.path.fileSystemRepresentation,format))[self message:[self tr:"Export failed" ru:"Ошибка экспорта"]];}}
- (void)openProfile:(id)sender{(void)sender;if(app.closing || app.scanning || app.monitoring || app.tools || app.io)return;NSOpenPanel *panel=NSOpenPanel.openPanel;panel.allowedFileTypes=@[@"lanprofile"];if([panel runModal]==NSModalResponseOK && !app.closing){if(!desktop_profile_open(&app,panel.URL.path.fileSystemRepresentation))[self message:text(app.error)];else{[self populateInterfaces];[self refreshRows];}}}
- (void)saveProfile:(id)sender{(void)sender;if(app.closing || app.io)return;NSSavePanel *panel=NSSavePanel.savePanel;panel.nameFieldStringValue=@"LookLAN.lanprofile";if([panel runModal]==NSModalResponseOK && !app.closing && !desktop_profile_save(&app,panel.URL.path.fileSystemRepresentation))[self message:[self tr:"Profile save failed" ru:"Не удалось сохранить профиль"]];}
- (void)about:(id)sender{(void)sender;[self message:@"Look@LAN-ng " @APP_VERSION @"\n© nr084, " @APP_BUILD_YEAR];}
- (void)showText:(NSString *)value title:(NSString *)title{if(app.closing)return;NSAlert *alert=[NSAlert new];alert.messageText=title;[alert addButtonWithTitle:[self tr:"Close" ru:"Закрыть"]];NSScrollView *scroll=[[NSScrollView alloc]initWithFrame:NSMakeRect(0,0,680,400)];scroll.hasVerticalScroller=YES;NSTextView *view=[[NSTextView alloc]initWithFrame:scroll.bounds];view.editable=NO;view.string=value;scroll.documentView=view;alert.accessoryView=scroll;[alert runModal];}
- (void)history:(id)sender{(void)sender;char *value=malloc(128*1024);if(!value)return;desktop_history(&app,value,128*1024);[self showText:text(value) title:[self tr:"History" ru:"История"]];free(value);}
- (void)statistics:(id)sender{(void)sender;char value[1024];desktop_statistics_text(&app,value,sizeof(value));[self showText:text(value) title:[self tr:"Statistics" ru:"Статистика"]];}
- (void)editProfile:(id)sender{
    (void)sender;if(app.closing || app.scanning || app.monitoring || app.tools || app.io)return;NSAlert *alert=[NSAlert new];alert.messageText=[self tr:"Profile" ru:"Профиль"];[alert addButtonWithTitle:[self tr:"Apply" ru:"Применить"]];[alert addButtonWithTitle:[self tr:"Cancel" ru:"Отмена"]];char ranges[4096],ports[PROFILE_PORT_TEXT_CAPACITY];profile_ranges_text(&app.profile,ranges,sizeof(ranges));profile_ports_text(&app.profile,ports,sizeof(ports));
    NSArray *names=@[[self tr:"Name" ru:"Название"],[self tr:"IPv4 ranges (;)" ru:"Диапазоны IPv4 (;)"],[self tr:"TCP ports" ru:"TCP-порты"],[self tr:"ICMP timeout (ms)" ru:"Таймаут ICMP (мс)"],[self tr:"TCP timeout (ms)" ru:"Таймаут TCP (мс)"],[self tr:"Workers (8–256)" ru:"Потоки (8–256)"],[self tr:"Interval (seconds)" ru:"Интервал (секунды)"],[self tr:"Notes" ru:"Заметки"]];
    NSArray *values=@[text(app.profile.name),[text(ranges) stringByReplacingOccurrencesOfString:@"\r\n" withString:@";"],text(ports),@(app.profile.icmp_timeout_ms).stringValue,@(app.profile.tcp_timeout_ms).stringValue,@(app.profile.concurrency).stringValue,@(app.profile.monitor_interval_sec).stringValue,text(app.profile.notes)];NSMutableArray *rows=[NSMutableArray new],*fields=[NSMutableArray new];
    for(NSUInteger i=0;i<names.count;++i){NSTextField *field=[[NSTextField alloc]initWithFrame:NSMakeRect(0,0,420,24)];field.stringValue=values[i];[fields addObject:field];[rows addObject:@[[NSTextField labelWithString:names[i]],field]];}NSGridView *grid=[NSGridView gridViewWithViews:rows];grid.frame=NSMakeRect(0,0,650,260);grid.rowSpacing=8;alert.accessoryView=grid;
    while(!app.closing && [alert runModal]==NSAlertFirstButtonReturn && !app.closing){unsigned numbers[4];BOOL valid=YES;for(unsigned i=0;i<4;++i){const char *s=((NSTextField *)fields[i+3]).stringValue.UTF8String;char *end;unsigned long n=strtoul(s,&end,10);if(!*s || *end || n>5000)valid=NO;numbers[i]=(unsigned)n;}
        if(valid && desktop_profile_configure(&app,((NSTextField *)fields[0]).stringValue.UTF8String,((NSTextField *)fields[1]).stringValue.UTF8String,((NSTextField *)fields[2]).stringValue.UTF8String,numbers[0],numbers[1],numbers[2],numbers[3],((NSTextField *)fields[7]).stringValue.UTF8String)){[self populateInterfaces];[self refreshRows];break;}[self message:valid ? text(app.error) : [self tr:"Invalid number" ru:"Некорректное число"]];}
}
- (void)single:(id)sender{(void)sender;if(app.closing || app.scanning || app.monitoring || app.tools || app.io)return;NSAlert *alert=[NSAlert new];alert.messageText=[self tr:"Single host" ru:"Один узел"];[alert addButtonWithTitle:[self tr:"Scan" ru:"Сканировать"]];[alert addButtonWithTitle:[self tr:"Cancel" ru:"Отмена"]];NSTextField *field=[[NSTextField alloc]initWithFrame:NSMakeRect(0,0,320,24)];field.placeholderString=[self tr:"Host name or IPv4" ru:"Имя узла или IPv4"];alert.accessoryView=field;if([alert runModal]==NSAlertFirstButtonReturn && !app.closing){if(!desktop_single_target(&app,field.stringValue.UTF8String))[self message:text(app.error)];[self refreshRows];}}
- (void)notes:(id)sender{(void)sender;if(app.closing)return;NSInteger row=self.table.selectedRow;if(row<0 || row>=(NSInteger)self.visible.count || self.visible[(NSUInteger)row].unsignedLongLongValue>=app.host_count)return;const lan_host_t *host=&app.hosts[self.visible[(NSUInteger)row].unsignedLongLongValue];uint32_t noteIP=ipv4_value(&host->address),noteInterface=host->interface_index;NSAlert *alert=[NSAlert new];alert.messageText=[self tr:"Notes" ru:"Заметки"];[alert addButtonWithTitle:[self tr:"Save" ru:"Сохранить"]];[alert addButtonWithTitle:[self tr:"Cancel" ru:"Отмена"]];NSTextField *field=[[NSTextField alloc]initWithFrame:NSMakeRect(0,0,480,24)];field.stringValue=text(details_note(app.details,noteIP,noteInterface));alert.accessoryView=field;if([alert runModal]==NSAlertFirstButtonReturn && !app.closing && !details_note_set(app.details,noteIP,noteInterface,field.stringValue.UTF8String))[self message:[self tr:"Notes limit exceeded" ru:"Превышен лимит заметок"]];if(!app.closing)[self refreshRows];}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender{(void)sender;if(NSApp.modalWindow)[NSApp abortModal];if(!self.terminating && desktop_close(&app)){[self.timer invalidate];return NSTerminateNow;}self.terminating=YES;return NSTerminateLater;}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender{(void)sender;return YES;}
- (BOOL)windowShouldClose:(NSWindow *)sender{(void)sender;[NSApp terminate:nil];return NO;}
@end
int main(int argc,char **argv){@autoreleasepool {
    if(argc>1 && !strcmp(argv[1],"--smoke-test"))return desktop_smoke(NSLocale.preferredLanguages.firstObject.UTF8String);
    [NSApplication sharedApplication];NSApp.activationPolicy=NSApplicationActivationPolicyRegular;__attribute__((objc_precise_lifetime)) LANDelegate *delegate=[LANDelegate new];NSApp.delegate=delegate;[NSApp run];return 0;
}}
