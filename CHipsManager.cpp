// CHipsManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CHipsManager.h"
#include "afxdialogex.h"
#include "CFileManager.h"

// CHipsManager 对话框
extern pFileRule g_ModuleRule;
IMPLEMENT_DYNAMIC(CHipsManager, CDialogEx)

CHipsManager::CHipsManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_HIPS_MANAGER, pParent)
	, m_ModuleRule(_T(""))
	, m_ruleState(_T(""))
{

}

CHipsManager::~CHipsManager()
{
}

void CHipsManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_ModuleRule);
	DDX_Text(pDX, IDC_EDIT2, m_ruleState);
	DDX_Control(pDX, IDC_LIST1, m_listCtrl);
}


BEGIN_MESSAGE_MAP(CHipsManager, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_ADDRULE, &CHipsManager::OnBnClickedButtonAddrule)
	ON_BN_CLICKED(IDC_BUTTON_DELRULE, &CHipsManager::OnBnClickedButtonDelrule)
	ON_BN_CLICKED(IDC_BUTTON_PAUSEMOUDLE, &CHipsManager::OnBnClickedButtonPausemoudle)
	ON_BN_CLICKED(IDC_BUTTON_RESTARTMOUDLE, &CHipsManager::OnBnClickedButtonRestartmoudle)
END_MESSAGE_MAP()


// CHipsManager 消息处理程序


void CHipsManager::OnBnClickedButtonAddrule()
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_ModuleRule.GetBuffer();
	AddToDriver(p, ADD_MODULE);
	int ret = AddPathList(p, &g_ModuleRule);

	int iLine = 0;
	if (ret)
	{
		iLine = m_listCtrl.GetItemCount();
		m_listCtrl.InsertItem(iLine, p);
	}
	switch (ret)
	{
	case 1:
		m_ruleState = L"添加监控模块成功";
		break;
	case 2:
		m_ruleState = L"添加监控模块失败，已经存在";
		break;
	case 3:
	default:
		m_ruleState = L"添加监控模块失败";
		break;
	}
	UpdateData(FALSE);
	writeToMoudleMonFile();
}


void CHipsManager::OnBnClickedButtonDelrule()
{
	// TODO: Add your control notification handler code here
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_ModuleRule.GetBuffer();
	DeleteFromDriver(p, DELETE_MODULE);

	int ret = DeletePathList(p, &g_ModuleRule);
	int iLine = 0;
	if (ret == 1)
	{
		while (m_listCtrl.DeleteItem(0));
		pFileRule new_filename, current, precurrent;
		current = precurrent = g_ModuleRule;
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
		m_ruleState = L"删除监控模块成功";
		break;
	case 2:
		m_ruleState = L"删除监控模块失败，不存在";
		break;
	case 3:
	default:
		m_ruleState = L"删除监控模块失败";
		break;
	}
	UpdateData(FALSE);
	writeToMoudleMonFile();
}
bool writeToMoudleMonFile()
{
	FILE *fp;
	_wfopen_s(&fp, L".\\MODULERULE.txt", L"w+");
	if (fp == NULL)
		return FALSE;
	pFileRule new_filename, current, precurrent;
	current = precurrent = g_ModuleRule;
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

void CHipsManager::OnBnClickedButtonPausemoudle()
{
	// TODO: Add your control notification handler code here
	Data data;
	void *pData = NULL;

	data.command = PAUSE_MODULE;

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


void CHipsManager::OnBnClickedButtonRestartmoudle()
{
	// TODO: Add your control notification handler code here
	Data data;
	void *pData = NULL;

	data.command = RESTART_MODULE;

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


BOOL CHipsManager::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_listCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_listCtrl.InsertColumn(0, _T("模块监控"), LVCFMT_LEFT, 240);

	FILE *fp;
	int iLine = 0;
	_wfopen_s(&fp, L".\\MODULERULE.txt", L"a+");
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
	// TODO:  Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}
