#include "framework.h"
#include "ToastSystem.h"
#include "tick.h"

class ToastSystem::Toast
{
public:
	Toast();
private:
	uint64_t m_duration; // fixed float point  use as / 4096
	wxString m_message;
};

ToastSystem::ToastSystem(wxFrame* pFrame)
	: m_isRunning(false)
	, m_pTargetWindow(pFrame)
{
	HRESULT hr;
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED , __uuidof( m_dwFactory ) , &m_dwFactory);
	hr = m_dwFactory->CreateTextFormat(L"gulim" , nullptr , DWRITE_FONT_WEIGHT_BOLD , DWRITE_FONT_STYLE_NORMAL , DWRITE_FONT_STRETCH_NORMAL , 15.f , L"ko-kr" , &m_dwTextFormat);
	assert(SUCCEEDED(hr));
}

ToastSystem::~ToastSystem()
{
}

void ToastSystem::ShowMessage(const wxString& msg)
{
	// TODO: Add Message
	if( !m_isRunning )
	{
		CallAfter(&ToastSystem::ShowToast);
	}
}

void ToastSystem::Render(const ComPtr<ID2D1DeviceContext1>& context)
{
	// TODO: Render messages
}

void ToastSystem::SetFont(const wxFont& font)
{
	// TODO: Recreate TextFormat and Toast's surface
}

void ToastSystem::ShowToast()
{
	m_isRunning = true;
	auto prevTick  = GetTick();
	auto frequency = GetTickFrequency();
	while( !m_messages.empty() && !m_pTargetWindow->IsBeingDeleted())
	{
		int64_t current = GetTick();
		const int64_t diff = current - prevTick;
		prevTick = current;
		uint64_t delta = ( ( diff * 4096ull ) / frequency );
		// TODO: Update time of each toast messages;


		// TODO: Rendering toast message;

		// TODO: Remove toast in list, if it is end of life.
		wxYieldIfNeeded();
	}

	m_isRunning = false;
}
