/*++

Copyright (c) 1999-2002  Microsoft Corporation

Module Name:

	scrubber.h

Abstract:
	Header file which contains the structures, type definitions,
	constants, global variables and function prototypes that are
	only visible within the kernel.

Environment:

	Kernel mode

--*/
#ifndef __FQDRV_H__
#define __FQDRV_H__

///////////////////////////////////////////////////////////////////////////
//
//  Global variables
//
///////////////////////////////////////////////////////////////////////////
#define MAX_PATH 256
struct Data {             //应用层发来的 路径+操作命令 结构体
    ULONG command;
    WCHAR filename[MAX_PATH];

};
typedef struct Data Data;
typedef enum _IOMONITOR_COMMAND {  //操作命令
    DEFAULT_PATH,
    ADD_PATH,
    DELETE_PATH,
    CLOSE_PATH,
    OPEN_PATH,
	PAUSE_REGMON,
	RESTART_REGMON,
	DEFAULT_PROCESS,
	ADD_PROCESS,
	DELETE_PROCESS,
	PAUSE_PROCESS,
	RESTART_PROCESS,
	DEFAULT_MODULE,
	ADD_MODULE,
	DELETE_MODULE,
	PAUSE_MODULE,
	RESTART_MODULE,
} IOMonitorCommand;

typedef enum _result {  //操作命令
	ADD_SUCCESS,
	ADD_PATH_ALREADY_EXISTS,
	ADD_FAITH,
	DELETE_SUCCESS,
	DELETE_PATH_NOT_EXISTS,
	DELETE_FAITH,
	PAUSE_FILEMON,
	RESTART_FILEMON,
	MPAUSE_REGMON,
	MRESTART_REGMON,
	MPAUSE_PROCESS,
	MRESTART_PROCESS,
	MPAUSE_MODULE,
	MRESTART_MODULE,

} RuleResult;

typedef struct filenames {      //路径链表结点
    UNICODE_STRING filename;
    struct filenames* pNext;
}filenames,*pFilenames;

//void ModifyPathList(PUNICODE_STRING  filename);  //之前把添加和删除在一个函数实现的，这个路径存在就默认为删除命令，不存在就默认本次为添加命令（因为懒得解析结构体，只发一个路径），后来感觉还是不合适，就分开了



typedef struct _FQDRV_DATA {

	//
	//  The object that identifies this driver.
	//

	PDRIVER_OBJECT DriverObject;

	//
	//  The filter handle that results from a call to
	//  FltRegisterFilter.
	//

	PFLT_FILTER Filter;

	//
	//  Listens for incoming connections
	//

	PFLT_PORT ServerPort;

	//
	//  User process that connected to the port
	//

	PEPROCESS UserProcess;

	//
	//  Client port for a connection to user-mode
	//

	PFLT_PORT ClientPort;

} FQDRV_DATA, *PFQDRV_DATA;

extern FQDRV_DATA FQDRVData;

typedef struct _FQDRV_STREAM_HANDLE_CONTEXT {

	BOOLEAN RescanRequired;

} FQDRV_STREAM_HANDLE_CONTEXT, *PFQDRV_STREAM_HANDLE_CONTEXT;

#pragma warning(push)
#pragma warning(disable:4200) // disable warnings for structures with zero length arrays.

typedef struct _FQDRV_CREATE_PARAMS {

	WCHAR String[0];

} FQDRV_CREATE_PARAMS, *PFQDRV_CREATE_PARAMS;

#pragma warning(pop)


///////////////////////////////////////////////////////////////////////////
//
//  Prototypes for the startup and unload routines used for 
//  this Filter.
//
//  Implementation in FQDRV.c
//
///////////////////////////////////////////////////////////////////////////
DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT DriverObject,
	__in PUNICODE_STRING RegistryPath
);

NTSTATUS
FQDRVUnload(
	__in FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
FQDRVQueryTeardown(
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
FQDRVPreCreate(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
FQDRVPostCreate(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in_opt PVOID CompletionContext,
	__in FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
FQDRVPreCleanup(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
FQDRVPreWrite(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
);

NTSTATUS
FQDRVInstanceSetup(
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in FLT_INSTANCE_SETUP_FLAGS Flags,
	__in DEVICE_TYPE VolumeDeviceType,
	__in FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

FLT_PREOP_CALLBACK_STATUS
FQDRVPreSetInforMation(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS
FQDRVPostSetInforMation(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in_opt PVOID CompletionContext,
	__in FLT_POST_OPERATION_FLAGS Flags
);
NTSTATUS MessageNotifyCallback(
	IN PVOID PortCookie,
	IN PVOID InputBuffer OPTIONAL,
	IN ULONG InputBufferLength,
	OUT PVOID OutputBuffer OPTIONAL,
	IN ULONG OutputBufferLength,//用户可以接受的数据的最大长度.
	OUT PULONG ReturnOutputBufferLength);
ULONG AddPathList(PUNICODE_STRING  filename, pFilenames *headFilenames);
ULONG DeletePathList(PUNICODE_STRING  filename, pFilenames *headFilenames);
BOOLEAN searchRule(WCHAR *path, pFilenames *headFilenames);

//注册表相关

NTKERNELAPI UCHAR* PsGetProcessImageFileName(PEPROCESS Process);//获取进程的DOS文件路径
LARGE_INTEGER CmHandle;
NTSTATUS PtRegisterInit();
NTSTATUS PtRegisterUnInit();
BOOLEAN IsProcessName(char *string, PEPROCESS eprocess);
BOOLEAN GetRegistryObjectCompleteName(PUNICODE_STRING pRegistryPath, PUNICODE_STRING pPartialRegistryPath, PVOID pRegistryObject);
NTSTATUS RegistryCallback
(
	IN PVOID CallbackContext,
	IN PVOID Argument1,//操作类型，
	IN PVOID Argument2//操作的结构体指针
);


//进程相关
PWCHAR GetProcessNameByProcessId(HANDLE ProcessId);
VOID MyCreateProcessNotifyEx(
	__inout   PEPROCESS Process,
	__in      HANDLE ProcessId,
	__in_opt  PPS_CREATE_NOTIFY_INFO CreateInfo
);

VOID MyCreateProcessNotifyEx
(
	__inout   PEPROCESS Process,
	__in      HANDLE ProcessId,
	__in_opt  PPS_CREATE_NOTIFY_INFO CreateInfo
);
NTSTATUS PtProcessInit();
NTSTATUS PtProcessUnInit();
BOOLEAN searchProcessRule(WCHAR *path, pFilenames *headFilenames);


void DenyLoadDriver(PVOID DriverEntry);
PVOID GetDriverEntryByImageBase(PVOID ImageBase);
VOID LoadImageNotifyRoutine
(
	__in_opt PUNICODE_STRING  FullImageName,
	__in HANDLE  ProcessId,
	__in PIMAGE_INFO  ImageInfo
);
NTSTATUS PtModuleInit();
NTSTATUS PtModuleUnInit();
BOOLEAN searchModuleRule(WCHAR *path, pFilenames *headFilenames);
#endif /* __FQDRV_H__ */
