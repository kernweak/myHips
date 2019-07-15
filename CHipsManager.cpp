// CHipsManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CHipsManager.h"
#include "afxdialogex.h"


// CHipsManager 对话框

IMPLEMENT_DYNAMIC(CHipsManager, CDialogEx)

CHipsManager::CHipsManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_HIPS_MANAGER, pParent)
{

}

CHipsManager::~CHipsManager()
{
}

void CHipsManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CHipsManager, CDialogEx)
END_MESSAGE_MAP()


// CHipsManager 消息处理程序
