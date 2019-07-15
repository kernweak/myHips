// CFirewallManager.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "CFirewallManager.h"
#include "afxdialogex.h"


// CFirewallManager 对话框

IMPLEMENT_DYNAMIC(CFirewallManager, CDialogEx)

CFirewallManager::CFirewallManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_FIREWALL_MANAGER, pParent)
{

}

CFirewallManager::~CFirewallManager()
{
}

void CFirewallManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CFirewallManager, CDialogEx)
END_MESSAGE_MAP()


// CFirewallManager 消息处理程序
