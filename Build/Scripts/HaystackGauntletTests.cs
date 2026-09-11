// Copyright (c) 2026 Vanan Andreas.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using AutomationTool;
using Gauntlet;
using UnrealBuildTool;

// RunUnreal searches the UnrealGame namespace for test names, so the nodes live here and run as -test=HayDig, -test=HayPerf.
namespace UnrealGame
{
	/// <summary>
	/// A listen server and bot clients dig until someone finds the needle.
	/// Every role runs UHayDigTestController, which adds the bot and ends the process when the needle is found.
	///
	/// -HayPlayers=N total players including the host, default 2. Bots dig straight at the needle unless -HayDigRandom.
	/// -HayNeedleDepth=Min,Max puts the needle in a depth band in cm, default 0,10 so a run ends in minutes.
	/// -HayDigTimeout=seconds, default 600. -HayResX, -HayResY, -HayMaxFps size and cap every role, default 960x540 at 60.
	/// -HayDigOut=dir receives a HayDig_<Configuration>_<time>.txt summary read from the role logs, default LogDir/HayDig.
	/// </summary>
	public class HayDig : UnrealTestNode<UnrealTestConfig>
	{
		const string DefaultMap = "/Game/ThirdPersonBP/Maps/ThirdPersonExampleMap";

		int Players;
		bool TowardNeedle;
		string NeedleDepth = "";
		string OutDir = "";

		public HayDig(UnrealTestContext InContext) : base(InContext)
		{
		}

		public override UnrealTestConfig GetConfiguration()
		{
			UnrealTestConfig Config = base.GetConfiguration();

			Players = Globals.Params.ParseValue("HayPlayers", 2);
			float Timeout = Globals.Params.ParseValue("HayDigTimeout", 600f);
			TowardNeedle = !Globals.Params.ParseParam("HayDigRandom");
			NeedleDepth = Globals.Params.ParseValue("HayNeedleDepth", "0,10");
			int ResX = Globals.Params.ParseValue("HayResX", 960);
			int ResY = Globals.Params.ParseValue("HayResY", 540);
			int MaxFps = Globals.Params.ParseValue("HayMaxFps", 60);
			string Map = string.IsNullOrEmpty(Config.Map) ? DefaultMap : Config.Map;
			OutDir = Globals.Params.ParseValue("HayDigOut", Path.Combine(Context.Options.LogDir, "HayDig"));

			// No server role, so Gauntlet puts each role's map on its command line. The first hosts, the rest connect to it.
			IEnumerable<UnrealTestRole> Roles = Config.RequireRoles(UnrealTargetRole.Client, Players);
			int Index = 0;
			foreach (UnrealTestRole Role in Roles)
			{
				Role.MapOverride = Index == 0 ? Map + "?listen" : "127.0.0.1";
				Role.Controllers.Add("HayDigTest");
				// Small windows and a frame cap. Several 4M piece instances on one GPU otherwise stall the host for seconds,
				// which freezes the clients' movement.
				Role.CommandLine += string.Format(" -HayDigTimeout={0}{1} -HayNeedleDepth={2} -ResX={3} -ResY={4} -log", Timeout, TowardNeedle ? " -HayDigNeedle" : "", NeedleDepth, ResX, ResY);
				Role.CommandLineParams.Add("ExecCmds", string.Format("t.MaxFPS {0},r.VSync 0", MaxFps));
				++Index;
			}

			Config.MaxDuration = Timeout + 180;
			return Config;
		}

		/// <summary>
		/// Writes one summary file from the role logs: the outcome, when the needle turned up, and each role's takes and spots.
		/// </summary>
		public override ITestReport CreateReport(TestResult Result, UnrealTestContext Context, UnrealBuildSource Build, IEnumerable<UnrealRoleResult> InResults, string InArtifactPath)
		{
			ITestReport Report = base.CreateReport(Result, Context, Build, InResults, InArtifactPath);

			List<string> Lines = new List<string>
			{
				"test=HayDig",
				string.Format("configuration={0}", Context.Options.Configuration),
				string.Format("build={0}", Context.Options.Build),
				string.Format("players={0}", Players),
				string.Format("toward_needle={0}", TowardNeedle ? "true" : "false"),
				string.Format("needle_depth_cm={0}", NeedleDepth),
				string.Format("result={0}", Result),
			};

			foreach (UnrealRoleResult RoleResult in InResults)
			{
				string LogPath = RoleResult.Artifacts?.LogPath;
				string Role = string.IsNullOrEmpty(LogPath) ? RoleResult.Artifacts?.SessionRole?.RoleType.ToString() : Path.GetFileName(Path.GetDirectoryName(LogPath));
				string Text = !string.IsNullOrEmpty(LogPath) && File.Exists(LogPath) ? File.ReadAllText(LogPath) : "";

				Match Found = Regex.Match(Text, @"HayDigTest: needle found after (\d+) s with (\d+) players");
				if (Found.Success && !Lines.Any(L => L.StartsWith("found_after_s=")))
				{
					Lines.Add(string.Format("found_after_s={0}", Found.Groups[1].Value));
					Lines.Add(string.Format("players_connected={0}", Found.Groups[2].Value));
				}
				Match FoundBy = Regex.Match(Text, @"Needle found by (\S+)");
				if (FoundBy.Success && !Lines.Any(L => L.StartsWith("found_by=")))
				{
					Lines.Add(string.Format("found_by={0}", FoundBy.Groups[1].Value));
				}

				// The bot prints its own totals when the needle turns up, or a running count every 20 takes.
				Match Totals = Regex.Match(Text, @"DigBot: needle found after (\d+) takes at (\d+) spots");
				if (!Totals.Success)
				{
					Totals = Regex.Matches(Text, @"DigBot: (\d+) takes, \d+ misses at spot (\d+)").Cast<Match>().LastOrDefault() ?? Match.Empty;
				}
				Match NetMode = Regex.Match(Text, @"HayDigTest: net mode (\w+)");
				Lines.Add(string.Format("role={0} net={1} exit={2} takes={3} spots={4}", Role, NetMode.Success ? NetMode.Groups[1].Value : "Standalone", RoleResult.ExitCode,
					Totals.Success ? Totals.Groups[1].Value : "0", Totals.Success ? Totals.Groups[2].Value : "0"));
			}

			Directory.CreateDirectory(OutDir);
			string Summary = Path.Combine(OutDir, string.Format("HayDig_{0}_{1:yyyyMMdd_HHmmss}.txt", Context.Options.Configuration, DateTime.Now));
			File.WriteAllLines(Summary, Lines);
			Log.Info("HayDig summary {0}", Summary);
			foreach (string Line in Lines)
			{
				Log.Info("  {0}", Line);
			}

			return Report;
		}
	}

	/// <summary>
	/// One client stands and looks at the pile while UHayPerfTestController records a CSV profiler capture and a summary.
	///
	/// -HayResX, -HayResY, default 1920x1080. -HayFullscreen. -HayPerfWarmup and -HayPerfCapture in seconds, default 5
	/// and 60. -HayPerfOut=dir, default LogDir/HayPerf. Output files are named after the configuration and resolution.
	/// </summary>
	public class HayPerf : UnrealTestNode<UnrealTestConfig>
	{
		const string DefaultMap = "/Game/ThirdPersonBP/Maps/ThirdPersonExampleMap";

		string OutDir = "";

		public HayPerf(UnrealTestContext InContext) : base(InContext)
		{
		}

		public override UnrealTestConfig GetConfiguration()
		{
			UnrealTestConfig Config = base.GetConfiguration();

			int ResX = Globals.Params.ParseValue("HayResX", 1920);
			int ResY = Globals.Params.ParseValue("HayResY", 1080);
			float Warmup = Globals.Params.ParseValue("HayPerfWarmup", 5f);
			float Capture = Globals.Params.ParseValue("HayPerfCapture", 60f);
			bool Fullscreen = Globals.Params.ParseParam("HayFullscreen");
			string Map = string.IsNullOrEmpty(Config.Map) ? DefaultMap : Config.Map;
			string Label = string.Format("{0}_{1}x{2}", Context.Options.Configuration, ResX, ResY);
			OutDir = Globals.Params.ParseValue("HayPerfOut", Path.Combine(Context.Options.LogDir, "HayPerf"));

			UnrealTestRole Client = Config.RequireRole(UnrealTargetRole.Client);
			Client.MapOverride = Map;
			Client.Controllers.Add("HayPerfTest");
			Client.CommandLine += string.Format(" -ResX={0} -ResY={1} {2} -HayPerfOut=\"{3}\" -HayPerfLabel={4} -HayPerfWarmup={5} -HayPerfCapture={6} -log",
				ResX, ResY, Fullscreen ? "-fullscreen" : "-windowed", OutDir, Label, Warmup, Capture);

			Config.MaxDuration = Warmup + Capture + 300;
			return Config;
		}

		public override ITestReport CreateReport(TestResult Result, UnrealTestContext Context, UnrealBuildSource Build, IEnumerable<UnrealRoleResult> InResults, string InArtifactPath)
		{
			ITestReport Report = base.CreateReport(Result, Context, Build, InResults, InArtifactPath);

			if (Directory.Exists(OutDir))
			{
				foreach (string Summary in Directory.GetFiles(OutDir, "*.txt"))
				{
					Log.Info("HayPerf summary {0}", Summary);
					foreach (string Line in File.ReadAllLines(Summary))
					{
						Log.Info("  {0}", Line);
					}
				}
			}

			return Report;
		}
	}
}
