//	UIControlNodeFocusMan ��� Ŭ����
//
//	���� �ۼ��� : ���⿱
//	���� ������ : 
//	�α�
//		[2003.11.13] - ��Ŀ�������� ����.

#pragma	once

#include "UIDataType.h"
#include "UIControl.h"
#include <list>

class	CUIFocusContainer
{
public:
	struct SUICONTROL
	{
		UIGUID cID;
		CUIControl * pUIControl;
	};

public:
	typedef std::list<SUICONTROL>					UICONTROL_FOCUSLIST;
	typedef	UICONTROL_FOCUSLIST::iterator			UICONTROL_FOCUSLIST_ITER;
	typedef	UICONTROL_FOCUSLIST::const_iterator		UICONTROL_FOCUSLIST_CITER;
	typedef	UICONTROL_FOCUSLIST::reverse_iterator	UICONTROL_FOCUSLIST_RITER;

public:
	CUIFocusContainer ();
	virtual	~CUIFocusContainer ();

public:
	bool empty()		{ return m_UIFocusList.empty (); }
	int size()			{ return static_cast<int>(m_UIFocusList.size()); }

	void RemoveAll()	{ m_UIFocusList.clear(); }
	bool EraseControl( UIGUID cID );

public:
	CUIControl * IsInserted( UIGUID cID );

	bool InsertHead( UIGUID cID, CUIControl * pUIContorl );
	bool InsertTail( UIGUID cID, CUIControl * pUIContorl );
	bool InsertAfter( UIGUID Where_cID, UIGUID cID, CUIControl * pUIContorl );

	CUIControl * GetTail();

public:	
	CUIControl * GetFocusControl();
	const UICONTROL_FOCUSLIST& GetFocusList() const { return m_UIFocusList; }

private:
	UICONTROL_FOCUSLIST	m_UIFocusList;
};