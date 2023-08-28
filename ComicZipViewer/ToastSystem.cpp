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
	void Render(float dpi, const ComPtr<ID2D1DeviceContext1>& context);
	bool IsFinished() const;
	void ResetResource(
		const ComPtr<IDWriteFactory>& dwFactory,
		const ComPtr<IDWriteTextFormat>& textFormat,
		float dpi,
		float maxWidth,
		float maxHeight);

	void SetColor(const ComPtr<ID2D1Brush>& textColorBrush, const ComPtr<ID2D1Brush>& backgroundBrush );
	const wxString& GetText() const;
private:
	uint64_t m_duration; // fixed float point  use as / 4096
	wxString m_message;
	ComPtr<ID2D1Brush> m_backgroundBrush;
	ComPtr<ID2D1Brush> m_textColorBrush;
	ComPtr<IDWriteTextLayout> m_textLayout;
	ComPtr<IDWriteTextFormat> m_textFormat;
	ComPtr<ID2D1Layer> m_d2dLayer;
	D2D1_SIZE_F m_size;
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
, m_backgroundBrush(std::move(rhs.m_backgroundBrush))
, m_textColorBrush(std::move(rhs.m_textColorBrush))
, m_d2dLayer(std::move(m_d2dLayer))
, m_textFormat(std::move(m_textFormat))
, m_size(rhs.m_size)
{
	rhs.m_duration = 0;
	rhs.m_size.height = 0.f;
	rhs.m_size.width = 0.f;
}

void ToastSystem::Toast::Update(uint64_t delta)
{
	m_duration += delta;
}

void ToastSystem::Toast::Render(float dpi, const ComPtr<ID2D1DeviceContext1>& context)
{
	if(!m_d2dLayer)
	{
		context->CreateLayer(&m_d2dLayer);
	}

	float alpha = 0.f;
	if(m_duration <= 4096ull / 4)
	{
		alpha = m_duration * (1.f / 4096.f);
	}
	else if(m_duration < 4096ull * 3 / 4)
	{
		alpha = 1.f;
	}
	else if(m_duration <= 4096ull * 1ull)
	{
		alpha = (4096ull - m_duration) * (1.f / 4096.f);
	}

	alpha *= 0.8f;
	float boxWidth = m_size.width + 15 * dpi;
	float boxHeight = m_size.height + 15 * dpi;
	boxWidth = std::max(boxWidth, 120.f * dpi);
	boxHeight = std::max(boxHeight, 30.f * dpi);
	auto rect = D2D1::RectF(
		boxWidth * -0.5f,
		boxHeight  * -0.5f,
		boxWidth * 0.5f,
		boxHeight  * 0.5f);
	context->PushLayer(D2D1::LayerParameters(rect, nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), alpha), m_d2dLayer.Get());
	

	context->FillRoundedRectangle(D2D1::RoundedRect(
		rect,
		15 * dpi,
		15 * dpi),
		m_backgroundBrush.Get());

	context->DrawTextLayout(
		D2D1::Point2F(m_size.width * -0.5f,
		m_size.height * -0.5f),
		m_textLayout.Get(),
		m_textColorBrush.Get()
	);

	context->PopLayer();
}

void ToastSystem::Toast::ResetResource(
	const ComPtr<IDWriteFactory>& dwFactory,
	const ComPtr<IDWriteTextFormat>& textFormat,
	float dpi,
	float maxWidth,
	float maxHeight)
{
	m_textFormat = textFormat;
	m_textLayout.Reset();
	dwFactory->CreateTextLayout(m_message.wx_str(), m_message.Length(), m_textFormat.Get(), maxWidth, maxHeight, &m_textLayout);

	DWRITE_TEXT_METRICS textMetrics;
	m_textLayout->GetMetrics(&textMetrics);
	m_size.width = textMetrics.width;
	m_size.height = textMetrics.height;
}

void ToastSystem::Toast::SetColor(const ComPtr<ID2D1Brush>& textColorBrush, const ComPtr<ID2D1Brush>& backgroundBrush)
{
	m_textColorBrush = textColorBrush;
	m_backgroundBrush = backgroundBrush;
}

const wxString& ToastSystem::Toast::GetText() const
{
	return m_message;
}

bool ToastSystem::Toast::IsFinished() const
{
	return m_duration >= 4096ull * 1; // 1s
}

ToastSystem::ToastSystem(ComicZipViewerFrame* pFrame)
: m_isRunning(false)
, m_pTargetWindow(pFrame)
, m_dpi(0.f)
{
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED , __uuidof( m_dwFactory ) , &m_dwFactory);
	
}

ToastSystem::~ToastSystem()
{
}

void ToastSystem::AddMessage(const wxString& msg, wxWindowID id, bool preventSameMessage)
{
	if(preventSameMessage)
	{
		for(auto& it: m_messages)
		{
			if(it.GetText() == msg)
			{
				return;
			}
		}
	}

	m_messages.emplace_back(msg);
	auto& it = m_messages.back();

	it.ResetResource(
		m_dwFactory,
		m_dwTextFormat,
		m_dpi,
		m_width,
		m_height
	);

	it.SetColor(m_textBrush, m_backgroundBrush);

	if( !m_isRunning )
	{
		CallAfter(&ToastSystem::ShowToast);
	}
}

void ToastSystem::Render(const ComPtr<ID2D1DeviceContext1>& context)
{
	D2D1_MATRIX_3X2_F oldTransform;
	D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Translation(m_width * 0.5f, m_height * 0.5f);
	context->GetTransform(&oldTransform);
	context->SetTransform(transform);
	for(auto & it: m_messages )
	{
		it.Render(m_dpi, context);
	}

	context->SetTransform(oldTransform);
}

void ToastSystem::SetFont(const wxFont& font)
{
	HRESULT hr;
	m_font = font;
	ResetResource();
}

void ToastSystem::SetClientSize(float dpi, const wxSize& size)
{
	m_dpi = dpi;
	m_width = size.GetWidth();
	m_height = size.GetHeight();
	ResetResource();
}

void ToastSystem::SetColor(const ComPtr<ID2D1DeviceContext1>& context, const D2D1::ColorF& text,
	const D2D1::ColorF& backgrund)
{
	context->CreateSolidColorBrush(text, &m_textBrush);
	context->CreateSolidColorBrush(backgrund, &m_backgroundBrush);
	for(auto& it: m_messages)
	{
		it.SetColor(m_textBrush, m_backgroundBrush);
	}
}

void ToastSystem::ResetResource()
{
	if(m_dpi <= 0.f || !m_font.IsOk())
		return;

	HRESULT hr;
	auto fontName = m_font.GetFaceName();
	DWRITE_FONT_WEIGHT weight = static_cast<DWRITE_FONT_WEIGHT>(m_font.GetWeight());
	if(weight > DWRITE_FONT_WEIGHT_ULTRA_BLACK)
		weight = DWRITE_FONT_WEIGHT_ULTRA_BLACK;

	float fontSize = m_dpi * m_font.GetFractionalPointSize();

	hr = m_dwFactory->CreateTextFormat(fontName.wx_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"ko-KR", &m_dwTextFormat);
	assert(SUCCEEDED(hr));
	for(auto& it: m_messages)
	{
		it.ResetResource(m_dwFactory, m_dwTextFormat, m_dpi, m_width, m_height);
	}
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
