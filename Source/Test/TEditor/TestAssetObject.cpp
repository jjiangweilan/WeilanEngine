
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "Core/AssetObject.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/AssetDatabase/Importers/GeneralImporter.hpp"
#include "Core/AssetDatabase/AssetImporter.hpp"
using namespace Engine;

class AssetB: public AssetObject
{
    public:
        AssetB(const UUID& uuid = UUID::empty) : AssetObject(uuid), x(Editable<int>(this, "x",  1)), y(Editable<std::string>(this, "y", "1")) {};
        AssetB(int x, const std::string& y) : x(Editable<int>(this, "x",  x)), y(Editable<std::string>(this, "y", y)), AssetObject(UUID::empty){}

        Editable<int> x;
        Editable<std::string> y;
        EDITABLE(std::vector<std::string>, vecStr, std::initializer_list<std::string>{"aaa", "bbb"});
};

//class NonAssetA {};
class AssetA : public AssetObject
{
    public:
        AssetA(const UUID& uuid = UUID::empty) : AssetObject(uuid) {};

        void SetX(float val) { *x = val; }
        float GetX() {return *x;}
        AssetB& GetB() {return *assetB;}

        void SetRefPtr(RefPtr<AssetB> b) { *refPtr = b; }
        void SetUniPtr(UniPtr<AssetB>&& b) { *uniPtr = std::move(b);}
        std::unordered_map<std::string, int>& GetUNMap() { return *unMap; }

        RefPtr<AssetB> GetRefPtr() {return *refPtr;}
    private:
        EDITABLE(RefPtr<AssetB>, refPtr);
        EDITABLE(UniPtr<AssetB>, uniPtr);

        EDITABLE(SINGLE_ARG(std::unordered_map<std::string, int>), unMap);
        EDITABLE(float, x, 2);
        EDITABLE(AssetB, assetB, 2, "123");
       // EDITABLE(nonAssetA, NonAssetA);
};

// Dispatch multiple drawcalls and present them shouldn't crash
TEST(TestAssetObject, GetEditable){
    AssetA x;
    AssetB b(3, "567");
    UniPtr<AssetB> bb = MakeUnique<AssetB>(2, "321");
    x.SetRefPtr(&b);
    x.SetUniPtr(std::move(bb));

    EXPECT_EQ(*(x.GetMember<UniPtr<AssetB>>("uniPtr")->Get()->y), "321");
    EXPECT_EQ(*(x.GetMember<RefPtr<AssetB>>("refPtr")->Get()->y), "567");
    EXPECT_EQ(*x.GetMember<float>("x"), 2);
    EXPECT_EQ(x.GetMember<int>("x"), nullptr);
    EXPECT_EQ(*(x.GetMember<AssetB>("assetB")->y), "123");
    EXPECT_EQ(x.GetX(), 2);
    EXPECT_EQ(*(x.GetB().y), "123");
}


TEST(TestAssetObject, Serialize)
{
class AssetSerializerTester : public AssetSerializer
{
public:
    void Test(const std::string& rootRefPtrUUID)
    {
        EXPECT_EQ(std::string((char*)(mem + dataOffset["_root_.refPtr"])), rootRefPtrUUID);
        EXPECT_EQ(std::string((char*)(mem + dataOffset["_root_.uniPtr.y"])), "321");
        EXPECT_NE(*(float*)(mem + dataOffset["root.x"]), 3);
    }
};

class AssetSerializerLoadTester : public AssetSerializer
{
public:
    void Test(const std::string& rootRefPtrUUID)
    {
        EXPECT_EQ(std::string((char*)(mem + dataOffset["_root_.refPtr"])), rootRefPtrUUID);
        EXPECT_EQ(std::string((char*)(mem + dataOffset["_root_.uniPtr.y"])), "321");
        EXPECT_NE(*(float*)(mem + dataOffset["_root_.x"]), 3);
    }
};

    AssetA x;
    *x.GetB().x = 10;
    x.SetX(8);
    AssetB b(3, "567");
    UniPtr<AssetB> bb = MakeUnique<AssetB>(2, "321");
    x.SetRefPtr(&b);
    x.SetUniPtr(std::move(bb));
    AssetSerializerTester serializer;
    x.Serialize(serializer);
    serializer.Test(b.GetUUID().ToString());
    serializer.WriteToDisk("Serialize.test");

    AssetSerializerLoadTester loader;
    loader.LoadFromDisk("Serialize.test");
    loader.Test(b.GetUUID().ToString());
    
    ReferenceResolver reoslve;
    AssetA xx;
    xx.Deserialize(loader, reoslve);
    EXPECT_EQ(xx.GetX(), 8);
    EXPECT_EQ(*xx.GetB().x, 10);
    EXPECT_EQ(*xx.GetB().y, "123");
    // AssetFile file1;
    // file1.Load<AssetA>("Saved.assetA");
}

TEST(TestAssetObject, TestAssetDatabase)
{
class AssetDatabaseTest : public AssetDatabase
{
    public:
    AssetDatabaseTest() : AssetDatabase() {}
};
    AssetDatabaseTest dataBase0;
    AssetDatabaseTest dataBase1;
    AssetImporter::RegisterImporter("assetA", []() { return MakeUnique<GeneralImporter<AssetA>>(); });
    AssetImporter::RegisterImporter("assetB", []() { return MakeUnique<GeneralImporter<AssetB>>(); });

    RefPtr<AssetA> a;
    RefPtr<AssetB> b;

    UniPtr<AssetA> aa = MakeUnique<AssetA>();
    UniPtr<AssetB> bb = MakeUnique<AssetB>();
    aa->SetName("aa");
    bb->SetName("bb");
    UUID uuid0 = aa->GetUUID();
    UUID uuid1 = bb->GetUUID();
    aa->SetX(1);
    aa->SetRefPtr(bb);
    aa->GetUNMap()["popo"] = 990;
    aa->GetUNMap()["popo9"] = 123123;
    *bb->x = 2;
    (*bb->vecStr)[0] = "cccc";
    a = (AssetA*)dataBase0.Save(std::move(aa), "a.assetA").Get();
    b = (AssetB*)dataBase0.Save(std::move(bb), "b.assetB").Get();
    EXPECT_EQ(uuid0, a->GetUUID());
    EXPECT_EQ(uuid1, b->GetUUID());

    RefPtr<AssetA> loadA = dataBase1.Load<AssetA>("a.assetA");
    EXPECT_EQ(loadA->GetUNMap()["popo"], 990);
    EXPECT_EQ(loadA->GetUNMap()["popo9"], 123123);
    EXPECT_EQ(loadA->GetX(), 1);
    EXPECT_EQ(loadA->GetUUID(), uuid0);
    EXPECT_TRUE(loadA->GetRefPtr() == nullptr);
    RefPtr<AssetB> loadB = dataBase1.Load<AssetB>("b.assetB");
    EXPECT_EQ(loadB->GetUUID(), uuid1);
    EXPECT_EQ(*(loadB->x), 2);
    EXPECT_EQ((*loadB->vecStr)[0], "cccc");
    EXPECT_TRUE(loadA->GetRefPtr() != nullptr);
}
