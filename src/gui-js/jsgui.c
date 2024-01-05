#include "dialog.h"
#include "sdlgui.h"

int SDLGui_Init(void) {
    return 0;
}

int SDLGui_UnInit(void) {
    return 0;
}

void SDLGui_GetFontSize(int *width, int *height) {
}

void SDLGui_Text(int x, int y, const char *txt) {
}

int Dialog_MainDlg(bool *bReset, bool *bLoadedSnapshot) {
    return 0;
}

bool DlgAlert_Query(const char *text) {
    printf("DlgAlert_Query: %s\n", text);
    return false;
}

void DlgMissing_Rom(const char* type, char *imgname, const char *defname, bool *enabled) {
    printf("DlgMissing_Rom: type=%s imgname=%s defname=%s enabled=%d\n", type, imgname, defname, *enabled);
    exit(1);
}

void DlgMissing_Dir(const char* type, char *dirname, const char *defname) {
    printf("DlgMissing_Dir: type=%s dirname=%s defname=%s\n", type, dirname, defname);
    exit(1);
}

void DlgMissing_Disk(const char* type, int num, char *imgname, bool *ins, bool *wp) {
    printf("DlgMissing_Disk: type=%s num=%d imgname=%s ins=%d wp=%d\n", type, num, imgname, *ins, *wp);
    exit(1);
}
