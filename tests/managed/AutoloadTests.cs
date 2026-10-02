using System;
using NUnit.Framework;
using Chained;

namespace Chained.Managed.Tests
{
    [Autoload]
    public class SampleGameManager : Script
    {
        public static bool Created = false;
        public static int SceneLoadedCount = 0;
        public static bool Destroyed = false;

        public override void OnCreate()
        {
            Created = true;
        }

        public override void OnSceneLoaded()
        {
            SceneLoadedCount++;
        }

        public override void OnDestroy()
        {
            Destroyed = true;
        }
    }

    public class RegularSceneScript : Script
    {
        public static bool Destroyed = false;

        public override void OnDestroy()
        {
            Destroyed = true;
        }
    }

    [TestFixture]
    public class AutoloadTests
    {
        [SetUp]
        public void SetUp()
        {
            SampleGameManager.Created = false;
            SampleGameManager.SceneLoadedCount = 0;
            SampleGameManager.Destroyed = false;
            RegularSceneScript.Destroyed = false;
        }

        [Test]
        public void TestAutoloadAttribute_IsRecognized()
        {
            var type = typeof(SampleGameManager);
            bool isAutoload = type.IsDefined(typeof(AutoloadAttribute), true) ||
                              type.IsDefined(typeof(GlobalScriptAttribute), true);

            Assert.That(isAutoload, Is.True, "SampleGameManager must have AutoloadAttribute");
        }

        [Test]
        public void TestClearAll_PreservesAutoloadAndCallsOnSceneLoaded()
        {
            // Simulate 1 tick of update so Autoload scripts are discovered
            ScriptEngine.OnUpdateManaged(0.016f);

            // Instantiate regular scene script on entity 100
            ScriptEngine.InstantiateScriptManaged(100, "Chained.Managed.Tests.RegularSceneScript");

            // Next tick runs OnCreate/OnStart
            ScriptEngine.OnUpdateManaged(0.016f);
            ScriptEngine.OnUpdateManaged(0.016f);

            // Now simulate scene change
            ScriptEngine.ClearAllManaged();

            // Regular scene script must be destroyed
            Assert.That(RegularSceneScript.Destroyed, Is.True, "Regular scene script should be destroyed on scene unload");

            // Autoload script must survive and receive OnSceneLoaded callback
            Assert.That(SampleGameManager.Destroyed, Is.False, "Autoload script must NOT be destroyed on scene unload");
            Assert.That(SampleGameManager.SceneLoadedCount, Is.GreaterThan(0), "Autoload script must receive OnSceneLoaded");
        }
    }
}
