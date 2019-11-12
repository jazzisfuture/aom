-- automatically generated by the FlatBuffers compiler, do not modify

-- namespace: Sample

local flatbuffers = require('flatbuffers')

local Monster = {} -- the module
local Monster_mt = {} -- the class metatable

function Monster.New()
    local o = {}
    setmetatable(o, {__index = Monster_mt})
    return o
end
function Monster.GetRootAsMonster(buf, offset)
    local n = flatbuffers.N.UOffsetT:Unpack(buf, offset)
    local o = Monster.New()
    o:Init(buf, n + offset)
    return o
end
function Monster_mt:Init(buf, pos)
    self.view = flatbuffers.view.New(buf, pos)
end
function Monster_mt:Pos()
    local o = self.view:Offset(4)
    if o ~= 0 then
        local x = o + self.view.pos
        local obj = require('MyGame.Sample.Vec3').New()
        obj:Init(self.view.bytes, x)
        return obj
    end
end
function Monster_mt:Mana()
    local o = self.view:Offset(6)
    if o ~= 0 then
        return self.view:Get(flatbuffers.N.Int16, o + self.view.pos)
    end
    return 150
end
function Monster_mt:Hp()
    local o = self.view:Offset(8)
    if o ~= 0 then
        return self.view:Get(flatbuffers.N.Int16, o + self.view.pos)
    end
    return 100
end
function Monster_mt:Name()
    local o = self.view:Offset(10)
    if o ~= 0 then
        return self.view:String(o + self.view.pos)
    end
end
function Monster_mt:Inventory(j)
    local o = self.view:Offset(14)
    if o ~= 0 then
        local a = self.view:Vector(o)
        return self.view:Get(flatbuffers.N.Uint8, a + ((j-1) * 1))
    end
    return 0
end
function Monster_mt:InventoryLength()
    local o = self.view:Offset(14)
    if o ~= 0 then
        return self.view:VectorLen(o)
    end
    return 0
end
function Monster_mt:Color()
    local o = self.view:Offset(16)
    if o ~= 0 then
        return self.view:Get(flatbuffers.N.Int8, o + self.view.pos)
    end
    return 2
end
function Monster_mt:Weapons(j)
    local o = self.view:Offset(18)
    if o ~= 0 then
        local x = self.view:Vector(o)
        x = x + ((j-1) * 4)
        x = self.view:Indirect(x)
        local obj = require('MyGame.Sample.Weapon').New()
        obj:Init(self.view.bytes, x)
        return obj
    end
end
function Monster_mt:WeaponsLength()
    local o = self.view:Offset(18)
    if o ~= 0 then
        return self.view:VectorLen(o)
    end
    return 0
end
function Monster_mt:EquippedType()
    local o = self.view:Offset(20)
    if o ~= 0 then
        return self.view:Get(flatbuffers.N.Uint8, o + self.view.pos)
    end
    return 0
end
function Monster_mt:Equipped()
    local o = self.view:Offset(22)
    if o ~= 0 then
        local obj = flatbuffers.view.New(require('flatbuffers.binaryarray').New(0), 0)
        self.view:Union(obj, o)
        return obj
    end
end
function Monster.Start(builder) builder:StartObject(10) end
function Monster.AddPos(builder, pos) builder:PrependStructSlot(0, pos, 0) end
function Monster.AddMana(builder, mana) builder:PrependInt16Slot(1, mana, 150) end
function Monster.AddHp(builder, hp) builder:PrependInt16Slot(2, hp, 100) end
function Monster.AddName(builder, name) builder:PrependUOffsetTRelativeSlot(3, name, 0) end
function Monster.AddInventory(builder, inventory) builder:PrependUOffsetTRelativeSlot(5, inventory, 0) end
function Monster.StartInventoryVector(builder, numElems) return builder:StartVector(1, numElems, 1) end
function Monster.AddColor(builder, color) builder:PrependInt8Slot(6, color, 2) end
function Monster.AddWeapons(builder, weapons) builder:PrependUOffsetTRelativeSlot(7, weapons, 0) end
function Monster.StartWeaponsVector(builder, numElems) return builder:StartVector(4, numElems, 4) end
function Monster.AddEquippedType(builder, equippedType) builder:PrependUint8Slot(8, equippedType, 0) end
function Monster.AddEquipped(builder, equipped) builder:PrependUOffsetTRelativeSlot(9, equipped, 0) end
function Monster.End(builder) return builder:EndObject() end

return Monster -- return the module