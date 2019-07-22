#pragma once


// CFirewallManager 对话框

class CFirewallManager : public CDialogEx
{
	DECLARE_DYNAMIC(CFirewallManager)

public:
	CFirewallManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CFirewallManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FIREWALL_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonAdd();
	afx_msg void OnBnClickedButtonDel();
	CString m_ruleState;
	CString m_NetRule;
	CListCtrl m_listCtrl;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonPause();
	afx_msg void OnBnClickedButtonRestart();
};


bool writeToNetMonFile();