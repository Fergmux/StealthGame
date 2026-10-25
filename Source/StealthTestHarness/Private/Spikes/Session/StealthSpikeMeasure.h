#pragma once

// THROWAWAY spike measurement (session-travel story 002, IVP stories 005-006): packet-level send/receive
// timestamps per connection, raw RTT samples and frame times, written as batched harness records once per
// second. Packet timing needs `-StealthPacketTiming`: the game net driver then uses UStealthTimingIpConnection
// (a UIpConnection that notes each flushed and each received packet id with FPlatformTime::Seconds(), which is
// comparable across processes on one machine). Analyser: tools/ci/mp/analyze_net.py.

#include "CoreMinimal.h"

class UNetConnection;

namespace StealthHarness
{
	/** Process-wide measurement sink (game thread only). */
	class FMeasure
	{
	public:
		/** Patches the IP net driver defaults to the timing connection class when -StealthPacketTiming is given. Module startup. */
		static void InstallIfRequested();

		/** True when packet timing is on for this process. */
		static bool IsPacketTimingOn();

		/** One raw RTT sample (seconds) of the connection named Conn. */
		static void AddRtt(const FString& Conn, float Seconds);

		/** A packet with id PacketId was flushed (bSend) or received on Connection at Time. */
		static void NotePacket(bool bSend, const UNetConnection* Connection, int32 PacketId, double Time);

		/** One frame of DeltaSeconds. */
		static void NoteFrame(float DeltaSeconds);

		/** Writes the batches (every second, and at the end with bForce). */
		static void Flush(bool bForce);
	};
}
