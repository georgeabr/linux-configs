-- Put the file in your ~ folder
local wezterm = require 'wezterm'

local mux = wezterm.mux

wezterm.on("gui-startup", function(cmd)
  local tab, pane, window = mux.spawn_window(cmd or {})
  window:gui_window():maximize()
end)

-- a comment here

return {
        check_for_updates = false,
        color_scheme = 'Builtin Dark',
        default_cursor_style = 'SteadyUnderline',
        window_decorations = "NONE",
        font = wezterm.font 'IBM Plex Mono', font_size = 14,


    keys = {
        { key = '"', mods = "CTRL|SHIFT|ALT", action=wezterm.action.SplitVertical { domain = 'CurrentPaneDomain' }},
        { key = '%', mods = "CTRL|SHIFT|ALT",action=wezterm.action.SplitHorizontal{args={'bash'}}},
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

}
