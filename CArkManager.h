#pragma once


// CArkManager 对话框

class CArkManager : public CDialogEx
{
	DECLARE_DYNAMIC(CArkManager)

public:
	CArkManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CArkManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_ARK_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
