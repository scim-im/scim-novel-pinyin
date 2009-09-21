/* 
 * Novel Pinyin Input Method
 * 
 * Copyright (c) 2005 James Su <suzhe@tsinghua.org.cn>
 * Copyright (c) 2008 Peng Wu <alexepico@gmail.com>
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
#define Uses_SCIM_IMENGINE
#define Uses_SCIM_CONFIG_BASE
#define Uses_SCIM_CONFIG_PATH
#define Uses_SCIM_LOOKUP_TABLE

#include <sys/stat.h>
#include <sys/types.h>
#include <scim.h>
#include <glib.h>
#include <config.h>
#include "novel_pinyin_private.h"
#include "novel_types.h"
#include "pinyin_base.h"
#include "pinyin_phrase.h"
#include "pinyin_large_table.h"
#include "phrase_index.h"
#include "ngram.h"
#include "lookup.h"
#include "novel_pinyin_global.h"
#include "novel_special_table.h"
#include "novel_native_lookup_table.h"
#include "novel_pinyin_imengine_config_keys.h"
#include "novel_pinyin_imengine.h"

using namespace novel;

#define scim_module_init novel_pinyin_LTX_scim_module_init
#define scim_module_exit novel_pinyin_LTX_scim_module_exit
#define scim_imengine_module_init novel_pinyin_LTX_scim_imengine_module_init
#define scim_imengine_module_create_factory novel_pinyin_LTX_scim_imengine_module_create_factory


#define NOVEL_PROP_STATUS                            "/IMEngine/Pinyin/Status"
#define NOVEL_PROP_LETTER                            "/IMEngine/Pinyin/Letter"
#define NOVEL_PROP_PUNCT                             "/IMEngine/Pinyin/Punct"

#define NOVEL_PROP_PINYIN_SCHEME                     "/IMEngine/Pinyin/PinyinScheme"
#define NOVEL_PROP_PINYIN_SCHEME_QUAN_PIN            "/IMEngine/Pinyin/PinyinScheme/QuanPin"
#define NOVEL_PROP_PINYIN_SCHEME_SP_STONE            "/IMEngine/Pinyin/PinyinScheme/SP-STONE"
#define NOVEL_PROP_PINYIN_SCHEME_SP_ZRM              "/IMEngine/Pinyin/PinyinScheme/SP-ZRM"
#define NOVEL_PROP_PINYIN_SCHEME_SP_MS               "/IMEngine/Pinyin/PinyinScheme/SP-MS"
#define NOVEL_PROP_PINYIN_SCHEME_SP_ZIGUANG          "/IMEngine/Pinyin/PinyinScheme/SP-ZIGUANG"
#define NOVEL_PROP_PINYIN_SCHEME_SP_ABC              "/IMEngine/Pinyin/PinyinScheme/SP-ABC"
#define NOVEL_PROP_PINYIN_SCHEME_SP_LIUSHI           "/IMEngine/Pinyin/PinyinScheme/SP-LIUSHI"

#ifndef NOVEL_PINYIN_DATADIR
    #define NOVEL_PINYIN_DATADIR "/usr/share/scim/novel-pinyin"
#endif

#ifndef SCIM_ICONDIR
    #define SCIM_ICONDIR "/usr/share/scim/icons"
#endif

#ifndef NOVEL_PINYIN_ICON_FILE
    #define NOVEL_PINYIN_ICON_FILE  (SCIM_ICONDIR "/novel-pinyin.png")
#endif

#define NOVEL_FULL_LETTER_ICON                             (SCIM_ICONDIR "/full-letter.png")
#define NOVEL_HALF_LETTER_ICON                             (SCIM_ICONDIR "/half-letter.png")
#define NOVEL_FULL_PUNCT_ICON                              (SCIM_ICONDIR "/full-punct.png")
#define NOVEL_HALF_PUNCT_ICON                              (SCIM_ICONDIR "/half-punct.png")

static IMEngineFactoryPointer _scim_pinyin_factory (0);

static ConfigPointer _scim_config (0);

static Property _status_property (NOVEL_PROP_STATUS, "");
static Property _letter_property (NOVEL_PROP_LETTER, "");
static Property _punct_property  (NOVEL_PROP_PUNCT, "");

static Property _pinyin_scheme_property     (NOVEL_PROP_PINYIN_SCHEME, "全");
static Property _pinyin_quan_pin_property   (NOVEL_PROP_PINYIN_SCHEME_QUAN_PIN,   "全拼");
static Property _pinyin_sp_stone_property   (NOVEL_PROP_PINYIN_SCHEME_SP_STONE,   "双拼-中文之星/四通利方");
static Property _pinyin_sp_zrm_property     (NOVEL_PROP_PINYIN_SCHEME_SP_ZRM,     "双拼-自然码");
static Property _pinyin_sp_ms_property      (NOVEL_PROP_PINYIN_SCHEME_SP_MS,      "双拼-微软拼音");
static Property _pinyin_sp_ziguang_property (NOVEL_PROP_PINYIN_SCHEME_SP_ZIGUANG, "双拼-紫光拼音");
static Property _pinyin_sp_abc_property     (NOVEL_PROP_PINYIN_SCHEME_SP_ABC,     "双拼-智能ABC");
static Property _pinyin_sp_liushi_property  (NOVEL_PROP_PINYIN_SCHEME_SP_LIUSHI,  "双拼-刘氏");

extern "C" {
    void scim_module_init (void)
    {
        bindtextdomain (GETTEXT_PACKAGE, NOVEL_PINYIN_LOCALEDIR);
        bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    }

    void scim_module_exit (void)
    {
        _scim_pinyin_factory.reset ();
        _scim_config.reset ();
    }

    uint32 scim_imengine_module_init (const ConfigPointer &config)
    {
        _status_property.set_tip (_("Current input method state. Click to change it."));
        _letter_property.set_tip (_("Input mode of the letters. Click to toggle between half and full."));
        _letter_property.set_label (_("Full/Half Letter"));
        _punct_property.set_tip (_("Input mode of the puncutations. Click to toggle between half and full."));
        _punct_property.set_label (_("Full/Half Punct"));

        _status_property.set_label ("英");
        _letter_property.set_icon (NOVEL_HALF_LETTER_ICON);
        _punct_property.set_icon (NOVEL_HALF_PUNCT_ICON);

        _scim_config = config;
        return 1;
    }

    IMEngineFactoryPointer scim_imengine_module_create_factory (uint32 engine)
    {
        if (engine != 0) return IMEngineFactoryPointer (0);
        if (_scim_pinyin_factory.null ()) {
            PinyinFactory *factory = new PinyinFactory (_scim_config); 
            if (factory && factory->valid ())
                _scim_pinyin_factory = factory;
            else
                delete factory;
        }
        return _scim_pinyin_factory;
    }
}

// implementation of PinyinFactory
PinyinFactory::PinyinFactory (const ConfigPointer &config)
    : m_config (config),
      m_pinyin_parser (0),
      m_pinyin_lookup (0),
      m_auto_fill_preedit(true),
      m_always_show_lookup(true),
      m_show_all_keys (false),
      m_valid(false),
      m_shuang_pin(false),
      m_shuang_pin_scheme (SHUANG_PIN_STONE),
      m_last_time ((time_t)0),
      m_save_period ((time_t)300)
{
    set_languages ("zh_CN,zh_TW,zh_HK,zh_SG");

    m_valid = init ();
    
    m_reload_signal_connection = m_config->signal_connect_reload(slot(this, &PinyinFactory::reload_config));
}

bool PinyinFactory::init (){
    String sys_bigram (String(NOVEL_PINYIN_DATADIR) +
		       String(SCIM_PATH_DELIM_STRING) +
		       String(BIGRAM_FILENAME));

    String sys_special_table        (String (NOVEL_PINYIN_DATADIR) +
                                     String (SCIM_PATH_DELIM_STRING) +
                                     String (SPECIAL_TABLE_FILENAME));

    String home_dir    (scim_get_home_dir());

    String str;
    String user_dir (home_dir + 
		     String(SCIM_PATH_DELIM_STRING) +
		     String(".scim") +
		     String(SCIM_PATH_DELIM_STRING) +
		     String("novel-pinyin"));
    
    mkdir(user_dir.c_str(), 0700);
    
    if (!m_pinyin_global.check_version(user_dir.c_str())){
	fprintf(stderr, "data file format updated, clean old files.\n");
	if (!m_pinyin_global.clean_old_files(user_dir.c_str(), GB_PHRASE_FILENAME) ){
	    fprintf(stderr, "can't clean old data file - %s.\n", GB_PHRASE_FILENAME);
	    exit(1);
	}
	if (!m_pinyin_global.clean_old_files(user_dir.c_str(), GBK_PHRASE_FILENAME) ){
	    fprintf(stderr, "can't clean old data file - %s.\n", GBK_PHRASE_FILENAME);
	    exit(1);
	}
	if (!m_pinyin_global.clean_old_files(user_dir.c_str(), BIGRAM_FILENAME) ){
	    fprintf(stderr,  "can't clean old data file - %s.\n", BIGRAM_FILENAME);
	    exit(1);
	}
	if (!m_pinyin_global.mark_version(user_dir.c_str()) ){
	    fprintf(stderr, "can't save version file.\n");
	    exit(1);
	}
    }

    String user_bigram (user_dir + 
			String(SCIM_PATH_DELIM_STRING) +
			String(BIGRAM_FILENAME));
    
    bool retval = m_pinyin_global.load_pinyin_table(PINYIN_TABLE_FILENAME);
    if ( !retval ) {
	fprintf(stderr, "can't load pinyin table %s.\n", PINYIN_TABLE_FILENAME);
	fprintf(stderr, "exit!\n");
	exit(1);
    }
    retval = m_pinyin_global.load_phrase_index(1, GB_PHRASE_FILENAME);
    if ( !retval ) {
	fprintf(stderr, "can't load phrase index %s.\n", GB_PHRASE_FILENAME);
	fprintf(stderr, "exit!\n");
	exit(1);
    }
    retval = m_pinyin_global.load_phrase_index(2, GBK_PHRASE_FILENAME);
    if ( !retval ) {
	fprintf(stderr, "can't load phrase index %s.\n", GBK_PHRASE_FILENAME);
	fprintf(stderr, "exit!\n");
	exit(1);
    }
    retval = m_pinyin_global.attach_bigram(sys_bigram.c_str(), user_bigram.c_str());
    if ( !retval ) {
	fprintf(stderr, "can't load bigram db %s and %s.\n", sys_bigram.c_str(), user_bigram.c_str());
	fprintf(stderr, "exit!\n");
	exit(1);
    }
    bool tone                = false;
    bool dynamic_adjust        = true;
    bool incomplete            = true;

    bool ambiguities [PINYIN_AmbLast + 2] =
        { false,  false,  false,  false,
          false,  false,  false,  false,
          false,  false,  false };

    const char *amb_names [PINYIN_AmbLast + 2] =
        { "Any",  "ZhiZi","ChiCi","ShiSi",
          "NeLe", "LeRi", "FoHe", "AnAng",
          "EnEng","InIng", 0};
    
    if (m_config){
        // Load configurations.
        tone =
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_TONE),
                            false);
        incomplete =
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_INCOMPLETE),
                            true);
        dynamic_adjust =
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_DYNAMIC_ADJUST),
                            true);

        m_auto_fill_preedit =
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_AUTO_FILL_PREEDIT),
                            true);

        m_always_show_lookup =
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_ALWAYS_SHOW_LOOKUP),
                            true);
        m_show_all_keys =
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_SHOW_ALL_KEYS),
                            false);


        m_save_period = (time_t)
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_SAVE_PERIOD),
                            300);
	

        // Read ambiguities config.
        for (int i = 0; i <= PINYIN_AmbLast; i++) {
            String amb = String (NOVEL_CONFIG_IMENGINE_PINYIN_AMBIGUITY) + 
                             String ("/") +
                             String (amb_names [i]);

            ambiguities [i] = m_config->read (amb, ambiguities [i]);
        }


        // Read and store hotkeys config.

        //Read full width punctuation keys
        str = m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_FULL_WIDTH_PUNCT_KEY),
                              String ("Control+period"));

        scim_string_to_key_list (m_full_width_punct_keys, str);

        //Read full width letter keys
        str = m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_FULL_WIDTH_LETTER_KEY),
                              String ("Shift+space"));

        scim_string_to_key_list (m_full_width_letter_keys, str);

        //Read mode switch keys
        str = m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_MODE_SWITCH_KEY),
                              String ("Alt+Shift_L+KeyRelease,") + 
                              String ("Alt+Shift_R+KeyRelease,") +
                              String ("Shift+Shift_L+KeyRelease,") +
                              String ("Shift+Shift_R+KeyRelease"));
        
        scim_string_to_key_list (m_mode_switch_keys, str);

        //Read chinese switch keys
        str = m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_CHINESE_SWITCH_KEY),
                              String ("Control+slash"));
        
        scim_string_to_key_list (m_chinese_switch_keys, str);

        //Read page up keys
        str = m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_PAGE_UP_KEY),
                              String ("comma,minus,bracketleft,Page_Up"));
        
        scim_string_to_key_list (m_page_up_keys, str);

        //Read page down keys
        str = m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_PAGE_DOWN_KEY),
                              String ("period,equal,bracketright,Page_Down"));
        
        scim_string_to_key_list (m_page_down_keys, str);

        // Load Pinyin Scheme config
        m_shuang_pin =
            m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_SHUANG_PIN),
                            m_shuang_pin);

        if (m_shuang_pin) {
            int tmp;
            tmp = m_config->read (String (NOVEL_CONFIG_IMENGINE_PINYIN_SHUANG_PIN_SCHEME),
                                  (int) m_shuang_pin_scheme);

            if (tmp >=0 && tmp <= SHUANG_PIN_LIUSHI)
                m_shuang_pin_scheme = static_cast<PinyinShuangPinScheme> (tmp);
        }
    }

    if (m_save_period > 0 && m_save_period < 60)
        m_save_period = 60;

    m_name = utf8_mbstowcs (_("Novel Pinyin"));

    m_pinyin_global.toggle_tone (tone);
    m_pinyin_global.toggle_incomplete (incomplete);
    m_pinyin_global.toggle_dynamic_adjust (dynamic_adjust);

    // Is there any ambiguities?
    if (ambiguities [0]) {
        for (int i=1; i <= PINYIN_AmbLast; i++) 
            m_pinyin_global.toggle_ambiguity (static_cast<PinyinAmbiguity> (i), ambiguities [i]);
    } else {
        m_pinyin_global.toggle_ambiguity (PINYIN_AmbAny, false);
    }

    m_pinyin_global.update_custom_settings ();

    if (m_full_width_punct_keys.size () == 0)
        m_full_width_punct_keys.push_back (KeyEvent (SCIM_KEY_comma, SCIM_KEY_ControlMask));

    if (m_full_width_letter_keys.size () == 0)
        m_full_width_letter_keys.push_back (KeyEvent (SCIM_KEY_space, SCIM_KEY_ShiftMask));

    if (m_mode_switch_keys.size () == 0) {
        m_mode_switch_keys.push_back (KeyEvent (SCIM_KEY_Shift_L, SCIM_KEY_AltMask | SCIM_KEY_ReleaseMask));
        m_mode_switch_keys.push_back (KeyEvent (SCIM_KEY_Shift_R, SCIM_KEY_AltMask | SCIM_KEY_ReleaseMask));
        m_mode_switch_keys.push_back (KeyEvent (SCIM_KEY_Shift_L, SCIM_KEY_ShiftMask | SCIM_KEY_ReleaseMask));
        m_mode_switch_keys.push_back (KeyEvent (SCIM_KEY_Shift_R, SCIM_KEY_ShiftMask | SCIM_KEY_ReleaseMask));
    }

    if (m_chinese_switch_keys.size () == 0)
        m_chinese_switch_keys.push_back (KeyEvent (SCIM_KEY_slash, SCIM_KEY_ControlMask));

    if (m_page_up_keys.size () == 0) {
        m_page_up_keys.push_back (KeyEvent (SCIM_KEY_comma, 0));
        m_page_up_keys.push_back (KeyEvent (SCIM_KEY_minus, 0));
        m_page_up_keys.push_back (KeyEvent (SCIM_KEY_bracketleft, 0));
        m_page_up_keys.push_back (KeyEvent (SCIM_KEY_Page_Up, 0));
    }

    if (m_page_down_keys.size () == 0) {
        m_page_down_keys.push_back (KeyEvent (SCIM_KEY_period, 0));
        m_page_down_keys.push_back (KeyEvent (SCIM_KEY_equal, 0));
        m_page_down_keys.push_back (KeyEvent (SCIM_KEY_bracketright, 0));
        m_page_down_keys.push_back (KeyEvent (SCIM_KEY_Page_Down, 0));
    }

    ifstream is_sys_special_table (sys_special_table.c_str ());

    if (is_sys_special_table)
        m_special_table.load (is_sys_special_table);

    m_pinyin_lookup = m_pinyin_global.get_pinyin_lookup();
    init_pinyin_parser ();

    return true;

}

void
PinyinFactory::init_pinyin_parser ()
{
    if (m_pinyin_parser)
        delete m_pinyin_parser;

    if (m_shuang_pin)
        m_pinyin_parser = new PinyinShuangPinParser (m_shuang_pin_scheme);
    else
        m_pinyin_parser = new PinyinDefaultParser ();
}

PinyinFactory::~PinyinFactory ()
{
    if (m_valid)
        save_user_library ();

    m_reload_signal_connection.disconnect ();
}

WideString
PinyinFactory::get_name () const
{
    return m_name;
}

WideString
PinyinFactory::get_authors () const
{
    return utf8_mbstowcs (
                String (_("Copyright (C) 2002, 2003 James Su <suzhe@tsinghua.org.cn>\n"
			  "Copyright (C) 2006-2008 Peng Wu <alexepico@gmail.com>")));
}

WideString
PinyinFactory::get_credits () const
{
    return WideString ();
}

WideString
PinyinFactory::get_help () const
{
    String full_width_letter;
    String full_width_punct;
    String chinese_switch;
    String mode_switch;
    String page_up;
    String page_down;
    String help;

    scim_key_list_to_string (full_width_letter, m_full_width_letter_keys);
    scim_key_list_to_string (full_width_punct, m_full_width_punct_keys);
    scim_key_list_to_string (chinese_switch, m_chinese_switch_keys);
    scim_key_list_to_string (mode_switch, m_mode_switch_keys);
    scim_key_list_to_string (page_up, m_page_up_keys);
    scim_key_list_to_string (page_down, m_page_down_keys);

    help =  String (_("Hot Keys:")) + 
            String (_("\n\n  ")) + full_width_letter + String (_(":\n")) +
            String (_("    Switch between full/half width letter mode.")) + 
            String (_("\n\n  ")) + full_width_punct + String (_(":\n")) +
            String (_("    Switch between full/half width punctuation mode.")) +
            String (_("\n\n  ")) + chinese_switch + String (_(":\n")) +
            String (_("    Switch between Simplified/Traditional Chinese mode.")) +
            String (_("\n\n  ")) + mode_switch + String (_(":\n")) +
            String (_("    Switch between English/Chinese mode.")) +
            String (_("\n\n  ")) + page_up + String (_(":\n")) +
            String (_("    Page up in lookup table.")) +
            String (_("\n\n  ")) + page_down + String (_(":\n")) +
            String (_("    Page down in lookup table.")) +
            String (_("\n\n  Esc:\n"
                      "    Reset the input method.\n")) +
            String (_("\n\n  v:\n"
                      "    Enter the English input mode.\n"
                      "    Press Space or Return to commit\n"
                      "    the inputed string and exit this mode.")) +
            String (_("\n\n  i:\n"
                      "    Enter the special input mode. For example:\n"
                      "      Input \"idate\" will give you the\n"
                      "      string of the current date.\n"
                      "      Input \"imath\" will give you the\n"
                      "      common mathematic symbols.\n"
                      "    For more information about this mode,\n"
                      "    please refer to\n"
                      "    /usr/share/scim/pinyin/special_table"));

    return utf8_mbstowcs (help);
}

String
PinyinFactory::get_uuid () const
{
    return String ("84d004b5-71f9-4038-92e0-39fd9975bb54");
}

String
PinyinFactory::get_icon_file () const
{
    return String (NOVEL_PINYIN_ICON_FILE);
}

IMEngineInstancePointer
PinyinFactory::create_instance (const String& encoding, int id)
{
    return new PinyinInstance (this, &m_pinyin_global, encoding, id);
}

void
PinyinFactory::refresh ()
{
    if (m_save_period == 0)
        return;

    time_t cur_time = time (0);

    if (cur_time < m_last_time || cur_time - m_last_time > m_save_period) {
        m_last_time = cur_time;
        save_user_library ();
    }
}

void
PinyinFactory::save_user_library ()
{
    m_pinyin_global.save_phrase_index(1, GB_PHRASE_FILENAME);
    m_pinyin_global.save_phrase_index(2, GBK_PHRASE_FILENAME);
}

void
PinyinFactory::reload_config (const ConfigPointer &config)
{
    m_config = config;
    m_valid = init ();
}

//implementation of PinyinInstance
PinyinInstance::PinyinInstance (PinyinFactory * factory,
				PinyinGlobal * pinyin_global,
				const String &encoding,
				int id)
    :IMEngineInstanceBase (factory, encoding, id),
     m_factory(factory),
     m_pinyin_global(pinyin_global),
     m_large_table(0),
     m_phrase_index(0),
     m_double_quotation_state(false),
     m_single_quotation_state(false),
     m_forward (false),
     m_focused (false),
     m_lookup_table_def_page_size(9),
     m_keys_caret(0),
     m_parsed_keys(0),
     m_parsed_poses(0),
     m_constraints(0),
     m_results(0)
{
    m_full_width_punctuation [0] = true;
    m_full_width_punctuation [1] = false;
    
    m_full_width_letter [0] = false;
    m_full_width_letter [1] = false;
    
    if ( m_factory->valid() && m_pinyin_global){
	m_large_table = m_pinyin_global->get_pinyin_table();
	m_phrase_index = m_pinyin_global->get_phrase_index();
	m_lookup_table.set_phrase_index(m_phrase_index);
    }

    m_parsed_keys = g_array_new (FALSE, FALSE, sizeof(PinyinKey));
    m_parsed_poses = g_array_new (FALSE, FALSE, sizeof(PinyinKeyPos));
    m_phrase_items_with_freq = g_array_new(FALSE, FALSE, sizeof(PhraseItemWithFreq));
    
    m_constraints = g_array_new (FALSE, FALSE, sizeof(lookup_constraint_t));

    m_results = g_array_new (FALSE, FALSE, sizeof(phrase_token_t));

    m_reload_signal_connection = m_factory->m_config->signal_connect_reload (slot (this, &PinyinInstance::reload_config));
    
    init_lookup_table_labels ();
}

PinyinInstance::~PinyinInstance ()
{
    g_array_free(m_parsed_keys, TRUE);
    g_array_free(m_parsed_poses, TRUE);
    g_array_free(m_phrase_items_with_freq, TRUE);
    g_array_free(m_constraints, TRUE);
    g_array_free(m_results, TRUE);
    
    m_reload_signal_connection.disconnect ();
}

bool
PinyinInstance::process_key_event (const KeyEvent & key)
{
    if ( !m_focused || !m_large_table || !m_phrase_index)
	return false;
    
    //capture the mode switch key events
    if (match_key_event (m_factory->m_mode_switch_keys, key)){
	m_forward = !m_forward;
	refresh_all_properties();
	reset();
	m_prev_key = key;
	return true;
    }
    
    //toggle full width punctuation mode
    if ( match_key_event (m_factory->m_full_width_punct_keys, key)){
	trigger_property(NOVEL_PROP_PUNCT);
	m_prev_key = key;
	return true;
    }
    
    //toggle full width letter mode
    if ( match_key_event (m_factory->m_full_width_letter_keys, key)){
	trigger_property(NOVEL_PROP_LETTER);
	m_prev_key = key;
	return true;
    }
    
    //capture the chinese switch key events
    if (match_key_event (m_factory->m_chinese_switch_keys, key)){
	trigger_property (NOVEL_PROP_STATUS);
	m_prev_key = key;
	return true;
    }
    
    m_prev_key = key;
    
    if (key.is_key_release ())
	return true;
    
    if (!m_forward) {
	//reset key
	if (key.code == SCIM_KEY_Escape && key.mask == 0){
	    if (m_inputed_string.length () == 0 &&
		m_converted_string.length () == 0 &&
		m_preedit_string.length () == 0)
		return false;
	    
	    reset();
	    return true;
	}
	
	if (!m_factory->m_shuang_pin){
	    //go into english mode
	    if ((!m_inputed_string.length () && key.code == SCIM_KEY_v &&
		 key.mask == 0) ||is_english_mode()){
		return english_mode_process_key_event (key);
	    }
	    
	    //go into special mode
	    if ((!m_inputed_string.length() && 
		 key.code == SCIM_KEY_i && key.mask == 0 &&
		 m_factory->m_special_table.valid()) ||
		is_special_mode ()) {
		return special_mode_process_key_event (key);
	    }
	}
	
	//caret left
	if (key.code == SCIM_KEY_Left && key.mask == 0)
	    return caret_left();
	
	//caret right
	if (key.code == SCIM_KEY_Right && key.mask == 0)
	    return caret_right();
	
	//caret home
	if (key.code == SCIM_KEY_Home && key.mask == 0)
	    return caret_left(true);

	//caret end
	if (key.code == SCIM_KEY_End && key.mask == 0)
	    return caret_right(true);
	
	//lookup table cursor up
	if ( key.code == SCIM_KEY_Up && key.mask == 0)
	    return lookup_cursor_up ();
	
	//lookup table cursor down
	if ( key.code == SCIM_KEY_Down && key.mask == 0)
	    return lookup_cursor_down ();
	
	//lookup table page up
	if ( match_key_event (m_factory->m_page_up_keys, key)){
	    if ( lookup_page_up ()) return true;
	    return post_process(key.get_ascii_code());
	}
	
	//lookup table page down
	if ( match_key_event (m_factory->m_page_down_keys, key)){
	    if ( lookup_page_down ()) return true;
	    return post_process (key.get_ascii_code());
	}
	
	//backspace key
	if ( key.code == SCIM_KEY_BackSpace) {
	    if ( key.mask == SCIM_KEY_ShiftMask )
		return erase_by_key();
	    else if (key.mask == 0)
		return erase();
	}

	//delete key
	if( key.code == SCIM_KEY_Delete){
	    if (key.mask == SCIM_KEY_ShiftMask)
		return erase_by_key(false);
	    else if (key.mask == 0)
		return erase(false);
	}

	//select lookup table
	if (m_pinyin_global->use_tone ()){
	    if ((( key.code >= SCIM_KEY_6 && key.code <= SCIM_KEY_9) ||
		 key.code == SCIM_KEY_0) && key.mask == 0) {
		int index;
		if ( key.code == SCIM_KEY_0) index = 4;
		else index = key.code - SCIM_KEY_6;
		if ( lookup_select(index)) return true;
	    }
	}else {
	    if (key.code >= SCIM_KEY_1 && key.code <= SCIM_KEY_9 
		&& key.mask == 0){
		int index = key.code - SCIM_KEY_1;
		if ( lookup_select(index)) return true;
	    }
	}
	
	//space hit 
	if (key.code == SCIM_KEY_space && key.mask == 0)
	    return space_hit();
	
	//enter hit
	if (key.code == SCIM_KEY_Return && key.mask == 0)
	    return enter_hit();

	if ((key.mask & ( ~ (SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask))) == 0)
	    return insert(key.get_ascii_code());
    }

    if ((key.mask & ( ~(SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask))) == 0)
	return post_process (key.get_ascii_code());
    return false;
}

void
PinyinInstance::select_candidate (unsigned int item)
{
    if ( is_special_mode ())
	special_mode_lookup_select (item);
    else
	lookup_select (item);
}

void
PinyinInstance::update_lookup_table_page_size (unsigned int page_size)
{
    if (page_size > 0)
	m_lookup_table.set_page_size (page_size);
}

void
PinyinInstance::lookup_table_page_up ()
{
    lookup_page_up ();
}

void
PinyinInstance::lookup_table_page_down()
{
    lookup_page_down ();
}

void
PinyinInstance::move_preedit_caret (unsigned int /*pos*/)
{
}

void
PinyinInstance::reset ()
{
    String encoding = get_encoding ();
    
    m_double_quotation_state = false;
    m_single_quotation_state = false;

    m_lookup_table.clear();

    m_inputed_string = String();

    m_converted_string = WideString ();
    m_preedit_string = WideString ();

    std::vector < std::pair <int, int> > ().swap (m_keys_preedit_index);
    g_array_set_size(m_parsed_keys, 0);
    g_array_set_size(m_parsed_poses, 0);
    g_array_set_size(m_phrase_items_with_freq, 0);

    g_array_set_size(m_constraints, 0);
    g_array_set_size(m_results, 0);

    m_keys_caret = 0;
    m_lookup_caret = 0;

    hide_lookup_table ();
    hide_preedit_string ();
    hide_aux_string ();
    refresh_all_properties ();
}

void
PinyinInstance::focus_in ()
{
    m_focused = true;
    
    initialize_all_properties ();
    
    hide_preedit_string ();
    hide_aux_string ();

    init_lookup_table_labels ();
    
    if ( !is_english_mode ()){
	refresh_preedit_string();
	refresh_preedit_caret();
	refresh_aux_string ();

	if (m_lookup_table.number_of_candidates ()){
	    m_lookup_table.set_page_size (m_lookup_table_def_page_size);
	    show_lookup_table ();
	    update_lookup_table (m_lookup_table);
	}
    } else {
	english_mode_refresh_preedit ();
    }
}

void
PinyinInstance::focus_out ()
{
    m_focused = false;
}

void
PinyinInstance::trigger_property (const String &property)
{
    bool update_pinyin_scheme = false;

    if ( property == NOVEL_PROP_STATUS) {
	m_forward = !m_forward;
	reset();
    }else if (property == NOVEL_PROP_LETTER) {
	int which = ((m_forward || is_english_mode ()) ? 1 : 0);
	m_full_width_letter[which] = !m_full_width_letter [which];

	refresh_letter_property ();
    }else if (property == NOVEL_PROP_PUNCT) {
	int which = ((m_forward || is_english_mode ()) ? 1 : 0);
	m_full_width_punctuation [which] = !m_full_width_punctuation [which];
	
	refresh_punct_property();
    }else if (property == NOVEL_PROP_PINYIN_SCHEME_QUAN_PIN){
	m_factory->m_shuang_pin = false;
	update_pinyin_scheme = true;
    }else if (property == NOVEL_PROP_PINYIN_SCHEME_SP_STONE) {
	m_factory->m_shuang_pin = true;
	m_factory->m_shuang_pin_scheme = SHUANG_PIN_STONE;
	update_pinyin_scheme = true;
    }else if (property == NOVEL_PROP_PINYIN_SCHEME_SP_ZRM) {
	m_factory->m_shuang_pin = true;
	m_factory->m_shuang_pin_scheme = SHUANG_PIN_ZRM;
	update_pinyin_scheme = true;
    }else if ( property == NOVEL_PROP_PINYIN_SCHEME_SP_MS) {
	m_factory->m_shuang_pin = true;
	m_factory->m_shuang_pin_scheme = SHUANG_PIN_MS;
	update_pinyin_scheme = true;
    }else if (property == NOVEL_PROP_PINYIN_SCHEME_SP_ZIGUANG) {
	m_factory->m_shuang_pin = true;
	m_factory->m_shuang_pin_scheme = SHUANG_PIN_ZIGUANG;
	update_pinyin_scheme = true;
    }else if (property == NOVEL_PROP_PINYIN_SCHEME_SP_ABC) {
	m_factory->m_shuang_pin = true;
	m_factory->m_shuang_pin_scheme = SHUANG_PIN_ABC;
	update_pinyin_scheme = true;
    }else if (property == NOVEL_PROP_PINYIN_SCHEME_SP_LIUSHI) {
	m_factory->m_shuang_pin = true;
	m_factory->m_shuang_pin_scheme = SHUANG_PIN_LIUSHI;
	update_pinyin_scheme = true;
    }

    if (update_pinyin_scheme){
	m_factory->init_pinyin_parser ();
	refresh_pinyin_scheme_property ();
	reset();
	m_factory->m_config->write (String (NOVEL_CONFIG_IMENGINE_PINYIN_SHUANG_PIN), m_factory->m_shuang_pin);
	m_factory->m_config->write (String (NOVEL_CONFIG_IMENGINE_PINYIN_SHUANG_PIN_SCHEME) ,(int ) m_factory->m_shuang_pin_scheme);
    }
}

void
PinyinInstance::refresh_aux_string ()
{
    if (m_factory->m_auto_fill_preedit) {
	WideString aux;
	AttributeList attrs;

	if ( m_factory->m_show_all_keys){
	    for(size_t i = 0; i < m_parsed_keys->len; ++i) {
		PinyinKey * pinyin_key = 
		    &g_array_index(m_parsed_keys, PinyinKey, i);
		WideString key = utf8_mbstowcs(pinyin_key->get_key_string());
		if (m_lookup_caret == i){
		    attrs.push_back(Attribute
				    (aux.length (),
				     key.length (),
				     SCIM_ATTR_DECORATE, SCIM_ATTR_DECORATE_REVERSE));
		
		}
		aux += key;
		aux.push_back(0x20);
				  
	    }
	} else {
	    int i;
	    if (m_parsed_keys->len == 0){
		aux = utf8_mbstowcs (m_inputed_string);
	    } else if (m_keys_caret >= m_parsed_keys->len){
		PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_parsed_poses->len - 1);
		int parsed_len = pos->get_end_pos();
		for ( i = parsed_len; i < (int) m_inputed_string.length (); ++i){
		    aux += (ucs4_t) (m_inputed_string[i]);
		}
	    }else {
		PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_keys_caret);
		for ( i = pos->get_pos(); i < pos->get_end_pos(); ++i)
		    aux += (ucs4_t) (m_inputed_string[i]);
	    }
	    
	    if ( m_parsed_keys->len &&
		 m_keys_caret > 0 &&
		 m_keys_caret <= m_parsed_keys->len){
		aux.insert(aux.begin(), (ucs4_t) 0x0020);
		
		PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_keys_caret -1);
		for ( i = pos->get_end_pos () - 1;
		      i >= pos->get_pos(); --i)
		    aux = (ucs4_t) (m_inputed_string [i]) + aux;
	    }
	}
	
	if ( aux.length ()){
	    update_aux_string (aux, attrs);
	    show_aux_string ();
	}else{
	    hide_aux_string ();
	}

	return ;
    }
}

void PinyinInstance::initialize_all_properties ()
{
    PropertyList proplist;
    

    proplist.push_back (_pinyin_scheme_property);
    proplist.push_back (_pinyin_quan_pin_property);
    proplist.push_back (_pinyin_sp_stone_property);
    proplist.push_back (_pinyin_sp_zrm_property);
    proplist.push_back (_pinyin_sp_ms_property);
    proplist.push_back (_pinyin_sp_ziguang_property);
    proplist.push_back (_pinyin_sp_abc_property);
    proplist.push_back (_pinyin_sp_liushi_property);
    proplist.push_back (_status_property);
    proplist.push_back (_letter_property);
    proplist.push_back (_punct_property);

    register_properties (proplist);
    refresh_all_properties ();
    refresh_pinyin_scheme_property ();
}

void
PinyinInstance::refresh_all_properties ()
{
    refresh_status_property ();
    refresh_letter_property ();
    refresh_punct_property ();
}

void
PinyinInstance::refresh_status_property ()
{
    if ( is_english_mode() || m_forward)
	_status_property.set_label("英");
    else
	_status_property.set_label ("中");

    update_property(_status_property);
}

void
PinyinInstance::refresh_letter_property ()
{
    _letter_property.set_icon
	(m_full_width_letter[(m_forward || is_english_mode ())? 1: 0] ? NOVEL_FULL_LETTER_ICON : NOVEL_HALF_LETTER_ICON);

    update_property (_letter_property);			  
}

void
PinyinInstance::refresh_punct_property ()
{
    _punct_property.set_icon 
	(m_full_width_punctuation [ (m_forward || is_english_mode ()) ? 1: 0] ?
	 NOVEL_FULL_PUNCT_ICON : NOVEL_HALF_PUNCT_ICON);

    update_property (_punct_property);
}

void
PinyinInstance::refresh_pinyin_scheme_property ()
{
    String tip;

    if (m_factory->m_shuang_pin) {
        switch (m_factory->m_shuang_pin_scheme) {
            case SHUANG_PIN_STONE:
                tip = _pinyin_sp_stone_property.get_label ();
                break;
            case SHUANG_PIN_ZRM:
                tip = _pinyin_sp_zrm_property.get_label ();
                break;
            case SHUANG_PIN_MS:
                tip = _pinyin_sp_ms_property.get_label ();
                break;
            case SHUANG_PIN_ZIGUANG:
                tip = _pinyin_sp_ziguang_property.get_label ();
                break;
            case SHUANG_PIN_ABC:
                tip = _pinyin_sp_abc_property.get_label ();
                break;
            case SHUANG_PIN_LIUSHI:
                tip = _pinyin_sp_liushi_property.get_label ();
                break;
        }
        _pinyin_scheme_property.set_label ("双");
    } else {
        tip = _pinyin_quan_pin_property.get_label ();
        _pinyin_scheme_property.set_label ("全");
    }

    _pinyin_scheme_property.set_tip (tip);
    update_property (_pinyin_scheme_property);
}

void
PinyinInstance::init_lookup_table_labels ()
{
    std::vector<WideString> candidate_labels;
    char buf [2] = { 0, 0 };

    if (m_pinyin_global->use_tone ()) {
        for (int i=5; i<9; i++) {
            buf [0] = '1' + i;
            candidate_labels.push_back (utf8_mbstowcs (buf));
        }

        buf [0] = '0';
        candidate_labels.push_back (utf8_mbstowcs (buf));
    } else {
        for (int i=0; i<9; i++) {
            buf [0] = '1' + i;
            candidate_labels.push_back (utf8_mbstowcs (buf));
        }
    }

    m_lookup_table_def_page_size = (int) candidate_labels.size ();

    m_lookup_table.set_page_size (m_lookup_table_def_page_size);
    m_lookup_table.set_candidate_labels (candidate_labels);
    m_lookup_table.show_cursor ();
}

bool
PinyinInstance::caret_left (bool home)
{
    if (m_inputed_string.length ()){
	if (m_keys_caret > 0) {
	    if (home) m_keys_caret = 0;
	    else m_keys_caret--;
	    
	    if (m_keys_caret <= (int) m_converted_string.length() &&
		m_keys_caret <= m_parsed_keys->len ){
		m_lookup_caret = m_keys_caret;
		refresh_preedit_string ();
		refresh_lookup_table (true);
	    }
	    refresh_aux_string ();
	    
	    refresh_preedit_caret ();
	}else {
	    return caret_right (true);
	}
	return true;
    }
    return false;
}

bool
PinyinInstance::caret_right (bool end)
{
    if (m_inputed_string.length ()) {
	if (m_keys_caret <= (int) m_parsed_keys->len){
	    if (end){
		if (has_unparsed_chars ())
		    m_keys_caret = m_parsed_keys->len + 1;
		else
		    m_keys_caret = m_parsed_keys->len;
	    }else {
		m_keys_caret ++;
	    }
	    
	    if ( !has_unparsed_chars () && m_keys_caret > (int) m_parsed_keys->len)
		return caret_left(true);

	    if (m_keys_caret <= (int) m_converted_string.length () &&
		m_keys_caret <= (int) m_parsed_keys->len ){
		m_lookup_caret = m_keys_caret;
		refresh_preedit_string();
		refresh_lookup_table(true);
	    }
	    refresh_aux_string ();
	    
	    refresh_preedit_caret ();
	}else {
	    return caret_left (true);
	}
	return true;
    }
    return false;
}

bool
PinyinInstance::validate_insert_key (char key)
{
    if (m_pinyin_global->use_tone() && key >= '1' && key <= '5')
	return true;
    
    if (m_factory->m_shuang_pin && key == ';')
	return true;

    if ( (key>= 'a' && key <= 'z') || key == '\'')
	return true;
    
    return false;
}

bool
PinyinInstance::insert (char key)
{
    if (key == 0) return false;

    if (validate_insert_key(key)) {
	int inputed_caret = calc_inputed_caret ();
	
	PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_parsed_poses->len - 1);
	if ((m_parsed_keys->len != 0 &&
	     m_inputed_string.length() - pos->get_end_pos() 
	     > NOVEL_PINYIN_KEY_MAXLEN) || 
	    (m_parsed_keys->len == 0 &&
	     m_inputed_string.length() > NOVEL_PINYIN_KEY_MAXLEN))
	    return true;

	if ( inputed_caret == 0 && ((key >= '1' && key <= '5') || key == '\'' ||
				     key == ';'))
	     return post_process(key);
	
	String::iterator i = m_inputed_string.begin () + inputed_caret;
	
	if ( key != '\'' ){
	    m_inputed_string.insert (i, key);
	} else {
	    //Don't insert more than on split chars in one place.
	    if ((i != m_inputed_string.begin () && *(i-1) == '\'') ||
		(i != m_inputed_string.end() && *i == '\''))
		return true;
	    m_inputed_string.insert(i, key);
	}

	calc_parsed_keys ();

	m_keys_caret = inputed_caret_to_key_index (inputed_caret + 1);
	//if (m_keys_caret < (int) m_converted_string.length ())
	//std::cout<<"m_keys_caret:"<<m_keys_caret<<"\tm_parsed_keys:"<<m_parsed_keys->len<<std::endl;
	if (m_keys_caret < (int) m_parsed_keys->len)
	    m_lookup_caret = m_keys_caret;
	else if (m_lookup_caret > (int) m_converted_string.length ())
	    m_lookup_caret = m_converted_string.length();

	bool calc_lookup = auto_fill_preedit ();

	calc_keys_preedit_index();
	refresh_preedit_string();
	refresh_preedit_caret ();
	refresh_aux_string ();
	refresh_lookup_table (calc_lookup);
	return true;
    }

    return post_process (key);
}

bool
PinyinInstance::erase (bool backspace)
{
    if (m_inputed_string.length ()){
	int inputed_caret = calc_inputed_caret ();
	
	if (!backspace && inputed_caret < (int) m_inputed_string.length ())
	    inputed_caret ++;

	if (inputed_caret > 0) {
	    m_inputed_string.erase (inputed_caret - 1, 1);
	    
	    calc_parsed_keys ();
	    
	    m_keys_caret = inputed_caret_to_key_index (inputed_caret - 1);

	    if ( m_keys_caret <= (int) m_converted_string.length () &&
		 m_lookup_caret > m_keys_caret )
		m_lookup_caret = m_keys_caret;
	    else if ( m_lookup_caret > (int) m_converted_string.length ())
		m_lookup_caret = m_converted_string.length();
	    
	    bool calc_lookup = auto_fill_preedit ();
	    
	    calc_keys_preedit_index ();
	    refresh_preedit_string ();
	    refresh_preedit_caret ();
	    refresh_aux_string ();
	    refresh_lookup_table(calc_lookup);
	}
	return true;
    }
    return false;
}

bool
PinyinInstance::erase_by_key (bool backspace)
{
    if (m_inputed_string.length ()){
	// If there is no completed key, then ordinary erase function 
	// to erase a char.
	if (!m_parsed_keys->len)
	    return erase(backspace);

	// If the caret is at the end of parsed keys and there are unparsed
	// chars left, and it's not backspace the call erase.
	if (has_unparsed_chars () && m_keys_caret >= m_parsed_keys->len ){
	    PinyinKeyPos * pos = & g_array_index(m_parsed_poses, PinyinKeyPos, m_parsed_poses->len - 1);
	    String unparsed_chars = m_inputed_string.substr(pos->get_end_pos());

	    // Only one unparsed split char, delete it directly
	    if ( unparsed_chars.length () == 1 && unparsed_chars [0] == '\'') {
		m_inputed_string.erase (m_inputed_string.begin() + 
					pos->get_end_pos());
	    }else if ((m_keys_caret > m_parsed_keys->len) ||
		      (m_keys_caret == m_parsed_keys->len && !backspace)){
		return erase (backspace);
	    }
	    m_keys_caret = m_parsed_keys->len;
	}

	// If the caret is at the beginning of the preedit string and
	// it's a backspace, then do nothing.
	if ( backspace && !m_keys_caret)
	    return true;

	int inputed_caret = m_keys_caret;
	
	
	if (!backspace && inputed_caret < (int) m_parsed_keys->len)
	    ++ inputed_caret;
	
	if ( inputed_caret > 0 ){
	    -- inputed_caret;
	    
	    PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, inputed_caret);
	    int erased_pos = pos->get_pos();
	    int erased_len = pos->get_length();

	    m_inputed_string.erase (erased_pos, erased_len);

	    //Check whether should insert a split char between the previous and the next key.
	    if ( erased_pos&& erased_pos < m_inputed_string.length ()) {
		if ( m_inputed_string [ erased_pos-1]!= '\'' &&
		     m_inputed_string [ erased_pos] != '\''){
		    m_inputed_string.insert (m_inputed_string.begin () +
					     erased_pos, '\'');
		    erased_len --;
		}else if( m_inputed_string[erased_pos - 1] == '\'' &&
			  m_inputed_string [erased_pos] == '\''){
		    m_inputed_string.erase (m_inputed_string.begin () + erased_pos);
		    erased_len ++;
		}
	    }
	    calc_parsed_keys();
	    
	    m_keys_caret = inputed_caret;
	    
            if (m_keys_caret <= (int) m_converted_string.length () &&
                m_lookup_caret > m_keys_caret)
                m_lookup_caret = m_keys_caret;
            else if (m_lookup_caret > (int) m_converted_string.length ())
                m_lookup_caret = m_converted_string.length ();

            bool calc_lookup = auto_fill_preedit ();
	    
            calc_keys_preedit_index ();
            refresh_preedit_string ();
            refresh_preedit_caret ();
            refresh_aux_string ();
            refresh_lookup_table (calc_lookup);
	}
	return true;
    }
    return false;
}

bool
PinyinInstance::space_hit ()
{
    if ( m_inputed_string.length ()){
	if ( m_converted_string.length () ==0 && m_lookup_table.number_of_candidates () == 0)
	    return true;
	if (m_lookup_table.number_of_candidates () &&
	    (m_converted_string.length() <= m_parsed_keys->len ||
	     m_keys_caret == m_lookup_caret))
	    lookup_to_converted (m_lookup_table.get_cursor_pos ());
	
	if (m_converted_string.length() >= m_parsed_keys->len){
	    if (!m_factory->m_auto_fill_preedit || 
		m_lookup_caret == m_parsed_keys->len){
		commit_converted ();
	    }else {
		m_keys_caret = m_lookup_caret = m_parsed_keys->len;
	    }
	}

	bool calc_lookup = auto_fill_preedit();
	
	calc_keys_preedit_index();
	refresh_preedit_string ();
	refresh_preedit_caret();
	refresh_aux_string ();
	refresh_lookup_table (calc_lookup);
	
	return true;
    }
    return post_process (' ');
}

bool
PinyinInstance::enter_hit ()
{
    if (m_inputed_string.length ()) {
        WideString str = utf8_mbstowcs (m_inputed_string);
        reset ();
        commit_string (str);
	clear_constraints();
        return true;
    }
    return false;
}

bool
PinyinInstance::lookup_cursor_up ()
{
    if (m_inputed_string.length () && m_lookup_table.number_of_candidates ()) {
        m_lookup_table.cursor_up ();
        m_lookup_table.set_page_size (m_lookup_table_def_page_size);
        update_lookup_table (m_lookup_table);
        return true;
    }
    return false;
}

bool
PinyinInstance::lookup_cursor_down ()
{
    if (m_inputed_string.length () && m_lookup_table.number_of_candidates ()) {
        m_lookup_table.cursor_down ();
        m_lookup_table.set_page_size (m_lookup_table_def_page_size);
        update_lookup_table (m_lookup_table);
        return true;
    }
    return false;
}

bool
PinyinInstance::lookup_page_up ()
{
    if (m_inputed_string.length () && m_lookup_table.number_of_candidates ()) {
        m_lookup_table.page_up ();
        m_lookup_table.set_page_size (m_lookup_table_def_page_size);
        update_lookup_table (m_lookup_table);
        return true;
    }
    return false;
}

bool
PinyinInstance::lookup_page_down ()
{
    if (m_inputed_string.length () && m_lookup_table.number_of_candidates ()) {
        m_lookup_table.page_down ();
        m_lookup_table.set_page_size (m_lookup_table_def_page_size);
        update_lookup_table (m_lookup_table);
        return true;
    }
    return false;
}

bool
PinyinInstance::lookup_select (int index)
{
    if (m_inputed_string.length ()){
	if (m_lookup_table.number_of_candidates () == 0)
	    return true;

	index += m_lookup_table.get_current_page_start ();
	
	lookup_to_converted(index);

#if 0
	if (m_converted_string.length () >= m_parsed_keys->len &&
	    m_lookup_caret == (int) m_converted_string.length ()){
	    commit_converted ();
	}
#endif
	bool calc_lookup = auto_fill_preedit();
	
	calc_keys_preedit_index ();
	refresh_preedit_string();
	refresh_preedit_caret ();
	refresh_aux_string ();
	refresh_lookup_table (calc_lookup);

	return true;
    }

    return false;
}

bool
PinyinInstance::post_process(char key)
{
    if ( m_inputed_string.length () > 0){
	if (m_converted_string.length () == m_parsed_keys->len &&
	    !has_unparsed_chars ()){
	    commit_converted();
	    calc_keys_preedit_index ();
	    refresh_preedit_string ();
	    refresh_preedit_caret ();
	    refresh_aux_string ();
	    refresh_lookup_table();
	}else {
	    return true;
	}
    }
    
    if ((ispunct (key) && m_full_width_punctuation [ m_forward ? 1:0]) ||
	((isalnum (key) || key == 0x20) && m_full_width_letter [ m_forward ? 1:0])){
	commit_string(convert_to_full_width (key));
	return true;
    }
    return false;
}

void
PinyinInstance::lookup_to_converted (int index)
{
    if ( index < 0 || index >= (int) m_lookup_table.number_of_candidates())
	return;
    
    //commit best match string
    if (index == 0){
        //commit_converted();
        m_keys_caret = m_lookup_caret = m_converted_string.length();
        return;
    }

    phrase_token_t token = m_lookup_table.get_token(index);
    
    if ( !m_phrase_index->get_phrase_item(token, m_cache_phrase_item) )
	return;

    m_factory->m_pinyin_lookup->add_constraint(m_constraints, m_lookup_caret, token);
    m_factory->m_pinyin_lookup->get_best_match(m_parsed_keys, m_constraints, m_results);
    char * string;
    m_factory->m_pinyin_lookup->convert_to_utf8(m_results, string);
    m_converted_string = utf8_mbstowcs(string);
    g_free(string);

    size_t phrase_length = m_cache_phrase_item.get_phrase_length();
  
    m_lookup_caret += phrase_length;
    
    if (m_keys_caret < m_lookup_caret )
	m_keys_caret = m_lookup_caret;

}

void
PinyinInstance::commit_converted ()
{
    if ( m_converted_string.length()){
	
	// Clear the preedit string to avoid screen blinking
	update_preedit_string (WideString (), AttributeList ());

	commit_string(m_converted_string);
	
	if (m_pinyin_global && m_pinyin_global->use_dynamic_adjust ()){
	    m_factory->m_pinyin_lookup->train_result(m_parsed_keys, m_constraints, m_results);
	    m_factory->refresh();
	}

	if (m_converted_string.length () <= m_parsed_keys->len){
	    m_keys_caret -= m_converted_string.length();
	    PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, 
						m_converted_string.length() - 1);
	    m_inputed_string.erase(0, pos->get_end_pos());
	}else{
	    m_keys_caret -= m_parsed_keys->len;
	    PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_parsed_poses->len - 1);
	    m_inputed_string.erase(0, pos->get_end_pos());
	}
	
	if (m_keys_caret < 0) m_keys_caret = 0;

	m_converted_string = WideString();
	m_lookup_caret = 0;
	
	calc_parsed_keys ();

        clear_constraints();
    }
}

void PinyinInstance::clear_constraints(){
    for ( size_t i = 0 ; i < m_constraints->len; ++i){
	lookup_constraint_t * constraint = &g_array_index(m_constraints, lookup_constraint_t, i);
	constraint->m_type = NO_CONSTRAINT;
    }
}

void
PinyinInstance::refresh_preedit_string ()
{
    calc_preedit_string ();
    
    if ( m_preedit_string.length()){
	AttributeList attrs;

	if ( m_lookup_caret >= 0 && m_lookup_caret < (int) m_keys_preedit_index.size()){
	    attrs.push_back(Attribute (m_keys_preedit_index[m_lookup_caret].first,
				       m_keys_preedit_index[m_lookup_caret].second -
				       m_keys_preedit_index[m_lookup_caret].first,
				       SCIM_ATTR_DECORATE, SCIM_ATTR_DECORATE_REVERSE));
	}
	update_preedit_string (m_preedit_string, attrs);
	show_preedit_string ();
    }else {
	hide_preedit_string ();
    }
}

void
PinyinInstance::refresh_lookup_table (bool calc){
    if (calc) calc_lookup_table();
    
    if (m_lookup_table.number_of_candidates ()){
	if ( m_factory->m_always_show_lookup ||
	     !m_factory->m_auto_fill_preedit ||
	     m_lookup_caret == m_keys_caret){
	    update_lookup_table (m_lookup_table);
	    show_lookup_table();
	    return;
	}
    }
    hide_lookup_table ();
}

void
PinyinInstance::refresh_preedit_caret ()
{
    if (m_inputed_string.length ()) {
        int caret = calc_preedit_caret ();
        update_preedit_caret (caret);
    }
}

void
PinyinInstance::calc_parsed_keys ()
{
    m_factory->m_pinyin_parser->parse (* (m_pinyin_global->
					  get_pinyin_validator ()),
                                          m_parsed_keys,
                                          m_parsed_poses,
                                          m_inputed_string.c_str ());
#if 0
    for (size_t i=0; i < m_parsed_keys->len; ++i){
        PinyinKey * key = &g_array_index(m_parsed_keys, PinyinKey, i);
        std::cout << key->get_key_string () << " ";
    }

    std::cout << std::endl;

    std::cout<<m_inputed_string<<std::endl;
#endif
    m_factory->m_pinyin_lookup->validate_constraint(m_constraints, m_parsed_keys);
}

static gint fsign(gfloat val){
    if ( val > 0 ){
	return 1;
    }else if ( val < 0){
	return -1;
    }else {
	return 0;
    }
}

static gint lookup_table_sort (gconstpointer a, gconstpointer b){
    const PhraseItemWithFreq* itema = (const PhraseItemWithFreq *) a;
    const PhraseItemWithFreq* itemb = (const PhraseItemWithFreq *) b;
    if ( itemb->m_freq != itema->m_freq )
	return fsign(itemb->m_freq - itema->m_freq);
    return itema->m_token - itemb->m_token;
}

void
PinyinInstance::calc_lookup_table (){
    const gfloat lambda_parameter = LAMBDA_PARAMETER;
    //calculate the preedit string
    if ( 0 == m_parsed_keys->len ){
	m_lookup_table.clear();
	return;
    }
    m_factory->m_pinyin_lookup->get_best_match(m_parsed_keys, m_constraints, m_results);
    char * string;
    m_factory->m_pinyin_lookup->convert_to_utf8(m_results, string);
    m_converted_string = utf8_mbstowcs(string);
    //std::cout<<string<<std::endl;
    
    m_lookup_table.set_cursor_pos(0);

    //std::cout<<"lookup caret:"<<m_lookup_caret<<"\tkey len:"<<m_parsed_keys->len<<"inputed string:"<<m_inputed_string<<std::endl;
    if ( m_lookup_caret >= m_parsed_keys->len ){
	m_lookup_table.clear();
	m_lookup_table.append_entry(m_converted_string);
	return;
    }

    //remove duplicate items
    glong written;
    utf16_t * string_utf16 = g_utf8_to_utf16(string, -1, NULL, &written, NULL);
    assert(m_converted_string.size() == written);
    g_free(string);

    utf16_t item_buffer_utf16[MAX_PHRASE_LENGTH];

    //get last token
    phrase_token_t last_token = sentence_start;
    phrase_token_t * cur_token = &g_array_index(m_results, phrase_token_t, m_lookup_caret);
    if ( *cur_token ){
	for ( size_t i = m_lookup_caret - 1; i >= 0; --i){
	    cur_token = &g_array_index(m_results, phrase_token_t, i);
	    if ( * cur_token ) {
		last_token = * cur_token;
		break;
	    }
	}
    }else
	last_token = NULL;

    PhraseItemWithFreq item;
    
    PinyinKey * pinyin_keys = (PinyinKey *) m_parsed_keys -> data;

    Bigram * bigram = m_pinyin_global->get_bigram();
    SingleGram * system = NULL, * user = NULL;
    if ( last_token ) {
	bigram->load(last_token, system, user);
	if ( system && user){
	    guint32 total_freq;
	    assert(user->get_total_freq(total_freq));
	    assert(system->set_total_freq(total_freq));
	}
    }

    //re-use array here
    BigramPhraseArray system_array = g_array_new(FALSE, FALSE, sizeof(BigramPhraseItem));
    BigramPhraseArray user_array = g_array_new(FALSE, FALSE, sizeof(BigramPhraseItem));


    size_t i = std::min(m_parsed_keys ->len - m_lookup_caret, (unsigned int)MAX_PHRASE_LENGTH);
    PhraseIndexRanges ranges;
    memset(ranges, 0, sizeof(ranges));
    m_lookup_table.clear();
    m_lookup_table.append_entry(m_converted_string);
    for (; i > 0; --i){
	m_factory->m_pinyin_lookup->prepare_pinyin_lookup(ranges);
	g_array_set_size(m_phrase_items_with_freq, 0);
	int result = m_large_table->search(i, pinyin_keys + m_lookup_caret, ranges);
	if (!(result & SEARCH_OK)){
            m_factory->m_pinyin_lookup->destroy_pinyin_lookup(ranges);
	    continue;
        }
	for ( size_t m = 0; m < PHRASE_INDEX_LIBRARY_COUNT; ++m){
	    GArray * array = ranges[m];
	    if (!array ) continue;
	    for ( size_t n = 0; n < array->len; ++n){
		PhraseIndexRange * range = &g_array_index(array, PhraseIndexRange, n);
		g_array_set_size(system_array, 0);
		g_array_set_size(user_array, 0);
		if ( system )
		    system->search(range, system_array);
		if ( user )
		    user->search(range, user_array);
		guint32 system_pos = 0, user_pos =0;
		for ( phrase_token_t token = range->m_range_begin; token != range->m_range_end; ++token){
		    //deal with user first.
		    bool has_user = false; gfloat poss = 0.;
		    for ( ; user_pos < user_array -> len; ++user_pos ){
			BigramPhraseItem * user_item = &g_array_index(user_array, BigramPhraseItem, user_pos);
			if ( token < user_item -> m_token )
			    break;
			if ( token == user_item -> m_token ){
			    poss = user_item -> m_freq;
			    has_user = true;
			}
		    }
		    if ( !has_user ) {
			for ( ; system_pos < system_array -> len; ++system_pos){
			    BigramPhraseItem * system_item = &g_array_index(system_array, BigramPhraseItem, system_pos);
			    if ( token < system_item -> m_token )
				break;
			    if ( token == system_item -> m_token ){
				poss = system_item -> m_freq;
			    }
			}
		    }
		    item.m_token = token;
		    m_phrase_index->get_phrase_item(token, m_cache_phrase_item);
		    if ( m_converted_string.size() == i ){
			assert(m_cache_phrase_item.get_phrase_string(item_buffer_utf16));
			if ( 0 == memcmp(string_utf16, item_buffer_utf16, 
					 i * sizeof(utf16_t) ) )
			    continue;
		    }
		
		    gfloat pinyin_poss =
			m_cache_phrase_item.get_pinyin_possibility
			(*(m_pinyin_global->get_custom_setting()),
			 pinyin_keys + m_lookup_caret);

		    item.m_freq = pinyin_poss * (poss * lambda_parameter + 
			m_cache_phrase_item.get_unigram_frequency() / 
			(gfloat)(m_phrase_index->get_phrase_index_total_freq())
			* (1 - lambda_parameter) ) ;
		    g_array_append_val(m_phrase_items_with_freq, item);
		}
	    }
	}
	g_array_sort(m_phrase_items_with_freq, lookup_table_sort);
	phrase_token_t last_token = NULL;
	for ( size_t m = 0; m < m_phrase_items_with_freq->len; ++m){
	    PhraseItemWithFreq * item = &g_array_index(m_phrase_items_with_freq, PhraseItemWithFreq, m);
	    if ( last_token != item->m_token )
		m_lookup_table.append_entry(item->m_token);
	    last_token = item->m_token;
	}
	g_array_set_size(m_phrase_items_with_freq, 0);
	m_factory->m_pinyin_lookup->destroy_pinyin_lookup(ranges);
    }
    //free resource
    g_array_free(system_array, TRUE);
    g_array_free(user_array, TRUE);

    if ( system )
	delete system;
    if ( user )
	delete user;
    g_free(string_utf16);
}

void
PinyinInstance::calc_keys_preedit_index ()
{
    m_keys_preedit_index.clear ();

    int numkeys = (int) m_parsed_keys->len;
    int numconvs = (int) m_converted_string.length ();
    int i;
    int len;

    std::pair <int, int> kpi;

    //Insert position index for converted string
    for (i = 0; i < numconvs; i++) {
        kpi.first = i;
        kpi.second = i+1;
        m_keys_preedit_index.push_back (kpi);
    }

    len = numconvs;

    for (i = numconvs; i < numkeys; i++) {
        kpi.first = len;
	PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, i);
        kpi.second = len + pos->get_length ();
        len += (pos->get_length () + 1);
        m_keys_preedit_index.push_back (kpi);
    }
}

void
PinyinInstance::calc_preedit_string ()
{
    m_preedit_string = WideString ();

    if (m_inputed_string.length () == 0) {
        return;
    }

    WideString unparsed_string;
    unsigned int i;

    m_preedit_string = m_converted_string;

    for (i = m_converted_string.length (); i<m_parsed_keys->len; i++) {
	PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, i);
        int endpos = pos->get_end_pos ();
        for (int j = pos->get_pos (); j<endpos; j++) {
            m_preedit_string += ((ucs4_t) m_inputed_string [j]);
        }
        m_preedit_string += (ucs4_t) 0x0020;
    }

    if (m_parsed_keys->len == 0) {
        unparsed_string = utf8_mbstowcs (m_inputed_string);
    } else {
	PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_parsed_poses->len - 1);
        int parsed_len = pos->get_end_pos ();
        for (i=parsed_len; i<m_inputed_string.length (); i++) {
            unparsed_string += (ucs4_t) (m_inputed_string [i]);
        }
    }

    if (unparsed_string.length ())
        m_preedit_string += unparsed_string;
}

int
PinyinInstance::calc_inputed_caret ()
{
    int caret;
    if (m_keys_caret <= 0)
        caret = 0;
    else if (m_keys_caret < (int) m_parsed_keys->len) {
	PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_keys_caret);
        caret = pos->get_pos ();
    } else if (m_keys_caret == (int) m_parsed_keys->len) {
	PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_keys_caret - 1);
        caret = pos->get_end_pos ();
        if (caret < (int)m_inputed_string.length () && m_inputed_string [caret] == '\'')
            caret ++;
    } else {
        caret = m_inputed_string.length ();
    }

    return caret;
}

int
PinyinInstance::calc_preedit_caret ()
{
    int caret;
    if (m_keys_caret <= 0)
        caret = 0;
    else if (m_keys_caret < (int)m_keys_preedit_index.size ()) {
        caret = m_keys_preedit_index [m_keys_caret].first;
    } else if (m_keys_caret == (int)m_keys_preedit_index.size ()) {
        caret = m_keys_preedit_index [m_keys_caret-1].second;
    } else {
        caret = m_preedit_string.length ();
    }
    return caret;
}

int
PinyinInstance::inputed_caret_to_key_index (int caret)
{
    if (m_parsed_keys->len == 0) {
        if (caret > 0) return 1;
        else return 0;
    }

    for (size_t i = 0; i < m_parsed_keys->len; i++) {
	PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, i);
        if (caret >= pos->get_pos () && caret < pos->get_end_pos ())
            return i;
    }

    PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_parsed_keys->len - 1);
    if (caret == pos->get_end_pos ())
        return m_parsed_keys->len;

    return m_parsed_keys->len + 1;
}

bool
PinyinInstance::has_unparsed_chars ()
{
    if (m_inputed_string.length () == 0) 
        return false;
    if (m_parsed_keys->len == 0)
        return true;
    PinyinKeyPos * pos = &g_array_index(m_parsed_poses, PinyinKeyPos, m_parsed_keys->len - 1);
    if (pos->get_end_pos () < (int)m_inputed_string.length ())
        return true;
    return false;
}

bool
PinyinInstance::match_key_event (const std::vector <KeyEvent>& keyvec, const KeyEvent& key)
{
    std::vector<KeyEvent>::const_iterator kit; 

    for (kit = keyvec.begin (); kit != keyvec.end (); ++kit) {
        if (key.code == kit->code && key.mask == kit->mask)
            if (!(key.mask & SCIM_KEY_ReleaseMask) || m_prev_key.code == key.code)
                return true;
    }
    return false;
}

bool
PinyinInstance::auto_fill_preedit (int invalid_pos)
{
    //if (m_factory->m_auto_fill_preedit) {

        calc_lookup_table ();

        return false;
    //}

    return true;
}

WideString
PinyinInstance::convert_to_full_width (char key)
{
    WideString str;
    if (key == '.')
        str.push_back (0x3002);
    else if (key == '\\')
        str.push_back (0x3001);
    else if (key == '^') {
        str.push_back (0x2026);
        str.push_back (0x2026);
    } else if (key == '\"') {
        if (!m_double_quotation_state)
            str.push_back (0x201c);
        else
            str.push_back (0x201d);
        m_double_quotation_state = !m_double_quotation_state;
    } else if (key == '\'') {
        if (!m_single_quotation_state)
            str.push_back (0x2018);
        else
            str.push_back (0x2019);
        m_single_quotation_state = !m_single_quotation_state;
    } else if (key == '<' && !m_forward) {
        str.push_back (0x300A);
    } else if (key == '>' && !m_forward) {
        str.push_back (0x300B);
    } else if (key == '$') {
        str.push_back (0xFFE5);
    } else if (key == '_') {
        str.push_back (0x2014);
        str.push_back (0x2014);
    } else {
        str.push_back (scim_wchar_to_full_width (key));
    }

    return str;
}

bool
PinyinInstance::english_mode_process_key_event (const KeyEvent &key)
{
    //Start forward mode
    if (!m_inputed_string.length () && key.code == SCIM_KEY_v && key.mask == 0) {
        m_inputed_string.push_back ('v');
        m_converted_string.push_back ('v');
        refresh_all_properties ();
    }
    //backspace key
    else if ((key.code == SCIM_KEY_BackSpace ||
              key.code == SCIM_KEY_Delete) &&
             key.mask == 0) {
        m_converted_string.erase (m_converted_string.length () - 1);
        if (m_converted_string.length () <= 1)
            m_converted_string.clear ();
    }
    //enter or space hit, commit string
    else if ((key.code == SCIM_KEY_Return || key.code == SCIM_KEY_space) &&
             (key.mask & (~ (SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask))) == 0) {
        WideString str = m_converted_string.substr (1);
        if (str.length ())
            commit_string (str);
        m_converted_string.clear ();
    }
    //insert normal keys
    else if ((key.mask & (~ (SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask))) == 0) {
        char ch = key.get_ascii_code ();
        if ((ispunct (ch) && m_full_width_punctuation [1]) ||
            (isalnum (ch) && m_full_width_letter [1])) {
            m_converted_string += convert_to_full_width (ch);
        } else if (ch) {
            ucs4_t wc;
            utf8_mbtowc (&wc, (unsigned char*) &ch, 1);
            m_converted_string += wc;
        } else {
            return true;
        }
    }
    //forward special keys directly.
    else if ((key.mask & (~ (SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask)))) 
        return false;

    if (m_converted_string.length ()) {
        english_mode_refresh_preedit ();
    } else {
        reset ();
    }

    return true;
}

void
PinyinInstance::english_mode_refresh_preedit ()
{
    WideString str = m_converted_string.substr (1);
    if (str.length ()) {
        update_preedit_string (str);
        update_preedit_caret (str.length ());
        show_preedit_string ();
    } else {
        hide_preedit_string ();
    }
}

bool
PinyinInstance::is_english_mode () const
{
    if (m_inputed_string.length () && m_inputed_string [0] == 'v' &&
        m_converted_string.length () && m_converted_string [0] == 'v')
        return true;
    return false;
}

bool
PinyinInstance::special_mode_process_key_event (const KeyEvent &key)
{
    //Start forward mode
    if (!m_inputed_string.length () && key.code == SCIM_KEY_i && key.mask == 0) {
        m_inputed_string.push_back ('i');
        m_converted_string.push_back ('i');
        special_mode_refresh_preedit ();
        special_mode_refresh_lookup_table ();
        return true;
    }

    //lookup table cursor up
    if (key.code == SCIM_KEY_Up && key.mask == 0)
        return lookup_cursor_up ();

    //lookup table cursor down
    if (key.code == SCIM_KEY_Down && key.mask == 0)
        return lookup_cursor_down ();

    //lookup table page up
    if (match_key_event (m_factory->m_page_up_keys, key))
        if (lookup_page_up ()) return true;

    //lookup table page down
    if (match_key_event (m_factory->m_page_down_keys, key))
        if (lookup_page_down ()) return true;

    //select lookup table
    if (m_pinyin_global->use_tone ()) {
        if (((key.code >= SCIM_KEY_6 && key.code <= SCIM_KEY_9) || key.code == SCIM_KEY_0) && key.mask == 0) {
            int index;
            if (key.code == SCIM_KEY_0) index = 4;
            else index = key.code - SCIM_KEY_6;
            if (special_mode_lookup_select (index)) return true;
        }
    } else {
        if (key.code >= SCIM_KEY_1 && key.code <= SCIM_KEY_9 && key.mask == 0) {
            int index = key.code - SCIM_KEY_1;
            if (special_mode_lookup_select (index)) return true;
        }
    }

    //backspace key
    if ((key.code == SCIM_KEY_BackSpace ||
         key.code == SCIM_KEY_Delete) &&
         key.mask == 0) {
        m_inputed_string.erase (m_inputed_string.length () - 1);
        m_converted_string.erase (m_converted_string.length () - 1);
    }
    //enter or space hit, commit string
    else if ((key.code == SCIM_KEY_space || key.code == SCIM_KEY_Return) &&
             (key.mask & (~ (SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask))) == 0) {

        if (m_lookup_table.number_of_candidates ())
            commit_string (m_lookup_table.get_candidate (m_lookup_table.get_cursor_pos ()));
        else
            commit_string (m_converted_string);

        m_inputed_string.clear ();
        m_converted_string.clear ();
    }
    //insert normal keys
    else if ((key.mask & (~ (SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask))) == 0 &&
              key.code != 0 &&
              m_inputed_string.length () <= m_factory->m_special_table.get_max_key_length ()) {
        char ch = key.get_ascii_code ();

        if (ch) {
            m_inputed_string += ch;
            m_converted_string += (ucs4_t) ch;
        } else {
            return true;
        }
    }
    //forward special keys directly.
    else if ((key.mask & (~ (SCIM_KEY_ShiftMask + SCIM_KEY_CapsLockMask)))) 
        return false;

    if (m_inputed_string.length ()) {
        special_mode_refresh_preedit ();
        special_mode_refresh_lookup_table ();
    } else {
        reset ();
    }

    return true;
}

void
PinyinInstance::special_mode_refresh_preedit ()
{
    if (m_converted_string.length ()) {
        update_preedit_string (m_converted_string);
        update_preedit_caret (m_converted_string.length ());
        show_preedit_string ();
    } else {
        hide_preedit_string ();
    }
}

void
PinyinInstance::special_mode_refresh_lookup_table ()
{
    m_lookup_table.clear ();
    m_lookup_table.set_page_size (m_lookup_table_def_page_size);

    if (m_inputed_string.length () > 1) {
        std::vector<WideString> result;
        String key = m_inputed_string.substr (1);

        if (m_factory->m_special_table.find (result, key) > 0) {
            for (std::vector<WideString>::iterator it = result.begin (); it != result.end (); ++ it) {
		m_lookup_table.append_entry (*it);
            }
            if (m_lookup_table.number_of_candidates ()) {
                show_lookup_table ();
                update_lookup_table (m_lookup_table);
                return;
            }
        }
    }
    hide_lookup_table ();
}

bool
PinyinInstance::special_mode_lookup_select (int index)
{
    if (m_inputed_string.length () && m_lookup_table.number_of_candidates ()) {

        index += m_lookup_table.get_current_page_start ();

        WideString str = m_lookup_table.get_candidate (index);
        if (str.length ()) {
            commit_string (str);
        }
        reset ();
        return true;
    }

    return false;
}

bool
PinyinInstance::is_special_mode () const
{
    if (m_inputed_string.length () && m_inputed_string [0] == 'i' &&
        m_converted_string.length () && m_converted_string [0] == 'i')
        return true;
    return false;
}

void
PinyinInstance::reload_config (const ConfigPointer &config)
{
    reset ();
    if (m_factory->valid () && m_pinyin_global) {
        m_large_table = m_pinyin_global->get_pinyin_table ();
        m_phrase_index = m_pinyin_global->get_phrase_index ();
	m_lookup_table.set_phrase_index(m_phrase_index);
    } else {
        m_large_table = 0;
	m_phrase_index = 0;
	m_lookup_table.set_phrase_index(0);
    }
}
