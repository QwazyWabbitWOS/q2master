
// **********************************************
//        Windows high performance counter
// **********************************************
// this version outputs to the debugger window
// instead of to the console log. 

#ifdef _WIN32
#ifdef _DEBUG

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>

#if _MSC_VER > 1500
#pragma warning(disable : 4996)	// disable warnings from VS 2010 about deprecated CRT functions (_CRT_SECURE_NO_WARNINGS).
#endif

LARGE_INTEGER start;
double totalTime = 0;

void _START_PERFORMANCE_TIMER (void)
{
	QueryPerformanceCounter (&start);
}

void _STOP_PERFORMANCE_TIMER (void)
{
	double res;
	LARGE_INTEGER stop;
	__int64 diff;
	LARGE_INTEGER freq;
	char string[64];

	QueryPerformanceCounter (&stop);
	QueryPerformanceFrequency (&freq);
	diff = stop.QuadPart - start.QuadPart;
	res = ((double)((double)diff / (double)freq.QuadPart));
	sprintf(string, "Function executed in %.6f secs.\n", res);
	OutputDebugString(string);
//	Com_Printf (string);
	totalTime += res;
}
#endif
#endif

