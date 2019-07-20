// CProcessManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CProcessManager.h"
#include "afxdialogex.h"
#include "CFileManager.h"

extern pFileRule g_ProcessRule;
// CProcessManager 对话框

IMPLEMENT_DYNAMIC(CProcessManager, CDialogEx)

CProcessManager::CProcessManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PROCESS_MANAGER, pParent)
	, m_ProcessRule(_T(""))
	, m_ruleState(_T(""))
{

}

CProcessManager::~CProcessManager()
{
}

void CProcessManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_ProcessRule);
	DDX_Text(pDX, IDC_EDIT2, m_ruleState);
}


BEGIN_MESSAGE_MAP(CProcessManager, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_ADD, &CProcessManager::OnBnClickedButtonAdd)
	ON_BN_CLICKED(IDC_BUTTON_DEL, &CProcessManager::OnBnClickedButtonDel)
	ON_BN_CLICKED(IDC_BUTTON_PAUSE, &CProcessManager::OnBnClickedButtonPause)
	ON_BN_CLICKED(IDC_BUTTON_RESTART, &CProcessManager::OnBnClickedButtonRestart)
END_MESSAGE_MAP()


// CProcessManager 消息处理程序


void CProcessManager::OnBnClickedButtonAdd()
{
	// TODO: Add your control notification handler code here
		// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_ProcessRule.GetBuffer();
	AddToDriver(p, ADD_PROCESS);
	int ret = AddPathList(p, &g_ProcessRule);
	switch (ret)
	{
	case 1:
		m_ruleState = L"添加路径成功";
		break;
	case 2:
		m_ruleState = L"添加路径失败，已经存在";
		break;
	case 3:
	default:
		m_ruleState = L"添加路径失败";
		break;
	}
	UpdateData(FALSE);
	writeToProcessMonFile();
}

bool writeToProcessMonFile()
{
	FILE *fp;
	_wfopen_s(&fp, L".\\PROCESSRULE.txt", L"w+");
	if (fp == NULL)
		return FALSE;
	pFileRule new_filename, current, precurrent;
	current = precurrent = g_ProcessRule;
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


void CProcessManager::OnBnClickedButtonDel()
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	WCHAR * p = m_ProcessRule.GetBuffer();
	DeleteFromDriver(p, DELETE_PROCESS);

	int ret = DeletePathList(p, &g_ProcessRule);
	switch (ret)
	{
	case 1:
		m_ruleState = L"删除路径成功";
		break;
	case 2:
		m_ruleState = L"删除路径失败，不存在";
		break;
	case 3:
	default:
		m_ruleState = L"删除路径失败";
		break;
	}
	UpdateData(FALSE);
	writeToFile();
}


void CProcessManager::OnBnClickedButtonPause()
{
	// TODO: Add your control notification handler code here
	Data data;
	void *pData = NULL;

	data.command = PAUSE_PROCESS;

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


void CProcessManager::OnBnClickedButtonRestart()
{
	// TODO: Add your control notification handler code here
	Data data;
	void *pData = NULL;

	data.command = RESTART_PROCESS;

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

//WCHAR* NopEnter(WCHAR* str)  // 此处代码是抄的,  但是比较 好懂.
//{
//	WCHAR* p;
//	if ((p = wcschr(str, '\n')) != NULL)
//		* p = '\0';
//	return str;
//}

BOOL CProcessManager::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  Add extra initialization here
	//addDefaultProcessRule();
	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}
