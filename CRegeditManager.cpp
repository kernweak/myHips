// CRegeditManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CRegeditManager.h"
#include "afxdialogex.h"


// CRegeditManager 对话框

IMPLEMENT_DYNAMIC(CRegeditManager, CDialogEx)

CRegeditManager::CRegeditManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_REGEDIT_MANAGER, pParent)
	, m_szUNC(_T(""))
	, m_szLocal(_T(""))
{

}

CRegeditManager::~CRegeditManager()
{
}

void CRegeditManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_UNC, m_szUNC);
	DDX_Text(pDX, IDC_EDIT_LOCAL, m_szLocal);
}


BEGIN_MESSAGE_MAP(CRegeditManager, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_U2L, &CRegeditManager::OnBnClickedButtonU2l)
END_MESSAGE_MAP()


// CRegeditManager 消息处理程序


void CRegeditManager::OnBnClickedButtonU2l()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	
}
