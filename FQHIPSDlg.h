
// FQHIPSDlg.h: 头文件
//

#pragma once
#include "CArkManager.h"
#include "CFileManager.h"
#include "CFirewallManager.h"
#include "CHipsManager.h"
#include "CLogManager.h"
#include "COtherManager.h"
#include "CProcessManager.h"
#include "CRegeditManager.h"

#define MAX_DLG_PAGE 8//子对话框数量

// CFQHIPSDlg 对话框
class CFQHIPSDlg : public CDialogEx
{
// 构造
public:
	CFQHIPSDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FQHIPS_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult);
	CTabCtrl m_tabCtrl;


	//定义其他对话框成员对象,做对话框组
	

	

	CLogManager      m_LogManager;
	CHipsManager     m_HipsManager;
	CFileManager     m_FileManager;
	CRegeditManager  m_RegeditManager;
	CProcessManager  m_ProcessManager;
	CFirewallManager m_FirewallManager;
	CArkManager      m_ArkManager;
	COtherManager    m_OtherManager;



	CDialog		 *m_pDialog[MAX_DLG_PAGE];
	int			 m_CurSelTab;
	


	afx_msg void OnClose();
	//添加托盘左右键回调函数
	LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);

	afx_msg void OnTrayRestore();
	afx_msg void OnTrayExit();
	afx_msg void OnBnClickedButtonStartdrv();
	afx_msg void OnBnClickedButtonPausedrv();
	afx_msg void OnBnClickedButtonInstalldrv();
	afx_msg void OnBnClickedButtonUninstalldrv();
	CString m_drvState;
};

BOOL InstallDriver(const WCHAR* lpszDriverName, const WCHAR* lpszDriverPath, const WCHAR* lpszAltitude);
BOOL StartDriver(const WCHAR* lpszDriverName);
BOOL StopDriver(const WCHAR* lpszDriverName);
BOOL DeleteDriver(const WCHAR* lpszDriverName);
