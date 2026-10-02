#include "command_history.h"
#include "engine/core/log.h"

namespace Chained
{
	CommandHistory::CommandHistory(size_t maxHistory)
		: m_MaxHistory(maxHistory)
	{
	}

	void CommandHistory::PushCommand(std::unique_ptr<IEditorCommand> command)
	{
		if (!command)
		{
			return;
		}

		command->Execute();
		m_RedoStack.clear();
		m_UndoStack.push_back(std::move(command));

		if (m_UndoStack.size() > m_MaxHistory)
		{
			m_UndoStack.pop_front();
		}

		CH_CORE_INFO("Command pushed: {} (Undo stack size: {})", m_UndoStack.back()->GetName(), m_UndoStack.size());
	}

	void CommandHistory::Undo()
	{
		if (m_UndoStack.empty())
		{
			return;
		}

		std::unique_ptr<IEditorCommand> command = std::move(m_UndoStack.back());
		m_UndoStack.pop_back();

		CH_CORE_INFO("Undoing command: {}", command->GetName());
		command->Undo();

		m_RedoStack.push_back(std::move(command));
	}

	void CommandHistory::Redo()
	{
		if (m_RedoStack.empty())
		{
			return;
		}

		std::unique_ptr<IEditorCommand> command = std::move(m_RedoStack.back());
		m_RedoStack.pop_back();

		CH_CORE_INFO("Redoing command: {}", command->GetName());
		command->Execute();

		m_UndoStack.push_back(std::move(command));
	}

} // namespace Chained
