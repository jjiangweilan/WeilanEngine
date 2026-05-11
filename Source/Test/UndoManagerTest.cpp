#include <gtest/gtest.h>

#if ENGINE_EDITOR
#include "Editor/UndoManager.hpp"

namespace
{
class TestUndoCommand : public Editor::UndoCommand
{
public:
    TestUndoCommand(std::string name, int& value, int redoValue, int undoValue)
        : name(std::move(name)), value(value), redoValue(redoValue), undoValue(undoValue)
    {}

    void Undo() override { value = undoValue; }
    void Redo() override { value = redoValue; }
    const std::string& GetName() const override { return name; }

private:
    std::string name;
    int& value;
    int redoValue;
    int undoValue;
};
} // namespace

TEST(UndoManagerTest, ExecuteUndoRedoAndClearRedo)
{
    Editor::UndoManager undoManager;
    int value = 0;

    undoManager.Execute(std::make_unique<TestUndoCommand>("First", value, 1, 0));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(undoManager.CanUndo());
    EXPECT_FALSE(undoManager.CanRedo());
    EXPECT_EQ(undoManager.GetUndoName(), "First");

    undoManager.Undo();
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(undoManager.CanUndo());
    EXPECT_TRUE(undoManager.CanRedo());
    EXPECT_EQ(undoManager.GetRedoName(), "First");

    undoManager.Redo();
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(undoManager.CanUndo());
    EXPECT_FALSE(undoManager.CanRedo());

    undoManager.Undo();
    undoManager.Execute(std::make_unique<TestUndoCommand>("Second", value, 2, 0));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(undoManager.CanUndo());
    EXPECT_FALSE(undoManager.CanRedo());
    EXPECT_EQ(undoManager.GetUndoName(), "Second");
}

TEST(UndoManagerTest, MaxHistoryTrimsOldestCommand)
{
    Editor::UndoManager undoManager;
    undoManager.SetMaxHistory(1);
    int value = 0;

    undoManager.Execute(std::make_unique<TestUndoCommand>("First", value, 1, 0));
    undoManager.Execute(std::make_unique<TestUndoCommand>("Second", value, 2, 1));

    EXPECT_EQ(undoManager.GetUndoName(), "Second");
    undoManager.Undo();
    EXPECT_EQ(value, 1);
    EXPECT_FALSE(undoManager.CanUndo());
}
#endif
