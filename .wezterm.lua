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
	colors = {
  		foreground = 'silver',
-- The default background color
                background = 'black',
                cursor_fg = 'black',
                cursor_bg = 'silver',
                  ansi = {
                    'black',
                    'maroon',
                    '#006200',
                    'olive',
--                  'steelblue',
--                  'darkslateblue',
                    '#1b3246',
--                  'slateblue',
                    '#8b008b',
                    'teal',
                    'silver',
                  },
                  brights = {
                    'grey',
                    'firebrick',
                    'silver',
                    'yellow',
                    'dimgray',
                    '#b081b0',
--                  'cornflowerblue',
                    '#2e2e93',
                    'silver'
                  },
	},
        default_cursor_style = 'SteadyUnderline',
        window_decorations = "NONE",
--		font = wezterm.font 'Iosevka Extended', font_size = 12,
       		-- font = wezterm.font 'Pragmata Pro', font_size = 13,
                -- font = wezterm.font 'Cousine Nerd Font', font_size = 13,
		-- font = wezterm.font 'SF Mono', font_size = 12,
		font = wezterm.font 'Noto Sans Mono', font_size = 13,


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
--		font = wezterm.font({ family = "Iosevka Extended", weight = "Regular"}),
		font = wezterm.font({ family = "Noto Sans Mono", weight = "Regular" }),
--		font = wezterm.font({ family = "IBM Plex Mono", weight = "Regular" }),
		 -- font = wezterm.font({ family = "PragmataPro", weight = "Regular" }),
		-- font = wezterm.font({ family = "SF Mono", weight = "Regular" }),
		-- font = wezterm.font({ family = "Noto Sans Mono", weight = "Regular" }),
		-- font = wezterm.font({ family = "Cousine Nerd Font", weight = "Regular" }),
		},
},

}

