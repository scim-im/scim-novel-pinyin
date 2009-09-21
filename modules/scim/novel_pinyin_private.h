/** @file novel_pinyin_private.h
 *  private used headers are included in this header.
 */

/* 
 * Novel Pinyin Input Method
 * 
 * Copyright (c) 2008 Peng Wu <alexepico@gmail.com>
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * $Id: novel_pinyin_private.h,v 1.6 2008/10/02 05:29:31 alexepico Exp $
 */

#ifndef __NOVEL_PINYIN_PRIVATE_H
#define __NOVEL_PINYIN_PRIVATE_H

// Include scim configuration header
#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#if defined(HAVE_LIBINTL_H) && defined(ENABLE_NLS)
  #include <libintl.h>
  #define _(String) dgettext(GETTEXT_PACKAGE,String)
  #define N_(String) (String)
#else
  #define _(String) (String)
  #define N_(String) (String)
  #define bindtextdomain(Package,Directory)
  #define textdomain(domain)
  #define bind_textdomain_codeset(domain,codeset)
#endif

#define NOVEL_PINYIN_KEY_MAXLEN    7

#endif //__NOVEL_PINYIN_PRIVATE_H

/*
vi:ts=4:nowrap:ai:expandtab
*/
