// CArkManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CArkManager.h"
#include "afxdialogex.h"


// CArkManager 对话框

IMPLEMENT_DYNAMIC(CArkManager, CDialogEx)

CArkManager::CArkManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_ARK_MANAGER, pParent)
{

}

CArkManager::~CArkManager()
{
}

void CArkManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CArkManager, CDialogEx)
END_MESSAGE_MAP()


// CArkManager 消息处理程序
