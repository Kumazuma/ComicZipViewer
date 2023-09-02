#include "framework.h"
#include "UpdateSystem.h"

#include "tick.h"

UpdateSystem::~UpdateSystem()
{
	for(auto it : m_workerList)
	{
		delete it;
	}
}

void UpdateSystem::AddWorker(Worker* pWorker)
{
	m_workerList.push_back(pWorker);
	CallAfter(&UpdateSystem::Update);
	if(!m_updateRunning)
	{
		
	}
}

void UpdateSystem::Update()
{
	auto prevTick = GetTick();
	auto frequency = GetTickFrequency();

	if( m_updateRunning )
	{
		return;
	}

	m_updateRunning = true;

	while(!m_workerList.empty())
	{

		const int64_t current = GetTick();
		const int64_t diff = current - prevTick;
		prevTick = current;
		const uint64_t delta = ( ( diff * 4096ull ) / frequency );

		auto list = std::move(m_workerList);
		for(auto it: list)
		{
			it->Update(delta);
		}

		for(auto it : list)
		{
			if(it->IsFinished() )
			{
				delete it;
				continue;
			}

			m_workerList.push_back(it);
		}

		wxYield();
	}
	m_updateRunning = false;
}
