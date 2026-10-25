// Story tdc-020 (production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md):
// the T3 multi-process smoke node (ADR-0008 sections 1 and 5). Gauntlet launches one cooked Development
// listen host and one client; each process runs the in-game StealthTestHarness scenario named by
// -StealthHarnessScenario=<Name> (default "Smoke") and writes a result JSON
// {scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]}
// into a folder this node passes as -StealthHarnessOut. The node then asserts that BOTH files exist, say
// replicationSystem == the expectation (legacy, ADR-0001) and passed == true. A missing file, a crashed
// process or a mismatch fails the node; nothing here passes by default.
//
// Negative control (ADR-0008 Validation 4): start the same node with extra role arguments, for example
//   -StealthRoleArgs="-StealthHarnessForceReplicationSystem=iris"   (harness seam: each process's probe reports Iris)
// and the node FAILS (observed 2026-10-25); the suite passes only when that failure is seen. The engine switch
//   -StealthRoleArgs="-UseIrisReplication=1"
// is NOT usable as the control on this project: UE 5.8.4 logs "Cmdline -UseIrisReplication=1 is forcing NetDriver ..."
// but only honours it when the net driver config allows Iris (bCanUseIris) and the Iris plugin is on, which this project
// deliberately does not do (ADR-0001), so the driver stays legacy and the node passes.
//
// Prerequisites and run (observed 2026-10-25, UE 5.8.4):
//   RunUAT.bat BuildCookRun -project=<uproject> -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak
//       -IgnoreCookErrors -stagingdirectory=<dir>        (the cook reports one error from the sample's broken camera import)
//   RunUAT.bat -ScriptsForProject=<uproject> RunUnreal -project=<uproject> -test=StealthMultiProcessSmoke
//       -Namespaces=StealthGame.Harness.MP -build=<dir> -platform=Win64 -configuration=Development
// Test name (ADR-0008 section 2): StealthGame.Harness.MP.Smoke
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using AutomationTool;
using Gauntlet;
using UnrealBuildTool;

namespace StealthGame.Harness.MP
{
	/// <summary>
	/// Cooked Development listen host plus one client, each asserting and recording its replication system.
	/// </summary>
	public class StealthMultiProcessSmoke : UnrealTestNode<UnrealTestConfiguration>
	{
		/// <summary>The map the listen host opens (the harness fixture map).</summary>
		const string HostMap = "/Game/StealthGame/Maps/L_Prototype?listen";

		/// <summary>The replication system the run must be on until ADR-0024 (ADR-0001). One edit to switch.</summary>
		const string PinnedReplicationSystem = "legacy";

		/// <summary>Folder the roles write their result JSON into.</summary>
		string ResultDir;

		public StealthMultiProcessSmoke(UnrealTestContext InContext) : base(InContext)
		{
		}

		/// <summary>The ADR-0008 test name for this node.</summary>
		public override string Name => "StealthGame.Harness.MP." + ScenarioName;

		/// <summary>The harness scenario both processes run (-StealthScenario=, default Smoke).</summary>
		string ScenarioName => Globals.Params.ParseValue("StealthScenario", "Smoke");

		public override UnrealTestConfiguration GetConfiguration()
		{
			UnrealTestConfiguration Config = base.GetConfiguration();

			ResultDir = Path.Combine(Globals.TempDir, "StealthHarness", Guid.NewGuid().ToString("N"));
			Directory.CreateDirectory(ResultDir);
			string ExtraArgs = Globals.Params.ParseValue("StealthRoleArgs", "");

			List<UnrealTestRole> Roles = Config.RequireRoles(UnrealTargetRole.Client, 2);
			UnrealTestRole Host = Roles[0];
			UnrealTestRole Client = Roles[1];

			string Common = string.Format(" -StealthHarnessScenario={0} -StealthHarnessOut=\"{1}\" -StealthHarnessClients=1 -nullrhi -log {2}", ScenarioName, ResultDir, ExtraArgs);
			Host.MapOverride = HostMap;
			Host.CommandLine += Common + " -StealthHarnessRole=server";
			Client.CommandLine += Common + " -StealthHarnessRole=client -StealthHarnessClient=1 -ExecCmds=\"open 127.0.0.1?Name=C1\"";
			return Config;
		}

		/// <summary>
		/// Passed only when Gauntlet saw both roles end cleanly AND both result files exist with the pinned
		/// replication system and passed == true. Anything else is a failure with the reason reported.
		/// </summary>
		protected override TestResult GetUnrealTestResult()
		{
			TestResult Base = base.GetUnrealTestResult();
			if (Base != TestResult.Passed)
			{
				return Base;
			}

			bool bOk = true;
			foreach (string FileName in new[] { "server-result.json", "client1-result.json" })
			{
				string FilePath = Path.Combine(ResultDir, FileName);
				if (!File.Exists(FilePath))
				{
					ReportError("{0} is missing: the process did not write its result (a crash or a harness that never ran is not a pass)", FileName);
					bOk = false;
					continue;
				}
				try
				{
					using (JsonDocument Doc = JsonDocument.Parse(File.ReadAllText(FilePath)))
					{
						JsonElement Root = Doc.RootElement;
						string ReportedSystem = Root.GetProperty("replicationSystem").GetString();
						bool bPassed = Root.GetProperty("passed").GetBoolean();
						bool bControl = Root.GetProperty("negativeControlDetected").GetBoolean();
						Log.Info("{File}: replicationSystem={System} passed={Passed} negativeControlDetected={Control}", FileName, ReportedSystem, bPassed, bControl);
						if (!string.Equals(ReportedSystem, PinnedReplicationSystem, StringComparison.OrdinalIgnoreCase))
						{
							ReportError("{0}: replicationSystem is '{1}', the pinned expectation is '{2}'", FileName, ReportedSystem, PinnedReplicationSystem);
							bOk = false;
						}
						if (!bPassed)
						{
							ReportError("{0}: the harness recorded passed=false", FileName);
							bOk = false;
						}
						if (!bControl)
						{
							ReportError("{0}: the harness's own negative control was not detected", FileName);
							bOk = false;
						}
					}
				}
				catch (Exception Ex)
				{
					ReportError("{0} could not be read: {1}", FileName, Ex.Message);
					bOk = false;
				}
			}
			return bOk ? TestResult.Passed : TestResult.Failed;
		}
	}
}
