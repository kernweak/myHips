#pragma once


// CFilePopWindows dialog

class CFilePopWindows : public CDialogEx
{
	DECLARE_DYNAMIC(CFilePopWindows)

public:
	CFilePopWindows(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CFilePopWindows();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FILEPOPWINDOWS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_information;
	bool m_allow;
	void SetText(LPCTSTR str);
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
};
