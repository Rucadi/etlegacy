/**
 * Wolfenstein: Enemy Territory GPL Source Code
 * Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
 *
 * ET: Legacy
 * Copyright (C) 2012 Jan Simek <mail@etlegacy.com>
 *
 * This file is part of ET: Legacy - http://www.etlegacy.com
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ET: Legacy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ET: Legacy. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, Wolfenstein: Enemy Territory GPL Source Code is also
 * subject to certain additional terms. You should have received a copy
 * of these additional terms immediately following the terms and conditions
 * of the GNU General Public License which accompanied the source code.
 * If not, please request a copy in writing from id Software at the address below.
 *
 * id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
 *
 * @file i18n_main.cpp
 * @brief Glue for findlocale and tinygettext
 */

#ifndef FEATURE_GETTEXT
#error This file should only be compiled if you want i18n support
#endif

extern "C"
{
#include "q_shared.h"
#include "qcommon.h"
#include "i18n_findlocale.h"
}

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <map>

#include "../../libs/tinygettext/po_parser.hpp"
#include "../../libs/tinygettext/tinygettext.hpp"
#include "../../libs/tinygettext/log.hpp"

tinygettext::DictionaryManager dictionary;
tinygettext::DictionaryManager dictionary_mod;

cvar_t      *cl_language;
cvar_t      *cl_languagedebug;
static char cl_language_last[3];

std::map <std::string, std::string> strings; // original text / translated text

static void TranslationMissing(const char *msgid);

/**
 * @brief Attempts to detect the system language unless cl_language was already set.
 * Then loads the PO file containing translated strings.
 */
void I18N_Init(void)
{
	FL_Locale                       *locale;
	std::set<tinygettext::Language> languages;

	cl_language      = Cvar_Get("cl_language", "en", CVAR_ARCHIVE);
	cl_languagedebug = Cvar_Get("cl_languagedebug", "0", CVAR_ARCHIVE);

	tinygettext::Log::set_log_error_callback(&Tinygettext_Error);
	tinygettext::Log::set_log_info_callback(&Tinygettext_Info);
	tinygettext::Log::set_log_warning_callback(&Tinygettext_Warning);

	FL_FindLocale(&locale, FL_MESSAGES);

	// Do not change the language if it is already set
	if (cl_language && !cl_language->string[0])
	{
		if (locale->lang && locale->lang[0]) // && locale->country && locale->country[0])
		{
			Cvar_Set("cl_language", va("%s", locale->lang)); //, locale->country));
		}
		else
		{
			// Language detection failed. Fallback to English
			Cvar_Set("cl_language", "en");
		}
	}

	dictionary.add_directory("locale");
	// TODO: dictionary_mod.add_directory();

	languages = dictionary.get_languages();

	Com_Printf("Available translations:");
	for (std::set<tinygettext::Language>::iterator p = languages.begin(); p != languages.end(); p++)
	{
		Com_Printf(" %s", p->get_name().c_str());
	}

	I18N_SetLanguage(cl_language->string);
	FL_FreeLocale(&locale);
}

/**
 * @brief Loads a localization file
 */
void I18N_SetLanguage(const char *language)
{
	// TODO: check if there is a localization file available for the selected language
	dictionary.set_language(tinygettext::Language::from_env(std::string(language)));

	Com_Printf("\nLanguage set to %s\n", dictionary.get_language().get_name().c_str());
	Com_sprintf(cl_language_last, sizeof(cl_language_last), language);

	strings.clear();
}

/**
 * @brief Translates a string using the specified dictionary
 *
 * Localized strings are stored in a map container as tinygettext would
 * attempt to read them from the po file at each call and would endlessly
 * spam the console with warnings if the requested translation did not exist.
 *
 * @param msgid original string in English
 * @param dict dictionary to use (client / mod)
 * @return translated string or English text if dictionary was not found
 */
static const char *_I18N_Translate(const char *msgid, tinygettext::DictionaryManager &dict)
{
	// HACK: how to tell tinygettext not to translate if cl_language is English?
	if (!Q_stricmp(cl_language->string, "en"))
	{
		return msgid;
	}

	if (Q_stricmp(cl_language->string, cl_language_last))
	{
		I18N_SetLanguage(cl_language->string);
	}

	// Store translated string if it is not there yet
	if (strings.find(msgid) == strings.end())
	{
		strings.insert(std::make_pair(msgid, dict.get_dictionary().translate(msgid)));
	}

	if (cl_languagedebug->integer != 0)
	{
		if (!Q_stricmp(strings.find(msgid)->second.c_str(), msgid))
		{
			TranslationMissing(msgid);
		}
	}

	return strings.find(msgid)->second.c_str();
}

const char *I18N_Translate(const char *msgid)
{
	return _I18N_Translate(msgid, dictionary);
}

const char *I18N_TranslateMod(const char *msgid)
{
	return _I18N_Translate(msgid, dictionary); // TODO: dictionary_mod
}

/**
 * @brief A dumb function which saves missing strings for the current language and mod
 * passed to it
 * @param msgid original text
 * @param filename where to save the missing strings
 */
static void TranslationMissing(const char *msgid)
{
	fileHandle_t file;

	FS_FOpenFileByMode("missing_translations.txt", &file, FS_APPEND);
	FS_Write(va("TRANSLATE(\"%s\");\n", msgid), MAX_STRING_CHARS, file);

	FS_FCloseFile(file);
}

/**
 * Logging functions which override the default ones from Tinygettext
 */

void Tinygettext_Error(const std::string& str)
{
	Com_Printf("^1%s^7", str.c_str());
}

void Tinygettext_Warning(const std::string& str)
{
	if (cl_languagedebug->integer != 0)
	{
		Com_Printf("^3%s^7", str.c_str());
	}
}

void Tinygettext_Info(const std::string& str)
{
	if (cl_languagedebug->integer != 0)
	{
		Com_Printf("%s", str.c_str());
	}
}
