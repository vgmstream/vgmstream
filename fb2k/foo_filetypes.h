#ifndef _FOO_FILETYPES_H_
#define _FOO_FILETYPES_H_

class input_file_type_v2_impl_vgmstream : public input_file_type_v2 {
public:
	input_file_type_v2_impl_vgmstream() {
		ext_list = libvgmstream_get_extensions(&ext_list_len);
	}

	unsigned get_count() { return ext_list_len; }

	bool is_associatable(unsigned idx) { return true; }

	void get_format_name(unsigned idx, pfc::string_base & out, bool isPlural) {
		out.reset();
		pfc::stringToUpperAppend(out, ext_list[idx], pfc::strlen_utf8(ext_list[idx]));
		out += " Audio File";
		if (isPlural) out += "s";
	}

	void get_extensions(unsigned idx, pfc::string_base & out) {
		out = ext_list[idx];
	}

private:
	const char** ext_list;
	int ext_list_len;
};

namespace { static service_factory_single_t<input_file_type_v2_impl_vgmstream> g_filetypes; }

#endif /*_FOO_FILETYPES_H_ */
