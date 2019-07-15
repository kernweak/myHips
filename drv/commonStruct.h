#pragma once
#ifndef __COMMONSTRUCT_H__
#define __COMMONSTRUCT_H__

//
//  Name of port used to communicate
//

const PWSTR FQDRVPortName = L"\\FQDRVPort";


#define FQDRV_READ_BUFFER_SIZE   1024
#define MAX_PATH 256
#define  MAXPATHLEN         512    
typedef struct _FQDRV_NOTIFICATION {
	ULONG Operation;
	WCHAR TargetPath[MAX_PATH];
	WCHAR ProcessPath[MAX_PATH];
	WCHAR RePathName[MAX_PATH];
} FQDRV_NOTIFICATION, *PFQDRV_NOTIFICATION;

typedef struct _FQDRV_REPLY {

	BOOLEAN SafeToOpen;

} FQDRV_REPLY, *PFQDRV_REPLY;

#endif //  __COMMONSTRUCT_H__