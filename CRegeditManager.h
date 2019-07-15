#pragma once


// CRegeditManager 对话框

class CRegeditManager : public CDialogEx
{
	DECLARE_DYNAMIC(CRegeditManager)

public:
	CRegeditManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CRegeditManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_REGEDIT_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CString m_szUNC;
	CString m_szLocal;
	afx_msg void OnBnClickedButtonU2l();
};
