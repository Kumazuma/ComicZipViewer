#pragma once
#include <wx/wx.h>

class UpdateSystem: public wxEvtHandler
{
public:
	UpdateSystem() = default;
	UpdateSystem(const UpdateSystem&) = delete;
	~UpdateSystem() override;
	class Worker;
	void AddWorker(Worker* pWorker);

protected:
	void Update();

private:
	bool m_updateRunning;
	std::vector<Worker*> m_workerList;
};

class UpdateSystem::Worker
{
public:
	virtual ~Worker() = default;
	virtual void Update(uint64_t delta) = 0;
	virtual bool IsFinished() = 0;
};
