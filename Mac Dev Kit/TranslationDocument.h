//
//  TranslationDocument.h
//  Sludge Dev Kit
//
//  Created by Rikard Peterson on 2009-08-05.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface TranslationDocument : NSDocument {
	IBOutlet NSTextField *languageName;
	IBOutlet NSTextField *languageID;
	IBOutlet NSTableView *listOfStrings;

	IBOutlet NSTextField *originalString;

	IBOutlet NSPopUpButton *showThese;
	
	struct transLine * firstTransLine;
	char * langName;
	unsigned int langID;
	
//	int *needsTranslation;
//	struct transLine ** translation;
}
- (IBAction)loadStrings:(id)sender;
-(IBAction) showTheseChanged:(id)sender;
- (IBAction)changeDone:(id)sender;


@end
