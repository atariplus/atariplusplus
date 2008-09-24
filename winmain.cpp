#undef __STDC_WANT_SECURE_LIB__
#define __STDC_WANT_SECURE_LIB__ 0
#include <windows.h>
#include <stdio.h>

extern int main(int argc,char **argv);
extern "C" {
extern int  __cdecl _setargv(void);            /* setargv.c, stdargv.c */
extern int  __argc;
extern char **__argv;
}

int WINAPI WinMain (HINSTANCE,HINSTANCE,LPTSTR cmdlinestr,int)
{
	int rc;

	//
	// Yuck! Call some VS internals to get the standard arguments
	// parsed off for main. We don't get them in WinMain...
	_setargv();
	rc = main(__argc,__argv);
	return rc;
}

//
// Open a console window for stdio emulation and redirect
// streams into it.
void OpenConsole()
{
static bool isopen = false;

	if (!isopen) {
		HANDLE hdl;
		if (AllocConsole()) {
			hdl = GetStdHandle(STD_OUTPUT_HANDLE);
			hdl = GetStdHandle(STD_INPUT_HANDLE);
			freopen("CON","w",stdout);
			freopen("CON","w",stderr);
			freopen("CON","r",stdin);
			isopen = true;
		}
	}
}

