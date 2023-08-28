#pragma once
#include "framework.h"
#include <d2d1_3.h>
#include <dwrite_3.h>

class ComicZipViewerFrame;
class ToastSystem: public wxEvtHandler
{
	class Toast;
public:
	ToastSystem(ComicZipViewerFrame* pFrame);
	~ToastSystem() override;
	void ShowMessage(const wxString& msg);
	void Render(const ComPtr<ID2D1DeviceContext1>& rendering);
	void SetFont(const wxFont& font);

protected:
	void ShowToast();
	void RemoveFinishedToasts();
private:
	bool m_isRunning;
	ComicZipViewerFrame* m_pTargetWindow;
	wxFont m_font;
	std::vector<Toast> m_messages;
	ComPtr<IDWriteFactory> m_dwFactory;
	ComPtr<IDWriteTextFormat> m_dwTextFormat;
};
