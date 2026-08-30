/*
 * dlref.c --
 *
 *   Reference-counted handles for dlsh dynamic lists.
 *
 * WHY
 *   Historically every dl_* command returned the *name* of a list ("%list7%")
 *   and the list's lifetime was pinned to a hidden Tcl variable of that same
 *   name, created in the frame that built it (see tclPutList).  When the frame
 *   exits the variable is unset, an unset trace fires, and the list is freed.
 *
 *   That means a plain
 *
 *       set x [dl_add $a $b]
 *
 *   copies only the *string* "%list7%" into x.  Ownership stays with the
 *   hidden variable, so the list dies at frame exit even though x still names
 *   it -- which is why dlsh grew dl_local/dl_return/dl_yield, all of which are
 *   really just "move that hidden claim onto a variable I control".
 *
 * WHAT THIS ADDS
 *   A Tcl_ObjType, "dynlist", whose internal rep points at a refcounted
 *   handle.  Tcl already counts references to Tcl_Objs, so storing a command
 *   result in a variable, a list, a dict, or returning it up a frame all keep
 *   the handle alive with no help from the script.
 *
 * OWNERSHIP RULE
 *   A list is freed when BOTH claims are gone:
 *
 *     objrefs == 0   no live Tcl_Obj refers to it, and
 *     framed  == 0   the frame trace / dlTable claim has been released.
 *
 *   This is deliberately additive.  Nothing that stays alive today dies any
 *   sooner: the frame trace still owns every list exactly as before, and the
 *   refcount only ever *extends* a lifetime.  What changes is that a list
 *   which escapes its frame -- returned, stashed in a global, lappend'd into
 *   an accumulator -- is kept alive by the reference that escaped instead of
 *   being freed out from under it.
 *
 *   Because the object's string rep is still the list's name, and a referenced
 *   list keeps its dlTable entry, every existing string-based command resolves
 *   it exactly as before.  That is what lets this land without converting the
 *   string-based dl_* commands to Obj commands.
 *
 * WHY THE HANDLE LIVES IN THE LIST
 *   The obvious implementation -- a hash table keyed by DYN_LIST * -- is
 *   unsafe here.  Lists are freed from ~500 call sites across the library, and
 *   any free that this layer does not intercept leaves a stale handle behind.
 *   The allocator then hands the same address to an unrelated new list, which
 *   silently inherits the dead handle's refcount.  Instead each DYN_LIST
 *   carries its own handle pointer (calloc'd to NULL, cleared on copy) and
 *   dfuFreeDynList calls one hook, so every free path detaches correctly no
 *   matter who calls it.
 *
 * NOT HANDLED HERE (deliberately, for now)
 *   Sublists and group columns.  tclFindDynList hands out interior pointers
 *   for "group:col" and "list:0"; those are owned by their parent and are
 *   never given handles -- dlRefSetObjResult is only called on top-level lists
 *   registered by tclPutList / dl_create.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tcl.h>
#include <df.h>
#include <dynio.h>
#include "dfana.h"
#include <labtcl.h>
#include "tcl_dl.h"
#include "dlref.h"

/* Must match the key tcl_dl.c registers DLSHINFO under. */
static const char *DLREF_DLSH_KEY = "dlsh";

typedef struct {
  DYN_LIST *dl;			/* the list, or NULL once detached     */
  DLSHINFO *dlinfo;		/* owning interp's dlTable             */
  Tcl_Interp *interp;		/* owning interp (for the frame re-arm)*/
  int objrefs;			/* live Tcl_Objs pointing here         */
  int framed;			/* frame trace / dlTable claim held    */
  int unwinding;		/* claim was released by a frame exit
				 * and the name has not been materialized
				 * since -- see dlRefDrop              */
  char name[DYN_LIST_NAME_SIZE];/* the list's name, kept when it is detached
				 * so a stale reference can still say which
				 * list it was                         */
} DL_REF;

static void dlRefFreeInternalRep(Tcl_Obj *objPtr);
static void dlRefDupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
static void dlRefUpdateString(Tcl_Obj *objPtr);
static int  dlRefSetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);

static const Tcl_ObjType dlRefObjType = {
  "dynlist",
  dlRefFreeInternalRep,
  dlRefDupInternalRep,
  dlRefUpdateString,
  dlRefSetFromAny,
  TCL_OBJTYPE_V0
};

/*****************************************************************************
 * Handles
 *****************************************************************************/

static DL_REF *dlRefFind(DYN_LIST *dl)
{
  return dl ? (DL_REF *) DYN_LIST_REFHANDLE(dl) : NULL;
}

/* Get dl's handle, creating it on first reference.  A new handle starts out
   framed: whatever put the list in dlTable -- a frame trace, or a name the
   user bound with dl_set -- still owns it. */

static DL_REF *dlRefGet(Tcl_Interp *interp, DYN_LIST *dl)
{
  DL_REF *ref;

  if (!dl) return NULL;
  if ((ref = (DL_REF *) DYN_LIST_REFHANDLE(dl))) return ref;

  ref = (DL_REF *) calloc(1, sizeof(DL_REF));
  if (!ref) return NULL;
  ref->dl = dl;
  ref->dlinfo = (DLSHINFO *) Tcl_GetAssocData(interp, DLREF_DLSH_KEY, NULL);
  ref->interp = interp;
  ref->objrefs = 0;
  ref->framed = 1;
  DYN_LIST_REFHANDLE(dl) = ref;
  return ref;
}

/* Detach a handle from its list without freeing the list.  After this the
   handle is inert: stale objects report a missing list. */

static void dlRefDetach(DL_REF *ref)
{
  if (!ref) return;
  if (ref->dl) {
    /* Keep the name: objects still holding this handle may not have
       generated their string rep yet, and "%list7%" makes a far better
       error than "". */
    strncpy(ref->name, DYN_LIST_NAME(ref->dl), DYN_LIST_NAME_SIZE - 1);
    ref->name[DYN_LIST_NAME_SIZE - 1] = 0;
    DYN_LIST_REFHANDLE(ref->dl) = NULL;
  }
  ref->dl = NULL;
  ref->dlinfo = NULL;
}

/* Free the list a handle owns, and its dlTable entry. */

static void dlRefDestroy(DL_REF *ref)
{
  DYN_LIST *dl = ref->dl;
  Tcl_HashEntry *entryPtr;

  if (dl && ref->dlinfo &&
      (entryPtr = Tcl_FindHashEntry(&ref->dlinfo->dlTable,
				    DYN_LIST_NAME(dl))) &&
      Tcl_GetHashValue(entryPtr) == (void *) dl) {
    Tcl_DeleteHashEntry(entryPtr);
  }
  dlRefDetach(ref);		/* clears refhandle: the free hook won't fire */
  if (dl) dfuFreeDynList(dl);
  free(ref);
}

/* Release one object reference.
 *
 * Losing the last reference does NOT mean the list is unreachable.  Its name
 * may still be held as a plain string -- which is exactly what shimmering
 * produces: [llength $x] makes Tcl generate the name into the string rep and
 * then discard our internal rep, so the reference vanishes while the variable
 * still spells the list.  Freeing here would strand that name.
 *
 * So instead of destroying an unreferenced-but-still-registered list, hand it
 * back to the current frame as an ordinary temp.  It then has precisely the
 * lifetime it would have had if it were created there, which is the same
 * lifetime dlsh temps have always had, and the name stays resolvable for the
 * rest of the frame.
 *
 * The exception is a drop that is itself part of a frame unwinding, which
 * must NOT be handed upward or every `set x [dl_...]` would promote a list
 * into its caller on return.  Tcl_PopCallFrame deletes a frame's hash
 * variables -- our hidden %listN% claim -- before its compiled locals -- the
 * user's x -- so during teardown the claim is always released first and every
 * drop would otherwise look like an escape.  That fixed ordering is also the
 * signal: dlRefFrameRelease marks the handle `unwinding`, and the mark is
 * cleared as soon as the name is materialized again (dlRefUpdateString),
 * which can only happen once some live frame is using it.  A drop that is
 * still marked is part of the teardown, and is freed outright. */

static void dlRefDrop(DL_REF *ref)
{
  if (!ref) return;
  if (--ref->objrefs > 0) return;
  if (ref->framed && ref->dl) return;	/* frame/dlTable still owns it */
  if (!ref->dl) { free(ref); return; }	/* already detached: just reclaim */

  if (!ref->unwinding && ref->interp &&
      dlReclaimInFrame(ref->interp, ref->dl)) {
    ref->framed = 1;
    return;
  }
  dlRefDestroy(ref);
}

/* dfuFreeDynList hook: the list is going away by some path outside this
   layer's control (dl_set replacing a target, a group absorbing and later
   freeing a column, interp teardown).  Detach so nothing double-frees and
   stale objects fail cleanly. */

static void dlRefListFreed(DYN_LIST *dl)
{
  DL_REF *ref = dlRefFind(dl);

  if (!ref) return;
  dlRefDetach(ref);
  if (ref->objrefs <= 0) free(ref);
}

/*****************************************************************************
 * Tcl_ObjType procs
 *****************************************************************************/

static DL_REF *dlRefFromObj(Tcl_Obj *objPtr)
{
  Tcl_ObjInternalRep *irPtr = Tcl_FetchInternalRep(objPtr, &dlRefObjType);
  return irPtr ? (DL_REF *) irPtr->twoPtrValue.ptr1 : NULL;
}

static void dlRefFreeInternalRep(Tcl_Obj *objPtr)
{
  dlRefDrop(dlRefFromObj(objPtr));
}

/* Sharing an internal rep is the whole point: both objects name the same
   list, so the count goes up and the data is untouched.  (The 1998 attempt in
   dlobj.c shared the pointer with no count, which is why the survivor was
   left holding freed memory.) */

static void dlRefDupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
  Tcl_ObjInternalRep *irPtr = Tcl_FetchInternalRep(srcPtr, &dlRefObjType);
  DL_REF *ref;

  if (!irPtr) return;
  ref = (DL_REF *) irPtr->twoPtrValue.ptr1;
  if (ref) ref->objrefs++;
  Tcl_StoreInternalRep(dupPtr, &dlRefObjType, irPtr);
}

/* Materializing the name is the moment a bare string of it can start
   existing, and it only happens while a live frame is running -- Tcl always
   generates the string rep before discarding an internal rep, which is what
   makes this the reliable hook for shimmering.  Clearing `unwinding` here
   tells dlRefDrop that a later drop belongs to this frame rather than to a
   teardown, so the list is handed to it instead of freed. */

static void dlRefUpdateString(Tcl_Obj *objPtr)
{
  DL_REF *ref = dlRefFromObj(objPtr);
  const char *name = !ref ? ""
                   : ref->dl ? DYN_LIST_NAME(ref->dl)
                   : ref->name;		/* detached: the name it used to have */
  size_t n = strlen(name);

  objPtr->bytes = (char *) Tcl_Alloc(n + 1);
  memcpy(objPtr->bytes, name, n + 1);
  objPtr->length = (Tcl_Size) n;

  if (ref) ref->unwinding = 0;
}

/* Resurrect a handle from a name.  This is what makes the type safe against
   shimmering: an object converted to another type (by [lindex], [expr],
   [string ...]) drops its internal rep, but the name survives in the string
   rep and the list is still in dlTable, so converting back finds it.  Only
   exact dlTable names convert -- notably NOT the "{1 2 3}" Tcl-list coercion
   tclFindDynList does, which would manufacture a list here. */

static int dlRefSetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
  DLSHINFO *dlinfo;
  Tcl_HashEntry *entryPtr;
  Tcl_ObjInternalRep ir;
  DYN_LIST *dl;
  DL_REF *ref;

  if (Tcl_FetchInternalRep(objPtr, &dlRefObjType)) return TCL_OK;
  if (!interp) return TCL_ERROR;

  dlinfo = (DLSHINFO *) Tcl_GetAssocData(interp, DLREF_DLSH_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;

  entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, Tcl_GetString(objPtr));
  if (!entryPtr || !(dl = (DYN_LIST *) Tcl_GetHashValue(entryPtr))) {
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("no such dynlist \"%s\"",
					   Tcl_GetString(objPtr)));
    return TCL_ERROR;
  }

  if (!(ref = dlRefGet(interp, dl))) return TCL_ERROR;
  ref->objrefs++;
  ir.twoPtrValue.ptr1 = ref;
  ir.twoPtrValue.ptr2 = NULL;
  Tcl_StoreInternalRep(objPtr, &dlRefObjType, &ir);
  return TCL_OK;
}

/*****************************************************************************
 * Public entry points
 *****************************************************************************/

void dlRefSetObjResult(Tcl_Interp *interp, DYN_LIST *dl)
{
  Tcl_ObjInternalRep ir;
  Tcl_Obj *objPtr;
  DL_REF *ref;

  if (!dl) return;

  if (!(ref = dlRefGet(interp, dl))) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj(DYN_LIST_NAME(dl), -1));
    return;
  }

  /* Deliberately handed back with no string rep: dlRefUpdateString generates
     the name on demand, and that call is what tells us the name has escaped
     into a live frame (see dlRefDrop).  Pre-filling the bytes here would mean
     updateString never runs and the signal would be lost.  Anything that
     wants the name still gets exactly "%listN%", just a moment later. */
  objPtr = Tcl_NewObj();
  Tcl_InvalidateStringRep(objPtr);

  ref->objrefs++;
  ir.twoPtrValue.ptr1 = ref;
  ir.twoPtrValue.ptr2 = NULL;
  Tcl_StoreInternalRep(objPtr, &dlRefObjType, &ir);

  Tcl_SetObjResult(interp, objPtr);
}

int dlRefFrameRelease(Tcl_Interp *interp, DYN_LIST *dl)
{
  DL_REF *ref = dlRefFind(dl);

  if (!ref) return 1;			/* untracked: free as always */

  ref->framed = 0;
  if (ref->objrefs > 0) {
    /* Held by something outside this frame's claim.  Whether that is a real
       escape or just a local about to be deleted two lines further into
       Tcl_PopCallFrame is not knowable yet, so mark it and let dlRefDrop
       decide: a drop that arrives before the name is materialized again is
       part of this same teardown. */
    ref->unwinding = 1;
    return 0;
  }

  /* No references: hand the list back to the caller to free exactly as
     before, taking the now-empty handle with us. */
  dlRefDetach(ref);
  free(ref);
  return 1;
}

void dlRefNoteUse(DYN_LIST *dl)
{
  DL_REF *ref = dlRefFind(dl);
  if (ref) ref->unwinding = 0;
}

int dlRefCount(Tcl_Interp *interp, DYN_LIST *dl)
{
  DL_REF *ref = dlRefFind(dl);
  return ref ? ref->objrefs : 0;
}

void dlRefInvalidate(Tcl_Interp *interp, DYN_LIST *dl)
{
  dlRefListFreed(dl);
}

/*****************************************************************************
 * Setup
 *****************************************************************************/

/* Called from Dl_Init, so once per interp -- and dserv runs a pool of them,
   one per thread.  Both steps are deliberately idempotent rather than
   guarded by a flag: Tcl_RegisterObjType takes Tcl's own table mutex, and the
   hook store writes the same function pointer every time, so repeat calls
   from several threads race on nothing. */

int dlRefInit(Tcl_Interp *interp)
{
  Tcl_RegisterObjType(&dlRefObjType);
  dfuDynListFreeHook = dlRefListFreed;
  return TCL_OK;
}
