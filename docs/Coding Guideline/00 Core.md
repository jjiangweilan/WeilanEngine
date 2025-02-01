# 000 ObjPtr life time
All `ObjPtr` should be destroyed before the program automatically destroy it, because `ObejctTracker` is a static instance so we can't make sure the destruction order between the `ObjectTracker` and the `ObjPtr`

