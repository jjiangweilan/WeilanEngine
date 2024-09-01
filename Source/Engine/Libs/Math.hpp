#include <glm/glm.hpp>

namespace Math
{
void DecomposeMatrix(const glm::mat4& m, glm::vec3& pos, glm::vec3& scale, glm::quat& rot);

} // namespace Math
