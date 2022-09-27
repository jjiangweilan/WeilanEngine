import pathlib
import subprocess
import glob
import os
import json

path = pathlib.Path(__file__).parent.resolve()

includeShaderPath = str(path.parent.parent.joinpath("glsl").joinpath("Include"))
glslShaderPath = str(path.parent.parent.joinpath("glsl").joinpath("Shader"))
assetShaderPath = str(path.parent.parent.joinpath("Assets").joinpath("Shader"))

def ConvertGLSLToSPV(glslPath, spvPath):
    subprocess.run(["glslc", glslPath, "-o", spvPath, "-I", includeShaderPath])

def GenReflection(spvPath):
    return subprocess.run(['spirv-cross', spvPath, "--reflect"], capture_output=True, text=True).stdout

filesInGLSLFolder = pathlib.Path(glslShaderPath).rglob("*")
for shaderFilePath in filesInGLSLFolder:
    if os.path.isdir(shaderFilePath):
        shaderFilePathStr = str(shaderFilePath)
        spvPath = shaderFilePathStr.replace(glslShaderPath, assetShaderPath)
        pathlib.Path(spvPath).mkdir(parents=True, exist_ok=True)

reflecJson = dict()

filesInGLSLFolder = pathlib.Path(glslShaderPath).rglob("*")
for shaderFilePath in filesInGLSLFolder:
    if os.path.isfile(shaderFilePath):
        shaderFilePathStr = str(shaderFilePath)
        spvPath = shaderFilePathStr.replace(glslShaderPath, assetShaderPath)
        shaderRelativePath = str(pathlib.PurePosixPath(pathlib.Path(spvPath).relative_to(assetShaderPath)))
        ConvertGLSLToSPV(shaderFilePathStr, spvPath)
        reflecJson[shaderRelativePath] = json.loads(GenReflection(spvPath))
        reflecJson[shaderRelativePath]["spvPath"] = shaderRelativePath

reflectOutputFilePath = assetShaderPath + "/ShaderReflect.json"
with open(reflectOutputFilePath, "w") as outfile:
    json.dump(reflecJson, outfile, indent=4)
        