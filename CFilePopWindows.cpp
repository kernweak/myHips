// CFilePopWindows.cpp : implementation file
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CFilePopWindows.h"
#include "afxdialogex.h"
#define TIMER_ELAPSE_ID 100

// CFilePopWindows dialog

IMPLEMENT_DYNAMIC(CFilePopWindows, CDialogEx)

CFilePopWindows::CFilePopWindows(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FILEPOPWINDOWS, pParent)
	, m_information(_T(""))
	, m_szTime(_T(""))
	, m_lefttime(16)
{
}

CFilePopWindows::~CFilePopWindows()
{
}

void CFilePopWindows::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_information);
	DDX_Text(pDX, IDC_STATIC_TIMER, m_szTime);
}
void CFilePopWindows::SetText(LPCTSTR str)
{
	m_information = str;
}

BEGIN_MESSAGE_MAP(CFilePopWindows, CDialogEx)
	ON_BN_CLICKED(IDOK, &CFilePopWindows::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CFilePopWindows::OnBnClickedCancel)
	ON_WM_TIMER()
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


void CFilePopWindows::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	// TODO: Add your message handler code here and/or call default
	switch (nIDEvent)
	{
	case TIMER_ELAPSE_ID:
		UpdateData(TRUE);
		m_szTime.Format(_T("ªπ £: %2d √Î"), --m_lefttime);
		UpdateData(FALSE);
		if (m_lefttime == 0)
		{
			KillTimer(nIDEvent);
			UpdateData(TRUE);
			CDialog::OnOK();
		}
		break;
	default:
		break;
	}

	CDialog::OnTimer(nIDEvent);
	//CDialogEx::OnTimer(nIDEvent);
}


BOOL CFilePopWindows::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	SetTimer(TIMER_ELAPSE_ID, 1 * 1000, NULL);
	// TODO:  Add extra initialization here
	m_szTime = _T("ªπ £: 16 √Î");
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}
