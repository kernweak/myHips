#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include "commonStruct.h"
#include "FQDrv.h"
#include "commonFun.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")
//注册表相关
#define REGISTRY_POOL_TAG 'pRE'

NTKERNELAPI NTSTATUS ObQueryNameString
(
	IN  PVOID Object,
	OUT POBJECT_NAME_INFORMATION ObjectNameInfo,
	IN  ULONG Length,
	OUT PULONG ReturnLength
);

NTKERNELAPI NTSTATUS RtlUnicodeStringCopy
(
	__out  PUNICODE_STRING DestinationString,
	__in   PUNICODE_STRING SourceString
);

//
//  Structure that contains all the global data structures
//  used throughout the FQDRV.
//
FQDRV_DATA FQDRVData;
UNICODE_STRING g_LastDelFileName = { 0 };

//
//  Function prototypes
//
pFilenames m_pfilenames=NULL;
ULONG isOpenFilter = 1;
ULONG isOpenReg = 1;
NTSTATUS
FQDRVPortConnect(
	__in PFLT_PORT ClientPort,
	__in_opt PVOID ServerPortCookie,
	__in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
	__in ULONG SizeOfContext,
	__deref_out_opt PVOID *ConnectionCookie
);

VOID
FQDRVPortDisconnect(
	__in_opt PVOID ConnectionCookie
);

NTSTATUS
FQDRVpScanFileInUserMode(
	__in PFLT_INSTANCE Instance,
	__in PFILE_OBJECT FileObject,
	__in PFLT_CALLBACK_DATA Data,
	__in ULONG		Operation,
	__out PBOOLEAN SafeToOpen
);

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FQDRVInstanceSetup)
#pragma alloc_text(PAGE, FQDRVPreCreate)
#pragma alloc_text(PAGE, FQDRVPostCreate)
#pragma alloc_text(PAGE, FQDRVPortConnect)
#pragma alloc_text(PAGE, FQDRVPortDisconnect)
#pragma alloc_text(PAGE, FQDRVPreSetInforMation)
#pragma alloc_text(PAGE, FQDRVPostSetInforMation)
#endif


//
//  Constant FLT_REGISTRATION structure for our filter.  This
//  initializes the callback routines our filter wants to register
//  for.  This is only used to register with the filter manager
//

const FLT_OPERATION_REGISTRATION Callbacks[] = {

	{ IRP_MJ_CREATE,
	  0,
	  FQDRVPreCreate,
	  FQDRVPostCreate},

	{ IRP_MJ_CLEANUP,
	  0,
	  FQDRVPreCleanup,
	  NULL},

	{ IRP_MJ_WRITE,
	  0,
	  FQDRVPreWrite,
	  NULL},

	{ IRP_MJ_SET_INFORMATION,
	  0,
	  FQDRVPreSetInforMation,
	  FQDRVPostSetInforMation},

	{ IRP_MJ_OPERATION_END}
};


const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {

	{ FLT_STREAMHANDLE_CONTEXT,
	  0,
	  NULL,
	  sizeof(FQDRV_STREAM_HANDLE_CONTEXT),
	  'chBS' },

	{ FLT_CONTEXT_END }
};

const FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),         //  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,                                  //  Flags
	ContextRegistration,                //  Context Registration.
	Callbacks,                          //  Operation callbacks
	FQDRVUnload,                      //  FilterUnload
	FQDRVInstanceSetup,               //  InstanceSetup
	FQDRVQueryTeardown,               //  InstanceQueryTeardown
	NULL,                               //  InstanceTeardownStart
	NULL,                               //  InstanceTeardownComplete
	NULL,                               //  GenerateFileName
	NULL,                               //  GenerateDestinationFileName
	NULL                                //  NormalizeNameComponent
};

////////////////////////////////////////////////////////////////////////////
//
//    Filter initialization and unload routines.
//
////////////////////////////////////////////////////////////////////////////

NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT DriverObject,
	__in PUNICODE_STRING RegistryPath
)
{
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING uniString;
	PSECURITY_DESCRIPTOR sd;
	NTSTATUS status;

	UNREFERENCED_PARAMETER(RegistryPath);

	g_LastDelFileName.Buffer = ExAllocatePool(NonPagedPool, MAX_PATH * 2);
	g_LastDelFileName.Length = g_LastDelFileName.MaximumLength = MAX_PATH * 2;
	memset(g_LastDelFileName.Buffer, '\0', MAX_PATH * 2);

	//
	//  Register with filter manager.
	//
	status = PtRegisterInit();
	if (!NT_SUCCESS(status))
	{
		PtRegisterUnInit();
	}
	status = FltRegisterFilter(DriverObject,
		&FilterRegistration,
		&FQDRVData.Filter);


	if (!NT_SUCCESS(status)) {

		return status;
	}

	//
	//  Create a communication port.
	//

	RtlInitUnicodeString(&uniString, FQDRVPortName);

	//
	//  We secure the port so only ADMINs & SYSTEM can acecss it.
	//

	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);//创建安全描述符，防止端口被非管理员用户打开

	if (NT_SUCCESS(status)) {

		InitializeObjectAttributes(&oa,
			&uniString,
			OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
			NULL,
			sd);

		status = FltCreateCommunicationPort(FQDRVData.Filter,
			&FQDRVData.ServerPort,
			&oa,
			NULL,
			FQDRVPortConnect,
			FQDRVPortDisconnect,
			MessageNotifyCallback,
			1);
		//
		//  Free the security descriptor in all cases. It is not needed once
		//  the call to FltCreateCommunicationPort() is made.
		//

		FltFreeSecurityDescriptor(sd);
		
		if (NT_SUCCESS(status)) {

			//
			//  Start filtering I/O.
			//

			status = FltStartFiltering(FQDRVData.Filter);

			if (NT_SUCCESS(status)) {

				return STATUS_SUCCESS;
			}

			FltCloseCommunicationPort(FQDRVData.ServerPort);
		}
	}

	FltUnregisterFilter(FQDRVData.Filter);

	return status;
}


NTSTATUS
FQDRVPortConnect(
	__in PFLT_PORT ClientPort,//应用层端口
	__in_opt PVOID ServerPortCookie,
	__in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
	__in ULONG SizeOfContext,
	__deref_out_opt PVOID *ConnectionCookie
)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie);

	ASSERT(FQDRVData.ClientPort == NULL);
	ASSERT(FQDRVData.UserProcess == NULL);

	//
	//  Set the user process and port.
	//

	FQDRVData.UserProcess = PsGetCurrentProcess();//应用层EPROCESS结构
	FQDRVData.ClientPort = ClientPort;

	DbgPrint("!!! FQDRV.sys --- connected, port=0x%p\n", ClientPort);

	return STATUS_SUCCESS;
}

VOID
FQDRVPortDisconnect(
	__in_opt PVOID ConnectionCookie
)
{
	UNREFERENCED_PARAMETER(ConnectionCookie);

	PAGED_CODE();

	DbgPrint("!!! FQDRV.sys --- disconnected, port=0x%p\n", FQDRVData.ClientPort);
	FltCloseClientPort(FQDRVData.Filter, &FQDRVData.ClientPort);
	FQDRVData.UserProcess = NULL;
}

NTSTATUS
FQDRVUnload(
	__in FLT_FILTER_UNLOAD_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Flags);

	//
	//  Close the server port.
	//
	if (g_LastDelFileName.Buffer)
		ExFreePool(g_LastDelFileName.Buffer);
	FltCloseCommunicationPort(FQDRVData.ServerPort);

	//
	//  Unregister the filter
	//

	FltUnregisterFilter(FQDRVData.Filter);
	PtRegisterUnInit();
	return STATUS_SUCCESS;
}

NTSTATUS
FQDRVInstanceSetup(
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in FLT_INSTANCE_SETUP_FLAGS Flags,
	__in DEVICE_TYPE VolumeDeviceType,
	__in FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);

	PAGED_CODE();

	ASSERT(FltObjects->Filter == FQDRVData.Filter);

	//
	//  Don't attach to network volumes.
	//

	if (VolumeDeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM) {

		return STATUS_FLT_DO_NOT_ATTACH;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
FQDRVQueryTeardown(
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	return STATUS_SUCCESS;
}


FLT_PREOP_CALLBACK_STATUS
FQDRVPreCreate(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	PAGED_CODE();

	//
	//  See if this create is being done by our user process.
	//

	if (IoThreadToProcess(Data->Thread) == FQDRVData.UserProcess) {//拿到发送创建请求的Eprocess结构

		DbgPrint("!!! FQDRV.sys -- allowing create for trusted process \n");

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}


FLT_POSTOP_CALLBACK_STATUS
FQDRVPostCreate(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in_opt PVOID CompletionContext,
	__in FLT_POST_OPERATION_FLAGS Flags
)
{
	//PFQDRV_STREAM_HANDLE_CONTEXT FQDRVContext;
	FLT_POSTOP_CALLBACK_STATUS returnStatus = FLT_POSTOP_FINISHED_PROCESSING;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	NTSTATUS status;
	BOOLEAN safeToOpen, scanFile;
	ULONG options;
	ULONG ulDisposition;
	BOOLEAN isPopWindow = FALSE;
	FILE_DISPOSITION_INFORMATION  fdi;

	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	//
	//  If this create was failing anyway, don't bother scanning now.
	//
	if (isOpenFilter) {
		if (!NT_SUCCESS(Data->IoStatus.Status) ||//打开不成功
			(STATUS_REPARSE == Data->IoStatus.Status)) {//文件是重定向的

			return FLT_POSTOP_FINISHED_PROCESSING;
		}

		options = Data->Iopb->Parameters.Create.Options;
		//下面判断
		if (FlagOn(options, FILE_DIRECTORY_FILE) ||//是目录
			FlagOn(FltObjects->FileObject->Flags, FO_VOLUME_OPEN) || //文件对象表示卷打开请求。
			FlagOn(Data->Flags, SL_OPEN_PAGING_FILE))//打开标识
		{
			return FLT_POSTOP_FINISHED_PROCESSING;
		}
		ulDisposition = (Data->Iopb->Parameters.Create.Options >> 24) & 0xFF;
		if (ulDisposition == FILE_CREATE || ulDisposition == FILE_OVERWRITE || ulDisposition == FILE_OVERWRITE_IF)
		{
			isPopWindow = TRUE;
		}

		//
		//  Check if we are interested in this file.
		//

		status = FltGetFileNameInformation(Data,//拿到文件名字
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&nameInfo);

		if (!NT_SUCCESS(status)) {

			return FLT_POSTOP_FINISHED_PROCESSING;
		}

		FltParseFileNameInformation(nameInfo);//w文件路径解析，扩展名，路径，等

		//scanFile = FQDRVpCheckExtension(&nameInfo->Extension);//看扩展名是不是需要的

		WCHAR tmp[256] = { 0 };
		wcsncpy_s(tmp, nameInfo->Name.Length, nameInfo->Name.Buffer, nameInfo->Name.Length);

		//DbgPrint(" tmp路径是%S\n", tmp);

		//scanFile = IsPatternMatch(L"\\*\\*\\WINDOWS\\SYSTEM32\\*\\*.*", tmp, TRUE);
		scanFile = searchRule(tmp);

		FltReleaseFileNameInformation(nameInfo);

		if (!scanFile) {
			return FLT_POSTOP_FINISHED_PROCESSING;
		}
		if (isPopWindow)
		{
			FQDRVpScanFileInUserMode(
				FltObjects->Instance,
				FltObjects->FileObject,
				Data,
				1,//1是创建
				&safeToOpen
			);

			if (!safeToOpen) {


				DbgPrint("拒绝创建操作\n");

				//就算拒绝了 也会创建一个空文件 这里我们删除
				fdi.DeleteFile = TRUE;
				FltSetInformationFile(FltObjects->Instance, FltObjects->FileObject, &fdi, sizeof(FILE_DISPOSITION_INFORMATION), FileDispositionInformation);
				FltCancelFileOpen(FltObjects->Instance, FltObjects->FileObject);

				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				Data->IoStatus.Information = 0;

				returnStatus = FLT_POSTOP_FINISHED_PROCESSING;

			}
		}

	}
	return returnStatus;
}


FLT_PREOP_CALLBACK_STATUS
FQDRVPreCleanup(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
FQDRVPreWrite(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
)
{
	FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Data);
	//
	//  If not client port just ignore this write.
	//

	if (FQDRVData.ClientPort == NULL) {

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}
	return returnStatus;
}

BOOLEAN isNeedWatchFile(PFLT_CALLBACK_DATA Data)
{
	BOOLEAN Ret = FALSE;
	//UNICODE_STRING ustrRule = { 0 };
	PFLT_FILE_NAME_INFORMATION nameInfo = { 0 };
	NTSTATUS status = STATUS_SUCCESS;
	status = FltGetFileNameInformation(Data,
		FLT_FILE_NAME_NORMALIZED |
		FLT_FILE_NAME_QUERY_DEFAULT,
		&nameInfo);
	if (!NT_SUCCESS(status)) {
		return FALSE;
	}
	FltParseFileNameInformation(nameInfo);

	WCHAR tmp[256] = { 0 };
	wcsncpy_s(tmp, nameInfo->Name.Length, nameInfo->Name.Buffer, nameInfo->Name.Length);
	//RtlInitUnicodeString(&ustrRule, L"\\*\\*\\WINDOWS\\SYSTEM32\\*\\*.SYS");
	//Ret = IsPatternMatch(L"\\*\\*\\WINDOWS\\SYSTEM32\\*\\*.*", tmp, TRUE);
	Ret=searchRule(tmp);
	FltReleaseFileNameInformation(nameInfo);
	return Ret;
}

BOOLEAN isRecycle(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObje)
{
	BOOLEAN Ret = FALSE;
	PFLT_FILE_NAME_INFORMATION nameInfo = { 0 };
	PFILE_RENAME_INFORMATION pRenameInfo = { 0 };
	NTSTATUS status = STATUS_SUCCESS;
	char *temp = (char*)ExAllocatePool(NonPagedPool, MAX_PATH * 2);
	if (temp == NULL)
		return TRUE;
	memset(temp, '\0', MAX_PATH * 2);
	//特殊情况,当字符串中包含$Recycle.Bin时是普通删除,实际上删除只是更名而已
	pRenameInfo = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
	status = FltGetDestinationFileNameInformation(FltObje->Instance, Data->Iopb->TargetFileObject, pRenameInfo->RootDirectory, pRenameInfo->FileName, pRenameInfo->FileNameLength, FLT_FILE_NAME_NORMALIZED, &nameInfo);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("FltGetDestinationFileNameInformation is faild! 0x%x", status);
		return TRUE;
	}
	UnicodeToChar(&nameInfo->Name, temp);
	if (strstr(temp, "Recycle.Bin"))
		Ret = TRUE;
	else
		Ret = FALSE;
	FltReleaseFileNameInformation(nameInfo);
	ExFreePool(temp);
	return Ret;
}


FLT_PREOP_CALLBACK_STATUS
FQDRVPreSetInforMation(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__deref_out_opt PVOID *CompletionContext
)
{
	FLT_PREOP_CALLBACK_STATUS status = FLT_PREOP_SUCCESS_NO_CALLBACK;
	ULONG Options = 0;//记录操作类型 1创建,2重命名,3删除
	BOOLEAN isAllow = TRUE;//是否放行
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	if (isOpenFilter) {
		//UNREFERENCED_PARAMETER(FltObjects);
		if (FQDRVData.ClientPort == NULL)
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
		if (FQDRVData.UserProcess == PsGetCurrentProcess())
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
		/*
				lpIrpStack->Parameters.SetFile.FileInformationClass == FileRenameInformation ||//重命名
				lpIrpStack->Parameters.SetFile.FileInformationClass == FileBasicInformation || //设置基础信息
				lpIrpStack->Parameters.SetFile.FileInformationClass == FileAllocationInformation ||
				lpIrpStack->Parameters.SetFile.FileInformationClass == FileEndOfFileInformation ||//设置大小
				lpIrpStack->Parameters.SetFile.FileInformationClass == FileDispositionInformation)//删除
		*/
		if (Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformation ||
			Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformation)
		{
			switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
			{
			case FileRenameInformation:
				Options = 2;
				break;
			case FileDispositionInformation:
				Options = 3;
				break;
			default:
				Options = 0;//爆炸啦
				break;
			}
			//判断是不是我们要监控的
			if (!isNeedWatchFile(Data))
			{
				return FLT_PREOP_SUCCESS_NO_CALLBACK;
			}
			if (Options == 2)
			{
				if (isRecycle(Data, FltObjects))
				{
					return FLT_PREOP_SUCCESS_NO_CALLBACK;
				}
			}
			//进程路径,操作类型,原路径,重命名后路径
			FQDRVpScanFileInUserMode(FltObjects->Instance, FltObjects->FileObject, Data, Options, &isAllow);
			if (!isAllow)
			{
				DbgPrint("ReName in PreSetInforMation被拒绝 !\n");
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				Data->IoStatus.Information = 0;
				status = FLT_PREOP_COMPLETE;
			}
			else
			{
				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			}
		}
	}
	return status;
}

FLT_POSTOP_CALLBACK_STATUS
FQDRVPostSetInforMation(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in_opt PVOID CompletionContext,
	__in FLT_POST_OPERATION_FLAGS Flags
)
{
	//FLT_POSTOP_CALLBACK_STATUS status = FLT_POSTOP_FINISHED_PROCESSING;
	//ULONG Options = 0;//记录操作类型 1创建,2重命名,3删除
	//BOOLEAN isAllow = TRUE;//是否放行
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	return FLT_POSTOP_FINISHED_PROCESSING;
}
//////////////////////////////////////////////////////////////////////////
//  Local support routines.
//
/////////////////////////////////////////////////////////////////////////
//操作类型 1创建 2重命名 3 删除
NTSTATUS
FQDRVpScanFileInUserMode(
	__in PFLT_INSTANCE Instance,
	__in PFILE_OBJECT FileObject,
	__in PFLT_CALLBACK_DATA Data,
	__in ULONG		Operation,
	__out PBOOLEAN SafeToOpen
)
{
	NTSTATUS status = STATUS_SUCCESS;

	PFQDRV_NOTIFICATION notification = NULL;

	ULONG replyLength = 0;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	PFLT_FILE_NAME_INFORMATION pOutReNameinfo;
	PFILE_RENAME_INFORMATION pRenameInfo;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(Instance);
	*SafeToOpen = TRUE;

	//
	//  If not client port just return.
	//

	if (FQDRVData.ClientPort == NULL) {

		return STATUS_SUCCESS;
	}

	try {
		notification = ExAllocatePoolWithTag(NonPagedPool,
			sizeof(FQDRV_NOTIFICATION),
			'nacS');

		if (NULL == notification)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			leave;
		}

		//在这里获取进程路径,操作类型,目标路径
		//拷贝操作类型

		notification->Operation = Operation;


		status = FltGetFileNameInformation(Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&nameInfo);
		//拷贝进程路径
		//wcsncpy(notification->ProcessPath, uSProcessPath->Buffer, uSProcessPath->Length);
		if (!NT_SUCCESS(status)) {

			status = STATUS_INSUFFICIENT_RESOURCES;
			leave;
		}

		//拷贝目标路径
		FltParseFileNameInformation(nameInfo);
		//wcsncpy(notification->ProcessPath, nameInfo->Name.Buffer, nameInfo->Name.Length);
		//12345
		WCHAR tmp[MAX_PATH] = { 0 };
		wcsncpy_s(tmp, nameInfo->Name.Length, nameInfo->Name.Buffer, nameInfo->Name.Length);
		
		GetNTLinkName(tmp,notification->ProcessPath);
		DbgPrint("notification->ProcessPath is %ls", notification->ProcessPath);
		FltReleaseFileNameInformation(nameInfo);
		//wcsncpy(¬ification->ProcessPath,L"test",wcslen(L"test"));


		status = FltGetFileNameInformation(Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&nameInfo);
		//FltGetDestinationFileNameInformation(

		if (!NT_SUCCESS(status)) {

			status = STATUS_INSUFFICIENT_RESOURCES;
			leave;
		}



		//拷贝目标路径
		FltParseFileNameInformation(nameInfo);

		//这里应该注意下多线程的
		if (Operation == 3)
		{
			//if (wcsncmp(g_LastDelFileName.Buffer, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength) == 0)
			//{
			//	FltReleaseFileNameInformation(nameInfo);
			//	memset(g_LastDelFileName.Buffer, '\0', MAX_PATH * 2);
			//	*SafeToOpen = TRUE;
			//	leave;
			//}

		}

		if (Operation == 3)
		{
			wcsncpy(g_LastDelFileName.Buffer, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);
		}
		//12345
		//wcsncpy(notification->TargetPath, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);
		WCHAR tmp3[MAX_PATH] = { 0 };
		wcsncpy_s(tmp3, nameInfo->Name.Length, nameInfo->Name.Buffer, nameInfo->Name.Length);

		GetNTLinkName(tmp3,notification->TargetPath);
		DbgPrint("notification->TargetPath is %ls", notification->TargetPath);

		FltReleaseFileNameInformation(nameInfo);

		if (Operation == 2)//重命名
		{
			pRenameInfo = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
			status = FltGetDestinationFileNameInformation(Instance, Data->Iopb->TargetFileObject, pRenameInfo->RootDirectory, pRenameInfo->FileName, pRenameInfo->FileNameLength, FLT_FILE_NAME_NORMALIZED, &pOutReNameinfo);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("FltGetDestinationFileNameInformation is faild! 0x%x", status);
				leave;
			}
			//wcsncpy(notification->RePathName, pOutReNameinfo->Name.Buffer, pOutReNameinfo->Name.MaximumLength);
			//12345
			WCHAR tmp1[MAX_PATH] = { 0 };
			wcsncpy_s(tmp1, pOutReNameinfo->Name.Length, pOutReNameinfo->Name.Buffer, pOutReNameinfo->Name.Length);
			GetNTLinkName(tmp1,notification->RePathName);
			DbgPrint("notification->RePathName is %ls", notification->RePathName);

			DbgPrint("重命名：%wZ\n", &pOutReNameinfo->Name);

			FltReleaseFileNameInformation(pOutReNameinfo);
		}

		replyLength = sizeof(FQDRV_REPLY);

		status = FltSendMessage(FQDRVData.Filter,
			&FQDRVData.ClientPort,
			notification,
			sizeof(FQDRV_NOTIFICATION),
			notification,
			&replyLength,
			NULL);

		if (STATUS_SUCCESS == status) {

			*SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;

		}
		else {

			//
			//  Couldn't send message
			//

			DbgPrint("!!! FQDRV.sys --- couldn't send message to user-mode to scan file, status 0x%X\n", status);
		}

	}
	finally{

	 if (NULL != notification) {

		 ExFreePoolWithTag(notification, 'nacS');
	 }

	}

	return status;
}

NTSTATUS MessageNotifyCallback(
	IN PVOID PortCookie,
	IN PVOID InputBuffer OPTIONAL,
	IN ULONG InputBufferLength,
	OUT PVOID OutputBuffer OPTIONAL,
	IN ULONG OutputBufferLength,//用户可以接受的数据的最大长度.
	OUT PULONG ReturnOutputBufferLength)
{
	NTSTATUS status = 0;
	WCHAR buffer[MAX_PATH] = { 0 };
	PAGED_CODE();

	ULONG level = KeGetCurrentIrql();
	RuleResult uResult = 0;
	UNREFERENCED_PARAMETER(PortCookie);
	Data *data = (Data*)InputBuffer;
	WCHAR rulePath[MAX_PATH] = { 0 };
	GetNtDeviceName(data->filename, rulePath);
	DbgPrint("用户发来的信息是:%ls,GetNtDeviceName之后是%ls\n", data->filename,rulePath);


	IOMonitorCommand command;
	command = data->command;
	WCHAR* p = rulePath;
	//WCHAR* p = data->filename;
	WCHAR cachePathTemp[MAX_PATH] = { 0 };
	ULONG i = 0;
	while (*p) {
		cachePathTemp[i] = *p;
		i++, p++;
	}
	KdPrint(("cachePathTemp是:%ls\n", cachePathTemp));
	////cachePathTemp[i] = 0;
	//
	//申请内存构造字符串
	UNICODE_STRING cachePath = { 0 };
	ULONG ulLength = (i) * sizeof(WCHAR);
	__try {
		cachePath.Buffer = ExAllocatePoolWithTag(PagedPool, MAX_PATH * sizeof(WCHAR), 'PMET');
		RtlZeroMemory(cachePath.Buffer, MAX_PATH * sizeof(WCHAR));
		cachePath.Length = ulLength;
		cachePath.MaximumLength = MAX_PATH*sizeof(WCHAR);
		wcscpy_s(cachePath.Buffer, ulLength, cachePathTemp);
		
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		ExFreePool(cachePath.Buffer);
		cachePath.Buffer = NULL;
		cachePath.Length = cachePath.MaximumLength = 0;
		return status;
	}
	DbgPrint("构建好的是%wZ\n", cachePath);
	
	switch (command)
	{
	case DEFAULT_PATH:
		uResult=AddPathList(&cachePath);
		break;
	case ADD_PATH:
		uResult=AddPathList(&cachePath);
		break;
	case DELETE_PATH:
		uResult = DeletePathList(&cachePath);
		break;
	case CLOSE_PATH:
		isOpenFilter = 0;
		uResult= PAUSE_FILEMON;
		break;
	case OPEN_PATH:
		isOpenFilter = 1;
		break;
	case PAUSE_REGMON:
		isOpenReg = 0;
		uResult = MPAUSE_REGMON;
		break;
	case RESTART_REGMON:
		isOpenReg = 1;
		uResult = MRESTART_REGMON;
		break;
	default:
		break;
	}
	
	////释放内存
	//ExFreePool(cachePath.Buffer);
	//cachePath.Buffer = NULL;
	//cachePath.Length = cachePath.MaximumLength = 0;
	//
	////打印用户发来的信息
	//KdPrint(("用户发来的信息是:%ls\n", rulePath));
	////返回用户一些信息.
	//ReturnOutputBufferLength = sizeof(buffer);
	//RtlCopyMemory(OutputBuffer, buffer, *ReturnOutputBufferLength);
	//释放内存
	
	ExFreePool(cachePath.Buffer);
	cachePath.Buffer = NULL;
	cachePath.Length = cachePath.MaximumLength = 0;
	//返回用户信息
	switch (uResult)
	{
	case ADD_SUCCESS:
		wcscpy_s(buffer, wcslen(L"ADD_SUCCESS")+1,L"ADD_SUCCESS");
		break;
	case ADD_PATH_ALREADY_EXISTS:
		wcscpy_s(buffer, wcslen(L"ADD_PATH_ALREADY_EXISTS") + 1, L"ADD_PATH_ALREADY_EXISTS");
		break;
	case ADD_FAITH:
		wcscpy_s(buffer, wcslen(L"ADD_FAITH") + 1, L"ADD_FAITH");
		break;
	case DELETE_SUCCESS:
		wcscpy_s(buffer, wcslen(L"DELETE_SUCCESS") + 1, L"DELETE_SUCCESS");
		break;
	case DELETE_PATH_NOT_EXISTS:
		wcscpy_s(buffer, wcslen(L"DELETE_PATH_NOT_EXISTS") + 1, L"DELETE_PATH_NOT_EXISTS");
		break;
	case DELETE_FAITH:
		wcscpy_s(buffer, wcslen(L"DELETE_FAITH") + 1, L"DELETE_FAITH");
	case PAUSE_FILEMON:
		wcscpy_s(buffer, wcslen(L"PAUSE_FILEMON") + 1, L"PAUSE_FILEMON");
	case RESTART_FILEMON:
		wcscpy_s(buffer, wcslen(L"RESTART_FILEMON") + 1, L"RESTART_FILEMON");
		break;
	case MPAUSE_REGMON:
		wcscpy_s(buffer, wcslen(L"PAUSE_REGMON") + 1, L"PAUSE_REGMON");
		break;
	case MRESTART_REGMON:
		wcscpy_s(buffer, wcslen(L"RESTART_REGMON") + 1, L"RESTART_REGMON");
		break;
	default:
		break;
	}
	ReturnOutputBufferLength = (wcslen(buffer) + 1) * sizeof(WCHAR);
	RtlCopyMemory(OutputBuffer, buffer, (wcslen(buffer) + 1) * sizeof(WCHAR));
	return status;
}


ULONG AddPathList(PUNICODE_STRING  filename)
{
	filenames * new_filename, *current, *precurrent;
	new_filename = ExAllocatePool(NonPagedPool, sizeof(filenames));
	new_filename->filename.Buffer = (PWSTR)ExAllocatePool(NonPagedPool, filename->MaximumLength);
	new_filename->filename.MaximumLength = filename->MaximumLength;
	__try {
		RtlCopyUnicodeString(&new_filename->filename, filename);

		new_filename->pNext = NULL;
		if (NULL == m_pfilenames)              //头是空的，路径添加到头
		{
			m_pfilenames = new_filename;
			DbgPrint("插入成功，头是空的，路径添加到头,插入的是%wZ\n", new_filename->filename);
			return ADD_SUCCESS;
		}
		current = m_pfilenames;
		while (current != NULL)
		{
			if (RtlEqualUnicodeString(&new_filename->filename, &current->filename, TRUE))
				//链表中含有这个路径，返回
			{
				RtlFreeUnicodeString(&new_filename->filename);
				ExFreePool(new_filename);
				new_filename = NULL;
				return ADD_PATH_ALREADY_EXISTS;
			}
			precurrent = current;
			current = current->pNext;
		}
		//链表中没有这个路径，添加
		current = m_pfilenames;
		while (current->pNext != NULL)
		{
			current = current->pNext;
		}
		current->pNext = new_filename;
		DbgPrint("插入成功,插入的是%wZ\n", new_filename->filename);
		return ADD_SUCCESS;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		RtlFreeUnicodeString(&new_filename->filename);
		ExFreePool(new_filename);
		new_filename = NULL;
		return ADD_FAITH;
	}
}
ULONG DeletePathList(PUNICODE_STRING  filename)
{
	filenames  *current, *precurrent;
	current = precurrent = m_pfilenames;
	while (current != NULL)
	{
		__try {
			if (RtlEqualUnicodeString(filename, &current->filename, TRUE))
			{
				//判断一下是否是头,如果是头，就让头指向第二个，删掉第一个
				if (current == m_pfilenames)
				{
					DbgPrint("删除成功，删除的是头结点是%wZ\n", current->filename);
					m_pfilenames = current->pNext;
					RtlFreeUnicodeString(&current->filename);
					
					ExFreePool(current);
					return DELETE_SUCCESS;
				}
				//如果不是头，删掉当前的
				DbgPrint("删除成功，删除的不是头结点%wZ\n", current->filename);
				precurrent->pNext = current->pNext;
				current->pNext = NULL;
				RtlFreeUnicodeString(&current->filename);
				
				ExFreePool(current);
				
				return DELETE_SUCCESS;
			}
			precurrent = current;
			current = current->pNext;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return DELETE_FAITH;
		}
	}
	return DELETE_PATH_NOT_EXISTS;
}

BOOLEAN searchRule(WCHAR *path)
{
	filenames *current;
	current =  m_pfilenames;
	WCHAR tmpPath[MAX_PATH]={0};
	while (current != NULL)
	{
		__try {
			WCHAR tmp[MAX_PATH] = { 0 };
			wcsncpy_s(tmp, current->filename.Length, current->filename.Buffer, current->filename.Length);
			ToUpperString(tmp);
			//DbgPrint("tmp is %ls,path is %ls", tmp, path);
			if (IsPatternMatch(tmp, path, TRUE))
			{
				return TRUE;
			}
			current = current->pNext;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return FALSE;
		}
	}
	return FALSE;
}


NTSTATUS PtRegisterInit()
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	status = CmRegisterCallback(RegistryCallback, NULL, &CmHandle);
	if (NT_SUCCESS(status))
		DbgPrint("CmRegisterCallback SUCCESS!");
	else
		DbgPrint("CmRegisterCallback Failed!");
	return status;

}
NTSTATUS PtRegisterUnInit()
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	status=CmUnRegisterCallback(CmHandle);
	if (NT_SUCCESS(status))
		DbgPrint("CmUnRegisterCallback SUCCESS!");
	else
		DbgPrint("CmUnRegisterCallback Failed!");
	return status;
}

BOOLEAN IsProcessName(char *string, PEPROCESS eprocess)
{
	char xx[260] = { 0 };
	strcpy(xx, PsGetProcessImageFileName(eprocess));
	if (!_stricmp(xx, string))
		return TRUE;
	else
		return FALSE;
}

BOOLEAN GetRegistryObjectCompleteName(PUNICODE_STRING pRegistryPath, PUNICODE_STRING pPartialRegistryPath, PVOID pRegistryObject)
{
	BOOLEAN foundCompleteName = FALSE;
	BOOLEAN partial = FALSE;
	if ((!MmIsAddressValid(pRegistryObject)) || (pRegistryObject == NULL))
		return FALSE;
	/* Check to see if the partial name is really the complete name */
	if (pPartialRegistryPath != NULL)
	{
		if ((((pPartialRegistryPath->Buffer[0] == '\\') || (pPartialRegistryPath->Buffer[0] == '%')) ||
			((pPartialRegistryPath->Buffer[0] == 'T') && (pPartialRegistryPath->Buffer[1] == 'R') &&
			(pPartialRegistryPath->Buffer[2] == 'Y') && (pPartialRegistryPath->Buffer[3] == '\\'))))
		{
			RtlCopyUnicodeString(pRegistryPath, pPartialRegistryPath);
			partial = TRUE;
			foundCompleteName = TRUE;
		}
	}
	if (!foundCompleteName)
	{
		/* Query the object manager in the kernel for the complete name */
		NTSTATUS status;
		ULONG returnedLength;
		PUNICODE_STRING pObjectName = NULL;
		status = ObQueryNameString(pRegistryObject, (POBJECT_NAME_INFORMATION)pObjectName, 0, &returnedLength);
		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			pObjectName = ExAllocatePoolWithTag(NonPagedPool, returnedLength, REGISTRY_POOL_TAG);
			status = ObQueryNameString(pRegistryObject, (POBJECT_NAME_INFORMATION)pObjectName, returnedLength, &returnedLength);
			if (NT_SUCCESS(status))
			{
				RtlCopyUnicodeString(pRegistryPath, pObjectName);
				foundCompleteName = TRUE;
			}
			ExFreePoolWithTag(pObjectName, REGISTRY_POOL_TAG);
		}
	}
	return foundCompleteName;
}

NTSTATUS RegistryCallback
(
	IN PVOID CallbackContext,
	IN PVOID Argument1,//操作类型，
	IN PVOID Argument2//操作的结构体指针
)
{
	if (!isOpenReg)
	{
		return STATUS_SUCCESS;
	}
	ULONG Options = 0;//记录操作类型 4创建,5删除,6设置键值,7删除键值，8重命名键
	BOOLEAN isAllow = TRUE;//是否放行
	PFQDRV_NOTIFICATION notification = NULL;
	ULONG replyLength = 0;
	BOOLEAN SafeToOpen;
	SafeToOpen = TRUE;
	NTSTATUS status = STATUS_SUCCESS;

	long type;
	NTSTATUS CallbackStatus = STATUS_SUCCESS;
	UNICODE_STRING registryPath;
	registryPath.Length = 0;
	registryPath.MaximumLength = 2048 * sizeof(WCHAR);
	registryPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, registryPath.MaximumLength, REGISTRY_POOL_TAG);


	notification = ExAllocatePoolWithTag(NonPagedPool,
		sizeof(FQDRV_NOTIFICATION),
		'nacS');

	if (NULL == notification)
	{
		CallbackStatus = STATUS_INSUFFICIENT_RESOURCES;
		return CallbackStatus;
	}


	if (registryPath.Buffer == NULL)
		return STATUS_SUCCESS;
	type = (REG_NOTIFY_CLASS)Argument1;
	switch (type)
	{
	case RegNtPreCreateKeyEx:	//出现两次是因为一次是OpenKey，一次是createKey
	{
		if (IsProcessName("regedit.exe", PsGetCurrentProcess()))
		{
			GetRegistryObjectCompleteName(&registryPath, NULL, ((PREG_CREATE_KEY_INFORMATION)Argument2)->RootObject);
			DbgPrint("[RegNtPreCreateKeyEx]KeyPath: %wZ", &registryPath);	//新键的路径
			DbgPrint("[RegNtPreCreateKeyEx]KeyName: %wZ",
				((PREG_CREATE_KEY_INFORMATION)Argument2)->CompleteName);//新键的名称

			notification->Operation = 4;
			char newPath[MAX_PATH] = { 0 };
			WCHAR newPath1[MAX_PATH] = { 0 };
			UnicodeToChar(&registryPath, newPath);
			CharToWchar(newPath, newPath1);
			wcscpy_s(notification->ProcessPath, MAX_PATH, newPath1);
			
			char newName[MAX_PATH] = { 0 };
			WCHAR newName1[MAX_PATH] = { 0 };
			UnicodeToChar(((PREG_CREATE_KEY_INFORMATION)Argument2)->CompleteName, newName);
			CharToWchar(newName, newName1);
			wcscpy_s(notification->TargetPath, MAX_PATH, newName1);
			replyLength = sizeof(FQDRV_REPLY);
			status = FltSendMessage(FQDRVData.Filter,
				&FQDRVData.ClientPort,
				notification,
				sizeof(FQDRV_NOTIFICATION),
				notification,
				&replyLength,
				NULL);
			if (STATUS_SUCCESS == status) {
			
				SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;
				if (SafeToOpen)
				{
					CallbackStatus = STATUS_SUCCESS;
				}
				else {
					CallbackStatus = STATUS_ACCESS_DENIED;
				}
			}

			//CallbackStatus = STATUS_ACCESS_DENIED;
		}
		break;
	}
	case RegNtPreDeleteKey:
	{
		if (IsProcessName("regedit.exe", PsGetCurrentProcess()))
		{
			GetRegistryObjectCompleteName(&registryPath, NULL, ((PREG_DELETE_KEY_INFORMATION)Argument2)->Object);
			DbgPrint("[RegNtPreDeleteKey]%wZ", &registryPath);				//新键的路径

			notification->Operation = 5;
			char newPath[MAX_PATH] = { 0 };
			WCHAR newPath1[MAX_PATH] = { 0 };
			UnicodeToChar(&registryPath, newPath);
			CharToWchar(newPath, newPath1);
			wcscpy_s(notification->ProcessPath, MAX_PATH, newPath1);

			replyLength = sizeof(FQDRV_REPLY);
			status = FltSendMessage(FQDRVData.Filter,
				&FQDRVData.ClientPort,
				notification,
				sizeof(FQDRV_NOTIFICATION),
				notification,
				&replyLength,
				NULL);
			if (STATUS_SUCCESS == status) {

				SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;
				if (SafeToOpen)
				{
					CallbackStatus = STATUS_SUCCESS;
				}
				else {
					CallbackStatus = STATUS_ACCESS_DENIED;
				}
			}

			//CallbackStatus = STATUS_ACCESS_DENIED;
		}
		break;
	}
	case RegNtPreSetValueKey:
	{
		if (IsProcessName("regedit.exe", PsGetCurrentProcess()))
		{
			GetRegistryObjectCompleteName(&registryPath, NULL, ((PREG_SET_VALUE_KEY_INFORMATION)Argument2)->Object);
			DbgPrint("[RegNtPreSetValueKey]KeyPath: %wZ", &registryPath);
			DbgPrint("[RegNtPreSetValueKey]ValName: %wZ", ((PREG_SET_VALUE_KEY_INFORMATION)Argument2)->ValueName);
			
			notification->Operation = 6;
			char newPath[MAX_PATH] = { 0 };
			WCHAR newPath1[MAX_PATH] = { 0 };
			UnicodeToChar(&registryPath, newPath);
			CharToWchar(newPath, newPath1);
			wcscpy_s(notification->ProcessPath, MAX_PATH, newPath1);

			char newName[MAX_PATH] = { 0 };
			WCHAR newName1[MAX_PATH] = { 0 };
			UnicodeToChar(((PREG_SET_VALUE_KEY_INFORMATION)Argument2)->ValueName, newName);
			CharToWchar(newName, newName1);
			wcscpy_s(notification->TargetPath, MAX_PATH, newName1);
			replyLength = sizeof(FQDRV_REPLY);
			status = FltSendMessage(FQDRVData.Filter,
				&FQDRVData.ClientPort,
				notification,
				sizeof(FQDRV_NOTIFICATION),
				notification,
				&replyLength,
				NULL);
			if (STATUS_SUCCESS == status) {

				SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;
				if (SafeToOpen)
				{
					CallbackStatus = STATUS_SUCCESS;
				}
				else {
					CallbackStatus = STATUS_ACCESS_DENIED;
				}
			}

			//CallbackStatus = STATUS_ACCESS_DENIED;
		}
		break;
	}
	case RegNtPreDeleteValueKey:
	{
		if (IsProcessName("regedit.exe", PsGetCurrentProcess()))
		{
			GetRegistryObjectCompleteName(&registryPath, NULL, ((PREG_DELETE_VALUE_KEY_INFORMATION)Argument2)->Object);
			DbgPrint("[RegNtPreDeleteValueKey]KeyPath: %wZ", &registryPath);
			DbgPrint("[RegNtPreDeleteValueKey]ValName: %wZ", ((PREG_DELETE_VALUE_KEY_INFORMATION)Argument2)->ValueName);

			notification->Operation = 7;
			char newPath[MAX_PATH] = { 0 };
			WCHAR newPath1[MAX_PATH] = { 0 };
			UnicodeToChar(&registryPath, newPath);
			CharToWchar(newPath, newPath1);
			wcscpy_s(notification->ProcessPath, MAX_PATH, newPath1);

			char newName[MAX_PATH] = { 0 };
			WCHAR newName1[MAX_PATH] = { 0 };
			UnicodeToChar(((PREG_DELETE_VALUE_KEY_INFORMATION)Argument2)->ValueName, newName);
			CharToWchar(newName, newName1);
			wcscpy_s(notification->TargetPath, MAX_PATH, newName1);
			replyLength = sizeof(FQDRV_REPLY);
			status = FltSendMessage(FQDRVData.Filter,
				&FQDRVData.ClientPort,
				notification,
				sizeof(FQDRV_NOTIFICATION),
				notification,
				&replyLength,
				NULL);
			if (STATUS_SUCCESS == status) {

				SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;
				if (SafeToOpen)
				{
					CallbackStatus = STATUS_SUCCESS;
				}
				else {
					CallbackStatus = STATUS_ACCESS_DENIED;
				}
			}

			//CallbackStatus = STATUS_ACCESS_DENIED;
		}
		break;
	}
	case RegNtPreRenameKey:
	{
		if (IsProcessName("regedit.exe", PsGetCurrentProcess()))
		{
			GetRegistryObjectCompleteName(&registryPath, NULL, ((PREG_RENAME_KEY_INFORMATION)Argument2)->Object);
			DbgPrint("[RegNtPreRenameKey]KeyPath: %wZ", &registryPath);
			DbgPrint("[RegNtPreRenameKey]NewName: %wZ", ((PREG_RENAME_KEY_INFORMATION)Argument2)->NewName);

			notification->Operation = 8;
			char newPath[MAX_PATH] = { 0 };
			WCHAR newPath1[MAX_PATH] = { 0 };
			UnicodeToChar(&registryPath, newPath);
			CharToWchar(newPath, newPath1);
			wcscpy_s(notification->ProcessPath, MAX_PATH, newPath1);

			char newName[MAX_PATH] = { 0 };
			WCHAR newName1[MAX_PATH] = { 0 };
			UnicodeToChar(((PREG_RENAME_KEY_INFORMATION)Argument2)->NewName, newName);
			CharToWchar(newName, newName1);
			wcscpy_s(notification->TargetPath, MAX_PATH, newName1);
			replyLength = sizeof(FQDRV_REPLY);
			status = FltSendMessage(FQDRVData.Filter,
				&FQDRVData.ClientPort,
				notification,
				sizeof(FQDRV_NOTIFICATION),
				notification,
				&replyLength,
				NULL);
			if (STATUS_SUCCESS == status) {

				SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;
				if (SafeToOpen)
				{
					CallbackStatus = STATUS_SUCCESS;
				}
				else {
					CallbackStatus = STATUS_ACCESS_DENIED;
				}
			}

			//CallbackStatus = STATUS_ACCESS_DENIED;
		}
		break;
	}
	//『注册表编辑器』里的“重命名键值”是没有直接函数的，是先SetValueKey再DeleteValueKey
	default:
		break;
	}
	

	if (registryPath.Buffer != NULL)
		ExFreePoolWithTag(registryPath.Buffer, REGISTRY_POOL_TAG);
	return CallbackStatus;
}