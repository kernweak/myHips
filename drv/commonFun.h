#pragma once
BOOLEAN IsPatternMatch(const PWCHAR pExpression, const PWCHAR pName, BOOLEAN IgnoreCase);
wchar_t *my_wcsrstr(const wchar_t *str, const wchar_t *sub_str);
VOID UnicodeToChar(PUNICODE_STRING dst, char *src);
VOID WcharToChar(PWCHAR src, PCHAR dst);
VOID CharToWchar(PCHAR src, PWCHAR dst);