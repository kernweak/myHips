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
	CString m_rule;
	afx_msg void OnBnClickedButtonAdd();
	afx_msg void OnBnClickedButtonDel();
	afx_msg void OnBnClickedButtonPause();
	afx_msg void OnBnClickedButtonRestart();
	CString m_ruleState;
};

const PWSTR FQDRVPortName = L"\\FQDRVPort";


#define FQDRV_READ_BUFFER_SIZE   1024
//#define MAX_PATH 256


//文件目录链表
struct filenames {
	wchar_t filename[MAX_PATH];
	struct filenames* pNext;
};

typedef struct fileRule
{
	WCHAR filePath[MAX_PATH];
	struct fileRule* pNext;
}fileRule,*pFileRule;

typedef struct filenames filenames;

//发送数据结构体
typedef struct Data {
	unsigned long command;
	wchar_t filename[MAX_PATH];

}Data,*pData;
//命令定义
typedef enum _IOMONITOR_COMMAND {
	DEFAULT_PATH,
	ADD_PATH,
	DELETE_PATH,
	CLOSE_PATH,
	OPEN_PATH,
	PAUSE_REGMON,
	RESTART_REGMON,
	DEFAULT_PROCESS,
	ADD_PROCESS,
	DELETE_PROCESS,
	PAUSE_PROCESS,
	RESTART_PROCESS,
} IOMonitorCommand;


//显示路径操作
//void ModifyPathList(wchar_t * filename);
//读取文件内容
//void ReadPath();
WCHAR* NopEnter(WCHAR* str);  // 此处代码是抄的,  但是比较 好懂.
//与驱动通讯
HRESULT SendToDriver(LPVOID lpInBuffer, DWORD dwInBufferSize);

//控制驱动路径
void AddToDriver(WCHAR * filename, IOMonitorCommand cmd);
void DeleteFromDriver(WCHAR * filename,IOMonitorCommand cmd);
void PauseDriver();
void RenewDriver();

//引用层操作规则函数
bool addDefaultRule();
int AddPathList(WCHAR*  filename, pFileRule* headFileRule);
int DeletePathList(WCHAR*  filename, pFileRule* headFileRule);
bool writeToFile();

//注册表通信函数
void PauseRegMon();
void RenewRegMon();


extern pFileRule g_fileRule;
extern pFileRule g_ProcessRule;

bool addDefaultProcessRule();