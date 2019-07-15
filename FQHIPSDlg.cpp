
// FQHIPSDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "FQHIPS.h"
#include "FQHIPSDlg.h"
#include "afxdialogex.h"

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
{
	m_hIcon = AfxGetApp()->LoadIcon(IDI_FQICON);
}

void CFQHIPSDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_MJFUNCTON, m_tabCtrl);
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
	m_tabCtrl.InsertItem(0, _T("日志管理"));
	m_tabCtrl.InsertItem(1, _T("主动防御"));
	m_tabCtrl.InsertItem(2, _T("文件管理"));
	m_tabCtrl.InsertItem(3, _T("注册表管理"));
	m_tabCtrl.InsertItem(4, _T("进程管理"));
	m_tabCtrl.InsertItem(5, _T("防火墙"));
	m_tabCtrl.InsertItem(6, _T("Ark功能"));
	m_tabCtrl.InsertItem(7, _T("其他功能"));
	m_tabCtrl.SetPadding(19);
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
