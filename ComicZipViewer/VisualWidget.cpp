#include "framework.h"
#include "VisualWidget.h"

VisualWidget::VisualWidget()
	: m_rect{}
{
}

void VisualWidget::SetRect(const D2D1_RECT_F& rect)
{
	m_rect = rect;
}

const D2D1_RECT_F& VisualWidget::GetRect() const
{
	return m_rect;
}
