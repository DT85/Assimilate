#include "Stdafx.h"
#include "Includes.h"
#include "gla.h"
#include "matrix4.h"
#include "xsiimp.h"


#define ONKILLFOCUS UpdateData(DIALOG_TO_DATA)

keywordArray_t CSequence_s_Symbols[] = 
{
	"*",		TK_ASTERISK,
	NULL,		TK_EOF,
};

keywordArray_t CSequence_s_Keywords[] = 
{
	"SCENE_FRAMESPEED",		TK_ASE_FRAMESPEED,
	"SCENE_FIRSTFRAME",		TK_ASE_FIRSTFRAME,
	"SCENE_LASTFRAME",		TK_ASE_LASTFRAME,
	NULL,					TK_EOF,
};

CSequence::CSequence()
{
}

CSequence::~CSequence()
{
}

void CSequence::Delete()
{
	while(m_comments != NULL)
	{
		CComment* curComment = m_comments;
		m_comments = curComment->GetNext();
		curComment->Delete();
	}
	if (m_path != NULL)
	{
		free(m_path);
		m_path = NULL;
	}
	if (m_name != NULL)
	{
		free(m_name);
		m_name = NULL;
	}
	if (m_nameExt != NULL)
	{
		free(m_nameExt);
		m_nameExt = NULL;
	}
	if (m_action != NULL)
	{
		free(m_action);
		m_action = NULL;
	}
	if (m_sound != NULL)
	{
		free(m_sound);
		m_sound = NULL;
	}

	for (int i=0; i<MAX_ADDITIONAL_SEQUENCES; i++)
	{
		if (AdditionalSeqs[i])
		{
			AdditionalSeqs[i]->Delete();
			AdditionalSeqs[i] = NULL;			
		}
	}

	delete this;
}

CSequence* CSequence::CreateFromFile(LPCTSTR path, CComment* comments)
{
	CSequence* retval = new CSequence();
	CString name = path;
	name.Replace('\\', '/');
	name.MakeLower();
	retval->_Init(false,!!(strstr(path,".gla")),name, 0, 0, 0, iDEFAULTSEQFRAMESPEED,iDEFAULTSEQFRAMESPEED,comments);	// (noticed) this fills in certain fields wrongly until the next 2 lines correct them
	retval->DeriveName();
	retval->ReadHeader();

	// at this point the m_path member is wrong because it contains the full path including "\quake\...", so...

	CString newPath = retval->GetPath();	
	Filename_RemoveBASEQ(newPath);
	retval->SetPath(newPath);	
	return retval;
}

void CSequence::ReadHeader()
{
	int iStartFrame, iFrameCount, iFrameSpeed;

	CString nameASE = ((CAssimilateApp*)AfxGetApp())->GetQuakeDir();
			nameASE+= GetPath();	

	ReadASEHeader( nameASE, iStartFrame, iFrameCount, iFrameSpeed);

	SetStartFrame( iStartFrame );
	SetFrameCount( iFrameCount );
	SetFrameSpeed( iFrameSpeed );	// iDEFAULTSEQFRAMESPEED
	SetFrameSpeedFromHeader( iFrameSpeed );	
}

void ReadXSIHeader(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed)
{
	OutputDebugString(va("ReadXSIHeader(): %s\n",psFilename));

	// let's try the fast way of reading first, by scanning directly for the header value if it's a 3.0 file...
	{
		bool bGotInfo = false;

		FILE *fp = fopen(psFilename, "r");

		if (fp)
		{
			fseek(fp,0,SEEK_END);
			int iLen = ftell(fp);

			if (iLen>1024)
				iLen=1024;	// findmeste

			char *psFileBin = new char[iLen+1];
			assert(psFileBin);
			psFileBin[iLen]='\0';
			fseek(fp,0,SEEK_SET);

			if (fread(psFileBin, 1, iLen, fp) == (size_t)iLen)
			{
				// I'm not sure how to tie this in with Gil's rather-complex XSI reader, so do this seperately...
				{
					char *psSearch = strstr(psFileBin,"SI_Scene");

					if (psSearch)
					{
						while (psSearch != (psFileBin+iLen) && *psSearch != '{')	// '}' don't bugger-up brace-matching
						{
							psSearch++;
						}
						if (*psSearch++=='{')	// '}' don't bugger-up brace-matching
						{
							char sScanBuffer[2048]={0};
				
							for (int i=0; i<sizeof(sScanBuffer)-1 && psSearch != psFileBin+iLen; i++, psSearch++)
							{
								if (isspace(*psSearch) || *psSearch=='"')
									continue;

								strcat(sScanBuffer,va("%c",*psSearch));
							}

							char	sTemp[2048];
							float	fOneBased;	// this number seems to be either 0 or 1, which affects the fFrames val.
							float	fFrames;
							float	fFPS;

							int iFieldsRead = sscanf(sScanBuffer,"%6s, %f, %f, %f",&sTemp,&fOneBased,&fFrames,&fFPS);

							if (iFieldsRead == 4 && !strcmp(sTemp,"FRAMES"))
							{
								// success...
								iStartFrame = 0;
								iFrameCount = ((int) fFrames - (int) fOneBased)+1;
								iFrameSpeed = (int) fFPS;

								bGotInfo = true;
							}
						}
					}
				}
			}
			delete [] psFileBin;
			fclose(fp);
			if (bGotInfo)
				return;
		}	
	}

	// if we get here, then this is probably an old 1.3 format file, so read the frames count etc the tedious way...

	// (Note that the XSI reader here is the original Assimilate one, not Gil's super-spiffy read-anything version, since it
	//	only has to deal with the old format files which this one copes with ok)...

	int iNumFrames = XSI_LoadFile(psFilename);

	iStartFrame = 0;
	iFrameCount = iNumFrames;
	iFrameSpeed = iDEFAULTSEQFRAMESPEED;

	XSI_Cleanup();
}

void CSequence::ReadXSIHeader(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed)
{
	::ReadXSIHeader(psFilename, iStartFrame, iFrameCount, iFrameSpeed);
}

bool gbSkipXSIRead_QuestionAsked = false;
bool gbSkipXSIRead = false;
typedef pair< int, int> FrameCountAndSpeed_t;
typedef map <CString, FrameCountAndSpeed_t> ASECachedInfo_t;
ASECachedInfo_t ASECachedInfo;

// this actually reads XSI or GLA headers...  historical mutation strikes again...

static void ReadASEHeader_Actual(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed, bool bReadingGLA, bool bCanSkipXSIRead /* = false */)
{
	// since the XSI loader is so damn slow and flakey I'm going to have to cache the info to avoid re-reading...
	// do we have it in the cache?...

	if (strstr(psFilename,".xsi") || strstr(psFilename,".XSI"))
	{
		ASECachedInfo_t::iterator iter = ASECachedInfo.find(psFilename);
		if (iter != ASECachedInfo.end())
		{
			iStartFrame = 0;
			iFrameCount = (*iter).second.first;
			iFrameSpeed = (*iter).second.second;
			return;
		}
	}

	// is it a GLA file?...

	if (bReadingGLA)
	{
		char sTemp[1024];
		strcpy(sTemp,psFilename);
		if (!(strstr(psFilename,".gla") || strstr(psFilename,".GLA")))
		{
			strcat(sTemp,".gla");
		}

		iStartFrame = 0;
		iFrameCount = GLA_ReadHeader(sTemp);
		iFrameSpeed = 20;	// any old value for GLA file
		return;
	}


	// it's not in the cache, but we may be able to avoid having to read it under some circumstances...

	bool bXSIShouldBeRead = true;

	if (gbCarWash_DoingScan)
	{
		bCanSkipXSIRead = false;	// stop it asking the question
		bXSIShouldBeRead= gbCarWash_YesToXSIScan;
	}

	if ( (strstr(psFilename,".xsi") || strstr(psFilename,".XSI"))
		&& bCanSkipXSIRead
		)
	{
		if (!gbSkipXSIRead && !gbSkipXSIRead_QuestionAsked)
		{
			gbSkipXSIRead_QuestionAsked = true;
			gbSkipXSIRead = !GetYesNo(va("Model file: \"%s\"\n\n... is an XSI, and they can be damn slow to read in\n\nDo you want to scan all the XSIs?",psFilename));
		}
					
		bXSIShouldBeRead = !gbSkipXSIRead;
	}

	if (strstr(psFilename,".xsi") || strstr(psFilename,".XSI"))
	{
		if (bXSIShouldBeRead)
		{
			ReadXSIHeader(psFilename, iStartFrame, iFrameCount, iFrameSpeed);
	
			if (iFrameCount!=0)
			{
				// cache it for future...

				ASECachedInfo[psFilename] = FrameCountAndSpeed_t(iFrameCount,iFrameSpeed);
			}
		}
		return;
	}

	// it must be an ASE file then instead....

	CTokenizer* tokenizer = CTokenizer::Create();
	tokenizer->AddParseFile(psFilename, ((CAssimilateApp*)AfxGetApp())->GetBufferSize());
	tokenizer->SetSymbols(CSequence_s_Symbols);
	tokenizer->SetKeywords(CSequence_s_Keywords);

	CToken* curToken = tokenizer->GetToken();
	while(curToken != NULL)
	{
		switch (curToken->GetType())
		{
		case TK_EOF:
			curToken->Delete();
			curToken = NULL;
			break;
		case TK_ASTERISK:
			curToken->Delete();
			curToken = tokenizer->GetToken();
			switch(curToken->GetType())
			{
			case TK_ASE_FIRSTFRAME:
				curToken->Delete();
				curToken = tokenizer->GetToken();
				if (curToken->GetType() == TK_INTEGER)
				{
					iStartFrame = curToken->GetIntValue();
					curToken->Delete();
					curToken = tokenizer->GetToken();
				}
				break;
			case TK_ASE_LASTFRAME:
				curToken->Delete();
				curToken = tokenizer->GetToken();
				if (curToken->GetType() == TK_INTEGER)
				{
					iFrameCount = curToken->GetIntValue() + 1;
					curToken->Delete();
					curToken = NULL;	// tells outer loop to finish
				}
				break;
			case TK_ASE_FRAMESPEED:
				curToken->Delete();
				curToken = tokenizer->GetToken();
				if (curToken->GetType() == TK_INTEGER)
				{
					iFrameSpeed = curToken->GetIntValue();
					curToken->Delete();
					curToken = tokenizer->GetToken();
				}
				break;
			case TK_EOF:
				curToken->Delete();
				curToken = NULL;
				break;
			default:
				curToken->Delete();
				curToken = tokenizer->GetToken();
				break;
			}
			break;
		default:
			curToken->Delete();
			curToken = tokenizer->GetToken();
			break;
		}
	}
	tokenizer->Delete();

	iFrameCount -= iStartFrame;	
	iStartFrame  = 0;
}

void ReadASEHeader(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed, bool bReadingGLA, bool bCanSkipXSIRead /* = false */)
{
	StatusText(va("Reading Header: \"%s\"",psFilename));
	ReadASEHeader_Actual(psFilename, iStartFrame, iFrameCount, iFrameSpeed, bReadingGLA, bCanSkipXSIRead);
	StatusText(NULL);
}

// this does NOT update any member vars, so can be called from outside this class, it just reads member vars to
//	find out what tables to pass to tokenizer...

void CSequence::ReadASEHeader(LPCSTR psFilename, int &iStartFrame, int &iFrameCount, int &iFrameSpeed, bool bCanSkipXSIRead /* = false */)
{
	::ReadASEHeader(psFilename, iStartFrame, iFrameCount, iFrameSpeed, m_bIsGLA, bCanSkipXSIRead);
}

bool CSequence::Parse()
{
	CASEFile* aseFile = CASEFile::Create(m_path);
	aseFile->Parse();
	aseFile->Delete();
	return true;
}

CSequence* CSequence::_Create(bool bGenLoopFrame, bool bIsGLA, LPCTSTR path, int startFrame, int targetFrame, int frameCount, int frameSpeed, int frameSpeedFromHeader, CComment* comments)
{
	CSequence* retval = new CSequence();
	retval->_Init(bGenLoopFrame, bIsGLA, path, startFrame, targetFrame, frameCount, frameSpeed, frameSpeedFromHeader, comments);
	return retval;
}

bool CSequence::DoProperties()
{
	bool dirty = false;

	if (IsGLA())
	{
		InfoBox("You can't edit the properties of a GLA file");
	}
	else
	{
		CPropertySheet* propSheet = new CPropertySheet(m_name);

 		CSequencePropPage* propPage = new CSequencePropPage();
		propPage->m_sequence = this;
		propPage->m_soilFlag = &dirty;
		propSheet->AddPage(propPage);

		propSheet->DoModal();

		delete propPage;
		delete propSheet;
	}

	return dirty;
}

CSequence* CSequence::GetNext()
{
	return m_next;
}

void CSequence::SetNext(CSequence* next)
{
	m_next = next;
}

bool CSequence::ValidEnum()
{
	return m_validEnum;
}

void CSequence::SetValidEnum(bool value)
{
	m_validEnum = value;
}

LPCTSTR CSequence::GetPath()
{
	return m_path;
}

void CSequence::SetPath(LPCTSTR path)
{
	if (m_path != NULL)
	{
		free(m_path);
	}
	if (path == NULL)
	{
		m_path = NULL;
	}
	else
	{
		m_path = (char*)malloc(strlen(path) + 1);
		strcpy(m_path, path);
	}
}

int CSequence::GetStartFrame()
{
	return m_startFrame;
}

void CSequence::SetStartFrame(int frame)
{
	m_startFrame = frame;
}

int CSequence::GetTargetFrame()
{
	return m_targetFrame;
}

void CSequence::SetTargetFrame(int frame)
{
	m_targetFrame = frame;
}

int CSequence::GetFrameCount()
{
	return m_frameCount;
}

void CSequence::SetFrameCount(int count)
{
	m_frameCount = count;
}

int CSequence::GetFrameSpeedFromHeader()
{
	return m_iFrameSpeedFromHeader;
}

void CSequence::SetFrameSpeedFromHeader(int speed)
{
	m_iFrameSpeedFromHeader = speed;
}

int CSequence::GetFrameSpeed()
{
	return m_frameSpeed;
}

void CSequence::SetFrameSpeed(int speed)
{
	m_frameSpeed = speed;
}

int CSequence::GetLoopFrame()
{
	return m_loopFrame;
}

void CSequence::SetLoopFrame(int loop)
{
	m_loopFrame = loop;
}

bool CSequence::GetGenLoopFrame(void)
{
	return m_bGenLoopFrame;
}

void CSequence::SetGenLoopFrame(bool bGenLoopFrame)
{
	m_bGenLoopFrame = bGenLoopFrame;
}


int CSequence::GetFill()
{
	return m_fill;
}

void CSequence::SetFill(int value)
{
	m_fill = value;
}

// this is now recursive, so watch what you're doing...

void CSequence::_Init(bool bGenLoopFrame, bool bIsGLA, LPCTSTR path, int startFrame, int targetFrame, int frameCount, int frameSpeed, int frameSpeedFromHeader, CComment* comments)
{
	static bool bHere = false;	

	m_path = NULL;
	m_next = NULL;
	m_name = NULL;
	m_nameExt = NULL;
	m_fill = -1;	
	// another Jake thing, not sure if this'll stay in when valid StarWars enums are available
#if 0
	m_enum = "";
#else
	m_enum = path;
	if (!m_enum.IsEmpty())
	{
		Filename_BaseOnly(m_enum);
		m_enum.MakeUpper();
	}
#endif
	m_sound = NULL;
	m_action = NULL;
	m_comments = comments;	
	m_validEnum = false;
	m_loopFrame = -1;
	m_bIsGLA = bIsGLA;
	m_bGenLoopFrame = bGenLoopFrame;

	SetPath(path);
	SetStartFrame(startFrame);
	SetTargetFrame(targetFrame);
	SetFrameCount(frameCount);
	SetFrameSpeed(frameSpeed);	
	SetFrameSpeedFromHeader(frameSpeedFromHeader);

	if (bHere)
	{
		for (int i=0; i<MAX_ADDITIONAL_SEQUENCES; i++)
		{
			AdditionalSeqs[i] = NULL;
		}
	}
	else
	{
		bHere = true;

		for (int i=0; i<MAX_ADDITIONAL_SEQUENCES; i++)
		{
			AdditionalSeqs[i] = new CSequence();
			AdditionalSeqs[i]->_Init(false, false,NULL, 0, 0, 0, iDEFAULTSEQFRAMESPEED, iDEFAULTSEQFRAMESPEED, NULL);
		}

		bHere = false;
	}
}

void CSequence::SetName(LPCTSTR name)
{
	if (m_name != NULL)
	{
		free(m_name);
	}
	if ((name == NULL) || (strlen(name) < 1))
	{
		m_name = NULL;
	}
	else
	{
		m_name = (char*)malloc(strlen(name) + 1);
		strcpy(m_name, name);
	}
}

void CSequence::SetNameExt(LPCTSTR name)
{
	if (m_nameExt != NULL)
	{
		free(m_nameExt);
	}
	if ((name == NULL) || (strlen(name) < 1))
	{
		m_nameExt = NULL;
	}
	else
	{
		m_nameExt = (char*)malloc(strlen(name) + 1);
		strcpy(m_nameExt, name);
	}
}

ENUMTYPE GetEnumTypeFromString(LPCSTR lpString)
{
	if (lpString)
	{
		if (!_strnicmp(lpString, "FACE_", 5))
		{
			return ET_FACE;
		}

		if (!_strnicmp(lpString,"BOTH_", 5))
		{
			return ET_BOTH;
		}
			
		if (!_strnicmp(lpString,"TORSO_", 6))
		{
			return ET_TORSO;
		}

		if (!_strnicmp(lpString,"LEGS_", 5))
		{
			return ET_LEGS;
		}

		if (!_strnicmp(lpString, "VM_", 3))
		{
			return ET_VM;
		}
	}

	return ET_INVALID;
}

// some enums are actually only seperators, the way to check is if the char straight after the 1st '_' is valid...

bool IsEnumSeperator(LPCSTR lpString)
{
	while (*lpString && *lpString!='_') lpString++;

	if (*lpString++)
	{
		if (*lpString)
		{
			if (!(isalpha(*lpString) || isdigit(*lpString)))
				return true;
		}
	}

	return false;
}

// converts (eg) "BOTH_   THE DEATH ANIMS"  to "THE DEATH ANIMS"...

LPCSTR StripSeperatorStart(LPCSTR lpString)
{
	if (!IsEnumSeperator(lpString))
		return lpString;

	while (*lpString && *lpString != '_')	lpString++;
	if (*lpString)
	{
		lpString++;
		while (*lpString && isspace(*lpString))	lpString++;
	}

	return lpString;
}



// this is so that part of it can be called for a dialog box string that's not actually in the m_enum var...

ENUMTYPE CSequence::GetEnumTypeFromString(LPCSTR lpString)
{
	if (!lpString)
	{
		return ET_INVALID;
	}

	if (!strlen(lpString))
	{
		return ET_INVALID;	// special bit so blank strings don't assert in this one enumtype-return function
	}

	ENUMTYPE et = ::GetEnumTypeFromString(lpString);

	// it has to be valid here, so check we haven't forgotten some code...

	ASSERT(et!=ET_INVALID);

	return et;
}

ENUMTYPE CSequence::GetEnumType()
{
	// this check done first to cope with sequences named (eg) "TORSO_" but not having an entry in anims.h
	//	to match this (which a strcmp won't detect). This shouldn't really be necessary since the menu system only
	//	allows you to pick valid ones from anims.h, but it copes with people editing ascii files by hand (or alerts
	//	people to entries being subsequently deleted from anims.h after use)

	if (ValidEnum())
	{	
		return GetEnumTypeFromString(GetEnum());
	}
	
	return ET_INVALID;
}

int CSequence::GetDisplayIconForTree(CModel *pModel)
{
	if (pModel->IsGhoul2())
	{
		if (IsGLA() && !(GetEnumType() == ET_INVALID))
		{
			return ObjID_ENUMG2GLA;
		}
		else if (IsGLA() && (GetEnumType() == ET_INVALID))
		{
			return ObjID_ENUMG2GLAINVALID;
		}
	}

	switch (GetEnumType())
	{
		case ET_FACE:		
			return ObjID_ENUMFACE;
		case ET_BOTH:		
			return ObjID_ENUMBOTH;
		case ET_TORSO:		
			return ObjID_ENUMTORSO;
		case ET_LEGS:		
			return ObjID_ENUMLEGS;
		case ET_VM:			
			return ObjID_ENUMVM;
		case ET_INVALID:
			return ObjID_ENUMINVALID;
	}

	ASSERT(0);
	return ObjID_ENUMINVALID;
}


// not the fastest of routines, but I only call it occasionally...

LPCSTR CSequence::GetDisplayNameForTree(CModel* pModel, bool bIncludeAnimEnum, bool bIncludeFrameDetails, bool bViewFrameDetails_Additional, CDC* pDC)
{
	if (pModel->IsGhoul2())
	{
	}


#define ACCOUNTFORWIDTH(width)								\
			iWidthSoFar+=width;								\
			while (1)										\
			{												\
				size = pDC->GetOutputTextExtent(string);	\
				if (size.cx>=iWidthSoFar)					\
					break;									\
				string+=" ";								\
			}

	static CString string;	
	int iWidthSoFar=0;
	CSize size;

	string.Empty();

	if (bIncludeFrameDetails)
	{
		CString temp;
		temp.Format("( Target: %4d, Count: %4d%sLoop: %4d, Speed: %4d )",GetTargetFrame(),GetFrameCount(),GetGenLoopFrame()?" + 1, ":",   ",GetLoopFrame(),GetFrameSpeed());

		string += temp;

		ACCOUNTFORWIDTH(400);
	}
	
	string += GetNameExt();
	ACCOUNTFORWIDTH(300);	
	
	if (bIncludeAnimEnum && strlen(GetEnum()))
	{
		CString _enum;

		if (ValidEnum())
		{
			_enum.Format("( %s",GetEnum());				
		}
		else
		{
			_enum.Format("( (BAD: %s)",GetEnum());				
		}
		string += _enum;

		for (int i=0; i<MAX_ADDITIONAL_SEQUENCES; i++)
		{			
			if (AdditionalSeqs[i]->AdditionalSequenceIsValid())
			{
				string += ",   +";	// spaces seem pretty small in default font
				string += AdditionalSeqs[i]->GetEnum();

				if (bViewFrameDetails_Additional)
				{
					CString temp;
					temp.Format("(T:%d C:%d L:%d S:%d)",
									AdditionalSeqs[i]->GetTargetFrame(),
										AdditionalSeqs[i]->GetFrameCount(),
											AdditionalSeqs[i]->GetLoopFrame(),
													AdditionalSeqs[i]->GetFrameSpeed()
								);

					string += temp;
				}
			}
			else
			{
				// either empty string, or a bad enum... (because of anims.h being edited)

				if (strlen(AdditionalSeqs[i]->GetEnum()))
				{
					string += ",   ";
					string += va("(BAD: %s)",AdditionalSeqs[i]->GetEnum());
				}
			}
		}

		string += " )";

		ACCOUNTFORWIDTH(200);	// this is flawed now because of additional-seqs, so keep this string as the last one
	}

	return string;
}

LPCTSTR CSequence::GetName()
{
	return m_name;
}

LPCTSTR CSequence::GetNameExt()
{
	return m_nameExt;
}

void CSequence::DeriveName()
{
	if (m_path == NULL)
	{
		if (m_name != NULL)
		{
			free(m_name);
			m_name = NULL;
		}
		return;
	}
	CString name = m_path;
	int loc = name.ReverseFind('.');
	if (loc > -1)
	{
		name = name.Left(loc);
	}
	loc = name.ReverseFind('/');
	if (loc > -1)
	{
		name = name.Right(name.GetLength() - loc - 1);
	}
	name.MakeUpper();
	SetName(name);
}

void CSequence::DeriveNameExt()
{
	if (m_path == NULL)
	{
		if (m_nameExt != NULL)
		{
			free(m_nameExt);
			m_nameExt = NULL;
		}
		return;
	}
	CString name = m_path;
	int loc = name.ReverseFind('.');
	loc = name.ReverseFind('/');
	if (loc > -1)
	{
		name = name.Right(name.GetLength() - loc - 1);
	}
	name.MakeLower();
	SetNameExt(name);
}

void CSequence::SetSound(LPCTSTR name)
{
	if (m_sound != NULL)
	{
		free(m_sound);
	}
	if ((name == NULL) || (strlen(name) < 1))
	{
		m_sound = NULL;
	}
	else
	{
		m_sound = (char*)malloc(strlen(name) + 1);
		strcpy(m_sound, name);
	}
}

LPCTSTR CSequence::GetSound()
{
	return m_sound;
}

void CSequence::SetAction(LPCTSTR name)
{
	if (m_action != NULL)
	{
		free(m_action);
	}
	if ((name == NULL) || (strlen(name) < 1))
	{
		m_action = NULL;
	}
	else
	{
		m_action = (char*)malloc(strlen(name) + 1);
		strcpy(m_action, name);
	}
}

bool CSequence::IsGLA()
{
	return m_bIsGLA;
}

LPCTSTR CSequence::GetAction()
{
	return m_action;
}

void CSequence::SetEnum(LPCTSTR name)
{
	m_enum = name;

	SetValidEnum(((CAssimilateApp*)AfxGetApp())->ValidEnum(m_enum));
}

LPCTSTR CSequence::GetEnum()
{
	return m_enum;
}

void CSequence::AddComment(CComment* comment)
{
	if (m_comments == NULL)
	{
		m_comments = comment;
	}
	else
	{
		CComment* curComment = m_comments;
		while (curComment->GetNext() != NULL)
		{
			curComment = curComment->GetNext();
		}
		curComment->SetNext(comment);
	}
}

// this should only be called on sequence that you know are Additional ones...

bool CSequence::AdditionalSequenceIsValid()
{
	return (GetEnumType()!=ET_INVALID);
}

// should only really be called on master sequences, though harmless otherwise...

int CSequence::GetValidAdditionalSequences()
{
	int iCount = 0;

	for (int i=0; i<MAX_ADDITIONAL_SEQUENCES; i++)
	{
		if (AdditionalSeqs[i])
		{
			if (AdditionalSeqs[i]->AdditionalSequenceIsValid())
			{
				iCount++;
			}
		}
	}

	return iCount;
}

CComment* CSequence::GetFirstComment()
{
	return m_comments;
}

void CSequence::Write(CTxtFile* file, bool bPreQuat)
{
	CComment* curComment = m_comments;
	while(curComment != NULL)
	{
		curComment->Write(file);
		curComment = curComment->GetNext();
	}

	file->Write("$", CAssimilateDoc::GetKeyword(IsGLA()?TK_AS_GRAB_GLA:TK_AS_GRAB, TABLE_QDT), " ");
	CString path = m_path;
	int loc = path.Find("/base");
	if (loc > -1)
	{
		path = path.Right(path.GetLength() - loc - 5);
		loc = path.Find("/");
		path = path.Right(path.GetLength() - loc - 1);
	}
	if (!path.GetLength())	// check that some dopey artist hasn't use the name "base" on the right hand side
	{
		path = m_path;
	}

	Filename_AccountForLOD(path, giLODLevelOverride);
	file->Write(path);	

	if (!IsGLA())
	{
		if (m_fill > -1)
		{
			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_FILL, TABLE_GRAB), " ");
			file->Write(m_fill);
		}
		if (m_loopFrame != 0)
		{
			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_LOOP, TABLE_GRAB), " ");
			file->Write(m_loopFrame);
		}
		if (m_enum.GetLength() && (strcmp(m_name, m_enum) != 0))
		{
			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_ENUM, TABLE_GRAB), " ");
			file->Write(m_enum);
		}
		if (m_sound != NULL)
		{
			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_SOUND, TABLE_GRAB), " ");
			file->Write(m_sound);
		}
		if (m_action != NULL)
		{
			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_ACTION, TABLE_GRAB), " ");
			file->Write(m_action);
		}	

		if (m_bGenLoopFrame)
		{
			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_GENLOOPFRAME, TABLE_GRAB), " ");
		}

		if (m_frameSpeed != m_iFrameSpeedFromHeader ||
			GetValidAdditionalSequences() ||
			bPreQuat
			)
		{
			// write out start marker (and stop-marker at bottom) so qdata can skip this more easily...

			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_QDSKIPSTART, TABLE_GRAB));
			
			if (m_frameSpeed != m_iFrameSpeedFromHeader)
			{
				file->Space();
				file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_FRAMESPEED, TABLE_GRAB), " ");
				file->Write(m_frameSpeed);
			}
			
			for (int i=0; i<MAX_ADDITIONAL_SEQUENCES; i++)
			{
				CSequence* additionalSeq = AdditionalSeqs[i];
				
				if (additionalSeq->AdditionalSequenceIsValid())
				{
					file->Space();
					file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_ADDITIONAL, TABLE_GRAB), " ");
					file->Write(additionalSeq->GetStartFrame());
					file->Space();
					file->Write(additionalSeq->GetFrameCount());
					file->Space();
					file->Write(additionalSeq->GetLoopFrame());
					file->Space();
					file->Write(additionalSeq->GetFrameSpeed());
					file->Space();
					file->Write(additionalSeq->GetEnum());
				}
			}

			// this is an unpleasant abuse, but needed to retro-hack into old versions of carcass...

			if (bPreQuat)
			{
				file->Space();
				file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_PREQUAT, TABLE_GRAB));
			}

			// any other stuff in future (sound events etc)...
			// ...

			// write end marker (so QData knows to start reading again)...

			file->Space();
			file->Write("-", CAssimilateDoc::GetKeyword(TK_AS_QDSKIPSTOP, TABLE_GRAB));
		}
	}

	file->Writeln();
}

// this writes the CFG file... (it's also now recursive, albeit with changing 'this' ptrs, so be aware of that)

void CSequence::WriteExternal(CModel *pModel, CTxtFile* file)
{
	// this now keeps the spacing nicer for cutting and pasting into source...

	CString sOutputEnum = m_enum;
	if (sOutputEnum.IsEmpty())
	{
		sOutputEnum = "???";
	}

	while(sOutputEnum.GetLength()<20) sOutputEnum+=" ";

	file->Write(sOutputEnum, "\t");	

	// special code, if this is a LEGS type enum then we need to adjust the target frame to 'lose' the chunk
	//	of frames occupied by the TORSO types...

	int iTargetFrame = m_targetFrame;
	file->Write(iTargetFrame);
	file->Write("\t");
	file->Write(m_frameCount);
	file->Write("\t");
	file->Write(m_loopFrame);
	file->Write("\t");
	file->Write(m_frameSpeed);

	if (!m_validEnum && !pModel->IsGhoul2())
	{
		file->Write("\t// fix me - invalid enum -");
	}

	if (m_sound != NULL)
	{
		file->Write("\t// sound - ");
	}

	if (m_action != NULL)
	{
		file->Write ("\t// action - ");
	}

	if (m_fill > -1)
	{
		file->Write("\t// fill = ");
		file->Write(m_fill);
	}

	file->Writeln();

	// now for a bit of recursion, write out any additional sequences that this sequence owns...
	for (int i=0; i<MAX_ADDITIONAL_SEQUENCES; i++)
	{
		CSequence *seq = AdditionalSeqs[i];

		if (seq)
		{
			if (seq->AdditionalSequenceIsValid())
			{
				seq->WriteExternal(pModel, file);
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
// CSequencePropPage property page

IMPLEMENT_DYNCREATE(CSequencePropPage, CPropertyPage)

CSequencePropPage::CSequencePropPage() : CPropertyPage(CSequencePropPage::IDD)
{
	//{{AFX_DATA_INIT(CSequencePropPage)
	m_path = _T("");
	//
	m_AnimationEnum = _T("");
	m_AnimationEnum2 = _T("");
	m_AnimationEnum3 = _T("");
	m_AnimationEnum4 = _T("");
	m_AnimationEnum5 = _T("");
	m_AnimationEnum6 = _T("");
	m_AnimationEnum7 = _T("");
	m_AnimationEnum8 = _T("");
	m_AnimationEnum9 = _T("");
	m_AnimationEnum10 = _T("");
	m_AnimationEnum11 = _T("");
	m_AnimationEnum12 = _T("");
	m_AnimationEnum13 = _T("");
	m_AnimationEnum14 = _T("");
	m_AnimationEnum15 = _T("");
	m_AnimationEnum16 = _T("");
	//
	m_frameCount = 0;
	m_frameCount2 = 0;
	m_frameCount3 = 0;
	m_frameCount4 = 0;
	m_frameCount5 = 0;
	m_frameCount6 = 0;
	m_frameCount7 = 0;
	m_frameCount8 = 0;
	m_frameCount9 = 0;
	m_frameCount10 = 0;
	m_frameCount11 = 0;
	m_frameCount12 = 0;
	m_frameCount13 = 0;
	m_frameCount14 = 0;
	m_frameCount15 = 0;
	m_frameCount16 = 0;
	//
	m_frameSpeed = 0;
	m_frameSpeed2 = 0;
	m_frameSpeed3 = 0;
	m_frameSpeed4 = 0;
	m_frameSpeed5 = 0;
	m_frameSpeed6 = 0;
	m_frameSpeed7 = 0;
	m_frameSpeed8 = 0;
	m_frameSpeed9 = 0;
	m_frameSpeed10 = 0;
	m_frameSpeed11 = 0;
	m_frameSpeed12 = 0;
	m_frameSpeed13 = 0;
	m_frameSpeed14 = 0;
	m_frameSpeed15 = 0;
	m_frameSpeed16 = 0;
	//
	m_iLoopFrame = 0;
	m_iLoopFrame2 = 0;
	m_iLoopFrame3 = 0;
	m_iLoopFrame4 = 0;
	m_iLoopFrame5 = 0;
	m_iLoopFrame6 = 0;
	m_iLoopFrame7 = 0;
	m_iLoopFrame8 = 0;
	m_iLoopFrame9 = 0;
	m_iLoopFrame10 = 0;
	m_iLoopFrame11 = 0;
	m_iLoopFrame12 = 0;
	m_iLoopFrame13 = 0;
	m_iLoopFrame14 = 0;
	m_iLoopFrame15 = 0;
	m_iLoopFrame16 = 0;
	//
	m_startFrame = 0;
	m_startFrame2 = 0;
	m_startFrame3 = 0;
	m_startFrame4 = 0;
	m_startFrame5 = 0;
	m_startFrame6 = 0;
	m_startFrame7 = 0;
	m_startFrame8 = 0;
	m_startFrame9 = 0;
	m_startFrame10 = 0;
	m_startFrame11 = 0;
	m_startFrame12 = 0;
	m_startFrame13 = 0;
	m_startFrame14 = 0;
	m_startFrame15 = 0;
	m_startFrame16 = 0;
	//
	m_bGenLoopFrame = FALSE;
	//}}AFX_DATA_INIT
}

CSequencePropPage::~CSequencePropPage()
{
}

void CSequencePropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSequencePropPage)
	DDX_Text(pDX, IDC_PATH, m_path);
	//
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM, m_AnimationEnum);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM2, m_AnimationEnum2);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM3, m_AnimationEnum3);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM4, m_AnimationEnum4);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM5, m_AnimationEnum5);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM6, m_AnimationEnum6);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM7, m_AnimationEnum7);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM8, m_AnimationEnum8);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM9, m_AnimationEnum9);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM10, m_AnimationEnum10);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM11, m_AnimationEnum11);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM12, m_AnimationEnum12);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM13, m_AnimationEnum13);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM14, m_AnimationEnum14);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM15, m_AnimationEnum15);
	DDX_Text(pDX, IDC_EDIT_ANIMATIONENUM16, m_AnimationEnum16);
	//
	DDX_Text(pDX, IDC_FRAMECOUNT, m_frameCount);
	DDX_Text(pDX, IDC_FRAMECOUNT2, m_frameCount2);
	DDX_Text(pDX, IDC_FRAMECOUNT3, m_frameCount3);
	DDX_Text(pDX, IDC_FRAMECOUNT4, m_frameCount4);
	DDX_Text(pDX, IDC_FRAMECOUNT5, m_frameCount5);
	DDX_Text(pDX, IDC_FRAMECOUNT6, m_frameCount6);
	DDX_Text(pDX, IDC_FRAMECOUNT7, m_frameCount7);
	DDX_Text(pDX, IDC_FRAMECOUNT8, m_frameCount8);
	DDX_Text(pDX, IDC_FRAMECOUNT9, m_frameCount9);
	DDX_Text(pDX, IDC_FRAMECOUNT10, m_frameCount10);
	DDX_Text(pDX, IDC_FRAMECOUNT11, m_frameCount11);
	DDX_Text(pDX, IDC_FRAMECOUNT12, m_frameCount12);
	DDX_Text(pDX, IDC_FRAMECOUNT13, m_frameCount13);
	DDX_Text(pDX, IDC_FRAMECOUNT14, m_frameCount14);
	DDX_Text(pDX, IDC_FRAMECOUNT15, m_frameCount15);
	DDX_Text(pDX, IDC_FRAMECOUNT16, m_frameCount16);
	//
	DDX_Text(pDX, IDC_FRAMESPEED, m_frameSpeed);
	DDX_Text(pDX, IDC_FRAMESPEED2, m_frameSpeed2);
	DDX_Text(pDX, IDC_FRAMESPEED3, m_frameSpeed3);
	DDX_Text(pDX, IDC_FRAMESPEED4, m_frameSpeed4);
	DDX_Text(pDX, IDC_FRAMESPEED5, m_frameSpeed5);
	DDX_Text(pDX, IDC_FRAMESPEED6, m_frameSpeed6);
	DDX_Text(pDX, IDC_FRAMESPEED7, m_frameSpeed7);
	DDX_Text(pDX, IDC_FRAMESPEED8, m_frameSpeed8);
	DDX_Text(pDX, IDC_FRAMESPEED9, m_frameSpeed9);
	DDX_Text(pDX, IDC_FRAMESPEED10, m_frameSpeed10);
	DDX_Text(pDX, IDC_FRAMESPEED11, m_frameSpeed11);
	DDX_Text(pDX, IDC_FRAMESPEED12, m_frameSpeed12);
	DDX_Text(pDX, IDC_FRAMESPEED13, m_frameSpeed13);
	DDX_Text(pDX, IDC_FRAMESPEED14, m_frameSpeed14);
	DDX_Text(pDX, IDC_FRAMESPEED15, m_frameSpeed15);
	DDX_Text(pDX, IDC_FRAMESPEED16, m_frameSpeed16);
	//
	DDX_Text(pDX, IDC_LOOPFRAME, m_iLoopFrame);
	DDX_Text(pDX, IDC_LOOPFRAME2, m_iLoopFrame2);
	DDX_Text(pDX, IDC_LOOPFRAME3, m_iLoopFrame3);
	DDX_Text(pDX, IDC_LOOPFRAME4, m_iLoopFrame4);
	DDX_Text(pDX, IDC_LOOPFRAME5, m_iLoopFrame5);
	DDX_Text(pDX, IDC_LOOPFRAME6, m_iLoopFrame6);
	DDX_Text(pDX, IDC_LOOPFRAME7, m_iLoopFrame7);
	DDX_Text(pDX, IDC_LOOPFRAME8, m_iLoopFrame8);
	DDX_Text(pDX, IDC_LOOPFRAME9, m_iLoopFrame9);
	DDX_Text(pDX, IDC_LOOPFRAME10, m_iLoopFrame10);
	DDX_Text(pDX, IDC_LOOPFRAME11, m_iLoopFrame11);
	DDX_Text(pDX, IDC_LOOPFRAME12, m_iLoopFrame12);
	DDX_Text(pDX, IDC_LOOPFRAME13, m_iLoopFrame13);
	DDX_Text(pDX, IDC_LOOPFRAME14, m_iLoopFrame14);
	DDX_Text(pDX, IDC_LOOPFRAME15, m_iLoopFrame15);
	DDX_Text(pDX, IDC_LOOPFRAME16, m_iLoopFrame16);
	//
	DDX_Text(pDX, IDC_STARTFRAME, m_startFrame);
	DDX_Text(pDX, IDC_STARTFRAME2, m_startFrame2);
	DDX_Text(pDX, IDC_STARTFRAME3, m_startFrame3);
	DDX_Text(pDX, IDC_STARTFRAME4, m_startFrame4);
	DDX_Text(pDX, IDC_STARTFRAME5, m_startFrame5);
	DDX_Text(pDX, IDC_STARTFRAME6, m_startFrame6);
	DDX_Text(pDX, IDC_STARTFRAME7, m_startFrame7);
	DDX_Text(pDX, IDC_STARTFRAME8, m_startFrame8);
	DDX_Text(pDX, IDC_STARTFRAME9, m_startFrame9);
	DDX_Text(pDX, IDC_STARTFRAME10, m_startFrame10);
	DDX_Text(pDX, IDC_STARTFRAME11, m_startFrame11);
	DDX_Text(pDX, IDC_STARTFRAME12, m_startFrame12);
	DDX_Text(pDX, IDC_STARTFRAME13, m_startFrame13);
	DDX_Text(pDX, IDC_STARTFRAME14, m_startFrame14);
	DDX_Text(pDX, IDC_STARTFRAME15, m_startFrame15);
	DDX_Text(pDX, IDC_STARTFRAME16, m_startFrame16);
	//
	DDX_Check(pDX, IDC_CHECK_GENLOOPFRAME, m_bGenLoopFrame);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSequencePropPage, CPropertyPage)
	//{{AFX_MSG_MAP(CSequencePropPage)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM, OnButtonChooseanimationenum)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM2, OnButtonChooseanimationenum2)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM3, OnButtonChooseanimationenum3)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM4, OnButtonChooseanimationenum4)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM5, OnButtonChooseanimationenum5)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM6, OnButtonChooseanimationenum6)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM7, OnButtonChooseanimationenum7)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM8, OnButtonChooseanimationenum8)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM9, OnButtonChooseanimationenum9)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM10, OnButtonChooseanimationenum10)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM11, OnButtonChooseanimationenum11)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM12, OnButtonChooseanimationenum12)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM13, OnButtonChooseanimationenum13)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM14, OnButtonChooseanimationenum14)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM15, OnButtonChooseanimationenum15)
	ON_BN_CLICKED(IDC_BUTTON_CHOOSEANIMATIONENUM16, OnButtonChooseanimationenum16)
	//
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM, OnButtonClearanimationenum)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM2, OnButtonClearanimationenum2)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM3, OnButtonClearanimationenum3)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM4, OnButtonClearanimationenum4)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM5, OnButtonClearanimationenum5)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM6, OnButtonClearanimationenum6)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM7, OnButtonClearanimationenum7)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM8, OnButtonClearanimationenum8)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM9, OnButtonClearanimationenum9)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM10, OnButtonClearanimationenum10)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM11, OnButtonClearanimationenum11)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM12, OnButtonClearanimationenum12)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM13, OnButtonClearanimationenum13)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM14, OnButtonClearanimationenum14)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM15, OnButtonClearanimationenum15)
	ON_BN_CLICKED(IDC_BUTTON_CLEARANIMATIONENUM16, OnButtonClearanimationenum16)
	//
	ON_EN_KILLFOCUS(IDC_STARTFRAME, OnKillfocusStartframe)
	ON_EN_KILLFOCUS(IDC_STARTFRAME2, OnKillfocusStartframe2)
	ON_EN_KILLFOCUS(IDC_STARTFRAME3, OnKillfocusStartframe3)
	ON_EN_KILLFOCUS(IDC_STARTFRAME4, OnKillfocusStartframe4)
	ON_EN_KILLFOCUS(IDC_STARTFRAME5, OnKillfocusStartframe5)
	ON_EN_KILLFOCUS(IDC_STARTFRAME6, OnKillfocusStartframe6)
	ON_EN_KILLFOCUS(IDC_STARTFRAME7, OnKillfocusStartframe7)
	ON_EN_KILLFOCUS(IDC_STARTFRAME8, OnKillfocusStartframe8)
	ON_EN_KILLFOCUS(IDC_STARTFRAME9, OnKillfocusStartframe9)
	ON_EN_KILLFOCUS(IDC_STARTFRAME10, OnKillfocusStartframe10)
	ON_EN_KILLFOCUS(IDC_STARTFRAME11, OnKillfocusStartframe11)
	ON_EN_KILLFOCUS(IDC_STARTFRAME12, OnKillfocusStartframe12)
	ON_EN_KILLFOCUS(IDC_STARTFRAME13, OnKillfocusStartframe13)
	ON_EN_KILLFOCUS(IDC_STARTFRAME14, OnKillfocusStartframe14)
	ON_EN_KILLFOCUS(IDC_STARTFRAME15, OnKillfocusStartframe15)
	ON_EN_KILLFOCUS(IDC_STARTFRAME16, OnKillfocusStartframe16)
	//
	ON_EN_KILLFOCUS(IDC_LOOPFRAME, OnKillfocusLoopframe)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME2, OnKillfocusLoopframe2)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME3, OnKillfocusLoopframe3)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME4, OnKillfocusLoopframe4)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME5, OnKillfocusLoopframe5)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME6, OnKillfocusLoopframe6)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME7, OnKillfocusLoopframe7)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME8, OnKillfocusLoopframe8)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME9, OnKillfocusLoopframe9)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME10, OnKillfocusLoopframe10)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME11, OnKillfocusLoopframe11)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME12, OnKillfocusLoopframe12)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME13, OnKillfocusLoopframe13)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME14, OnKillfocusLoopframe14)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME15, OnKillfocusLoopframe15)
	ON_EN_KILLFOCUS(IDC_LOOPFRAME16, OnKillfocusLoopframe16)
	//
	ON_EN_KILLFOCUS(IDC_FRAMESPEED, OnKillfocusFramespeed)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED2, OnKillfocusFramespeed2)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED3, OnKillfocusFramespeed3)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED4, OnKillfocusFramespeed4)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED5, OnKillfocusFramespeed5)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED6, OnKillfocusFramespeed6)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED7, OnKillfocusFramespeed7)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED8, OnKillfocusFramespeed8)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED9, OnKillfocusFramespeed9)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED10, OnKillfocusFramespeed10)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED11, OnKillfocusFramespeed11)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED12, OnKillfocusFramespeed12)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED13, OnKillfocusFramespeed13)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED14, OnKillfocusFramespeed14)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED15, OnKillfocusFramespeed15)
	ON_EN_KILLFOCUS(IDC_FRAMESPEED16, OnKillfocusFramespeed16)
	//
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT, OnKillfocusFramecount)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT2, OnKillfocusFramecount2)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT3, OnKillfocusFramecount3)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT4, OnKillfocusFramecount4)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT5, OnKillfocusFramecount5)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT6, OnKillfocusFramecount6)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT7, OnKillfocusFramecount7)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT8, OnKillfocusFramecount8)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT9, OnKillfocusFramecount9)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT10, OnKillfocusFramecount10)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT11, OnKillfocusFramecount11)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT12, OnKillfocusFramecount12)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT13, OnKillfocusFramecount13)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT14, OnKillfocusFramecount14)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT15, OnKillfocusFramecount15)
	ON_EN_KILLFOCUS(IDC_FRAMECOUNT16, OnKillfocusFramecount16)
	//
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM, OnKillfocusEditAnimationenum)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM2, OnKillfocusEditAnimationenum2)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM3, OnKillfocusEditAnimationenum3)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM4, OnKillfocusEditAnimationenum4)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM5, OnKillfocusEditAnimationenum5)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM6, OnKillfocusEditAnimationenum6)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM7, OnKillfocusEditAnimationenum7)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM8, OnKillfocusEditAnimationenum8)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM9, OnKillfocusEditAnimationenum9)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM10, OnKillfocusEditAnimationenum10)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM11, OnKillfocusEditAnimationenum11)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM12, OnKillfocusEditAnimationenum12)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM13, OnKillfocusEditAnimationenum13)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM14, OnKillfocusEditAnimationenum14)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM15, OnKillfocusEditAnimationenum15)
	ON_EN_KILLFOCUS(IDC_EDIT_ANIMATIONENUM16, OnKillfocusEditAnimationenum16)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSequencePropPage message handlers

void CSequencePropPage::OnButtonChooseanimationenum() 
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))	
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal()==IDOK)
		{
			SetModified(true);	// enable the apply button
			m_AnimationEnum = sReturnString;
			UpdateData(DATA_TO_DIALOG);	
			HandleAllItemsGraying();
		}		
	}
}

// deliberately does not do enum...

#define DEFAULT_FROM_MASTER(number) \
			if (m_AnimationEnum ## number.IsEmpty())	\
			{											\
				m_frameCount ## number = m_frameCount;	\
				m_frameSpeed ## number = m_frameSpeed;	\
				m_iLoopFrame ## number = m_iLoopFrame;	\
				m_startFrame ## number = m_startFrame;	\
			}

void CSequencePropPage::OnButtonChooseanimationenum2() 
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))	
	{		
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal()==IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(2);
			m_AnimationEnum2 = sReturnString;			
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}		
	}
}

void CSequencePropPage::OnButtonChooseanimationenum3() 
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))	
	{		
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal()==IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(3);
			m_AnimationEnum3 = sReturnString;			
			UpdateData(DATA_TO_DIALOG);	
			HandleAllItemsGraying();
		}		
	}
}

void CSequencePropPage::OnButtonChooseanimationenum4() 
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))	
	{		
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal()==IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(4);
			m_AnimationEnum4 = sReturnString;			
			UpdateData(DATA_TO_DIALOG);	
			HandleAllItemsGraying();
		}		
	}
}

void CSequencePropPage::OnButtonChooseanimationenum5() 
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{		
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal()==IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(5);
			m_AnimationEnum5 = sReturnString;			
			UpdateData(DATA_TO_DIALOG);	
			HandleAllItemsGraying();
		}		
	}
}

void CSequencePropPage::OnButtonChooseanimationenum6() 
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{		
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal()==IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(6);
			m_AnimationEnum6 = sReturnString;			
			UpdateData(DATA_TO_DIALOG);	
			HandleAllItemsGraying();
		}		
	}
}

void CSequencePropPage::OnButtonChooseanimationenum7()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(7);
			m_AnimationEnum7 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum8()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(8);
			m_AnimationEnum8 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum9()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(9);
			m_AnimationEnum9 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum10()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(10);
			m_AnimationEnum10 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum11()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(11);
			m_AnimationEnum11 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum12()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(12);
			m_AnimationEnum12 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum13()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(13);
			m_AnimationEnum13 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum14()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(14);
			m_AnimationEnum14 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum15()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(15);
			m_AnimationEnum15 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

void CSequencePropPage::OnButtonChooseanimationenum16()
{
	char sReturnString[200];	// OTT

	if (UpdateData(DIALOG_TO_DATA))
	{
		CAnimPicker animDialog(&sReturnString[0]);
		if (animDialog.DoModal() == IDOK)
		{
			SetModified(true);	// enable the apply button
			DEFAULT_FROM_MASTER(16);
			m_AnimationEnum16 = sReturnString;
			UpdateData(DATA_TO_DIALOG);
			HandleAllItemsGraying();
		}
	}
}

/////////////////////////////////////////////////////////////////

void CSequencePropPage::OnButtonClearanimationenum() 
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum = "";
		UpdateData(DATA_TO_DIALOG);	
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum2() 
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum2 = "";
		UpdateData(DATA_TO_DIALOG);	
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum3() 
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum3 = "";
		UpdateData(DATA_TO_DIALOG);	
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum4() 
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum4 = "";
		UpdateData(DATA_TO_DIALOG);	
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum5() 
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum5 = "";
		UpdateData(DATA_TO_DIALOG);	
		HandleAllItemsGraying();				
	}
}

void CSequencePropPage::OnButtonClearanimationenum6() 
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum6 = "";
		UpdateData(DATA_TO_DIALOG);	
		HandleAllItemsGraying();				
	}
}

void CSequencePropPage::OnButtonClearanimationenum7()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum7 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum8()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum8 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum9()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum9 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum10()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum10 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum11()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum11 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum12()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum12 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum13()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum13 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum14()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum14 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum15()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum15 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

void CSequencePropPage::OnButtonClearanimationenum16()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		SetModified(true);	// enable the apply button
		m_AnimationEnum16 = "";
		UpdateData(DATA_TO_DIALOG);
		HandleAllItemsGraying();
	}
}

/////////////////////////////////////////////////////////////////

void CSequencePropPage::OnCancel() 
{
	UpdateData(DATA_TO_DIALOG);		// stops a nasty bug in MFC whereby having crap data (eg 'a' in an int-box) gets you stuck in a loop if you ESC from a prop page
	CPropertyPage::OnCancel();
}

void CSequencePropPage::OnKillfocusStartframe() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusStartframe2() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusStartframe3() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusStartframe4() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusStartframe5() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusStartframe6() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusStartframe7()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe8()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe9()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe10()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe11()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe12()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe13()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe14()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe15()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusStartframe16()
{
	ONKILLFOCUS;
}

/////////////////////////////////////////////////////////////////

void CSequencePropPage::OnKillfocusLoopframe() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusLoopframe2() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusLoopframe3() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusLoopframe4() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusLoopframe5() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusLoopframe6() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusLoopframe7()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe8()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe9()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe10()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe11()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe12()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe13()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe14()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe15()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusLoopframe16()
{
	ONKILLFOCUS;
}

/////////////////////////////////////////////////////////////////

void CSequencePropPage::OnKillfocusFramespeed()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed2()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed3()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed4()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed5()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed6() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusFramespeed7()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed8()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed9()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed10()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed11()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed12()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed13()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed14()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed15()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramespeed16()
{
	ONKILLFOCUS;
}

/////////////////////////////////////////////////////////////////

void CSequencePropPage::OnKillfocusFramecount()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount2()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount3()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount4()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount5() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusFramecount6()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount7()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount8()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount9()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount10()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount11()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount12()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount13()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount14()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount15()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusFramecount16()
{
	ONKILLFOCUS;
}

/////////////////////////////////////////////////////////////////

void CSequencePropPage::OnKillfocusEditAnimationenum()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum2()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum3()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum4()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum5()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum6() 
{
	ONKILLFOCUS;	
}

void CSequencePropPage::OnKillfocusEditAnimationenum7()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum8()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum9()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum10()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum11()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum12()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum13()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum14()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum15()
{
	ONKILLFOCUS;
}

void CSequencePropPage::OnKillfocusEditAnimationenum16()
{
	ONKILLFOCUS;
}

// this is called by the OK and/or Apply buttons...

void CSequencePropPage::OkOrApply()
{
	m_sequence->SetFrameCount(m_frameCount);
	m_sequence->SetFrameSpeed(m_frameSpeed);
	m_sequence->SetStartFrame(m_startFrame);
	m_sequence->SetLoopFrame(m_iLoopFrame);
	m_sequence->SetEnum(m_AnimationEnum);		
	m_sequence->SetGenLoopFrame(!!m_bGenLoopFrame);

#define OK_ADDITIONAL(number)	\
	m_sequence->AdditionalSeqs[number-2]->SetFrameCount	(m_frameCount ## number);	\
	m_sequence->AdditionalSeqs[number-2]->SetFrameSpeed	(m_frameSpeed ## number);	\
	m_sequence->AdditionalSeqs[number-2]->SetStartFrame	(m_startFrame ## number);	\
	m_sequence->AdditionalSeqs[number-2]->SetLoopFrame	(m_iLoopFrame ## number);	\
	m_sequence->AdditionalSeqs[number-2]->SetEnum		(m_AnimationEnum ## number);

	OK_ADDITIONAL(2);
	OK_ADDITIONAL(3);
	OK_ADDITIONAL(4);
	OK_ADDITIONAL(5);
	OK_ADDITIONAL(6);
	OK_ADDITIONAL(7);
	OK_ADDITIONAL(8);
	OK_ADDITIONAL(9);
	OK_ADDITIONAL(10);
	OK_ADDITIONAL(11);
	OK_ADDITIONAL(12);
	OK_ADDITIONAL(13);
	OK_ADDITIONAL(14);
	OK_ADDITIONAL(15);
	OK_ADDITIONAL(16);

	// ensure I don't forget any future expansions by putting the next index in the sequence in an err-check...

	#if !(17 == (MAX_ADDITIONAL_SEQUENCES+2))
	#error Need more OK_ code...
	#endif

	m_sequence->SetPath(m_path);	
	*m_soilFlag = true;
}

void CSequencePropPage::OnOK() 
{	
	OkOrApply();

	// damn stupid C++...

	if (ghAssimilateView)
	{
		CAssimilateDoc* pDoc = ghAssimilateView->GetDocument();
		if (pDoc)
		{
			pDoc->UpdateAllViews(NULL, AS_FILESUPDATED, NULL);
		}
	}

	CPropertyPage::OnOK();
}

BOOL CSequencePropPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_frameCount = m_sequence->GetFrameCount();
	m_frameSpeed = m_sequence->GetFrameSpeed();	
	m_startFrame = m_sequence->GetStartFrame();
	m_iLoopFrame = m_sequence->GetLoopFrame();
	m_AnimationEnum = m_sequence->GetEnum();
	m_bGenLoopFrame = m_sequence->GetGenLoopFrame();

#define INIT_ADDITIONAL(number)	\
	m_frameCount ## number = m_sequence->AdditionalSeqs[number-2]->GetFrameCount();	\
	m_frameSpeed ## number = m_sequence->AdditionalSeqs[number-2]->GetFrameSpeed();	\
	m_startFrame ## number = m_sequence->AdditionalSeqs[number-2]->GetStartFrame();	\
	m_iLoopFrame ## number = m_sequence->AdditionalSeqs[number-2]->GetLoopFrame();	\
	m_AnimationEnum ## number = m_sequence->AdditionalSeqs[number-2]->GetEnum();

	INIT_ADDITIONAL(2);
	INIT_ADDITIONAL(3);
	INIT_ADDITIONAL(4);
	INIT_ADDITIONAL(5);
	INIT_ADDITIONAL(6);
	INIT_ADDITIONAL(7);
	INIT_ADDITIONAL(8);
	INIT_ADDITIONAL(9);
	INIT_ADDITIONAL(10);
	INIT_ADDITIONAL(11);
	INIT_ADDITIONAL(12);
	INIT_ADDITIONAL(13);
	INIT_ADDITIONAL(14);
	INIT_ADDITIONAL(15);
	INIT_ADDITIONAL(16);

	// ensure I don't forget any future expansions by putting the next index in the sequence in an err-check...

	#if !(17 == (MAX_ADDITIONAL_SEQUENCES+2))
	#error Need more INIT_ code...
	#endif

	m_path = m_sequence->GetPath();

	UpdateData(DATA_TO_DIALOG);	

	HandleAllItemsGraying();	
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CSequencePropPage::OnApply() 
{
	OkOrApply();	
	return CPropertyPage::OnApply();
}


void CSequencePropPage::HandleAdditionalEditBoxesGraying()
{
	bool bOnOff;

	// note check runs off the dialog box strings, NOT the standard m_sequence member (because the data isn't set till OK)

#define GRAY_ADDITIONAL(number)	\
	bOnOff = !(m_sequence->GetEnumTypeFromString(m_AnimationEnum ## number) == ET_INVALID);\
	GetDlgItem(IDC_STARTFRAME ## number)->EnableWindow(bOnOff);	\
	GetDlgItem(IDC_FRAMECOUNT ## number)->EnableWindow(bOnOff);	\
	GetDlgItem(IDC_LOOPFRAME  ## number)->EnableWindow(bOnOff);	\
	GetDlgItem(IDC_FRAMESPEED ## number)->EnableWindow(bOnOff);

	GRAY_ADDITIONAL(2);
	GRAY_ADDITIONAL(3);
	GRAY_ADDITIONAL(4);
	GRAY_ADDITIONAL(5);
	GRAY_ADDITIONAL(6);
	GRAY_ADDITIONAL(7);
	GRAY_ADDITIONAL(8);
	GRAY_ADDITIONAL(9);
	GRAY_ADDITIONAL(10);
	GRAY_ADDITIONAL(11);
	GRAY_ADDITIONAL(12);
	GRAY_ADDITIONAL(13);
	GRAY_ADDITIONAL(14);
	GRAY_ADDITIONAL(15);
	GRAY_ADDITIONAL(16);

	// ensure I don't forget any future expansions by putting the next index in the sequence in an err-check...

	#if !(17 == (MAX_ADDITIONAL_SEQUENCES+2))
	#error Need more gray-handler code...
	#endif
}

void CSequencePropPage::HandleAllItemsGraying()
{
	if (UpdateData(DIALOG_TO_DATA))
	{
		HandleAdditionalEditBoxesGraying();
	}
}
