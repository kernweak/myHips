// COtherManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "COtherManager.h"
#include "afxdialogex.h"


// COtherManager 对话框

IMPLEMENT_DYNAMIC(COtherManager, CDialogEx)

COtherManager::COtherManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_OTHER_MANAGER, pParent)
{

}

COtherManager::~COtherManager()
{
}

void COtherManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(COtherManager, CDialogEx)
END_MESSAGE_MAP()


// COtherManager 消息处理程序
