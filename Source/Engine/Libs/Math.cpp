#include "Math.hpp"
#include "glm/gtc/quaternion.hpp"
namespace Math
{
void DecomposeMatrix(const glm::mat4& m, glm::vec3& pos, glm::vec3& scale, glm::quat& rot)
{
    using namespace glm;

    pos = m[3];
    for (int i = 0; i < 3; i++)
        scale[i] = glm::length(vec3(m[i]));

    glm::mat3 row;
    row[0] = m[0];
    row[1] = m[1];
    row[2] = m[2];

    vec3 sign{1,1,1};
    auto Pdum3 = cross(row[1], row[2]); // v3Cross(row[1], row[2], Pdum3);
    if (dot(row[0], Pdum3) < 0)
    {
        for (length_t i = 0; i < 3; i++)
        {
            scale[i] *= -1.0f;
            row[i] *= -1.0f;
            sign[i] = -1.0f;
        }
    }

    const glm::mat3 rotMtx(
        (scale[0] == 0.0f) ? vec3(sign.x, 0, 0) : (vec3(m[0] / scale[0])),
        (scale[1] == 0.0f) ? vec3(0, sign.y, 0) : (vec3(m[1] / scale[1])),
        (scale[2] == 0.0f) ? vec3(0, 0, sign.z) : (vec3(m[2] / scale[2]))
    );
    rot = glm::quat_cast(rotMtx);
}
} // namespace Math
