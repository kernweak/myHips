#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include "commonStruct.h"
#include "FQDrv.h"
#include "commonFun.h"
#include <ntimage.h>

//#pragma comment(lib,"ntoskrnl.lib")
//#pragma comment(lib,"ndis.lib")
//#pragma comment(lib,"fwpkclnt.lib")
//#pragma comment(lib,"uuid.lib")
#define NDIS61

#include <ntddk.h>
#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union
#pragma warning(disable:4995)
#include <fwpsk.h>
#pragma warning(pop)
#include <ndis.h>
#include <fwpmk.h>
#include <limits.h>
#include <ws2ipdef.h>
#include <in6addr.h>
#include <ip2string.h>
#include <strsafe.h>
#define INITGUID
#include <guiddef.h>
#define bool BOOLEAN
#define true TRUE 
#define false FALSE
#define DEVICE_NAME L"\\Device\\WFP_TEST"
#define DEVICE_DOSNAME L"\\DosDevices\\WFP_TEST"
#define kmalloc(_s) ExAllocatePoolWithTag(NonPagedPool, _s, 'SYSQ')
#define kfree(_p) ExFreePool(_p)


#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


DEFINE_GUID // {6812FC83-7D3E-499a-A012-55E0D85F348B}
(
	GUID_ALE_AUTH_CONNECT_CALLOUT_V4,
	0x6812fc83,
	0x7d3e,
	0x499a,
	0xa0, 0x12, 0x55, 0xe0, 0xd8, 0x5f, 0x34, 0x8b
);

PDEVICE_OBJECT  gDevObj;

HANDLE	gEngineHandle = 0;
HANDLE	gInjectHandle = 0;
//CalloutId
UINT32	gAleConnectCalloutId = 0;
//FilterId
UINT64	gAleConnectFilterId = 0;

/*
	以下两个回调函数没啥用
*/
NTSTATUS NTAPI WallNotifyFn
(
	IN FWPS_CALLOUT_NOTIFY_TYPE  notifyType,
	IN const GUID  *filterKey,
	IN const FWPS_FILTER  *filter
)
{
	return STATUS_SUCCESS;
}

VOID NTAPI WallFlowDeleteFn
(
	IN UINT16  layerId,
	IN UINT32  calloutId,
	IN UINT64  flowContext
)
{
	return;
}


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

//进程监控相关
NTKERNELAPI PCHAR PsGetProcessImageFileName(PEPROCESS Process);
NTKERNELAPI NTSTATUS PsLookupProcessByProcessId(HANDLE ProcessId, PEPROCESS *Process);

//删除文件相关

NTSTATUS
ZwQueryDirectoryFile(
	__in HANDLE  FileHandle,
	__in_opt HANDLE  Event,
	__in_opt PIO_APC_ROUTINE  ApcRoutine,
	__in_opt PVOID  ApcContext,
	__out PIO_STATUS_BLOCK  IoStatusBlock,
	__out PVOID  FileInformation,
	__in ULONG  Length,
	__in FILE_INFORMATION_CLASS  FileInformationClass,
	__in BOOLEAN  ReturnSingleEntry,
	__in_opt PUNICODE_STRING  FileName,
	__in BOOLEAN  RestartScan
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
pFilenames m_pfilenames=NULL;//文件规则链表
ERESOURCE  g_fileLock;

pFilenames m_pProcessNames = NULL;//进程规则链表
ERESOURCE  g_processLock;

pFilenames m_pMoudleNames = NULL;//模块规则链表
ERESOURCE  g_moduleLock;

pFilenames m_pNetRejectNames = NULL;//禁止网络访问规则链表
ERESOURCE  g_NetRejectLock;

ULONG g_ulCurrentWaitID = 0;//攻击事件ID

VOID __stdcall LockWrite(ERESOURCE *lpLock)
{
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite(lpLock, TRUE);
}


VOID __stdcall UnLockWrite(ERESOURCE *lpLock)
{
	ExReleaseResourceLite(lpLock);
	KeLeaveCriticalRegion();
}


VOID __stdcall LockRead(ERESOURCE *lpLock)
{
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(lpLock, TRUE);
}


VOID __stdcall LockReadStarveWriter(ERESOURCE *lpLock)
{
	KeEnterCriticalRegion();
	ExAcquireSharedStarveExclusive(lpLock, TRUE);
}


VOID __stdcall UnLockRead(ERESOURCE *lpLock)
{
	ExReleaseResourceLite(lpLock);
	KeLeaveCriticalRegion();
}


VOID __stdcall InitLock(ERESOURCE *lpLock)
{
	ExInitializeResourceLite(lpLock);
}

VOID __stdcall DeleteLock(ERESOURCE *lpLock)
{
	ExDeleteResourceLite(lpLock);
}


ULONG isOpenFilter = 1;
ULONG isOpenReg = 1;
ULONG isOpenProcess = 1;
ULONG isOpenModule = 1;
ULONG isOpenNet = 1;
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


//协议代码转为名称
char* ProtocolIdToName(UINT16 id)
{
	char *ProtocolName = kmalloc(16);
	switch (id)	//http://www.ietf.org/rfc/rfc1700.txt
	{
	case 1:
		strcpy_s(ProtocolName, 4 + 1, "ICMP");
		break;
	case 2:
		strcpy_s(ProtocolName, 4 + 1, "IGMP");
		break;
	case 6:
		strcpy_s(ProtocolName, 3 + 1, "TCP");
		break;
	case 17:
		strcpy_s(ProtocolName, 3 + 1, "UDP");
		break;
	case 27:
		strcpy_s(ProtocolName, 3 + 1, "RDP");
		break;
	default:
		strcpy_s(ProtocolName, 7 + 1, "UNKNOWN");
		break;
	}
	return ProtocolName;
}

//最重要的过滤函数
//http://msdn.microsoft.com/en-us/library/windows/hardware/ff551238(v=vs.85).aspx
void NTAPI WallALEConnectClassify
(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const void* classifyContext,
	IN const FWPS_FILTER* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT* classifyOut
)
{
	char *ProtocolName = NULL;
	DWORD LocalIp, RemoteIP;
	LocalIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_LOCAL_ADDRESS].value.uint32;
	RemoteIP = inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_ADDRESS].value.uint32;
	ProtocolName = ProtocolIdToName(inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_PROTOCOL].value.uint16);
	//DbgPrint("[WFP]IRQL=%d;PID=%ld;Path=%S;Local=%u.%u.%u.%u:%d;Remote=%u.%u.%u.%u:%d;Protocol=%s\n",
	//	(USHORT)KeGetCurrentIrql(),
	//	(DWORD)(inMetaValues->processId),
	//	(PWCHAR)inMetaValues->processPath->data,	//NULL,//
	//	(LocalIp >> 24) & 0xFF, (LocalIp >> 16) & 0xFF, (LocalIp >> 8) & 0xFF, LocalIp & 0xFF,
	//	inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_LOCAL_PORT].value.uint16,
	//	(RemoteIP >> 24) & 0xFF, (RemoteIP >> 16) & 0xFF, (RemoteIP >> 8) & 0xFF, RemoteIP & 0xFF,
	//	inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_PORT].value.uint16,
	//	ProtocolName);
	kfree(ProtocolName);
	//classifyOut->actionType = FWP_ACTION_PERMIT;//允许连接
	//禁止IE联网（设置“行动类型”为FWP_ACTION_BLOCK）
	if((isOpenNet==1)&&(searchModuleRule(inMetaValues->processPath->data,&m_pNetRejectNames)))
	 {
	 classifyOut->actionType = FWP_ACTION_BLOCK;
	 classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	 classifyOut->flags |= FWPS_CLASSIFY_OUT_FLAG_ABSORB;
	 DbgPrint("%S静止连网",(PWCHAR)inMetaValues->processPath->data);
	 }
	 else
	 {
		 classifyOut->actionType = FWP_ACTION_PERMIT;//允许连接
	 }
	return;
}

NTSTATUS RegisterCalloutForLayer
(
	IN const GUID* layerKey,
	IN const GUID* calloutKey,
	IN FWPS_CALLOUT_CLASSIFY_FN classifyFn,
	IN FWPS_CALLOUT_NOTIFY_FN notifyFn,
	IN FWPS_CALLOUT_FLOW_DELETE_NOTIFY_FN flowDeleteNotifyFn,
	OUT UINT32* calloutId,
	OUT UINT64* filterId
)
{
	NTSTATUS        status = STATUS_SUCCESS;
	FWPS_CALLOUT    sCallout = { 0 };
	FWPM_FILTER     mFilter = { 0 };
	FWPM_FILTER_CONDITION mFilter_condition[1] = { 0 };
	FWPM_CALLOUT    mCallout = { 0 };
	FWPM_DISPLAY_DATA mDispData = { 0 };
	BOOLEAN         bCalloutRegistered = FALSE;
	sCallout.calloutKey = *calloutKey;
	sCallout.classifyFn = classifyFn;
	sCallout.flowDeleteFn = flowDeleteNotifyFn;
	sCallout.notifyFn = notifyFn;
	//要使用哪个设备对象注册
	status = FwpsCalloutRegister(gDevObj, &sCallout, calloutId);
	if (!NT_SUCCESS(status))
		goto exit;
	bCalloutRegistered = TRUE;
	mDispData.name = L"WFP TEST";
	mDispData.description = L"TESLA.ANGELA's WFP TEST";
	//你感兴趣的内容
	mCallout.applicableLayer = *layerKey;
	//你感兴趣的内容的GUID
	mCallout.calloutKey = *calloutKey;
	mCallout.displayData = mDispData;
	//添加回调函数
	status = FwpmCalloutAdd(gEngineHandle, &mCallout, NULL, NULL);
	if (!NT_SUCCESS(status))
		goto exit;
	mFilter.action.calloutKey = *calloutKey;
	//在callout里决定
	mFilter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
	mFilter.displayData.name = L"WFP TEST";
	mFilter.displayData.description = L"TESLA.ANGELA's WFP TEST";
	mFilter.layerKey = *layerKey;
	mFilter.numFilterConditions = 0;
	mFilter.filterCondition = mFilter_condition;
	mFilter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
	mFilter.weight.type = FWP_EMPTY;
	//添加过滤器
	status = FwpmFilterAdd(gEngineHandle, &mFilter, NULL, filterId);
	if (!NT_SUCCESS(status))
		goto exit;
exit:
	if (!NT_SUCCESS(status))
	{
		if (bCalloutRegistered)
		{
			FwpsCalloutUnregisterById(*calloutId);
		}
	}
	return status;
}

NTSTATUS WallRegisterCallouts()
{
	NTSTATUS    status = STATUS_SUCCESS;
	BOOLEAN     bInTransaction = FALSE;
	BOOLEAN     bEngineOpened = FALSE;
	FWPM_SESSION session = { 0 };
	session.flags = FWPM_SESSION_FLAG_DYNAMIC;
	//开启WFP引擎
	status = FwpmEngineOpen(NULL,
		RPC_C_AUTHN_WINNT,
		NULL,
		&session,
		&gEngineHandle);
	if (!NT_SUCCESS(status))
		goto exit;
	bEngineOpened = TRUE;
	//确认过滤权限
	status = FwpmTransactionBegin(gEngineHandle, 0);
	if (!NT_SUCCESS(status))
		goto exit;
	bInTransaction = TRUE;
	//注册回调函数
	status = RegisterCalloutForLayer(
		&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
		&GUID_ALE_AUTH_CONNECT_CALLOUT_V4,
		WallALEConnectClassify,
		WallNotifyFn,
		WallFlowDeleteFn,
		&gAleConnectCalloutId,
		&gAleConnectFilterId);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("RegisterCalloutForLayer-FWPM_LAYER_ALE_AUTH_CONNECT_V4 failed!\n");
		goto exit;
	}
	//确认所有内容并提交，让回调函数正式发挥作用
	status = FwpmTransactionCommit(gEngineHandle);
	if (!NT_SUCCESS(status))
		goto exit;
	bInTransaction = FALSE;
exit:
	if (!NT_SUCCESS(status))
	{
		if (bInTransaction)
		{
			FwpmTransactionAbort(gEngineHandle);
		}
		if (bEngineOpened)
		{
			FwpmEngineClose(gEngineHandle);
			gEngineHandle = 0;
		}
	}
	return status;
}

NTSTATUS WallUnRegisterCallouts()
{
	if (gEngineHandle != 0)
	{
		//删除FilterId
		FwpmFilterDeleteById(gEngineHandle, gAleConnectFilterId);
		//删除CalloutId
		FwpmCalloutDeleteById(gEngineHandle, gAleConnectCalloutId);
		//清空FilterId
		gAleConnectFilterId = 0;
		//反注册CalloutId
		FwpsCalloutUnregisterById(gAleConnectCalloutId);
		//清空CalloutId
		gAleConnectCalloutId = 0;
		//关闭引擎
		FwpmEngineClose(gEngineHandle);
		gEngineHandle = 0;
	}
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT driverObject)
{
	NTSTATUS status;

	status = WallUnRegisterCallouts();
	if (!NT_SUCCESS(status))
	{
		DbgPrint("网络监控开启 SUCCESS\n");
		return;
	}
	if (gDevObj)
	{
		IoDeleteDevice(gDevObj);
		gDevObj = NULL;
	}
	DbgPrint("网络监控开启 failed\n");
}

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

	//UNREFERENCED_PARAMETER(RegistryPath);
	//DriverObject->DriverUnload = DriverUnload;


	UNICODE_STRING  deviceName = { 0 };
	UNICODE_STRING  deviceDosName = { 0 };

	DriverObject->DriverUnload = DriverUnload;
	RtlInitUnicodeString(&deviceName, DEVICE_NAME);
	status = IoCreateDevice(DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_NETWORK,
		0,
		FALSE,
		&gDevObj);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("WFP设备IoCreateDevice failed!\n");
		return STATUS_UNSUCCESSFUL;
	}
	RtlInitUnicodeString(&deviceDosName, DEVICE_DOSNAME);
	status = IoCreateSymbolicLink(&deviceDosName, &deviceName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("WFP设备Create Symbolink name failed!\n");
		return STATUS_UNSUCCESSFUL;
	}
	status = WallRegisterCallouts();
	if (!NT_SUCCESS(status))
	{
		DbgPrint("WFP设备WallRegisterCallouts failed!\n");
		return STATUS_UNSUCCESSFUL;
	}
	DbgPrint("WFP设备 loaded! WallRegisterCallouts() success!\n");



	InitLock(&g_fileLock);
	InitLock(&g_processLock);
	InitLock(&g_moduleLock);

	g_LastDelFileName.Buffer = ExAllocatePool(NonPagedPool, MAX_PATH * 2);
	g_LastDelFileName.Length = g_LastDelFileName.MaximumLength = MAX_PATH * 2;
	memset(g_LastDelFileName.Buffer, '\0', MAX_PATH * 2);

	//注册表监控初始化
	status = PtRegisterInit();
	if (!NT_SUCCESS(status))
	{
		PtRegisterUnInit();
	}
	status = PtProcessInit();
	if (!NT_SUCCESS(status))
	{
		DbgPrint("初始化进程监控失败\n");
		PtProcessUnInit();
	}
	status = PtModuleInit();
	if (!NT_SUCCESS(status))
	{
		DbgPrint("初始化模块失败\n");
		PtProcessUnInit();
	}

	status = FltRegisterFilter(DriverObject,
		&FilterRegistration,
		&FQDRVData.Filter);
	//status = WallRegisterCallouts();
	if (!NT_SUCCESS(status))
	{
		DbgPrint("WFP设备WallRegisterCallouts failed!\n");
		return STATUS_UNSUCCESSFUL;
	}
	DbgPrint("WFP设备 loaded! WallRegisterCallouts() success!\n");


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
	NTSTATUS status = STATUS_SUCCESS;
	FltUnregisterFilter(FQDRVData.Filter);
	PtRegisterUnInit();
	PtProcessUnInit();
	PtModuleUnInit();

	DeleteLock(&g_fileLock);
	DeleteLock(&g_processLock); 
	DeleteLock(&g_moduleLock);
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
		//LockRead(&g_fileLock);
		scanFile = searchRule(tmp,&m_pfilenames);
		//UnLockRead(&g_fileLock);
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
	//LockRead(&g_fileLock);
	Ret=searchRule(tmp,&m_pfilenames);
	//UnLockRead(&g_fileLock);
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
				Options = 0;//
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

			wcsncpy(g_LastDelFileName.Buffer, nameInfo->Name.Buffer, nameInfo->Name.MaximumLength);
		}
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
		LARGE_INTEGER WaitTimeOut = { 0 };
		replyLength = sizeof(FQDRV_REPLY);
		WaitTimeOut.QuadPart = -20 * 10000000;
		status = FltSendMessage(FQDRVData.Filter,
			&FQDRVData.ClientPort,
			notification,
			sizeof(FQDRV_NOTIFICATION),
			notification,
			&replyLength,
			&WaitTimeOut);

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
	WCHAR filePath[MAX_PATH] = { 0 };
	WCHAR filePathHead[MAX_PATH]=L"\\??\\";
	if (data->command == DEFAULT_PATH || data->command == ADD_PATH || data->command == DELETE_PATH)
	{
		
		GetNtDeviceName(data->filename, rulePath);
		DbgPrint("用户发来的信息是:%ls,GetNtDeviceName之后是%ls\n", data->filename, rulePath);
	}
	else if(data->command == DELETE_FILE)
	{
		wcscat(filePathHead, data->filename);
		wcscpy_s(rulePath, MAX_PATH, filePathHead);

	}
	else
	{
		wcscpy_s(rulePath, MAX_PATH, data->filename);
	}
	


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
		//LockWrite(&g_fileLock);
		uResult=AddPathList(&cachePath,&m_pfilenames);
		//LockWrite(&g_fileLock);
		break;
	case ADD_PATH:
		//LockWrite(&g_fileLock);
		uResult=AddPathList(&cachePath, &m_pfilenames);
		//LockWrite(&g_fileLock);
		break;
	case DELETE_PATH:
		//LockWrite(&g_fileLock);
		uResult = DeletePathList(&cachePath, &m_pfilenames);
		//LockWrite(&g_fileLock);
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
	case DEFAULT_PROCESS:
		uResult = AddPathList(&cachePath, &m_pProcessNames);
		break;
	case ADD_PROCESS:
		uResult = AddPathList(&cachePath, &m_pProcessNames);
		break;
	case DELETE_PROCESS:
		uResult = DeletePathList(&cachePath, &m_pProcessNames);
		break;
	case PAUSE_PROCESS:
		isOpenProcess = 0;
		uResult = MPAUSE_PROCESS;
		break;
	case RESTART_PROCESS:
		isOpenProcess = 1;
		uResult = MRESTART_PROCESS;
		break;
	case ADD_MODULE:
		uResult = AddPathList(&cachePath, &m_pMoudleNames);
		break;
	case DELETE_MODULE:
		uResult = DeletePathList(&cachePath, &m_pMoudleNames);
		break;
	case PAUSE_MODULE:
		isOpenModule = 0;
		uResult = MPAUSE_MODULE;
		break;
	case RESTART_MODULE:
		isOpenModule = 1;
		uResult = MRESTART_MODULE;
		break;
	case DELETE_FILE:
		//uResult = MRESTART_MODULE;
		DbgPrint("filePathHead is %ls\n", filePathHead);
		status=myDelFileDir(filePathHead);
		if (NT_SUCCESS(status)) {
			uResult = DELETE_SUCCESS;
		}
		else {
			uResult = DELETE_FAITH;
		}
		break;
	case ADD_NETREJECT:
		//LockWrite(&g_fileLock);
		uResult = AddPathList(&cachePath, &m_pNetRejectNames);
		//LockWrite(&g_fileLock);
		break;
	case DELETE_NETREJECT:
		//LockWrite(&g_fileLock);
		uResult = DeletePathList(&cachePath, &m_pNetRejectNames);
		//LockWrite(&g_fileLock);
		break;
	case PAUSE_NETMON:
		isOpenNet = 0;
		uResult = MPAUSE_NETMON;
		break;
	case RESTART_NETMON:
		isOpenNet = 1;
		uResult = MRESTART_NETMON;
		break;
	default:
		break;
	}
	
	////释放内存
	
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
	case MPAUSE_PROCESS:
		wcscpy_s(buffer, wcslen(L"PAUSE_PROCESS") + 1, L"PAUSE_PROCESS");
		break;
	case MRESTART_PROCESS:
		wcscpy_s(buffer, wcslen(L"RESTART_PROCESS") + 1, L"RESTART_PROCESS");
		break;
	case MPAUSE_MODULE:
		wcscpy_s(buffer, wcslen(L"PAUSE_MODULE") + 1, L"PAUSE_MODULE");
		break;
	case MRESTART_MODULE:
		wcscpy_s(buffer, wcslen(L"RESTART_MODULE") + 1, L"RESTART_MODULE");
		break;
	case MPAUSE_NETMON:
		wcscpy_s(buffer, wcslen(L"PAUSE_NETMON") + 1, L"PAUSE_NETMON");
		break;
	case MRESTART_NETMON:
		wcscpy_s(buffer, wcslen(L"RESTART_NETMON") + 1, L"RESTART_NETMON");
		break;
	default:
		break;
	}
	ReturnOutputBufferLength = (wcslen(buffer) + 1) * sizeof(WCHAR);
	RtlCopyMemory(OutputBuffer, buffer, (wcslen(buffer) + 1) * sizeof(WCHAR));
	return status;
}


ULONG AddPathList(PUNICODE_STRING  filename, pFilenames *headFilenames)
{
	filenames * new_filename, *current, *precurrent;
	new_filename = ExAllocatePool(NonPagedPool, sizeof(filenames));
	new_filename->filename.Buffer = (PWSTR)ExAllocatePool(NonPagedPool, filename->MaximumLength);
	new_filename->filename.MaximumLength = filename->MaximumLength;
	__try {
		RtlCopyUnicodeString(&new_filename->filename, filename);

		new_filename->pNext = NULL;
		if (NULL == *headFilenames)              //头是空的，路径添加到头
		{
			*headFilenames = new_filename;
			DbgPrint("插入成功，头是空的，路径添加到头,插入的是%wZ\n", new_filename->filename);
			return ADD_SUCCESS;
		}
		current = *headFilenames;
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
		current = *headFilenames;
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
ULONG DeletePathList(PUNICODE_STRING  filename, pFilenames *headFilenames)
{
	filenames  *current, *precurrent;
	current = precurrent = *headFilenames;
	while (current != NULL)
	{
		__try {
			if (RtlEqualUnicodeString(filename, &current->filename, TRUE))
			{
				//判断一下是否是头,如果是头，就让头指向第二个，删掉第一个
				if (current == *headFilenames)
				{
					DbgPrint("删除成功，删除的是头结点是%wZ\n", current->filename);
					*headFilenames = current->pNext;
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

BOOLEAN searchRule(WCHAR *path, pFilenames *headFilenames)
{
	filenames *current;
	current = *headFilenames;
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

BOOLEAN searchProcessRule(WCHAR *path, pFilenames *headFilenames)
{
	filenames *current;
	current = *headFilenames;
	WCHAR tmpPath[MAX_PATH] = { 0 };
	while (current != NULL)
	{
		__try {
			WCHAR tmp[MAX_PATH] = { 0 };
			wcsncpy_s(tmp, current->filename.Length, current->filename.Buffer, current->filename.Length);
			//ToUpperString(tmp);
			//DbgPrint("tmp is %ls,path is %ls", tmp, path);
			if (!wcscmp(tmp, path))
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
	if (NULL != notification) {

		ExFreePoolWithTag(notification, 'nacS');
	}

	if (registryPath.Buffer != NULL)
		ExFreePoolWithTag(registryPath.Buffer, REGISTRY_POOL_TAG);
	return CallbackStatus;
}



PWCHAR GetProcessNameByProcessId(HANDLE ProcessId)
{
	NTSTATUS st = STATUS_UNSUCCESSFUL;
	PEPROCESS ProcessObj = NULL;
	PWCHAR string = NULL;
	st = PsLookupProcessByProcessId(ProcessId, &ProcessObj);
	if (NT_SUCCESS(st))
	{
		string = PsGetProcessImageFileName(ProcessObj);
		ObfDereferenceObject(ProcessObj);
	}
	return string;
}

VOID MyCreateProcessNotifyEx
(
	__inout   PEPROCESS Process,
	__in      HANDLE ProcessId,
	__in_opt  PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
	if (isOpenProcess) {
		NTSTATUS st = 0;
		HANDLE hProcess = NULL;
		OBJECT_ATTRIBUTES oa = { 0 };
		CLIENT_ID ClientId = { 0 };
		char xxx[16] = { 0 };
		ULONG Options = 0;//9进程创建
		BOOLEAN isAllow = TRUE;//是否放行
		PFQDRV_NOTIFICATION notification = NULL;
		BOOLEAN SafeToOpen;
		SafeToOpen = TRUE;
		BOOLEAN scanFile;
		scanFile = TRUE;
		ULONG replyLength = 0;
		notification = ExAllocatePoolWithTag(NonPagedPool,
			sizeof(FQDRV_NOTIFICATION),
			'nacS');
		if (NULL == notification)
		{
			st = STATUS_INSUFFICIENT_RESOURCES;
			return st;
		}

		if (CreateInfo != NULL)	//进程创建事件
		{
			DbgPrint("进程监控[%ld]%s创建进程: %wZ",
				CreateInfo->ParentProcessId,
				GetProcessNameByProcessId(CreateInfo->ParentProcessId),
				CreateInfo->ImageFileName);
			strcpy(xxx, PsGetProcessImageFileName(Process));

			WCHAR tmp[256] = { 0 };
			CharToWchar(PsGetProcessImageFileName(Process), tmp);

			scanFile = searchProcessRule(tmp, &m_pProcessNames);
			if (scanFile) {

				notification->Operation = 9;
				CHAR parentProcessName[MAX_PATH] = { 0 };
				WCHAR wparentProcessName[MAX_PATH] = { 0 };
				strcpy_s(parentProcessName, MAX_PATH, GetProcessNameByProcessId(CreateInfo->ParentProcessId));
				CharToWchar(parentProcessName, wparentProcessName);
				wcscpy_s(notification->ProcessPath, MAX_PATH, wparentProcessName);

				char processName[MAX_PATH] = { 0 };
				WCHAR wProcessName[MAX_PATH] = { 0 };
				UnicodeToChar(CreateInfo->ImageFileName, processName);
				CharToWchar(processName, wProcessName);
				wcscpy_s(notification->TargetPath, MAX_PATH, wProcessName);
				replyLength = sizeof(FQDRV_REPLY);
				st = FltSendMessage(FQDRVData.Filter,
					&FQDRVData.ClientPort,
					notification,
					sizeof(FQDRV_NOTIFICATION),
					notification,
					&replyLength,
					NULL);
				if (STATUS_SUCCESS == st) {

					SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;
					if (SafeToOpen)
					{
						CreateInfo->CreationStatus = STATUS_SUCCESS;
					}
					else {
						CreateInfo->CreationStatus = STATUS_UNSUCCESSFUL;
					}
				}

			}
		}
		else
		{
			DbgPrint("进程监控：进程退出: %s", PsGetProcessImageFileName(Process));
		}

		if (NULL != notification) {

			ExFreePoolWithTag(notification, 'nacS');
		}
	}
}


NTSTATUS PtProcessInit()
{
	NTSTATUS status = 0;
	status=PsSetCreateProcessNotifyRoutineEx((PCREATE_PROCESS_NOTIFY_ROUTINE_EX)MyCreateProcessNotifyEx, FALSE);
	if (NT_SUCCESS(status))
		DbgPrint("进程监控开启 SUCCESS!");
	else
		DbgPrint("进程监控开启 Failed!");
	return status;
}
NTSTATUS PtProcessUnInit()
{
	NTSTATUS status = 0;
	status = PsSetCreateProcessNotifyRoutineEx((PCREATE_PROCESS_NOTIFY_ROUTINE_EX)MyCreateProcessNotifyEx, TRUE);
	if (NT_SUCCESS(status))
		DbgPrint("进程监控关闭 SUCCESS!");
	else
		DbgPrint("进程监控关闭 Failed!");
	return status;
}

//模块加载相关
NTSTATUS RtlSuperCopyMemory(IN VOID UNALIGNED *Dst,
	IN CONST VOID UNALIGNED *Src,
	IN ULONG Length)
{
	//MDL是一个对物理内存的描述，负责把虚拟内存映射到物理内存
	PMDL pmdl = IoAllocateMdl(Dst, Length, 0, 0, NULL);//分配mdl
	if (pmdl == NULL)
		return STATUS_UNSUCCESSFUL;

	MmBuildMdlForNonPagedPool(pmdl);//build mdl
	unsigned int *Mapped = (unsigned int *)MmMapLockedPages(pmdl, KernelMode);//锁住内存
	if (!Mapped) {
		IoFreeMdl(pmdl);
		return STATUS_UNSUCCESSFUL;
	}

	KIRQL kirql = KeRaiseIrqlToDpcLevel();
	RtlCopyMemory(Mapped, Src, Length);
	KeLowerIrql(kirql);

	MmUnmapLockedPages((PVOID)Mapped, pmdl);
	IoFreeMdl(pmdl);

	return STATUS_SUCCESS;
}


void DenyLoadDriver(PVOID DriverEntry)
{
	UCHAR fuck[] = "\xB8\x22\x00\x00\xC0\xC3";
	RtlSuperCopyMemory(DriverEntry, fuck, sizeof(fuck));
}


PVOID GetDriverEntryByImageBase(PVOID ImageBase)
{
	PIMAGE_DOS_HEADER pDOSHeader;
	PIMAGE_NT_HEADERS64 pNTHeader;
	PVOID pEntryPoint;
	pDOSHeader = (PIMAGE_DOS_HEADER)ImageBase;
	pNTHeader = (PIMAGE_NT_HEADERS64)((ULONG64)ImageBase + pDOSHeader->e_lfanew);
	pEntryPoint = (PVOID)((ULONG64)ImageBase + pNTHeader->OptionalHeader.AddressOfEntryPoint);
	return pEntryPoint;
}

VOID LoadImageNotifyRoutine
(
	__in_opt PUNICODE_STRING  FullImageName,
	__in HANDLE  ProcessId,
	__in PIMAGE_INFO  ImageInfo
)
{
	if (isOpenModule) {
		PVOID pDrvEntry;
		char szFullImageName[260] = { 0 };
		ULONG Options = 0;//10模块加载
		BOOLEAN isAllow = TRUE;//是否放行
		PFQDRV_NOTIFICATION notification = NULL;
		BOOLEAN SafeToOpen;
		SafeToOpen = TRUE;
		BOOLEAN scanFile;
		ULONG replyLength = 0;
		NTSTATUS st = 0;
		if (FullImageName != NULL && MmIsAddressValid(FullImageName))
		{
			if (ProcessId == 0)
			{
				notification = ExAllocatePoolWithTag(NonPagedPool,
					sizeof(FQDRV_NOTIFICATION),
					'nacS');
				DbgPrint("模块加载监控%wZ\n", FullImageName);
				pDrvEntry = GetDriverEntryByImageBase(ImageInfo->ImageBase);
				DbgPrint("模块加载监控 模块的DriverEntry: %p\n", pDrvEntry);
				UnicodeToChar(FullImageName, szFullImageName);
				WCHAR newPath1[MAX_PATH] = { 0 };

				CharToWchar(szFullImageName, newPath1);
				//searchModuleRule(WCHAR *path, pFilenames *headFilenames);
				//if (strstr(szFullImageName, "myTestDriver.sys"))
				if (searchModuleRule(newPath1, &m_pMoudleNames))
				{
					//DbgPrint("Deny load [DelDir.sys]");
					//禁止加载win64ast.sys
					notification->Operation = 10;
					wcscpy_s(notification->ProcessPath, MAX_PATH, newPath1);


					replyLength = sizeof(FQDRV_REPLY);
					st = FltSendMessage(FQDRVData.Filter,
						&FQDRVData.ClientPort,
						notification,
						sizeof(FQDRV_NOTIFICATION),
						notification,
						&replyLength,
						NULL);

					if (STATUS_SUCCESS == st) {

						SafeToOpen = ((PFQDRV_REPLY)notification)->SafeToOpen;
						if (SafeToOpen)
						{
							DbgPrint("同意模块加载\n");
						}
						else {
							DenyLoadDriver(pDrvEntry);
						}
					}


				}
				else {
					DbgPrint("没匹配上");
				}

				if (NULL != notification) {

					ExFreePoolWithTag(notification, 'nacS');
				}
			}
		}
	}
}

NTSTATUS PtModuleInit()
{
	NTSTATUS status = 0;
	status = PsSetLoadImageNotifyRoutine((PLOAD_IMAGE_NOTIFY_ROUTINE)LoadImageNotifyRoutine);
	if (NT_SUCCESS(status))
		DbgPrint("模块监控开启 SUCCESS!");
	else
		DbgPrint("模块监控开启 Failed!");
	return status;
}
NTSTATUS PtModuleUnInit()
{
	NTSTATUS status = 0;
	status = PsRemoveLoadImageNotifyRoutine((PLOAD_IMAGE_NOTIFY_ROUTINE)LoadImageNotifyRoutine);
	if (NT_SUCCESS(status))
		DbgPrint("模块监控关闭 SUCCESS!");
	else
		DbgPrint("模块监控关闭 Failed!");
	return status;
}



BOOLEAN searchModuleRule(WCHAR *path, pFilenames *headFilenames)
{
	filenames *current;
	current = *headFilenames;
	WCHAR tmpPath[MAX_PATH] = { 0 };
	while (current != NULL)
	{
		__try {
			WCHAR tmp[MAX_PATH] = { 0 };
			wcsncpy_s(tmp, current->filename.Length, current->filename.Buffer, current->filename.Length);
			//ToUpperString(tmp);
			//DbgPrint("tmp is %ls,path is %ls", tmp, path);
			//if (strstr(szFullImageName, "myTestDriver.sys"))
			if (wcsstr(path, tmp))
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




//删除文件相关
NTSTATUS myDelFile(const WCHAR* fileName)
{
	NTSTATUS						status = STATUS_SUCCESS;
	UNICODE_STRING					uFileName = { 0 };
	OBJECT_ATTRIBUTES				objAttributes = { 0 };
	HANDLE							handle = NULL;
	IO_STATUS_BLOCK					iosb = { 0 };//io完成情况，成功true，失败false
	FILE_DISPOSITION_INFORMATION	disInfo = { 0 };
	RtlInitUnicodeString(&uFileName, fileName);
	//初始化属性
	InitializeObjectAttributes(
		&objAttributes,
		&uFileName,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL
	);
	status = ZwCreateFile(
		&handle,
		SYNCHRONIZE | FILE_WRITE_ACCESS | DELETE,
		&objAttributes,
		&iosb,
		NULL,
		FILE_ATTRIBUTE_NORMAL,//普通文件，非文件夹
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT | FILE_DELETE_ON_CLOSE,
		NULL,
		0);
	if (!NT_SUCCESS(status))
	{
		if (status == STATUS_ACCESS_DENIED) {
			//如果访问拒绝，可能是只读的，就以没delete去打开，然后进去把属性设置成narmal在删除打开
			status = ZwCreateFile(
				&handle,
				SYNCHRONIZE | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
				&objAttributes,
				&iosb,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				FILE_OPEN,
				FILE_SYNCHRONOUS_IO_NONALERT,
				NULL,
				0
			);
			if (NT_SUCCESS(status)) {
				FILE_BASIC_INFORMATION		basicInfo = { 0 };//因为要修改文件属性所以床FileBasicInformation
				status = ZwQueryInformationFile(
					handle, &iosb, &basicInfo,
					sizeof(basicInfo),//要查什么这里就是跟其相关的结构体
					FileBasicInformation
				);
				if (!NT_SUCCESS(status)) {
					DbgPrint("ZwQueryInformationFile %wZ is failed %x\n", &uFileName, status);
				}
				//然后修改文件属性
				basicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
				status = ZwSetInformationFile(handle, &iosb, &basicInfo, sizeof(basicInfo), FileBasicInformation);
				if (!NT_SUCCESS(status)) {
					DbgPrint("ZwSetInformationFile (%wZ) failed (%x)\n", &uFileName, status);
				}
				ZwClose(handle);
				status = ZwCreateFile(
					&handle,
					SYNCHRONIZE | FILE_WRITE_DATA | FILE_SHARE_DELETE,
					&objAttributes,
					&iosb,
					NULL,
					FILE_ATTRIBUTE_NORMAL,
					FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
					FILE_OPEN,
					FILE_SYNCHRONOUS_IO_ALERT | FILE_DELETE_ON_CLOSE,
					NULL,
					0
				);
			}
		}
		if (!NT_SUCCESS(status))
		{
			KdPrint(("ZwCreateFile(%wZ) failed(%x)\n", &uFileName, status));
			return status;
		}

	}
	disInfo.DeleteFile = TRUE;
	status = ZwSetInformationFile(handle, &iosb, &disInfo, sizeof(disInfo), FileDispositionInformation);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("ZwSetInformationFile(%wZ) failed(%x)\n", &uFileName, status));
	}

	ZwClose(handle);
	return status;
}

NTSTATUS myDelFileDir(const WCHAR * directory)
{
	UNICODE_STRING						uDirName = { 0 };
	PWSTR                            	nameBuffer = NULL;	//记录文件夹
	PFILE_LIST_ENTRY                	tmpEntry = NULL;	//链表结点
	LIST_ENTRY                        	listHead = { 0 };	//链表，用来存放删除过程中的目录
	NTSTATUS                        	status = 0;
	PFILE_LIST_ENTRY                	preEntry = NULL;
	OBJECT_ATTRIBUTES                	objAttributes = { 0 };
	HANDLE                            	handle = NULL;
	IO_STATUS_BLOCK                    	iosb = { 0 };
	BOOLEAN                            	restartScan = FALSE;
	PVOID                            	buffer = NULL;
	ULONG                            	bufferLength = 0;
	PFILE_DIRECTORY_INFORMATION        	dirInfo = NULL;
	UNICODE_STRING                    	nameString = { 0 };
	FILE_DISPOSITION_INFORMATION    	disInfo = { 0 };

	RtlInitUnicodeString(&uDirName, directory);
	nameBuffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, uDirName.Length + sizeof(WCHAR), 'DRID');//为了防'\0'加sizeofwchar
	if (!nameBuffer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	tmpEntry = (PFILE_LIST_ENTRY)ExAllocatePoolWithTag(PagedPool, sizeof(FILE_LIST_ENTRY), 'DRID');
	if (!tmpEntry) {
		ExFreePool(nameBuffer);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlCopyMemory(nameBuffer, uDirName.Buffer, uDirName.Length);
	nameBuffer[uDirName.Length / sizeof(WCHAR)] = L'\0';
	InitializeListHead(&listHead);//初始化链表
	tmpEntry->NameBuffer = nameBuffer;
	InsertHeadList(&listHead, &tmpEntry->ENTRY);//将要删除的文件夹首先插入链表   
	// listHead里初始化为要删除的文件夹。
	//之后遍历文件夹下的文件和目录，判断是文件，则立即删除；判断是目录，则放进listHead里面
	//每次都从listHead里拿出一个目录来处理
	while (!IsListEmpty(&listHead)) {
		//先将要删除的文件夹和之前打算删除的文件夹比较一下，如果从链表里取下来的还是之前的Entry，表明没有删除成功，说明里面非空
		//否则，已经成功删除，不可能是它自身；或者还有子文件夹，在前面，也不可能是它自身。
		tmpEntry = (PFILE_LIST_ENTRY)RemoveHeadList(&listHead);
		if (preEntry == tmpEntry)
		{
			status = STATUS_DIRECTORY_NOT_EMPTY;
			break;
		}

		preEntry = tmpEntry;
		InsertHeadList(&listHead, &tmpEntry->ENTRY); //放进去，等删除了里面的内容，再移除。如果移除失败，则说明还有子文件夹或者目录非空

		RtlInitUnicodeString(&nameString, tmpEntry->NameBuffer);
		//初始化内核对象
		InitializeObjectAttributes(&objAttributes, &nameString, OBJ_CASE_INSENSITIVE, NULL, NULL);
		//打开文件，查询
		status = ZwCreateFile(
			&handle,
			FILE_ALL_ACCESS,
			&objAttributes,
			&iosb,
			NULL,
			0,
			FILE_SHARE_VALID_FLAGS, FILE_OPEN,
			FILE_SYNCHRONOUS_IO_ALERT,
			NULL,
			0
		);
		if (!NT_SUCCESS(status))
		{

			DbgPrint("ZwCreateFile(%ws) failed(%x)\n", tmpEntry->NameBuffer, status);
			break;
		}
		//从第一个扫描
		restartScan = TRUE;//这是里面一个属性里面要改false不然ZwQueryDirectoryFile每次从第一个开始
		while (TRUE) //遍历从栈上摘除的文件夹
		{
			buffer = NULL;
			bufferLength = 64;//后面指数扩大
			status = STATUS_BUFFER_OVERFLOW;
			while ((status == STATUS_BUFFER_OVERFLOW) || (status == STATUS_INFO_LENGTH_MISMATCH))//不断分配内存，直到够大
			{
				if (buffer)
				{
					ExFreePool(buffer);
				}

				bufferLength *= 2;
				buffer = ExAllocatePoolWithTag(PagedPool, bufferLength, 'DRID');
				if (!buffer)
				{
					KdPrint(("ExAllocatePool failed\n"));
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}

				status = ZwQueryDirectoryFile(handle, NULL, NULL,
					NULL, &iosb, buffer, bufferLength, FileDirectoryInformation,
					FALSE, NULL, restartScan);//False代表一次只遍历每次只返回一个文件夹，restartScan设置为true代表顺序查，如果不改成false每次都是第一个文件夹
			}
			//上面查询完了，下面进行判断。查询结果在buffer里
			//如果是空说明遍历完了
			if (status == STATUS_NO_MORE_FILES)
			{
				ExFreePool(buffer);
				status = STATUS_SUCCESS;
				break;
			}
			restartScan = FALSE;

			if (!NT_SUCCESS(status))
			{
				KdPrint(("ZwQueryDirectoryFile(%ws) failed(%x)\n", tmpEntry->NameBuffer, status));
				if (buffer)
				{
					ExFreePool(buffer);
				}
				break;
			}

			dirInfo = (PFILE_DIRECTORY_INFORMATION)buffer;
			//申请一块内存保存当前文件夹路径和查询到子目录拼接在一起的完整目录
			nameBuffer = (PWSTR)ExAllocatePoolWithTag(PagedPool,
				wcslen(tmpEntry->NameBuffer) * sizeof(WCHAR) + dirInfo->FileNameLength + 4, 'DRID');
			if (!nameBuffer)
			{
				KdPrint(("ExAllocatePool failed\n"));
				ExFreePool(buffer);
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			//tmpEntry->NameBuffer是当前文件夹路径
			//下面的操作在拼接文件夹下面的文件路径
			RtlZeroMemory(nameBuffer, wcslen(tmpEntry->NameBuffer) * sizeof(WCHAR) + dirInfo->FileNameLength + 4);
			wcscpy(nameBuffer, tmpEntry->NameBuffer);
			wcscat(nameBuffer, L"\\");
			RtlCopyMemory(&nameBuffer[wcslen(nameBuffer)], dirInfo->FileName, dirInfo->FileNameLength);
			RtlInitUnicodeString(&nameString, nameBuffer);
			if (dirInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				//如果是非'.'和'..'两个特殊的目录，则将目录放入listHead
				if ((dirInfo->FileNameLength == sizeof(WCHAR)) && (dirInfo->FileName[0] == L'.'))
				{

				}
				else if ((dirInfo->FileNameLength == sizeof(WCHAR) * 2) &&
					(dirInfo->FileName[0] == L'.') &&
					(dirInfo->FileName[1] == L'.'))
				{
				}
				else
				{
					//将文件夹插入listHead中
					PFILE_LIST_ENTRY localEntry;
					localEntry = (PFILE_LIST_ENTRY)ExAllocatePoolWithTag(PagedPool, sizeof(FILE_LIST_ENTRY), 'DRID');
					if (!localEntry)
					{
						KdPrint(("ExAllocatePool failed\n"));
						ExFreePool(buffer);
						ExFreePool(nameBuffer);
						status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}
					localEntry->NameBuffer = nameBuffer;
					nameBuffer = NULL;
					InsertHeadList(&listHead, &localEntry->ENTRY); //插入头部，先把子文件夹里的删除
				}
			}
			else//如果是文件，直接删除
			{
				status = myDelFile(nameBuffer);
				if (!NT_SUCCESS(status))
				{
					KdPrint(("dfDeleteFile(%wZ) failed(%x)\n", &nameString, status));
					ExFreePool(buffer);
					ExFreePool(nameBuffer);
					break;
				}
			}
			ExFreePool(buffer);
			if (nameBuffer)
			{
				ExFreePool(nameBuffer);
			}//继续在循环里处理下一个子文件或者子文件夹

		}//  while (TRUE) ，一直弄目录里的文件和文件夹

		if (NT_SUCCESS(status))
		{
			//处理完目录里的文件文件夹，再处理目录文件夹
			disInfo.DeleteFile = TRUE;
			status = ZwSetInformationFile(handle, &iosb,
				&disInfo, sizeof(disInfo), FileDispositionInformation);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("ZwSetInformationFile(%ws) failed(%x)\n", tmpEntry->NameBuffer, status));
			}
		}
		ZwClose(handle);
		if (NT_SUCCESS(status))
		{
			//删除成功，从链表里移出该目录
			RemoveEntryList(&tmpEntry->ENTRY);
			ExFreePool(tmpEntry->NameBuffer);
			ExFreePool(tmpEntry);
		}
		//如果失败，则表明在文件夹还有子文件夹，继续先删除子文件夹
	}// while (!IsListEmpty(&listHead)) 
	while (!IsListEmpty(&listHead))
	{
		tmpEntry = (PFILE_LIST_ENTRY)RemoveHeadList(&listHead);
		ExFreePool(tmpEntry->NameBuffer);
		ExFreePool(tmpEntry);
	}
	return status;
}


