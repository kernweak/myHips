
#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <strsafe.h>
#include <Ntstrsafe.h >
#include "FQDrv.h"
#include "commonFun.h"

#define  MAXPATHLEN         512    
#define MAX_PATH 256
//UNICODE_STRINGz 转换为 CHAR*
//输入 UNICODE_STRING 的指针，输出窄字符串，BUFFER 需要已经分配好空间
VOID UnicodeToChar(PUNICODE_STRING dst, char *src)
{
	ANSI_STRING string;
	RtlUnicodeStringToAnsiString(&string, dst, TRUE);
	strcpy(src, string.Buffer);
	RtlFreeAnsiString(&string);
}

//WCHAR*转换为 CHAR*
//输入宽字符串首地址，输出窄字符串，BUFFER 需要已经分配好空间
VOID WcharToChar(PWCHAR src, PCHAR dst)
{
	UNICODE_STRING uString;
	ANSI_STRING aString;
	RtlInitUnicodeString(&uString, src);
	RtlUnicodeStringToAnsiString(&aString, &uString, TRUE);
	strcpy(dst, aString.Buffer);
	RtlFreeAnsiString(&aString);
}

//CHAR*转 WCHAR*
//输入窄字符串首地址，输出宽字符串，BUFFER 需要已经分配好空间
VOID CharToWchar(PCHAR src, PWCHAR dst)
{
	UNICODE_STRING uString;
	ANSI_STRING aString;
	RtlInitAnsiString(&aString, src);
	RtlAnsiStringToUnicodeString(&uString, &aString, TRUE);
	wcscpy(dst, uString.Buffer);
	RtlFreeUnicodeString(&uString);
}



wchar_t *my_wcsrstr(const wchar_t *str, const wchar_t *sub_str)
{
	const wchar_t	*p = NULL;
	const wchar_t	*q = NULL;
	wchar_t			*last = NULL;

	if (NULL == str || NULL == sub_str)
	{
		return NULL;
	}

	for (; *str; str++)
	{
		p = str, q = sub_str;
		while (*q && *p)
		{
			if (*q != *p)
			{
				break;
			}
			p++, q++;
		}
		if (*q == 0)
		{
			last = (wchar_t *)str;
			str = p - 1;
		}
	}
	return last;
}


BOOLEAN IsPatternMatch(const PWCHAR pExpression, const PWCHAR pName, BOOLEAN IgnoreCase)
{
	if (NULL == pExpression || wcslen(pExpression) == 0)
	{
		return TRUE;
	}
	if (pName == NULL || wcslen(pName) == 0)
	{
		return FALSE;
	}
	if (L'$' == pExpression[0])
	{
		UNICODE_STRING			uNewExpression = { 0 };
		WCHAR					buffer[MAXPATHLEN] = { 0 };
		UNICODE_STRING			uExpression = { 0 };
		UNICODE_STRING			uName = { 0 };

		RtlInitUnicodeString(&uName, pName);
		RtlInitUnicodeString(&uExpression, &pExpression[1]);

		if (FsRtlIsNameInExpression(&uExpression, &uName, IgnoreCase, NULL))
		{
			WCHAR* last = my_wcsrstr(pExpression, L"\\*");
			if (NULL == last)
			{
				return FALSE;
			}
			StringCbCopyNW(buffer, MAXPATHLEN * sizeof(WCHAR), &pExpression[1], (last - pExpression + 1) * sizeof(WCHAR));
			StringCbCatW(buffer, MAXPATHLEN * sizeof(WCHAR), last);
			RtlInitUnicodeString(&uNewExpression, buffer);
			if (FsRtlIsNameInExpression(&uNewExpression, &uName, IgnoreCase, NULL))
			{
				return FALSE;
			}
			else
			{
				return TRUE;
			}
		}
		else
		{
			return FALSE;
		}

	}
	else
	{
		UNICODE_STRING			uExpression = { 0 };
		UNICODE_STRING			uName = { 0 };

		RtlInitUnicodeString(&uExpression, pExpression);
		RtlInitUnicodeString(&uName, pName);

		return FsRtlIsNameInExpression(&uExpression, &uName, IgnoreCase, NULL);
	}
}

BOOLEAN IsShortNamePath(WCHAR * wszFileName)
{
	WCHAR *p = wszFileName;

	while (*p != L'\0')
	{
		if (*p == L'~')
		{
			return TRUE;
		}
		p++;
	}

	return FALSE;
}

NTSTATUS QuerySymbolicLink(
	IN PUNICODE_STRING SymbolicLinkName,
	OUT PUNICODE_STRING LinkTarget
)
{
	OBJECT_ATTRIBUTES	oa = { 0 };
	NTSTATUS			status = 0;
	HANDLE				handle = NULL;

	InitializeObjectAttributes(
		&oa,
		SymbolicLinkName,
		OBJ_CASE_INSENSITIVE,
		0,
		0);

	status = ZwOpenSymbolicLinkObject(&handle, GENERIC_READ, &oa);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	LinkTarget->MaximumLength = MAX_PATH * sizeof(WCHAR);
	LinkTarget->Length = 0;
	LinkTarget->Buffer = ExAllocatePoolWithTag(PagedPool, LinkTarget->MaximumLength, 'SOD');
	if (!LinkTarget->Buffer)
	{
		ZwClose(handle);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(LinkTarget->Buffer, LinkTarget->MaximumLength);

	status = ZwQuerySymbolicLinkObject(handle, LinkTarget, NULL);
	ZwClose(handle);

	if (!NT_SUCCESS(status))
	{
		ExFreePool(LinkTarget->Buffer);
	}

	return status;
}

NTSTATUS
MyRtlVolumeDeviceToDosName(
	IN PUNICODE_STRING DeviceName,
	OUT PUNICODE_STRING DosName
)

/*++
Routine Description:
This routine returns a valid DOS path for the given device object.
This caller of this routine must call ExFreePool on DosName->Buffer
when it is no longer needed.
Arguments:
VolumeDeviceObject - Supplies the volume device object.
DosName - Returns the DOS name for the volume
Return Value:
NTSTATUS
--*/

{
	NTSTATUS				status = 0;
	UNICODE_STRING			driveLetterName = { 0 };
	WCHAR					driveLetterNameBuf[128] = { 0 };
	WCHAR					c = L'\0';
	WCHAR					DriLetter[3] = { 0 };
	UNICODE_STRING			linkTarget = { 0 };

	for (c = L'A'; c <= L'Z'; c++)
	{
		RtlInitEmptyUnicodeString(&driveLetterName, driveLetterNameBuf, sizeof(driveLetterNameBuf));
		RtlAppendUnicodeToString(&driveLetterName, L"\\??\\");
		DriLetter[0] = c;
		DriLetter[1] = L':';
		DriLetter[2] = 0;
		RtlAppendUnicodeToString(&driveLetterName, DriLetter);
		status = QuerySymbolicLink(&driveLetterName, &linkTarget);
		if (!NT_SUCCESS(status))
		{
			continue;
		}
		if (RtlEqualUnicodeString(&linkTarget, DeviceName, TRUE))
		{
			ExFreePool(linkTarget.Buffer);
			break;
		}
		ExFreePool(linkTarget.Buffer);
	}
	if (c <= L'Z')
	{
		DosName->Buffer = ExAllocatePoolWithTag(PagedPool, 3 * sizeof(WCHAR), 'SOD');
		if (!DosName->Buffer)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		DosName->MaximumLength = 6;
		DosName->Length = 4;
		*DosName->Buffer = c;
		*(DosName->Buffer + 1) = ':';
		*(DosName->Buffer + 2) = 0;

		return STATUS_SUCCESS;
	}

	return status;
}

//c:\\windows\\hi.txt<--\\device\\harddiskvolume1\\windows\\hi.txt
BOOLEAN NTAPI GetNTLinkName(IN WCHAR * wszNTName, OUT WCHAR * wszFileName)
{
	UNICODE_STRING		ustrFileName = { 0 };
	UNICODE_STRING		ustrDosName = { 0 };
	UNICODE_STRING		ustrDeviceName = { 0 };

	WCHAR				*pPath = NULL;
	ULONG				i = 0;
	ULONG				ulSepNum = 0;


	if (wszFileName == NULL ||
		wszNTName == NULL ||
		_wcsnicmp(wszNTName, L"\\device\\harddiskvolume", wcslen(L"\\device\\harddiskvolume")) != 0)
	{
		return FALSE;
	}

	ustrFileName.Buffer = wszFileName;
	ustrFileName.Length = 0;
	ustrFileName.MaximumLength = sizeof(WCHAR)*MAX_PATH;

	while (wszNTName[i] != L'\0')
	{

		if (wszNTName[i] == L'\0')
		{
			break;
		}
		if (wszNTName[i] == L'\\')
		{
			ulSepNum++;
		}
		if (ulSepNum == 3)
		{
			wszNTName[i] = UNICODE_NULL;
			pPath = &wszNTName[i + 1];
			break;
		}
		i++;
	}
	if (pPath == NULL)
	{
		return FALSE;
	}
	RtlInitUnicodeString(&ustrDeviceName, wszNTName);
	if (!NT_SUCCESS(MyRtlVolumeDeviceToDosName(&ustrDeviceName, &ustrDosName)))
	{
		return FALSE;
	}
	RtlCopyUnicodeString(&ustrFileName, &ustrDosName);
	RtlAppendUnicodeToString(&ustrFileName, L"\\");
	RtlAppendUnicodeToString(&ustrFileName, pPath);
	ExFreePool(ustrDosName.Buffer);
	return TRUE;
}

BOOLEAN QueryVolumeName(WCHAR ch, WCHAR * name, USHORT size)
{
	WCHAR szVolume[7] = L"\\??\\C:";
	UNICODE_STRING LinkName;
	UNICODE_STRING VolName;
	UNICODE_STRING ustrTarget;
	NTSTATUS ntStatus = 0;

	RtlInitUnicodeString(&LinkName, szVolume);

	szVolume[4] = ch;
	ustrTarget.Buffer = name;
	ustrTarget.Length = 0;
	ustrTarget.MaximumLength = size;

	ntStatus = QuerySymbolicLink(&LinkName, &VolName);
	if (NT_SUCCESS(ntStatus))
	{
		RtlCopyUnicodeString(&ustrTarget, &VolName);
		ExFreePool(VolName.Buffer);
	}
	return NT_SUCCESS(ntStatus);

}
//\\??\\c:\\windows\\hi.txt-->\\device\\harddiskvolume1\\windows\\hi.txt
BOOLEAN NTAPI GetNtDeviceName(IN WCHAR * filename, OUT WCHAR * ntname)
{
	UNICODE_STRING uVolName = { 0,0,0 };
	WCHAR volName[MAX_PATH] = L"";
	WCHAR tmpName[MAX_PATH] = L"";
	WCHAR chVol = L'\0';
	WCHAR * pPath = NULL;
	int i = 0;


	RtlStringCbCopyW(tmpName, MAX_PATH * sizeof(WCHAR), filename);

	for (i = 1; i < MAX_PATH - 1; i++)
	{
		if (tmpName[i] == L':')
		{
			pPath = &tmpName[(i + 1) % MAX_PATH];
			chVol = tmpName[i - 1];
			break;
		}
	}

	if (pPath == NULL)
	{
		return FALSE;
	}

	if (chVol == L'?')
	{
		uVolName.Length = 0;
		uVolName.MaximumLength = MAX_PATH * sizeof(WCHAR);
		uVolName.Buffer = ntname;
		RtlAppendUnicodeToString(&uVolName, L"\\Device\\HarddiskVolume?");
		RtlAppendUnicodeToString(&uVolName, pPath);
		return TRUE;
	}
	else if (QueryVolumeName(chVol, volName, MAX_PATH * sizeof(WCHAR)))
	{
		uVolName.Length = 0;
		uVolName.MaximumLength = MAX_PATH * sizeof(WCHAR);
		uVolName.Buffer = ntname;
		RtlAppendUnicodeToString(&uVolName, volName);
		RtlAppendUnicodeToString(&uVolName, pPath);
		return TRUE;
	}

	return FALSE;
}
