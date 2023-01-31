// namespace glm
// {
//     void to_json(nlohmann::json& j, const glm::mat4& v)
//     {
//         j[0][0] = v[0][0];
//         j[0][1] = v[0][1];
//         j[0][2] = v[0][2];
//         j[0][3] = v[0][3];

//         j[1][0] = v[1][0];
//         j[1][1] = v[1][1];
//         j[1][2] = v[1][2];
//         j[1][3] = v[1][3];

//         j[2][0] = v[2][0];
//         j[2][1] = v[2][1];
//         j[2][2] = v[2][2];
//         j[2][3] = v[2][3];

//         j[3][0] = v[3][0];
//         j[3][1] = v[3][1];
//         j[3][2] = v[3][2];
//         j[3][3] = v[3][3];
//     }

//     void from_json(const nlohmann::json& j, glm::mat4& v)
//     {
//         v[0][0] = j[0][0];
//         v[0][1] = j[0][1];
//         v[0][2] = j[0][2];
//         v[0][3] = j[0][3];

//         v[1][0] = j[1][0];
//         v[1][1] = j[1][1];
//         v[1][2] = j[1][2];
//         v[1][3] = j[1][3];

//         v[2][0] = j[2][0];
//         v[2][1] = j[2][1];
//         v[2][2] = j[2][2];
//         v[2][3] = j[2][3];

//         v[3][0] = j[3][0];
//         v[3][1] = j[3][1];
//         v[3][2] = j[3][2];
//         v[3][3] = j[3][3];
//     }
// }

// namespace Engine
// {
//     void to_json(nlohmann::json& j, const Engine::AssetObjectMeta& v)
//     {
//         j = nlohmann::json{ {"uuid", v.uuid.ToString()}, {"type", v.type} };
//     }

//     void from_json(const nlohmann::json& j, Engine::AssetObjectMeta& v)
//     {
//         j.at("uuid").get_to(v.uuid);
//         j.at("type").get_to(v.type);
//     }
// }