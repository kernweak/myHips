// CFirewallManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CFirewallManager.h"
#include "afxdialogex.h"
#include "CFileManager.h"

// CFirewallManager 对话框
extern pFileRule g_NetRule;
IMPLEMENT_DYNAMIC(CFirewallManager, CDialogEx)

CFirewallManager::CFirewallManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FIREWALL_MANAGER, pParent)
	, m_ruleState(_T(""))
	, m_NetRule(_T(""))
{

}

CFirewallManager::~CFirewallManager()
{
}

void CFirewallManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT2, m_ruleState);
	DDX_Text(pDX, IDC_EDIT1, m_NetRule);
	DDX_Control(pDX, IDC_LIST1, m_listCtrl);
}


BEGIN_MESSAGE_MAP(CFirewallManager, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_ADD, &CFirewallManager::OnBnClickedButtonAdd)
	ON_BN_CLICKED(IDC_BUTTON_DEL, &CFirewallManager::OnBnClickedButtonDel)
	ON_BN_CLICKED(IDC_BUTTON_PAUSE, &CFirewallManager::OnBnClickedButtonPause)
	ON_BN_CLICKED(IDC_BUTTON_RESTART, &CFirewallManager::OnBnClickedButtonRestart)
END_MESSAGE_MAP()


// CFirewallManager 消息处理程序


void CFirewallManager::OnBnClickedButtonAdd()
{
	// TODO: Add your control notification handler code here
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_NetRule.GetBuffer();
	AddToDriver(p, ADD_NETREJECT);
	int ret = AddPathList(p, &g_NetRule);

	int iLine = 0;
	if (ret)
	{
		iLine = m_listCtrl.GetItemCount();
		m_listCtrl.InsertItem(iLine, p);
	}
	switch (ret)
	{
	case 1:
		m_ruleState = L"添加关闭网络进程成功";
		break;
	case 2:
		m_ruleState = L"添加关闭网络进程失败，已经存在";
		break;
	case 3:
	default:
		m_ruleState = L"添加关闭网络进程失败";
		break;
	}
	UpdateData(FALSE);
	writeToNetMonFile();
}


void CFirewallManager::OnBnClickedButtonDel()
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_NetRule.GetBuffer();
	DeleteFromDriver(p, DELETE_NETREJECT);

	int ret = DeletePathList(p, &g_NetRule);
	int iLine = 0;
	if (ret == 1)
	{
		while (m_listCtrl.DeleteItem(0));
		pFileRule new_filename, current, precurrent;
		current = precurrent = g_NetRule;
		while (current != NULL)
		{
			int iLine = m_listCtrl.GetItemCount();
			m_listCtrl.InsertItem(iLine, current->filePath);
			current = current->pNext;
		}
		UpdateData(FALSE);
	}
	switch (ret)
	{
	case 1:
		m_ruleState = L"删除关闭网络进程成功";
		break;
	case 2:
		m_ruleState = L"删除关闭网络进程失败，不存在";
		break;
	case 3:
	default:
		m_ruleState = L"删除关闭网络进程失败";
		break;
	}
	UpdateData(FALSE);
	writeToNetMonFile();
}


BOOL CFirewallManager::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  Add extra initialization here

	m_listCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_listCtrl.InsertColumn(0, _T("禁止访问网络的进程"), LVCFMT_LEFT, 240);

	FILE *fp;
	int iLine = 0;
	_wfopen_s(&fp, L".\\NETRULE.txt", L"a+");
	if (fp == NULL)
		return FALSE;
	while (!feof(fp))
	{
		int ret = 0;
		WCHAR p[MAX_PATH] = { 0 };
		WCHAR *p1;
		fgetws(p, MAX_PATH, fp);
		p1 = NopEnter(p);
		m_listCtrl.InsertItem(iLine, p1);
	}
	fclose(fp);


	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}
bool writeToNetMonFile()
{
	FILE *fp;
	_wfopen_s(&fp, L".\\NETRULE.txt", L"w+");
	if (fp == NULL)
		return FALSE;
	pFileRule new_filename, current, precurrent;
	current = precurrent = g_NetRule;
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

void CFirewallManager::OnBnClickedButtonPause()
{
	// TODO: Add your control notification handler code here
	Data data;
	void *pData = NULL;

	data.command = PAUSE_NETMON;

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


void CFirewallManager::OnBnClickedButtonRestart()
{
	// TODO: Add your control notification handler code here
	Data data;
	void *pData = NULL;

	data.command = RESTART_NETMON;

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
