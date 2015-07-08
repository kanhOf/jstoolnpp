/*
This file is part of JSONViewer Plugin for Notepad++
Copyright (C)2011 Kapil Ratnani <kapil.ratnani@iiitb.net>

This file is also part of JSMinNpp Plugin for Notepad++ now :)
Copyright (C)2012 SUN Junwen <sunjw8888@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "JSONDialog.h"

#include "jsonpp.h"
#include "jsonStringProc.h"
#include "JsonTree.h"

using namespace std;
using namespace sunjwbase;

extern NppData nppData;


BOOL CALLBACK JSONDialog::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hWnd = getHSelf();
	switch (message) 
	{
	case WM_INITDIALOG:
		{
			m_hDlg = hWnd;
			m_hTree = GetDlgItem(hWnd, IDC_TREE_JSON); // tree control
			::SendMessage(hWnd, DM_SETDEFID, 
                        (WPARAM) IDC_BTN_SEARCH, 
                        (LPARAM) 0);
		}
		return FALSE;
	case WM_SIZE:
		{
			int iDlgWidth,iDlgHeight;
			iDlgWidth = LOWORD(lParam);
			iDlgHeight = HIWORD(lParam);

			int iJsonTreeWidth = iDlgWidth, iJsonTreeHeight = iDlgHeight - 55;
			SetWindowPos(GetDlgItem(hWnd, IDC_TREE_JSON), 
				HWND_TOP, 0, 30, iJsonTreeWidth, iJsonTreeHeight, 
				SWP_SHOWWINDOW);

			if(iDlgWidth < 215) // 170 + 45
				iDlgWidth = 215;

			int iSearchEditWidth = iDlgWidth - 170;
			SetWindowPos(GetDlgItem(hWnd, IDC_BTN_SEARCH), 
				HWND_TOP, 92 + iSearchEditWidth, 0, 74, 22, 
				SWP_SHOWWINDOW);

			SetWindowPos(GetDlgItem(hWnd, IDC_SEARCHEDIT), 
				HWND_TOP, 88, 2, iSearchEditWidth, 18, 
				SWP_SHOWWINDOW);

			int iJsonPathEditWidth = iDlgWidth - 4;
			SetWindowPos(GetDlgItem(hWnd, IDC_JSONPATH), 
				HWND_TOP, 1, iJsonTreeHeight + 33, iJsonPathEditWidth, 18, 
				SWP_SHOWWINDOW);
		}
		return FALSE;
	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
            {
				case IDC_BTN_REFRESH:
					refreshTree(m_hCurrScintilla);
					break;
				case IDC_BTN_SEARCH:
					search();
					break;
			}
		}
		return FALSE;
	case WM_NOTIFY:
		{
			clickJsonTree(lParam);
		}
		return FALSE;
	default:
		return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
	}

	return FALSE;
}

void JSONDialog::disableControls()
{
	EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_REFRESH), FALSE);
	EnableWindow(GetDlgItem(m_hDlg, IDC_SEARCHEDIT), FALSE);
	EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_SEARCH), FALSE);
	EnableWindow(GetDlgItem(m_hDlg, IDC_TREE_JSON), FALSE);
}

void JSONDialog::enableControls()
{
	EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_REFRESH), TRUE);
	EnableWindow(GetDlgItem(m_hDlg, IDC_SEARCHEDIT), TRUE);
	EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_SEARCH), TRUE);
	EnableWindow(GetDlgItem(m_hDlg, IDC_TREE_JSON), TRUE);
}

void JSONDialog::focusOnControl(int nId)
{
	::PostMessage(m_hDlg, 
		WM_NEXTDLGCTL, 
		(WPARAM) GetDlgItem(m_hDlg, nId), 
		TRUE);
}

/*
Delete all items from the tree and creates the root node
*/
HTREEITEM JSONDialog::initTree()
{
	int TreeCount = TreeView_GetCount(GetDlgItem(m_hDlg, IDC_TREE_JSON));
	if(TreeCount>0)
		TreeView_DeleteAllItems(GetDlgItem(m_hDlg, IDC_TREE_JSON));

	HTREEITEM root = insertTree(TEXT("ROOT"), -1, TVI_ROOT);
	
	return root;		
}

HTREEITEM JSONDialog::insertTree(LPCTSTR text, LPARAM lparam, HTREEITEM parentNode)
{
	TV_INSERTSTRUCT tvinsert;

	if(parentNode == TVI_ROOT)
	{
		tvinsert.hParent = NULL;
		tvinsert.hInsertAfter = TVI_ROOT;
	}
	else
	{
		tvinsert.hParent = parentNode;   
		tvinsert.hInsertAfter = TVI_LAST;
	}
	tvinsert.item.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_PARAM;
	tvinsert.item.pszText = (LPTSTR)text;
	tvinsert.item.lParam = lparam;

	HTREEITEM item = (HTREEITEM)SendDlgItemMessage(
		m_hDlg, IDC_TREE_JSON, TVM_INSERTITEM, 0, (LPARAM)&tvinsert);

	return item;
}

void JSONDialog::refreshTree(HWND hCurrScintilla)
{
	disableControls();

	// Scintilla handle will change.
	m_hCurrScintilla = hCurrScintilla;
	// Rebuild JsonTree.
	m_jsonTree = JsonTree(m_hCurrScintilla, m_hDlg, m_hTree);

	size_t jsLen, jsLenSel;
	jsLen = ::SendMessage(m_hCurrScintilla, SCI_GETTEXTLENGTH, 0, 0);
    if (jsLen == 0) 
		return;

	size_t selStart = ::SendMessage(m_hCurrScintilla, SCI_GETSELECTIONSTART, 0, 0);
	size_t selEnd = ::SendMessage(m_hCurrScintilla, SCI_GETSELECTIONEND, 0, 0);
	bool bFormatSel = !(selStart == selEnd);

	char *pJS;

	if(!bFormatSel)
	{
		pJS = new char[jsLen+1];
		::SendMessage(m_hCurrScintilla, SCI_GETTEXT, jsLen + 1, (LPARAM)pJS);
		m_iSelStartLine = 0;
	}
	else
	{
		jsLenSel = ::SendMessage(m_hCurrScintilla, SCI_GETSELTEXT, 0, 0);
		pJS = new char[jsLenSel];
		::SendMessage(m_hCurrScintilla, SCI_GETSELTEXT, jsLen, (LPARAM)pJS);

		m_iSelStartLine = ::SendMessage(m_hCurrScintilla, SCI_LINEFROMPOSITION, selStart, 0);
	}

	std::string strJSCode(pJS);
	int codePage = ::SendMessage(m_hCurrScintilla, SCI_GETCODEPAGE, 0, 0);
	if(codePage == 65001)
	{
		// UTF-8
		strJSCode = asciiconv(strJSCode);
	}

	JsonStringProc jsonProc(strJSCode);

	JsonValue jsonVal;
	jsonProc.Go(jsonVal);

	drawTree(jsonVal);

	delete[] pJS;

	enableControls();

	focusOnControl(IDC_SEARCHEDIT);
}

void JSONDialog::drawTree(const JsonValue& jsonValue)
{
	HTREEITEM rootNode;

	rootNode = initTree();

	const JsonValue::VALUE_TYPE& valType = jsonValue.GetValueType();
	if(valType == JsonValue::UNKNOWN_VALUE)
	{
		::MessageBox(nppData._nppHandle, TEXT("Cannot parse json..."), TEXT("JsonViewer"), MB_OK);
		return;
	}

	insertJsonValue(jsonValue, rootNode);

	TreeView_Expand(GetDlgItem(m_hDlg, IDC_TREE_JSON), rootNode, TVE_EXPAND);
}

void JSONDialog::insertJsonValue(const JsonValue& jsonValue, HTREEITEM node)
{
	JsonValue::VALUE_TYPE valType = jsonValue.GetValueType();
	
	if(valType == JsonValue::MAP_VALUE)
	{
		const JsonUnsortedMap& mapValue = jsonValue.GetMapValue();

		JsonUnsortedMap::const_iterator itr = mapValue.begin();
		for(; itr != mapValue.end(); ++itr)
		{
			const string& key = itr->first;
			const JsonValue& value = itr->second;

			insertJsonValue(key, value, node);
		}
	}
	else if(valType == JsonValue::ARRAY_VALUE)
	{
		const JsonVec& arrayValue = jsonValue.GetArrayValue();

		char buffer[1024];

		JsonVec::const_iterator itr = arrayValue.begin();
		JsonVec::size_type count = 0;
		for(; itr != arrayValue.end(); ++itr, ++count)
		{
			const JsonValue& value = *itr;
			
			itoa(count, buffer, 10);
			string key(buffer);
			key = "[" + key + "]";

			insertJsonValue(key, value, node);
		}
	}
}

void JSONDialog::insertJsonValue(const string& key, const JsonValue& jsonValue, HTREEITEM node)
{
	tstring tstr(strtotstr(key));
	JsonValue::VALUE_TYPE valType = jsonValue.GetValueType();

	if(valType == JsonValue::UNKNOWN_VALUE ||
		valType == JsonValue::NUMBER_VALUE ||
		valType == JsonValue::BOOL_VALUE ||
		valType == JsonValue::REGULAR_VALUE)
	{
		tstr.append(TEXT(" : "));
		tstr.append(strtotstr(jsonValue.GetStrValue()));
		insertTree(tstr.c_str(), jsonValue.line, node);
	}
	else if(valType == JsonValue::STRING_VALUE)
	{
		tstr.append(TEXT(" : "));
		tstr.append(TEXT("\""));
		tstr.append(strtotstr(jsonValue.GetStrValue()));
		tstr.append(TEXT("\""));
		insertTree(tstr.c_str(), jsonValue.line, node);
	}
	if(valType == JsonValue::MAP_VALUE ||
		valType == JsonValue::ARRAY_VALUE)
	{
		if(valType == JsonValue::MAP_VALUE)
			tstr.append(TEXT(" : [Object]"));
		if(valType == JsonValue::ARRAY_VALUE)
			tstr.append(TEXT(" : [Array]"));

		HTREEITEM newNode = insertTree(tstr.c_str(), jsonValue.line, node);
		insertJsonValue(jsonValue, newNode);
	}
}

void JSONDialog::clickJsonTree(LPARAM lParam)
{
	LPNMHDR lpnmh = (LPNMHDR)lParam;
    if(lpnmh->code == NM_CLICK && lpnmh->idFrom == IDC_TREE_JSON)  
    {
		DWORD dwPos = GetMessagePos();
		POINT pt;
		pt.x = LOWORD(dwPos);
		pt.y = HIWORD(dwPos);
		ScreenToClient(m_hTree, &pt);
		TVHITTESTINFO ht = {0};
		ht.pt = pt;
		//ht.flags = TVHT_ONITEMLABEL;
		HTREEITEM hItem = TreeView_HitTest(m_hTree, &ht);
		if(hItem && (ht.flags & TVHT_ONITEMLABEL))
		{
			clickJsonTreeItem(hItem);
		}
	}  
}

void JSONDialog::clickJsonTreeItem(HTREEITEM htiNode)
{
	string strJsonPath = m_jsonTree.getJsonNodePath(htiNode);
	SetDlgItemText(m_hDlg, IDC_JSONPATH, strtotstr(strJsonPath).c_str());
	m_jsonTree.jumpToSciLine(htiNode, m_iSelStartLine);
}

void JSONDialog::search()
{
	disableControls();

	TCHAR buffer[256];
	GetWindowText(GetDlgItem(m_hDlg, IDC_SEARCHEDIT), buffer, 255);
	
	tstring tstrSearchKey(buffer);
	string strSearchKey = tstrtostr(tstrSearchKey);

	HTREEITEM htiSelected = TreeView_GetSelection(m_hTree);
	if(htiSelected == NULL)
	{
		// Nothing, so we do search from ROOT
		htiSelected = TreeView_GetRoot(m_hTree);
	}
	if(htiSelected == NULL)
		return; // Still NULL, return.

	/*
	 * Now, we have a valid "selectedItem".
	 * We do search.
	 */
	HTREEITEM htiFound = NULL;
	
	htiFound = m_jsonTree.search(strSearchKey, htiSelected);

	if(htiFound != NULL)
	{
		// We found in search.
		TreeView_SelectItem(m_hTree, htiFound);
		clickJsonTreeItem(htiFound);
	}
	else
	{
		MessageBox(m_hDlg, TEXT("No results found."), TEXT("Search in Json"), MB_ICONINFORMATION | MB_OK);
	}

	enableControls();
}

