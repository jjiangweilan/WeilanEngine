# 000 How to use Lua script
A script is loaded by dragging the scripting lua file into the GameObject's inspector

# 001 Overall structure and idea (WIP)
a lua script can be loaded from Assets folder in the project by the `LuaScriptLoader` as `LuaScript`. `LuaScript` should hold a luaRef to the table return by the script, when a `LuaScript` is set to `GameScript`(the component) `GameScript` should instantiate a new table by using `New` method of `LuaScript`. Later `GameScript` should call `Construct`, `Desctruct` and `Tick` respective inside the game loop

# 002 How to write a lua binding for class
lua binding is written in the file LuaBackend_Internal.hpp. `LuaBackend` should call xxx_LuaBinding.Bind() and it should leave a table in the lua stack representing the class it binds. Then LuaBackend will set the table to `wl` table, which is a table in global lua space representing the WeilanEngine scripting API.

