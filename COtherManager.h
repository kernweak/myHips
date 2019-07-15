#pragma once


// COtherManager 对话框

class COtherManager : public CDialogEx
{
	DECLARE_DYNAMIC(COtherManager)

public:
	COtherManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~COtherManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_OTHER_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
