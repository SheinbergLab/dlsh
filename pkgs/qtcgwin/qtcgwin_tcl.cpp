#include "qtcgwin.hpp"
#include "qtcgmanager.hpp"
#include <tcl.h>
#include <QApplication>
#include <QWidget>
#include <QFileDialog>

// Only include cgraph headers in this module
extern "C" {
#include <cgraph.h>
#include <gbuf.h>
}

// Bridge class to set up callbacks and access private members
class QtCGWinBridge {
public:
    static void setupCallbacks() {
        // Set cgraph callbacks to use QtCGWin methods
        setline((LHANDLER) QtCGWin::Line);
        setclearfunc((HANDLER) QtCGWin::Clearwin);
        setpoint((PHANDLER) QtCGWin::Point);
        setcolorfunc((COHANDLER) QtCGWin::Setcolor);
        setchar((THANDLER) QtCGWin::Char);
        settext((THANDLER) QtCGWin::Text);
        strwidthfunc((SWHANDLER) QtCGWin::Strwidth);
        strheightfunc((SHHANDLER) QtCGWin::Strheight);
        setfontfunc((SFHANDLER) QtCGWin::Setfont);
        setfilledpoly((FHANDLER) QtCGWin::FilledPolygon);
        setcircfunc((CHANDLER) QtCGWin::Circle);
    }
    
    // Set the graphics buffer in the widget
    static void setGraphicsBuffer(QtCGWin* widget, void* gbuf) {
        widget->gbuf = gbuf;
    }
    
    // Set the frame pointer in the widget
    static void setFrame(QtCGWin* widget, void* frame) {
        widget->frame = (FRAME_MINIMAL*)frame;
    }
};

// Helper function to get widget from name or pointer
static QtCGWin* getWidgetFromNameOrPtr(Tcl_Interp *interp, Tcl_Obj *obj)
{
    QtCGWin* widget = nullptr;
    
    // Try to interpret as a pointer first
    void* ptr = nullptr;
    if (Tcl_GetLongFromObj(nullptr, obj, (long*)&ptr) == TCL_OK && ptr) {
        widget = static_cast<QtCGWin*>(ptr);
    } else {
        // If not a pointer, try as a window name
        const char* name = Tcl_GetString(obj);
        auto& manager = QtCGManager::getInstance();
        widget = manager.getCGWin(name);
    }
    
    return widget;
}


// Initialize a widget
static int qtcgwin_init_widget_cmd(ClientData data, Tcl_Interp *interp,
                                  int objc, Tcl_Obj *const objv[])
{
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "widget_ptr width height");
        return TCL_ERROR;
    }
    
    void* ptr = nullptr;
    int width, height;
    
    if (Tcl_GetLongFromObj(interp, objv[1], (long*)&ptr) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[2], &width) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[3], &height) != TCL_OK) return TCL_ERROR;
    
    QtCGWin* widget = static_cast<QtCGWin*>(ptr);
    
    // Create and initialize the graphics buffer
    GBUF_DATA* gbuf = (GBUF_DATA*) calloc(1, sizeof(GBUF_DATA));
    gbDisableGevents();
    gbInitGeventBuffer(gbuf);
    gbSetGeventBuffer(gbuf);
    gbEnableGevents();
    
    // Store in widget using bridge
    QtCGWinBridge::setGraphicsBuffer(widget, gbuf);
    
    // Also store the frame pointer
    FRAME* frame = getframe();
    QtCGWinBridge::setFrame(widget, frame);
    
    // Set up resolution
    setresol(width, height);
    setwindow(0, 0, width-1, height-1);
    setfviewport(0, 0, 1, 1);
    setcolor(0);
    gbInitGevents();
    
    return TCL_OK;
}

// Playback graphics events
static int qtcgwin_playback_cmd(ClientData data, Tcl_Interp *interp,
                               int objc, Tcl_Obj *const objv[])
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "gbuf_ptr");
        return TCL_ERROR;
    }
    
    void* ptr = nullptr;
    if (Tcl_GetLongFromObj(interp, objv[1], (long*)&ptr) != TCL_OK) return TCL_ERROR;
    
    GBUF_DATA* gbuf = static_cast<GBUF_DATA*>(ptr);
    if (gbuf) {
        gbSetGeventBuffer(gbuf);
        
        // Make sure resolution matches widget size
        QtCGWin* widget = QtCGManager::getInstance().getCurrentCGWin();
        if (widget) {
            FRAME* f = getframe();
            if (f) {
                int width = widget->width();
                int height = widget->height();
                if (f->xsres != width || f->ysres != height) {
                    setresol(width, height);
                    setwindow(0, 0, width-1, height-1);
                    setfviewport(0, 0, 1, 1);
                }
            }
        }
        
        gbPlaybackGevents();
    }
    
    return TCL_OK;
}

// Updated resize command
static int qtcgwin_resize_cmd(ClientData data, Tcl_Interp *interp,
                             int objc, Tcl_Obj *const objv[])
{
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "window_name width height");
        return TCL_ERROR;
    }
    
    QtCGWin* widget = getWidgetFromNameOrPtr(interp, objv[1]);
    if (!widget) {
        Tcl_AppendResult(interp, "CGraph window not found", NULL);
        return TCL_ERROR;
    }
    
    int width, height;
    if (Tcl_GetIntFromObj(interp, objv[2], &width) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[3], &height) != TCL_OK) return TCL_ERROR;
    
    // Make sure we're operating on the correct buffer
    if (widget->getGraphicsBuffer()) {
        GBUF_DATA* gbuf = static_cast<GBUF_DATA*>(widget->getGraphicsBuffer());
        gbSetGeventBuffer(gbuf);
        
        // Update cgraph resolution and window
        setresol(width, height);
        setwindow(0, 0, width-1, height-1);
        setfviewport(0, 0, 1, 1);
        
        // Get the frame and update it too
        FRAME* f = getframe();
        if (f) {
            // Update frame resolution
            f->xsres = width;
            f->ysres = height;
            // Update viewport to match new size
            f->xr = width - 1;
            f->yt = height - 1;
        }
        
        // Trigger a repaint
        widget->refresh();
    }
    
    return TCL_OK;
}

// Updated clear command
static int qtcgwin_clear_cmd(ClientData data, Tcl_Interp *interp,
                            int objc, Tcl_Obj *const objv[])
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "window_name");
        return TCL_ERROR;
    }
    
    QtCGWin* widget = getWidgetFromNameOrPtr(interp, objv[1]);
    if (!widget) {
        Tcl_AppendResult(interp, "CGraph window not found", NULL);
        return TCL_ERROR;
    }
    
    if (widget->getGraphicsBuffer()) {
        GBUF_DATA* gbuf = static_cast<GBUF_DATA*>(widget->getGraphicsBuffer());
        gbSetGeventBuffer(gbuf);
        gbResetGevents();
    }
    
    return TCL_OK;
}

// Helper function to parse color string to QColor
static QColor parseColorString(const char* colorStr)
{
    QColor color;
    
    if (!colorStr || !colorStr[0]) {
        return color; // Invalid (default constructed QColor)
    }
    
    // Check for hex color
    if (colorStr[0] == '#') {
        color = QColor(colorStr);
        return color;
    }
    
    // Convert to lowercase for case-insensitive comparison
    QString colorName = QString(colorStr).toLower();
    
    // Map of common color names to Qt colors
    static const QMap<QString, QColor> colorMap = {
        {"white", Qt::white},
        {"black", Qt::black},
        {"red", Qt::red},
        {"green", Qt::green},
        {"blue", Qt::blue},
        {"yellow", Qt::yellow},
        {"cyan", Qt::cyan},
        {"magenta", Qt::magenta},
        {"gray", Qt::gray},
        {"grey", Qt::gray},
        {"lightgray", Qt::lightGray},
        {"lightgrey", Qt::lightGray},
        {"darkgray", Qt::darkGray},
        {"darkgrey", Qt::darkGray},
        {"darkred", Qt::darkRed},
        {"darkgreen", Qt::darkGreen},
        {"darkblue", Qt::darkBlue},
        {"darkyellow", Qt::darkYellow},
        {"darkcyan", Qt::darkCyan},
        {"darkmagenta", Qt::darkMagenta},
        {"transparent", Qt::transparent}
    };
    
    // Try our color map first
    auto it = colorMap.find(colorName);
    if (it != colorMap.end()) {
        return it.value();
    }
    
    // Fall back to Qt's color database
    // QColor constructor handles many more color names
    color = QColor(colorStr);
    
    return color;
}

// Simplified setbackground command using the helper
static int qtcgwin_setbackground_cmd(ClientData data, Tcl_Interp *interp,
                                    int objc, Tcl_Obj *const objv[])
{
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "window_name color");
        return TCL_ERROR;
    }
    
    QtCGWin* widget = getWidgetFromNameOrPtr(interp, objv[1]);
    if (!widget) {
        Tcl_AppendResult(interp, "CGraph window not found", NULL);
        return TCL_ERROR;
    }
    
    // Parse the color using our helper
    const char* colorStr = Tcl_GetString(objv[2]);
    QColor color = parseColorString(colorStr);
    
    if (!color.isValid()) {
        Tcl_AppendResult(interp, "invalid color: ", colorStr, NULL);
        return TCL_ERROR;
    }
    
    // Set the background color
    widget->setBackgroundColor(color);
    
    return TCL_OK;
}

// Add a color name mapping structure
static QMap<QString, int> colorNameToIndex = {
    {"black", 0},
    {"blue", 1},
    {"dark_green", 2},
    {"cyan", 3},
    {"red", 4},
    {"magenta", 5},
    {"brown", 6},
    {"white", 7},
    {"gray", 8},
    {"grey", 8},  // Alternative spelling
    {"light_blue", 9},
    {"green", 10},
    {"light_cyan", 11},
    {"deep_pink", 12},
    {"medium_purple", 13},
    {"yellow", 14},
    {"navy", 15},
    {"bright_white", 16},
    {"light_gray", 17},
    {"light_grey", 17}  // Alternative spelling
};

// In qtcgwin_tcl.cpp, add this new command:

// Setcolor command that accepts both index and name
static int qtcgwin_setcolor_cmd(ClientData data, Tcl_Interp *interp,
                               int objc, Tcl_Obj *const objv[])
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "color_index_or_name");
        return TCL_ERROR;
    }
    
    int colorIndex = -1;
    
    // Try to parse as integer first
    if (Tcl_GetIntFromObj(nullptr, objv[1], &colorIndex) == TCL_OK) {
        // It's a number, use it directly
    } else {
        // Try as a color name
        QString colorName = QString(Tcl_GetString(objv[1])).toLower();
        
        // Check our color name map
        auto it = colorNameToIndex.find(colorName);
        if (it != colorNameToIndex.end()) {
            colorIndex = it.value();
        } else {
            Tcl_AppendResult(interp, "Unknown color name: ", 
                           Tcl_GetString(objv[1]), NULL);
            return TCL_ERROR;
        }
    }
    
    // Make sure we have a current widget
    QtCGWin* current = QtCGManager::getInstance().getCurrentCGWin();
    if (!current) {
        Tcl_SetResult(interp, const_cast<char*>("No current cgraph window"), TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Call the original setcolor function
    int oldColor = setcolor(colorIndex);
    
    // Return the old color index
    Tcl_SetObjResult(interp, Tcl_NewIntObj(oldColor));
    return TCL_OK;
}

// list available color names as a dictionary
static int qtcgwin_colorlist_cmd(ClientData data, Tcl_Interp *interp,
                               int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj* dictObj = Tcl_NewDictObj();
    
    // Add all color names to dictionary (excluding alternative spellings)
    for (auto it = colorNameToIndex.begin(); it != colorNameToIndex.end(); ++it) {
        // Skip alternative spellings
        if (it.key() == "grey" || it.key() == "light_grey") continue;
        
        Tcl_Obj* key = Tcl_NewStringObj(it.key().toUtf8().constData(), -1);
        Tcl_Obj* value = Tcl_NewIntObj(it.value());
        Tcl_DictObjPut(interp, dictObj, key, value);
    }
    
    Tcl_SetObjResult(interp, dictObj);
    return TCL_OK;
}

// Set current graphics buffer
static int qtcgwin_set_current_cmd(ClientData data, Tcl_Interp *interp,
                                  int objc, Tcl_Obj *const objv[])
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "gbuf_ptr");
        return TCL_ERROR;
    }
    
    void* ptr = nullptr;
    if (Tcl_GetLongFromObj(interp, objv[1], (long*)&ptr) != TCL_OK) return TCL_ERROR;
    
    GBUF_DATA* gbuf = static_cast<GBUF_DATA*>(ptr);
    if (gbuf) {
        gbSetGeventBuffer(gbuf);
    }
    
    return TCL_OK;
}

static int qtcgwin_bind_cmd(ClientData data, Tcl_Interp *interp,
                           int objc, Tcl_Obj *const objv[])
{
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "window_name event script");
        return TCL_ERROR;
    }
    
    QtCGWin* widget = getWidgetFromNameOrPtr(interp, objv[1]);
    if (!widget) {
        Tcl_AppendResult(interp, "CGraph window not found", NULL);
        return TCL_ERROR;
    }
    
    const char* event = Tcl_GetString(objv[2]);
    const char* script = Tcl_GetString(objv[3]);
    
    // Mouse events
    if (strcmp(event, "<ButtonPress>") == 0 || strcmp(event, "<Button>") == 0) {
        widget->setMouseDownScript(script);
    } else if (strcmp(event, "<ButtonRelease>") == 0) {
        widget->setMouseUpScript(script);
    } else if (strcmp(event, "<Motion>") == 0) {
        widget->setMouseMoveScript(script);
        widget->setMouseTracking(strlen(script) > 0);
    } else if (strcmp(event, "<Double-Button>") == 0) {
        widget->setMouseDoubleClickScript(script);
    } else if (strcmp(event, "<MouseWheel>") == 0) {
        widget->setMouseWheelScript(script);
    }
    // Keyboard events
    else if (strcmp(event, "<KeyPress>") == 0 || strcmp(event, "<Key>") == 0) {
        widget->setKeyPressScript(script);
    } else if (strcmp(event, "<KeyRelease>") == 0) {
        widget->setKeyReleaseScript(script);
    }
    // Focus events
    else if (strcmp(event, "<FocusIn>") == 0) {
        widget->setFocusInScript(script);
    } else if (strcmp(event, "<FocusOut>") == 0) {
        widget->setFocusOutScript(script);
    }
    else {
        Tcl_AppendResult(interp, "Unknown event: ", event, 
            ". Supported events: <ButtonPress>, <ButtonRelease>, <Motion>, "
            "<Double-Button>, <MouseWheel>, <KeyPress>, <KeyRelease>, "
            "<FocusIn>, <FocusOut>", NULL);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}


// Override the cgraph flushwin command
static int cgwinFlushwinCmd(ClientData data, Tcl_Interp *interp,
                           int objc, Tcl_Obj * const objv[])
{
    QtCGWin* currentWidget = QtCGManager::getInstance().getCurrentCGWin();
    if (currentWidget) {
        currentWidget->refresh();
    }
    return TCL_OK;
}

// Manager commands - list all windows
static int qtcg_list_cmd(ClientData data, Tcl_Interp *interp,
                        int objc, Tcl_Obj *const objv[])
{
    auto& manager = QtCGManager::getInstance();
    QStringList names = manager.getAllCGWinNames();
    
    Tcl_Obj* listObj = Tcl_NewListObj(0, NULL);
    for (const QString& name : names) {
        Tcl_ListObjAppendElement(interp, listObj, 
            Tcl_NewStringObj(name.toUtf8().constData(), -1));
    }
    
    Tcl_SetObjResult(interp, listObj);
    return TCL_OK;
}

// Select a window by name
static int qtcg_select_cmd(ClientData data, Tcl_Interp *interp,
                          int objc, Tcl_Obj *const objv[])
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "window_name");
        return TCL_ERROR;
    }
    
    const char* name = Tcl_GetString(objv[1]);
    auto& manager = QtCGManager::getInstance();
    QtCGWin* cgwin = manager.getCGWin(name);
    
    if (!cgwin) {
        Tcl_AppendResult(interp, "CGraph window not found: ", name, NULL);
        return TCL_ERROR;
    }
    
    manager.setCurrentCGWin(cgwin);
    
    // Set the graphics buffer
    if (cgwin->getGraphicsBuffer()) {
        GBUF_DATA* gbuf = static_cast<GBUF_DATA*>(cgwin->getGraphicsBuffer());
        gbSetGeventBuffer(gbuf);
    }
    
    return TCL_OK;
}

// Get current window name
static int qtcg_current_cmd(ClientData data, Tcl_Interp *interp,
                           int objc, Tcl_Obj *const objv[])
{
    auto& manager = QtCGManager::getInstance();
    QString name = manager.getCurrentCGWinName();
    
    if (name.isEmpty()) {
        Tcl_SetResult(interp, const_cast<char*>(""), TCL_STATIC);
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(name.toUtf8().constData(), -1));
    }
    
    return TCL_OK;
}

// Export current window (with dialog)
static int qtcg_export_dialog_cmd(ClientData data, Tcl_Interp *interp,
                                 int objc, Tcl_Obj *const objv[])
{
    QtCGWin* current = QtCGManager::getInstance().getCurrentCGWin();
    if (!current) {
        Tcl_SetResult(interp, const_cast<char*>("No current cgraph window"), TCL_STATIC);
        return TCL_ERROR;
    }
    
    bool success = current->exportToPDFDialog();
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(success));
    return TCL_OK;
}

// Tab widget commands
static int qtCgAddTab_cmd(ClientData data, Tcl_Interp *interp,
                         int objc, Tcl_Obj *const objv[])
{
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "tabs_widget ?label?");
        return TCL_ERROR;
    }
    
    // Get the tab widget
    const char* widget_id = Tcl_GetString(objv[1]);
    QtCGTabWidget* tabs = static_cast<QtCGTabWidget*>(
        Tcl_GetAssocData(interp, widget_id, nullptr));
    
    if (!tabs) {
        Tcl_AppendResult(interp, "Tab widget not found: ", widget_id, NULL);
        return TCL_ERROR;
    }
    
    QString label;
    if (objc > 2) {
        label = Tcl_GetString(objv[2]);
    }
    
    QString tabName = tabs->addCGTab(label);
    
    Tcl_SetObjResult(interp, Tcl_NewStringObj(tabName.toUtf8().constData(), -1));
    return TCL_OK;
}

// Select tab by name
static int qtCgSelectTab_cmd(ClientData data, Tcl_Interp *interp,
                            int objc, Tcl_Obj *const objv[])
{
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "tabs_widget tab_name");
        return TCL_ERROR;
    }
    
    const char* widget_id = Tcl_GetString(objv[1]);
    QtCGTabWidget* tabs = static_cast<QtCGTabWidget*>(
        Tcl_GetAssocData(interp, widget_id, nullptr));
    
    if (!tabs) {
        Tcl_AppendResult(interp, "Tab widget not found: ", widget_id, NULL);
        return TCL_ERROR;
    }
    
    const char* tab_name = Tcl_GetString(objv[2]);
    if (tabs->selectCGTab(tab_name)) {
        return TCL_OK;
    }
    
    Tcl_SetResult(interp, const_cast<char*>("Tab not found"), TCL_STATIC);
    return TCL_ERROR;
}

// Delete tab by name
static int qtCgDeleteTab_cmd(ClientData data, Tcl_Interp *interp,
                            int objc, Tcl_Obj *const objv[])
{
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "tabs_widget tab_name");
        return TCL_ERROR;
    }
    
    const char* widget_id = Tcl_GetString(objv[1]);
    QtCGTabWidget* tabs = static_cast<QtCGTabWidget*>(
        Tcl_GetAssocData(interp, widget_id, nullptr));
    
    if (!tabs) {
        Tcl_AppendResult(interp, "Tab widget not found: ", widget_id, NULL);
        return TCL_ERROR;
    }
    
    const char* tab_name = Tcl_GetString(objv[2]);
    if (tabs->deleteCGTab(tab_name)) {
        return TCL_OK;
    }
    
    Tcl_SetResult(interp, const_cast<char*>("Tab not found"), TCL_STATIC);
    return TCL_ERROR;
}



// Helper to create namespace and commands
static void createNamespaceCommands(Tcl_Interp *interp)
{
    // Create the namespaces
    Tcl_Eval(interp, "namespace eval ::cg {}");
    Tcl_Eval(interp, "namespace eval ::cg::win {}");
    Tcl_Eval(interp, "namespace eval ::cg::man {}");
    Tcl_Eval(interp, "namespace eval ::cg::tab {}");
    
    // Window commands
    Tcl_Eval(interp, 
        "proc ::cg::win::setbg {window color} { "
        "    qtcgwin_setbackground $window $color "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::win::clear {window} { "
        "    qtcgwin_clear $window "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::win::refresh {window} { "
        "    qtcgwin_refresh $window "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::win::resize {window width height} { "
        "    qtcgwin_resize $window $width $height "
        "}");
    
    // Manager commands
    Tcl_Eval(interp, 
        "proc ::cg::man::list {} { "
        "    qtcg_list "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::man::select {window} { "
        "    qtcg_select $window "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::man::current {} { "
        "    qtcg_current "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::man::export {{window \"\"}} { "
        "    if {$window eq \"\"} { "
        "        qtcg_export_dialog "
        "    } else { "
        "        qtcg_select $window; "
        "        qtcg_export_dialog "
        "    } "
        "}");
    
    // Tab commands
    Tcl_Eval(interp, 
        "proc ::cg::tab::add {widget {label \"\"}} { "
        "    qtCgAddTab $widget $label "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::tab::select {widget tab} { "
        "    qtCgSelectTab $widget $tab "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::tab::delete {widget tab} { "
        "    qtCgDeleteTab $widget $tab "
        "}");
    
    // Convenience commands
    Tcl_Eval(interp, 
        "proc ::cglist {} { ::cg::man::list }");
    
    Tcl_Eval(interp, 
        "proc ::cgselect {window} { ::cg::man::select $window }");
    
    Tcl_Eval(interp, 
        "proc ::cgcurrent {} { ::cg::man::current }");
    
    Tcl_Eval(interp, 
        "proc ::cgbg {{color \"\"}} { "
        "    set current [::cg::man::current]; "
        "    if {$current ne \"\"} { "
        "        if {$color eq \"\"} { "
        "            error \"Getting background color not implemented\" "
        "        } else { "
        "            ::cg::win::setbg $current $color "
        "        } "
        "    } else { "
        "        error \"No current cgraph window\" "
        "    } "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cgclear {} { "
        "    set current [::cg::man::current]; "
        "    if {$current ne \"\"} { "
        "        ::cg::win::clear $current "
        "    } else { "
        "        error \"No current cgraph window\" "
        "    } "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cgrefresh {} { "
        "    set current [::cg::man::current]; "
        "    if {$current ne \"\"} { "
        "        ::cg::win::refresh $current "
        "    } else { "
        "        error \"No current cgraph window\" "
        "    } "
        "}");

    Tcl_Eval(interp, 
        "proc ::cg::win::setcolor {color} { "
        "    qtcgwin_setcolor $color "
        "}");
    
    Tcl_Eval(interp, 
        "proc ::cg::colorlist {} { "
        "    qtcgwin_colorlist "
        "}");

    // Override the global setcolor to support names
    Tcl_Eval(interp,
        "rename setcolor _original_setcolor; "
        "proc setcolor {color} { "
        "    qtcgwin_setcolor $color "
        "}");    

    Tcl_Eval(interp, 
        "proc ::cg::win::bind {window event script} { "
        "    qtcgwin_bind $window $event $script "
        "}");
    
    // Convenience binding for current window
    Tcl_Eval(interp, 
        "proc ::cgbind {event script} { "
        "    set current [::cg::man::current]; "
        "    if {$current ne \"\"} { "
        "        ::cg::win::bind $current $event $script "
        "    } else { "
        "        error \"No current cgraph window\" "
        "    } "
        "}");
    
    Tcl_Eval(interp,
	     "proc ::cg::help {} { "
	     "    return \"CGraph Qt Commands:\\n"
	     "  Namespaced commands:\\n"
	     "    cg::win::setbg window color  - Set background color\\n"
	     "    cg::win::clear window        - Clear window\\n"
	     "    cg::win::refresh window      - Refresh window\\n"
	     "    cg::win::resize window w h   - Resize window\\n"
	     "    cg::win::setcolor color      - Set drawing color (name or index)\\n"
	     "    cg::man::list                - List all windows\\n"
	     "    cg::man::select window       - Select window\\n"
	     "    cg::man::current             - Get current window\\n"
	     "    cg::man::export ?window?     - Export to PDF\\n"
	     "    cg::tab::add widget ?label?  - Add tab\\n"
	     "    cg::tab::select widget tab   - Select tab\\n"
	     "    cg::tab::delete widget tab   - Delete tab\\n"
	     "    cg::colorlist                - Get color dictionary\\n"
	     "\\n"
	     "  Convenience commands (operate on current window):\\n"
	     "    cglist                       - List windows\\n"
	     "    cgselect window              - Select window\\n"
	     "    cgcurrent                    - Get current window\\n"
	     "    cgbg ?color?                 - Set/get background\\n"
	     "    cgclear                      - Clear current window\\n"
	     "    cgrefresh                    - Refresh current window\\n"
	     "\\n"
	     "  Color names: black, blue, dark_green, cyan, red, magenta, brown,\\n"
	     "               white, gray, light_blue, green, light_cyan, deep_pink,\\n"
	     "               medium_purple, yellow, navy, bright_white, light_gray\" "
	     "}");   
}

// Extension initialization
extern "C" int Qtcgwin_Init(Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, "9.0", 0) == nullptr) {
        return TCL_ERROR;
    }

    if (Tcl_PkgProvide(interp, "qtcgwin", "1.0") != TCL_OK) {
        return TCL_ERROR;
    }
    
    // Set up the cgraph callbacks
    QtCGWinBridge::setupCallbacks();

    // Register core widget commands
    Tcl_CreateObjCommand(interp, "qtcgwin_init_widget",
                        (Tcl_ObjCmdProc *) qtcgwin_init_widget_cmd,
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcgwin_playback",
                        (Tcl_ObjCmdProc *) qtcgwin_playback_cmd,
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcgwin_resize",
                        (Tcl_ObjCmdProc *) qtcgwin_resize_cmd,
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcgwin_clear",
                        (Tcl_ObjCmdProc *) qtcgwin_clear_cmd,
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcgwin_setbackground",
                        (Tcl_ObjCmdProc *) qtcgwin_setbackground_cmd,
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcgwin_setcolor",
                        (Tcl_ObjCmdProc *) qtcgwin_setcolor_cmd,
                        (ClientData) NULL, NULL);    
    Tcl_CreateObjCommand(interp, "qtcgwin_colorlist",
			 (Tcl_ObjCmdProc *) qtcgwin_colorlist_cmd,
			 (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcgwin_set_current",
                        (Tcl_ObjCmdProc *) qtcgwin_set_current_cmd,
                        (ClientData) NULL, NULL);

    // Bind events
    Tcl_CreateObjCommand(interp, "qtcgwin_bind",
                        (Tcl_ObjCmdProc *) qtcgwin_bind_cmd,
                        (ClientData) NULL, NULL);
    
    // Register manager commands
    Tcl_CreateObjCommand(interp, "qtcg_list", 
                        (Tcl_ObjCmdProc *) qtcg_list_cmd, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcg_select",
                        (Tcl_ObjCmdProc *) qtcg_select_cmd, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcg_current",
                        (Tcl_ObjCmdProc *) qtcg_current_cmd, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtcg_export_dialog",
                        (Tcl_ObjCmdProc *) qtcg_export_dialog_cmd,
                        (ClientData) NULL, NULL);
    
    // Register tab widget commands
    Tcl_CreateObjCommand(interp, "qtCgAddTab", 
                        (Tcl_ObjCmdProc *) qtCgAddTab_cmd, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtCgSelectTab",
                        (Tcl_ObjCmdProc *) qtCgSelectTab_cmd, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtCgDeleteTab",
                        (Tcl_ObjCmdProc *) qtCgDeleteTab_cmd, 
                        (ClientData) NULL, NULL);
    
    // Override flushwin command
    Tcl_CreateObjCommand(interp, "flushwin",
                        (Tcl_ObjCmdProc *) cgwinFlushwinCmd,
                        (ClientData) NULL,
                        (Tcl_CmdDeleteProc *) NULL);

    // Create the namespace structure and convenience commands
    createNamespaceCommands(interp);

    return TCL_OK;
}
