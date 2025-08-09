/*
 * cg_base_wrapper.h - Helper macros for ensuring interpreter context
 * 
 * This header provides wrapper macros that ensure the correct interpreter
 * context is set before calling cgraph functions from Tcl commands.
 */

#ifndef CG_BASE_WRAPPER_H
#define CG_BASE_WRAPPER_H

/* Declare the initialization function from cgraph.c */
extern void Cgraph_InitInterp(Tcl_Interp *interp);

/* 
 * Wrapper macro for Tcl command procedures that use cgraph
 * This ensures the interpreter context is set before executing the command
 */
#define CG_CMD_WRAPPER(name) \
static int name##_wrapper(ClientData clientData, Tcl_Interp *interp, \
                         int argc, char *argv[]) \
{ \
    Cgraph_InitInterp(interp); \
    return name(clientData, interp, argc, argv); \
}

/* 
 * Alternative: Define both the wrapper declaration and implementation
 * Use this if you need to declare wrappers before the original functions
 */
#define CG_CMD_WRAPPER_DECL(name) \
static int name##_wrapper(ClientData clientData, Tcl_Interp *interp, \
                         int argc, char *argv[]);

#define CG_CMD_WRAPPER_IMPL(name) \
static int name##_wrapper(ClientData clientData, Tcl_Interp *interp, \
                         int argc, char *argv[]) \
{ \
    Cgraph_InitInterp(interp); \
    return name(clientData, interp, argc, argv); \
}

/* 
 * Macro to create a wrapped command
 * Use this instead of Tcl_CreateCommand for cgraph commands
 */
#define Tcl_CreateCgCommand(interp, cmdName, proc, clientData, deleteProc) \
    Tcl_CreateCommand(interp, cmdName, (Tcl_CmdProc *) proc##_wrapper, \
                     clientData, deleteProc)

#endif /* CG_BASE_WRAPPER_H */
