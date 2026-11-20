#pragma once

// Setting one key of an INI file's text, process-free: everything else stays
// byte for byte, the file's own line ends too. WritePrivateProfileString
// rewrote the whole file under Wine - blank lines dropped, a comment with "="
// joined (2026-10-24, ReplaceLogic.h InsertSectionLines) - so the plugin
// writes the file itself.

#include <cstring>

#include "ReplaceLogic.h"
#include "Types.h"

namespace walkers {

namespace ini_detail {

inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; }

// Whether text[from, to) is `key`, the case aside.
inline bool SameKey(const char* text, UInt32 from, UInt32 to, const char* key) {
	const UInt32 length = static_cast<UInt32>(std::strlen(key));
	if (to - from != length) {
		return false;
	}
	for (UInt32 i = 0; i < length; ++i) {
		if (Lower(text[from + i]) != Lower(key[i])) {
			return false;
		}
	}
	return true;
}

}  // namespace ini_detail

// `text` (`length` bytes) with "key=value" set in [section] - the value of
// the first such key replaced in place (what stands before the value and
// behind it is kept), or, with no such key, a new line behind the
// section's last key line (behind the comments right under its header when
// it has no key), or, with no such section, the section added at the file's
// end. The file's own line end, a carriage return and line feed when it has
// none. Returns the bytes written to `out`; 0 when they do not fit.
inline UInt32 SetIniKey(const char* text, UInt32 length, const char* section, const char* key, const char* value,
                        char* out, UInt32 capacity) {
	bool feedsOnly = false;
	for (UInt32 i = 0; i < length; ++i) {
		if (text[i] == '\n') {
			feedsOnly = i == 0 || text[i - 1] != '\r';
			break;
		}
	}
	const char* newline = feedsOnly ? "\n" : "\r\n";

	bool inSection = false;
	bool sectionSeen = false;
	bool keySeen = false;
	bool blankSeen = false;
	bool found = false;
	UInt32 insertAt = length;
	UInt32 replaceFrom = 0;
	UInt32 replaceTo = 0;
	for (UInt32 begin = 0; begin < length;) {
		UInt32 end = begin;
		while (end < length && text[end] != '\n') {
			++end;
		}
		const UInt32 next = end < length ? end + 1 : length;
		UInt32 from = begin;
		UInt32 to = end;
		TrimLine(text, &from, &to);
		if (!inSection) {
			if (!sectionSeen && IsSectionHeader(text, from, to, section)) {
				inSection = true;
				sectionSeen = true;
				insertAt = next;
			}
		} else if (from < to && text[from] == '[') {
			break;
		} else if (from == to) {
			blankSeen = true;
		} else if (text[from] == ';' || text[from] == '#') {
			if (!keySeen && !blankSeen) {
				insertAt = next;
			}
		} else {
			keySeen = true;
			insertAt = next;
			UInt32 equals = from;
			while (equals < to && text[equals] != '=') {
				++equals;
			}
			if (equals < to) {
				UInt32 nameTo = equals;
				while (nameTo > from && (text[nameTo - 1] == ' ' || text[nameTo - 1] == '\t')) {
					--nameTo;
				}
				if (ini_detail::SameKey(text, from, nameTo, key)) {
					found = true;
					replaceFrom = equals + 1;
					while (replaceFrom < to && (text[replaceFrom] == ' ' || text[replaceFrom] == '\t')) {
						++replaceFrom;
					}
					replaceTo = to;
					break;
				}
			}
		}
		begin = next;
	}

	UInt32 used = 0;
	bool fits = true;
	const auto put = [&](const char* bytes, UInt32 count) {
		if (!fits || used + count > capacity) {
			fits = false;
			return;
		}
		std::memcpy(out + used, bytes, count);
		used += count;
	};
	const UInt32 newlineLength = static_cast<UInt32>(std::strlen(newline));
	const UInt32 valueLength = static_cast<UInt32>(std::strlen(value));
	const UInt32 keyLength = static_cast<UInt32>(std::strlen(key));
	if (found) {
		put(text, replaceFrom);
		put(value, valueLength);
		put(text + replaceTo, length - replaceTo);
		return fits ? used : 0;
	}
	put(text, insertAt);
	if (insertAt == length && length > 0 && text[length - 1] != '\n') {
		put(newline, newlineLength);
	}
	if (!sectionSeen) {
		put("[", 1);
		put(section, static_cast<UInt32>(std::strlen(section)));
		put("]", 1);
		put(newline, newlineLength);
	}
	put(key, keyLength);
	put("=", 1);
	put(value, valueLength);
	put(newline, newlineLength);
	put(text + insertAt, length - insertAt);
	return fits ? used : 0;
}

}  // namespace walkers
