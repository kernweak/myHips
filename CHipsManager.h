#pragma once


// CHipsManager 对话框

class CHipsManager : public CDialogEx
{
	DECLARE_DYNAMIC(CHipsManager)

public:
	CHipsManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CHipsManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_HIPS_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
