local t = {}
table.insert(t, 1)
print("Initial insert ok")

-- Enable lock
if table.share then
    table.share(t)
    print("table.share called")
else
    print("table.share NOT found")
    os.exit(1)
end

table.insert(t, 2)
print("Insert after share ok")

local v = t[1]
print("Read after share ok:", v)

if #t == 2 then
    print("Table lock test: PASS")
else
    print("Table lock test: FAIL")
end
