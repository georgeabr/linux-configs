-- Put the file in your ~ folder
local wezterm = require 'wezterm'

local mux = wezterm.mux

--wezterm.on("gui-startup", function(cmd)
wezterm.on("gui-startup", function()
--  local tab, pane, window = mux.spawn_window(cmd or {})
  local tab, pane, window = mux.spawn_window{}
    window:gui_window():maximize()
end)

-- a comment here

return {
        check_for_updates = false,
        hide_tab_bar_if_only_one_tab = true,
--        color_scheme = 'Builtin Dark',
--	color_scheme = 'Muse (terminal.sexy)',

       -- disable system bell (audible + visual)
       audible_bell     = "Disabled",
       enable_visual_bell = false,	
colors = {
    foreground = '#c0c0c0',  -- Light silver for better readability
    background = '#121212',  -- Slightly softened black to reduce eye strain
    cursor_fg = '#ffffff',   -- White cursor foreground for visibility
    cursor_bg = '#c0c0c0',   -- Light silver cursor background

    ansi = {
        '#000000',  -- Black
        '#b22222',  -- Firebrick (richer red)
        '#008000',  -- Standard dark green
        '#b8860b',  -- Dark goldenrod (warmer olive)
        '#4682b4',  -- Steel blue (balanced blue)
        '#8a2be2',  -- Blue violet (deep purple)
        '#20b2aa',  -- Light sea green (refined teal)
        '#c0c0c0',  -- Silver (neutral gray)
    },

    brights = {
        '#808080',  -- Grey (softer bright black)
        '#ff4500',  -- Orange red (vibrant red)
        '#32cd32',  -- Lime green (brighter green)
        '#ffd700',  -- Gold (warmer yellow)
        '#696969',  -- Dim gray (muted dark gray)
        '#da70d6',  -- Orchid (softer purple)
        '#4169e1',  -- Royal blue (stronger blue)
        '#ffffff'   -- White (true bright white)
    },
},
        default_cursor_style = 'SteadyUnderline',
        window_decorations = "NONE",
		-- font = wezterm.font 'Iosevka Extended', font_size = 12,
       		-- font = wezterm.font 'Pragmata Pro', font_size = 13,
--		font = wezterm.font 'Noto Sans Mono', font_size = 13,
		font = wezterm.font 'Cousine Nerd Font', font_size = 13,


    keys = {
        { key = '"', mods = "CTRL|SHIFT", action=wezterm.action.SplitVertical { domain = 'CurrentPaneDomain' }},
        { key = '%', mods = "CTRL|SHIFT",action=wezterm.action.SplitHorizontal{args={'bash'}}},
    },

        window_frame = {
                inactive_titlebar_bg = '#353535',
                active_titlebar_bg = '#000000',
                inactive_titlebar_fg = '#cccccc',
                active_titlebar_fg = '#d9d9d9',
                inactive_titlebar_border_bottom = '#2b2042',
                active_titlebar_border_bottom = '#2b2042',
                button_fg = '#cccccc',
                button_bg = '#2b2042',
                button_hover_fg = '#ffffff',
                button_hover_bg = '#3b3052',
        },

	font_rules = {
  		{
		intensity = "Bold",
		-- font = wezterm.font({ family = "Iosevka Extended", weight = "Regular"}),
		-- font = wezterm.font({ family = "IBM Plex Mono", weight = "Regular" }),
		 -- font = wezterm.font({ family = "PragmataPro", weight = "Regular" }),
--		font = wezterm.font({ family = "Noto Sans Mono", weight = "Regular" }),
		font = wezterm.font({ family = "Cousine Nerd Font", weight = "Regular" }),
		},
},

}


