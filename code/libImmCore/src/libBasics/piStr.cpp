//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#ifdef WINDOWS
#include <windows.h>
#endif
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>

#ifdef ANDROID
#include "android/piCStr.h"
#include <stdio.h>
#include <wchar.h>
#include <string>
#include <string>
#include <cstring>
#endif

#include "piTypes.h"

namespace ImmCore
{

	char *pistrtok(char *strToken, const char *strDelimit, char **context)
	{
#ifdef WINDOWS
#if _MSC_VER<1300
		return strtok(strToken, strDelimit);
#else
		return strtok_s(strToken, strDelimit, context);
#endif
#else
		return strtok(strToken, strDelimit);
#endif

#if 0
		const char *sep;
		char *start;
		char *string;


		if (_string != 0) string = _string; else string = *temp;

		if (_string == 0 && *temp == 0) return 0;

		start = string;

		while ((*string) != 0)
		{
			sep = _sep;
			while ((*sep) != 0)
			{
				if ((*string) == (*sep))
				{
					*string = 0;
					*temp = string + 1;
					return(start);
				}
				sep++;
			}
			string++;
		}
		*temp = 0;
		return(start);
#endif

	}

	int pistrlen(const char *str)
	{
		return (int)strlen(str);
	}

	int pistrcmp(const char *stra, const char *strb)
	{
		return strcmp(stra, strb);
	}

	void pistrncpy(char *strDest, size_t sizeInBytes, const char *strSource, size_t count)
	{
#ifdef WINDOWS
#if _MSC_VER<1300
		strncpy(strDest, strSource, sizeInBytes);
#else
		if (count == 0) count = _TRUNCATE;
		strncpy_s(strDest, sizeInBytes, strSource, count);
#endif
#else
		strncpy(strDest, strSource, sizeInBytes);
#endif
	}

	void pistrcpy(char *strDestination, size_t sizeInBytes, const char *strSource)
	{
#ifdef WINDOWS
#if _MSC_VER<1300   
		strcpy(strDestination, strSource);
#else
		strcpy_s(strDestination, sizeInBytes, strSource);
#endif
#else
		strcpy(strDestination, strSource);
#endif
	}


	void pistrncat(char *strDest, size_t bufferSizeInBytes, const char *strSource, size_t count)
	{
#ifdef WINDOWS
#if _MSC_VER<1300   
		strncat(strDest, strSource, bufferSizeInBytes);
#else
		if (count == 0) count = _TRUNCATE;
		strncat_s(strDest, bufferSizeInBytes, strSource, count);
#endif
#else
		strncat(strDest, strSource, bufferSizeInBytes);
#endif
	}

	char *pistrconcat(const char *stra, const char *strb)
	{
		const int lena = (int)strlen(stra);
		const int lenb = (int)strlen(strb);
		char *ptr = (char*)malloc(lena + lenb + 1);
		if (!ptr) return NULL;
		strcpy(ptr, stra);
		strcat(ptr, strb);
		return ptr;
	}


	int pisprintf(char *buffer, size_t sizeOfBuffer, const char *format, ...)
	{
		int res;

		va_list marker;

		va_start(marker, format);

#ifdef WINDOWS
#if _MSC_VER<1300   
		res = vsprintf(buffer, format, marker);
#else
		res = vsprintf_s(buffer, sizeOfBuffer, format, marker);
#endif
#else
		res = vsprintf(buffer, format, marker);
#endif

		va_end(marker);

		return res;
	}

	int pivsprintf(char *buffer, size_t sizeInBytes, const char *format, va_list arglist)
	{
		int res;

#ifdef WINDOWS
#if _MSC_VER<1300   
		res = vsprintf(buffer, format, arglist);
#else
		res = vsprintf_s(buffer, sizeInBytes, format, arglist);
#endif
#else
		res = vsprintf(buffer, format, arglist);
#endif

		return res;
	}

	wchar_t *piwstrtok(wchar_t *strToken, const wchar_t *strDelimit, wchar_t **context)
	{
#ifdef WINDOWS
		return wcstok_s(strToken, strDelimit, context);
#else
		return wcstok(strToken, strDelimit, context);
#endif
	}

	int piwsprintf(wchar_t *buffer, size_t sizeOfBuffer, const wchar_t *format, ...)
	{
		va_list args;
		va_start(args, format);
#ifdef WINDOWS
		//const int res = vswprintf_s( buffer, sizeOfBuffer, format, args );
        const int res = _vsnwprintf(buffer, sizeOfBuffer, format, args);
#elif ANDROID
		// Apparently MSVCRT has %s and %ls backwards; lets fix the format string
		std::wstring formatStr(format);
		replaceAll(formatStr, L"%s", L"%ls");

		const int res = vswprintf( buffer, sizeOfBuffer, formatStr.c_str(), args );
#else
		const int res = vswprintf(buffer, sizeOfBuffer, format, args);
#endif 
		va_end(args);

		return res;
	}

	int pivwsprintf(wchar_t *buffer, size_t sizeInBytes, const wchar_t *format, va_list arglist)
	{
		int res;

#ifdef WINDOWS
#if _MSC_VER<1300   
		res = vsprintf(buffer, format, arglist);
#else
		//res = vswprintf_s(buffer, sizeInBytes, format, arglist);
        res = _vsnwprintf(buffer, sizeInBytes, format, arglist);
        
#endif
#elif ANDROID
		// Apparently MSVCRT has %s and %ls backwards; lets fix the format string
		std::wstring formatStr(format);
		replaceAll(formatStr, L"%s", L"%ls");

		res = vswprintf( buffer, sizeInBytes, format, arglist );
#else
		res = vswprintf(buffer, sizeInBytes, format, arglist);
#endif

		return res;

	}

	int pivscwprintf(const wchar_t *format, va_list arglist)
	{
#ifdef WINDOWS
		return _vscwprintf(format, arglist);
#elif ANDROID
		// Unlike vsnprintf(), vswprintf() does not tell you how many
		// characters would have been written if there was space enough in
		// the buffer - it just reports an error when there is not enough
		// space. Assume a moderately large machine so kilobytes of wchar_t
		// on the stack is not a problem.
		int buf_size = 1024;
		while (buf_size < 1024 * 1024)
		{
			va_list args;
			va_copy(args, arglist);
			wchar_t buffer[buf_size];
			int fmt_size = vswprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), format, args);
			if (fmt_size >= 0)
				return fmt_size;
			buf_size *= 2;
		}
		return -1;
#else
		int buf_size = 1024;
		while (buf_size < 1024 * 1024)
		{
			va_list args;
			va_copy(args, arglist);
			wchar_t buffer[buf_size];
			int fmt_size = vswprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), format, args);
			if (fmt_size >= 0)
				return fmt_size;
			buf_size *= 2;
		}
		return -1;
#endif
	}

	void piwstrcat(wchar_t *strDest, size_t bufferSizeInBytes, const wchar_t *strSource)
	{
#ifdef WINDOWS
		wcscat_s(strDest, bufferSizeInBytes, strSource);
#elif ANDROID
		wcscat( strDest, strSource );
#else
		(void)bufferSizeInBytes;
		wcscat(strDest, strSource);
#endif
	}

	const wchar_t *piwcsrchr(const wchar_t *str, wchar_t c)
	{
#if defined(WINDOWS) || defined(ANDROID)
		return wcsrchr(str, c);
#else
		return wcsrchr(str, c);
#endif
	}

	void piwstrcpy(wchar_t *strDestination, size_t sizeInBytes, const wchar_t *strSource)
	{
#if defined(WINDOWS) || defined(ANDROID)
		wcscpy(strDestination, strSource);
#else
		(void)sizeInBytes;
		wcscpy(strDestination, strSource);
#endif
	}

	int piwstrlen(const wchar_t *buffer)
	{
#if defined(WINDOWS) || defined(ANDROID)
		return (int)wcslen(buffer);
#else
		return (int)wcslen(buffer);
#endif
	}

	int piwstrcmp(const wchar_t *a, const wchar_t *b)
	{
#if defined(WINDOWS) || defined(ANDROID)
		return wcscmp(a, b);
#else
		return wcscmp(a, b);
#endif
	}

	void piwstrncpy(wchar_t *strDest, size_t sizeInBytes, const wchar_t *strSource, size_t count)
	{
#if defined(WINDOWS) || defined(ANDROID)
		if (count == 0) count = piwstrlen(strSource);
		wcsncpy(strDest, strSource, count);
#else
		(void)sizeInBytes;
		if (count == 0) count = (size_t)piwstrlen(strSource);
		wcsncpy(strDest, strSource, count);
#endif
	}


	void piwstrncat(wchar_t *strDest, size_t bufferSizeInBytes, const wchar_t *strSource, size_t count)
	{
#if defined(WINDOWS) || defined(ANDROID)
		wcsncat(strDest, strSource, count);
#else
		(void)bufferSizeInBytes;
		wcsncat(strDest, strSource, count);
#endif
	}

	int piwstr2int(const wchar_t *str, bool *status)
	{
#if defined(WINDOWS) || defined(ANDROID)
		//int res = _wtoi( str );
		wchar_t *ec;
		int res = wcstoul(str, &ec, 10);
		if (ec[0] != 0)
			*status = false;
		else
			*status = true;
#else
		wchar_t *ec;
		int res = wcstol(str, &ec, 10);
		if (ec[0] != 0)
			*status = false;
		else
			*status = true;
#endif
		return res;
	}


	uint64_t piwstr2uint64(const wchar_t *str, bool *status)
	{
#if defined(WINDOWS) || defined(ANDROID)
		//int res = _wtoi( str );
		wchar_t *ec;
		uint64_t res = wcstoull(str, &ec, 10);
		if (ec[0] != 0)
			*status = false;
		else
			*status = true;
#else
		wchar_t *ec;
		uint64_t res = wcstoull(str, &ec, 10);
		if (ec[0] != 0)
			*status = false;
		else
			*status = true;
#endif
		return res;
	}

    void piuint642wstr(wchar_t *dst, uint64_t n)
    {
        const wchar_t *h = L"0123456789abcdef";
        dst[0] = h[(n >> 60) & 0xf];
        dst[1] = h[(n >> 56) & 0xf];
        dst[2] = h[(n >> 52) & 0xf];
        dst[3] = h[(n >> 48) & 0xf];
        dst[4] = h[(n >> 44) & 0xf];
        dst[5] = h[(n >> 40) & 0xf];
        dst[6] = h[(n >> 36) & 0xf];
        dst[7] = h[(n >> 32) & 0xf];
        dst[8] = h[(n >> 28) & 0xf];
        dst[9] = h[(n >> 24) & 0xf];
        dst[10] = h[(n >> 20) & 0xf];
        dst[11] = h[(n >> 16) & 0xf];
        dst[12] = h[(n >> 12) & 0xf];
        dst[13] = h[(n >> 8) & 0xf];
        dst[14] = h[(n >> 4) & 0xf];
        dst[15] = h[(n >> 0) & 0xf];
        dst[16] = 0;
    }

	int piwstrcspn(const wchar_t *str, const wchar_t *charSet)
	{
#if defined(WINDOWS) || defined(ANDROID)
		return (int)wcscspn(str, charSet);
#else
		return (int)wcscspn(str, charSet);
#endif
	}

	wchar_t *piwstrdup(const wchar_t *str, bool *status)
	{
		if (str == 0) { status[0] = true; return 0; }
		// get length
		int len; for (len = 0; str[len]; len++) {}
		// alloc
		wchar_t *res = (wchar_t*)malloc((1 + len) * sizeof(wchar_t));
		if (!res) { status[0] = false; return 0; }
		// copy
		for (int i = 0; i < len; i++) res[i] = str[i]; res[len] = 0;

		*status = true;
		return res;
	}

    void piwstrtoupper( wchar_t *str)
    {
        int i = 0;
        while (str[i])
        {
            str[i] = towupper(str[i]);
            i++;
        }
    }

	bool piwstrequ(const wchar_t *a, const wchar_t *b)
	{
		if (a == 0 || b == 0) return false;

		for (int i = 0; ; i++)
		{
			if (a[i] != b[i]) return false;
			if (a[i] == 0) return true;
			if (b[i] == 0) return true;
		}
		return true;
	}

    bool piwstrequnocase(const wchar_t *a, const wchar_t *b)
    {
        if (a == 0 || b == 0) return false;

        for (int i = 0; ; i++)
        {
            if (towupper(a[i]) != towupper(b[i])) return false;
            if (a[i] == 0) return true;
            if (b[i] == 0) return true;
        }
        return true;
    }

    bool piwstrcontains(const wchar_t *str, const wchar_t *what)
    {
        return (wcsstr(str, what) != NULL);
    }


	wchar_t * pistr2ws(const char *ori)
	{
		const int len = (int)strlen(ori);

#ifdef WINDOWS
		int n = MultiByteToWideChar(CP_ACP, 0, ori, len, 0, 0);
		if (n < 1) return NULL;

		wchar_t *dst = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
		if (!dst) return 0;

		MultiByteToWideChar(CP_ACP, 0, ori, len, dst, n);
		dst[n] = 0;

		return dst;
#elif defined(ANDROID)
		size_t wlen = mbstowcs(0, ori, 0);
		if (wlen == (size_t)-1) return 0;
		wchar_t * dst = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
		if (!dst) return 0;
		mbstowcs(dst, ori, wlen + 1);
		return dst;
#else
		size_t wlen = mbstowcs(0, ori, 0);
		if (wlen == (size_t)-1) return 0;
		wchar_t * dst = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
		if (!dst) return 0;
		mbstowcs(dst, ori, wlen + 1);
		return dst;
#endif
	}

	bool pistr2ws(wchar_t *dst, const int dstLen, const char *ori)
	{
		const int len = (int)strlen(ori);

#ifdef WINDOWS
        int n = MultiByteToWideChar(CP_ACP, 0, ori, len, 0, 0);
        if (n < 1) return false;
        if( (n+1) > dstLen ) return false;

        MultiByteToWideChar(CP_ACP, 0, ori, len, dst, n);
        dst[n] = 0;

        return true;
#elif defined(ANDROID)
		mbstowcs(dst, ori, dstLen);
		return true;
#else
		mbstowcs(dst, ori, dstLen);
		return true;
#endif
	}


	char* piws2str(const wchar_t *ori)
	{
	    // determine size in multi-byte
		char tmp[MB_LEN_MAX];
		int numMultiBytes = 0;
		int wcLength = piwstrlen(ori);
		for(int i=0; i<wcLength; i++)
		{
            numMultiBytes += wctomb(tmp, ori[i]);
		}
		// allocate and convert
		char *res = (char*)malloc(numMultiBytes + 1);
		if (!res) return 0;
		int l = (int)wcstombs(res, ori, numMultiBytes);
		res[l] = 0;
		return res;
	}
}
