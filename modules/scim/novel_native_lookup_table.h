/** @file novel_native_lookup_table.h
 * definition of NativeLookupTable classes.
 */

/*
 * Novel Pinyin Input Method
 * 
 * Copyright (c) 2008 Peng Wu <alexepico@gmail.com>
 *
 */


#ifndef __NOVEL_NATIVE_LOOKUP_TABLE_H
#define __NOVEL_NATIVE_LOOKUP_TABLE_H

using namespace scim;
using namespace std;

/**
 * lookup table class used by SCIM servers itself.
 */

class FacadePhraseIndex;

namespace novel{

class NativeLookupTable : public LookupTable{
    vector<WideString> m_strings;
    vector<phrase_token_t> m_phrases;
    FacadePhraseIndex * m_phrase_index;
    
    
public:
    NativeLookupTable (int page_size = 10);

    virtual void clear(){
	m_strings.clear();
	m_phrases.clear();
    }

    virtual uint32 number_of_candidates () const {
	return m_strings.size() + m_phrases.size();
    }

    virtual WideString get_candidate (int index) const;
    
    virtual AttributeList get_attributes (int index) const;

public:
    bool append_entry(phrase_token_t token){
	m_phrases.push_back(token);
	return true;
    }
    
    bool append_entry(WideString & string){
	m_strings.push_back(string);
	return true;
    }

    phrase_token_t get_token(int index) const{
	if ( index < 0 || index >= (int) number_of_candidates() )
	    return 0;
	if ( index < m_strings.size() )
	    return 0;
	return m_phrases[index - m_strings.size()];
    }

    bool set_phrase_index(FacadePhraseIndex * phrase_index){
	m_phrase_index = phrase_index;
	return true;
    }
};
};

#endif
