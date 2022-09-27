# Coding Guideline

## Asset Serialization
Asset serialization consists two parts one part in AssetObject and another part in AssetSerializer
AssetObject can handle templated class and AssetObjects which need special handling. If a member is an ordinary class AssetObject will use AssetSerializer which can be extended