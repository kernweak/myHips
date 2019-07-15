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
{

}

CFileManager::~CFileManager()
{
}

void CFileManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_FILEPATH, m_szPath);
}


BEGIN_MESSAGE_MAP(CFileManager, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE, &CFileManager::OnBnClickedButtonBrowse)
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_BUTTON_FILEMON, &CFileManager::OnBnClickedButtonFilemon)
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
		default:
			wcscpy_s(strOptions, 50, L"爆炸");
			break;
		}

		//memset(strPop,'\0',MAX_PATH*2);
		if (notification->Operation == 2)
		{
			//sprintf(strPop,"进程:%S\r\n操作:%s\r\n目标:%S\r\n重名为:%S\r\n是否放行?",notification->ProcessPath,strOptions,notification->TargetPath,notification->RePathName);
			tip.Format(L"进程:%s\r\n操作:%s\r\n目标:%s\r\n重名为:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath, notification->RePathName);
		}
		else
		{
			//sprintf(strPop,"进程:%S\r\n操作:%s\r\n目标:%S\r\n是否放行?",notification->ProcessPath,strOptions,notification->TargetPath);
			tip.Format(L"进程:%s\r\n操作:%s\r\n目标:%s\r\n是否放行?", notification->ProcessPath, strOptions, notification->TargetPath);
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
}

