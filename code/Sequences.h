// Sequences.h

#define MAX_ADDITIONAL_SEQUENCES 15
#define iDEFAULTSEQFRAMESPEED 20

class CModel;

enum 
{
	TK_ASTERISK = TK_USERDEF,
	TK_ASE_FIRSTFRAME,
	TK_ASE_LASTFRAME,
	TK_ASE_FRAMESPEED,
};


// note that these are also in order of importance for the treeview sorting
//
typedef enum
{
	ET_FACE = 0,
	ET_BOTH,
	ET_TORSO,
	ET_LEGS,
	ET_VM,
	ET_INVALID,

} ENUMTYPE;

//

class CSequence
{
public:
	CSequence();
	virtual ~CSequence();

	virtual void Delete();
	static CSequence* _Create(bool bGenLoopFrame, bool bIsGLA, LPCTSTR path = NULL, int startFrame = 0, int targetFrame = 0, int frameCount = 0, int frameSpeed = iDEFAULTSEQFRAMESPEED, int frameSpeedFromHeader = iDEFAULTSEQFRAMESPEED, CComment* comments = NULL);
	static CSequence* CreateFromFile(LPCTSTR path, CComment* comments);

	void Write(CTxtFile* file, bool bPreQuat);
	void WriteExternal(CModel *pModel, CTxtFile* file);
	void ReadHeader();
	void ReadASEHeader(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed, bool bCanSkipXSIRead = false);
	void ReadXSIHeader(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed);
	bool Parse();

	CSequence* GetNext();
	void SetNext(CSequence* next);

	bool DoProperties();

	LPCTSTR GetPath();
	void SetPath(LPCTSTR path);

	bool ValidEnum();
	void SetValidEnum(bool value = true);

	int GetStartFrame();
	void SetStartFrame(int frame);
	int GetTargetFrame();
	void SetTargetFrame(int frame);
	int GetFrameCount();
	void SetFrameCount(int count);
	int GetFrameSpeed();
	void SetFrameSpeed(int speed);
	int GetFrameSpeedFromHeader();
	void SetFrameSpeedFromHeader(int speed);
	int GetLoopFrame();
	void SetLoopFrame(int loop);
	bool GetGenLoopFrame(void);
	void SetGenLoopFrame(bool bGenLoopFrame);

	void AddComment(CComment* comment);
	CComment* GetFirstComment();

	int GetDisplayIconForTree(CModel* pModel);
	LPCSTR GetDisplayNameForTree(CModel* pModel, bool bIncludeAnimEnum, bool bIncludeFrameDetails, bool bViewFrameDetails_Additional, CDC* pDC);
	LPCTSTR GetName();
	LPCTSTR GetNameExt();
	void SetName(LPCTSTR name);
	void SetNameExt(LPCTSTR name);
	void DeriveName();
	void DeriveNameExt();

	int GetFill();
	void SetFill(int value);

	ENUMTYPE GetEnumTypeFromString(LPCSTR lpString);
	ENUMTYPE GetEnumType();
	LPCTSTR GetEnum();
	void SetEnum(LPCTSTR name);

	LPCTSTR GetSound();
	void SetSound(LPCTSTR name);

	LPCTSTR GetAction();
	void SetAction(LPCTSTR name);

	bool IsGLA();

	int						m_iSequenceNumber;	// temp-use during tree-sort/model-resequencing

	// this isn't a brilliant wy of doing things, but enables retro-fitting of extra data fairly painlessly...
	CSequence*	AdditionalSeqs[MAX_ADDITIONAL_SEQUENCES];
	int GetValidAdditionalSequences();
	bool AdditionalSequenceIsValid();

protected:
	void _Init(bool bGenLoopFrame, bool bIsGLA, LPCTSTR path, int startFrame, int targetFrame, int frameCount, int frameSpeed, int frameSpeedFromheader, CComment* comments);

	CSequence*				m_next;
	CComment*				m_comments;
	char*					m_path;
	char*					m_name;
	char*					m_nameExt;
	CString					m_enum;
	char*					m_sound;
	char*					m_action;
	int						m_fill;
	int						m_startFrame;
	int						m_targetFrame;
	int						m_frameCount;
	int						m_frameSpeed;
	int						m_iFrameSpeedFromHeader;
	int						m_loopFrame;
	bool					m_bIsGLA;
	bool					m_bGenLoopFrame;

	bool					m_validEnum;

//	static keywordArray_t	s_Symbols[];
//	static keywordArray_t	s_Keywords[];
};



/////////////////////////////////////////////////////////////////////////////
// CSequencePropPage dialog

class CSequencePropPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CSequencePropPage)

// Construction
public:
	CSequencePropPage();
	~CSequencePropPage();

	CSequence*				m_sequence;
	bool*					m_soilFlag;

// Dialog Data
	//{{AFX_DATA(CSequencePropPage)
	enum { IDD = IDD_PP_SEQUENCE };
	CString	m_path;
	//
	CString	m_AnimationEnum;
	CString	m_AnimationEnum2;
	CString	m_AnimationEnum3;
	CString	m_AnimationEnum4;
	CString	m_AnimationEnum5;
	CString	m_AnimationEnum6;
	CString	m_AnimationEnum7;
	CString	m_AnimationEnum8;
	CString	m_AnimationEnum9;
	CString	m_AnimationEnum10;
	CString	m_AnimationEnum11;
	CString	m_AnimationEnum12;
	CString	m_AnimationEnum13;
	CString	m_AnimationEnum14;
	CString	m_AnimationEnum15;
	CString	m_AnimationEnum16;
	//
	int		m_frameCount;
	int		m_frameCount2;
	int		m_frameCount3;
	int		m_frameCount4;
	int		m_frameCount5;
	int		m_frameCount6;
	int		m_frameCount7;
	int		m_frameCount8;
	int		m_frameCount9;
	int		m_frameCount10;
	int		m_frameCount11;
	int		m_frameCount12;
	int		m_frameCount13;
	int		m_frameCount14;
	int		m_frameCount15;
	int		m_frameCount16;
	//
	int		m_frameSpeed;
	int		m_frameSpeed2;
	int		m_frameSpeed3;
	int		m_frameSpeed4;
	int		m_frameSpeed5;
	int		m_frameSpeed6;
	int		m_frameSpeed7;
	int		m_frameSpeed8;
	int		m_frameSpeed9;
	int		m_frameSpeed10;
	int		m_frameSpeed11;
	int		m_frameSpeed12;
	int		m_frameSpeed13;
	int		m_frameSpeed14;
	int		m_frameSpeed15;
	int		m_frameSpeed16;
	//
	int		m_iLoopFrame;
	int		m_iLoopFrame2;
	int		m_iLoopFrame3;
	int		m_iLoopFrame4;
	int		m_iLoopFrame5;
	int		m_iLoopFrame6;
	int		m_iLoopFrame7;
	int		m_iLoopFrame8;
	int		m_iLoopFrame9;
	int		m_iLoopFrame10;
	int		m_iLoopFrame11;
	int		m_iLoopFrame12;
	int		m_iLoopFrame13;
	int		m_iLoopFrame14;
	int		m_iLoopFrame15;
	int		m_iLoopFrame16;
	//
	int		m_startFrame;
	int		m_startFrame2;
	int		m_startFrame3;
	int		m_startFrame4;
	int		m_startFrame5;
	int		m_startFrame6;
	int		m_startFrame7;
	int		m_startFrame8;
	int		m_startFrame9;
	int		m_startFrame10;
	int		m_startFrame11;
	int		m_startFrame12;
	int		m_startFrame13;
	int		m_startFrame14;
	int		m_startFrame15;
	int		m_startFrame16;
	//
	BOOL	m_bGenLoopFrame;
	//}}AFX_DATA


	void OkOrApply();
// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSequencePropPage)
	public:
	virtual void OnOK();
	virtual BOOL OnApply();
	virtual void OnCancel();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void HandleAdditionalEditBoxesGraying();
	void HandleAllItemsGraying();
	// Generated message map functions
	//{{AFX_MSG(CSequencePropPage)
	virtual BOOL OnInitDialog();
	//
	afx_msg void OnButtonChooseanimationenum();
	afx_msg void OnButtonChooseanimationenum2();
	afx_msg void OnButtonChooseanimationenum3();
	afx_msg void OnButtonChooseanimationenum4();
	afx_msg void OnButtonChooseanimationenum5();
	afx_msg void OnButtonChooseanimationenum6();
	afx_msg void OnButtonChooseanimationenum7();
	afx_msg void OnButtonChooseanimationenum8();
	afx_msg void OnButtonChooseanimationenum9();
	afx_msg void OnButtonChooseanimationenum10();
	afx_msg void OnButtonChooseanimationenum11();
	afx_msg void OnButtonChooseanimationenum12();
	afx_msg void OnButtonChooseanimationenum13();
	afx_msg void OnButtonChooseanimationenum14();
	afx_msg void OnButtonChooseanimationenum15();
	afx_msg void OnButtonChooseanimationenum16();
	//
	afx_msg void OnButtonClearanimationenum();
	afx_msg void OnButtonClearanimationenum2();
	afx_msg void OnButtonClearanimationenum3();
	afx_msg void OnButtonClearanimationenum4();
	afx_msg void OnButtonClearanimationenum5();
	afx_msg void OnButtonClearanimationenum6();
	afx_msg void OnButtonClearanimationenum7();
	afx_msg void OnButtonClearanimationenum8();
	afx_msg void OnButtonClearanimationenum9();
	afx_msg void OnButtonClearanimationenum10();
	afx_msg void OnButtonClearanimationenum11();
	afx_msg void OnButtonClearanimationenum12();
	afx_msg void OnButtonClearanimationenum13();
	afx_msg void OnButtonClearanimationenum14();
	afx_msg void OnButtonClearanimationenum15();
	afx_msg void OnButtonClearanimationenum16();
	//
	afx_msg void OnKillfocusStartframe();
	afx_msg void OnKillfocusStartframe2();
	afx_msg void OnKillfocusStartframe3();
	afx_msg void OnKillfocusStartframe4();
	afx_msg void OnKillfocusStartframe5();
	afx_msg void OnKillfocusStartframe6();
	afx_msg void OnKillfocusStartframe7();
	afx_msg void OnKillfocusStartframe8();
	afx_msg void OnKillfocusStartframe9();
	afx_msg void OnKillfocusStartframe10();
	afx_msg void OnKillfocusStartframe11();
	afx_msg void OnKillfocusStartframe12();
	afx_msg void OnKillfocusStartframe13();
	afx_msg void OnKillfocusStartframe14();
	afx_msg void OnKillfocusStartframe15();
	afx_msg void OnKillfocusStartframe16();
	//
	afx_msg void OnKillfocusLoopframe();
	afx_msg void OnKillfocusLoopframe2();
	afx_msg void OnKillfocusLoopframe3();
	afx_msg void OnKillfocusLoopframe4();
	afx_msg void OnKillfocusLoopframe5();
	afx_msg void OnKillfocusLoopframe6();
	afx_msg void OnKillfocusLoopframe7();
	afx_msg void OnKillfocusLoopframe8();
	afx_msg void OnKillfocusLoopframe9();
	afx_msg void OnKillfocusLoopframe10();
	afx_msg void OnKillfocusLoopframe11();
	afx_msg void OnKillfocusLoopframe12();
	afx_msg void OnKillfocusLoopframe13();
	afx_msg void OnKillfocusLoopframe14();
	afx_msg void OnKillfocusLoopframe15();
	afx_msg void OnKillfocusLoopframe16();
	//
	afx_msg void OnKillfocusFramespeed();
	afx_msg void OnKillfocusFramespeed2();
	afx_msg void OnKillfocusFramespeed3();
	afx_msg void OnKillfocusFramespeed4();
	afx_msg void OnKillfocusFramespeed5();
	afx_msg void OnKillfocusFramespeed6();
	afx_msg void OnKillfocusFramespeed7();
	afx_msg void OnKillfocusFramespeed8();
	afx_msg void OnKillfocusFramespeed9();
	afx_msg void OnKillfocusFramespeed10();
	afx_msg void OnKillfocusFramespeed11();
	afx_msg void OnKillfocusFramespeed12();
	afx_msg void OnKillfocusFramespeed13();
	afx_msg void OnKillfocusFramespeed14();
	afx_msg void OnKillfocusFramespeed15();
	afx_msg void OnKillfocusFramespeed16();
	//
	afx_msg void OnKillfocusFramecount();
	afx_msg void OnKillfocusFramecount2();
	afx_msg void OnKillfocusFramecount3();
	afx_msg void OnKillfocusFramecount4();
	afx_msg void OnKillfocusFramecount5();
	afx_msg void OnKillfocusFramecount6();
	afx_msg void OnKillfocusFramecount7();
	afx_msg void OnKillfocusFramecount8();
	afx_msg void OnKillfocusFramecount9();
	afx_msg void OnKillfocusFramecount10();
	afx_msg void OnKillfocusFramecount11();
	afx_msg void OnKillfocusFramecount12();
	afx_msg void OnKillfocusFramecount13();
	afx_msg void OnKillfocusFramecount14();
	afx_msg void OnKillfocusFramecount15();
	afx_msg void OnKillfocusFramecount16();
	//
	afx_msg void OnKillfocusEditAnimationenum();
	afx_msg void OnKillfocusEditAnimationenum2();
	afx_msg void OnKillfocusEditAnimationenum3();
	afx_msg void OnKillfocusEditAnimationenum4();
	afx_msg void OnKillfocusEditAnimationenum5();
	afx_msg void OnKillfocusEditAnimationenum6();
	afx_msg void OnKillfocusEditAnimationenum7();
	afx_msg void OnKillfocusEditAnimationenum8();
	afx_msg void OnKillfocusEditAnimationenum9();
	afx_msg void OnKillfocusEditAnimationenum10();
	afx_msg void OnKillfocusEditAnimationenum11();
	afx_msg void OnKillfocusEditAnimationenum12();
	afx_msg void OnKillfocusEditAnimationenum13();
	afx_msg void OnKillfocusEditAnimationenum14();
	afx_msg void OnKillfocusEditAnimationenum15();
	afx_msg void OnKillfocusEditAnimationenum16();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

ENUMTYPE GetEnumTypeFromString(LPCSTR lpString);
bool IsEnumSeperator(LPCSTR lpString);
LPCSTR StripSeperatorStart(LPCSTR lpString);
void ReadASEHeader(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed, bool bReadingGLA, bool bCanSkipXSIRead = false);

