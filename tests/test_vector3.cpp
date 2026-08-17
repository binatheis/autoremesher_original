#include <gtest/gtest.h>

#include <AutoRemesher/Vector3>

using AutoRemesher::Vector3;

TEST(Vector3Test, Construction)
{
    Vector3 a;
    EXPECT_DOUBLE_EQ(a.x(), 0.0);
    EXPECT_DOUBLE_EQ(a.y(), 0.0);
    EXPECT_DOUBLE_EQ(a.z(), 0.0);

    Vector3 b(1.0, 2.0, 3.0);
    EXPECT_DOUBLE_EQ(b.x(), 1.0);
    EXPECT_DOUBLE_EQ(b.y(), 2.0);
    EXPECT_DOUBLE_EQ(b.z(), 3.0);
}

TEST(Vector3Test, Arithmetic)
{
    const Vector3 a(1.0, 2.0, 3.0);
    const Vector3 b(4.0, 5.0, 6.0);

    const Vector3 sum = a + b;
    EXPECT_DOUBLE_EQ(sum.x(), 5.0);
    EXPECT_DOUBLE_EQ(sum.y(), 7.0);
    EXPECT_DOUBLE_EQ(sum.z(), 9.0);

    const Vector3 diff = b - a;
    EXPECT_DOUBLE_EQ(diff.x(), 3.0);

    const Vector3 scaled = a * 2.0;
    EXPECT_DOUBLE_EQ(scaled.z(), 6.0);

    const Vector3 scaledR = 2.0 * a;
    EXPECT_DOUBLE_EQ(scaledR.z(), 6.0);

    const Vector3 divided = b / 2.0;
    EXPECT_DOUBLE_EQ(divided.x(), 2.0);

    const Vector3 negated = -a;
    EXPECT_DOUBLE_EQ(negated.x(), -1.0);
}

TEST(Vector3Test, LengthAndNormalize)
{
    Vector3 v(3.0, 4.0, 0.0);
    EXPECT_DOUBLE_EQ(v.length(), 5.0);
    EXPECT_DOUBLE_EQ(v.lengthSquared(), 25.0);

    const Vector3 n = v.normalized();
    EXPECT_NEAR(n.length(), 1.0, 1e-12);

    v.normalize();
    EXPECT_NEAR(v.length(), 1.0, 1e-12);
    EXPECT_FALSE(v.isZero());
    EXPECT_TRUE(Vector3().isZero());
}

TEST(Vector3Test, Products)
{
    const Vector3 x(1.0, 0.0, 0.0);
    const Vector3 y(0.0, 1.0, 0.0);

    EXPECT_DOUBLE_EQ(Vector3::dotProduct(x, y), 0.0);
    EXPECT_DOUBLE_EQ(Vector3::dotProduct(x, x), 1.0);

    const Vector3 z = Vector3::crossProduct(x, y);
    EXPECT_DOUBLE_EQ(z.x(), 0.0);
    EXPECT_DOUBLE_EQ(z.y(), 0.0);
    EXPECT_DOUBLE_EQ(z.z(), 1.0);

    EXPECT_NEAR(Vector3::angle(x, y), 1.5707963267948966, 1e-12);
}

TEST(Vector3Test, NormalAndArea)
{
    const Vector3 a(0.0, 0.0, 0.0);
    const Vector3 b(1.0, 0.0, 0.0);
    const Vector3 c(0.0, 1.0, 0.0);

    const Vector3 n = Vector3::normal(a, b, c);
    EXPECT_DOUBLE_EQ(n.z(), 1.0);

    EXPECT_DOUBLE_EQ(Vector3::area(a, b, c), 0.5);
}

TEST(Vector3Test, Indexing)
{
    Vector3 v(7.0, 8.0, 9.0);
    EXPECT_DOUBLE_EQ(v[0], 7.0);
    EXPECT_DOUBLE_EQ(v[2], 9.0);
    v[1] = 42.0;
    EXPECT_DOUBLE_EQ(v.y(), 42.0);
}
