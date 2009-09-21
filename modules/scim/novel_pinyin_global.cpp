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

#define Uses_STL_AUTOPTR
#define Uses_STL_FUNCTIONAL
#define Uses_STL_VECTOR
#define Uses_STL_IOSTREAM
#define Uses_STL_FSTREAM
#define Uses_STL_ALGORITHM
#define Uses_STL_MAP
#define Uses_STL_UTILITY
#define Uses_STL_IOMANIP
#define Uses_STL_STRING
#define Uses_C_STDIO
#define Uses_SCIM_UTILITY
#define Uses_SCIM_SERVER
#define Uses_SCIM_ICONV
#define Uses_SCIM_CONFIG_BASE
#define Uses_SCIM_CONFIG_PATH
#define Uses_SCIM_LOOKUP_TABLE

#include <errno.h>
#include <scim.h>
#include <glib.h>
#include "novel_pinyin_private.h"
#include "novel_types.h"
#include "pinyin_base.h"
#include "pinyin_phrase.h"
#include "pinyin_large_table.h"
#include "phrase_index.h"
#include "ngram.h"
#include "lookup.h"
#include "novel_pinyin_global.h"

using namespace novel;

// functions of PinyinGlobal

PinyinGlobal::PinyinGlobal()
    : m_custom(NULL),
      m_large_table(NULL),
      m_phrase_index(NULL),
      m_bigram(NULL),
      m_pinyin_lookup(NULL),
      m_validator(NULL)
{
    
    m_custom = new PinyinCustomSettings;
    m_validator = new BitmapPinyinValidator;
    m_large_table = new PinyinLargeTable(m_custom);
    m_bigram = new Bigram;
    m_phrase_index = new FacadePhraseIndex;
    m_pinyin_lookup = new PinyinLookup(m_custom, m_large_table, m_phrase_index, m_bigram);

    if (!m_custom || !m_validator || !m_large_table || 
	!m_bigram || !m_phrase_index || !m_pinyin_lookup){
	delete m_custom;
	delete m_validator;
	delete m_large_table;
	delete m_bigram;
	delete m_phrase_index;
	delete m_pinyin_lookup;
	exit(1);
    }
    
    toggle_tone(true);
    toggle_incomplete(false);
    toggle_dynamic_adjust(true);
    toggle_ambiguity(PINYIN_AmbAny, false);
    
    update_custom_settings();
}

PinyinGlobal::~PinyinGlobal (){
	if (m_custom) delete m_custom;
	if (m_validator) delete m_validator;
	if (m_large_table) delete m_large_table;
	if (m_bigram) delete m_bigram;
	if (m_phrase_index) delete m_phrase_index;
	if (m_pinyin_lookup) delete m_pinyin_lookup;  
}

bool
PinyinGlobal::use_tone () const
{
	return m_use_tone;
}

bool
PinyinGlobal::use_ambiguity (const PinyinAmbiguity &amb) const
{
	return m_custom->use_ambiguities [static_cast<int>(amb)];
}

bool
PinyinGlobal::use_incomplete () const
{
	return m_custom->use_incomplete;
}

bool
PinyinGlobal::use_dynamic_adjust () const
{
	return m_use_dynamic_adjust;
}

void
PinyinGlobal::toggle_tone (bool use)
{
	m_use_tone  = use;
}

void
PinyinGlobal::toggle_incomplete (bool use)
{
    m_custom->set_use_incomplete(use);
}

void
PinyinGlobal::toggle_dynamic_adjust (bool use)
{
    m_use_dynamic_adjust = use;
}

void
PinyinGlobal::toggle_ambiguity (const PinyinAmbiguity &amb, bool use)
{
    m_custom->set_use_ambiguities(amb, use);
}

void
PinyinGlobal::update_custom_settings ()
{
     m_validator->initialize (m_large_table);
}

bool PinyinGlobal::check_version(const char * user_dir){
    String version_file = String(user_dir) + String(SCIM_PATH_DELIM_STRING) +
	String(VERSION_FILE);
    MemoryChunk chunk;
    bool retval = chunk.load(version_file.c_str());
    if (!retval)
	return retval;
    if ( 0 != memcmp(FORMAT_VERSION, chunk.begin(),
		     strlen(FORMAT_VERSION) + 1) )
	return false;
    return true;
}

bool PinyinGlobal::clean_old_files(const char * user_dir, const char * filename){
    String fullpathname = String(user_dir) + String(SCIM_PATH_DELIM_STRING) +
	String(filename);
	int retval = unlink(fullpathname.c_str());
	if ( !retval )
		return true;
	if ( retval && ENOENT == errno )
		return true;
	return false;
}

bool PinyinGlobal::mark_version(const char * user_dir){
    String version_file = String(user_dir) + String(SCIM_PATH_DELIM_STRING) +
	String(VERSION_FILE);
    MemoryChunk chunk;
    chunk.set_content(0, FORMAT_VERSION, strlen(FORMAT_VERSION) + 1);
    bool retval = chunk.save(version_file.c_str());
    return retval;
}

bool PinyinGlobal::load_pinyin_table (const char * filename){
    String novel_path = String(NOVEL_PINYIN_DATADIR) + String(SCIM_PATH_DELIM_STRING) + String(filename);
    MemoryChunk * new_chunk = new MemoryChunk;
    bool retval = new_chunk->load(novel_path.c_str());
    if ( !retval ) 
	return retval;
    retval = m_large_table->load(new_chunk);
    if ( !retval )
	return retval;
    update_custom_settings ();
    return true;
}

bool PinyinGlobal::load_phrase_index(guint8 phrase_index, const char * filename){
    String home_dir (scim_get_home_dir());
    String user_data_directory = home_dir +
	String (SCIM_PATH_DELIM_STRING) +
	String (".scim") + 
	String (SCIM_PATH_DELIM_STRING) +
	String("novel-pinyin");
    String user_phrase_index = user_data_directory + 
	String (SCIM_PATH_DELIM_STRING) +
	String(filename);
    MemoryChunk * new_chunk = new MemoryChunk;
    if ( ! new_chunk->load(user_phrase_index.c_str()) ){
	fprintf(stdout, "user phrase index %d not found\n", phrase_index);
	delete new_chunk;
    }else if (m_phrase_index->load(phrase_index, new_chunk)){
	return true;
    }
    String novel_phrase_index = String(NOVEL_PINYIN_DATADIR) + String(SCIM_PATH_DELIM_STRING) + String(filename);
    new_chunk = new MemoryChunk;
    bool retval = new_chunk->load(novel_phrase_index.c_str());
    if ( !retval )
	return retval;
    return m_phrase_index->load(phrase_index, new_chunk);
}

bool PinyinGlobal::unload_phrase_index(guint8 phrase_index){
    return m_phrase_index->unload(phrase_index);
}

bool PinyinGlobal::save_phrase_index(guint8 phrase_index, const char * filename){
    String home_dir (scim_get_home_dir());
    String user_data_directory = home_dir +
	String (SCIM_PATH_DELIM_STRING) +
	String (".scim") + 
	String (SCIM_PATH_DELIM_STRING) +
	String("novel-pinyin");
    String user_phrase_index = user_data_directory + 
	String (SCIM_PATH_DELIM_STRING) +
	String(filename);
    String user_phrase_index_bak = user_phrase_index + String(".bak");
    MemoryChunk * new_chunk = new MemoryChunk;
    m_phrase_index->store(phrase_index, new_chunk);
    bool retval = new_chunk->save(user_phrase_index_bak.c_str());
    if ( !retval )
	return retval;
    rename(user_phrase_index_bak.c_str(), user_phrase_index.c_str());
    return true;
}

bool PinyinGlobal::attach_bigram(const char * system, const char * user){
    return m_bigram->attach(system, user);
}

