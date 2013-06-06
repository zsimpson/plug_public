// @ZBS {
//		+DESCRIPTION {
//			Kill all processes of a given name
//		}
//		*MODULE_DEPENDS _killproc.cpp
//		*INTERFACE console
// }

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <psapi.h>


void main( int argc, char **argv ) {
	char *toKill = 0;
	if( argc == 1 ) {
		toKill = "iexplore.exe";
	}
	else {
		toKill = argv[1];
	}
	
	printf( "killing all: %s\n", toKill );

	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded );

	// Calculate how many process identifiers were returned.
	cProcesses = cbNeeded / sizeof(DWORD);

	// Print the name and process identifier for each process.
	for ( i = 0; i < cProcesses; i++ ) {
		if( aProcesses[i] != 0 ) {
			DWORD processID = aProcesses[i];

			TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

			// Get a handle to the process.
			HANDLE hProcess = OpenProcess( PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );

			// Get the process name.
			if( NULL != hProcess ) {
				HMODULE hMod;
				DWORD cbNeeded;

				if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &cbNeeded) ) {
					GetModuleBaseName( hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR) );
				}
			}

			// Print the process name and identifier.
			//_tprintf( TEXT("%s  (PID: %u)\n"), szProcessName, processID );
			
			BOOL a = 0;
			DWORD b = 0;
			if( !_tcscmp(szProcessName,toKill) ) {
				a = TerminateProcess( hProcess, 1 );
				if( a == 0 ) {
					b = GetLastError();
				}
			}

			CloseHandle( hProcess );

		}		          
	}
}
