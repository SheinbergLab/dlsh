/*
 * dlref.h --
 *
 *   Reference-counted handles for dlsh dynamic lists.  See dlref.c for the
 *   ownership rules.
 */

#ifndef __dlref_h__
#define __dlref_h__

#ifdef __cplusplus
extern "C" {
#endif

/* Register the "dynlist" Tcl_ObjType and this interp's handle table.  Called
   once from Dl_Init. */
int dlRefInit(Tcl_Interp *interp);

/* Set the interp result to a "dynlist" object referring to dl.  dl must
   already be registered in dlinfo->dlTable under its current name; the
   object's string rep is that name, so string-based commands see exactly
   what they saw before this layer existed. */
void dlRefSetObjResult(Tcl_Interp *interp, DYN_LIST *dl);

/* The frame trace that owned dl is firing.  Returns 1 if the caller should
   free the list and drop its dlTable entry as it always has, or 0 if live
   object references remain and the list must be left in place. */
int dlRefFrameRelease(Tcl_Interp *interp, DYN_LIST *dl);

/* dl is about to be freed by a path this layer does not control (dl_delete,
   dl_set replacing a list, interp teardown).  Detaches any outstanding
   handles so that stale objects report a missing list rather than reading
   freed memory. */
void dlRefInvalidate(Tcl_Interp *interp, DYN_LIST *dl);

/* Number of live object references to dl (0 if none / not tracked).  Used by
   dl_clean and friends to leave referenced lists alone. */
int dlRefCount(Tcl_Interp *interp, DYN_LIST *dl);

#ifdef __cplusplus
}
#endif

#endif /* __dlref_h__ */
