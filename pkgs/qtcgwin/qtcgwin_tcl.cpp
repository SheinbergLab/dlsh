#include "qtcgwin.hpp"
#include <tcl.h>
#include <QApplication>
#include <QWidget>
#include <QLayout>  // Need full include, not just forward declaration

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
        QtCGWin* widget = QtCGTabManager::getInstance().getCurrentCGWin();
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

// Resize handler
static int qtcgwin_resize_cmd(ClientData data, Tcl_Interp *interp,
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
    
    // Make sure we're operating on the correct buffer
    if (widget && widget->getGraphicsBuffer()) {
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

// Clear graphics buffer
static int qtcgwin_clear_cmd(ClientData data, Tcl_Interp *interp,
                            int objc, Tcl_Obj *const objv[])
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "widget_ptr");
        return TCL_ERROR;
    }
    
    void* ptr = nullptr;
    if (Tcl_GetLongFromObj(interp, objv[1], (long*)&ptr) != TCL_OK) return TCL_ERROR;
    
    QtCGWin* widget = static_cast<QtCGWin*>(ptr);
    if (widget && widget->getGraphicsBuffer()) {
        GBUF_DATA* gbuf = static_cast<GBUF_DATA*>(widget->getGraphicsBuffer());
        gbSetGeventBuffer(gbuf);
        gbResetGevents();
    }
    
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

// Override the cgraph flushwin command
static int cgwinFlushwinCmd(ClientData data, Tcl_Interp *interp,
                           int objc, Tcl_Obj * const objv[])
{
    QtCGWin* currentWidget = QtCGTabManager::getInstance().getCurrentCGWin();
    if (currentWidget) {
        currentWidget->refresh();
    }
    return TCL_OK;
}

// Add a cgraph widget to a Qt container
static int add_cgraph_widget_func(ClientData data, Tcl_Interp *interp,
                                 int objc, Tcl_Obj *const objv[])
{
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "widget_ptr");
        return TCL_ERROR;
    }
    
    // Get the widget pointer (passed as a string from Qt)
    const char* ptr_str = Tcl_GetString(objv[1]);
    void* ptr = nullptr;
    if (sscanf(ptr_str, "%p", &ptr) != 1 || !ptr) {
        Tcl_AppendResult(interp, "Invalid widget pointer", NULL);
        return TCL_ERROR;
    }
    
    QWidget* parent = static_cast<QWidget*>(ptr);
    
    // Create a single cgraph widget
    auto cgwin = new QtCGWin(interp, parent);
    
    // If parent has a layout, add to it
    if (parent->layout()) {
        parent->layout()->addWidget(cgwin);
    }
    
    // Make it current
    QtCGTabManager::getInstance().setCurrentCGWin(cgwin);
    GBUF_DATA* gbuf = static_cast<GBUF_DATA*>(cgwin->getGraphicsBuffer());
    if (gbuf) {
        gbSetGeventBuffer(gbuf);
    }
    
    return TCL_OK;
}

// Add a cgraph tab widget
static int add_cgraph_tabs_func(ClientData data, Tcl_Interp *interp,
                               int objc, Tcl_Obj *const objv[])
{
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "widget_ptr");
        return TCL_ERROR;
    }
    
    // Get the widget pointer
    const char* ptr_str = Tcl_GetString(objv[1]);
    void* ptr = nullptr;
    if (sscanf(ptr_str, "%p", &ptr) != 1 || !ptr) {
        Tcl_AppendResult(interp, "Invalid widget pointer", NULL);
        return TCL_ERROR;
    }
    
    QWidget* parent = static_cast<QWidget*>(ptr);
    
    // Create tab widget
    auto tabs = new QtCGTabWidget(interp, parent);
    
    // If parent has a layout, add to it
    if (parent->layout()) {
        parent->layout()->addWidget(tabs);
    }
    
    // Store the tab widget pointer for later use
    QString widgetId = QString("qtcgtabs_%1").arg((quintptr)tabs);
    Tcl_SetAssocData(interp, widgetId.toUtf8().constData(), NULL, tabs);
    
    // Return the widget ID
    Tcl_SetObjResult(interp, Tcl_NewStringObj(widgetId.toUtf8().constData(), -1));
    
    return TCL_OK;
}

// Add a new tab
static int add_cgraph_tab_func(ClientData data, Tcl_Interp *interp,
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

// Select a tab
static int select_cgraph_tab_func(ClientData data, Tcl_Interp *interp,
                                 int objc, Tcl_Obj *const objv[])
{
    if (objc < 3) {
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

// Delete a tab
static int delete_cgraph_tab_func(ClientData data, Tcl_Interp *interp,
                                 int objc, Tcl_Obj *const objv[])
{
    if (objc < 3) {
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

// Get current widget pointer (for Qt integration)
static int get_current_cgwin_func(ClientData data, Tcl_Interp *interp,
                                 int objc, Tcl_Obj *const objv[])
{
    QtCGWin* current = QtCGTabManager::getInstance().getCurrentCGWin();
    if (current) {
        char ptr_str[32];
        snprintf(ptr_str, sizeof(ptr_str), "%p", current);
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ptr_str, -1));
        return TCL_OK;
    }
    
    Tcl_SetResult(interp, const_cast<char*>("No current cgraph widget"), TCL_STATIC);
    return TCL_ERROR;
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

    // Register commands
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
    Tcl_CreateObjCommand(interp, "qtcgwin_set_current",
                        (Tcl_ObjCmdProc *) qtcgwin_set_current_cmd,
                        (ClientData) NULL, NULL);
    
    Tcl_CreateObjCommand(interp, "qtCgAddWidget", 
                        (Tcl_ObjCmdProc *) add_cgraph_widget_func, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtCgAddTabs", 
                        (Tcl_ObjCmdProc *) add_cgraph_tabs_func, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtCgAddTab", 
                        (Tcl_ObjCmdProc *) add_cgraph_tab_func, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtCgSelectTab",
                        (Tcl_ObjCmdProc *) select_cgraph_tab_func, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtCgDeleteTab",
                        (Tcl_ObjCmdProc *) delete_cgraph_tab_func, 
                        (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp, "qtCgGetCurrent",
                        (Tcl_ObjCmdProc *) get_current_cgwin_func, 
                        (ClientData) NULL, NULL);
    
    // Override flushwin command
    Tcl_CreateObjCommand(interp, "flushwin",
                        (Tcl_ObjCmdProc *) cgwinFlushwinCmd,
                        (ClientData) NULL,
                        (Tcl_CmdDeleteProc *) NULL);
    
    return TCL_OK;
}