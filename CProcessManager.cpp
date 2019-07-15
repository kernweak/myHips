// CProcessManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CProcessManager.h"
#include "afxdialogex.h"


// CProcessManager 对话框

IMPLEMENT_DYNAMIC(CProcessManager, CDialogEx)

CProcessManager::CProcessManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PROCESS_MANAGER, pParent)
{

}

CProcessManager::~CProcessManager()
{
}

void CProcessManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CProcessManager, CDialogEx)
END_MESSAGE_MAP()


// CProcessManager 消息处理程序
