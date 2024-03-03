#include "stdafx.h"
#include "Includes.h"
#include "BuildAll.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBuildAll dialog


CBuildAll::CBuildAll(CString strPath, bool bPreValidate,
					CWnd* pParent)
	: CDialog(CBuildAll::IDD, pParent)
{
	//{{AFX_DATA_INIT(CBuildAll)
	m_bPreValidateCARs = bPreValidate;
	m_strBuildPath = strPath;
	//}}AFX_DATA_INIT
}


void CBuildAll::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBuildAll)
	DDX_Check(pDX, IDC_CHECK_PREVALIDATECARS, m_bPreValidateCARs);
	DDX_Text(pDX, IDC_EDIT_BUILDPATH, m_strBuildPath);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBuildAll, CDialog)
	//{{AFX_MSG_MAP(CBuildAll)
	ON_BN_CLICKED(IDC_BUILDPATH_BROWSE, OnBuildPathBrowse)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBuildAll message handlers

void CBuildAll::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void CBuildAll::OnOK() 
{
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}

void CBuildAll::GetData(CString& strPath, bool& bPreValidate)
{
	strPath		= m_strBuildPath;
	bPreValidate= !!m_bPreValidateCARs;
}

void CBuildAll::OnBuildPathBrowse()
{
	CFolderPickerDialog theDialog;
	//theDialog.m_ofn.lpstrInitialDir = _T("C:\\");

	int result = theDialog.DoModal();
	if (result != IDOK)
	{
		return;
	}

	m_strBuildPath = theDialog.GetPathName();

	Filename_RemoveBASEQ(m_strBuildPath);

	// May need to add a '\' for usage in GUI and for later file saving, 
	// as there is no '\' on the returned name
	m_strBuildPath += _T("/");
	UpdateData(FALSE);
}
