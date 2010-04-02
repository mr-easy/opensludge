#include "debug.h"
#include "allfiles.h"
#ifdef __linux
#include <SDL/SDL.h>
#else
#include "SDL.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#include <complex>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>

#include "sludger.h"
#include "builtin.h"
#include "stringy.h"
#include "newfatal.h"
#include "cursors.h"
#include "statusba.h"
#include "loadsave.h"
#include "backdrop.h"
#include "bg_effects.h"
#include "sprites.h"
#include "fonttext.h"
#include "sprbanks.h"
#include "people.h"
#include "sound.h"
#include "objtypes.h"
#include "floor.h"
#include "zbuffer.h"
#include "talk.h"
#include "region.h"
#include "moreio.h"
#include "savedata.h"
#include "freeze.h"
#include "colours.h"
#include "language.h"
#include "thumbnail.h"
#include "graphics.h"

extern char * gamePath;

int speechMode = 0;
int cameraX, cameraY;
spritePalette pastePalette;

char * launchMe = NULL;
variable * launchResult = NULL;

extern int lastFramesPerSecond, thumbWidth, thumbHeight;
extern bool allowAnyFilename;
extern bool captureAllKeys;
extern short fontSpace;
extern eventHandlers * currentEvents;
extern variableStack * noStack;
extern statusStuff * nowStatus;
extern screenRegion * overRegion;
extern HWND hMainWindow;
extern int /*dialogValue,*/ sceneWidth, sceneHeight;
extern int numBIFNames, numUserFunc;
extern char builtInFunctionNames[][25];

extern char * * allUserFunc;
extern char * * allBIFNames;
extern inputType input;
extern char * loadNow;
extern byte fontTable[256];

extern GLuint backdropTextureName;


extern float speechSpeed;
extern unsigned char brightnessLevel;
extern unsigned char fadeMode;
extern unsigned short saveEncoding;
extern frozenStuffStruct * frozenStuff;
extern unsigned int currentBlankColour;
extern unsigned int languageID;
extern unsigned char currentBurnR, currentBurnG, currentBurnB;
extern settingsStruct gameSettings;
extern aaSettingsStruct maxAntiAliasSettings;

int paramNum[] = {-1, 0, 1, 1, -1, -1, 1, 3, 4, 1, 0, 0, 8, -1,		// SAY -> MOVEMOUSE
						-1, 0, 0, -1, -1, 1, 1, 1, 1, 4, 1, 1, 2, 1,// FOCUS -> REMOVEREGION
						2, 2, 0, 0, 2,								// ANIMATE -> SETSCALE
						-1, 2, 1, 0, 0, 0, 1, 0, 3, 				// new/push/pop stack, status stuff
						2, 0, 0, 3, 1, 0, 2,						// delFromStack -> completeTimers
						-1, -1, -1, 2, 2, 0, 3, 1, 					// anim, costume, pO, setC, wait, sS, substring, stringLength
						0, 1, 1, 0, 2,  							// dark, save, load, quit, rename
						1, 3, 3, 1, 2, 1, 1, 3, 1, 0, 0, 2, 1,		// stackSize, pasteString, startMusic, defvol, vol, stopmus, stopsound, setfont, alignStatus, show x 2, pos'Status, setFloor
						-1, -1, 1, 1, 2, 1, 1, 1, -1, -1, 1, 1, 1,	// force, jump, peekstart, peekend, enqueue, getSavedGames, inFont, loopSound, removeChar, stopCharacter
						1, 0, 3, 3, 1, 2, 1, 2, 2,					// launch, howFrozen, pastecol, litcol, checksaved, float, cancelfunc, walkspeed, delAll
						2, 3, 1, 2, 2, 0, 0, 1, 2, 3, 1, -1,		// extras, mixoverlay, pastebloke, getMScreenX/Y, setSound(Default/-)Volume, looppoints, speechMode, setLightMap
						-1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1,			// think, getCharacterDirection, is(char/region/moving), deleteGame, renameGame, hardScroll, stringWidth, speechSpeed, normalCharacter
						2, 1, 2, 1, 3, 1, 1, 2, 1,					// fetchEvent, setBrightness, spin, fontSpace, burnString, captureAll, cacheSound, setSpinSpeed, transitionMode
						1, 0, 0, 1, 0, 2, 1, 1, 1,					// movie(Start/Abort/Playing), updateDisplay, getSoundCache, savedata, loaddata, savemode, freeSound
						3, 0, 3, 3, 2, 1, 1,						// setParallax, clearParallax, setBlankColour, setBurnColour, getPixelColour, makeFastArray, getCharacterScale
						0, 2, 0,									// getLanguage, launchWith, getFramesPerSecond
						3, 2, 2, 0, 0, 1,							// readThumbnail, setThumbnailSize, hasFlag, snapshot, clearSnapshot, anyFilename
						2, 1,										// regGet, fatal
						4, 3, -1, 0,								// chr AA, max AA, setBackgroundEffect, doBackgroundEffect
						2											// setCharacterAngleOffset
};

bool failSecurityCheck (char * fn) {
	if (fn == NULL) return true;

	int a = 0;

	while (fn[a]) {
		switch (fn[a]) {
			case ':':
			case '\\':
			case '/':
			case '*':
			case '?':
			case '"':
			case '<':
			case '>':
			case '|':
			fatal ("Filenames may not contain the following characters: \n\n\\  /  :  \"  <  >  |  ?  *\n\nConsequently, the following filename is not allowed:", fn);
			return true;
		}
		a ++;
	}
	return false;
}

loadedFunction * saverFunc;

//void deb(char * c, char * c2);

typedef builtReturn (* builtInSludgeFunc) (int numParams, loadedFunction * fun);
struct builtInFunctionData
{
	builtInSludgeFunc func;
};

#define builtIn(a) 			static builtReturn builtIn_ ## a (int numParams, loadedFunction * fun)
#define UNUSEDALL		 	(void) (0 && sizeof(numParams) && sizeof (fun));

static builtReturn sayCore (int numParams, loadedFunction * fun, bool sayIt)
{
	int fileNum = -1;
	char * newText;
	int objT, p;
	killSpeechTimers ();

	switch (numParams) {
		case 3:
		if (! getValueType (fileNum, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
		trimStack (fun -> stack);
		// No break; here

		case 2:
		newText = getTextFromAnyVar (fun -> stack -> thisVar);
		if (! newText) return BR_NOCOMMENT;
		trimStack (fun -> stack);
		if (! getValueType (objT, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
		trimStack (fun -> stack);
		p = wrapSpeech (newText, objT, fileNum, sayIt);
		fun -> timeLeft = p;
		//debugOut ("BUILTIN: sayCore: %s (%i)\n", newText, p);
		fun -> isSpeech = true;
		delete newText;
		newText = NULL;
		return BR_KEEP_AND_PAUSE;
	}

	fatal ("Function should have either 2 or 3 parameters");
	return BR_NOCOMMENT;
}

#pragma mark -
#pragma mark Built in functions

builtIn(say)
{
	 UNUSEDALL
	return sayCore (numParams, fun, true);
	//return BR_CONTINUE; <- Why was this here? I have no idea. /RP
}

builtIn(think)
{
	 UNUSEDALL
	return sayCore (numParams, fun, false);
	//return BR_CONTINUE;
}

builtIn(freeze)
{
	 UNUSEDALL
	freeze ();
	freezeSubs ();
	fun -> freezerLevel = 0;
	return BR_CONTINUE;
}

builtIn(unfreeze)
{
	 UNUSEDALL
	unfreeze ();
	unfreezeSubs ();
	return BR_CONTINUE;
}

builtIn(howFrozen)
{
	 UNUSEDALL
	setVariable (fun -> reg, SVT_INT, howFrozen ());
	return BR_CONTINUE;
}

builtIn(setCursor)
{
	 UNUSEDALL
	personaAnimation * aa = getAnimationFromVar (fun -> stack -> thisVar);
	pickAnimCursor (aa);
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(getMouseX)
{
	 UNUSEDALL
	setVariable (fun -> reg, SVT_INT, input.mouseX + cameraX);
	return BR_CONTINUE;
}

builtIn(getMouseY)
{
	 UNUSEDALL
	setVariable (fun -> reg, SVT_INT, input.mouseY + cameraY);
	return BR_CONTINUE;
}

builtIn(getMouseScreenX)
{
	 UNUSEDALL
	setVariable (fun -> reg, SVT_INT, input.mouseX);
	return BR_CONTINUE;
}

builtIn(getMouseScreenY)
{
	 UNUSEDALL
	setVariable (fun -> reg, SVT_INT, input.mouseY);
	return BR_CONTINUE;
}

builtIn(getStatusText)
{
	 UNUSEDALL
	makeTextVar (fun -> reg, statusBarText ());
	return BR_CONTINUE;
}

builtIn(getMatchingFiles)
{
	 UNUSEDALL
				char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
				if (! newText) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				unlinkVar (fun -> reg);

				// Return value
				fun -> reg.varType = SVT_STACK;
				fun -> reg.varData.theStack = new stackHandler;
				if (! checkNew (fun -> reg.varData.theStack)) return BR_NOCOMMENT;
				fun -> reg.varData.theStack -> first = NULL;
				fun -> reg.varData.theStack -> last = NULL;
				fun -> reg.varData.theStack -> timesUsed = 1;
				if (! getSavedGamesStack (fun -> reg.varData.theStack, newText)) return BR_NOCOMMENT;
				delete newText;
				newText = NULL;
			return BR_CONTINUE;
}

builtIn(saveGame)
{
	 UNUSEDALL

				if (frozenStuff) {
					fatal ("Can't save game state while the engine is frozen");
				}

				loadNow = getTextFromAnyVar (fun -> stack -> thisVar);
				trimStack (fun -> stack);

				char * aaaaa = encodeFilename (loadNow);
				delete loadNow;
				if (failSecurityCheck (aaaaa)) return BR_NOCOMMENT;		// Won't fail if encoded, how cool is that? OK, not very.

				loadNow = joinStrings (":", aaaaa);
				delete aaaaa;

				setVariable (fun -> reg, SVT_INT, 0);
				saverFunc = fun;
				return BR_KEEP_AND_PAUSE;
}

builtIn(fileExists)
{
	 UNUSEDALL
				loadNow = getTextFromAnyVar (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				char * aaaaa = encodeFilename (loadNow);
				delete loadNow;
				if (failSecurityCheck (aaaaa)) return BR_NOCOMMENT;
				FILE * fp = fopen (aaaaa, "rb");
				if (! fp) {
					char currentDir[1000];
					if (! getcwd (currentDir, 998)) {
						fprintf(stderr, "Can't get current directory.\n");
					}

					chdir (gamePath);
					fp = fopen (aaaaa, "rb");
					chdir (currentDir);
				}
				// Return value
				setVariable (fun -> reg, SVT_INT, (fp != NULL));
				if (fp) fclose (fp);
				delete aaaaa;
				loadNow = NULL;
			return BR_CONTINUE;
}

builtIn(loadGame)
{
	 UNUSEDALL
				char * aaaaa = getTextFromAnyVar (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				loadNow = encodeFilename (aaaaa);
				delete aaaaa;

				if (frozenStuff) {
					fatal ("Can't load a saved game while the engine is frozen");
				}

				if (failSecurityCheck (loadNow)) return BR_NOCOMMENT;
				FILE * fp = fopen (loadNow, "rb");
				if (fp) {
					fclose (fp);
					return BR_KEEP_AND_PAUSE;
				}
				delete loadNow;
				loadNow = NULL;
	return BR_CONTINUE;
}

			//-----------------------------
			// BACKGROUND IMAGE - Painting
			//-----------------------------

builtIn(blankScreen)
{
	 UNUSEDALL
			blankScreen (0, 0, sceneWidth, sceneHeight);
			return BR_CONTINUE;
}

builtIn(blankArea)
{
	 UNUSEDALL
				int x1, y1, x2, y2;
				if (! getValueType (y2, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x2, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (y1, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x1, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				blankScreen (x1, y1, x2, y2);
				return BR_CONTINUE;
}

builtIn(darkBackground)
{
	 UNUSEDALL
			darkScreen ();
//			blurScreen ();
			return BR_CONTINUE;
}

builtIn(addOverlay)
{
	 UNUSEDALL
				int fileNumber, xPos, yPos;
				if (! getValueType (yPos, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (xPos, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				loadBackDrop (fileNumber, xPos, yPos);
				return BR_CONTINUE;
}

builtIn(mixOverlay)
{
	 UNUSEDALL
				int fileNumber, xPos, yPos;
				if (! getValueType (yPos, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (xPos, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				mixBackDrop (fileNumber, xPos, yPos);
				return BR_CONTINUE;
}

builtIn(pasteImage)
			{
	 UNUSEDALL
				int x, y;
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				personaAnimation * pp = getAnimationFromVar (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				if (pp == NULL) return BR_CONTINUE;

				pasteCursor (x, y, pp);
				return BR_CONTINUE;
			}

			//------------------------------
			// BACKGROUND IMAGE - Scrolling
			//------------------------------

builtIn(setSceneDimensions)
{
	 UNUSEDALL
				int x, y;
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (resizeBackdrop (x, y)) {
					blankScreen (0, 0, x, y);
					return BR_CONTINUE;
				}
				fatal ("Out of memory creating new backdrop.");
				return BR_NOCOMMENT;
			}

builtIn(aimCamera)
			{
	 UNUSEDALL
				if (! getValueType (cameraY, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (cameraX, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				cameraX -= winWidth >> 1;
				cameraY -= winHeight >> 1;

				if (cameraX < 0) cameraX = 0;
				else if (cameraX > sceneWidth - winWidth) cameraX = sceneWidth - winWidth;
				if (cameraY < 0) cameraY = 0;
				else if (cameraY > sceneHeight - winHeight) cameraY = sceneHeight - winHeight;
				return BR_CONTINUE;
			}

			//----------------------
			// VARIABLES - Anything
			//----------------------

			builtIn(pickOne)
			{
	 UNUSEDALL
				if (! numParams) {
					fatal ("Built-in function should have at least 1 parameter");
					return BR_NOCOMMENT;
				}
				int i = rand() % numParams;

				// Return value
				while (numParams --) {
					if (i == numParams) copyVariable (fun -> stack -> thisVar, fun -> reg);
					trimStack (fun -> stack);
				}
				return BR_CONTINUE;
			}


			//---------------------
			// VARIABLES - Strings
			//---------------------

builtIn(substring)
{
	 UNUSEDALL
	char * wholeString;
	char * newString;
	int start, length, a;

	//debugOut ("BUILTIN: substring\n");

	if (! getValueType (length, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (! getValueType (start, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	wholeString = getTextFromAnyVar (fun -> stack -> thisVar);
	trimStack (fun -> stack);
	//debugOut ("String is %s (%i)\n", wholeString, length);
	if (length<0) {
		length=0;
	}

	newString = new char[length + 1];
	if (! checkNew (newString)) {
		//debugOut ("BUILTIN: substring exiting early.\n");
		return BR_NOCOMMENT;
	}

	for (a = 0; a < length; a ++) newString[a] = wholeString[start ++];

	newString[length] = NULL;
	makeTextVar (fun -> reg, newString);
	delete newString;
	//debugOut ("BUILTIN: substring exiting normally.\n");
	return BR_CONTINUE;
}

builtIn(stringLength)
{
	 UNUSEDALL
	char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
	trimStack (fun -> stack);
	setVariable (fun -> reg, SVT_INT, strlen (newText));
	delete newText;
	return BR_CONTINUE;
}

builtIn(newStack)
{
	 UNUSEDALL
			unlinkVar (fun -> reg);

			// Return value
			fun -> reg.varType = SVT_STACK;
			fun -> reg.varData.theStack = new stackHandler;
			if (! checkNew (fun -> reg.varData.theStack)) return BR_NOCOMMENT;
			fun -> reg.varData.theStack -> first = NULL;
			fun -> reg.varData.theStack -> last = NULL;
			fun -> reg.varData.theStack -> timesUsed = 1;
			while (numParams --) {
				if (! addVarToStack (fun -> stack -> thisVar, fun -> reg.varData.theStack -> first)) return BR_NOCOMMENT;
				if (fun -> reg.varData.theStack -> last == NULL) {
					fun -> reg.varData.theStack -> last = fun -> reg.varData.theStack -> first;
				}
				trimStack (fun -> stack);
			}
			return BR_CONTINUE;
}

// wait is exactly the same function, but limited to 2 parameters
#define builtIn_wait builtIn_newStack

builtIn(stackSize)
{
	 UNUSEDALL
			switch (fun -> stack -> thisVar.varType) {
				case SVT_STACK:
				// Return value
				setVariable (fun -> reg, SVT_INT, stackSize (fun -> stack -> thisVar.varData.theStack));
				trimStack (fun -> stack);
				return BR_CONTINUE;

				case SVT_FASTARRAY:
				// Return value
				setVariable (fun -> reg, SVT_INT, fun -> stack -> thisVar.varData.fastArray -> size);
				trimStack (fun -> stack);
				return BR_CONTINUE;

				default:
				break;
			}
			fatal ("Parameter isn't a stack or a fast array.");
			return BR_NOCOMMENT;
}

builtIn(copyStack)
{
	 UNUSEDALL
			if (fun -> stack -> thisVar.varType != SVT_STACK) {
				fatal ("Parameter isn't a stack.");
				return BR_NOCOMMENT;
			}
			// Return value
			if (! copyStack (fun -> stack -> thisVar, fun -> reg)) return BR_NOCOMMENT;
			trimStack (fun -> stack);
			return BR_CONTINUE;
}

builtIn(pushToStack)
{
	 UNUSEDALL
	if (fun -> stack -> next -> thisVar.varType != SVT_STACK) {
		fatal ("Parameter isn't a stack");
		return BR_NOCOMMENT;
	}

	if (! addVarToStack (fun -> stack -> thisVar, fun -> stack -> next -> thisVar.varData.theStack -> first))
		return BR_NOCOMMENT;

	if (fun -> stack -> next -> thisVar.varData.theStack -> first -> next == NULL)
		fun -> stack -> next -> thisVar.varData.theStack -> last = fun -> stack -> next -> thisVar.varData.theStack -> first;

	trimStack (fun -> stack);
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(enqueue)
{
	 UNUSEDALL
	if (fun -> stack -> next -> thisVar.varType != SVT_STACK) {
		fatal ("Parameter isn't a stack");
		return BR_NOCOMMENT;
	}

	if (fun -> stack -> next -> thisVar.varData.theStack -> first == NULL)
	{
		if (! addVarToStack (fun -> stack -> thisVar, fun -> stack -> next -> thisVar.varData.theStack -> first))
				return BR_NOCOMMENT;

		fun -> stack -> next -> thisVar.varData.theStack -> last = fun -> stack -> next -> thisVar.varData.theStack -> first;
	}
	else
	{
		if (! addVarToStack (fun -> stack -> thisVar,
			fun -> stack -> next -> thisVar.varData.theStack -> last -> next))
				return BR_NOCOMMENT;
		fun -> stack -> next -> thisVar.varData.theStack -> last = fun -> stack -> next -> thisVar.varData.theStack -> last -> next;
	}

	trimStack (fun -> stack);
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(deleteFromStack)
{
	 UNUSEDALL
	if (fun -> stack -> next -> thisVar.varType != SVT_STACK) {
		fatal ("Parameter isn't a stack.");
		return BR_NOCOMMENT;
	}

	// Return value
	setVariable (fun -> reg, SVT_INT,
		deleteVarFromStack (fun -> stack -> thisVar,
			fun -> stack -> next -> thisVar.varData.theStack -> first, false));

	// Horrible hacking because 'last' value might now be wrong!
	fun->stack->next->thisVar.varData.theStack->last = stackFindLast (fun->stack->next->thisVar.varData.theStack->first);

	trimStack (fun -> stack);
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(deleteAllFromStack)
{
	 UNUSEDALL
	if (fun -> stack -> next -> thisVar.varType != SVT_STACK) {
		fatal ("Parameter isn't a stack.");
		return BR_NOCOMMENT;
	}

	// Return value
	setVariable (fun -> reg, SVT_INT,
		deleteVarFromStack (fun -> stack -> thisVar,
			fun -> stack -> next -> thisVar.varData.theStack -> first, true));

	// Horrible hacking because 'last' value might now be wrong!
	fun->stack->next->thisVar.varData.theStack->last = stackFindLast (fun->stack->next->thisVar.varData.theStack->first);

	trimStack (fun -> stack);
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(popFromStack)
{
	 UNUSEDALL
	if (fun -> stack -> thisVar.varType != SVT_STACK) {
		fatal ("Parameter isn't a stack.");
		return BR_NOCOMMENT;
	}
	if (fun -> stack -> thisVar.varData.theStack -> first == NULL) {
		fatal ("The stack's empty.");
		return BR_NOCOMMENT;
	}

	// Return value
	copyVariable (fun -> stack -> thisVar.varData.theStack -> first -> thisVar, fun -> reg);
	trimStack (fun -> stack -> thisVar.varData.theStack -> first);
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(peekStart)
{
	 UNUSEDALL
	if (fun -> stack -> thisVar.varType != SVT_STACK) {
		fatal ("Parameter isn't a stack.");
		return BR_NOCOMMENT;
	}
	if (fun -> stack -> thisVar.varData.theStack -> first == NULL) {
		fatal ("The stack's empty.");
		return BR_NOCOMMENT;
	}

	// Return value
	copyVariable (fun -> stack -> thisVar.varData.theStack -> first -> thisVar, fun -> reg);
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(peekEnd)
{
	 UNUSEDALL
				if (fun -> stack -> thisVar.varType != SVT_STACK) {
					fatal ("Parameter isn't a stack.");
					return BR_NOCOMMENT;
				}
				if (fun -> stack -> thisVar.varData.theStack -> first == NULL) {
					fatal ("The stack's empty.");
					return BR_NOCOMMENT;
				}

				// Return value
				copyVariable (fun -> stack -> thisVar.varData.theStack -> last -> thisVar, fun -> reg);
				trimStack (fun -> stack);
				return BR_CONTINUE;
}

builtIn(random)
{
	 UNUSEDALL
				int num;

				if (! getValueType (num, SVT_INT, fun -> stack -> thisVar))
					return BR_NOCOMMENT;

				trimStack (fun -> stack);
				if (num <= 0) num = 1;
				setVariable (fun -> reg, SVT_INT, rand() % num);
				return BR_CONTINUE;
}

static bool getRGBParams(int & red, int & green, int & blue, loadedFunction * fun)
{
				if (! getValueType (blue, SVT_INT, fun -> stack -> thisVar)) return false;
				trimStack (fun -> stack);
				if (! getValueType (green, SVT_INT, fun -> stack -> thisVar)) return false;
				trimStack (fun -> stack);
				if (! getValueType (red, SVT_INT, fun -> stack -> thisVar)) return false;
				trimStack (fun -> stack);
				return true;
}

builtIn (setStatusColour)
{
	 UNUSEDALL
	int red, green, blue;

	if (! getRGBParams(red, green, blue, fun))
		return BR_NOCOMMENT;

	statusBarColour ((byte) red, (byte) green, (byte) blue);
	return BR_CONTINUE;
}

builtIn (setLitStatusColour)
{
	 UNUSEDALL
	int red, green, blue;

	if (! getRGBParams(red, green, blue, fun))
		return BR_NOCOMMENT;

	statusBarLitColour ((byte) red, (byte) green, (byte) blue);
	return BR_CONTINUE;
}

builtIn (setPasteColour)
{
	 UNUSEDALL
	int red, green, blue;

	if (! getRGBParams(red, green, blue, fun))
		return BR_NOCOMMENT;

	setFontColour (pastePalette, (byte) red, (byte) green, (byte) blue);
	return BR_CONTINUE;
}

builtIn (setBlankColour)
{
	 UNUSEDALL
	int red, green, blue;

	if (! getRGBParams(red, green, blue, fun))
		return BR_NOCOMMENT;

	currentBlankColour = makeColour (red & 255, green & 255, blue & 255);
	setVariable (fun -> reg, SVT_INT, 1);
	return BR_CONTINUE;
}

builtIn (setBurnColour)
{
	 UNUSEDALL
	int red, green, blue;

	if (! getRGBParams(red, green, blue, fun))
		return BR_NOCOMMENT;

	currentBurnR = red;
	currentBurnG = green;
	currentBurnB = blue;
	setVariable (fun -> reg, SVT_INT, 1);
	return BR_CONTINUE;
}


builtIn(setFont)
{
	 UNUSEDALL
				int fileNumber, newHeight;
				if (! getValueType (newHeight, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
//				newDebug ("  Height:", newHeight);
				trimStack (fun -> stack);
				char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
				if (! newText) return BR_NOCOMMENT;
//				newDebug ("  Character supported:", newText);
				trimStack (fun -> stack);
				if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
//				newDebug ("  File:", fileNumber);
				trimStack (fun -> stack);
				if (! loadFont (fileNumber, newText, newHeight)) return BR_NOCOMMENT;
//				newDebug ("  Done!");
				delete newText;

				return BR_CONTINUE;
}

builtIn(inFont)
{
	 UNUSEDALL
	char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
	if (! newText) return BR_NOCOMMENT;
	trimStack (fun -> stack);

	// Return value
	setVariable (fun -> reg, SVT_INT, newText[0] && newText[1] == 0 && fontTable[(unsigned char) newText[0]] != 0);
	return BR_CONTINUE;
}

builtIn(pasteString)
{
	 UNUSEDALL
				char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				int y, x;
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (x == IN_THE_CENTRE) x = (winWidth - stringWidth (newText)) >> 1;
				fixFont (pastePalette);
				pasteStringToBackdrop (newText, x, y, pastePalette);
				delete newText;
				return BR_CONTINUE;
}

builtIn(anim)
{
	 UNUSEDALL
	if (numParams < 2) {
		fatal ("Built-in function anim() must have at least 2 parameters.");
		return BR_NOCOMMENT;
	}

	// First store the frame numbers and take 'em off the stack
	personaAnimation * ba = createPersonaAnim (numParams - 1, fun -> stack);

	// Only remaining paramter is the file number
	int fileNumber;
	if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);

	// Load the required sprite bank
	loadedSpriteBank * sprBanky = loadBankForAnim (fileNumber);
	if (! sprBanky) return BR_NOCOMMENT;	// File not found, fatal done already
	setBankFile (ba, sprBanky);

	// Return value
	newAnimationVariable (fun -> reg, ba);

	return BR_CONTINUE;
}

builtIn(costume)
{
	UNUSEDALL
	persona * newPersona = new persona;
	if (! checkNew (newPersona)) return BR_NOCOMMENT;
	newPersona -> numDirections = numParams / 3;
	if (numParams == 0 || newPersona -> numDirections * 3 != numParams) {
		fatal ("Illegal number of parameters (should be greater than 0 and divisible by 3)");
		return BR_NOCOMMENT;
	}
	int iii;
	newPersona -> animation = new personaAnimation * [numParams];
	if (! checkNew (newPersona -> animation)) return BR_NOCOMMENT;
	for (iii = numParams - 1; iii >= 0; iii --) {
		newPersona -> animation[iii] = getAnimationFromVar (fun -> stack -> thisVar);
		trimStack (fun -> stack);
	}

	// Return value
	newCostumeVariable (fun -> reg, newPersona);
	return BR_CONTINUE;
}

builtIn(launch)
{
	UNUSEDALL
	char * newTextA = getTextFromAnyVar (fun -> stack -> thisVar);
	if (! newTextA) return BR_NOCOMMENT;

	char * newText = encodeFilename (newTextA);
	delete newTextA;

	trimStack (fun -> stack);
	if (newText[0] == 'h' &&
		newText[1] == 't' &&
		newText[2] == 't' &&
		newText[3] == 'p' &&
		newText[4] == ':') {

		// IT'S A WEBSITE!
		launchMe = newText;
//					fatal (launchMe);
//					return BR_NOCOMMENT;
	} else {
		char gameDir[1000];
		if (! getcwd (gameDir, 998)) {
			fatal ("Can't get current directory");
			return BR_NOCOMMENT;
		}
#ifdef _WIN32
		if (gameDir[strlen (gameDir) - 1] != '\\') {
			gameDir[strlen (gameDir) + 1] = NULL;
			gameDir[strlen (gameDir)] = '\\';
		}
#else
		if (gameDir[strlen (gameDir) - 1] != '/') {
			gameDir[strlen (gameDir) + 1] = NULL;
			gameDir[strlen (gameDir)] = '/';
		}
#endif
		launchMe = joinStrings (gameDir, newText);
		delete newText;
		if (! launchMe) return BR_NOCOMMENT;
	}
	setGraphicsWindow(false);
	setVariable (fun -> reg, SVT_INT, 1);
	launchResult = &fun->reg;

	return BR_KEEP_AND_PAUSE;
}

builtIn(pause)
{
	UNUSEDALL
	int theTime;
	if (! getValueType (theTime, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (theTime > 0) {
		fun -> timeLeft = theTime - 1;
		fun -> isSpeech = false;
		return BR_KEEP_AND_PAUSE;
	}
	return BR_CONTINUE;
}

builtIn(completeTimers)
{
	UNUSEDALL
	completeTimers();
	return BR_CONTINUE;
}

builtIn(callEvent)
{
	UNUSEDALL
	int obj1, obj2;
	if (! getValueType (obj2, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (! getValueType (obj1, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);

	int fNum = getCombinationFunction (obj1, obj2);

	// Return value
	if (fNum) {
		setVariable (fun -> reg, SVT_FUNC, fNum);
		return BR_CALLAFUNC;
	}
	setVariable (fun -> reg, SVT_INT, 0);
	return BR_CONTINUE;
}

// The movie functions are deprecated and does nothing.
builtIn(_rem_movieStart)
{
	UNUSEDALL
	trimStack (fun -> stack);
	return BR_CONTINUE;
}

builtIn(_rem_movieAbort)
{
	UNUSEDALL
	setVariable (fun -> reg, SVT_INT, 0);
	return BR_CONTINUE;
}

builtIn(_rem_moviePlaying)
{
	UNUSEDALL
	setVariable (fun -> reg, SVT_INT, 0);
	return BR_CONTINUE;
}


SDL_Event quit_event;
bool reallyWantToQuit = false;

builtIn(quitGame)
{
	UNUSEDALL
	reallyWantToQuit = true;
	quit_event.type=SDL_QUIT;
	SDL_PushEvent(&quit_event);
	//PostQuitMessage(0);
	return BR_NOCOMMENT;
}

//-------------------------
// AUDIO FUNCTIONS - Music
//-------------------------

builtIn(startMusic)
{
	UNUSEDALL
	int fromTrack, musChan, fileNumber;
	if (! getValueType (fromTrack, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (! getValueType (musChan, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (! playMOD (fileNumber, musChan, fromTrack)) return BR_CONTINUE; //BR_NOCOMMENT;
	return BR_CONTINUE;
}

builtIn(stopMusic)
{
	UNUSEDALL
	int v;
	if (! getValueType (v, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	stopMOD (v);
	return BR_CONTINUE;
}

builtIn(setMusicVolume)
{
	UNUSEDALL
	int musChan, v;
	if (! getValueType (v, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (! getValueType (musChan, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	setMusicVolume (musChan, v);
	return BR_CONTINUE;
}

builtIn(setDefaultMusicVolume)
{
	UNUSEDALL
	int v;
	if (! getValueType (v, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	setDefaultMusicVolume (v);
	return BR_CONTINUE;
}

builtIn(playSound)
{
	UNUSEDALL
	int fileNumber;
	if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	fprintf (stderr, "Playing sound %d\n", fileNumber);
	if (! startSound (fileNumber, false)) return BR_CONTINUE;	// Was BR_NOCOMMENT
	return BR_CONTINUE;
}
builtIn(loopSound)
{
	UNUSEDALL
	int fileNumber;
	if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	if (! startSound (fileNumber, true)) return BR_CONTINUE;	// Was BR_NOCOMMENT
	return BR_CONTINUE;
}

builtIn(stopSound)
			{
	UNUSEDALL
				int v;
				if (! getValueType (v, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				huntKillSound (v);
				return BR_CONTINUE;
			}

builtIn(setDefaultSoundVolume)
			{
	UNUSEDALL
				int v;
				if (! getValueType (v, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setDefaultSoundVolume (v);
				return BR_CONTINUE;
			}

builtIn(setSoundVolume)
			{
	UNUSEDALL
				int musChan, v;
				if (! getValueType (v, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (musChan, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setSoundVolume (musChan, v);
				return BR_CONTINUE;
			}


builtIn(setSoundLoopPoints)
			{
	UNUSEDALL
				int musChan, theEnd, theStart;
				if (! getValueType (theEnd, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (theStart, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (musChan, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setSoundLoop (musChan, theStart, theEnd);
				return BR_CONTINUE;
			}

			//-------------------------
			// EXTRA ROOM BITS - Floor
			//-------------------------

builtIn(setFloor)
{
	UNUSEDALL
			if (fun -> stack -> thisVar.varType == SVT_FILE) {
				int v;
				getValueType (v, SVT_FILE, fun -> stack -> thisVar);
				trimStack (fun -> stack);
				if (! setFloor (v)) return BR_NOCOMMENT;
			} else {
				trimStack (fun -> stack);
				setFloorNull ();
			}
			return BR_CONTINUE;
}

builtIn(showFloor)
{
	UNUSEDALL
			drawFloor ();
			return BR_CONTINUE;
}

			//----------------------------
			// EXTRA ROOM BITS - Z-buffer
			//----------------------------

builtIn(setZBuffer)
{
	UNUSEDALL
			if (fun -> stack -> thisVar.varType == SVT_FILE) {
				int v;
				getValueType (v, SVT_FILE, fun -> stack -> thisVar);
				trimStack (fun -> stack);
				if (! setZBuffer (v)) return BR_NOCOMMENT;
			} else {
				trimStack (fun -> stack);
				killZBuffer ();
			}
			return BR_CONTINUE;
}

				//-----------------------------------------
			// OBJECTS - Screen regions AND characters
			//-----------------------------------------

builtIn(setSpeechMode)
{
	UNUSEDALL
 				if (! getValueType (speechMode, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (speechMode < 0 || speechMode > 2) {
					fatal ("Valid parameters are be SPEECHANDTEXT, SPEECHONLY or TEXTONLY");
					return BR_NOCOMMENT;
				}
				return BR_CONTINUE;
}

builtIn(somethingSpeaking)
{
	UNUSEDALL
				int i = isThereAnySpeechGoingOn ();
				if (i == -1) {
					setVariable (fun -> reg, SVT_INT, 0);
				} else {
					setVariable (fun -> reg, SVT_OBJTYPE, i);
				}
				return BR_CONTINUE;
}

builtIn(skipSpeech)
{
	UNUSEDALL
			killSpeechTimers ();
			return BR_CONTINUE;
}

builtIn(getOverObject)
{
	UNUSEDALL
			if (overRegion)
				// Return value
				setVariable (fun -> reg, SVT_OBJTYPE, overRegion -> thisType -> objectNum);
			else
				// Return value
				setVariable (fun -> reg, SVT_INT, 0);
			return BR_CONTINUE;
}

builtIn(rename)
{
	UNUSEDALL
				char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
				int objT;
				if (! newText) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (objT, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				objectType * o = findObjectType (objT);
				delete o -> screenName;
				o -> screenName = newText;
				return BR_CONTINUE;
}

builtIn (getObjectX)
{
	UNUSEDALL
	int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);

	onScreenPerson * pers = findPerson (objectNumber);
	if (pers) {
		setVariable (fun -> reg, SVT_INT, pers -> x);
	} else {
		screenRegion * la = getRegionForObject (objectNumber);
		if (la) {
			setVariable (fun -> reg, SVT_INT, la -> sX);
		} else {
			setVariable (fun -> reg, SVT_INT, 0);
		}
	}
	return BR_CONTINUE;
}

builtIn (getObjectY)
{
	UNUSEDALL
	int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);

	onScreenPerson * pers = findPerson (objectNumber);
	if (pers) {
		setVariable (fun -> reg, SVT_INT, pers -> y);
	} else {
		screenRegion * la = getRegionForObject (objectNumber);
		if (la) {
			setVariable (fun -> reg, SVT_INT, la -> sY);
		} else {
			setVariable (fun -> reg, SVT_INT, 0);
		}
	}
	return BR_CONTINUE;
}


builtIn(addScreenRegion)
{
			UNUSEDALL
		int sX, sY, x1, y1, x2, y2, di, objectNumber;
				if (! getValueType (di, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (sY, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (sX, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (y2, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x2, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (y1, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x1, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (addScreenRegion (x1, y1, x2, y2, sX, sY, di, objectNumber)) return BR_CONTINUE;
				return BR_NOCOMMENT;

}

builtIn(removeScreenRegion)
{
			UNUSEDALL
		int objectNumber;
				if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				removeScreenRegion (objectNumber);
				return BR_CONTINUE;
}

builtIn(showBoxes)
{
			UNUSEDALL
	showBoxes ();
			return BR_CONTINUE;
}

builtIn(removeAllScreenRegions)
{
			UNUSEDALL
	killAllRegions ();
			return BR_CONTINUE;
}

builtIn(addCharacter)
{
	UNUSEDALL
				persona * p;
				int x, y, objectNumber;

				p = getCostumeFromVar (fun -> stack -> thisVar);
				if (p == NULL) return BR_NOCOMMENT;

				trimStack (fun -> stack);
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (addPerson (x, y, objectNumber, p)) return BR_CONTINUE;
				return BR_NOCOMMENT;
}

builtIn(hideCharacter)
{
		UNUSEDALL
int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	setShown (false, objectNumber);
	return BR_CONTINUE;
}

builtIn(showCharacter)
{
	UNUSEDALL
	int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	setShown (true, objectNumber);
	return BR_CONTINUE;
}

builtIn(removeAllCharacters)
{
	UNUSEDALL
			killSpeechTimers ();
			killMostPeople ();
			return BR_CONTINUE;
}

builtIn(setCharacterDrawMode)
{
	UNUSEDALL
				int obj, di;
				if (! getValueType (di, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setDrawMode (di, obj);
				return BR_CONTINUE;
}

builtIn(setScale)
{
	UNUSEDALL
				int val1, val2;
				if (! getValueType (val2, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (val1, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setScale ((short int) val1, (short int) val2);
				return BR_CONTINUE;
}

builtIn(stopCharacter)
{
	UNUSEDALL
				int obj;
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				// Return value
				setVariable (fun -> reg, SVT_INT, stopPerson (obj));
				return BR_CONTINUE;
}

builtIn(pasteCharacter)
{
	UNUSEDALL
				int obj;
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				onScreenPerson * thisPerson = findPerson (obj);
				if (thisPerson) {
					personaAnimation * myAnim;
					myAnim = thisPerson -> myAnim;
					if (myAnim != thisPerson -> lastUsedAnim) {
						thisPerson -> lastUsedAnim = myAnim;
						thisPerson -> frameNum = 0;
						thisPerson -> frameTick = myAnim -> frames[0].howMany;
					}

					int fNum = myAnim -> frames[thisPerson -> frameNum].frameNum;
					fixScaleSprite (thisPerson -> x, thisPerson -> y, myAnim -> theSprites -> bank.sprites[abs (fNum)], myAnim -> theSprites -> bank.myPalette, thisPerson -> scale, thisPerson -> drawMode, thisPerson -> floaty, ! (thisPerson -> extra & EXTRA_NOZB), ! (thisPerson -> extra & EXTRA_NOLITE), 0, 0, fNum < 0, & thisPerson->aaSettings);
					setVariable (fun -> reg, SVT_INT, 1);
				} else {
					setVariable (fun -> reg, SVT_INT, 0);
				}
				return BR_CONTINUE;
}

builtIn(animate)
{
	UNUSEDALL
				int obj;
				personaAnimation * pp = getAnimationFromVar (fun -> stack -> thisVar);
				if (pp == NULL) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				animatePerson (obj, pp);
				setVariable (fun -> reg, SVT_INT, timeForAnim (pp));
				return BR_CONTINUE;
}

builtIn(setCostume)
{
	UNUSEDALL
				int obj;
				persona * pp = getCostumeFromVar (fun -> stack -> thisVar);
				if (pp == NULL) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				animatePerson (obj, pp);
				return BR_CONTINUE;
}

builtIn(floatCharacter)
			{
	UNUSEDALL
				int obj, di;
				if (! getValueType (di, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, floatCharacter (di, obj));
				return BR_CONTINUE;
			}

builtIn(setCharacterWalkSpeed)
			{
	UNUSEDALL
				int obj, di;
				if (! getValueType (di, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, setCharacterWalkSpeed (di, obj));
				return BR_CONTINUE;
			}

builtIn(turnCharacter)
{
	UNUSEDALL
				int obj, di;
				if (! getValueType (di, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, turnPersonToFace (obj, di));
				return BR_CONTINUE;
}

builtIn(setCharacterExtra)
			{
	UNUSEDALL
				int obj, di;
				if (! getValueType (di, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, setPersonExtra (obj, di));
				return BR_CONTINUE;
			}

builtIn(removeCharacter)
			{
	UNUSEDALL
				int objectNumber;
				if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				removeOneCharacter (objectNumber);
				return BR_CONTINUE;
			}

static builtReturn moveChr(int numParams, loadedFunction * fun, bool force, bool immediate)
{
			switch (numParams) {
				case 3:
				{
					int x, y, objectNumber;

					if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
					trimStack (fun -> stack);
					if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
					trimStack (fun -> stack);
					if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
					trimStack (fun -> stack);

					if (force) {
						if (forceWalkingPerson (x, y, objectNumber, fun, -1)) return BR_PAUSE;
					} else if (immediate) {
						jumpPerson (x, y, objectNumber);
					} else {
						if (makeWalkingPerson (x, y, objectNumber, fun, -1)) return BR_PAUSE;
					}
					return BR_CONTINUE;
				}

				case 2:
				{
					int toObj, objectNumber;
					screenRegion * reggie;

					if (! getValueType (toObj, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
					trimStack (fun -> stack);
					if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
					trimStack (fun -> stack);
					reggie = getRegionForObject (toObj);
					if (reggie == NULL) return BR_CONTINUE;

					if (force)
					{
						if (forceWalkingPerson (reggie -> sX, reggie -> sY,	objectNumber, fun, reggie -> di)) return BR_PAUSE;
					}
					else if (immediate)
					{
						jumpPerson (reggie -> sX, reggie -> sY,	objectNumber);
					}
					else
					{
						if (makeWalkingPerson (reggie -> sX, reggie -> sY, objectNumber, fun, reggie -> di)) return BR_PAUSE;
					}
					return BR_CONTINUE;
				}

				default:
				fatal ("Built-in function must have either 2 or 3 parameters.");
				return BR_NOCOMMENT;
			}
}

builtIn(moveCharacter)
{
	UNUSEDALL
	return moveChr(numParams, fun, false, false);
}

builtIn(forceCharacter)
{
	UNUSEDALL
	return moveChr(numParams, fun, true, false);
}

builtIn(jumpCharacter)
{
	UNUSEDALL
	return moveChr(numParams, fun, false, true);
}

builtIn(clearStatus)
{
	UNUSEDALL
			clearStatusBar ();
			return BR_CONTINUE;
}

builtIn(removeLastStatus)
{
	UNUSEDALL
			killLastStatus ();
			return BR_CONTINUE;
}

builtIn(addStatus)
{
	UNUSEDALL
			addStatusBar ();
			return BR_CONTINUE;
}

builtIn(statusText)
{
	UNUSEDALL
				char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
				if (! newText) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setStatusBar (newText);
				delete newText;
				return BR_CONTINUE;
			}

builtIn(lightStatus)
			{
	UNUSEDALL
				int val;
				if (! getValueType (val, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setLitStatus (val);
				return BR_CONTINUE;
}

builtIn(positionStatus)
{
	UNUSEDALL
				int x, y;
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				positionStatus (x, y);
				return BR_CONTINUE;
			}

builtIn(alignStatus)
			{
	UNUSEDALL
				int val;
				if (! getValueType (val, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				nowStatus -> alignStatus = (short) val;
				return BR_CONTINUE;
}

static bool getFuncNumForCallback(int numParams, loadedFunction * fun, int & functionNum)
{
				switch (numParams) {
					case 0:
					functionNum = 0;
					break;

					case 1:
					if (! getValueType (functionNum, SVT_FUNC, fun -> stack -> thisVar)) return false;
					trimStack (fun -> stack);
					break;

					default:
					fatal ("Too many parameters.");
					return false;
				}
				return true;
}

builtIn (onLeftMouse)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		currentEvents -> leftMouseFunction = functionNum;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (onLeftMouseUp)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		currentEvents -> leftMouseUpFunction = functionNum;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (onRightMouse)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		currentEvents -> rightMouseFunction = functionNum;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (onRightMouseUp)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		currentEvents -> rightMouseUpFunction = functionNum;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (onFocusChange)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		currentEvents -> focusFunction = functionNum;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (onMoveMouse)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		currentEvents -> moveMouseFunction = functionNum;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (onKeyboard)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		currentEvents -> spaceFunction = functionNum;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (spawnSub)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		if (! startNewFunctionNum (functionNum, 0, NULL, noStack)) return BR_NOCOMMENT;
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn (cancelSub)
{
	UNUSEDALL
	int functionNum;
	if (getFuncNumForCallback (numParams, fun, functionNum))
	{
		bool killedMyself;
		cancelAFunction (functionNum, fun, killedMyself);
		if (killedMyself) {
			abortFunction (fun);
			return BR_ALREADY_GONE;
		}
		return BR_CONTINUE;
	}
	return BR_NOCOMMENT;
}

builtIn(stringWidth)
{
	UNUSEDALL
			char * theText = getTextFromAnyVar (fun -> stack -> thisVar);
			if (! theText) return BR_NOCOMMENT;
			trimStack (fun -> stack);

			// Return value
			setVariable (fun -> reg, SVT_INT, stringWidth (theText));
			delete theText;
			return BR_CONTINUE;
}

builtIn(hardScroll)
{
	UNUSEDALL
			int v;
			if (! getValueType (v, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
			trimStack (fun -> stack);
			hardScroll (v);
			return BR_CONTINUE;
}

			//----------------------------
			// EXTRA ROOM BITS - Lightmap
			//----------------------------

builtIn(setLightMap)
{
	UNUSEDALL
			switch (numParams) {
				case 2:
				if (! getValueType (lightMapMode, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				lightMapMode %= LIGHTMAPMODE_NUM;
				// No break;

				case 1:
				if (fun -> stack -> thisVar.varType == SVT_FILE) {
					int v;
					getValueType (v, SVT_FILE, fun -> stack -> thisVar);
					trimStack (fun -> stack);
					if (! loadLightMap (v)) return BR_NOCOMMENT;
					setVariable (fun -> reg, SVT_INT, 1);
				} else {
					trimStack (fun -> stack);
					killLightMap ();
					setVariable (fun -> reg, SVT_INT, 0);
				}
				break;

				default:
				fatal ("Function should have either 2 or 3 parameters");
				return BR_NOCOMMENT;
			}
			return BR_CONTINUE;
}

builtIn(isScreenRegion)
			{
	UNUSEDALL
				int objectNumber;
				if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, getRegionForObject (objectNumber) != NULL);
				return BR_CONTINUE;
			}

builtIn(setSpeechSpeed)
			{
	UNUSEDALL
				int number;
				if (! getValueType (number, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				speechSpeed = number * 0.01;
				setVariable (fun -> reg, SVT_INT, 1);
				return BR_CONTINUE;
			}

builtIn(setFontSpacing)
			{
	UNUSEDALL
				int fontSpaceI;
				if (! getValueType (fontSpaceI, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				fontSpace = fontSpaceI;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, 1);
				return BR_CONTINUE;
			}

builtIn(transitionLevel)
			{
	UNUSEDALL
				int number;
				if (! getValueType (number, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				if (number < 0)
					brightnessLevel = 0;
				else if (number > 255)
					brightnessLevel = 255;
				else
					brightnessLevel = number;

				setVariable (fun -> reg, SVT_INT, 1);
				return BR_CONTINUE;
			}

builtIn(captureAllKeys)
			{
	UNUSEDALL
				captureAllKeys = getBoolean (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, captureAllKeys);
				return BR_CONTINUE;
			}


builtIn(spinCharacter)
			{
	UNUSEDALL
				int number, objectNumber;
				if (! getValueType (number, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				onScreenPerson * thisPerson = findPerson (objectNumber);
				if (thisPerson) {
					thisPerson -> wantAngle = number;
					thisPerson -> spinning = true;
					thisPerson -> continueAfterWalking = fun;
					setVariable (fun -> reg, SVT_INT, 1);
					return BR_PAUSE;
				} else {
					setVariable (fun -> reg, SVT_INT, 0);
					return BR_CONTINUE;
				}
}

builtIn(getCharacterDirection)
{
	UNUSEDALL
	int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	onScreenPerson * thisPerson = findPerson (objectNumber);
	if (thisPerson) {
		setVariable (fun -> reg, SVT_INT, thisPerson -> direction);
	} else {
		setVariable (fun -> reg, SVT_INT, 0);
	}
	return BR_CONTINUE;
}

builtIn(isCharacter)
{
	UNUSEDALL
	int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	onScreenPerson * thisPerson = findPerson (objectNumber);
	setVariable (fun -> reg, SVT_INT, thisPerson != NULL);
	return BR_CONTINUE;
}

builtIn(normalCharacter)
{
	UNUSEDALL
	int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	onScreenPerson * thisPerson = findPerson (objectNumber);
	if (thisPerson)
	{
		thisPerson -> myAnim = thisPerson -> myPersona -> animation[thisPerson -> direction];
		setVariable (fun -> reg, SVT_INT, 1);
	} else {
		setVariable (fun -> reg, SVT_INT, 0);
	}
	return BR_CONTINUE;
}

builtIn(isMoving)
{
	UNUSEDALL
	int objectNumber;
	if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
	trimStack (fun -> stack);
	onScreenPerson * thisPerson = findPerson (objectNumber);
	if (thisPerson)
	{
		setVariable (fun -> reg, SVT_INT, thisPerson -> walking);
	} else {
		setVariable (fun -> reg, SVT_INT, 0);
	}
	return BR_CONTINUE;
}

builtIn(fetchEvent)
{
	UNUSEDALL
				int obj1, obj2;
				if (! getValueType (obj2, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (obj1, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				int fNum = getCombinationFunction (obj1, obj2);

				// Return value
				if (fNum) {
					setVariable (fun -> reg, SVT_FUNC, fNum);
				} else {
					setVariable (fun -> reg, SVT_INT, 0);
				}
				return BR_CONTINUE;
			}

builtIn(deleteFile)
			{
	UNUSEDALL

				char * namNormal = getTextFromAnyVar (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				char * nam = encodeFilename (namNormal);
				delete namNormal;
				if (failSecurityCheck (nam)) return BR_NOCOMMENT;
				setVariable (fun -> reg, SVT_INT, remove (nam));
				delete nam;

				return BR_CONTINUE;
			}

builtIn(renameFile)
{
	UNUSEDALL
				char * temp;

				temp = getTextFromAnyVar (fun -> stack -> thisVar);
				char * newnam = encodeFilename (temp);
				trimStack (fun -> stack);
				if (failSecurityCheck (newnam)) return BR_NOCOMMENT;
				delete temp;

				temp = getTextFromAnyVar (fun -> stack -> thisVar);
				char * nam = encodeFilename (temp);
				trimStack (fun -> stack);
				if (failSecurityCheck (nam)) return BR_NOCOMMENT;
				delete temp;

				setVariable (fun -> reg, SVT_INT, rename (nam, newnam));
				delete nam;
				delete newnam;


			return BR_CONTINUE;
			}

builtIn(cacheSound)
			{
	UNUSEDALL
				int fileNumber;
				if (! getValueType (fileNumber, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (cacheSound (fileNumber) == -1) return BR_NOCOMMENT;
				return BR_CONTINUE;
			}

builtIn(burnString)
			{
	UNUSEDALL
				char * newText = getTextFromAnyVar (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				int y, x;
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (x == IN_THE_CENTRE) x = (winWidth - stringWidth (newText)) >> 1;
				fixFont (pastePalette);
				burnStringToBackdrop (newText, x, y, pastePalette);
				delete newText;
				return BR_CONTINUE;
			}

builtIn(setCharacterSpinSpeed)
			{
	UNUSEDALL
				int speed, who;
				if (! getValueType (speed, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (who, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				onScreenPerson * thisPerson = findPerson (who);

				if (thisPerson) {
					thisPerson -> spinSpeed = speed;
					setVariable (fun -> reg, SVT_INT, 1);
				} else {
					setVariable (fun -> reg, SVT_INT, 0);
				}
				return BR_CONTINUE;
			}

builtIn(setCharacterAngleOffset)
{
	UNUSEDALL
				int angle, who;
				if (! getValueType (angle, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (who, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				
				onScreenPerson * thisPerson = findPerson (who);
				
				if (thisPerson) {
					thisPerson -> angleOffset = angle;
					setVariable (fun -> reg, SVT_INT, 1);
				} else {
					setVariable (fun -> reg, SVT_INT, 0);
				}
				return BR_CONTINUE;
}


builtIn(transitionMode)
			{
	UNUSEDALL
				int n;
				if (! getValueType (n, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				fadeMode = n;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, 1);
				return BR_CONTINUE;
			}


// Deprecated function - does nothing
builtIn(_rem_updateDisplay)
{
	UNUSEDALL
			//updateDisplay = getBoolean (fun -> stack -> thisVar);
			trimStack (fun -> stack);
			setVariable (fun -> reg, SVT_INT, true /*updateDisplay*/);
			return BR_CONTINUE;
}

builtIn(getSoundCache)
{
	UNUSEDALL
				fun -> reg.varType = SVT_STACK;
				fun -> reg.varData.theStack = new stackHandler;
				if (! checkNew (fun -> reg.varData.theStack)) return BR_NOCOMMENT;
				fun -> reg.varData.theStack -> first = NULL;
				fun -> reg.varData.theStack -> last = NULL;
				fun -> reg.varData.theStack -> timesUsed = 1;
				if (! getSoundCacheStack (fun -> reg.varData.theStack)) return BR_NOCOMMENT;
			return BR_CONTINUE;
			}

builtIn(saveCustomData)

			{
	UNUSEDALL
				// saveCustomData (thisStack, fileName);
				char * fileNameB = getTextFromAnyVar (fun -> stack -> thisVar);
				if (! checkNew (fileNameB)) return BR_NOCOMMENT;

				char * fileName = encodeFilename (fileNameB);
				delete fileNameB;

				if (failSecurityCheck (fileName)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				if (fun -> stack -> thisVar.varType != SVT_STACK) {
					fatal ("First parameter isn't a stack");
					return BR_NOCOMMENT;
				}
				if (! stackToFile (fileName, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				delete fileName;
			return BR_CONTINUE;
			}

builtIn(loadCustomData)
			{
	UNUSEDALL

				char * newTextA = getTextFromAnyVar (fun -> stack -> thisVar);
				if (! checkNew (newTextA)) return BR_NOCOMMENT;

				char * newText = encodeFilename (newTextA);
				delete newTextA;

				if (failSecurityCheck (newText)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				unlinkVar (fun -> reg);
				fun -> reg.varType = SVT_STACK;
				fun -> reg.varData.theStack = new stackHandler;
				if (! checkNew (fun -> reg.varData.theStack)) return BR_NOCOMMENT;
				fun -> reg.varData.theStack -> first = NULL;
				fun -> reg.varData.theStack -> last = NULL;
				fun -> reg.varData.theStack -> timesUsed = 1;
				if (! fileToStack (newText, fun -> reg.varData.theStack)) return BR_NOCOMMENT;
				delete newText;
			return BR_CONTINUE;
			}

builtIn(setCustomEncoding)
			{
	UNUSEDALL
				int n;
				if (! getValueType (n, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				saveEncoding = n;
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, 1);
				return BR_CONTINUE;
			}

builtIn(freeSound)
			{
	UNUSEDALL
				int v;
				if (! getValueType (v, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				huntKillFreeSound (v);
				return BR_CONTINUE;
			}

builtIn(parallaxAdd)
{
	UNUSEDALL
			if (frozenStuff) {
				fatal ("Can't set background parallax image while frozen");
				return BR_NOCOMMENT;
			} else {
				int wrapX, wrapY, v;
				if (! getValueType (wrapY, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (wrapX, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (v, SVT_FILE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				if (! loadParallax (v, wrapX, wrapY)) return BR_NOCOMMENT;
				setVariable (fun -> reg, SVT_INT, 1);
			}
			return BR_CONTINUE;
}

builtIn(parallaxClear)
{
	UNUSEDALL
			killParallax ();
			setVariable (fun -> reg, SVT_INT, 1);
			return BR_CONTINUE;
}

builtIn(getPixelColour)
			{
	UNUSEDALL
				int x, y;
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				unlinkVar (fun -> reg);
				fun -> reg.varType = SVT_STACK;
				fun -> reg.varData.theStack = new stackHandler;
				if (! checkNew (fun -> reg.varData.theStack)) return BR_NOCOMMENT;
				fun -> reg.varData.theStack -> first = NULL;
				fun -> reg.varData.theStack -> last = NULL;
				fun -> reg.varData.theStack -> timesUsed = 1;
				if (! getRGBIntoStack (x, y, fun -> reg.varData.theStack)) return BR_NOCOMMENT;

				return BR_CONTINUE;
			}

builtIn(makeFastArray)
			{
	UNUSEDALL
				switch (fun -> stack -> thisVar.varType) {
					case SVT_STACK:
					{
						bool success = makeFastArrayFromStack (fun -> reg, fun -> stack -> thisVar.varData.theStack);
						trimStack (fun -> stack);
						return success ? BR_CONTINUE : BR_NOCOMMENT;
					}
					break;

					case SVT_INT:
					{
						int i = fun -> stack -> thisVar.varData.intValue;
						trimStack (fun -> stack);
						return makeFastArraySize (fun -> reg, i) ? BR_CONTINUE : BR_NOCOMMENT;
					}
					break;

					default:
					break;
				}
				fatal ("Parameter must be a number or a stack.");
				return BR_NOCOMMENT;
			}

builtIn(getCharacterScale)
{
	UNUSEDALL
				int objectNumber;
				if (! getValueType (objectNumber, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				onScreenPerson * pers = findPerson (objectNumber);
				if (pers) {
					setVariable (fun -> reg, SVT_INT, pers -> scale * 100);
				} else {
					setVariable (fun -> reg, SVT_INT, 0);
				}
				return BR_CONTINUE;
}

builtIn(getLanguageID)
			{
	UNUSEDALL
				setVariable (fun -> reg, SVT_INT, gameSettings.languageID);
				return BR_CONTINUE;
			}

// Deprecated function
builtIn(_rem_launchWith)
			{
	UNUSEDALL

				trimStack (fun -> stack);
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, false);

				return BR_CONTINUE;

			}

builtIn(getFramesPerSecond)
			{
	UNUSEDALL
				setVariable (fun -> reg, SVT_INT, lastFramesPerSecond);
				return BR_CONTINUE;
			}

builtIn(showThumbnail)
{
	UNUSEDALL
				int x, y;
				if (! getValueType (y, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (x, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);

				// Encode the name! Encode the name!
				char * aaaaa = getTextFromAnyVar (fun -> stack -> thisVar);
//				deb ("Got name:", aaaaa;)
				trimStack (fun -> stack);
//				deb ("About to encode", aaaaa);
				char * file = encodeFilename (aaaaa);
//				deb ("Made new name", file);
//				deb ("aaaaa is still ", aaaaa);
				delete aaaaa;
//				deb ("Deleted", "aaaaa");
				showThumbnail (file, x, y);
				delete file;
			return BR_CONTINUE;
			}

builtIn(setThumbnailSize)
{
	UNUSEDALL
			if (! getValueType (thumbHeight, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
			trimStack (fun -> stack);
			if (! getValueType (thumbWidth, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
			trimStack (fun -> stack);
			if (thumbWidth < 0 || thumbHeight < 0 || thumbWidth > winWidth || thumbHeight > winHeight) {
				char buff[50];
				sprintf (buff, "%d x %d", thumbWidth, thumbHeight);
				fatal ("Invalid thumbnail size", buff);
				return BR_NOCOMMENT;
			}
			return BR_CONTINUE;
}

builtIn(hasFlag)
{
	UNUSEDALL
				int objNum, flagIndex;
				if (! getValueType (flagIndex, SVT_INT, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				if (! getValueType (objNum, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
				trimStack (fun -> stack);
				objectType * objT = findObjectType (objNum);
				if (! objT) return BR_NOCOMMENT;
				setVariable (fun -> reg, SVT_INT, objT->flags & (1 << flagIndex));
				return BR_CONTINUE;
			}

builtIn(snapshotGrab)
{
	UNUSEDALL
			return snapshot () ? BR_CONTINUE : BR_NOCOMMENT;
}

builtIn(snapshotClear)
			{
	UNUSEDALL
			nosnapshot ();
			return BR_CONTINUE;
}

builtIn(bodgeFilenames)
			{
	UNUSEDALL
				bool lastValue = allowAnyFilename;
				allowAnyFilename = getBoolean (fun -> stack -> thisVar);
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, lastValue);
			return BR_CONTINUE;
			}

// Deprecated - does nothing.
builtIn(_rem_registryGetString)
			{
	UNUSEDALL
				trimStack (fun -> stack);
				trimStack (fun -> stack);
				setVariable (fun -> reg, SVT_INT, 0);

				return BR_CONTINUE;
			}

builtIn(quitWithFatalError)
{
	UNUSEDALL
	char * mess = getTextFromAnyVar (fun -> stack -> thisVar);
	trimStack (fun -> stack);
	fatal (mess);
	return BR_NOCOMMENT;
}

builtIn(setCharacterAntiAliasing)
{
	UNUSEDALL

	int who;
	aaSettingsStruct tempSettings;

	if (aaGetFromStack (& tempSettings, fun))
	{
		if (! getValueType (who, SVT_OBJTYPE, fun -> stack -> thisVar)) return BR_NOCOMMENT;
		trimStack (fun -> stack);

		onScreenPerson * thisPerson = findPerson (who);

		if (thisPerson) {
			aaCopy (& thisPerson->aaSettings, & tempSettings);
			setVariable (fun -> reg, SVT_INT, 1);
		} else {
			setVariable (fun -> reg, SVT_INT, 0);
		}
		return BR_CONTINUE;
	}
	else
	{
		return BR_NOCOMMENT;
	}
}

builtIn(setMaximumAntiAliasing)
{
	UNUSEDALL

	if (aaGetFromStack (& maxAntiAliasSettings, fun))
	{
		glBindTexture(GL_TEXTURE_2D, backdropTextureName);
		if (maxAntiAliasSettings.useMe) {
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		} else {
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
		return BR_CONTINUE;
	}
	else
	{
		return BR_NOCOMMENT;
	}
}

builtIn(setBackgroundEffect)
{
	UNUSEDALL
	bool done = blur_createSettings(numParams, fun->stack);
	setVariable (fun -> reg, SVT_INT, done ? 1 : 0);
	return BR_CONTINUE;
}

builtIn(doBackgroundEffect)
{
	UNUSEDALL
	bool done = blurScreen ();
	setVariable (fun -> reg, SVT_INT, done ? 1 : 0);
	return BR_CONTINUE;
}

#pragma mark -
#pragma mark Other functions

//-------------------------------------

#define FUNC(special,name)		{builtIn_ ## name},
static builtInFunctionData builtInFunctionArray[] =
{
	#include "functionlist.h"
};
#undef FUNC

#define FUNC(special,name)		{#name},
char builtInFunctionNames[][25] =
{
#include "functionlist.h"
};
#undef FUNC

#define NUM_FUNCS			(sizeof (builtInFunctionArray) / sizeof (builtInFunctionArray[0]))



builtReturn callBuiltIn (int whichFunc, int numParams, loadedFunction * fun) {
    //fprintf (stderr, "Calling function %d: %s\n", whichFunc, builtInFunctionNames[whichFunc]);    fflush (stderr);
	if (numBIFNames) {

//		deb ("IN:", (fun -> originalNumber < numUserFunc) ? allUserFunc[fun -> originalNumber] : "Unknown user function");
//		deb ("GO:", (whichFunc < numBIFNames) ? allBIFNames[whichFunc] : "Unknown built-in function");

		setFatalInfo (
			(fun -> originalNumber < numUserFunc) ? allUserFunc[fun -> originalNumber] : "Unknown user function",
			(whichFunc < numBIFNames) ? allBIFNames[whichFunc] : "Unknown built-in function");
	}

	if (whichFunc < NUM_FUNCS) {
		if (paramNum[whichFunc] != -1) {
			if (paramNum[whichFunc] != numParams) {
				char buff[100];
				sprintf (buff, "Built in function must have %i parameter%s",
							paramNum[whichFunc],
							(paramNum[whichFunc] == 1) ? "" : "s");

				fatal (copyString (buff));
				return BR_NOCOMMENT;
			}
		}
	}

	if (builtInFunctionArray[whichFunc].func)
	{
		//fprintf (stderr, "Calling %i: %s\n", whichFunc, builtInFunctionNames[whichFunc]);
		return builtInFunctionArray[whichFunc].func (numParams, fun);
	}

	fatal ("Unknown / unimplemented built-in function.");
	return BR_NOCOMMENT;
}

