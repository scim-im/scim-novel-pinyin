/** @file pinyin_global.h
 *  definition of PinyinGlobal class
 */

/* 
 *  novel-pinyin,
 *  A Simplified Chinese Sentence-Based Pinyin Input Method Engine
 *  Based On Markov Model.
 *  
 *  Copyright (C) 2006-2007 Peng Wu
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 * the purpose of class PinyinGlobal is to store
 * global configuration and data of Pinyin input Method.
 *
 */

#if !defined (__NOVEL_PINYIN_GLOBAL_H)
#define __NOVEL_PINYIN_GLOBAL_H

struct PinyinCustomSettings;
class PinyinLargeTable;
class FacadePhraseIndex;
class Bigram;
class PinyinLookup;
class PinyinDefaultParser;
class BitmapPinyinValidator;

using namespace scim;
using namespace std;

#define VERSION_FILE "version"
#define FORMAT_VERSION "0.2.3"

namespace novel{

class PinyinGlobal
{
    PinyinCustomSettings * m_custom;
    PinyinLargeTable * m_large_table;
    FacadePhraseIndex * m_phrase_index;
    Bigram * m_bigram;
    
    PinyinLookup * m_pinyin_lookup;

    BitmapPinyinValidator * m_validator;

    bool m_use_tone;
    bool m_use_dynamic_adjust;
    
public:
    PinyinGlobal ();
    
    ~PinyinGlobal ();
    
    bool use_tone () const;
    bool use_ambiguity (const PinyinAmbiguity & amb) const;
    bool use_incomplete () const;
    bool use_dynamic_adjust () const;

    void toggle_tone (bool use = false);
    void toggle_incomplete (bool use = false);
    void toggle_dynamic_adjust (bool use = true);
    void toggle_ambiguity (const PinyinAmbiguity & amb, bool use = false);

    bool check_version(const char * user_dir);
    bool clean_old_files(const char * user_dir, const char * filename);
    bool mark_version(const char * user_dir);

    bool load_pinyin_table (const char * filename);
    
    bool load_phrase_index(guint8 phrase_index, const char * filename);
    bool unload_phrase_index(guint8 phrase_index);
    bool save_phrase_index(guint8 phrase_index, const char * filename);
    
    bool attach_bigram(const char * system, const char * user);
 
    PinyinCustomSettings * get_custom_setting(){
	return m_custom;
    }

    PinyinLargeTable * get_pinyin_table(){
	return m_large_table;
    }
   
    FacadePhraseIndex * get_phrase_index(){
	return m_phrase_index;
    }

    Bigram * get_bigram(){
	return m_bigram;
    }

    PinyinLookup * get_pinyin_lookup () {
	return m_pinyin_lookup;
    }

    const BitmapPinyinValidator * get_pinyin_validator(){
	 return m_validator;
    }

    void update_custom_settings();
};
};
#endif
