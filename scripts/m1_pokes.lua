-- scripts/m1_inputs.lua plus memory pokes, for seed harvests only: a run with
-- pokes is never a frame-by-frame reference (m1run cannot poke). An
-- -autoboot_script; M1INPUT_SCRIPT as for m1_inputs.lua, and
--
--   M1POKE="addr=value@from-to:size;..."
--
-- writes value (size 1, 2 or 4 bytes) to the V60's address every frame from
-- frame `from` to `to`, after the controls. Virtua Formula (docs/design/MODEL1.md):
-- 50116c=1920@1600-99999:2 holds the race time at 60 s; 400eb5=4@6000-6000:1
-- puts the car on its last lap (Beginner), so it finishes first and gets name entry.
dofile((debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "") .. "m1_inputs.lua")
local pokes = {}
for a, v, f, t, s in (os.getenv("M1POKE") or ""):gmatch("(%w+)=(%w+)@(%d+)%-(%d+):(%d)") do
	pokes[#pokes + 1] = { tonumber(a), tonumber(v), tonumber(f), tonumber(t), tonumber(s) }
end
local space = manager.machine.devices[":maincpu"].spaces["program"]
local pscreen = manager.machine.screens[":screen"]
sub_pokes = emu.add_machine_frame_notifier(function()
	local f = pscreen:frame_number()
	for _, p in ipairs(pokes) do
		if f >= p[3] and f <= p[4] then
			if p[5] == 1 then space:write_u8(p[1], p[2])
			elseif p[5] == 2 then space:write_u16(p[1], p[2])
			else space:write_u32(p[1], p[2]) end
		end
	end
end)
