#pragma once

#ifndef _FOO_PREFS_H
#define _FOO_PREFS_H

#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/atl-misc.h>
#include "resource.h"

#define DEFAULT_FADE_SECONDS "10.00"
#define DEFAULT_FADE_DELAY_SECONDS "0.00"
#define DEFAULT_LOOP_COUNT "2.00"
#define DEFAULT_LOOP_FOREVER false
#define DEFAULT_IGNORE_LOOP false
#define DEFAULT_DISABLE_SUBSONGS false
#define DEFAULT_DOWNMIX_CHANNELS "8"
#define DEFAULT_TAGFILE_DISABLE false
#define DEFAULT_OVERRIDE_TITLE false
#define DEFAULT_EXTS_UNKNOWN_ON false
#define DEFAULT_EXTS_COMMON_ON false

class vgmstreamPreferences : public CDialogImpl<vgmstreamPreferences>, public preferences_page_instance {
public:
	//Constructor - invoked by preferences_page_impl helpers -
    //don't do Create() in here, preferences_page_impl does this for us
	vgmstreamPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

	//Note that we don't bother doing anything regarding destruction of our class.
	//The host ensures that our dialog is destroyed first, then the last reference to our
	//preferences_page_instance object is released, causing our object to be deleted.


	//dialog resource ID
	enum {IDD = IDD_CONFIG};
	// preferences_page_instance methods (not all of them - get_wnd() is supplied by preferences_page_impl helpers)
	t_uint32 get_state();
	void apply();
	void reset();

	//WTL message map
	BEGIN_MSG_MAP(UsfPreferences)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_IGNORE_LOOP, BN_CLICKED, OnEditChange)
		COMMAND_HANDLER_EX(IDC_LOOP_FOREVER, BN_CLICKED, OnEditChange)
		COMMAND_HANDLER_EX(IDC_LOOP_NORMALLY, BN_CLICKED, OnEditChange)
		COMMAND_HANDLER_EX(IDC_FADE_SECONDS, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_FADE_DELAY_SECONDS, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_LOOP_COUNT, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_DISABLE_SUBSONGS, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_DOWNMIX_CHANNELS, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_TAGFILE_DISABLE, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_OVERRIDE_TITLE, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_EXTS_UNKNOWN_ON, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_EXTS_COMMON_ON, BN_CLICKED, OnEditChange)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM);
	void OnEditChange(UINT, int, CWindow);
	bool HasChanged();
	void OnChanged();

	const preferences_page_callback::ptr m_callback;
};



class vgmstream_prefs : public preferences_page_impl<vgmstreamPreferences> {

public:
	const char * get_name();
	GUID get_guid();
	GUID get_parent_guid();

};



#endif
