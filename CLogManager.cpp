// CLogManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CLogManager.h"
#include "afxdialogex.h"


// CLogManager 对话框

IMPLEMENT_DYNAMIC(CLogManager, CDialogEx)

CLogManager::CLogManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_LOG_MANAGER, pParent)
{

}

CLogManager::~CLogManager()
{
}

void CLogManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CLogManager, CDialogEx)
END_MESSAGE_MAP()


// CLogManager 消息处理程序
