#ifndef NULLSOFT_WINAMP_IN2H_EXTRA
#define NULLSOFT_WINAMP_IN2H_EXTRA

/* stuff not found in SDKs that can be exported for Winamp */

//#include "out.h"
#include "in2.h"

__declspec(dllexport) In_Module * winampGetInModule2();

/* for Winamp 5.24 */
__declspec (dllexport) int winampGetExtendedFileInfo(char *filename, char *metadata, char *ret, int retlen);

/* for Winamp 5.3+ */
__declspec (dllexport) int winampGetExtendedFileInfoW(wchar_t *filename, char *metadata, wchar_t *ret, int retlen);

__declspec(dllexport) int winampUseUnifiedFileInfoDlg(const wchar_t * fn);
__declspec(dllexport) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param);

__declspec(dllexport) void* winampGetExtendedRead_open(const char *fn, int *size, int *bps, int *nch, int *srate);
__declspec(dllexport) void* winampGetExtendedRead_openW(const wchar_t *fn, int *size, int *bps, int *nch, int *srate);
__declspec(dllexport) size_t winampGetExtendedRead_getData(void *handle, char *dest, size_t len, int *killswitch);
__declspec(dllexport) int winampGetExtendedRead_setTime(void *handle, int time_in_ms);
__declspec(dllexport) void winampGetExtendedRead_close(void *handle);

/* other winamp sekrit exports: */
#if 0
winampGetExtendedRead_open_float
winampGetExtendedRead_openW_float
void winampAddUnifiedFileInfoPane
#endif

#endif
