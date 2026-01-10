#include "ShaderConfig.hpp"
#include "Engine/Library/Hash.hpp"
Gfx::PipelineConfig Gfx::PipelineConfig::FromJson(const nlohmann::json& j)
{
    PipelineConfig_t config;

    // Basic settings
    config.cullMode = static_cast<CullMode>(j.value("cullMode", static_cast<int>(CullMode::Back)));
    config.topology = static_cast<Topology>(j.value("topology", static_cast<int>(Topology::TriangleList)));
    config.polygonMode = static_cast<PolygonMode>(j.value("polygonMode", static_cast<int>(PolygonMode::Fill)));

    // Depth settings
    if (j.contains("depth"))
    {
        const auto& depthJson = j["depth"];
        config.depth.writeEnable = depthJson.value("writeEnable", true);
        config.depth.testEnable = depthJson.value("testEnable", true);
        config.depth.compOp =
            static_cast<CompareOp>(depthJson.value("compOp", static_cast<int>(CompareOp::Greater_Or_Equal)));
        config.depth.boundTestEnable = depthJson.value("boundTestEnable", false);
        config.depth.minBounds = depthJson.value("minBounds", 0.0f);
        config.depth.maxBounds = depthJson.value("maxBounds", 1.0f);
        config.depth.depthBias = depthJson.value("depthBias", 1.0f);
        config.depth.depthSlopBias = depthJson.value("depthSlopBias", 1.0f);
    }

    // Stencil settings
    if (j.contains("stencil"))
    {
        const auto& stencilJson = j["stencil"];
        config.stencil.testEnable = stencilJson.value("testEnable", false);

        auto parseStencilOpState = [](const nlohmann::json& sJson) -> StencilOpState
        {
            StencilOpState state;
            state.failOp = static_cast<StencilOp>(sJson.value("failOp", static_cast<int>(StencilOp::Keep)));
            state.passOp = static_cast<StencilOp>(sJson.value("passOp", static_cast<int>(StencilOp::Keep)));
            state.depthFailOp = static_cast<StencilOp>(sJson.value("depthFailOp", static_cast<int>(StencilOp::Keep)));
            state.compareOp = static_cast<CompareOp>(sJson.value("compareOp", static_cast<int>(CompareOp::Always)));
            state.compareMask = sJson.value("compareMask", 0);
            state.writeMask = sJson.value("writeMask", 0);
            state.reference = sJson.value("reference", 0);
            return state;
        };

        if (stencilJson.contains("front"))
            config.stencil.front = parseStencilOpState(stencilJson["front"]);
        else
            config.stencil.front = parseStencilOpState(stencilJson);

        if (stencilJson.contains("back"))
            config.stencil.back = parseStencilOpState(stencilJson["back"]);
        else
            config.stencil.front = parseStencilOpState(stencilJson);
    }

    // Color settings
    if (j.contains("color"))
    {
        const auto& colorJson = j["color"];
        if (colorJson.contains("blends"))
        {
            for (const auto& blendJson : colorJson["blends"])
            {
                ColorBlendAttachmentState blend;
                blend.blendEnable = blendJson.value("blendEnable", false);
                blend.srcColorBlendFactor =
                    static_cast<BlendFactor>(blendJson.value("srcColorBlendFactor", static_cast<int>(BlendFactor::One)));
                blend.dstColorBlendFactor =
                    static_cast<BlendFactor>(blendJson.value("dstColorBlendFactor", static_cast<int>(BlendFactor::Zero)));
                blend.colorBlendOp =
                    static_cast<BlendOp>(blendJson.value("colorBlendOp", static_cast<int>(BlendOp::Add)));
                blend.srcAlphaBlendFactor =
                    static_cast<BlendFactor>(blendJson.value("srcAlphaBlendFactor", static_cast<int>(BlendFactor::One)));
                blend.dstAlphaBlendFactor =
                    static_cast<BlendFactor>(blendJson.value("dstAlphaBlendFactor", static_cast<int>(BlendFactor::Zero)));
                blend.alphaBlendOp =
                    static_cast<BlendOp>(blendJson.value("alphaBlendOp", static_cast<int>(BlendOp::Add)));
                blend.colorWriteMask = blendJson.value("colorWriteMask", 0);
                config.color.blends.push_back(blend);
            }
        }

        if (colorJson.contains("blendConstants") && colorJson["blendConstants"].is_array())
        {
            nlohmann::json blendConstants = colorJson["blendConstants"];
            for (int i = 0; i < 4; ++i)
            {
                config.color.blendConstants[i] = blendConstants[i];
            }
        }
    }

    return config;
}

nlohmann::json Gfx::PipelineConfig::ToJson() const
{

    nlohmann::json j;
    j["cullMode"] = static_cast<int>(v->cullMode);
    j["topology"] = static_cast<int>(v->topology);
    j["polygonMode"] = static_cast<int>(v->polygonMode);

    // Depth settings
    j["depth"] = nlohmann::json::object_t();
    j["depth"]["writeEnable"] = v->depth.writeEnable;
    j["depth"]["testEnable"] = v->depth.testEnable;
    j["depth"]["compOp"] = static_cast<int>(v->depth.compOp);
    j["depth"]["boundTestEnable"] = v->depth.boundTestEnable;
    j["depth"]["minBounds"] = v->depth.minBounds;
    j["depth"]["maxBounds"] = v->depth.maxBounds;
    j["depth"]["depthBias"] = v->depth.depthBias;
    j["depth"]["depthSlopBias"] = v->depth.depthSlopBias;

    // Stencil settings
    j["stencil"] = nlohmann::json::object_t();
    j["stencil"]["testEnable"] = v->stencil.testEnable;
    j["stencil"]["front"] = nlohmann::json::object_t();
    j["stencil"]["front"]["failOp"] = static_cast<int>(v->stencil.front.failOp);
    j["stencil"]["front"]["passOp"] = static_cast<int>(v->stencil.front.passOp);
    j["stencil"]["front"]["depthFailOp"] = static_cast<int>(v->stencil.front.depthFailOp);
    j["stencil"]["front"]["compareOp"] = static_cast<int>(v->stencil.front.compareOp);
    j["stencil"]["front"]["compareMask"] = v->stencil.front.compareMask;
    j["stencil"]["front"]["writeMask"] = v->stencil.front.writeMask;
    j["stencil"]["front"]["reference"] = v->stencil.front.reference;

    j["stencil"]["back"] = nlohmann::json::object_t();
    j["stencil"]["back"]["failOp"] = static_cast<int>(v->stencil.back.failOp);
    j["stencil"]["back"]["passOp"] = static_cast<int>(v->stencil.back.passOp);
    j["stencil"]["back"]["depthFailOp"] = static_cast<int>(v->stencil.back.depthFailOp);
    j["stencil"]["back"]["compareOp"] = static_cast<int>(v->stencil.back.compareOp);
    j["stencil"]["back"]["compareMask"] = v->stencil.back.compareMask;
    j["stencil"]["back"]["writeMask"] = v->stencil.back.writeMask;
    j["stencil"]["back"]["reference"] = v->stencil.back.reference;

    // Color settings
    j["color"] = nlohmann::json::object_t();
    j["color"]["blends"] = nlohmann::json::array();
    for (const auto& blend : v->color.blends)
    {
        nlohmann::json blendJson;
        blendJson["blendEnable"] = blend.blendEnable;
        blendJson["srcColorBlendFactor"] = static_cast<int>(blend.srcColorBlendFactor);
        blendJson["dstColorBlendFactor"] = static_cast<int>(blend.dstColorBlendFactor);
        blendJson["colorBlendOp"] = static_cast<int>(blend.colorBlendOp);
        blendJson["srcAlphaBlendFactor"] = static_cast<int>(blend.srcAlphaBlendFactor);
        blendJson["dstAlphaBlendFactor"] = static_cast<int>(blend.dstAlphaBlendFactor);
        blendJson["alphaBlendOp"] = static_cast<int>(blend.alphaBlendOp);
        blendJson["colorWriteMask"] = blend.colorWriteMask;
        j["color"]["blends"].push_back(blendJson);
    }
    j["color"]["blendConstants"] =
        {v->color.blendConstants[0], v->color.blendConstants[1], v->color.blendConstants[2], v->color.blendConstants[3]};

    return j;
}

void Gfx::PipelineConfig::Rehash()
{

    uint64_t seed = 0;
    Hash64(seed, v->cullMode);
    Hash64(seed, v->topology);
    Hash64(seed, v->polygonMode);
    Hash64(seed, v->depth);
    Hash64(seed, v->stencil);
    for (auto& c : v->color.blends)
    {
        Hash64(seed, c);
    }
    Hash64(seed, v->color.blendConstants[0]);
    Hash64(seed, v->color.blendConstants[1]);
    Hash64(seed, v->color.blendConstants[2]);
    Hash64(seed, v->color.blendConstants[3]);
    hash = seed;
}
