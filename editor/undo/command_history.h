#ifndef CH_COMMAND_HISTORY_H
#define CH_COMMAND_HISTORY_H

#include "deque"
#include "command.h"
#include <functional>
#include <memory>
#include <string>

namespace Chained
{
	class CommandHistory
	{
	public:
		CommandHistory(size_t maxHistory = 50);
		~CommandHistory() = default;

		void PushCommand(std::unique_ptr<IEditorCommand> command);
		void Undo();
		void Redo();

	private:
		size_t m_MaxHistory;
		std::deque<std::unique_ptr<IEditorCommand>> m_UndoStack;
		std::deque<std::unique_ptr<IEditorCommand>> m_RedoStack;
	};
} // namespace Chained

#endif // CH_COMMAND_HISTORY_H
