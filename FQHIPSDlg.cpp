
// FQHIPSDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "FQHIPSDlg.h"
#include "afxdialogex.h"


#include <windows.h>
#include <winioctl.h>
#include <winsvc.h>
#include <stdio.h>
#include <conio.h>
#define DRIVER_NAME L"FQDrv"
#define DRIVER_PATH L".\\FQDrv.sys"
#define DRIVER_ALTITUDE	 L"265000" //有同学反馈，他这里没有使用UNICODE编码，导致安装不生效

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define		WM_ICON_NOTIFY	WM_USER+100//托盘右下角消息

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CFQHIPSDlg 对话框



CFQHIPSDlg::CFQHIPSDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FQHIPS_DIALOG, pParent)
	, m_drvState(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDI_FQICON);
}

void CFQHIPSDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_MJFUNCTON, m_tabCtrl);
	DDX_Text(pDX, IDC_EDIT1, m_drvState);
}

BEGIN_MESSAGE_MAP(CFQHIPSDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MJFUNCTON, &CFQHIPSDlg::OnTcnSelchangeTab1)
	ON_WM_CLOSE()
	ON_MESSAGE(WM_ICON_NOTIFY, OnTrayNotification)
	ON_COMMAND(ID_TRAY_RESTORE, &CFQHIPSDlg::OnTrayRestore)
	ON_COMMAND(ID_TRAY_EXIT, &CFQHIPSDlg::OnTrayExit)
	ON_BN_CLICKED(IDC_BUTTON_STARTDRV, &CFQHIPSDlg::OnBnClickedButtonStartdrv)
	ON_BN_CLICKED(IDC_BUTTON_PAUSEDRV, &CFQHIPSDlg::OnBnClickedButtonPausedrv)
	ON_BN_CLICKED(IDC_BUTTON_INSTALLDRV, &CFQHIPSDlg::OnBnClickedButtonInstalldrv)
	ON_BN_CLICKED(IDC_BUTTON_UNINSTALLDRV, &CFQHIPSDlg::OnBnClickedButtonUninstalldrv)
END_MESSAGE_MAP()


// CFQHIPSDlg 消息处理程序

BOOL CFQHIPSDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码


	//为tabControl添加标签
	m_tabCtrl.InsertItem(0, _T("程序引导"));
	m_tabCtrl.InsertItem(1, _T("模块监控"));
	m_tabCtrl.InsertItem(2, _T("文件管理"));
	m_tabCtrl.InsertItem(3, _T("注册表管理"));
	m_tabCtrl.InsertItem(4, _T("进程管理"));
	m_tabCtrl.InsertItem(5, _T("防火墙"));
	m_tabCtrl.InsertItem(6, _T("Ark功能（未完成）"));
	m_tabCtrl.InsertItem(7, _T("其他功能（关闭）"));
	m_tabCtrl.SetPadding(10);
	m_tabCtrl.SetMinTabWidth(50);
	m_tabCtrl.SetItemSize(CSize(50, 25));
	//创建子对话框
	m_LogManager.Create(IDD_DIALOG_LOG_MANAGER, &m_tabCtrl);
	m_HipsManager.Create(IDD_DIALOG_HIPS_MANAGER, &m_tabCtrl);
	m_FileManager.Create(IDD_DIALOG_FILE_MANAGER, &m_tabCtrl);
	m_RegeditManager.Create(IDD_DIALOG_REGEDIT_MANAGER, &m_tabCtrl);
	m_ProcessManager.Create(IDD_DIALOG_PROCESS_MANAGER, &m_tabCtrl);
	m_FirewallManager.Create(IDD_DIALOG_FIREWALL_MANAGER, &m_tabCtrl);
	m_ArkManager.Create(IDD_DIALOG_ARK_MANAGER, &m_tabCtrl);
	m_OtherManager.Create(IDD_DIALOG_OTHER_MANAGER, &m_tabCtrl);

	//设定在Tab内显示的范围,将子对话框拖动到主对话框内
	CRect rc;
	m_tabCtrl.GetClientRect(rc);
	rc.top += 25;
	// 	rc.bottom -= 8;
	// 	rc.left += 8;
	// 	rc.right -= 8;

	m_LogManager.MoveWindow(&rc);
	m_HipsManager.MoveWindow(&rc);
	m_FileManager.MoveWindow(&rc);
	m_RegeditManager.MoveWindow(&rc);
	m_ProcessManager.MoveWindow(&rc);
	m_FirewallManager.MoveWindow(&rc);
	m_ArkManager.MoveWindow(&rc);
	m_OtherManager.MoveWindow(&rc);

	//把子对话框指针保存在主对话框的数组中
	m_pDialog[0] = &m_LogManager;
	m_pDialog[1] = &m_HipsManager;
	m_pDialog[2] = &m_FileManager;
	m_pDialog[3] = &m_RegeditManager;
	m_pDialog[4] = &m_ProcessManager;
	m_pDialog[5] = &m_FirewallManager;
	m_pDialog[6] = &m_ArkManager;
	m_pDialog[7] = &m_OtherManager;

	//显示初始界面

	m_pDialog[0]->ShowWindow(SW_SHOW);
	m_pDialog[1]->ShowWindow(SW_HIDE);
	m_pDialog[2]->ShowWindow(SW_HIDE);
	m_pDialog[3]->ShowWindow(SW_HIDE);
	m_pDialog[4]->ShowWindow(SW_HIDE);
	m_pDialog[5]->ShowWindow(SW_HIDE);
	m_pDialog[6]->ShowWindow(SW_HIDE);
	m_pDialog[7]->ShowWindow(SW_HIDE);
	//保存当前选择
	m_CurSelTab = 0;


	//增加程序托盘功能
	NOTIFYICONDATA m_tnid;

	m_tnid.cbSize = sizeof(NOTIFYICONDATA);//设置结构大小// 
	m_tnid.hWnd = this->m_hWnd;//设置图标对应的窗口 
	m_tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;//图标属性 
	m_tnid.uCallbackMessage = WM_ICON_NOTIFY;//应用程序定义的回调消息ID

	CString szToolTip;
	szToolTip = _T("非酋主防");
	_tcscpy_s(m_tnid.szTip, szToolTip);//帮助信息 
	m_tnid.uID = IDR_MAINFRAME;//应用程序图标  
	m_tnid.hIcon = m_hIcon;//图标句柄 
	PNOTIFYICONDATA m_ptnid = &m_tnid;
	::Shell_NotifyIcon(NIM_ADD, m_ptnid);//增加图标到系统盘

	bool ret1=InstallDriver(DRIVER_NAME, DRIVER_PATH, DRIVER_ALTITUDE);
	if (!ret1)
	{
		MessageBoxW(NULL, L"安装失败，请检查文件是否完整", MB_OK);
	}
	bool ret = InstallDriver(DRIVER_NAME, DRIVER_PATH, DRIVER_ALTITUDE);
	if (!ret)
	{
		m_drvState = L"安装驱动失败";
	}
	else
	{
		m_drvState = L"安装驱动成功";
	}
	UpdateData(FALSE);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CFQHIPSDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
	//如果最小化，隐藏到托盘
	if (nID == SC_MINIMIZE)
	{
		ShowWindow(FALSE); //隐藏窗口
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CFQHIPSDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CFQHIPSDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CFQHIPSDlg::OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	m_pDialog[m_CurSelTab]->ShowWindow(SW_HIDE);
	m_CurSelTab = m_tabCtrl.GetCurSel();
	if (m_pDialog[m_CurSelTab])
	{
		m_pDialog[m_CurSelTab]->ShowWindow(SW_SHOW);
	}
	*pResult = 0;
}


void CFQHIPSDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	NOTIFYICONDATA   nd = { 0 };
	//实现关闭程序托盘也消失的功能
	nd.cbSize = sizeof(NOTIFYICONDATA);
	nd.hWnd = m_hWnd;
	nd.uID = IDR_MAINFRAME;
	nd.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nd.uCallbackMessage = WM_ICON_NOTIFY;
	nd.hIcon = m_hIcon;

	Shell_NotifyIcon(NIM_DELETE, &nd);
	CDialogEx::OnClose();
}

//托盘左右键函数
LRESULT CFQHIPSDlg::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
	switch (lParam)
	{
	case WM_LBUTTONDOWN:
	{
		AfxGetApp()->m_pMainWnd->ShowWindow(SW_SHOWNORMAL);
		SetForegroundWindow();
		break;
	}
	case WM_RBUTTONUP:
	{
		POINT point;
		HMENU hMenu, hSubMenu;
		GetCursorPos(&point); //鼠标位置
		hMenu = LoadMenu(NULL,
			MAKEINTRESOURCE(IDR_MENU_TRAY)); // 加载菜单
		hSubMenu = GetSubMenu(hMenu, 0);//得到子菜单(因为弹出式菜单是子菜单)
		SetForegroundWindow(); // 激活窗口并置前

		TrackPopupMenu(hSubMenu, 0,
			point.x, point.y, 0, m_hWnd, NULL);

	}
	}
	return 1;
}

void CFQHIPSDlg::OnTrayRestore()
{
	// TODO: 在此添加命令处理程序代码
	AfxGetApp()->m_pMainWnd->ShowWindow(SW_SHOWNORMAL);
	SetForegroundWindow();
}


void CFQHIPSDlg::OnTrayExit()
{
	OnClose();
	// TODO: 在此添加命令处理程序代码
}


BOOL InstallDriver(const WCHAR* lpszDriverName, const WCHAR* lpszDriverPath, const WCHAR* lpszAltitude)
{
	WCHAR    szTempStr[MAX_PATH];
	HKEY    hKey;
	DWORD    dwData;
	WCHAR    szDriverImagePath[MAX_PATH];

	if (NULL == lpszDriverName || NULL == lpszDriverPath)
	{
		return FALSE;
	}
	//得到完整的驱动路径
	GetFullPathName(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

	SC_HANDLE hServiceMgr = NULL;// SCM管理器的句柄
	SC_HANDLE hService = NULL;// NT驱动程序的服务句柄

	//打开服务控制管理器
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		// OpenSCManager失败
		CloseServiceHandle(hServiceMgr);
		return FALSE;
	}
	// OpenSCManager成功  

	//创建驱动所对应的服务
	hService = CreateService(hServiceMgr,
		lpszDriverName,             // 驱动程序的在注册表中的名字
		lpszDriverName,             // 注册表驱动程序的DisplayName 值
		SERVICE_ALL_ACCESS,         // 加载驱动程序的访问权限
		SERVICE_FILE_SYSTEM_DRIVER, // 表示加载的服务是文件系统驱动程序
		SERVICE_DEMAND_START,       // 注册表驱动程序的Start 值
		SERVICE_ERROR_NORMAL,       // 注册表驱动程序的ErrorControl 值
		szDriverImagePath,          // 注册表驱动程序的ImagePath 值
		L"FQDrv Content Montior",// 注册表驱动程序的Group 值
		NULL,
		L"FltMgr",                   // 注册表驱动程序的DependOnService 值
		NULL,
		NULL);

	if (hService == NULL)
	{
		if (GetLastError() == ERROR_SERVICE_EXISTS)
		{
			//服务创建失败，是由于服务已经创立过
			CloseServiceHandle(hService);       // 服务句柄
			CloseServiceHandle(hServiceMgr);    // SCM句柄
			return TRUE;
		}
		else
		{
			CloseServiceHandle(hService);       // 服务句柄
			CloseServiceHandle(hServiceMgr);    // SCM句柄
			return FALSE;
		}
	}
	CloseServiceHandle(hService);       // 服务句柄
	CloseServiceHandle(hServiceMgr);    // SCM句柄

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances子健下的键值项 
	//-------------------------------------------------------------------------------------------------------
	wcscpy_s(szTempStr,L"SYSTEM\\CurrentControlSet\\Services\\");
	wcscat_s(szTempStr, lpszDriverName);
	wcscat_s(szTempStr, L"\\Instances");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// 注册表驱动程序的DefaultInstance 值 
	wsprintf(szTempStr, L"\%ls Instance", lpszDriverName);
	//wcscpy_s(szTempStr, lpszDriverName);
	//wcscat_s(szTempStr, L" Instance");
	if (RegSetValueEx(hKey, L"DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)(wcslen(szTempStr)*sizeof(WCHAR))) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	RegFlushKey(hKey);//刷新注册表
	RegCloseKey(hKey);


	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance子健下的键值项 
	//-------------------------------------------------------------------------------------------------------
	wcscpy_s(szTempStr, L"SYSTEM\\CurrentControlSet\\Services\\");
	wcscat_s(szTempStr, lpszDriverName);
	wcscat_s(szTempStr, L"\\Instances\\");
	wcscat_s(szTempStr, lpszDriverName);
	wcscat_s(szTempStr, L" Instance");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// 注册表驱动程序的Altitude 值
	wcscpy_s(szTempStr, lpszAltitude);
	if (RegSetValueEx(hKey, L"Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)(wcslen(szTempStr)*sizeof(WCHAR))) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// 注册表驱动程序的Flags 值
	dwData = 0x0;
	if (RegSetValueEx(hKey, L"Flags", 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	RegFlushKey(hKey);//刷新注册表
	RegCloseKey(hKey);

	return TRUE;
}


BOOL StartDriver(const WCHAR* lpszDriverName)
{
	SC_HANDLE schManager;
	SC_HANDLE schService;
	SERVICE_STATUS svcStatus;

	if (NULL == lpszDriverName)
	{
		return FALSE;
	}

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		CloseServiceHandle(schManager);
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}

	if (!StartService(schService, 0, NULL))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		int i = GetLastError();
		CString STemp;
		STemp.Format(_T("%d"), i);
		MessageBox(NULL,STemp,STemp,MB_OK);
		if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
		{
			// 服务已经开启
			return TRUE;
		}
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}

BOOL StopDriver(const WCHAR* lpszDriverName)
{
	SC_HANDLE        schManager;
	SC_HANDLE        schService;
	SERVICE_STATUS    svcStatus;
	bool            bStopped = false;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return FALSE;
	}
	if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}

BOOL DeleteDriver(const WCHAR* lpszDriverName)
{
	SC_HANDLE        schManager;
	SC_HANDLE        schService;
	SERVICE_STATUS    svcStatus;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return FALSE;
	}
	ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus);
	if (!DeleteService(schService))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}


void CFQHIPSDlg::OnBnClickedButtonStartdrv()
{
	// TODO: Add your control notification handler code here
	bool ret = StartDriver(DRIVER_NAME);
	if (!ret)
	{
		m_drvState = L"开启驱动失败";
	}
	else
	{
		m_drvState = L"开启驱动成功";
	}	
	UpdateData(FALSE);
}




void CFQHIPSDlg::OnBnClickedButtonPausedrv()
{
	// TODO: Add your control notification handler code here
	bool ret=StopDriver(DRIVER_NAME);
	if (!ret)
	{
		m_drvState = L"暂停驱动失败";
	}
	else
	{
		m_drvState = L"暂停驱动成功";
	}
	UpdateData(FALSE);
}


void CFQHIPSDlg::OnBnClickedButtonInstalldrv()
{
	// TODO: Add your control notification handler code here
	bool ret = InstallDriver(DRIVER_NAME, DRIVER_PATH, DRIVER_ALTITUDE);
	if (!ret)
	{
		m_drvState = L"安装驱动失败";
	}
	else
	{
		m_drvState = L"安装驱动成功";
	}
	UpdateData(FALSE);
}


void CFQHIPSDlg::OnBnClickedButtonUninstalldrv()
{
	// TODO: Add your control notification handler code here
	bool ret=DeleteDriver(DRIVER_NAME);
	if (!ret)
	{
		m_drvState = L"卸载驱动失败";
	}
	else
	{
		m_drvState = L"卸载驱动成功";
	}
	UpdateData(FALSE);
}
