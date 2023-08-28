#include "framework.h"
#include "ToastSystem.h"
#include "ComicZipViewerFrame.h"
#include "tick.h"

class ToastSystem::Toast
{
public:
	Toast(const wxString& text);
	Toast(const Toast& rhs) = default;
	Toast(Toast&& rhs) noexcept;
	Toast& operator=(const Toast&) = default;
	Toast& operator=(Toast&&) noexcept = default;

	void Update(uint64_t delta);
	void Render(const ComPtr<ID2D1DeviceContext1>& context);
	bool IsFinished() const;
	void SetTextLayout(const ComPtr<IDWriteTextFormat>& textFormat);
	
private:
	uint64_t m_duration; // fixed float point  use as / 4096
	wxString m_message;
	ComPtr<IDWriteTextLayout> m_textLayout;
	ComPtr<IDWriteTextFormat> m_textFormat;
};

ToastSystem::Toast::Toast(const wxString& text)
: m_duration(0)
, m_message(text)
{
	
}

ToastSystem::Toast::Toast(Toast&& rhs) noexcept
: m_duration(rhs.m_duration)
, m_message(std::move(rhs.m_message))
, m_textLayout(std::move(rhs.m_textLayout))
{
	rhs.m_duration = 0;
}

void ToastSystem::Toast::Update(uint64_t delta)
{
	m_duration += delta;
}

void ToastSystem::Toast::Render(const ComPtr<ID2D1DeviceContext1>& context)
{
}

void ToastSystem::Toast::SetTextLayout(const ComPtr<IDWriteTextFormat>& textFormat)
{
	textFormat->Get
	m_textFormat = textFormat;
	m_textLayout.Reset();
}

bool ToastSystem::Toast::IsFinished() const
{
	return m_duration >= 1024 * 10; // 10s
}

ToastSystem::ToastSystem(ComicZipViewerFrame* pFrame)
	: m_isRunning(false)
	, m_pTargetWindow(pFrame)
{
	HRESULT hr;
	auto font = m_pTargetWindow->GetFont();
	auto fontFamilityName = font.GetFaceName();
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED , __uuidof( m_dwFactory ) , &m_dwFactory);
	hr = m_dwFactory->CreateTextFormat(fontFamilityName.wx_str(), nullptr , DWRITE_FONT_WEIGHT_BOLD , DWRITE_FONT_STYLE_NORMAL , DWRITE_FONT_STRETCH_NORMAL , 15.f , L"ko-kr" , &m_dwTextFormat);
	assert(SUCCEEDED(hr));
}

ToastSystem::~ToastSystem()
{
}

void ToastSystem::ShowMessage(const wxString& msg)
{
	// TODO: Add Message
	m_messages.emplace_back(msg);
	m_messages.back().SetTextLayout(m_)
	if( !m_isRunning )
	{
		CallAfter(&ToastSystem::ShowToast);
	}
}

void ToastSystem::Render(const ComPtr<ID2D1DeviceContext1>& context)
{
	for(auto & it: m_messages )
	{
		it.Render(context);
	}
}

void ToastSystem::SetFont(const wxFont& font)
{
	// TODO: Recreate TextFormat and Toast's surface
	m_font = font;

}

void ToastSystem::ShowToast()
{
	m_isRunning = true;
	auto prevTick  = GetTick();
	auto frequency = GetTickFrequency();
	while( !m_messages.empty() && !m_pTargetWindow->IsBeingDeleted())
	{
		RemoveFinishedToasts();

		int64_t current = GetTick();
		const int64_t diff = current - prevTick;
		prevTick = current;
		uint64_t delta = ( ( diff * 4096ull ) / frequency );
		for(auto& it: m_messages)
		{
			it.Update(delta);
		}

		m_pTargetWindow->TryRender();
		wxYieldIfNeeded();
	}

	m_isRunning = false;
}

void ToastSystem::RemoveFinishedToasts()
{
	auto it = m_messages.begin();
	while(it != m_messages.end() )
	{
		if(it->IsFinished() )
		{
			it = m_messages.erase(it);
		}
		else
		{
			++it;
		}
	}
}
