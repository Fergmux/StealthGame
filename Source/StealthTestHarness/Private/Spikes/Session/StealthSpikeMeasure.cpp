// THROWAWAY spike measurement. See StealthSpikeMeasure.h.
#include "Spikes/Session/StealthSpikeMeasure.h"

#include "Scenarios/StealthHarnessRun.h"
#include "Spikes/Session/StealthTimingIpConnection.h"
#include "StealthTestHarnessModule.h"

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectIterator.h"

namespace StealthHarness
{
	namespace MeasurePrivate
	{
		struct FConnectionBatch
		{
			int32 Serial = 0;
			FString Name;
			FString Tx;
			FString Rx;
			int32 TxCount = 0;
			int32 RxCount = 0;
		};

		static bool GPacketTiming = false;
		static TMap<TObjectKey<UNetConnection>, FConnectionBatch> GBatches;
		static int32 GNextSerial = 1;
		static TMap<FString, FString> GRtt;
		static TArray<float> GFrames;
		static double GLastFlush = 0.0;

		static FString NameOf(const UNetConnection* Connection)
		{
			if (Connection && Connection->Driver && Connection->Driver->ServerConnection == Connection)
			{
				return TEXT("server");
			}
			return FRun::ConnectionName(Connection);
		}
	}

	void FMeasure::InstallIfRequested()
	{
		using namespace MeasurePrivate;
		if (!FParse::Param(FCommandLine::Get(), TEXT("StealthPacketTiming")))
		{
			return;
		}
		GPacketTiming = true;
		const FString TimingClass = UStealthTimingIpConnection::StaticClass()->GetPathName();
		int32 Patched = 0;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf(UNetDriver::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}
			UNetDriver* Defaults = It->GetDefaultObject<UNetDriver>();
			if (Defaults && Defaults->NetConnectionClassName == TEXT("/Script/OnlineSubsystemUtils.IpConnection"))
			{
				Defaults->NetConnectionClassName = TimingClass;
				++Patched;
			}
		}
		UE_LOGF(LogStealthHarness, Display, "Packet timing on: %d net driver class defaults use %ls", Patched, *TimingClass);
	}

	bool FMeasure::IsPacketTimingOn()
	{
		return MeasurePrivate::GPacketTiming;
	}

	void FMeasure::AddRtt(const FString& Conn, float Seconds)
	{
		FString& Batch = MeasurePrivate::GRtt.FindOrAdd(Conn);
		Batch += FString::Printf(TEXT("%s%.2f"), Batch.IsEmpty() ? TEXT("") : TEXT(","), Seconds * 1000.f);
	}

	void FMeasure::NotePacket(bool bSend, const UNetConnection* Connection, int32 PacketId, double Time)
	{
		using namespace MeasurePrivate;
		FConnectionBatch& Batch = GBatches.FindOrAdd(Connection);
		if (Batch.Serial == 0)
		{
			Batch.Serial = GNextSerial++;
		}
		FString& Out = bSend ? Batch.Tx : Batch.Rx;
		Out += FString::Printf(TEXT("%s[%d,%.6f]"), Out.IsEmpty() ? TEXT("") : TEXT(","), PacketId, Time);
		++(bSend ? Batch.TxCount : Batch.RxCount);
		// Resolve the name while the connection is alive (a client is named once its controller is renamed C<n>).
		const FString Name = NameOf(Connection);
		if (Name != TEXT("?"))
		{
			Batch.Name = Name;
		}
	}

	void FMeasure::NoteFrame(float DeltaSeconds)
	{
		MeasurePrivate::GFrames.Add(DeltaSeconds);
	}

	void FMeasure::Flush(bool bForce)
	{
		using namespace MeasurePrivate;
		const double Now = FPlatformTime::Seconds();
		FRun* Run = FRun::Get();
		if (!Run || (!bForce && Now - GLastFlush < 1.0))
		{
			return;
		}
		GLastFlush = Now;
		for (TPair<TObjectKey<UNetConnection>, FConnectionBatch>& Pair : GBatches)
		{
			FConnectionBatch& Batch = Pair.Value;
			if (Batch.TxCount + Batch.RxCount == 0)
			{
				continue;
			}
			Run->Write(TEXT("pkt"), FString::Printf(TEXT("\"c\":%s,\"serial\":%d,\"tx\":[%s],\"rx\":[%s]"),
				*FRun::Q(Batch.Name.IsEmpty() ? FString(TEXT("?")) : Batch.Name), Batch.Serial, *Batch.Tx, *Batch.Rx));
			Batch.Tx.Reset();
			Batch.Rx.Reset();
			Batch.TxCount = 0;
			Batch.RxCount = 0;
		}
		for (TPair<FString, FString>& Pair : GRtt)
		{
			if (!Pair.Value.IsEmpty())
			{
				Run->Write(TEXT("rtt"), FString::Printf(TEXT("\"c\":%s,\"ms\":[%s]"), *FRun::Q(Pair.Key), *Pair.Value));
				Pair.Value.Reset();
			}
		}
		if (GFrames.Num() > 0)
		{
			GFrames.Sort();
			float Sum = 0.f;
			for (const float Frame : GFrames)
			{
				Sum += Frame;
			}
			const float P99 = GFrames[FMath::Min(GFrames.Num() - 1, FMath::FloorToInt(0.99f * GFrames.Num()))];
			Run->Write(TEXT("frames"), FString::Printf(TEXT("\"n\":%d,\"meanMs\":%.3f,\"p99Ms\":%.3f,\"maxMs\":%.3f"),
				GFrames.Num(), 1000.f * Sum / GFrames.Num(), 1000.f * P99, 1000.f * GFrames.Last()));
			GFrames.Reset();
		}
	}
}

UStealthTimingIpConnection::UStealthTimingIpConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UStealthTimingIpConnection::FlushNet(bool bIgnoreSimulation)
{
	const int32 Before = OutPacketId;
	const double Time = FPlatformTime::Seconds();
	Super::FlushNet(bIgnoreSimulation);
	if (OutPacketId != Before)
	{
		StealthHarness::FMeasure::NotePacket(true, this, Before, Time);
	}
}

void UStealthTimingIpConnection::ReceivedPacket(FBitReader& Reader, bool bIsReinjectedPacket, bool bDispatchPacket)
{
	const int32 Before = InPacketId;
	const double Time = FPlatformTime::Seconds();
	Super::ReceivedPacket(Reader, bIsReinjectedPacket, bDispatchPacket);
	if (InPacketId != Before)
	{
		StealthHarness::FMeasure::NotePacket(false, this, InPacketId, Time);
	}
}
