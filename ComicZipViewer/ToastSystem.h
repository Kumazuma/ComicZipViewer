#pragma once
#include "framework.h"
#include "UpdateSystem.h"
#include <d2d1_3.h>
#include <dwrite_3.h>

class ComicZipViewerFrame;
class ToastSystem: public wxEvtHandler
{
	class Toast;
	class Worker;
public:
	ToastSystem(ComicZipViewerFrame* pFrame);
	~ToastSystem() override;
	void AddMessage(const wxString& msg, wxWindowID id, bool preventSameMessage);
	void Render(const ComPtr<ID2D1DeviceContext1>& context);
	void SetFont(const wxFont& font);
	void SetClientSize(float dpi, const wxSize& size);
	void SetColor(const ComPtr<ID2D1DeviceContext1>& context, const D2D1::ColorF& text, const D2D1::ColorF& backgrund);
protected:
	void RemoveFinishedToasts();
	void ResetResource();
private:
	bool m_isRunning;
	ComicZipViewerFrame* m_pTargetWindow;
	wxFont m_font;
	std::vector<Toast> m_messages;
	ComPtr<IDWriteFactory> m_dwFactory;
	ComPtr<IDWriteTextFormat> m_dwTextFormat;
	ComPtr<ID2D1SolidColorBrush> m_textBrush;
	ComPtr<ID2D1SolidColorBrush> m_backgroundBrush;
	float m_dpi;
	float m_width;
	float m_height;
	Worker* m_pWorker;
};
