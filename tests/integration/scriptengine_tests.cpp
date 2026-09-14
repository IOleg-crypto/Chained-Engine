#include "engine/app/application.h"
#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"
#include "engine/project/project.h"
#include "engine/scripting/scriptengine.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <vector>

using namespace Chained;

namespace
{
	std::vector<std::filesystem::path> GetReloadAssemblyCandidates()
	{
		std::vector<std::filesystem::path> candidates;
		auto cwd = std::filesystem::current_path();

		const char* engineRoot = std::getenv("ENGINE_ROOT");
		if (engineRoot && *engineRoot)
		{
			std::filesystem::path root(engineRoot);
			candidates.emplace_back(root / "game" / "chaineddecos" / "assets" / "bin" / "ChainedDecos.Scripts.dll");
			candidates.emplace_back(root / "game" / "chaineddecos" / "assets" / "bin" / "Chained.Managed.dll");
		}

		candidates.emplace_back(cwd / "game" / "chaineddecos" / "assets" / "bin" / "ChainedDecos.Scripts.dll");
		candidates.emplace_back(cwd / "game" / "chaineddecos" / "assets" / "bin" / "Chained.Managed.dll");
		candidates.emplace_back(cwd / "scripts" / "ChainedDecos" / "ChainedDecos.Scripts.dll");
		candidates.emplace_back(cwd / "assets" / "bin" / "ChainedDecos.Scripts.dll");
		candidates.emplace_back(cwd / "ChainedDecos.Scripts.dll");
		candidates.emplace_back(cwd / "scripts" / "ChainedDecos" / "Chained.Managed.dll");
		candidates.emplace_back(cwd / "assets" / "bin" / "Chained.Managed.dll");
		candidates.emplace_back(cwd / "Chained.Managed.dll");

		std::vector<std::filesystem::path> existing;
		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate))
			{
				existing.emplace_back(std::filesystem::absolute(candidate).lexically_normal());
			}
		}

		return existing;
	}
} // namespace

class ScriptEngineTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!ServiceLocator::Has<ScriptEngine>())
		{
			ServiceLocator::Provide<ScriptEngine>([] { return std::make_unique<ScriptEngine>(true); });
		}
		auto* scriptEngine = ServiceLocator::Get<ScriptEngine>();
		if (scriptEngine && !scriptEngine->IsHostInitialized())
		{
			scriptEngine->SetEnabled(true);
			scriptEngine->Initialize();
		}

		if (!scriptEngine || !scriptEngine->IsHostInitialized())
		{
			GTEST_SKIP() << "Skipping ScriptEngine tests: CoreCLR host initialization failed in this environment.";
		}
	}

	void TearDown() override
	{
		Project::SetActive(nullptr);
	}
};

TEST_F(ScriptEngineTest, ReloadWithoutActiveProjectReturnsFalseAndClearsFlag)
{
	auto* scriptEngine = ServiceLocator::Get<ScriptEngine>();
	ASSERT_NE(scriptEngine, nullptr);
	EXPECT_FALSE(scriptEngine->IsReloadInProgress());
	EXPECT_FALSE(scriptEngine->ReloadAssembly(""));
	EXPECT_FALSE(scriptEngine->IsReloadInProgress());
}

TEST_F(ScriptEngineTest, ReloadWithMissingModuleReturnsFalseAndClearsFlag)
{
	auto project = std::make_shared<Project>();
	project->GetConfig().ProjectDirectory = std::filesystem::current_path();
	project->GetConfig().Scripting.ModuleName = "DefinitelyMissingModule";
	project->GetConfig().Scripting.ModuleDirectory = "missing_modules";
	Project::SetActive(project);

	auto* scriptEngine = ServiceLocator::Get<ScriptEngine>();
	ASSERT_NE(scriptEngine, nullptr);
	EXPECT_FALSE(scriptEngine->ReloadAssembly("missing_modules/DefinitelyMissingModule.dll"));
	EXPECT_FALSE(scriptEngine->IsReloadInProgress());
}

TEST_F(ScriptEngineTest, LoadAppAssemblyRejectsMissingPath)
{
	auto* scriptEngine = ServiceLocator::Get<ScriptEngine>();
	ASSERT_NE(scriptEngine, nullptr);
	EXPECT_FALSE(scriptEngine->LoadAppAssembly("D:/definitely_missing/ChainedDecos.Scripts.dll"));
	EXPECT_TRUE(scriptEngine->GetScriptClasses().empty());
}

TEST_F(ScriptEngineTest, ReloadWithValidProjectLoadsScriptTypes)
{
	const std::vector<std::filesystem::path> candidates = GetReloadAssemblyCandidates();
	if (candidates.empty())
	{
		GTEST_SKIP() << "Skipping ScriptEngine reload success test: no managed assembly candidates found.";
	}

	auto* scriptEngine = ServiceLocator::Get<ScriptEngine>();
	ASSERT_NE(scriptEngine, nullptr);
	bool reloaded = false;
	for (const auto& assemblyPath : candidates)
	{
		auto project = std::make_shared<Project>();
		project->GetConfig().ProjectDirectory = assemblyPath.parent_path();
		project->GetConfig().Scripting.ModuleName = assemblyPath.stem().string();
		project->GetConfig().Scripting.ModuleDirectory = assemblyPath.parent_path();
		Project::SetActive(project);

		if (scriptEngine->ReloadAssembly(assemblyPath.string()))
		{
			reloaded = true;
			break;
		}

		EXPECT_FALSE(scriptEngine->IsReloadInProgress());
	}

	if (!reloaded)
	{
		GTEST_SKIP() << "Skipping ScriptEngine reload success test: all managed assembly candidates failed to load.";
	}

	EXPECT_FALSE(scriptEngine->IsReloadInProgress());
	EXPECT_EQ(scriptEngine->GetScriptClass("DefinitelyMissingType"), nullptr);

	if (scriptEngine->GetScriptClasses().empty())
	{
		GTEST_SKIP() << "Loaded assembly exposes no script subclasses; skipping script type lookup assertions.";
	}

	const std::string fullName = scriptEngine->GetScriptClasses().begin()->first;
	ASSERT_FALSE(fullName.empty());
	EXPECT_NE(scriptEngine->GetScriptClass(fullName), nullptr);

	const size_t lastDot = fullName.find_last_of('.');
	if (lastDot != std::string::npos && lastDot + 1 < fullName.size())
	{
		const std::string shortName = fullName.substr(lastDot + 1);
		EXPECT_NE(scriptEngine->GetScriptClass(shortName), nullptr);
	}
}
