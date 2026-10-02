using System;
using System.Reflection;
using NUnit.Framework;
using Chained;

namespace Chained.Managed.Tests
{
    [AutoAttach("Player")]
    public class SamplePlayerController : Script
    {
    }

    [AutoAttach("Enemy")]
    [AutoAttach("Boss")]
    public class SampleEnemyAI : Script
    {
    }

    [TestFixture]
    public class AutoAttachTests
    {
        [Test]
        public void TestAutoAttachAttributes_ArePresent()
        {
            var playerAttrs = typeof(SamplePlayerController).GetCustomAttributes<AutoAttachAttribute>(true);
            Assert.That(playerAttrs, Is.Not.Empty);

            var attrList = new System.Collections.Generic.List<AutoAttachAttribute>(playerAttrs);
            Assert.That(attrList[0].Tag, Is.EqualTo("Player"));

            var enemyAttrs = typeof(SampleEnemyAI).GetCustomAttributes<AutoAttachAttribute>(true);
            var enemyList = new System.Collections.Generic.List<AutoAttachAttribute>(enemyAttrs);
            Assert.That(enemyList.Count, Is.EqualTo(2));
            Assert.That(enemyList.Exists(a => a.Tag == "Enemy"), Is.True);
            Assert.That(enemyList.Exists(a => a.Tag == "Boss"), Is.True);
        }
    }
}
