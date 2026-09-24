using NUnit.Framework;
using Chained;

namespace Chained.Managed.Tests
{
    [TestFixture]
    public class MathTests
    {
        [Test]
        public void TestVector3Operations()
        {
            var a = new Vector3(1.0f, 2.0f, 3.0f);
            var b = new Vector3(4.0f, 5.0f, 6.0f);

            var sum = a + b;
            Assert.That(sum.X, Is.EqualTo(5.0f));
            Assert.That(sum.Y, Is.EqualTo(7.0f));
            Assert.That(sum.Z, Is.EqualTo(9.0f));

            var diff = b - a;
            Assert.That(diff.X, Is.EqualTo(3.0f));
            Assert.That(diff.Y, Is.EqualTo(3.0f));
            Assert.That(diff.Z, Is.EqualTo(3.0f));

            float dot = Vector3.Dot(a, b);
            Assert.That(dot, Is.EqualTo(1 * 4 + 2 * 5 + 3 * 6)); // 4 + 10 + 18 = 32
        }

        [Test]
        public void TestVector4Operations()
        {
            var v = new Vector4(1.0f, 0.5f, 0.2f, 1.0f);
            Assert.That(v.X, Is.EqualTo(1.0f));
            Assert.That(v.Y, Is.EqualTo(0.5f));
            Assert.That(v.Z, Is.EqualTo(0.2f));
            Assert.That(v.W, Is.EqualTo(1.0f));
        }

        [Test]
        public void TestEntityValidity()
        {
            var validEntity = new Entity(42);
            Assert.That(validEntity.IsValid, Is.True);

            var nullEntity = new Entity(Entity.NullID);
            Assert.That(nullEntity.IsValid, Is.False);

            var maxEntity = new Entity(ulong.MaxValue);
            Assert.That(maxEntity.IsValid, Is.False);
        }
    }
}
