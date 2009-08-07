//
//  zDocument.m
//  Sludge Dev Kit
//
//  Created by Rikard Peterson on 2009-08-07.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

#import "zDocument.h"
#include "zBuffer.h"

@implementation zDocument

- (id)init
{
    self = [super init];
    if (self) {		
        // Add your subclass-specific initialization here.
        // If an error occurs here, send a [self release] message and return nil.
		buffer = 1;

		backdrop.total=0;
		backdrop.type=2;
		if (!reserveSpritePal (&backdrop.myPalette, 0)) {
			[self release];
			return nil;
		}
		
    }
    return self;
}

- (NSString *)windowNibName {
    // Implement this to return a nib to load OR implement -makeWindowControllers to manually create your controllers.
    return @"zDocument";
}

- (void)windowControllerDidLoadNib:(NSWindowController *) aController
{
	[zView connectToDoc: self];

	if (! [self fileURL]) {
		NSString *path = nil;
		NSOpenPanel *openPanel = [ NSOpenPanel openPanel ];
		[openPanel setTitle:@"Load file to zBuffer"];
		NSArray *files = [NSArray arrayWithObjects:@"tga", nil];
	
		if ( [ openPanel runModalForDirectory:nil file:nil types:files] ) {
			path = [ openPanel filename ];
			bool success = loadZBufferFromTGA ((char *) [path UTF8String], &backdrop);
			if (! success) {
				[self close];
				return;
			}
			[self updateChangeCount: NSChangeDone];
		} else {
			[self close];
			return;
		}
	}
	
    [super windowControllerDidLoadNib:aController];
	[zBufSlider setMaxValue:backdrop.total];
	[zBufSlider setNumberOfTickMarks:backdrop.total];
	[numBuffers setIntValue:backdrop.total];
	[self setBuffer:1];
}

- (BOOL)readFromURL:(NSURL *)absoluteURL 
			 ofType:(NSString *)typeName 
			  error:(NSError **)outError
{
	if ([typeName isEqualToString:@"SLUDGE zBuffer"]) {		
		UInt8 path[1024];
		if (CFURLGetFileSystemRepresentation((CFURLRef) absoluteURL, true, path, 1023)) {
			if (loadZBufferFile ((char *) path, &backdrop)) {
				return YES;
			}
		}
	} 
	*outError = [NSError errorWithDomain:@"Error" code:1 userInfo:nil];
	return NO;
}

- (BOOL)writeToURL:(NSURL *)absoluteURL 
			ofType:(NSString *)typeName 
			 error:(NSError **)outError
{
	if ([typeName isEqualToString:@"SLUDGE zBuffer"]) {		
		UInt8 path[1024];
		if (CFURLGetFileSystemRepresentation((CFURLRef) absoluteURL, true, path, 1023)) {
			if (saveZBufferFile ((char *) path, &backdrop)) {
				return YES;
			}
		}
	} 
	
	*outError = [NSError errorWithDomain:@"Error" code:1 userInfo:nil];
	return NO;
}

- (struct spriteBank *) getBackdrop {
	return &backdrop;
}

- (int) buffer
{
	return buffer;
}

- (void) setBuffer:(int)i
{
	// Validation shouldn't be done here, but I'm cheating.
	if (i < 1) i = 1;
	if (i > backdrop.total) i = backdrop.total;
	[bufferYTextField setIntValue: backdrop.sprites[i-1].tex_x];
	if (i > 1)
		[bufferYTextField setEnabled:YES];
	else
		[bufferYTextField setEnabled:NO];
	buffer = i;
	[zView setNeedsDisplay:YES];
}

- (IBAction)setBufferY:(id)sender
{
	backdrop.sprites[buffer-1].tex_x = [bufferYTextField intValue];
	[zView setNeedsDisplay:YES];
	[self updateChangeCount: NSChangeDone];
}

@end

@implementation zOpenGLView

- (void) connectToDoc: (id) myDoc
{
	doc = myDoc;
	backdrop = [doc getBackdrop];
}

- (void) setCoords {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(x+(-w/2)*zmul, x+(w/2)*zmul, y+(h/2)*zmul, y+(-h/2)*zmul, 1.0, -1.0);	
	glMatrixMode(GL_MODELVIEW);	
}

- (void)mouseDown:(NSEvent *)theEvent {
    BOOL keepOn = YES;
    NSPoint mouseLoc;
	NSPoint mouseLoc1;
	
	mouseLoc1 = [theEvent locationInWindow];
	
	int x1 = x;
	int y1 = y;
	
    while (keepOn) {
        theEvent = [[self window] nextEventMatchingMask: NSLeftMouseUpMask |
			NSLeftMouseDraggedMask];
        mouseLoc = [theEvent locationInWindow];
		
        switch ([theEvent type]) {
            case NSLeftMouseUp:
				keepOn = NO;
				// continue
            case NSLeftMouseDragged:
				x = x1 + (mouseLoc1.x - mouseLoc.x)*zmul;
				y = y1 + (mouseLoc.y - mouseLoc1.y)*zmul;
				[self setCoords];
				[self setNeedsDisplay:YES];
				
				break;
            default:
				/* Ignore any other kind of event. */
				break;
        }
    };
	
    return;
}

- (void)scrollWheel:(NSEvent *)theEvent
{
	
	z += [theEvent deltaY];
	if (z > 200.0) z = 200.0;
	if (z < -16.0) z = -16.0;
	
	zmul = (1.0+z/20);
	
	if ([theEvent deltaX]<0.0)
		[doc setBuffer: [doc buffer]+1];
	if ([theEvent deltaX]>0.0)
		[doc setBuffer: [doc buffer]-1];

	[self setCoords];
	[self setNeedsDisplay:YES];
}


- (void)prepareOpenGL 
{
	if (! backdrop) return;
	if (! backdrop->total) {
		addSprite(0, backdrop);
		backdrop->sprites[0].width = 640;
		backdrop->sprites[0].height = 480;
		backdrop->sprites[0].yhot = backdrop->sprites[0].height;
	} else
		loadZTextures (backdrop);

	z = 1.0;
	zmul = (1.0+z/20);
	[self setCoords];
}

- (void)reshape {
	NSRect bounds = [self bounds];
	if (! w) x = bounds.size.width / 2;
	if (! h) y = bounds.size.height / 2;
	h = bounds.size.height;
	w = bounds.size.width;
	glViewport (bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);	
	
	[self setCoords];
}

-(void) drawRect: (NSRect) bounds
{	
	if (! backdrop) return;
	
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
	
	int i;
	int b = [doc buffer]-1;
	
	if (backdrop->total>1)
		for (i=1; i< backdrop->total; i++) {
			if (i == b) {
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			} else 
				glColor4f(0.2f, 0.0f, 0.5f, 1.0f);
			
			pasteSprite (&backdrop->sprites[i], &backdrop->myPalette, false);
		}
	
	glDisable (GL_TEXTURE_2D);
	
		
    glColor3f(1.0f, 0.35f, 0.35f);
    glBegin(GL_LINE_LOOP);
    {
        glVertex3f(  0.0,  0.0, 0.0);
        glVertex3f(  backdrop->sprites[0].width,  0.0, 0.0);
        glVertex3f(  backdrop->sprites[0].width, -backdrop->sprites[0].height, 0.0);
        glVertex3f(  0.0, -backdrop->sprites[0].height, 0.0);
    }
    glEnd();

	if (b>0) {
		glColor4f(0.0f, 1.0f, 0.0f, 0.7f);
		glBegin(GL_LINES);
		{
			glVertex3f(  0.0,  backdrop->sprites[b].tex_x, 0.0);
			glVertex3f(  backdrop->sprites[0].width,  backdrop->sprites[b].tex_x, 0.0);
		}
		glEnd();
	}
	
	glFlush();
}


@end
