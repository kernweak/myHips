#pragma once


// CLogManager 对话框

class CLogManager : public CDialogEx
{
	DECLARE_DYNAMIC(CLogManager)

public:
	CLogManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CLogManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_LOG_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
