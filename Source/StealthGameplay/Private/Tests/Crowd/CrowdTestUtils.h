#pragma once

// Shared helpers of the StealthGame.Crowd.* tests (stories cnb-002 onward): run a project CI shell script (the lint
// rules under tools/ci/lint/) and read its observation lines. Mirrors StealthFoundation's build-test helper, which is
// private to that module.

#include "CoreMinimal.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"

namespace StealthCrowdTest
{
	/** Git for Windows' bash (or STEALTH_BASH); never a bare "bash" from PATH (the WSL launcher). Empty when none. */
	inline FString FindBash()
	{
		const FString FromEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("STEALTH_BASH"));
		if (!FromEnv.IsEmpty() && FPaths::FileExists(FromEnv))
		{
			return FromEnv;
		}
		for (const TCHAR* EnvVar : { TEXT("ProgramW6432"), TEXT("ProgramFiles") })
		{
			const FString Root = FPlatformMisc::GetEnvironmentVariable(EnvVar);
			const FString Candidate = FPaths::Combine(Root, TEXT("Git/bin/bash.exe"));
			if (!Root.IsEmpty() && FPaths::FileExists(Candidate))
			{
				return Candidate;
			}
		}
		return FPaths::FileExists(TEXT("/bin/bash")) ? FString(TEXT("/bin/bash")) : FString();
	}

	/** One script run: launched, exit code and stdout + stderr. */
	struct FScriptRun
	{
		bool bLaunched = false;
		int32 ExitCode = -1;
		FString Output;
	};

	/** Runs a command line once with a timeout; false when it timed out (the process tree is then terminated). */
	inline bool RunWithTimeout(const FString& Exe, const FString& Args, double TimeoutSec, FScriptRun& Run)
	{
		void* ReadPipe = nullptr;
		void* WritePipe = nullptr;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
		FProcHandle Proc = FPlatformProcess::CreateProc(*Exe, *Args, false, true, true, nullptr, 0, nullptr, WritePipe, nullptr, WritePipe);
		if (!Proc.IsValid())
		{
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
			return true;
		}
		Run.bLaunched = true;
		const double Start = FPlatformTime::Seconds();
		bool bTimedOut = false;
		while (FPlatformProcess::IsProcRunning(Proc))
		{
			Run.Output += FPlatformProcess::ReadPipe(ReadPipe);
			if (FPlatformTime::Seconds() - Start > TimeoutSec)
			{
				FPlatformProcess::TerminateProc(Proc, true);
				bTimedOut = true;
				break;
			}
			FPlatformProcess::Sleep(0.01f);
		}
		Run.Output += FPlatformProcess::ReadPipe(ReadPipe);
		int32 Code = -1;
		FPlatformProcess::GetProcReturnCode(Proc, &Code);
		Run.ExitCode = bTimedOut ? -1 : Code;
		FPlatformProcess::CloseProc(Proc);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		return !bTimedOut;
	}

	/**
	 * Runs a project-relative bash script with extra arguments. A run that exceeds TimeoutSec is killed and retried (Git
	 * for Windows' bash has been seen to hang in a pipeline subshell under the editor, once in many runs); after
	 * MaxAttempts the last run's output is returned with ExitCode -1.
	 */
	inline FScriptRun RunBashScript(const FString& RelativeScript, const FString& ExtraArgs = FString(), double TimeoutSec = 120.0, int32 MaxAttempts = 3)
	{
		FScriptRun Run;
		const FString Bash = FindBash();
		if (Bash.IsEmpty())
		{
			return Run;
		}
		const FString Script = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), RelativeScript));
		const FString Args = FString::Printf(TEXT("\"%s\" %s"), *Script, *ExtraArgs);
		for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
		{
			Run = FScriptRun();
			if (RunWithTimeout(Bash, Args, TimeoutSec, Run) || !Run.bLaunched)
			{
				break;
			}
			UE_LOGF(LogTemp, Warning, "crowd test: %ls timed out after %.0f s (attempt %d of %d)", *RelativeScript, TimeoutSec, Attempt, MaxAttempts);
		}
		return Run;
	}

	/** The output lines that start with Prefix (for SCANNED and finding lines). */
	inline TArray<FString> LinesWith(const FString& Output, const FString& Needle)
	{
		TArray<FString> Lines;
		Output.ParseIntoArrayLines(Lines);
		return Lines.FilterByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle); });
	}
}
