#include "ConsoleCommands.h"






void ConsoleCommands::DBD_ForceStartMatch()
{
	if (Utilities::Game::IsInOnlineLobby() == false)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ForceStartMatch] Not in online lobby\n");
		return;
	}

	static const uintptr_t SCAN_POS = 0x0C236BF8;
	static const std::vector<uintptr_t> SCAN_OFFSETS =
	{
			0x20,   // Offset 0
			0xB0,   // Offset 1
			0x50,   // Offset 2
			0x60,   // Offset 3
			0xB0,   // Offset 4
			0x128,  // Offset 5
			0x3D0   // Offset 6
	};

	HMODULE hMainModule = GetModuleHandle(nullptr);
	uintptr_t startingAddress = Utilities::Memory::Internal::AddressAddOffset(reinterpret_cast<uintptr_t>(hMainModule), SCAN_POS);
	uintptr_t countdownAddress = Utilities::Memory::Internal::AddressFollowPointerChain(startingAddress, SCAN_OFFSETS);

	if (countdownAddress == 0x0)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ForceStartMatch] Failed to find countdown\n");
		return;
	}

	static const float countdownRange_Min = -1.0f;
	static const float countdownRange_Max = 131072.0f;

	static const float countdownValue_ForceStartMatch = 5.9000001f;

	float countdownValue = Utilities::Memory::Internal::GetFloat(countdownAddress);
	if (countdownValue == 0.0f)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ForceStartMatch] Failed to read countdown value\n");
		return;
	}

	if (countdownValue < countdownRange_Min || countdownValue > countdownRange_Max)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ForceStartMatch] Countdown value is out of range\n");
		return;
	}

	if (countdownValue > countdownRange_Min && countdownValue < countdownValue_ForceStartMatch)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ForceStartMatch] Match is already starting\n");
		return;
	}

	float newCountdownValue = countdownValue_ForceStartMatch;
	if (Utilities::Memory::Internal::SetFloat(countdownAddress, newCountdownValue) == false)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ForceStartMatch] Failed to change countdown value\n");
		return;
	}

	Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ForceStartMatch] _timerValue = %.2f\n", newCountdownValue);
}

void ConsoleCommands::DBD_ResetPallets()
{
	SDK::UWorld* world = SDK::UWorld::GetWorld();
	if (world == nullptr)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ResetPallets] Failed to get World\n");
		return;
	}

	if (Utilities::Game::IsInMatch(world) == false)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ResetPallets] Not in a match\n");
		return;
	}

	SDK::APlayerController* playerController = SDK::UGameplayStatics::GetPlayerController(world, 0);
	if (playerController == nullptr || playerController->HasAuthority() == false)
	{
		Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ResetPallets] Failed to validate authority\n");
		return;
	}

	std::vector<SDK::APallet*> pallets = Utilities::Game::GetPallets();
	int32_t palletCount = pallets.size();
	int32_t palletResetCount = 0;
	for (SDK::APallet* plt : pallets)
	{
		if (plt == nullptr)
			continue;

		if (plt->_state == SDK::EPalletState::Up)
			continue;
		else if (plt->_state != SDK::EPalletState::Destroyed)
		{
			plt->ResetPallet();
			plt->Multicast_ResetPallet();
			palletResetCount++;
		}
		else
		{
			SDK::FTransform palletTransform = plt->GetTransform();
			SDK::UClass* palletClass = plt->Class;

			/* Destroy original pallet */
			plt->K2_DestroyActor();

			SDK::AActor* newPallet = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(world, palletClass, palletTransform, SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr);
			SDK::UGameplayStatics::FinishSpawningActor(newPallet, palletTransform);
			palletResetCount++;
		}
	}

	Utilities::Game::Print(L"[DBDConsoleCommands][DBD_ResetPallets] %d out of %d pallets reset\n", palletResetCount, palletCount);
}




void ConsoleCommands::Process()
{
	SDK::UEngine* engine = SDK::UEngine::GetEngine();
	if (engine == nullptr || engine->GameViewport == nullptr)
		return;

	SDK::UGameViewportClient* gameViewportClient = engine->GameViewport;
	if (gameViewportClient->ViewportConsole == nullptr)
		return;

	SDK::UConsole* console = gameViewportClient->ViewportConsole;
	int32_t historyCount = console->HistoryBuffer.Num();

	if (historyCount == 0)
	{
		if (isConsoleInitialized == false)
		{
			isConsoleInitialized = true;
		}

		lastHistoryCount = 0;
		lastConsoleInput.clear();
		return;
	}

	std::wstring consoleInput = console->HistoryBuffer[historyCount - 1].ToWString();
	if (isConsoleInitialized == false)
	{
		std::wstring consoleInputLowerCase = Utilities::String::ToLowerCase(consoleInput);
		bool consoleCommandMatch = false;

		for (auto& [execCommand, execFunction] : execConsoleCommands)
		{
			std::wstring execCommandLowerCase = Utilities::String::ToLowerCase(execCommand);
			if (execCommandLowerCase == consoleInputLowerCase)
			{
				console->HistoryBuffer.Remove(historyCount - 1);
				consoleCommandMatch = true;
				break;
			}
		}
		
		if (consoleCommandMatch == false)
		{
			for (auto& [cvarCommand, cvarFunction] : cvarConsoleCommands)
			{
				std::wstring cvarCommandLowerCase = Utilities::String::ToLowerCase(cvarCommand);
				std::wstring commandPrefix = cvarCommandLowerCase + L" ";

				if (Utilities::String::StartsWith(consoleInputLowerCase, commandPrefix, true))
				{
					console->HistoryBuffer.Remove(historyCount - 1);
					consoleCommandMatch = true;
					break;
				}
			}
		}

		lastHistoryCount = historyCount;
		lastConsoleInput = consoleCommandMatch ? L" " : consoleInput;
		isConsoleInitialized = true;
		return;
	}

	if (consoleInput.empty())
		return;

	bool isNewCommand = false;
	if (historyCount > lastHistoryCount)
	{
		isNewCommand = true;
	}
	else if (historyCount == lastHistoryCount)
	{
		if (consoleInput != lastConsoleInput)
		{
			isNewCommand = true;
		}
	}
	else
	{
		isNewCommand = true;
	}

	if (isNewCommand == false)
		return;

	lastHistoryCount = historyCount;
	lastConsoleInput = consoleInput;

	std::wstring consoleInputLowerCase = Utilities::String::ToLowerCase(consoleInput);
	bool commandExecuted = false;

	for (auto& [execCommand, execFunction] : execConsoleCommands)
	{
		std::wstring execCommandLowerCase = Utilities::String::ToLowerCase(execCommand);
		if (execCommandLowerCase == consoleInputLowerCase)
		{
			wprintf(L"[DBDConsoleCommands] Console Command (Exec): %ls\n", consoleInput.c_str());

			execFunction();
			commandExecuted = true;
			break;
		}
	}

	if (commandExecuted == false)
	{
		for (auto& [cvarCommand, cvarFunction] : cvarConsoleCommands)
		{
			std::wstring cvarCommandLowerCase = Utilities::String::ToLowerCase(cvarCommand);
			std::wstring commandPrefix = cvarCommandLowerCase + L" ";

			if (Utilities::String::StartsWith(consoleInputLowerCase, commandPrefix, true))
			{
				std::wstring cvarValueStr = consoleInput.substr(commandPrefix.length());
				std::wstring cvarValueStrLower = Utilities::String::ToLowerCase(cvarValueStr);

				try
				{
					if (std::holds_alternative<std::function<void(int)>>(cvarFunction))
					{
						std::get<std::function<void(int)>>(cvarFunction)(std::stoi(cvarValueStr));
					}
					else if (std::holds_alternative<std::function<void(float)>>(cvarFunction))
					{
						std::get<std::function<void(float)>>(cvarFunction)(std::stof(cvarValueStr));
					}
					else if (std::holds_alternative<std::function<void(bool)>>(cvarFunction))
					{
						bool val = (cvarValueStrLower == L"1" || cvarValueStrLower == L"true");
						std::get<std::function<void(bool)>>(cvarFunction)(val);
					}
					else if (std::holds_alternative<std::function<void(std::wstring)>>(cvarFunction))
					{
						std::wstring cleanStr = cvarValueStr;

						if (Utilities::String::StartsWith(cleanStr, L"\"", true) &&
							Utilities::String::EndsWith(cleanStr, L"\"", true) &&
							cleanStr.length() >= 2)
						{
							cleanStr = cleanStr.substr(1, cleanStr.length() - 2);
						}

						std::get<std::function<void(std::wstring)>>(cvarFunction)(cleanStr);
					}

					wprintf(L"[DBDConsoleCommands] Console Command (CVar): %ls\n", consoleInput.c_str());
					commandExecuted = true;
				}
				catch (const std::invalid_argument&)
				{
					wprintf(L"[DBDConsoleCommands] Invalid argument type for CVAR: %ls\n", cvarCommand.c_str());
				}
				catch (const std::out_of_range&)
				{
					wprintf(L"[DBDConsoleCommands] Value out of range for CVAR: %ls\n", cvarCommand.c_str());
				}

				break;
			}
		}
	}

	if (commandExecuted)
	{
		console->HistoryBuffer.Remove(historyCount - 1);

		if (historyCount > 1)
			lastConsoleInput = console->HistoryBuffer[historyCount - 2].ToWString();
		else
			lastConsoleInput.clear();
	}
}

DWORD WINAPI ConsoleCommands::ThreadSafeWorker()
{
	printf("[DBDConsoleCommands] ThreadSafeWorker() is now entering the loop\n");

	while (true)
	{
		EXCEPTION_POINTERS* exceptionInfo = nullptr;

		__try
		{
			Process();
			Sleep(100);
		}
		__except (exceptionInfo = GetExceptionInformation(), EXCEPTION_EXECUTE_HANDLER)
		{
			if (exceptionInfo)
			{
				void* crashAddress = exceptionInfo->ExceptionRecord->ExceptionAddress;
				DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

				HMODULE hModule = GetModuleHandle(nullptr);
				uintptr_t baseAddress = (uintptr_t)hModule;
				uintptr_t relativeOffset = (uintptr_t)crashAddress - baseAddress;

				printf("[DBDConsoleCommands] FATAL ERROR / CRASH in %s\n", __FUNCSIG__);
				printf("[DBDConsoleCommands] Exception Code:   0x%08X\n", exceptionCode);
				printf("[DBDConsoleCommands] Absolute Address: 0x%llX\n", (unsigned long long)crashAddress);
				printf("[DBDConsoleCommands] Module Base:      0x%llX\n", (unsigned long long)baseAddress);
				printf("[DBDConsoleCommands] Relative Offset:  0x%llX\n", (unsigned long long)relativeOffset);
			}

			printf("[DBDConsoleCommands] Restarting loop in 3 seconds...\n");
			Sleep(3000);
		}
	}

	return 0;
}




void ConsoleCommands::Init()
{
	printf("[DBDConsoleCommands] Starting initialization sequence...\n");
	while (Utilities::Game::IsInitialized() == false)
	{
		Sleep(10);
	}

	printf("[DBDConsoleCommands] Game is initialized, starting ThreadSafeWorker()...\n");
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadSafeWorker, 0, 0, 0);
}