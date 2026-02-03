#pragma once

namespace DisabledReferenceIntegrityFix
{
	class CellFullyLoadedEventHandler
		: public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESCellFullyLoadedEvent*           a_event,
			RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) override;

		static CellFullyLoadedEventHandler* GetSingleton();
	};

	void MessageHandler(SKSE::MessagingInterface::Message* a_message);
}
