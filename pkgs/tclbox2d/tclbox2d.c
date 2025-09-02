/*
 * tclbox2d.c
 *  Use the Box2D library to simulate 2D physics
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#define DllEntryPoint DllMain
#define EXPORT(a,b) __declspec(dllexport) a b
#undef WIN32_LEAN_AND_MEAN
#else
#include <math.h>
#endif
#include <tcl.h>
#include "box2d/box2d.h"

/* limit the number of shapes inside a body */
#define MAX_SHAPES_PER_BODY 16

/*
enum b2BodyType
 {
     b2_staticBody = 0,
     b2_kinematicBody,
     b2_dynamicBody
 };
*/

Tcl_Mutex Box2D_Mutex;		/* to handle multi threads */
struct Box2D_world;		/* for circular reference */

typedef struct Box2D_world {
  char name[32];
  Tcl_Interp *interp;
  b2WorldId worldId;
  b2Vec2 gravity;

  /* store contact events after a simulation step */
  b2ContactEvents contactEvents;
  
  int bodyCount;
  Tcl_HashTable bodyTable;

  int jointCount;
  Tcl_HashTable jointTable;

  int subStepCount;
  
  int time;
  int lasttime;
} BOX2D_WORLD;

typedef struct Box2D_userdata {
  BOX2D_WORLD *world;
  char name[32];
} BOX2D_USERDATA;

typedef struct Box2D_info {
  Tcl_HashTable Box2DTable;
  int Box2DCount;
} BOX2D_INFO;

#define BOX2D_ASSOC_DATA_KEY "b2dinfo"

static int find_Box2D(Tcl_Interp *interp, char *name, BOX2D_WORLD **b2dw)
{
  BOX2D_INFO *b2info =
    (BOX2D_INFO *) Tcl_GetAssocData(interp,
				    BOX2D_ASSOC_DATA_KEY, NULL);
  BOX2D_WORLD *world;
  Tcl_HashEntry *entryPtr;

  if ((entryPtr = Tcl_FindHashEntry(&b2info->Box2DTable, name))) {
    world = (BOX2D_WORLD *) Tcl_GetHashValue(entryPtr);
    if (!world) {
      Tcl_AppendResult(interp, "bad Box2D world ptr in hash table", NULL);
      return TCL_ERROR;
    }
    if (b2dw) *b2dw = world;
    return TCL_OK;
  }
  else {
    if (b2dw) {
      Tcl_AppendResult(interp, "world \"", name, "\" not found", 
		       (char *) NULL);
    }
    return TCL_ERROR;
  }
}

static int find_body(BOX2D_WORLD *bw, char *name, b2BodyId *b)
{
  b2BodyId *body;
  Tcl_HashEntry *entryPtr;
  
  if ((entryPtr = Tcl_FindHashEntry(&bw->bodyTable, name))) {
    body = (b2BodyId *) Tcl_GetHashValue(entryPtr);
    if (!body) {
      Tcl_AppendResult(bw->interp,
		       "bad body ptr in hash table", NULL);
      return TCL_ERROR;
    }
    if (b) *b = *body;
    return TCL_OK;
  }
  else {
    if (b) {
      Tcl_AppendResult(bw->interp, "body \"", name, "\" not found", 
		       (char *) NULL);
    }
    return TCL_ERROR;
  }
}


static int find_joint(BOX2D_WORLD *bw, char *name, b2JointId *j)
{
  b2JointId *joint;
  Tcl_HashEntry *entryPtr;
  
  if ((entryPtr = Tcl_FindHashEntry(&bw->jointTable, name))) {
    joint = (b2JointId *) Tcl_GetHashValue(entryPtr);
    if (!joint) {
      Tcl_AppendResult(bw->interp,
		       "bad joint ptr in hash table", NULL);
      return TCL_ERROR;
    }
    if (j) *j = *joint;
    return TCL_OK;
  }
  else {
    if (j) {
      Tcl_AppendResult(bw->interp, "joint \"", name, "\" not found", 
		       (char *) NULL);
    }
    return TCL_ERROR;
  }
}



static int find_revolute_joint(BOX2D_WORLD *bw, char *name, b2JointId *j)
{
  if (find_joint(bw, name, j) != TCL_OK) return TCL_ERROR;
  if (b2Joint_GetType(*j) != b2_revoluteJoint) {
    Tcl_AppendResult(bw->interp,
		     "joint ", name, " not a revolute joint", NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}




/***********************************************************************/
/***********************      Box2D OBJ Funcs     **********************/
/***********************************************************************/


static void Box2DDelete(BOX2D_WORLD *bw) 
{
  b2BodyId *body;
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch search;

  /* iterate over the table of bodies and free userdata */
  for (entryPtr = Tcl_FirstHashEntry(&bw->bodyTable, &search);
       entryPtr != NULL;
       entryPtr = Tcl_NextHashEntry(&search)) {
    body = (b2BodyId *) Tcl_GetHashValue(entryPtr);
    if (body && b2Body_IsValid(*body)) {  // Add validity check
      void *userdata = b2Body_GetUserData(*body);
      if (userdata) {  // Check for NULL
	free(userdata);
      }
    }    
  }

  /* free hash table */
  Tcl_DeleteHashTable(&bw->bodyTable);

  Tcl_MutexLock(&Box2D_Mutex);
  b2DestroyWorld(bw->worldId);
  Tcl_MutexUnlock(&Box2D_Mutex);
  
  free((void *) bw);
}



static int Box2DGetBodiesCmd (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  Tcl_HashEntry *entryPtr;
  Tcl_Obj *bodylist;
  Tcl_HashSearch search;
  int typemask = 0x7;		// all three types
  b2BodyId* body;

  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " world [typemask]", (char *) NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;

  if (argc > 2) {
    if (Tcl_GetInt(interp, argv[2], &typemask) != TCL_OK) return TCL_ERROR;
  }
  
  bodylist = Tcl_NewObj();
  
  for (entryPtr = Tcl_FirstHashEntry(&bw->bodyTable, &search);
       entryPtr != NULL;
       entryPtr = Tcl_NextHashEntry(&search)) {
    body = (b2BodyId *) Tcl_GetHashValue(entryPtr);
    if ((1 << (int) b2Body_GetType(*body)) & typemask) {
      Tcl_ListObjAppendElement(interp, bodylist,
			       Tcl_NewStringObj((char *) Tcl_GetHashKey(&bw->bodyTable, entryPtr), -1));
    }
  }
  Tcl_SetObjResult(interp, bodylist);
  return TCL_OK;
}

static int Box2DDestroyCmd (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  Tcl_HashEntry *entryPtr;
  int i;

  BOX2D_INFO *b2info = (BOX2D_INFO *) data;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world(s)", (char *) NULL);
    return TCL_ERROR;
  }
  
  for (i = 1; i < argc; i++) {
    if ((entryPtr = Tcl_FindHashEntry(&b2info->Box2DTable, argv[i]))) {
      if ((bw = (BOX2D_WORLD *) Tcl_GetHashValue(entryPtr))) 
	Box2DDelete(bw);
      Tcl_DeleteHashEntry(entryPtr);
    }
  
    /* 
     * If user has specified "delete ALL" iterate through the hash table
     * and recursively call this function to close all open world
     */
#ifdef _WIN32
    else if (!_stricmp(argv[i],"ALL"))
#else
    else if (!strcasecmp(argv[i],"ALL"))
#endif
      {
	Tcl_HashSearch search;
	for (entryPtr = Tcl_FirstHashEntry(&b2info->Box2DTable, &search);
	     entryPtr != NULL;
	   entryPtr = Tcl_NextHashEntry(&search)) {
	  Tcl_VarEval(interp, argv[0], " ",
		      Tcl_GetHashKey(&b2info->Box2DTable, entryPtr),
		      (char *) NULL);
	}
	b2info->Box2DCount = 0;
      }
    else {
      Tcl_AppendResult(interp, argv[0], ": world \"", argv[i], 
		       "\" not found", (char *) NULL);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

static int Box2DAddWorld(Tcl_Interp *interp,
			 BOX2D_WORLD *newworld,
			 char *worldname)
{
  BOX2D_INFO *b2info = (BOX2D_INFO *) Tcl_GetAssocData(interp,
					   BOX2D_ASSOC_DATA_KEY, NULL);
  Tcl_HashEntry *entryPtr;
  int newentry;
  strncpy(newworld->name, worldname, sizeof(newworld->name)-1); 
  entryPtr = Tcl_CreateHashEntry(&b2info->Box2DTable, worldname, &newentry);
  Tcl_SetHashValue(entryPtr, newworld);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(worldname, -1));
  return TCL_OK;
}

static int Box2DCreateWorldCmd(ClientData clientData, Tcl_Interp *interp,
			       int objc, Tcl_Obj * const objv[])
{
  char worldname[128];
  BOX2D_WORLD *bw;
  BOX2D_INFO *b2info = (BOX2D_INFO *) clientData;
  
  bw = (BOX2D_WORLD *) calloc(1, sizeof(BOX2D_WORLD));

  bw->gravity.x = 0.0f;
  bw->gravity.y = -10.0f;

  /* Reasonable simulation settings */
  bw->subStepCount = 4;

  b2Vec2 gravity = {bw->gravity.x, bw->gravity.y};
  b2WorldDef worldDef = b2DefaultWorldDef();
  worldDef.gravity = gravity;

  /* b2CreateWorld() is not thread safe! */
  Tcl_MutexLock(&Box2D_Mutex);
  b2WorldId worldId = b2CreateWorld(&worldDef);
  snprintf(worldname, sizeof(worldname), "box2d%d", b2info->Box2DCount++);
  Tcl_MutexUnlock(&Box2D_Mutex);
  
  bw->worldId = worldId;

  bw->interp = interp;

  Tcl_InitHashTable(&bw->bodyTable, TCL_STRING_KEYS);
  Tcl_InitHashTable(&bw->jointTable, TCL_STRING_KEYS);
  
  return Box2DAddWorld(interp, bw, worldname);
}

static int Box2DStepCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  double elapsed;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world elapsed", NULL);
    return TCL_ERROR;
  }
  
  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &elapsed) != TCL_OK) return TCL_ERROR;

  bw->lasttime = bw->time;
  bw->time += elapsed;

  b2World_Step(bw->worldId, elapsed, bw->subStepCount);
  bw->contactEvents = b2World_GetContactEvents(bw->worldId);
  
  return(TCL_OK);
}

static int Box2DGetContactBeginEventCountCmd(ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[])
{
  BOX2D_WORLD *bw;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world", NULL);
    return TCL_ERROR;
  }
  
  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;
  Tcl_SetObjResult(interp, Tcl_NewIntObj(bw->contactEvents.beginCount));
  return(TCL_OK);
}


static int Box2DGetContactEndEventCountCmd(ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[])
{
  BOX2D_WORLD *bw;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world", NULL);
    return TCL_ERROR;
  }
  
  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;
  Tcl_SetObjResult(interp, Tcl_NewIntObj(bw->contactEvents.endCount));
  return(TCL_OK);
}

static int Box2DGetContactBeginEventsCmd(ClientData clientData, Tcl_Interp *interp,
				    int argc, char *argv[])
{
  BOX2D_WORLD *bw;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world", NULL);
    return TCL_ERROR;
  }
  
  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;

  // store lists of begin and end touches
  Tcl_Obj *events = Tcl_NewObj();

  for (int i = 0; i < bw->contactEvents.beginCount; ++i)
    {
      b2ContactBeginTouchEvent* beginEvent = bw->contactEvents.beginEvents + i;
      char *bodyName1 = 
  ((BOX2D_USERDATA *) b2Body_GetUserData(b2Shape_GetBody(beginEvent->shapeIdA)))->name;
      char *bodyName2 = 
      ((BOX2D_USERDATA *) b2Body_GetUserData(b2Shape_GetBody(beginEvent->shapeIdB)))->name;
      Tcl_Obj *bodies = Tcl_NewObj();
      Tcl_ListObjAppendElement(interp, bodies, Tcl_NewStringObj(bodyName1, -1));
      Tcl_ListObjAppendElement(interp, bodies, Tcl_NewStringObj(bodyName2, -1));
      Tcl_ListObjAppendElement(interp, events, bodies);
    }
  Tcl_SetObjResult(interp, events);
  return(TCL_OK);
}


static int Box2DGetContactEndEventsCmd(ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[])
{
  BOX2D_WORLD *bw;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world", NULL);
    return TCL_ERROR;
  }
  
  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;

  // store lists of begin and end touches
  Tcl_Obj *events = Tcl_NewObj();

  Tcl_Obj *ends = Tcl_NewObj();
  for (int i = 0; i < bw->contactEvents.endCount; ++i)
    {
      b2ContactEndTouchEvent* endEvent = bw->contactEvents.endEvents + i;
      char *bodyName1 = 
  ((BOX2D_USERDATA *) b2Body_GetUserData(b2Shape_GetBody(endEvent->shapeIdA)))->name;
      char *bodyName2 = 
  ((BOX2D_USERDATA *) b2Body_GetUserData(b2Shape_GetBody(endEvent->shapeIdB)))->name;

      Tcl_Obj *bodies = Tcl_NewObj();
      Tcl_ListObjAppendElement(interp, bodies, Tcl_NewStringObj(bodyName1, -1));
      Tcl_ListObjAppendElement(interp, bodies, Tcl_NewStringObj(bodyName2, -1));
      Tcl_ListObjAppendElement(interp, events, bodies);
    }

  Tcl_SetObjResult(interp, events);
  return(TCL_OK);
}


/***********************************************************************/
/**********************      Tcl Bound Funcs     ***********************/
/***********************************************************************/

static int Box2DCreateBoxCmd(ClientData clientData, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  BOX2D_USERDATA *userdata;
  Tcl_HashEntry *entryPtr;
  int newentry;
  char *name = NULL;
  double x, y, width, height, angle;
  int bodyType;
  int enableContact = true;
  int enableHits = false;
  
  if (argc < 9) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " world name type x y w h angle", NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;
  name = argv[2];
  if (Tcl_GetInt(interp, argv[3], &bodyType) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[5], &y) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[6], &width) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[7], &height) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[8], &angle) != TCL_OK) return TCL_ERROR;

  
  userdata = (BOX2D_USERDATA *) calloc(1, sizeof(BOX2D_USERDATA));
  userdata->world = bw;

  b2BodyDef bodyDef = b2DefaultBodyDef();
  bodyDef.type = (b2BodyType) bodyType;
  bodyDef.position = (b2Vec2){x, y};
  bodyDef.rotation = b2MakeRot(angle);
  bodyDef.angularDamping = .05;
  bodyDef.linearDamping = .05;
  
  b2BodyId bodyId = b2CreateBody(bw->worldId, &bodyDef);
  b2Body_SetUserData (bodyId, userdata);

  /* create the box shape for this body */
  b2Polygon box = b2MakeBox(width/2., height/2.);
  b2ShapeDef shapeDef = b2DefaultShapeDef();
  shapeDef.density = 1.0f;

  shapeDef.enableContactEvents = enableContact;
  shapeDef.enableHitEvents = enableHits;
    
  b2CreatePolygonShape(bodyId, &shapeDef, &box);  

  if (!name || !strlen(name)) {
    snprintf(userdata->name, sizeof(userdata->name),
	     "body%d", bw->bodyCount++); 
  }
  else {
    strncpy(userdata->name, name, sizeof(userdata->name));
  }
  
  entryPtr = Tcl_CreateHashEntry(&bw->bodyTable, userdata->name, &newentry);

  /* the body is an opaque structure, so to hash create copy and add to table */
  b2BodyId *body = (b2BodyId *) malloc(sizeof(b2BodyId));
  *body = bodyId;
  Tcl_SetHashValue(entryPtr, body);

  Tcl_SetObjResult(interp, Tcl_NewStringObj(userdata->name, -1));
  return(TCL_OK);
}

static int Box2DCreateCircleCmd(ClientData clientData, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  BOX2D_USERDATA *userdata;
  Tcl_HashEntry *entryPtr;
  int newentry;
  char *name = NULL;
  double x, y, r, angle = 0.0;
  int bodyType;
  int enableContact = true;
  int enableHits = false;
  
  if (argc < 7) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " world name type x y r", NULL);
    return TCL_ERROR;
  }
  
  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;
  name = argv[2];
  if (Tcl_GetInt(interp, argv[3], &bodyType) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[5], &y) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[6], &r) != TCL_OK) return TCL_ERROR;

  userdata = (BOX2D_USERDATA *) calloc(1, sizeof(BOX2D_USERDATA));
  userdata->world = bw;
  
  b2BodyDef bodyDef = b2DefaultBodyDef();
  bodyDef.type = (b2BodyType) bodyType;
  bodyDef.position = (b2Vec2){x, y};
  bodyDef.angularDamping = .05;
  bodyDef.linearDamping = .05;
  
  b2BodyId bodyId = b2CreateBody(bw->worldId, &bodyDef);
  b2Body_SetUserData (bodyId, userdata);

  /* create the box shape for this body */
  b2Circle circle;
  circle.center = (b2Vec2){0,0};
  circle.radius = r;
  
  b2ShapeDef shapeDef = b2DefaultShapeDef();
  shapeDef.density = 1.0f;

  shapeDef.enableContactEvents = enableContact;
  shapeDef.enableHitEvents = enableHits;
  
  b2CreateCircleShape(bodyId, &shapeDef, &circle);  

  if (!name || !strlen(name)) {
    snprintf(userdata->name, sizeof(userdata->name), "body%d", bw->bodyCount++); 
  }
  else {
    strncpy(userdata->name, name, sizeof(userdata->name));
  }

  entryPtr = Tcl_CreateHashEntry(&bw->bodyTable, userdata->name, &newentry);

  /* the body is an opaque structure, so to hash create copy and add to table */
  b2BodyId *body = (b2BodyId *) malloc(sizeof(b2BodyId));
  *body = bodyId;
  Tcl_SetHashValue(entryPtr, body);

  Tcl_SetObjResult(interp, Tcl_NewStringObj(userdata->name, -1));
  return(TCL_OK);
}

static int Box2DSetBodyTypeCmd(ClientData clientData, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  b2BodyId body;
  int body_type;

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world body type",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;

  if (find_body(bw, argv[2], &body) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetInt(interp, argv[3], &body_type) != TCL_OK) return TCL_ERROR;
  if (body_type < 0 || body_type > 2) {
    Tcl_AppendResult(interp, argv[0], ": invalid body type", NULL);
    return TCL_ERROR;
  }

  b2Body_SetType(body, body_type);
  
  return TCL_OK;
}


static int Box2DGetBodyInfoCmd(ClientData clientData, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  float angle;
  b2Vec2 position;
  b2Rot rotation;
  BOX2D_WORLD *bw;
  b2BodyId body;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world body",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;
  if (find_body(bw, argv[2], &body) != TCL_OK) return TCL_ERROR;

  position = b2Body_GetPosition(body);
  angle = b2Rot_GetAngle(b2Body_GetRotation(body));

  Tcl_Obj *obj = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(position.x));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(position.y));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(angle));
  Tcl_SetObjResult(interp, obj);

  return TCL_OK;
}

static int Box2DSetTransformCmd(ClientData clientData, Tcl_Interp *interp,
				int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  b2BodyId body;
  int body_type;
  double x, y, angle = 0;
  
  if (argc < 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world body x y [angle=0]",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;

  if (find_body(bw, argv[2], &body) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetDouble(interp, argv[3], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y) != TCL_OK) return TCL_ERROR;
  if (argc > 5) {
    if (Tcl_GetDouble(interp, argv[5], &angle) != TCL_OK) return TCL_ERROR;
  }

  b2Body_SetTransform(body, (b2Vec2){x, y}, b2MakeRot(angle));
  
  return TCL_OK;
}

static int Box2DSetLinearVelocityCmd(ClientData clientData, Tcl_Interp *interp,
                                     int argc, char *argv[])
{
  BOX2D_WORLD *bw;
  b2BodyId body;
  double vx, vy;
  
  if (argc != 5)
  {
    Tcl_AppendResult(interp, "usage: ", argv[0], " world body vx vy", NULL);
    return TCL_ERROR;
  }
  

  if (find_Box2D(interp, argv[1], &bw) != TCL_OK) return TCL_ERROR;

  if (find_body(bw, argv[2], &body) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetDouble(interp, argv[3], &vx) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &vy) != TCL_OK) return TCL_ERROR;
  
  b2Body_SetLinearVelocity(body, (b2Vec2){vx, vy});
  return TCL_OK;
}

/***********************************************************************/
/**********************      Shape Settings       **********************/
/***********************************************************************/


static int Box2DSetRestitutionCmd(ClientData clientData, Tcl_Interp *interp,
				  int objc, Tcl_Obj * const objv[])
{
  BOX2D_WORLD *bw;
  b2BodyId body;
  b2ShapeId shapes[MAX_SHAPES_PER_BODY];
  double restitution;

  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]),
		     " world body restitution", NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_body(bw, Tcl_GetString(objv[2]), &body) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp, objv[3], &restitution) != TCL_OK) return TCL_ERROR;

  int nshapes = b2Body_GetShapes(body, shapes, MAX_SHAPES_PER_BODY);
  for (int i = 0; i < nshapes; i++) {
    b2Shape_SetRestitution(shapes[i], restitution);
  }
  return(TCL_OK);
}

/***********************************************************************/
/**********************          Joints           **********************/
/***********************************************************************/


static int Box2DRevoluteJointCreateCmd(ClientData clientData, Tcl_Interp *interp,
				       int objc, Tcl_Obj * const objv[])
{
  BOX2D_WORLD *bw;
  b2BodyId bodyA;
  b2BodyId bodyB;

  char joint_name[32];
  Tcl_HashEntry *entryPtr;
  int newentry;
  
  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]),
		     " world bodyA bodyB", NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_body(bw, Tcl_GetString(objv[2]), &bodyA) != TCL_OK) return TCL_ERROR;
  if (find_body(bw, Tcl_GetString(objv[3]), &bodyB) != TCL_OK) return TCL_ERROR;

  /* check this, was get GetWorldCenter for 2.4 */
  b2Vec2 pivot = b2Body_GetWorldCenterOfMass(bodyA);
  b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
  jointDef.base.bodyIdA = bodyA;
  jointDef.base.bodyIdB = bodyB;
  jointDef.base.localFrameA.p =
    b2Body_GetLocalPoint(jointDef.base.bodyIdA, pivot);
  jointDef.base.localFrameB.p =
    b2Body_GetLocalPoint(jointDef.base.bodyIdB, pivot);

  b2JointId jointID = b2CreateRevoluteJoint(bw->worldId, &jointDef);
  snprintf(joint_name, sizeof(joint_name), "joint%d", bw->jointCount++); 
  entryPtr = Tcl_CreateHashEntry(&bw->jointTable, joint_name, &newentry);

  /* the joint is an opaque structure, so to hash create copy and add to table */
  b2JointId *joint = (b2JointId *) malloc(sizeof(b2JointId));
  *joint = jointID;
  Tcl_SetHashValue(entryPtr, joint);
  
  Tcl_SetObjResult(interp, Tcl_NewStringObj(joint_name, -1));
  return(TCL_OK);
}


/// Enable/disable the revolute joint spring
// B2_API void b2RevoluteJoint_EnableSpring( b2JointId jointId, bool enableSpring );

static int Box2DRevoluteJoint_EnableSpringCmd(ClientData clientData, Tcl_Interp *interp,
					      int objc, Tcl_Obj * const objv[])
{
  int enable;
  BOX2D_WORLD *bw;
  b2JointId joint;

  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint enable?",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[3], &enable) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_EnableSpring(joint, enable);
  return TCL_OK;
}

/// Enable/disable the revolute joint limit
// B2_API void b2RevoluteJoint_EnableLimit( b2JointId jointId, bool enableLimit );
static int Box2DRevoluteJoint_EnableLimitCmd(ClientData clientData, Tcl_Interp *interp,
					     int objc, Tcl_Obj * const objv[])
{
  int enable;
  BOX2D_WORLD *bw;
  b2JointId joint;

  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint enable?",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[3], &enable) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_EnableLimit(joint, enable);
  return TCL_OK;
}

/// Enable/disable a revolute joint motor
// B2_API void b2RevoluteJoint_EnableMotor( b2JointId jointId, bool enableMotor );
static int Box2DRevoluteJoint_EnableMotorCmd(ClientData clientData, Tcl_Interp *interp,
					     int objc, Tcl_Obj * const objv[])
{
  int enable;
  BOX2D_WORLD *bw;
  b2JointId joint;

  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint enable?",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[3], &enable) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_EnableMotor(joint, enable);
  return TCL_OK;
}

/// Set the revolute joint limits in radians
// B2_API void b2RevoluteJoint_SetLimits( b2JointId jointId, float lower, float upper );
static int Box2DRevoluteJoint_SetLimitsCmd(ClientData clientData, Tcl_Interp *interp,
					   int objc, Tcl_Obj * const objv[])
{
  double lower, upper;
  BOX2D_WORLD *bw;
  b2JointId joint;

  if (objc < 5) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint lower_angle upper_angle",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp, objv[3], &lower) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp, objv[4], &upper) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_SetLimits(joint, lower, upper);
  return TCL_OK;
}

/// Set the revolute joint spring stiffness in Hertz
// B2_API void b2RevoluteJoint_SetSpringHertz( b2JointId jointId, float hertz );
static int Box2DRevoluteJoint_SetSpringHertzCmd(ClientData clientData, Tcl_Interp *interp,
						int objc, Tcl_Obj * const objv[])
{
  double spring_hertz;
  BOX2D_WORLD *bw;
  b2JointId joint;

  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint spring_hertz",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp, objv[3], &spring_hertz) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_SetSpringHertz(joint, spring_hertz);
  return TCL_OK;
}

/// Set the revolute joint spring damping ratio, non-dimensional
// B2_API void b2RevoluteJoint_SetSpringDampingRatio( b2JointId jointId, float dampingRatio );
static int Box2DRevoluteJoint_SetSpringDampingRatioCmd(ClientData clientData, Tcl_Interp *interp,
						       int objc, Tcl_Obj * const objv[])
{
  double damping_ratio;
  BOX2D_WORLD *bw;
  b2JointId joint;
  
  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint damping_ratio",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp, objv[3], &damping_ratio) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_SetSpringDampingRatio(joint, damping_ratio);
  return TCL_OK;
}

/// Set the revolute joint motor speed in radians per second
// B2_API void b2RevoluteJoint_SetMotorSpeed( b2JointId jointId, float motorSpeed );
static int Box2DRevoluteJoint_SetMotorSpeedCmd(ClientData clientData, Tcl_Interp *interp,
					       int objc, Tcl_Obj * const objv[])
{
  double motor_speed;
  BOX2D_WORLD *bw;
  b2JointId joint;
  
  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint motor_speed",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp, objv[3], &motor_speed) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_SetMotorSpeed(joint, motor_speed);
  return TCL_OK;
}

/// Set the revolute joint maximum motor torque, typically in newton-meters
// B2_API void b2RevoluteJoint_SetMaxMotorTorque( b2JointId jointId, float torque );
static int Box2DRevoluteJoint_SetMaxMotorTorqueCmd(ClientData clientData, Tcl_Interp *interp,
						   int objc, Tcl_Obj * const objv[])
{
  double max_motor_torque;
  BOX2D_WORLD *bw;
  b2JointId joint;
  
  if (objc < 4) {
    Tcl_AppendResult(interp, "usage: ", Tcl_GetString(objv[0]), " world joint max_motor_torque",
		     NULL);
    return TCL_ERROR;
  }

  if (find_Box2D(interp, Tcl_GetString(objv[1]), &bw) != TCL_OK) return TCL_ERROR;
  if (find_revolute_joint(bw, Tcl_GetString(objv[2]), &joint) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp, objv[3], &max_motor_torque) != TCL_OK) return TCL_ERROR;

  b2RevoluteJoint_SetMaxMotorTorque(joint, max_motor_torque);
  return TCL_OK;
}



#if 0
/// It the revolute angular spring enabled?
B2_API bool b2RevoluteJoint_IsSpringEnabled( b2JointId jointId );

/// Get the revolute joint spring stiffness in Hertz
B2_API float b2RevoluteJoint_GetSpringHertz( b2JointId jointId );

/// Get the revolute joint spring damping ratio, non-dimensional
B2_API float b2RevoluteJoint_GetSpringDampingRatio( b2JointId jointId );

/// Get the revolute joint current angle in radians relative to the reference angle
///	@see b2RevoluteJointDef::referenceAngle
B2_API float b2RevoluteJoint_GetAngle( b2JointId jointId );

/// Is the revolute joint limit enabled?
B2_API bool b2RevoluteJoint_IsLimitEnabled( b2JointId jointId );

/// Get the revolute joint lower limit in radians
B2_API float b2RevoluteJoint_GetLowerLimit( b2JointId jointId );

/// Get the revolute joint upper limit in radians
B2_API float b2RevoluteJoint_GetUpperLimit( b2JointId jointId );

/// Is the revolute joint motor enabled?
B2_API bool b2RevoluteJoint_IsMotorEnabled( b2JointId jointId );

/// Get the revolute joint motor speed in radians per second
B2_API float b2RevoluteJoint_GetMotorSpeed( b2JointId jointId );

/// Get the revolute joint current motor torque, typically in newton-meters
B2_API float b2RevoluteJoint_GetMotorTorque( b2JointId jointId );

/// Get the revolute joint maximum motor torque, typically in newton-meters
B2_API float b2RevoluteJoint_GetMaxMotorTorque( b2JointId jointId );

#endif

static void PlankoCleanupCallback(ClientData clientData, Tcl_Interp *interp) {
  
    Tcl_MutexUnlock(&Box2D_Mutex);  // Force unlock in case held
    
    BOX2D_INFO *b2info = (BOX2D_INFO *) clientData;

    if (b2info) {
      // Clean up all worlds in this interpreter
      Tcl_HashEntry *entryPtr;
      Tcl_HashSearch search;
      
      for (entryPtr = Tcl_FirstHashEntry(&b2info->Box2DTable, &search);
	   entryPtr != NULL;
	   entryPtr = Tcl_NextHashEntry(&search)) {
	BOX2D_WORLD *bw = (BOX2D_WORLD *) Tcl_GetHashValue(entryPtr);
	if (bw) {
	  Box2DDelete(bw);
	}
      }
      
      Tcl_DeleteHashTable(&b2info->Box2DTable);
      free(b2info);
    }
}

#ifndef WIN32
#ifdef __cplusplus
extern "C" {
#endif
  extern int Tclbox_Init(Tcl_Interp *interp);
#ifdef __cplusplus
}
#endif
#endif

#ifdef WIN32
EXPORT(int,Tclbox_Init) (Tcl_Interp *interp)
#else
int Tclbox_Init(Tcl_Interp *interp)
#endif
{
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.6-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.6-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_PkgProvide(interp, "box2d", "1.0") != TCL_OK) {
    return TCL_ERROR;
  }

  BOX2D_INFO *b2info = (BOX2D_INFO *) calloc(1, sizeof(BOX2D_INFO));

  Tcl_SetAssocData(interp, BOX2D_ASSOC_DATA_KEY,
		   NULL, (void *) b2info);
  Tcl_InitHashTable(&b2info->Box2DTable, TCL_STRING_KEYS);
  b2info->Box2DCount = 0;

  Tcl_CallWhenDeleted(interp, PlankoCleanupCallback, (ClientData) b2info);
  
  Tcl_Eval(interp, "namespace eval box2d {}");
  
  Tcl_CreateObjCommand(interp, "box2d::createWorld", 
		       (Tcl_ObjCmdProc *) Box2DCreateWorldCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::destroy", 
		    (Tcl_CmdProc *) Box2DDestroyCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::createBox", 
		    (Tcl_CmdProc *) Box2DCreateBoxCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::createCircle", 
		    (Tcl_CmdProc *) Box2DCreateCircleCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);

  /* Body and Shape Getters/Setters */
  Tcl_CreateObjCommand(interp, "box2d::setRestitution", 
		       (Tcl_ObjCmdProc *) Box2DSetRestitutionCmd,
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  
  /* Revolute Joints */
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointCreate", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJointCreateCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointEnableSpring", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_EnableSpringCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointEnableMotor", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_EnableMotorCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointEnableLimit", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_EnableLimitCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointSetLimits", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_SetLimitsCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointSetSpringHertz", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_SetSpringHertzCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointSetSpringDampingRatio", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_SetSpringDampingRatioCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointSetMotorSpeed", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_SetMotorSpeedCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "box2d::revoluteJointSetMaxMotorTorque", 
		       (Tcl_ObjCmdProc *) Box2DRevoluteJoint_SetMaxMotorTorqueCmd, 
		       (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);

  
  Tcl_CreateCommand(interp, "box2d::getBodyInfo", 
		    (Tcl_CmdProc *) Box2DGetBodyInfoCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::getBodies", 
		    (Tcl_CmdProc *) Box2DGetBodiesCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "box2d::setBodyType", 
		    (Tcl_CmdProc *) Box2DSetBodyTypeCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::setTransform", 
		    (Tcl_CmdProc *) Box2DSetTransformCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::setLinearVelocity", 
		    (Tcl_CmdProc *) Box2DSetLinearVelocityCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "box2d::step", 
		    (Tcl_CmdProc *) Box2DStepCmd, 
		    (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "box2d::getContactBeginEventCount", 
        (Tcl_CmdProc *) Box2DGetContactBeginEventCountCmd, 
        (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::getContactBeginEvents", 
        (Tcl_CmdProc *) Box2DGetContactBeginEventsCmd, 
        (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::getContactEndEventCount", 
        (Tcl_CmdProc *) Box2DGetContactEndEventCountCmd, 
        (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "box2d::getContactEndEvents", 
        (Tcl_CmdProc *) Box2DGetContactEndEventsCmd, 
        (ClientData) b2info, (Tcl_CmdDeleteProc *) NULL);

  return TCL_OK;
}




