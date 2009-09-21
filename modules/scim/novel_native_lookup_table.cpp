#define Uses_STL_AUTOPTR
#define Uses_STL_FUNCTIONAL
#define Uses_STL_VECTOR
#define Uses_STL_IOSTREAM
#define Uses_STL_FSTREAM
#define Uses_STL_ALGORITHM
#define Uses_STL_MAP 
#define Uses_STL_UTILITY
#define Uses_STL_IOMANIP
#define Uses_C_STDIO
#define Uses_SCIM_UTILITY
#define Uses_SCIM_IMENGINE
#define Uses_SCIM_ICONV
#define Uses_SCIM_CONFIG_BASE
#define Uses_SCIM_CONFIG_PATH
#define Uses_SCIM_LOOKUP_TABLE

#include <scim.h>
#include "novel_pinyin_private.h"

#include "novel_types.h"
#include "phrase_index.h"
#include "novel_native_lookup_table.h"

using namespace novel;

NativeLookupTable::NativeLookupTable (int page_size)
	: LookupTable (page_size),
	m_phrase_index(0){
    std::vector <WideString> labels;
    char buf [2] = { 0, 0 };
    for (int i = 0; i < 9; ++i) {
        buf [0] = '1' + i;
        labels.push_back (utf8_mbstowcs (buf));
    }
    buf [0] = '0';
    labels.push_back (utf8_mbstowcs (buf));

    set_candidate_labels (labels);
}

WideString NativeLookupTable::get_candidate (int index) const{
    if ( index < m_strings.size ())
	return m_strings[index];
    phrase_token_t token = get_token(index);
    if ( 0 == token )
	return WideString ();
    PhraseItem item;
    if (!m_phrase_index)
	return WideString ();
    if (!m_phrase_index->get_phrase_item(token, item))
	return WideString ();
    utf16_t buffer[MAX_PHRASE_LENGTH];
    item.get_phrase_string(buffer);
    guint8 phrase_length = item.get_phrase_length();
    char *  utf8_string = 
	g_utf16_to_utf8(buffer, phrase_length, NULL, NULL, NULL);
    WideString string(utf8_mbstowcs(utf8_string));
    g_free(utf8_string);
    return string;
}

AttributeList NativeLookupTable::get_attributes (int index) const{
    AttributeList attrs;
    return attrs;

}
