-- The Digital Sound Board under MAME (Star Wars Arcade), for dsbcheck and as
-- m1run --dsb-log logs ours: each byte the sound 68000 sends it ("T"), each
-- byte its Z80 reads from the UART ("R") and each MPEG register write ("W"),
-- timed in seconds. An -autoboot_script that also runs the controls script
-- (scripts/m1_inputs.lua, M1INPUT_SCRIPT); the log goes to DSB_LOG.
dofile(debug.getinfo(1, "S").source:sub(2):match("(.*/)") .. "m1_inputs.lua")
local log = io.open(os.getenv("DSB_LOG"), "w")
local snd_space = manager.machine.devices[":m1audio:sndcpu"].spaces["program"]
local io_space = manager.machine.devices[":dsbz80:mpegcpu"].spaces["io"]
local function now() return manager.machine.time:as_double() end
dsb_tx_tap = snd_space:install_write_tap(0xc20000, 0xc20001, "dsb_tx", function(offset, data, mask)
	log:write(string.format("%.6f T c2 %02x\n", now(), data & 0xff))
end)
dsb_read_tap = io_space:install_read_tap(0xf0, 0xf0, "dsb_rx", function(offset, data, mask)
	log:write(string.format("%.6f R f0 %02x\n", now(), data & 0xff))
end)
dsb_write_tap = io_space:install_write_tap(0xe0, 0xe9, "dsb_mpeg", function(offset, data, mask)
	log:write(string.format("%.6f W %02x %02x\n", now(), offset & 0xff, data & 0xff))
	log:flush()
end)
