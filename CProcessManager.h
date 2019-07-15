#pragma once


// CProcessManager 对话框

class CProcessManager : public CDialogEx
{
	DECLARE_DYNAMIC(CProcessManager)

public:
	CProcessManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CProcessManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_PROCESS_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
