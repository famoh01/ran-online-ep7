// stdafx.h : �� ������� �ʰ� ���� ����ϴ�
// ǥ�� �ý��� ���� ���� �� ������Ʈ ���� ���� ������ 
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

//#define WIN32_LEAN_AND_MEAN		// ���� ������ �ʴ� ������ Windows ������� �����մϴ�.
//// Windows ��� �����Դϴ�.
//#include <windows.h>
//// C�� ��Ÿ�� ��� �����Դϴ�.
//#include <stdlib.h>
//#include <malloc.h>
//#include <memory.h>
//#include <tchar.h>

// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#define STRSAFE_NO_DEPRECATE
#include "strsafe.h"

//	Note : �⺻ �ش� ����.
//
#include <windows.h>
#include <windowsx.h>
#include <basetsd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tchar.h>

#include <malloc.h> // _alloca
#include <mmsystem.h>
#include <objbase.h>
#include <assert.h>

#include <string>
#include <vector>
#include <list>
#include <algorithm>
using namespace std;

#ifndef STRICT
	#define STRICT
#endif //STRICT

//	Note : DX ���� �ش� ����.
//
#include <ddraw.h>

#ifndef DIRECTINPUT_VERSION
	#define DIRECTINPUT_VERSION 0x0800
#endif //DIRECTINPUT_VERSION
#include <dinput.h>

#include <d3d9.h>
#include <D3d8types.h>
#include <D3DX8.h>
#include <D3dx8math.h>
#include <DXErr8.h>
#include <rmxfguid.h>


//	Note : DX ��ƿ �ش� ����.
//
#include "xrmxftmpl.h"
#include "dxutil.h"
#include "d3dutil.h"
#include "d3dfile.h"
#include "d3dfont.h"
#include "d3dapp.h"

//	Note : engine lib �ش� ����.
//
#include "DebugSet.h"
#include "profile.h"

#include <afxdlgs.h>