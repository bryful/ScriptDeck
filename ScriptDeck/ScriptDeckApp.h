#pragma once

#include "LaunchOptions.h"

class ScriptDeckApp
{
public:
	int Run(const LaunchOptions& options);

private:
	int RunPlayer(const LaunchOptions& options);
	int RunMagic(const LaunchOptions& options);
	int RunConsole(const LaunchOptions& options);
};