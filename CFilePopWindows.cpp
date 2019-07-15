// CFilePopWindows.cpp : implementation file
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CFilePopWindows.h"
#include "afxdialogex.h"


// CFilePopWindows dialog

IMPLEMENT_DYNAMIC(CFilePopWindows, CDialogEx)

CFilePopWindows::CFilePopWindows(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FILEPOPWINDOWS, pParent)
	, m_information(_T(""))
{
}

CFilePopWindows::~CFilePopWindows()
{
}

void CFilePopWindows::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_information);
}
void CFilePopWindows::SetText(LPCTSTR str)
{
	m_information = str;
}

BEGIN_MESSAGE_MAP(CFilePopWindows, CDialogEx)
	ON_BN_CLICKED(IDOK, &CFilePopWindows::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CFilePopWindows::OnBnClickedCancel)
END_MESSAGE_MAP()


// CFilePopWindows message handlers


void CFilePopWindows::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	m_allow = TRUE;
	CDialogEx::OnOK();
}


void CFilePopWindows::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	m_allow = FALSE;
	CDialogEx::OnCancel();
}
