/*
 * NAME
 *   postgres.c
 *
 * DESCRIPTION
 *
 * AUTHOR
 *   DLS, 11/24
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h" 
#undef WIN32_LEAN_AND_MEAN
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#define DllEntryPoint DllMain
#define EXPORT(a,b) __declspec(dllexport) a b
#else
#define DllEntryPoint
#define EXPORT a b
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <tcl.h>
#include <libpq-fe.h>
#include <jansson.h>

static Tcl_HashTable connectionTable;
static int connectionCount = 0;

static int findConnection(Tcl_Interp *interp, char *name, PGconn **connection)
{
  PGconn *conn;
  Tcl_HashEntry *entryPtr;

  if ((entryPtr = Tcl_FindHashEntry(&connectionTable, name))) {
    conn = Tcl_GetHashValue(entryPtr);
    if (!conn) {
      Tcl_SetResult(interp, "bad connection ptr in hash table", TCL_STATIC);
      return TCL_ERROR;
    }
    if (connection) *connection = conn;
    return TCL_OK;
  }
  else {
    /* if we weren't just checking for existence, return error */
    if (connection) {
      Tcl_AppendResult(interp, "connection \"", name, "\" not found", 
		       (char *) NULL);
    }
    return TCL_ERROR;
  }
}

static int addConnection(Tcl_Interp *interp, PGconn *conn, char *name)
{
  Tcl_HashEntry *entryPtr;
  int newentry;

  entryPtr = Tcl_CreateHashEntry(&connectionTable, name, &newentry);
  Tcl_SetHashValue(entryPtr, conn);
  Tcl_SetResult(interp, name, TCL_VOLATILE);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    postgresConnectCmd
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    postgres::connect
 *
 * DESCRIPTION
 *    Open connection to postgres
 *
 ****************************************************************************/

static int postgresConnectCmd (ClientData data, Tcl_Interp *interp,
			    int objc, Tcl_Obj *objv[])
{
  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "conninfo");
    return TCL_ERROR;
  }

  // Connection string (replace with your actual credentials)
  const char *conninfo = Tcl_GetString(objv[1]);
  
  // Establish connection to the database
  PGconn *conn = PQconnectdb(conninfo);
  
  // Check if the connection was successful
  if (PQstatus(conn) != CONNECTION_OK) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": connection to database failed -",
		     PQerrorMessage(conn), NULL);
    PQfinish(conn);  // Close the connection and clean up
    return TCL_ERROR;
  }

  char name[32];
  snprintf(name, sizeof(name), "pgconn%d", connectionCount);
  addConnection(interp, conn, name);
  return TCL_OK;
}

static int postgresFinishCmd (ClientData data, Tcl_Interp *interp,
			       int objc, Tcl_Obj *objv[])
{
  PGconn *conn;
  Tcl_HashEntry *entryPtr;
  
  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "conn");
    return TCL_ERROR;
  }

  if (findConnection(interp, Tcl_GetString(objv[1]), &conn) != TCL_OK)
    return TCL_ERROR;
  
  PQfinish(conn);

  /* this should always be true, as we found the connection */
  if ((entryPtr = Tcl_FindHashEntry(&connectionTable,
				    Tcl_GetString(objv[1])))) {
    Tcl_DeleteHashEntry(entryPtr);
  }
  return TCL_OK;
}


static int postgresExecCmd(ClientData data, Tcl_Interp *interp,
			   int objc, Tcl_Obj *objv[])
{
  int nparams;
  PGconn *conn;
  
  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "conn statement");
    return TCL_ERROR;
  }
  
  if (findConnection(interp, Tcl_GetString(objv[1]), &conn) != TCL_OK)
    return TCL_ERROR;

  PGresult *res = PQexec(conn, Tcl_GetString(objv[2]));
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": exec failed - ",
		     PQerrorMessage(conn), NULL);
    PQclear(res);
    return TCL_ERROR;
  }
  Tcl_AppendResult(interp, Tcl_GetString(objv[1]), NULL);
  return TCL_OK;
}

static json_t *col_to_list(PGresult *res, int col)
{
  int i;  
  json_t *array = json_array();

  // Get the column type OID
  Oid coltype = PQftype(res, col);
  int nrows = PQntuples(res);

  switch (coltype) {
  case 16: // boolean
  case 18: // char";
  case 20: // int8
  case 21: // int2
  case 23: // int4"
    {
      int val;
      for (i = 0; i < nrows; i++) {
	val = atoi(PQgetvalue(res, i, col));
	json_array_append_new(array, json_integer(val));
      }
    }
    break;
  case 700: // float4
    {
      float val;
      for (i = 0; i < nrows; i++) {
	val = atoi(PQgetvalue(res, i, col));
	json_array_append_new(array, json_real(val));
      }
    }
    break;      
  case 701: // float8
    {
      double val;
      for (i = 0; i < nrows; i++) {
	val = atoi(PQgetvalue(res, i, col));
	json_array_append_new(array, json_real(val));
      }
    }
    break;
  default:
      {
	for (i = 0; i < nrows; i++) {
	  char *val = PQgetvalue(res, i, col);
	  json_array_append_new(array, json_string(val));
	}
	break;
      }
  }
  return array;
}


static int postgresQueryCmd(ClientData data, Tcl_Interp *interp,
			   int objc, Tcl_Obj *objv[])
{
  int nparams;
  PGconn *conn;
  
  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "conn query");
    return TCL_ERROR;
  }
  
  if (findConnection(interp, Tcl_GetString(objv[1]), &conn) != TCL_OK)
    return TCL_ERROR;

  PGresult *res = PQexec(conn, Tcl_GetString(objv[2]));
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": query failed - ",
		     PQerrorMessage(conn), NULL);
    PQclear(res);
    return TCL_ERROR;
  }

  // Get the number of rows and columns returned by the query
  int nrows = PQntuples(res);
  int ncols = PQnfields(res);

  // For single value, just return it
  if (nrows == 1 && ncols == 1) {
    Tcl_SetResult(interp, PQgetvalue(res, 0, 0), TCL_VOLATILE);
    PQclear(res);
    return TCL_OK;
  }

  json_t *json_result, *json_list;
  json_result = json_object();
  
  // for each column in the result store as named array
  for (int i = 0; i < ncols; i++) {
    json_t *json_list = col_to_list(res, i);
    const char *col_name = PQfname(res, i);
    
    // Add the column to the JSON object
    json_object_set_new(json_result, col_name, json_list);
  }

  // Convert the JSON object to a string
  char *json_str = json_dumps(json_result, 0);
  
  // Clean up the result object and release the JSON object
  PQclear(res);
  json_decref(json_result);

  // Set the result string
  Tcl_SetObjResult(interp, Tcl_NewStringObj(json_str, -1));

  // Free the string, which has been copied
  free(json_str);

  return TCL_OK;
}



static int postgresPrepareCmd(ClientData data, Tcl_Interp *interp,
			       int objc, Tcl_Obj *objv[])
{
  int nparams;
  PGconn *conn;
  
  if (objc < 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "conn name statement nparams");
    return TCL_ERROR;
  }
  
  if (findConnection(interp, Tcl_GetString(objv[1]), &conn) != TCL_OK)
    return TCL_ERROR;
  
  if (Tcl_GetIntFromObj(interp, objv[4], &nparams) != TCL_OK)
    return TCL_ERROR;
  
  // Prepare the query
  PGresult *res = PQprepare(conn,
			    Tcl_GetString(objv[2]),
			    Tcl_GetString(objv[3]),
			    nparams, NULL);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": prepared failed - ",
		     PQerrorMessage(conn), NULL);
    PQclear(res);
    return TCL_ERROR;
  }

  Tcl_AppendResult(interp, Tcl_GetString(objv[1]), NULL);
  return TCL_OK;
}
  

static int postgresExecPreparedCmd (ClientData data, Tcl_Interp *interp,
				    int objc, Tcl_Obj *objv[])
{
  int nparams;
  PGconn *conn;
  const char *prepared;
  const char **values;
  
  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "conn name ?param1? ?param2?");
    return TCL_ERROR;
  }

  if (findConnection(interp, Tcl_GetString(objv[1]), &conn) != TCL_OK)
    return TCL_ERROR;
  
  prepared = Tcl_GetString(objv[2]);

  /* get info about prepared statement */
  PGresult *res = PQdescribePrepared(conn, prepared);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": prepared statement \"",
		     prepared, "\" not found", NULL);
    PQclear(res);
    return TCL_ERROR;
  }
  
  nparams = PQnparams(res);
  PQclear(res);

  if (objc != nparams+3) {
    char pstr[16];
    char nstr[16];
    
    snprintf(pstr, sizeof(pstr), "%d", nparams);
    snprintf(nstr, sizeof(nstr), "%d", objc-3);
    
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": statement \"", Tcl_GetString(objv[2]),
		     "\" requires ", pstr, " params (",
		     nstr, " supplied)", NULL);
    return TCL_ERROR;
  }

  values = (const char **) calloc(nparams, sizeof(char *));
  for (int i = 0; i < nparams; i++) {
    values[i] = Tcl_GetString(objv[i+3]);
  }
  
  // Execute the query with parameters
  res = PQexecPrepared(conn, prepared, nparams, values, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": execPrepared failed - ",
		     PQerrorMessage(conn), NULL);
    free(values);
    PQclear(res);
    return TCL_ERROR;
  }

  free(values);
  return TCL_OK;
}

  
/*****************************************************************************
 *
 * EXPORT
 *
 *****************************************************************************/

#ifdef WIN32
EXPORT(int,Postgres_Init) (Tcl_Interp *interp)
#else
int Postgres_Init(Tcl_Interp *interp)
#endif
{
  
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.5-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.5-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }
  if (Tcl_PkgProvide(interp, "postgres", "1.0") != TCL_OK) {
    return TCL_ERROR;
  }

  Tcl_InitHashTable(&connectionTable, TCL_STRING_KEYS);
  
  Tcl_Eval(interp, "namespace eval postgres {}");
  
  Tcl_CreateObjCommand(interp, "postgres::connect",
		       (Tcl_ObjCmdProc *) postgresConnectCmd,
		       (ClientData) &connectionTable,
		       (Tcl_CmdDeleteProc *) NULL);
  
  Tcl_CreateObjCommand(interp, "postgres::finish",
		       (Tcl_ObjCmdProc *) postgresFinishCmd,
		       (ClientData) &connectionTable,
		       (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "postgres::prepare",
		       (Tcl_ObjCmdProc *) postgresPrepareCmd,
		       (ClientData) &connectionTable,
		       (Tcl_CmdDeleteProc *) NULL);
  
  Tcl_CreateObjCommand(interp, "postgres::exec_prepared",
		       (Tcl_ObjCmdProc *) postgresExecPreparedCmd,
		       (ClientData) &connectionTable,
		       (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "postgres::exec",
		       (Tcl_ObjCmdProc *) postgresExecCmd,
		       (ClientData) &connectionTable,
		       (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp, "postgres::query",
		       (Tcl_ObjCmdProc *) postgresQueryCmd,
		       (ClientData) &connectionTable,
		       (Tcl_CmdDeleteProc *) NULL);
  
  
  return TCL_OK;
 }





