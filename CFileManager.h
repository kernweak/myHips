#pragma once


// CFileManager 对话框

class CFileManager : public CDialogEx
{
	DECLARE_DYNAMIC(CFileManager)

public:
	CFileManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CFileManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FILE_MANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonBrowse();
	CString m_szPath;
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedButtonFilemon();
};


const PWSTR FQDRVPortName = L"\\FQDRVPort";


#define FQDRV_READ_BUFFER_SIZE   1024
//#define MAX_PATH 256



