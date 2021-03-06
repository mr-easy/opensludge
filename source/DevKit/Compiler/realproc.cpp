#include <stdio.h>
#include <unistd.h>

//#include "typedef.h"
#include "splitter.hpp"
#include "sludge_functions.h"
#include "moreio.h"
#include "helpers.h"
#include "tokens.h"
#include "objtype.h"
#include "interface.h"
#include "messbox.h"
#include "allknown.h"
#include "compilerinfo.h"

int subNum = 0;
//extern HWND compWin;
const char * emptyString = "";
const char * inThisClass = emptyString;
//extern stringArray * & localVars;
stringArray * globalVarFileOrigins;

bool globalVar (char * theString, stringArray * & globalVars, compilationSpace & globalSpace, const char * filename, unsigned int fileLine) {
	int numVar;

	stringArray * multi = splitString (theString, ',');
	while (multi) {
		stringArray * getVarName = splitString (multi -> string, '=', ONCE);

		if (! checkValidName (getVarName->string, "Not a valid global variable name", filename)) return false;
//		if (! checkNotKnown (getVarName -> string)) return false;
		char * variableName = joinStrings (inThisClass, getVarName -> string);

		if (! checkNotKnown (variableName, filename)) return false;
		addToStringArray (globalVars, variableName);
		addToStringArray (globalVarFileOrigins, filename);
		numVar = findElement (globalVars, variableName);

		stringArray * thisVar = returnArray(globalVars, numVar);
		thisVar->line = fileLine;
		
		
		delete variableName;

		if (getVarName -> next) {
			if (! compileSourceLine (getVarName -> next -> string, globalVars, globalSpace, nullArray, filename, 0)) return addComment (ERRORTYPE_PROJECTERROR, "Can't compile code to set initial value for global variable", multi -> string, filename,0);
			outputDoneCode (globalSpace, SLU_SET_GLOBAL, numVar);
		}

		destroyAll (getVarName);
		destroyFirst (multi);
	}
	return true;
}

bool realWork (int fileNumber, stringArray * & globalVars, compilationSpace & globalSpace) {
	char * theSource;
	char inputName[13];
	stringArray * origName;
	stringArray * theBits;
	stringArray * removeTokenString;

	sprintf (inputName, "_T%05i.TMP", fileNumber);
	theSource = grabWholeFile (inputName);
	unlink (inputName);
	origName = splitString (theSource, '*', ONCE);
	delete theSource;
	
	if (! origName) return false;

	setCompilerText (COMPILER_TXT_FILENAME, origName -> string);

	theBits = splitString (origName -> next -> string, ';', REPEAT);

	subNum = 0;

	while (theBits) {
		if (theBits -> string[0]) {
			removeTokenString = splitString (theBits -> string, ' ', ONCE);
			switch (getToken (removeTokenString -> string)) {
				case TOK_SUB:
				if (! destroyFirst (removeTokenString)) {
					return addComment (ERRORTYPE_PROJECTERROR, "Bad sub declaration", theBits -> string, origName->string, theBits->line);
				}
				if (! outdoorSub (removeTokenString -> string, origName -> string, theBits->line)) {
					return false;
				}
				break;

				case TOK_VAR:
				if (! destroyFirst (removeTokenString)) return addComment (ERRORTYPE_PROJECTERROR, "Bad global variable definition", theBits -> string, origName->string, theBits->line);
				if (! globalVar (removeTokenString -> string, globalVars, globalSpace, origName->string, theBits->line)) return false;
				break;

				case TOK_FLAG:
				case TOK_FLAGS:
				if (! destroyFirst (removeTokenString)) return addComment (ERRORTYPE_PROJECTERROR, "Bad flags definition", theBits -> string, origName->string, theBits->line);
				if (! handleFlags (removeTokenString -> string)) return false;
				break;

				case TOK_OBJECTTYPE:
				if (! destroyFirst (removeTokenString)) {
					return addComment (ERRORTYPE_PROJECTERROR, "Bad objectType declaration", theBits -> string, origName->string, theBits->line);
				}
				if (! createObjectType (removeTokenString -> string, origName -> string, globalVars, globalSpace, origName -> string)) return false;
				break;

				case TOK_UNKNOWN:
				default:
				addComment (ERRORTYPE_PROJECTERROR, "Unknown or illegal token (only 'sub', 'var', 'flag', 'flags' and 'objectType' allowed here)", removeTokenString -> string, origName->string, theBits->line);
				return false;
			}
			destroyAll (removeTokenString);
		}
		subNum ++;
		destroyFirst (theBits);
	}
	destroyAll (origName);
//	finishTask ();
	return true;
}

