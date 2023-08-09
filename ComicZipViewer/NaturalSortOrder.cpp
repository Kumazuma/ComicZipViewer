#include "framework.h"
#include "NaturalSortOrder.h"

constexpr bool IsNumber(wchar_t ch)
{
	return L'0' <= ch && ch <= L'9';
}

uint32_t GetNumber(const wxString& str, size_t& offset)
{
	 uint32_t value = 0;
	while(offset != str.size() && IsNumber(str[offset]))
	{
		uint32_t radix = str[offset] - L'0';
		if((value != 0 && radix == 0) || radix != 0)
		{
			value *= 10;
			value += radix;
		}

		offset += 1;
	}

	return value;
}

bool NaturalSortOrder::operator()(const wxString& lhs, const wxString& rhs) const
{
#if wxUSE_UNICODE_WCHAR
	size_t lhsOffset = 0;
	size_t rhsOffset = 0;
	while(lhs.size() != lhsOffset && rhs.size() != rhsOffset)
	{
		const wchar_t lhsChar = lhs[lhsOffset];
		const wchar_t rhsChar = rhs[lhsOffset];
		const bool lhsIsNumber = IsNumber(lhsChar);
		const bool rhsIsNumber = IsNumber(rhsChar);
		// 만약 둘 다 숫자라면
		if(lhsIsNumber && rhsIsNumber)
		{
			uint32_t lhsNumber = GetNumber(lhs, lhsOffset);
			uint32_t rhsNumber = GetNumber(rhs, rhsOffset);

			if(lhsNumber < rhsNumber)
			{
				return true;
			}
			else if(lhsNumber > rhsNumber)
			{
				return false;
			}
				
		}
		else if(lhsIsNumber)
		{
			return true;
		}
		else if(rhsIsNumber)
		{
			return false;
		}

		if(lhsChar < rhsChar)
		{
			return true;
		}
		else if(lhsChar > rhsChar)
		{
			return false;
		}

		lhsOffset += 1;
		rhsOffset += 1;
	}

#else
#error "Not implemented yet!"
#endif

	return false;
}
