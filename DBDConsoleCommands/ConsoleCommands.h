#pragma once
#include "Utilities.h"

#include <variant>






class ConsoleCommands
{
private:
	static inline bool isConsoleInitialized = false;

	static inline int32_t lastHistoryCount = 0;
	static inline std::wstring lastConsoleInput = std::wstring();

	static void DBD_ForceStartMatch();
	static void DBD_ResetPallets();
	static inline const std::vector<std::pair<std::wstring, std::function<void()>>> execConsoleCommands =
	{
		{L"DBD_ForceStartMatch", DBD_ForceStartMatch},
		{L"DBD_ResetPallets", DBD_ResetPallets},
	};

	using CVarCallback = std::variant
	<
		std::function<void(int)>,
		std::function<void(float)>,
		std::function<void(bool)>,
		std::function<void(std::wstring)>
	>;
	static inline const std::vector<std::pair<std::wstring, CVarCallback>> cvarConsoleCommands =
	{
	};


	static void Process();
	static DWORD WINAPI ThreadSafeWorker();


public:
	static void Init();
};

