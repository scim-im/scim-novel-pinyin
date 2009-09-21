/** @file novel_pinyin_imengine.h
 * definition of Pinyin IMEngine related classes.
 */

/* 
 * Novel Pinyin Input Method
 * 
 * Copyright (c) 2005 James Su <suzhe@tsinghua.org.cn>
 * Copyright (c) 2008 Peng Wu <alexepico@gmail.com>
 */

#if !defined (__NOVEL_PINYIN_IMENGINE_H)
#define __NOVEL_PINYIN_IMENGINE_H

using namespace scim;

#define PINYIN_TABLE_FILENAME "pinyin_index.bin"
#define GB_PHRASE_FILENAME "gb_char.bin"
#define GBK_PHRASE_FILENAME "gbk_char.bin"
#define BIGRAM_FILENAME "bigram.db"
#define SPECIAL_TABLE_FILENAME "special_table"

namespace novel{

class PinyinFactory : public IMEngineFactoryBase
{
    PinyinGlobal m_pinyin_global;
    
    SpecialTable m_special_table;

    ConfigPointer m_config;

    WideString m_name;

    PinyinParser *m_pinyin_parser;
    PinyinLookup *m_pinyin_lookup;

    String m_user_data_directory;
    String m_system_bigram;
    String m_user_bigram;

    std::vector<KeyEvent> m_full_width_punct_keys;
    std::vector<KeyEvent> m_full_width_letter_keys;
    std::vector<KeyEvent> m_mode_switch_keys;
    std::vector<KeyEvent> m_chinese_switch_keys;
    std::vector<KeyEvent> m_page_up_keys;
    std::vector<KeyEvent> m_page_down_keys;

    bool m_auto_fill_preedit;
    bool m_always_show_lookup;
    bool m_show_all_keys;
    
    bool m_valid;
    
    time_t             m_last_time;
    time_t             m_save_period;

    bool m_shuang_pin;
    
    PinyinShuangPinScheme m_shuang_pin_scheme;

    Connection m_reload_signal_connection;
    
    friend class PinyinInstance;

public:
    PinyinFactory (const ConfigPointer &config);
    
    virtual ~PinyinFactory ();
    
    virtual WideString get_name() const;
    virtual WideString get_authors () const;
    virtual WideString get_credits () const;
    virtual WideString get_help () const;
    virtual String     get_uuid () const;
    virtual String     get_icon_file () const;

    virtual IMEngineInstancePointer create_instance (const String & encoding, 
						     int id = -1);
    void refresh ();
    
    bool valid () const { return m_valid;}

private:
    bool init ();
    
    void init_pinyin_parser ();
    
    void save_user_library ();

    void reload_config (const ConfigPointer &config);
};

struct PhraseItemWithFreq{
    phrase_token_t m_token;
    gfloat        m_freq;
};

typedef GArray * PhraseItemWithFreqArray; /*Array of PhraseItemWithFreq */

class PinyinInstance : public IMEngineInstanceBase
{
    PinyinFactory      *m_factory;
    PinyinGlobal       *m_pinyin_global;
    
    PinyinLargeTable   *m_large_table;
    FacadePhraseIndex  *m_phrase_index;
    
    bool                m_double_quotation_state;
    bool                m_single_quotation_state;
    
    bool                m_full_width_punctuation [2];
    bool                m_full_width_letter [2];

    bool                m_forward;
    bool                m_focused;
    
    int                 m_lookup_table_def_page_size;
    
    int                 m_keys_caret;
    int                 m_lookup_caret;
    
    String              m_client_encoding;
    
    String              m_inputed_string;
    
    WideString          m_converted_string;
    WideString          m_preedit_string;
    
    KeyEvent            m_prev_key;
    
    NativeLookupTable   m_lookup_table;
    PhraseItem          m_cache_phrase_item;

    PinyinKeyVector     m_parsed_keys;
    PinyinKeyPosVector  m_parsed_poses;
    PhraseItemWithFreqArray m_phrase_items_with_freq;

    

    std::vector < std::pair<int, int> >    m_keys_preedit_index;
    
    //use m_parsed_keys to validate m_constraints;
    CandidateConstraints m_constraints;
    MatchResults         m_results;

    Connection          m_reload_signal_connection;

public:
    PinyinInstance (PinyinFactory * factory,
		    PinyinGlobal * pinyin_global,
		    const String & encoding,
		    int id = -1);
    
    virtual ~PinyinInstance ();
    
    virtual bool process_key_event (const KeyEvent & key);
    virtual void move_preedit_caret (unsigned int pos);
    virtual void select_candidate (unsigned int item);
    virtual void update_lookup_table_page_size (unsigned int page_size);
    virtual void lookup_table_page_up ();
    virtual void lookup_table_page_down ();
    virtual void reset();
    virtual void focus_in();
    virtual void focus_out();
    virtual void trigger_property (const String & property);
    
private:
    void clear_constraints();
    void init_lookup_table_labels ();
    bool caret_left (bool home = false);
    bool caret_right (bool end = false);
    
    bool validate_insert_key(char key);
    
    bool insert(char key);
    bool erase(bool backspace = true);
    bool erase_by_key(bool backspace = true);
    
    bool space_hit ();
    bool enter_hit();
    bool lookup_cursor_up ();
    bool lookup_cursor_down();
    bool lookup_page_up();
    bool lookup_page_down();
    bool lookup_select(int index);
    void lookup_to_converted (int index);

    bool post_process (char key);
    
    void commit_converted ();
    
    void refresh_preedit_caret ();
    void refresh_preedit_string ();
    void refresh_lookup_table (bool calc =true);
    void refresh_aux_string ();
    
    void initialize_all_properties ();
    void refresh_all_properties ();
    void refresh_status_property ();
    void refresh_letter_property ();
    void refresh_punct_property ();
    void refresh_pinyin_scheme_property ();

    void calc_keys_preedit_index ();

    bool has_unparsed_chars ();
    int  calc_inputed_caret ();
    void calc_parsed_keys ();
    void calc_preedit_string ();
    void calc_lookup_table ();
    int  calc_preedit_caret (); 



    bool auto_fill_preedit (int invalid_pos = 0);

    int  inputed_caret_to_key_index (int caret);
    
    bool english_mode_process_key_event (const KeyEvent &key);
    
    void english_mode_refresh_preedit ();
    
    bool special_mode_process_key_event (const KeyEvent & key);
    
    void special_mode_refresh_preedit();
    
    void special_mode_refresh_lookup_table ();
    
    bool special_mode_lookup_select (int index);
    
    bool match_key_event (const std::vector <KeyEvent> & keyvec, const KeyEvent  & key);
    
    bool is_english_mode () const;
    
    bool is_special_mode () const;
    
    WideString convert_to_full_width (char key);
    
    void reload_config (const ConfigPointer & config);
};
};

#endif
