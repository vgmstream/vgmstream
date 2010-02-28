#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <io.h>

#include <foobar2000.h>
#include <helpers.h>
#include <shared.h>
#include "foo_prefs.h"
extern "C" {
#include "../src/vgmstream.h"
#include "../src/util.h"
}
#include "foo_vgmstream.h"


static const char * priority_strings[] = {"Idle","Lowest","Below Normal","Normal","Above Normal","Highest (not recommended)","Time Critical (not recommended)"};
static const int priority_values[] = {THREAD_PRIORITY_IDLE,THREAD_PRIORITY_LOWEST,THREAD_PRIORITY_BELOW_NORMAL,THREAD_PRIORITY_NORMAL,THREAD_PRIORITY_ABOVE_NORMAL,THREAD_PRIORITY_HIGHEST,THREAD_PRIORITY_TIME_CRITICAL};

static const GUID guid_cfg_Priority = { 0x6a94e07f, 0xf565, 0x455f, { 0x87, 0x27, 0x30, 0xce, 0x81, 0x44, 0xd2, 0xf } };
static const GUID guid_cfg_LoopForever = { 0xa19e36eb, 0x72a0, 0x4077, { 0x91, 0x43, 0x38, 0xb4, 0x5, 0xfc, 0x91, 0xc5 } };
static const GUID guid_cfg_IgnoreLoop = { 0xddda7ab6, 0x7bb6, 0x4abe, { 0xb9, 0x66, 0x2d, 0xb7, 0x8f, 0xe4, 0xcc, 0xab } };
static const GUID guid_cfg_LoopCount = { 0xfc8dfd72, 0xfae8, 0x44cc, { 0xbe, 0x99, 0x1c, 0x7b, 0x27, 0x7a, 0xb6, 0xb9 } };
static const GUID guid_cfg_FadeLength = { 0x61da7ef1, 0x56a5, 0x4368, { 0xae, 0x6, 0xec, 0x6f, 0xd7, 0xe6, 0x15, 0x5d } };
static const GUID guid_cfg_FadeDelay = { 0x73907787, 0xaf49, 0x4659, { 0x96, 0x8e, 0x9f, 0x70, 0xa1, 0x62, 0x49, 0xc4 } };

static cfg_uint cfg_Priority(guid_cfg_Priority, DEFAULT_THREAD_PRIORITY);
static cfg_bool cfg_LoopForever(guid_cfg_LoopForever, DEFAULT_LOOP_FOREVER);
static cfg_bool cfg_IgnoreLoop(guid_cfg_IgnoreLoop, DEFAULT_IGNORE_LOOP);
static cfg_string cfg_LoopCount(guid_cfg_LoopCount, DEFAULT_LOOP_COUNT);
static cfg_string cfg_FadeLength(guid_cfg_FadeLength, DEFAULT_FADE_SECONDS);
static cfg_string cfg_FadeDelay(guid_cfg_FadeDelay, DEFAULT_FADE_DELAY_SECONDS);

// Needs to be here in rder to access the static config
void input_vgmstream::load_settings()
{
	// no verification needed here, as it is done below
	sscanf(cfg_FadeLength.get_ptr(),"%lf",&fade_seconds);
	sscanf(cfg_LoopCount.get_ptr(),"%lf",&loop_count);
	sscanf(cfg_FadeDelay.get_ptr(),"%lf",&fade_delay_seconds);
	thread_priority = priority_values[cfg_Priority];
	loop_forever = cfg_LoopForever;
	ignore_loop = cfg_IgnoreLoop;
}

const char * vgmstream_prefs::get_name()
{
	return "vgmstream";
}


GUID vgmstream_prefs::get_guid()
{
	static const GUID guid = { 0x2b5d0302, 0x165b, 0x409c, { 0x94, 0x74, 0x2c, 0x8c, 0x2c, 0xd7, 0x6a, 0x25 } };;
	return guid;
}


GUID vgmstream_prefs::get_parent_guid()
{
	return guid_input;
}


BOOL vgmstreamPreferences::OnInitDialog(CWindow, LPARAM)
{
	CheckDlgButton(IDC_LOOP_FOREVER, cfg_LoopForever?BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(IDC_IGNORE_LOOP, cfg_IgnoreLoop?BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(IDC_LOOP_NORMALLY, (!cfg_IgnoreLoop && !cfg_LoopForever)?BST_CHECKED:BST_UNCHECKED);

	uSetDlgItemText(m_hWnd, IDC_LOOP_COUNT, cfg_LoopCount);
	uSetDlgItemText(m_hWnd, IDC_FADE_SECONDS, cfg_FadeLength);
	uSetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS, cfg_FadeDelay);

	SendDlgItemMessage(IDC_THREAD_PRIORITY_SLIDER, TBM_SETRANGE, 1, MAKELONG(0, 6));
	SendDlgItemMessage(IDC_THREAD_PRIORITY_SLIDER, TBM_SETPOS, 1, cfg_Priority);

	uSetDlgItemText(m_hWnd, IDC_THREAD_PRIORITY_TEXT, priority_strings[cfg_Priority]);
	return TRUE;
}


t_uint32 vgmstreamPreferences::get_state()
{
	t_uint32 state = preferences_state::resettable;
	if (HasChanged()) state |= preferences_state::changed;
	return state;
}


void vgmstreamPreferences::reset()
{
	CheckDlgButton(IDC_LOOP_FOREVER, DEFAULT_LOOP_FOREVER?BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(IDC_IGNORE_LOOP, DEFAULT_IGNORE_LOOP?BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(IDC_LOOP_NORMALLY, (!DEFAULT_IGNORE_LOOP && !DEFAULT_LOOP_FOREVER)?BST_CHECKED:BST_UNCHECKED);

	uSetDlgItemText(m_hWnd, IDC_LOOP_COUNT, DEFAULT_LOOP_COUNT);
	uSetDlgItemText(m_hWnd, IDC_FADE_SECONDS, DEFAULT_FADE_SECONDS);
	uSetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS, DEFAULT_FADE_DELAY_SECONDS);

	SendDlgItemMessage(IDC_THREAD_PRIORITY_SLIDER, TBM_SETRANGE, 1, MAKELONG(0, 6));
	SendDlgItemMessage(IDC_THREAD_PRIORITY_SLIDER, TBM_SETPOS, 1, DEFAULT_THREAD_PRIORITY);

	uSetDlgItemText(m_hWnd, IDC_THREAD_PRIORITY_TEXT, priority_strings[DEFAULT_THREAD_PRIORITY]);
}



void vgmstreamPreferences::apply()
{
	cfg_LoopForever = IsDlgButtonChecked(IDC_LOOP_FOREVER)?true:false;
	cfg_IgnoreLoop = IsDlgButtonChecked(IDC_IGNORE_LOOP)?true:false;

	double temp_fade_seconds;
	double temp_fade_delay_seconds;
	double temp_loop_count;
	int consumed;

	pfc::string buf;
	buf = uGetDlgItemText(m_hWnd, IDC_FADE_SECONDS);
	if (sscanf(buf.get_ptr(),"%lf%n",&temp_fade_seconds,&consumed)<1
		|| consumed!=strlen(buf.get_ptr()) ||
		temp_fade_seconds<0) {
		uMessageBox(m_hWnd,
				"Invalid value for Fade Length\n"
				"Must be a number greater than or equal to zero",
				"Error",MB_OK|MB_ICONERROR);
		return;
	} else cfg_FadeLength = buf.get_ptr();

	buf = uGetDlgItemText(m_hWnd, IDC_LOOP_COUNT);
	if (sscanf(buf.get_ptr(),"%lf%n",&temp_loop_count,&consumed)<1
		|| consumed!=strlen(buf.get_ptr()) ||
		temp_loop_count<0) {
		uMessageBox(m_hWnd,
				"Invalid value for Loop Count\n"
				"Must be a number greater than or equal to zero",
				"Error",MB_OK|MB_ICONERROR);
		return;
	} else cfg_LoopCount = buf.get_ptr();

	buf = uGetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS);
	if (sscanf(buf.get_ptr(),"%lf%n",&temp_fade_delay_seconds,&consumed)<1
		|| consumed!=strlen(buf.get_ptr()) ||
		temp_fade_delay_seconds<0) {
		uMessageBox(m_hWnd,
				"Invalid value for Fade Delay\n"
				"Must be a number",
				"Error",MB_OK|MB_ICONERROR);
		return;
	} else cfg_FadeDelay = buf.get_ptr();

	cfg_Priority = (t_uint32)SendDlgItemMessage(IDC_THREAD_PRIORITY_SLIDER, TBM_GETPOS, 0, 0);
}


bool vgmstreamPreferences::HasChanged()
{
	if(IsDlgButtonChecked(IDC_LOOP_FOREVER))
		if(cfg_LoopForever != true) return true;

	if(IsDlgButtonChecked(IDC_IGNORE_LOOP))
		if(cfg_IgnoreLoop != true) return true;

	if(IsDlgButtonChecked(IDC_LOOP_NORMALLY))
		if(cfg_IgnoreLoop != false || cfg_LoopForever != false) return true;

	pfc::string FadeLength(cfg_FadeLength);
	pfc::string FadeDelay(cfg_FadeDelay);
	pfc::string LoopCount(cfg_LoopCount);

	if(FadeLength != uGetDlgItemText(m_hWnd, IDC_FADE_SECONDS)) return true;
	if(FadeDelay != uGetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS)) return true;
	if(LoopCount != uGetDlgItemText(m_hWnd, IDC_LOOP_COUNT)) return true;

	int Priority = 6 - SendDlgItemMessage(IDC_THREAD_PRIORITY_SLIDER, TBM_GETPOS, 0, 0);
	if(Priority != cfg_Priority) return true;
    return FALSE;
}


void vgmstreamPreferences::OnEditChange(UINT, int, CWindow)
{
	m_callback->on_state_changed();
}

void vgmstreamPreferences::OnVScroll(UINT nSBCode, UINT nPos, CTrackBarCtrl pScrollBar)
{
	int index = pScrollBar.GetPos();
	uSetDlgItemText(m_hWnd, IDC_THREAD_PRIORITY_TEXT, priority_strings[index]);
	m_callback->on_state_changed();
}


static preferences_page_factory_t<vgmstream_prefs> g_vgmstream_preferences_page_factory;
