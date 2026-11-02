-- Scripted controls for a Model 1 MAME run (an -autoboot_script). The
-- script (M1INPUT_SCRIPT) is the one m1native --inputs reads:
--
--   # comment
--   <from>-<to> <name>=<v>    hold a control over a frame range (inclusive)
--   <at> <name>=<v>           one frame only
--
-- Frames are the screen's frame numbers. Names: coin, test, service, start,
-- vr1..vr4, shiftdown, shiftup (1 = pressed); wheel, accel, brake (raw ADC
-- values); Star Wars Arcade's start2, trigger, button2, button3, trigger2p,
-- button2p and stickx, sticky, throttle, stick2x, stick2y; Virtua Fighter's
-- punch, kick, guard, up, down, left, right (player 2's with a 2p suffix);
-- Wing War's gun, missile, smoke (views vr1..vr4; stickx, sticky, throttle).
-- Unscripted controls rest at their defaults. Later lines win.
local CONTROLS = {
	coin = { ":IN.0", 0x01 }, test = { ":IN.0", 0x04 }, service = { ":IN.0", 0x08 }, start = { ":IN.0", 0x10 },
	vr1 = { ":IN.0", 0x20 }, vr2 = { ":IN.0", 0x40 }, vr3 = { ":IN.0", 0x80 }, vr4 = { ":IN.1", 0x01 },
	shiftdown = { ":IN.1", 0x10 }, shiftup = { ":IN.1", 0x20 },
	wheel = { ":WHEEL", 0xff }, accel = { ":ACCEL", 0xff }, brake = { ":BRAKE", 0xff },
	-- Star Wars Arcade
	start2 = { ":IN.0", 0x20 }, trigger = { ":IN.1", 0x01 }, button2 = { ":IN.1", 0x02 },
	trigger2p = { ":IN.1", 0x04 }, button2p = { ":IN.1", 0x08 }, button3 = { ":IN.1", 0x10 },
	stickx = { ":STICK1X", 0xff, ":STICKX" }, sticky = { ":STICK1Y", 0xff, ":STICKY" }, throttle = { ":THROTTLE", 0xff },
	stick2x = { ":STICK2X", 0xff }, stick2y = { ":STICK2Y", 0xff },
	-- Virtua Fighter (and player 2's on IN.2)
	punch = { ":IN.1", 0x01 }, kick = { ":IN.1", 0x02 }, guard = { ":IN.1", 0x04 },
	down = { ":IN.1", 0x10 }, up = { ":IN.1", 0x20 }, right = { ":IN.1", 0x40 }, left = { ":IN.1", 0x80 },
	punch2p = { ":IN.2", 0x01 }, kick2p = { ":IN.2", 0x02 }, guard2p = { ":IN.2", 0x04 },
	down2p = { ":IN.2", 0x10 }, up2p = { ":IN.2", 0x20 }, right2p = { ":IN.2", 0x40 }, left2p = { ":IN.2", 0x80 },
	-- Wing War (its stick is STICKX, STICKY)
	gun = { ":IN.1", 0x10 }, missile = { ":IN.1", 0x20 }, smoke = { ":IN.1", 0x40 },
}

local path = os.getenv("M1INPUT_SCRIPT")
local lines = {}
for line in io.lines(path) do
	line = line:gsub("#.*", "")
	local from, to, rest = line:match("^%s*(%d+)%-(%d+)%s+(.*)$")
	if not from then
		from, rest = line:match("^%s*(%d+)%s+(.*)$")
		to = from
	end
	if from then
		for name, v in rest:gmatch("(%w+)=(%w+)") do
			assert(CONTROLS[name], "m1_inputs: unknown control " .. name)
			lines[#lines + 1] = { tonumber(from), tonumber(to), name, tonumber(v) }
		end
	end
end

local fields = {}
local function field(name)
	if not fields[name] then
		local c = CONTROLS[name]
		local port = manager.machine.ioport.ports[c[1]] or manager.machine.ioport.ports[c[3]]
		for _, f in pairs(port.fields) do
			if f.mask == c[2] then fields[name] = f end
		end
	end
	return fields[name]
end

local held = {}
local function apply(frame)
	local want = {}
	for _, l in ipairs(lines) do
		if frame >= l[1] and frame <= l[2] then want[l[3]] = l[4] end
	end
	for name in pairs(CONTROLS) do
		local v = want[name]
		if v ~= held[name] then
			local f = field(name)
			if v then f:set_value(v) else f:clear_value() end
			held[name] = v
		end
	end
end

-- M1INPUT_SNAPS=264,515: also save those frames' screens (snap/<frame>.png)
local snaps = {}
for n in (os.getenv("M1INPUT_SNAPS") or ""):gmatch("%d+") do snaps[tonumber(n)] = true end

local screen = manager.machine.screens[":screen"]
apply(0)
sub_frame = emu.add_machine_frame_notifier(function()
	local frame = screen:frame_number()
	if snaps[frame] then screen:snapshot(string.format("%05d.png", frame)) end
	apply(frame + 1)
end)
