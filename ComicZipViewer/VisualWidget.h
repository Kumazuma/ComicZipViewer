#pragma once
#include <wx/wx.h>
#undef GetHwnd
#include <d2d1_3.h>

class VisualWidget
{
public:
	VisualWidget();
	virtual void Draw(ID2D1DeviceContext* pContext) = 0;
	virtual D2D1_RECT_F GetBestSize() = 0;
	void SetRect(const D2D1_RECT_F& rect);
	const D2D1_RECT_F& GetRect() const;
private:
	D2D1_RECT_F m_rect;
};
