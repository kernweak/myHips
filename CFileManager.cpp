// CFileManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CFileManager.h"
#include "afxdialogex.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <string.h>
#include <crtdbg.h>
#include <assert.h>
#include <fltUser.h>
#include "CFilePopWindows.h"
#include "scanuk.h"
#include "scanuser.h"
#include <dontuse.h>
#pragma comment(lib,"fltLib.lib")


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCANNER_DEFAULT_REQUEST_COUNT       5
#define SCANNER_DEFAULT_THREAD_COUNT        2
#define SCANNER_MAX_THREAD_COUNT            64





HANDLE g_port = 0;
pFileRule g_fileRule = NULL;
pFileRule g_ProcessRule = NULL;
pFileRule g_ModuleRule = NULL;
typedef struct _SCANNER_THREAD_CONTEXT {

	HANDLE Port;
	HANDLE Completion;

} SCANNER_THREAD_CONTEXT, *PSCANNER_THREAD_CONTEXT;

DWORD HandData(LPCTSTR  tip)
{
		CFilePopWindows m_FilePopWindows;
		m_FilePopWindows.SetText(tip);
		m_FilePopWindows.DoModal();
		if (m_FilePopWindows.m_allow == TRUE)
		{
			return 1;
		}
		else
		{
			return 0;
		}
}
// CFileManager 对话框

IMPLEMENT_DYNAMIC(CFileManager, CDialogEx)

CFileManager::CFileManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FILE_MANAGER, pParent)
	, m_szPath(_T(""))
	, m_rule(_T(""))
	, m_ruleState(_T(""))
{

}

CFileManager::~CFileManager()
{
}

void CFileManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_FILEPATH, m_szPath);
	DDX_Text(pDX, IDC_EDIT_RULE, m_rule);
	DDX_Text(pDX, IDC_EDIT1, m_ruleState);
}


BEGIN_MESSAGE_MAP(CFileManager, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE, &CFileManager::OnBnClickedButtonBrowse)
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_BUTTON_FILEMON, &CFileManager::OnBnClickedButtonFilemon)
	ON_BN_CLICKED(IDC_BUTTON_ADD, &CFileManager::OnBnClickedButtonAdd)
	ON_BN_CLICKED(IDC_BUTTON_DEL, &CFileManager::OnBnClickedButtonDel)
	ON_BN_CLICKED(IDC_BUTTON_PAUSE, &CFileManager::OnBnClickedButtonPause)
	ON_BN_CLICKED(IDC_BUTTON_RESTART, &CFileManager::OnBnClickedButtonRestart)
	ON_BN_CLICKED(IDC_BUTTON_DELFILE, &CFileManager::OnBnClickedButtonDelfile)
END_MESSAGE_MAP()


// CFileManager 消息处理程序


void CFileManager::OnBnClickedButtonBrowse()
{
	// TODO: 在此添加控件通知处理程序代码


	
	
		TCHAR           szFolderPath[MAX_PATH] = { 0 };
		

		BROWSEINFO      sInfo;
		::ZeroMemory(&sInfo, sizeof(BROWSEINFO));
		sInfo.pidlRoot = 0;
		sInfo.lpszTitle = _T("请选择处理结果存储路径");
		sInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_DONTGOBELOWDOMAIN;
		sInfo.lpfn = NULL;

		// 显示文件夹选择对话框  
		LPITEMIDLIST lpidlBrowse = ::SHBrowseForFolder(&sInfo);
		if (lpidlBrowse != NULL)
		{
			// 取得文件夹名  
			if (::SHGetPathFromIDList(lpidlBrowse, szFolderPath))
			{
				m_szPath = szFolderPath;
			}
		}
		if (lpidlBrowse != NULL)
		{
			::CoTaskMemFree(lpidlBrowse);
		}
	UpdateData(FALSE);
}


void CFileManager::OnDropFiles(HDROP hDropInfo)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	CDialogEx::OnDropFiles(hDropInfo);

		// TODO: Add your message handler code here and/or call default
		// TODO: Add your message handler code here and/or call default
	UINT count;
	TCHAR filePath[MAX_PATH] = { 0 };

	count = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
	if (count == 1)
	{
		DragQueryFile(hDropInfo, 0, filePath, sizeof(filePath));
		m_szPath = filePath;
		UpdateData(FALSE);
		DragFinish(hDropInfo);

		CDialog::OnDropFiles(hDropInfo);
		return;

	}
	else
	{
		//m_vectorFile.clear();
		for (UINT i = 0; i < count; i++)
		{
			int pathLen = DragQueryFile(hDropInfo, i, filePath, sizeof(filePath));
			m_szPath = filePath;
			//m_vectorFile.push_back(filePath);
			//break;
		}

		UpdateData(FALSE);
		DragFinish(hDropInfo);
	}

		CDialogEx::OnDropFiles(hDropInfo);
	


}
//此函数需放到线程中执行
DWORD
ScannerWorker(
	__in PSCANNER_THREAD_CONTEXT Context
)
{
	DWORD		dwRet = 0;
	DWORD	    outSize = 0;

	HRESULT		hr = 0;
	ULONG_PTR	key = 0;

	BOOL		result = TRUE;
	//CHAR		strPop[MAX_PATH*2]	= {0};
	WCHAR		strOptions[50 * 2] = { 0 };//操作类型字符串
	WCHAR		LastPath[MAX_PATH * 2] = { 0 };
	BOOL		LastResult = TRUE;

	LPOVERLAPPED pOvlp;
	PSCANNER_NOTIFICATION notification;
	SCANNER_REPLY_MESSAGE replyMessage;
	PSCANNER_MESSAGE message;
	//DWORD dwRet = 0;
	CString tip = NULL;

	memset(LastPath, '\0', MAX_PATH * 2);
#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant

	while (TRUE) {

#pragma warning(pop)

		//
		//  Poll for messages from the filter component to scan.
		//

		result = GetQueuedCompletionStatus(Context->Completion, &outSize, &key, &pOvlp, INFINITE);

		//
		//  Obtain the message: note that the message we sent down via FltGetMessage() may NOT be
		//  the one dequeued off the completion queue: this is solely because there are multiple
		//  threads per single port handle. Any of the FilterGetMessage() issued messages can be
		//  completed in random order - and we will just dequeue a random one.
		//

		message = CONTAINING_RECORD(pOvlp, SCANNER_MESSAGE, Ovlp);

		if (!result) {
			hr = HRESULT_FROM_WIN32(GetLastError());
			break;
		}

		//printf( "Received message, size %d\n", pOvlp->InternalHigh );
		//tip.Format(L"Received message, size %d\n", pOvlp->InternalHigh );


		notification = &message->Notification;

		if (notification->Operation == 1)
		{
			if (wcsncmp(LastPath, notification->TargetPath, wcslen(notification->TargetPath)) == 0)
			{
				memset(LastPath, '\0', MAX_PATH * 2);
				result = LastResult;
				goto EX;
			}
		}

		memset(strOptions, '\0', 50);


		switch (notification->Operation)
		{
		case 1:
			wcscpy_s(strOptions, 50, L"创建");
			break;
		case 2:
			wcscpy_s(strOptions, 50, L"重命名");
			break;
		case 3:
			wcscpy_s(strOptions, 50, L"删除");
			break;
		case 4:
			wcscpy_s(strOptions, 50, L"新键注册表");
			break;
		case 5:
			wcscpy_s(strOptions, 50, L"删除key");
			break;
		case 6:
			wcscpy_s(strOptions, 50, L"设置键值");
			break;
		case 7:
			wcscpy_s(strOptions, 50, L"删除键值");
			break;
		case 8:
			wcscpy_s(strOptions, 50, L"重命名键值");
			break;
		case 9:
			wcscpy_s(strOptions, 50, L"进程创建");
			break;
		case 10:
			wcscpy_s(strOptions, 50, L"模块加载");
			break;
		default:
			wcscpy_s(strOptions, 50, L"爆炸");
			break;
		}

		//memset(strPop,'\0',MAX_PATH*2);
		switch (notification->Operation)
		{
		case 1:
			tip.Format(L"进程:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
			break;
		case 2:
			tip.Format(L"进程:%s\r\n操作:%s\r\n目标:%s\r\n重名为:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath, notification->RePathName);
			break;
		case 3:
			tip.Format(L"进程:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
			break;
		case 4:
			tip.Format(L"注册表路径:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
			break;
		case 5:
			tip.Format(L"注册表路径:%s\r\n操作:%s\r\n是否放行?", notification->ProcessPath, strOptions);
			break;
		case 6:
			tip.Format(L"注册表路径:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
			break;
		case 7:
			tip.Format(L"注册表路径:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
			break;
		case 8:
			tip.Format(L"注册表路径:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
			break;
		case 9:
			tip.Format(L"进程监控:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
			break;
		case 10:
			tip.Format(L"模块监控:%s\r\n操作:%s\r\n是否放行?", notification->ProcessPath, strOptions);
			break;
		default:
			break;
		}

		Sleep(1000);
		//dwRet = MessageBoxA(NULL,strPop,"监控到攻击行为",MB_YESNO);

		dwRet = HandData(tip);

		if (dwRet == 1)
			result = FALSE;
		else
			result = TRUE;

		LastResult = result;
	EX:
		wcsncpy_s(LastPath, MAX_PATH * 2, notification->TargetPath, MAX_PATH * 2);

		replyMessage.ReplyHeader.Status = 0;
		replyMessage.ReplyHeader.MessageId = message->MessageHeader.MessageId;

		replyMessage.Reply.SafeToOpen = !result;

		printf("Replying message, ResultCode: %d\n", replyMessage.Reply.SafeToOpen);

		hr = FilterReplyMessage(Context->Port,
			(PFILTER_REPLY_HEADER)&replyMessage,
			sizeof(replyMessage));

		if (SUCCEEDED(hr)) {

			printf("Replied message\n");

		}
		else {

			printf("Scanner: Error replying message. Error = 0x%X\n", hr);
			break;
		}

		memset(&message->Ovlp, 0, sizeof(OVERLAPPED));

		hr = FilterGetMessage(Context->Port,
			&message->MessageHeader,
			FIELD_OFFSET(SCANNER_MESSAGE, Ovlp),
			&message->Ovlp);

		if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {

			break;
		}
	}

	if (!SUCCEEDED(hr)) {

		if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE)) {

			//
			//  Scanner port disconncted.
			//

			printf("Scanner: Port is disconnected, probably due to scanner filter unloading.\n");

		}
		else {

			printf("Scanner: Unknown error occured. Error = 0x%X\n", hr);
		}
	}

	free(message);

	return hr;
}




//此函数需放到线程中执行
int InitFltUser()
{
	DWORD dwDefRequestCount = SCANNER_DEFAULT_REQUEST_COUNT;
	DWORD dwDefThreadCount = SCANNER_DEFAULT_THREAD_COUNT;
	DWORD dwMaxThreadCount = SCANNER_MAX_THREAD_COUNT;
	SCANNER_THREAD_CONTEXT context;
	PSCANNER_MESSAGE msg;
	HANDLE threads[SCANNER_MAX_THREAD_COUNT];
	CString tip = NULL;
	DWORD threadId;
	DWORD i = 0;

	//连接到端口

	HRESULT hr = FilterConnectCommunicationPort(ScannerPortName, 0, NULL, 0, NULL, &g_port);

	if (IS_ERROR(hr))
	{
		AfxMessageBox(L"hr is null\n");
		return 0;
	}

	//为这个句柄创建一个Comption

	HANDLE completion = CreateIoCompletionPort(g_port,
		NULL,
		0,
		dwDefThreadCount);

	if (completion == NULL)
	{
		tip.Format(L"ERROR: Creating completion port: %d\n", GetLastError());
		AfxMessageBox(tip);
		CloseHandle(g_port);
		return 0;
	}

	//tip.Format(L"Scanner: Port = 0x%p Completion = 0x%p\n", g_port, completion );
	//this->SetWindowTextW(tip);

	context.Port = g_port;
	context.Completion = completion;

	//创建规定的线程


	for (i = 0; i < dwDefThreadCount; i++)
	{
		//创建线程
		threads[i] = CreateThread(NULL,
			0,
			(LPTHREAD_START_ROUTINE)ScannerWorker,
			&context,
			0,
			&threadId);

		if (threads[i] == NULL)
		{
			hr = GetLastError();
			tip.Format(L"ERROR: Couldn't create thread: %d\n", hr);
			//this->SetWindowTextW(tip);
			goto main_cleanup;
		}

		for (DWORD j = 0; j < dwDefRequestCount; j++)
		{

			//
			//  Allocate the message.
			//

#pragma prefast(suppress:__WARNING_MEMORY_LEAK, "msg will not be leaked because it is freed in ScannerWorker")

			msg = (PSCANNER_MESSAGE)malloc(sizeof(SCANNER_MESSAGE));

			if (msg == NULL) {

				hr = ERROR_NOT_ENOUGH_MEMORY;
				goto main_cleanup;
			}

			memset(&msg->Ovlp, 0, sizeof(OVERLAPPED));

			//
			//  Request messages from the filter driver.
			//

			hr = FilterGetMessage(g_port,
				&msg->MessageHeader,
				FIELD_OFFSET(SCANNER_MESSAGE, Ovlp),
				&msg->Ovlp);

			if (hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING))
			{

				free(msg);
				goto main_cleanup;
			}
		}
	}

	hr = S_OK;

	WaitForMultipleObjectsEx(i, threads, TRUE, INFINITE, FALSE);
	//返回线程数组和 数组个数

main_cleanup:

	tip.Format(L"Scanner:  All done. Result = 0x%08x\n", hr);
	//this->SetWindowTextW(tip);
	CloseHandle(g_port);
	CloseHandle(completion);

	return hr + 1;
}

void CFileManager::OnBnClickedButtonFilemon()
{
	// TODO: Add your control notification handler code here
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitFltUser, NULL, 0, NULL);
	bool ret=addDefaultRule();
	addDefaultProcessRule();
	addDefaultModuleRule();
	//if(!ret){
	//	MessageBoxW(NULL, L"读取文件失败", MB_OK);
	//
	//}
}

HRESULT SendToDriver(LPVOID lpInBuffer, DWORD dwInBufferSize)
{
	//通讯端口
	HRESULT hResult = S_OK;
	wchar_t OutBuffer[MAX_PATH] = { 0 };
	DWORD bytesReturned = 0;
	Data *data = (pData)lpInBuffer;
	hResult = FilterSendMessage(g_port, lpInBuffer, dwInBufferSize, OutBuffer, sizeof(OutBuffer), &bytesReturned);
	if (IS_ERROR(hResult)) {
		return hResult;
	}
	//MessageBoxW(NULL, OutBuffer, L"返回结果", MB_OK);
	OutputDebugString(L"从内核发来的信息是:");
	OutputDebugString(OutBuffer);
	OutputDebugString(L"\n");
	return hResult;
}



void AddToDriver(WCHAR * filename, IOMonitorCommand cmd)
{
	Data data;
	Data *pData = NULL;
	data.command = cmd;
	wcscpy_s(data.filename, MAX_PATH, filename);
	pData = &data;
	HRESULT hResult = SendToDriver(pData, sizeof(data));
	if (IS_ERROR(hResult)) {
		OutputDebugString(L"FilterSendMessage fail!\n");
	}
	else
	{
		OutputDebugString(L"FilterSendMessage is ok!\n");
	}
}
void DeleteFromDriver(WCHAR * filename, IOMonitorCommand cmd)
{
	Data data;
	void *pData = NULL;
	data.command = cmd;
	wcscpy_s(data.filename, MAX_PATH, filename);

	pData = &data.command;
	
	HRESULT hResult = SendToDriver(pData, sizeof(data));
	if (IS_ERROR(hResult)) {
		OutputDebugString(L"FilterSendMessage fail!\n");
	}
	else
	{
		OutputDebugString(L"FilterSendMessage is ok!\n");
	}
}
void PauseDriver()
{
	Data data;
	void *pData = NULL;

	data.command = CLOSE_PATH;

	pData = &data.command;

	HRESULT hResult = SendToDriver(pData, sizeof(data));
	if (IS_ERROR(hResult)) {
		OutputDebugString(L"FilterSendMessage fail!\n");
	}
	else
	{
		OutputDebugString(L"FilterSendMessage is ok!\n");
	}
}
void RenewDriver()
{
	Data data;
	void *pData = NULL;

	data.command = OPEN_PATH;

	pData = &data.command;

	HRESULT hResult = SendToDriver(pData, sizeof(data));
	if (IS_ERROR(hResult)) {
		OutputDebugString(L"FilterSendMessage fail!\n");
	}
	else
	{

		OutputDebugString(L"FilterSendMessage is ok!\n");
	}

}



void CFileManager::OnBnClickedButtonAdd()
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_rule.GetBuffer();
	AddToDriver(p,ADD_PATH);
	int ret=AddPathList(p,&g_fileRule);
	switch (ret)
	{
	case 1:
		m_ruleState = L"添加路径成功";
		break;
	case 2:
		m_ruleState = L"添加路径失败，已经存在";
		break;
	case 3:
	default:
		m_ruleState = L"添加路径失败";
		break;
	}
	UpdateData(FALSE);
	writeToFile();
}


void CFileManager::OnBnClickedButtonDel()
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_rule.GetBuffer();
	DeleteFromDriver(p,DELETE_PATH);

	int ret = DeletePathList(p,&g_fileRule);
	switch (ret)
	{
	case 1:
		m_ruleState = L"删除路径成功";
		break;
	case 2:
		m_ruleState = L"删除路径失败，不存在";
		break;
	case 3:
	default:
		m_ruleState = L"删除路径失败";
		break;
	}
	UpdateData(FALSE);
	writeToFile();
}


void CFileManager::OnBnClickedButtonPause()
{
	PauseDriver();
	m_ruleState = L"暂停监控";
	UpdateData(FALSE);
	// TODO: Add your control notification handler code here
}


void CFileManager::OnBnClickedButtonRestart()
{
	// TODO: Add your control notification handler code here
	RenewDriver();
	m_ruleState = L"继续监控";
	UpdateData(FALSE);
}
WCHAR* NopEnter(WCHAR* str)  // 此处代码是抄的,  但是比较 好懂.
{
	WCHAR* p;
	if ((p =wcschr(str, '\n')) != NULL)
		* p = '\0';
	return str;
}
bool addDefaultRule()
{
	FILE *fp; 
	_wfopen_s(&fp,L".\\FILERULE.txt", L"a+");
	if (fp == NULL)
		return FALSE;
	while (!feof(fp))
	{
		WCHAR p[MAX_PATH] = { 0 };
		WCHAR *p1;
		fgetws(p, MAX_PATH, fp);
		p1 = NopEnter(p);		
		AddToDriver(p1,ADD_PATH);
		AddPathList(p1,&g_fileRule);
	}
	fclose(fp);
	return true;
	

}


bool addDefaultProcessRule()
{
	FILE *fp;
	_wfopen_s(&fp, L".\\PROCESSRULE.txt", L"a+");
	if (fp == NULL)
		return FALSE;
	while (!feof(fp))
	{
		WCHAR p[MAX_PATH] = { 0 };
		WCHAR *p1;
		fgetws(p, MAX_PATH, fp);
		p1 = NopEnter(p);
		AddToDriver(p1, ADD_PROCESS);
		AddPathList(p1, &g_ProcessRule);
	}
	fclose(fp);
	return true;


}
bool addDefaultModuleRule()
{
	FILE *fp;
	_wfopen_s(&fp, L".\\MODULERULE.txt", L"a+");
	if (fp == NULL)
		return FALSE;
	while (!feof(fp))
	{
		WCHAR p[MAX_PATH] = { 0 };
		WCHAR *p1;
		fgetws(p, MAX_PATH, fp);
		p1 = NopEnter(p);
		AddToDriver(p1, ADD_MODULE);
		AddPathList(p1, &g_ModuleRule);
	}
	fclose(fp);
	return true;


}


int AddPathList(WCHAR*  filename ,pFileRule* headFileRule)
{
	pFileRule new_filename, current, precurrent;
	new_filename = (pFileRule)malloc(sizeof(fileRule));
	ZeroMemory(new_filename, sizeof(fileRule));
	memcpy_s(new_filename->filePath, MAX_PATH, filename, MAX_PATH);
	//MessageBoxW(NULL, new_filename->filePath, new_filename->filePath, MB_OK);
	new_filename->pNext = NULL;
	__try {
	

		new_filename->pNext = NULL;
		if (NULL == *headFileRule)              //头是空的，路径添加到头
		{
			*headFileRule = new_filename;
			return 1;
		}
		current = *headFileRule;
		while (current != NULL)
		{
			if (!wcscmp(current->filePath,new_filename->filePath))//链表中含有这个路径，返回
			{
				free(new_filename);
				new_filename = NULL;
				return 2;
			}
			precurrent = current;
			current = current->pNext;
		}
		//链表中没有这个路径，添加
		current = *headFileRule;
		while (current->pNext != NULL)
		{
			current = current->pNext;
		}
		current->pNext = new_filename;
		return 1;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		free(new_filename);
		new_filename = NULL;
		return 3;
	}
}

int DeletePathList(WCHAR*  filename, pFileRule* headFileRule)
{
	pFileRule new_filename, current, precurrent;
	current = precurrent = *headFileRule;
	while (current != NULL)
	{
		__try {
			if (!wcscmp(current->filePath, filename))
			{
				//判断一下是否是头,如果是头，就让头指向第二个，删掉第一个
				if (current == *headFileRule)
				{
					*headFileRule = current->pNext;
					free(current);
					current = NULL;
					return 1;
				}
				//如果不是头，删掉当前的
				precurrent->pNext = current->pNext;
				current->pNext = NULL;
				free(current);
				current = NULL;
				return 1;
			}
			precurrent = current;
			current = current->pNext;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return 3;
		}
	}
	return 2;
}

bool writeToFile()
{
	FILE *fp;
	_wfopen_s(&fp, L".\\FILERULE.txt", L"w+");
	if (fp == NULL)
		return FALSE;
	pFileRule new_filename, current, precurrent;
	current = precurrent = g_fileRule;
	while (current != NULL)
	{
		
		fputws(current->filePath, fp);
		if (current->pNext != NULL)
		{
			fputwc('\n', fp);
		}
		current = current->pNext;
	}
	fclose(fp);
	return true;
}


//注册表
void PauseRegMon()
{
	Data data;
	void *pData = NULL;

	data.command = PAUSE_REGMON;

	pData = &data.command;

	HRESULT hResult = SendToDriver(pData, sizeof(data));
	if (IS_ERROR(hResult)) {
		OutputDebugString(L"FilterSendMessage fail!\n");
	}
	else
	{
		OutputDebugString(L"FilterSendMessage is ok!\n");
	}
}


void RenewRegMon()
{
	Data data;
	void *pData = NULL;

	data.command = RESTART_REGMON;

	pData = &data.command;

	HRESULT hResult = SendToDriver(pData, sizeof(data));
	if (IS_ERROR(hResult)) {
		OutputDebugString(L"FilterSendMessage fail!\n");
	}
	else
	{
		OutputDebugString(L"FilterSendMessage is ok!\n");
	}
}

BOOL CFileManager::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  Add extra initialization here
	ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
	ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);       // 0x0049 == WM_COPYGLOBALDATA

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}


void CFileManager::OnBnClickedButtonDelfile()
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_szPath.GetBuffer();
	DeleteFromDriver(p, DELETE_FILE);
}
