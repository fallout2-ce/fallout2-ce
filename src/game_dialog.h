#ifndef GAME_DIALOG_H
#define GAME_DIALOG_H

#include "interpreter.h"
#include "obj_types.h"

namespace fallout {

extern Object* gGameDialogSpeaker;
extern bool gGameDialogSpeakerIsPartyMember;
extern int gGameDialogHeadFid;
extern int gGameDialogSid;

int gameDialogInit();
int gameDialogReset();
int gameDialogExit();
bool _gdialogActive();
void gameDialogEnter(Object* speaker, int mode);
void _gdialogSystemEnter();
void gameDialogStartLips(const char* audioFileName);
int gameDialogEnable();
int gameDialogDisable();
int _gdialogInitFromScript(int headFid, int reaction);
int _gdialogExitFromScript();
void gameDialogSetBackground(int background);
void gameDialogRenderSupplementaryMessage(const char* msg);
int _gdialogStart();
int _gdialogSayMessage();
int gameDialogAddMessageOptionWithProcIdentifier(int messageListId, int messageId, const char* procName, int reaction);
int gameDialogAddTextOptionWithProcIdentifier(int messageListId, const char* text, const char* procName, int reaction);
int gameDialogAddMessageOptionWithProc(int messageListId, int messageId, int proc, int reaction);
int gameDialogAddTextOptionWithProc(int messageListId, const char* text, int proc, int reaction);
int gameDialogSetMessageReply(Program* program, int messageListId, int messageId);
int gameDialogSetTextReply(Program* program, int messageListId, const char* text);
int _gdialogGo();
void _gdialogUpdatePartyStatus();
void _talk_to_critter_reacts(int reaction);
void gameDialogSetBarterModifier(int modifier);
int gameDialogBarter(int modifier);
void gameDialogEndBarter();

} // namespace fallout

#endif /* GAME_DIALOG_H */
