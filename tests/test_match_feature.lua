local data = {type = "player", hp = 5}
match data do
    case {type = "player", hp = x} if x < 10 -> print("Dying!")
    case {type = "monster", [1] = id} -> print("attack", id)
    case _ -> print("Unknown")
end
