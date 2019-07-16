#pragma once
BOOLEAN IsPatternMatch(const PWCHAR pExpression, const PWCHAR pName, BOOLEAN IgnoreCase);
wchar_t *my_wcsrstr(const wchar_t *str, const wchar_t *sub_str);
VOID UnicodeToChar(PUNICODE_STRING dst, char *src);
VOID WcharToChar(PWCHAR src, PCHAR dst);
VOID CharToWchar(PCHAR src, PWCHAR dst);
BOOLEAN IsShortNamePath(WCHAR * wszFileName);
NTSTATUS
MyRtlVolumeDeviceToDosName(
	IN PUNICODE_STRING DeviceName,
	OUT PUNICODE_STRING DosName
);
BOOLEAN NTAPI GetNTLinkName(IN WCHAR * wszNTName, OUT WCHAR * wszFileName);
NTSTATUS QuerySymbolicLink(
	IN PUNICODE_STRING SymbolicLinkName,
	OUT PUNICODE_STRING LinkTarget
);

BOOLEAN QueryVolumeName(WCHAR ch, WCHAR * name, USHORT size);
BOOLEAN NTAPI GetNtDeviceName(IN WCHAR * filename, OUT WCHAR * ntname);